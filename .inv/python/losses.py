"""Physics-informed, lithology-supervised & Zhdanov focusing losses for 3D potential-field inversion.

The focusing functionals use the SAME physical definitions as the C++ engine
(``inv::FocusingEvaluator``): volume-weighted Minimum Support and face-based, area-weighted
Minimum Gradient Support (jump form). On a regular lattice they are returned as fractions of the
domain volume / interior face area, hence independent of the grid resolution:

    S_MS  = sum_j V_j x_j^2 / (x_j^2 + e^2)  /  sum_j V_j,                x = m - m_ref
    S_MGS = sum_f A_f D_f^2 / (D_f^2 + e^2)  /  sum_f A_f,                D = jump across face f

Tensors are (B, 1, D=nz, H=nx, W=ny) with spacings (dz, dx, dy).

References:
- Portniaguine, O. & Zhdanov, M. S. (1999). Focusing geophysical inversion images.
- Zhdanov, M. S. (2002). Geophysical Inverse Theory and Regularization Problems.
"""

import torch
import torch.nn as nn
import torch.nn.functional as F


class ZhdanovFocusingLoss(nn.Module):
    def __init__(self, spacing=(1.0, 1.0, 1.0)):
        super().__init__()
        self.dz, self.dx, self.dy = spacing

    def minimum_support(self, m, e, m_ref=None):
        x = m if m_ref is None else m - m_ref
        x2 = x * x
        return torch.mean(x2 / (x2 + e * e))  # uniform lattice: volume weights cancel

    def minimum_gradient_support(self, m, e, m_ref=None):
        x = m if m_ref is None else m - m_ref
        e2 = e * e
        faces = (
            (x[:, :, 1:, :, :] - x[:, :, :-1, :, :], self.dx * self.dy),   # z-normal faces
            (x[:, :, :, 1:, :] - x[:, :, :, :-1, :], self.dz * self.dy),   # x-normal faces
            (x[:, :, :, :, 1:] - x[:, :, :, :, :-1], self.dz * self.dx),   # y-normal faces
        )
        num, den = 0.0, 0.0
        for d, area in faces:
            d2 = d * d
            num = num + area * torch.sum(d2 / (d2 + e2))
            den = den + area * d.numel()
        return num / den

    def forward(self, m, e, e_jump=None, m_ref=None):
        return (self.minimum_support(m, e, m_ref),
                self.minimum_gradient_support(m, e if e_jump is None else e_jump, m_ref))


def _dilate3d(mask, r: int):
    """Fast separable 3D max-pool dilation with Chebyshev radius ``r`` (supports multi-channel ``(B, C, D, H, W)``)."""
    B, C, D, H, W = mask.shape
    k = 2 * r + 1
    x = F.max_pool2d(mask.reshape(B * C * D, 1, H, W), kernel_size=k, stride=1, padding=r)
    x = x.reshape(B * C, D, H * W).transpose(1, 2)
    x = F.max_pool1d(x, kernel_size=k, stride=1, padding=r)
    return x.transpose(1, 2).reshape(B, C, D, H, W)


def _gap_saddle_mask(fg_mask, max_gap: int = 3):
    """Detect background voxels bracketed on opposite (-/+ axis) sides by foreground within ``max_gap`` cells."""
    bg = 1.0 - fg_mask
    saddle = torch.zeros_like(fg_mask)
    for dim, pad_fn in (
        (2, lambda t, k: F.pad(t, (0, 0, 0, 0, k, k))),
        (3, lambda t, k: F.pad(t, (0, 0, k, k, 0, 0))),
        (4, lambda t, k: F.pad(t, (k, k, 0, 0, 0, 0))),
    ):
        size = fg_mask.shape[dim]
        padded = pad_fn(fg_mask, max_gap)
        pos = torch.zeros_like(fg_mask)
        neg = torch.zeros_like(fg_mask)
        for off in range(1, max_gap + 1):
            pos = torch.maximum(pos, padded.narrow(dim, max_gap + off, size))
            neg = torch.maximum(neg, padded.narrow(dim, max_gap - off, size))
        saddle = torch.maximum(saddle, pos * neg * bg)
    return saddle


@torch.no_grad()
def _internal_unit_contact_mask(m_ref, fg_mask, tol: float = 1e-4, unit_id=None):
    """Detect foreground voxels touching an internal contact between distinct geological units."""
    contact = torch.zeros_like(fg_mask)
    fg_b = fg_mask > 0.5
    if unit_id is not None:
        uid = unit_id.long()
        cz = (uid[:, :, 1:, :, :] > 0) & (uid[:, :, :-1, :, :] > 0) & (uid[:, :, 1:, :, :] != uid[:, :, :-1, :, :])
        cx = (uid[:, :, :, 1:, :] > 0) & (uid[:, :, :, :-1, :] > 0) & (uid[:, :, :, 1:, :] != uid[:, :, :, :-1, :])
        cy = (uid[:, :, :, :, 1:] > 0) & (uid[:, :, :, :, :-1] > 0) & (uid[:, :, :, :, 1:] != uid[:, :, :, :, :-1])
    else:
        cz = fg_b[:, :, 1:, :, :] & fg_b[:, :, :-1, :, :] & ((m_ref[:, :, 1:, :, :] - m_ref[:, :, :-1, :, :]).abs() > tol)
        cx = fg_b[:, :, :, 1:, :] & fg_b[:, :, :, :-1, :] & ((m_ref[:, :, :, 1:, :] - m_ref[:, :, :, :-1, :]).abs() > tol)
        cy = fg_b[:, :, :, :, 1:] & fg_b[:, :, :, :, :-1] & ((m_ref[:, :, :, :, 1:] - m_ref[:, :, :, :, :-1]).abs() > tol)

    cf_z = cz.to(fg_mask.dtype)
    contact[:, :, 1:, :, :] = torch.maximum(contact[:, :, 1:, :, :], cf_z)
    contact[:, :, :-1, :, :] = torch.maximum(contact[:, :, :-1, :, :], cf_z)

    cf_x = cx.to(fg_mask.dtype)
    contact[:, :, :, 1:, :] = torch.maximum(contact[:, :, :, 1:, :], cf_x)
    contact[:, :, :, :-1, :] = torch.maximum(contact[:, :, :, :-1, :], cf_x)

    cf_y = cy.to(fg_mask.dtype)
    contact[:, :, :, :, 1:] = torch.maximum(contact[:, :, :, :, 1:], cf_y)
    contact[:, :, :, :, :-1] = torch.maximum(contact[:, :, :, :, :-1], cf_y)
    return contact


def _interior_edt_proxy(fg_mask, steps: int = 4, contact_mask=None):
    """Normalized interior distance-to-surface in [0, 1] via iterative 3D morphological erosion.

    When ``contact_mask`` is supplied, internal contacts between distinct abutting geological units
    also act as zero-depth boundaries so touching units do not merge into a single undifferentiated core.
    """
    seed = fg_mask if contact_mask is None else fg_mask * (1.0 - contact_mask)
    cur = seed
    acc = (fg_mask * 0.25 + 0.75 * seed) if contact_mask is not None else cur
    for _ in range(steps - 1):
        cur = (1.0 - _dilate3d(1.0 - cur, 1)).clamp_min(0.0)
        acc = acc + cur
    return acc / float(steps)


def _grad3d(u):
    """3D interior face jumps (dz, dx, dy) along depth and horizontal axes."""
    return (
        u[:, :, 1:, :, :] - u[:, :, :-1, :, :],
        u[:, :, :, 1:, :] - u[:, :, :, :-1, :],
        u[:, :, :, :, 1:] - u[:, :, :, :, :-1],
    )


def _charbonnier_rmse(num, den):
    """Zero-at-exact smooth relative L2 norm (avoids late-epoch squared-loss vanishing)."""
    return (torch.sqrt(num / den.clamp_min(1e-12) + 1e-6) - 1e-3).clamp_min(0.0).mean()


class CompoundInversionLoss(nn.Module):
    """Scale-normalised supervised + lithology-embedding + distance + halo + sharpness + physics loss.

    Improvements over v1:
    1. Fixed true-norm denominator in ``l_grad`` + explicit interior body flatness (stops spiky cones).
    2. Per-contrast-band mass balance (keeps weak rocks intact alongside strong cavities).
    3. Non-squared Charbonnier RMSE data misfit + 2D horizontal anomaly gradient matching (physics
       gradients stay active through final fine-tuning epochs).
    4. Multi-class lithology CE + soft Dice on v2 embedding logits ``aux["logits"]`` ({bg, air,
       water, light/coal, sediment/waste, mafic/ultramafic, ore}) for exact cavities & bounded bands.
    5. Zhdanov Minimum Gradient Support structural match weighted by ``lambda_ms``.
    """

    def __init__(self, lambda_data=0.25, lambda_ms=0.05, lambda_cls=0.25, body_boost=9.0,
                 e_rel=0.05, depth_boost=0.5, halo_boost=3.0, forward_op=None,
                 lambda_nll=0.06, beta_nll=0.5, lambda_coarse=0.20, lambda_amp=0.08,
                 use_v6_losses=False, tversky_alpha=0.35, tversky_beta=0.65, lambda_rho_init=0.10):
        super().__init__()
        self.lambda_data, self.lambda_ms, self.lambda_cls = lambda_data, lambda_ms, lambda_cls
        self.body_boost, self.e_rel = body_boost, e_rel
        self.depth_boost, self.halo_boost = depth_boost, halo_boost
        self.lambda_nll = lambda_nll
        self.beta_nll = beta_nll
        self.lambda_coarse = lambda_coarse
        self.lambda_amp = lambda_amp
        self.use_v6_losses = bool(use_v6_losses)
        self.tversky_alpha = float(tversky_alpha)
        self.tversky_beta = float(tversky_beta)
        self.lambda_rho_init = float(lambda_rho_init)
        self.forward_op = forward_op
        self.zhdanov = ZhdanovFocusingLoss()
        self._bp_cache = {}

    def _border_project_2d(self, g: torch.Tensor) -> torch.Tensor:
        """Project out perimeter-fitted affine plane P_BP(g) for detrend-invariant residual loss."""
        _, _, h, w = g.shape
        key = (int(h), int(w), str(g.device), str(g.dtype))
        cached = self._bp_cache.get(key)
        if cached is None:
            yy, xx = torch.meshgrid(
                torch.linspace(-1.0, 1.0, h, device="cpu", dtype=torch.float32),
                torch.linspace(-1.0, 1.0, w, device="cpu", dtype=torch.float32),
                indexing="ij",
            )
            phi = torch.stack([torch.ones_like(xx), xx, yy], dim=0)
            mask = torch.zeros(h, w, dtype=torch.bool)
            mask[0, :] = True
            mask[-1, :] = True
            mask[:, 0] = True
            mask[:, -1] = True
            q_b = torch.linalg.pinv(phi[:, mask].T.double())
            w_b = torch.zeros(3, 1, h, w, dtype=torch.float32)
            for k in range(3):
                buf = torch.zeros(h, w, dtype=torch.float32)
                buf[mask] = q_b[k].float()
                w_b[k, 0] = buf
            cached = (
                w_b.to(device=g.device, dtype=g.dtype),
                phi.unsqueeze(0).to(device=g.device, dtype=g.dtype),
            )
            self._bp_cache[key] = cached
        w_b, phi = cached
        coeffs = F.conv2d(g, w_b)
        return g - (coeffs * phi).sum(dim=1, keepdim=True)

    def _lithology_loss(self, aux, m_true, fg_mask, d3, w_z, gap_mask, contact_vox=None, is_v6: bool = False):
        logits = aux["logits"].float()                              # (B, K, D, H, W)
        probs = aux["probs"].float()                                # (B, K, D, H, W)
        num_classes = logits.shape[1]
        sub_mask = aux.get("mask")
        if sub_mask is None:
            sub_mask = torch.ones_like(fg_mask)

        with torch.no_grad():
            t0 = m_true[:, 0]
            cls_tgt = torch.zeros_like(t0, dtype=torch.long)
            cls_tgt[t0 <= -2.17] = 1                                # air cavity (-2.67)
            if num_classes >= 7:
                if is_v6:
                    cls_tgt[(t0 > -2.17) & (t0 <= -1.635)] = 2      # water cavity (-1.67)
                    cls_tgt[(t0 > -1.635) & (t0 <= -0.855)] = 3     # light rock / coal [-1.60, -0.80]
                    cls_tgt[(t0 > -0.855) & (t0 < -1e-5)] = 4       # sediment / waste [-0.85, 0.00)
                else:
                    cls_tgt[(t0 > -2.17) & (t0 <= -1.61)] = 2       # water cavity (-1.67)
                    cls_tgt[(t0 > -1.61) & (t0 <= -0.825)] = 3      # light rock / coal [-1.55, -0.85]
                    cls_tgt[(t0 > -0.825) & (t0 < -1e-5)] = 4       # sediment / waste [-0.80, -0.10]
                cls_tgt[(t0 > 1e-5) & (t0 <= 0.775)] = 5            # mafic / ultramafic [+0.15, +0.75] / [0, +0.80]
                cls_tgt[t0 > 0.775] = 6                             # high-grade ore [+0.80, +1.85] / [+0.75, +1.95]
            else:
                cls_tgt[(t0 > -2.17) & (t0 <= -1.235)] = 2
                cls_tgt[(t0 > -1.235) & (t0 < -1e-5)] = 3
                cls_tgt[t0 > 1e-5] = 4

        ce = F.cross_entropy(logits, cls_tgt, reduction="none").unsqueeze(1)  # (B, 1, D, H, W)
        fg_w = fg_mask * w_z
        l_ce_fg_vox = ((ce * fg_w).flatten(1).sum(1) / fg_w.flatten(1).sum(1).clamp_min(1.0)).mean()
        cls_ce_list = []
        for c_idx in range(1, num_classes):
            cw = (cls_tgt == c_idx).float().unsqueeze(1) * w_z
            denom = cw.sum()
            if float(denom) > 0.0:
                cls_ce_list.append((ce * cw).sum() / denom)
        l_ce_fg_bal = (sum(cls_ce_list) / len(cls_ce_list)) if cls_ce_list else l_ce_fg_vox
        l_ce_fg = 0.50 * l_ce_fg_vox + 0.50 * l_ce_fg_bal

        bd_core = (d3 - fg_mask).clamp_min(0.0) * (1.0 + 4.0 * gap_mask)
        if contact_vox is not None:
            bd_core = bd_core + 3.0 * contact_vox
        bd_w = bd_core * sub_mask
        bg_far_w = (1.0 - d3).clamp_min(0.0) * sub_mask

        l_ce_bd = ((ce * bd_w).flatten(1).sum(1) / bd_w.flatten(1).sum(1).clamp_min(1.0)).mean()
        l_ce_far = ((ce * bg_far_w).flatten(1).sum(1) / bg_far_w.flatten(1).sum(1).clamp_min(1.0)).mean()
        l_ce = 0.50 * l_ce_fg + 0.35 * l_ce_bd + 0.15 * l_ce_far

        # Multi-class soft Dice (foreground classes + boundary-shell background) & absent-class suppression
        dice_losses = []
        absent_leak = torch.zeros(m_true.shape[0], device=m_true.device, dtype=m_true.dtype)
        for c_idx in range(1, num_classes):
            tgt_c = (cls_tgt == c_idx).float().unsqueeze(1)
            prob_c = probs[:, c_idx:c_idx + 1] * sub_mask
            present = tgt_c.flatten(1).sum(1) > 0
            inter = (prob_c * tgt_c).flatten(1).sum(1)
            union = prob_c.flatten(1).sum(1) + tgt_c.flatten(1).sum(1)
            dc = 1.0 - (2.0 * inter + 1e-5) / (union + 1e-5)
            if present.any():
                dice_losses.append(dc[present].mean())
            absent_leak = absent_leak + torch.where(
                present, torch.zeros_like(absent_leak), prob_c.flatten(1).mean(1)
            )
        bg_shell = d3 * sub_mask
        bg_tgt = (1.0 - fg_mask) * bg_shell
        bg_prob = probs[:, 0:1] * bg_shell
        bg_inter = (bg_prob * bg_tgt).flatten(1).sum(1)
        bg_union = bg_prob.flatten(1).sum(1) + bg_tgt.flatten(1).sum(1)
        dice_losses.append((1.0 - (2.0 * bg_inter + 1e-5) / (bg_union + 1e-5)).mean())
        l_dice_cls = sum(dice_losses) / len(dice_losses)
        return 0.5 * l_ce + 0.5 * l_dice_cls + 0.25 * absent_leak.mean()

    def _unit_and_instance_iou_loss(self, pn, tn, fg_mask, d3, w_z, sub_mask, u_masks, u_d3, t_dil3):
        """Instance-balanced local-contrast IoU & per-unit 50%-isosurface IoU (scale-invariant & zero-at-exact)."""
        _, _, D, H, W = tn.shape
        kd = 9 if D >= 9 else (D if D % 2 == 1 else max(1, D - 1))
        kh = 9 if H >= 9 else (H if H % 2 == 1 else max(1, H - 1))
        kw = 9 if W >= 9 else (W if W % 2 == 1 else max(1, W - 1))
        with torch.no_grad():
            t_ref = torch.where(fg_mask > 0.5, tn.abs(), t_dil3).clamp_min(0.05)
            v_loc = F.avg_pool3d(
                fg_mask, kernel_size=(kd, kh, kw), stride=1, padding=(kd // 2, kh // 2, kw // 2)
            ).clamp_min(1e-3)
            w_vol = (1.0 / v_loc.sqrt()) * d3 * sub_mask * w_z
            t_loc = (tn / t_ref) * fg_mask
            t_abs_loc = t_loc.abs()
            u_zone = u_d3 * (1.0 - (fg_mask - u_masks)).clamp_min(0.0) * sub_mask
            u_vox = u_masks.flatten(2).sum(2)
            present_u = (u_vox > 0).to(pn.dtype)
            t0_sig = torch.sigmoid(pn.new_tensor(-5.0))
            t1_sig = torch.sigmoid(pn.new_tensor(5.0))
            inv_span = 1.0 / (t1_sig - t0_sig)
            a_true_1ch = ((torch.sigmoid(10.0 * (t_abs_loc - 0.5)) - t0_sig) * inv_span).clamp(0.0, 1.0) * fg_mask

        p_loc = (pn / t_ref) * d3 * sub_mask
        p_abs_loc = p_loc.abs()
        p_ali_fg = F.relu(p_loc * torch.sign(t_loc))
        inter_vox = torch.minimum(p_ali_fg, t_abs_loc)
        denom_vox = p_abs_loc + t_abs_loc

        inter_loc = (inter_vox * w_vol).flatten(1).sum(1)
        denom_loc = (denom_vox * w_vol).flatten(1).sum(1) + 1e-6
        l_inst_iou = (1.0 - (2.0 * inter_loc + 1e-6) / denom_loc).clamp_min(0.0).mean()

        p_eff = torch.where(fg_mask > 0.5, p_ali_fg, p_abs_loc)
        a_pred_1ch = ((torch.sigmoid(10.0 * (p_eff - 0.5)) - t0_sig) * inv_span).clamp(0.0, 1.0) * d3 * sub_mask
        iso_inter_vox = torch.minimum(a_pred_1ch, a_true_1ch)
        iso_union_vox = torch.maximum(a_pred_1ch, a_true_1ch)

        u_inter = (inter_vox * u_masks).flatten(2).sum(2)
        u_denom = (denom_vox * u_zone).flatten(2).sum(2) + 1e-6
        u_tan = (1.0 - (2.0 * u_inter + 1e-6) / u_denom).clamp_min(0.0)

        iso_inter = (iso_inter_vox * u_masks).flatten(2).sum(2)
        iso_union = (iso_union_vox * u_zone).flatten(2).sum(2) + 1e-6
        u_iso = (1.0 - (iso_inter + 1e-6) / iso_union).clamp_min(0.0)

        u_comb = (0.5 * u_tan + 0.5 * u_iso) * present_u
        l_unit_iou = (u_comb.sum(1) / present_u.sum(1).clamp_min(1.0)).mean()
        return 0.50 * l_inst_iou + 0.50 * l_unit_iou

    def _unit_and_instance_iou_loss_v6(self, pn, tn, fg_mask, d3, w_z, sub_mask,
                                       u_masks, u_d3, t_dil3, unit_id=None):
        """V6 unit_id-aware asymmetric Tversky + per-unit 50%-contrast IoU (recall-weighted)."""
        _, _, D, H, W = tn.shape
        kd = 9 if D >= 9 else (D if D % 2 == 1 else max(1, D - 1))
        kh = 9 if H >= 9 else (H if H % 2 == 1 else max(1, H - 1))
        kw = 9 if W >= 9 else (W if W % 2 == 1 else max(1, W - 1))
        alpha, beta = self.tversky_alpha, self.tversky_beta
        with torch.no_grad():
            t_ref = torch.where(fg_mask > 0.5, tn.abs(), t_dil3).clamp_min(0.05)
            v_loc = F.avg_pool3d(
                fg_mask, kernel_size=(kd, kh, kw), stride=1, padding=(kd // 2, kh // 2, kw // 2)
            ).clamp_min(1e-3)
            w_vol = (1.0 / v_loc.sqrt()) * d3 * sub_mask * w_z
            t_loc = (tn / t_ref) * fg_mask
            t_abs_loc = t_loc.abs()
            t0_sig = torch.sigmoid(pn.new_tensor(-5.0))
            t1_sig = torch.sigmoid(pn.new_tensor(5.0))
            inv_span = 1.0 / (t1_sig - t0_sig)
            a_true_1ch = ((torch.sigmoid(10.0 * (t_abs_loc - 0.5)) - t0_sig) * inv_span).clamp(0.0, 1.0) * fg_mask

            if unit_id is not None and int(unit_id.amax().item()) > 0:
                uid_3d = unit_id[:, 0].long()
                max_u = min(16, int(uid_3d.amax().item()))
                grp_masks = (
                    F.one_hot(uid_3d.clamp(0, max_u), num_classes=max_u + 1)[..., 1:]
                    .permute(0, 4, 1, 2, 3)
                    .to(pn.dtype)
                )
                grp_d3 = _dilate3d(grp_masks, 3)
            else:
                grp_masks = u_masks
                grp_d3 = u_d3
            grp_zone = grp_d3 * (1.0 - (fg_mask - grp_masks)).clamp_min(0.0) * sub_mask
            present_u = (grp_masks.flatten(2).sum(2) > 0).to(pn.dtype)

        p_loc = (pn / t_ref) * d3 * sub_mask
        p_abs_loc = p_loc.abs()
        p_ali_fg = F.relu(p_loc * torch.sign(t_loc))
        tp_vox = torch.minimum(p_ali_fg, t_abs_loc)
        fp_vox = F.relu(p_abs_loc - t_abs_loc)
        fn_vox = F.relu(t_abs_loc - p_ali_fg)

        tp_loc = (tp_vox * w_vol).flatten(1).sum(1)
        fp_loc = (fp_vox * w_vol).flatten(1).sum(1)
        fn_loc = (fn_vox * w_vol).flatten(1).sum(1)
        l_inst_tversky = (1.0 - (tp_loc + 1e-6) / (tp_loc + alpha * fp_loc + beta * fn_loc + 1e-6)).clamp_min(0.0).mean()

        p_eff = torch.where(fg_mask > 0.5, p_ali_fg, p_abs_loc)
        a_pred_1ch = ((torch.sigmoid(10.0 * (p_eff - 0.5)) - t0_sig) * inv_span).clamp(0.0, 1.0) * d3 * sub_mask
        iso_tp_vox = torch.minimum(a_pred_1ch, a_true_1ch)
        iso_fp_vox = F.relu(a_pred_1ch - a_true_1ch)
        iso_fn_vox = F.relu(a_true_1ch - a_pred_1ch)

        u_tp = (tp_vox * grp_masks * w_z).flatten(2).sum(2)
        u_fp = (fp_vox * grp_zone * w_z).flatten(2).sum(2)
        u_fn = (fn_vox * grp_masks * w_z).flatten(2).sum(2)
        u_tver = (1.0 - (u_tp + 1e-6) / (u_tp + alpha * u_fp + beta * u_fn + 1e-6)).clamp_min(0.0)

        iso_tp = (iso_tp_vox * grp_masks * w_z).flatten(2).sum(2)
        iso_fp = (iso_fp_vox * grp_zone * w_z).flatten(2).sum(2)
        iso_fn = (iso_fn_vox * grp_masks * w_z).flatten(2).sum(2)
        u_iso = (1.0 - (iso_tp + 1e-6) / (iso_tp + alpha * iso_fp + beta * iso_fn + 1e-6)).clamp_min(0.0)

        u_comb = (0.5 * u_tver + 0.5 * u_iso) * present_u
        l_unit_iou = (u_comb.sum(1) / present_u.sum(1).clamp_min(1.0)).mean()
        return 0.45 * l_inst_tversky + 0.55 * l_unit_iou

    def forward(self, m_pred, m_true, d_clean=None, z_obs=None, aux=None, unit_id=None):
        is_v6 = bool(
            self.use_v6_losses
            or unit_id is not None
            or (aux is not None and (aux.get("is_v6", False) or "rho_init" in aux))
        )
        m_pred = m_pred.float()
        m_true = m_true.float()
        if d_clean is not None:
            d_clean = d_clean.float()
        if z_obs is not None:
            z_obs = z_obs.float()
        D = m_true.shape[2]
        a = m_true.abs().amax(dim=(1, 2, 3, 4), keepdim=True).clamp_min(0.05)
        pn, tn = m_pred / a, m_true / a
        fg_mask = (m_true != 0).float()
        sub_mask = aux.get("mask") if aux is not None else None
        if sub_mask is None:
            sub_mask = torch.ones_like(fg_mask)

        # Depth weighting w_z: V6 uses geometric terrain+clearance depth <= 3.0; V1..V5 preserves exact baseline
        if is_v6:
            z_top = torch.arange(D, device=m_true.device, dtype=m_true.dtype).view(1, 1, D, 1, 1) / float(D)
            if aux is not None and "terr" in aux:
                terr_n = aux["terr"].float().to(m_true.device)
                if terr_n.dim() == 4:
                    terr_n = terr_n.unsqueeze(2)
                clr_n = aux["clr"].float().to(m_true.device) if "clr" in aux else torch.zeros_like(terr_n)
                if clr_n.dim() == 4:
                    clr_n = clr_n.unsqueeze(2)
            elif aux is not None and "mask" in aux:
                terr_n = (1.0 - sub_mask).sum(dim=2, keepdim=True) / float(D)
                clr_n = torch.zeros_like(terr_n)
            else:
                terr_n = (z_obs[:, None, None].clamp_min(0.0) / float(D * 50.0)).clamp(0.0, 0.5) if z_obs is not None else torch.zeros_like(z_top)
                clr_n = torch.zeros_like(terr_n)
            d_sub = (z_top - terr_n).clamp_min(0.0) / (1.0 - terr_n).clamp_min(0.25)
            clr_fac = 1.0 + 0.35 * (clr_n / (clr_n + 1.0))
            w_z = (1.0 + max(self.depth_boost, 1.0) * d_sub * clr_fac).clamp(1.0, 3.0)
        else:
            z_rel = torch.linspace(0.0, 1.0, D, device=m_true.device, dtype=m_true.dtype).view(1, 1, D, 1, 1)
            w_z = 1.0 + self.depth_boost * z_rel

        # Per-voxel local contrast normalisation (symmetric core accuracy + mild spiky-overshoot guard)
        t_loc = tn.abs().clamp_min(0.05)
        e_loc = (pn - tn).abs() / t_loc
        overshoot = F.relu(pn.abs() - tn.abs()) / t_loc
        fg_w = fg_mask * w_z
        fg_cnt = fg_w.flatten(1).sum(1).clamp_min(1.0)
        err = (e_loc + 0.35 * e_loc ** 2) if is_v6 else (e_loc + 0.35 * e_loc ** 2 + 0.20 * overshoot)
        l_fg = ((err * fg_w).flatten(1).sum(1) / fg_cnt).mean()

        with torch.no_grad():
            pos_mask = (tn > 0).float()
            neg_mask = (tn < 0).float()
            hi_neg = ((tn <= -0.5) & (tn < 0)).float()
            lo_neg = ((tn > -0.5) & (tn < 0)).float()
            hi_pos = (tn >= 0.5).float()
            lo_pos = ((tn > 0) & (tn < 0.5)).float()
            u_masks = torch.cat([hi_neg, lo_neg, hi_pos, lo_pos], dim=1)

            # Fused multi-channel Chebyshev dilations: r=1 (all 8 channels), then chained +2 (r=3) and +4 (r=7)
            m_stack = torch.cat([fg_mask, pos_mask, neg_mask, u_masks, tn.abs()], dim=1)
            d1_all = _dilate3d(m_stack, 1)
            d3_all = _dilate3d(d1_all, 2)
            d7_sub = _dilate3d(d3_all[:, :3], 4)

            d1, d3, d7 = d1_all[:, 0:1], d3_all[:, 0:1], d7_sub[:, 0:1]
            pos_prox = d1_all[:, 1:2] + d3_all[:, 1:2] + d7_sub[:, 1:2]
            neg_prox = d1_all[:, 2:3] + d3_all[:, 2:3] + d7_sub[:, 2:3]
            band_d1 = (d1_all[:, 3:4], d1_all[:, 4:5], d1_all[:, 5:6], d1_all[:, 6:7])
            band_d3 = d3_all[:, 3:7]
            t_dil3 = d3_all[:, 7:8]

            gap_mask = _gap_saddle_mask(fg_mask, max_gap=3)
            contact_vox = _internal_unit_contact_mask(tn, fg_mask, unit_id=unit_id)
            edt_tgt = (_interior_edt_proxy(fg_mask, steps=4, contact_mask=contact_vox)
                       if (aux is not None and "edt" in aux) else None)
            w_dist = (1.0 + 1.5 * (1.0 - fg_mask) + 1.5 * (1.0 - d1)
                      + 2.0 * (1.0 - d3) + 4.0 * (1.0 - d7) + 6.0 * gap_mask)

            only_pos = (neg_mask.amax(dim=(1, 2, 3, 4), keepdim=True) == 0).float()
            only_neg = (pos_mask.amax(dim=(1, 2, 3, 4), keepdim=True) == 0).float()
            pos_zone = (((pos_prox > neg_prox).float() + only_pos).clamp_max(1.0)) * (tn >= 0).float()
            neg_zone = (((neg_prox > pos_prox).float() + only_neg).clamp_max(1.0)) * (tn <= 0).float()

        opp_mag = F.relu(-pn) * pos_zone + F.relu(pn) * neg_zone
        opp_prox_w = 1.0 + pos_prox * pos_zone + neg_prox * neg_zone
        sum_l1 = pn.abs().flatten(1).sum(1) + tn.abs().flatten(1).sum(1) + 1e-6
        l_halo = (self.halo_boost * (opp_mag * opp_prox_w).flatten(1).sum(1) / sum_l1).mean()

        w_tot = w_dist + self.halo_boost * (opp_mag > 0).float()
        diff_l1 = ((pn - tn).abs() * w_tot).flatten(1).sum(1)
        l_rel_l1 = (diff_l1 / sum_l1).mean()

        # Signed L1-IoU (Tanimoto) loss + per-instance / per-unit 50%-contrast IoU
        p_aligned = F.relu(pn * torch.sign(tn))
        inter = torch.minimum(p_aligned, tn.abs()).flatten(1).sum(1)
        l_dice = (1.0 - (2.0 * inter + 1e-6) / sum_l1).clamp_min(0.0).mean()
        if is_v6:
            l_unit_inst_iou = self._unit_and_instance_iou_loss_v6(
                pn, tn, fg_mask, d3, w_z, sub_mask, u_masks, band_d3, t_dil3, unit_id=unit_id
            )
        else:
            l_unit_inst_iou = self._unit_and_instance_iou_loss(
                pn, tn, fg_mask, d3, w_z, sub_mask, u_masks, band_d3, t_dil3
            )
        l_iou_comb = 0.45 * l_dice + 0.55 * l_unit_inst_iou

        # Fixed-denominator 3D face-jump loss + local-contrast contact sharpness + interior body flatness
        dp, dt = _grad3d(pn), _grad3d(tn)
        wz_faces = (
            0.5 * (w_z[:, :, 1:, :, :] + w_z[:, :, :-1, :, :]),
            0.5 * (w_z[:, :, :, 1:, :] + w_z[:, :, :, :-1, :]) if w_z.shape[3] > 1 else w_z,
            0.5 * (w_z[:, :, :, :, 1:] + w_z[:, :, :, :, :-1]) if w_z.shape[4] > 1 else w_z,
        )
        fg_pairs = (
            (fg_mask[:, :, 1:, :, :] > 0) & (fg_mask[:, :, :-1, :, :] > 0),
            (fg_mask[:, :, :, 1:, :] > 0) & (fg_mask[:, :, :, :-1, :] > 0),
            (fg_mask[:, :, :, :, 1:] > 0) & (fg_mask[:, :, :, :, :-1] > 0),
        )
        g_diff = sum(((p - t).abs() * wz).flatten(1).sum(1) for p, t, wz in zip(dp, dt, wz_faces))
        g_true = sum((t.abs() * wz).flatten(1).sum(1) for t, wz in zip(dt, wz_faces)) + 1e-4
        if is_v6:
            if unit_id is not None:
                uid = unit_id.long()
                uid_pairs = (
                    (uid[:, :, 1:, :, :], uid[:, :, :-1, :, :]),
                    (uid[:, :, :, 1:, :], uid[:, :, :, :-1, :]),
                    (uid[:, :, :, :, 1:], uid[:, :, :, :, :-1]),
                )
                contact_faces = tuple((ua != ub) & ((ua > 0) | (ub > 0)) for ua, ub in uid_pairs)
                same_faces = tuple((ua > 0) & (ua == ub) for ua, ub in uid_pairs)
            else:
                contact_faces = tuple(t.abs() > 0.02 for t in dt)
                same_faces = tuple((t.abs() <= 0.02) & fg_p for t, fg_p in zip(dt, fg_pairs))
            c_err = sum(((p - t).abs() * cf.float() * wz).flatten(1).sum(1)
                        for p, t, cf, wz in zip(dp, dt, contact_faces, wz_faces))
            c_err_loc = sum((((p - t).abs() / t.abs().clamp_min(0.12)) * cf.float() * wz).flatten(1).sum(1)
                            for p, t, cf, wz in zip(dp, dt, contact_faces, wz_faces))
            c_cnt = sum((cf.float() * wz).flatten(1).sum(1) for cf, wz in zip(contact_faces, wz_faces)).clamp_min(1.0)
            c_term = 0.5 * (c_err / c_cnt) + 0.5 * (c_err_loc / c_cnt)
            # Continuous interior residual: handles both piecewise-constant units and smooth graded halos
            int_err = sum(((p - t).abs() * sf.float() * wz).flatten(1).sum(1)
                          for p, t, sf, wz in zip(dp, dt, same_faces, wz_faces))
            int_cnt = sum((sf.float() * wz).flatten(1).sum(1) for sf, wz in zip(same_faces, wz_faces)).clamp_min(1.0)
        else:
            c_err = sum(((p - t).abs() * (t != 0).float() * wz).flatten(1).sum(1) for p, t, wz in zip(dp, dt, wz_faces))
            c_err_loc = sum((((p - t).abs() / t.abs().clamp_min(0.05)) * (t != 0).float() * wz).flatten(1).sum(1)
                            for p, t, wz in zip(dp, dt, wz_faces))
            c_cnt = sum(((t != 0).float() * wz).flatten(1).sum(1) for t, wz in zip(dt, wz_faces)).clamp_min(1.0)
            c_term = 0.5 * (c_err / c_cnt) + 0.5 * (c_err_loc / c_cnt)
            int_err = sum((p.abs() * ((t == 0) & fg_p).float() * wz).flatten(1).sum(1)
                          for p, t, fg_p, wz in zip(dp, dt, fg_pairs, wz_faces))
            int_cnt = sum((((t == 0) & fg_p).float() * wz).flatten(1).sum(1)
                          for t, fg_p, wz in zip(dt, fg_pairs, wz_faces)).clamp_min(1.0)
        l_grad = 0.40 * (g_diff / g_true).mean() + 0.35 * c_term.mean() + 0.25 * (int_err / int_cnt).mean()

        # Multi-band mass conservation (exact core mass + 1-cell zone prevents skirt amplitude bleed)
        band_errs = []
        for bm, bm_d1 in zip((hi_neg, lo_neg, hi_pos, lo_pos), band_d1):
            has_b = bm.flatten(1).sum(1) > 0
            if has_b.any():
                b_zone = bm_d1 * (1.0 - (fg_mask - bm)).clamp_min(0.0)
                t_sgn = torch.where(tn < 0, -torch.ones_like(tn), torch.ones_like(tn))
                m_true_b = (tn.abs() * bm).flatten(1).sum(1)
                m_pred_core = (F.relu(pn * t_sgn) * bm).flatten(1).sum(1)
                m_pred_zone = (F.relu(pn * t_sgn) * b_zone).flatten(1).sum(1)
                core_rel = (m_pred_core - m_true_b).abs() / m_true_b.clamp_min(1e-6)
                zone_rel = (m_pred_zone - m_true_b).abs() / m_true_b.clamp_min(1e-6)
                band_errs.append(torch.where(has_b, 0.65 * core_rel + 0.35 * zone_rel,
                                             torch.zeros_like(m_true_b)).mean())
        l_band = sum(band_errs) / max(len(band_errs), 1)

        # Foreground amplitude consistency (prevents systematic underestimation in large bodies)
        l_amp_cons = torch.zeros((), device=m_pred.device, dtype=m_pred.dtype)
        if self.lambda_amp > 0:
            t_sgn = torch.where(tn < 0, -torch.ones_like(tn), torch.ones_like(tn))
            fg_m = fg_mask * w_z
            fg_sum = fg_m.flatten(1).sum(1).clamp_min(1.0)
            p_mean = (F.relu(pn * t_sgn) * fg_m).flatten(1).sum(1) / fg_sum
            t_mean = (tn.abs() * fg_m).flatten(1).sum(1) / fg_sum
            has_fg = fg_mask.flatten(1).sum(1) > 0
            w_core = tn.abs() * fg_mask
            w_core_sum = w_core.flatten(1).sum(1).clamp_min(1e-6)
            p_core = (F.relu(pn * t_sgn) * w_core).flatten(1).sum(1) / w_core_sum
            t_core = (tn.abs() * w_core).flatten(1).sum(1) / w_core_sum
            amp_err = 0.5 * (
                (p_mean - t_mean).abs() / t_mean.clamp_min(1e-6)
                + (p_core - t_core).abs() / t_core.clamp_min(1e-6)
            )
            l_amp_cons = torch.where(has_fg, amp_err, torch.zeros_like(amp_err)).mean()

        # Zhdanov Minimum Gradient Support consistency (wired via lambda_ms)
        l_mgs = torch.zeros((), device=m_pred.device)
        if self.lambda_ms > 0:
            mgs_p = self.zhdanov.minimum_gradient_support(pn, self.e_rel)
            mgs_t = self.zhdanov.minimum_gradient_support(tn, self.e_rel)
            l_mgs = self.lambda_ms * (mgs_p - mgs_t).abs()

        eff_lambda_amp = max(float(self.lambda_amp), 0.20) if is_v6 and self.lambda_amp > 0 else float(self.lambda_amp)
        l_sup = (0.23 * (l_fg + l_rel_l1 + l_iou_comb + l_grad) + 0.12 * l_band
                 + l_halo + l_mgs + eff_lambda_amp * l_amp_cons)

        l_cls = torch.zeros((), device=m_pred.device)
        if aux is not None and "logits" in aux and self.lambda_cls > 0:
            l_cls = self._lithology_loss(
                aux, m_true, fg_mask, d3, w_z, gap_mask, contact_vox=contact_vox, is_v6=is_v6
            )

        l_edt = torch.zeros((), device=m_pred.device)
        if edt_tgt is not None:
            edt_mask = d3 * sub_mask * (1.0 + 3.0 * gap_mask + 3.0 * contact_vox)
            edt_err = F.smooth_l1_loss(aux["edt"], edt_tgt, reduction="none", beta=0.15)
            l_edt = 0.25 * ((edt_err * edt_mask).flatten(1).sum(1) / edt_mask.flatten(1).sum(1).clamp_min(1.0)).mean()

        l_coarse = torch.zeros((), device=m_pred.device)
        if aux is not None and "coarse_pred" in aux and self.lambda_coarse > 0:
            cpn = aux["coarse_pred"].float() / a
            ctn = F.avg_pool3d(m_true, 2) / a
            csum = cpn.abs().flatten(1).sum(1) + ctn.abs().flatten(1).sum(1) + 1e-6
            c_l1 = F.smooth_l1_loss(cpn, ctn, reduction="none", beta=0.01).flatten(1).sum(1) / csum
            c_ali = F.relu(cpn * torch.sign(ctn))
            c_int = torch.minimum(c_ali, ctn.abs()).flatten(1).sum(1)
            c_iou = (1.0 - (2.0 * c_int + 1e-6) / csum).clamp_min(0.0)
            l_coarse = (0.5 * c_l1 + 0.5 * c_iou).mean()

        l_rho_init = torch.zeros((), device=m_pred.device)
        if is_v6 and aux is not None and "rho_init" in aux and self.lambda_rho_init > 0:
            ri_n = aux["rho_init"].float() / a
            ri_l1 = (((ri_n - tn).abs() * w_tot).flatten(1).sum(1) / (ri_n.abs().flatten(1).sum(1) + tn.abs().flatten(1).sum(1) + 1e-6)).mean()
            ri_ali = F.relu(ri_n * torch.sign(tn))
            ri_tp = torch.minimum(ri_ali, tn.abs())
            ri_fp = F.relu(ri_n.abs() - tn.abs())
            ri_fn = F.relu(tn.abs() - ri_ali)
            tp_i = (ri_tp * d3 * sub_mask * w_z).flatten(1).sum(1)
            fp_i = (ri_fp * d3 * sub_mask * w_z).flatten(1).sum(1)
            fn_i = (ri_fn * d3 * sub_mask * w_z).flatten(1).sum(1)
            ri_tver = (1.0 - (tp_i + 1e-6) / (tp_i + self.tversky_alpha * fp_i + self.tversky_beta * fn_i + 1e-6)).clamp_min(0.0).mean()
            l_rho_init = 0.5 * ri_l1 + 0.5 * ri_tver

        l_nll = torch.zeros((), device=m_pred.device)
        eff_lambda_nll = max(float(self.lambda_nll), 0.10) if is_v6 and self.lambda_nll > 0 else float(self.lambda_nll)
        if aux is not None and "log_var" in aux and eff_lambda_nll > 0:
            lv = aux["log_var"].float().clamp(-8.0, 4.0)
            sigma_b = torch.exp(0.5 * self.beta_nll * lv).detach()
            if is_v6:
                # Stratified Huber-Gaussian NLL aligned with Gaussian sigma = exp(0.5 * log_var)
                delta = (m_pred - m_true).abs()
                c_h = 3.0
                huber_sq = torch.where(delta <= c_h, delta * delta, 2.0 * c_h * delta - c_h * c_h)
                vox_nll = sigma_b * (0.5 * huber_sq * torch.exp(-lv) + 0.5 * lv + 4.0)
                fg_n = fg_mask * sub_mask
                d1_n = (d1 - fg_mask).clamp_min(0.0) * sub_mask
                d3_d1_n = (d3 - d1).clamp_min(0.0) * sub_mask
                far_n = (1.0 - d3).clamp_min(0.0) * sub_mask
                nll_fg = ((vox_nll * fg_n).flatten(1).sum(1) / fg_n.flatten(1).sum(1).clamp_min(1.0)).mean()
                nll_d1 = ((vox_nll * d1_n).flatten(1).sum(1) / d1_n.flatten(1).sum(1).clamp_min(1.0)).mean()
                nll_d3 = ((vox_nll * d3_d1_n).flatten(1).sum(1) / d3_d1_n.flatten(1).sum(1).clamp_min(1.0)).mean()
                nll_far = ((vox_nll * far_n).flatten(1).sum(1) / far_n.flatten(1).sum(1).clamp_min(1.0)).mean()
                l_nll = 0.45 * nll_fg + 0.18 * nll_d1 + 0.12 * nll_d3 + 0.25 * nll_far
            else:
                err_abs = (m_pred - m_true).abs()
                vox_nll = sigma_b * (err_abs * torch.exp(-0.5 * lv) + 0.5 * lv + 4.0)
                fg_n = fg_mask * sub_mask
                bd_n = (d3 - fg_mask).clamp_min(0.0) * sub_mask
                far_n = (1.0 - d3).clamp_min(0.0) * sub_mask
                nll_fg = ((vox_nll * fg_n).flatten(1).sum(1) / fg_n.flatten(1).sum(1).clamp_min(1.0)).mean()
                nll_bd = ((vox_nll * bd_n).flatten(1).sum(1) / bd_n.flatten(1).sum(1).clamp_min(1.0)).mean()
                nll_far = ((vox_nll * far_n).flatten(1).sum(1) / far_n.flatten(1).sum(1).clamp_min(1.0)).mean()
                l_nll = 0.50 * nll_fg + 0.35 * nll_bd + 0.15 * nll_far

        l_data = torch.zeros((), device=m_pred.device)
        if self.forward_op is not None and d_clean is not None:
            try:
                d_err = self.forward_op(m_pred - m_true, z_obs, level_stride=2)
            except TypeError:
                d_err = self.forward_op(m_pred, z_obs) - d_clean
            num = (d_err ** 2).flatten(1).sum(1)
            den = (d_clean ** 2).flatten(1).sum(1) + 1e-12
            l_amp = _charbonnier_rmse(num, den)
            dx_e, dy_e = d_err[:, :, 1:, :] - d_err[:, :, :-1, :], d_err[:, :, :, 1:] - d_err[:, :, :, :-1]
            dx_c, dy_c = d_clean[:, :, 1:, :] - d_clean[:, :, :-1, :], d_clean[:, :, :, 1:] - d_clean[:, :, :, :-1]
            g_num = (dx_e ** 2).flatten(1).sum(1) + (dy_e ** 2).flatten(1).sum(1)
            g_den = (dx_c ** 2).flatten(1).sum(1) + (dy_c ** 2).flatten(1).sum(1) + 1e-12
            l_dgrad = _charbonnier_rmse(g_num, g_den)
            if is_v6 and d_err.shape[-2] >= 4 and d_err.shape[-1] >= 4:
                d_err_bp = self._border_project_2d(d_err)
                d_clean_bp = self._border_project_2d(d_clean)
                bp_num = (d_err_bp ** 2).flatten(1).sum(1)
                bp_den = (d_clean_bp ** 2).flatten(1).sum(1) + 1e-12
                l_bp_amp = _charbonnier_rmse(bp_num, bp_den)
                l_data = 0.50 * l_amp + 0.30 * l_dgrad + 0.20 * l_bp_amp
            else:
                l_data = 0.65 * l_amp + 0.35 * l_dgrad

        total = (l_sup + self.lambda_cls * l_cls + l_edt
                 + self.lambda_coarse * l_coarse + self.lambda_rho_init * l_rho_init
                 + eff_lambda_nll * l_nll + self.lambda_data * l_data)
        return total, {
            "loss_total": total.item(),
            "loss_sup": l_sup.item(),
            "loss_data": l_data.item(),
            "loss_ms": l_rel_l1.item(),
            "loss_grad": l_grad.item(),
            "loss_halo": l_halo.item(),
            "loss_cls": l_cls.item(),
            "loss_edt": l_edt.item(),
            "loss_coarse": l_coarse.item(),
            "loss_rho_init": l_rho_init.item(),
            "loss_nll": l_nll.item(),
            "loss_amp": l_amp_cons.item(),
            "loss_unit_iou": l_unit_inst_iou.item(),
        }
