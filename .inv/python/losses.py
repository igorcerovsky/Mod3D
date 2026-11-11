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
    """Fast separable 3D max-pool dilation with Chebyshev radius ``r`` (native on MPS/CUDA/CPU)."""
    B, C, D, H, W = mask.shape
    k = 2 * r + 1
    x = F.max_pool2d(mask.reshape(B * D, 1, H, W), kernel_size=k, stride=1, padding=r)
    x = x.reshape(B, D, H * W).transpose(1, 2)
    x = F.max_pool1d(x, kernel_size=k, stride=1, padding=r)
    return x.transpose(1, 2).reshape(B, C, D, H, W)


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
                 e_rel=0.05, depth_boost=0.5, halo_boost=3.0, forward_op=None):
        super().__init__()
        self.lambda_data, self.lambda_ms, self.lambda_cls = lambda_data, lambda_ms, lambda_cls
        self.body_boost, self.e_rel = body_boost, e_rel
        self.depth_boost, self.halo_boost = depth_boost, halo_boost
        self.forward_op = forward_op
        self.zhdanov = ZhdanovFocusingLoss()

    def _lithology_loss(self, aux, m_true, fg_mask, d3, w_z):
        logits = aux["logits"]                                      # (B, K, D, H, W)
        probs = aux["probs"]                                        # (B, K, D, H, W)
        num_classes = logits.shape[1]
        sub_mask = aux.get("mask")
        if sub_mask is None:
            sub_mask = torch.ones_like(fg_mask)

        with torch.no_grad():
            t0 = m_true[:, 0]
            cls_tgt = torch.zeros_like(t0, dtype=torch.long)
            cls_tgt[t0 <= -2.17] = 1                                # air cavity (-2.67)
            if num_classes >= 7:
                cls_tgt[(t0 > -2.17) & (t0 <= -1.61)] = 2           # water cavity (-1.67)
                cls_tgt[(t0 > -1.61) & (t0 <= -0.825)] = 3          # light rock / coal [-1.55, -0.85]
                cls_tgt[(t0 > -0.825) & (t0 < -1e-5)] = 4           # sediment / waste [-0.80, -0.10]
                cls_tgt[(t0 > 1e-5) & (t0 <= 0.775)] = 5            # mafic / ultramafic [+0.15, +0.75]
                cls_tgt[t0 > 0.775] = 6                             # high-grade ore [+0.80, +1.85]
            else:
                cls_tgt[(t0 > -2.17) & (t0 <= -1.235)] = 2
                cls_tgt[(t0 > -1.235) & (t0 < -1e-5)] = 3
                cls_tgt[t0 > 1e-5] = 4

        ce = F.cross_entropy(logits, cls_tgt, reduction="none").unsqueeze(1)  # (B, 1, D, H, W)
        fg_w = fg_mask * w_z
        bd_w = (d3 - fg_mask).clamp_min(0.0) * sub_mask
        bg_far_w = (1.0 - d3).clamp_min(0.0) * sub_mask

        l_ce_fg = ((ce * fg_w).flatten(1).sum(1) / fg_w.flatten(1).sum(1).clamp_min(1.0)).mean()
        l_ce_bd = ((ce * bd_w).flatten(1).sum(1) / bd_w.flatten(1).sum(1).clamp_min(1.0)).mean()
        l_ce_far = ((ce * bg_far_w).flatten(1).sum(1) / bg_far_w.flatten(1).sum(1).clamp_min(1.0)).mean()
        l_ce = 0.55 * l_ce_fg + 0.30 * l_ce_bd + 0.15 * l_ce_far

        # Multi-class soft Dice over foreground classes present in each sample + absent-class suppression
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
        l_dice_cls = (sum(dice_losses) / len(dice_losses) if dice_losses else torch.zeros((), device=m_true.device))
        return 0.5 * l_ce + 0.5 * l_dice_cls + 0.25 * absent_leak.mean()

    def forward(self, m_pred, m_true, d_clean=None, z_obs=None, aux=None):
        D = m_true.shape[2]
        a = m_true.abs().amax(dim=(1, 2, 3, 4), keepdim=True).clamp_min(0.05)
        pn, tn = m_pred / a, m_true / a
        fg_mask = (m_true != 0).float()

        # Depth weighting w_z in [1.0, 1.0 + depth_boost] to sharpen bottom contacts & deep bodies
        z_rel = torch.linspace(0.0, 1.0, D, device=m_true.device, dtype=m_true.dtype).view(1, 1, D, 1, 1)
        w_z = 1.0 + self.depth_boost * z_rel

        # Per-voxel local contrast normalisation + strong core overshoot penalty
        t_loc = tn.abs().clamp_min(0.05)
        e_loc = (pn - tn).abs() / t_loc
        overshoot = F.relu(pn.abs() - tn.abs()) / t_loc
        fg_w = fg_mask * w_z
        fg_cnt = fg_w.flatten(1).sum(1).clamp_min(1.0)
        err = e_loc + 0.5 * e_loc ** 2 + 1.0 * overshoot + 0.5 * overshoot ** 2
        l_fg = ((err * fg_w).flatten(1).sum(1) / fg_cnt).mean()

        with torch.no_grad():
            d1 = _dilate3d(fg_mask, 1)
            d3 = _dilate3d(fg_mask, 3)
            d7 = _dilate3d(fg_mask, 7)
            w_dist = 1.0 + 1.5 * (1.0 - fg_mask) + 1.5 * (1.0 - d1) + 2.0 * (1.0 - d3) + 4.0 * (1.0 - d7)

            pos_mask = (tn > 0).float()
            neg_mask = (tn < 0).float()
            pos_prox = _dilate3d(pos_mask, 1) + _dilate3d(pos_mask, 3) + _dilate3d(pos_mask, 7)
            neg_prox = _dilate3d(neg_mask, 1) + _dilate3d(neg_mask, 3) + _dilate3d(neg_mask, 7)
            only_pos = (neg_mask.amax(dim=(1, 2, 3, 4), keepdim=True) == 0).float()
            only_neg = (pos_mask.amax(dim=(1, 2, 3, 4), keepdim=True) == 0).float()
            pos_zone = (((pos_prox > neg_prox).float() + only_pos).clamp_max(1.0)) * (tn >= 0).float()
            neg_zone = (((neg_prox > pos_prox).float() + only_neg).clamp_max(1.0)) * (tn <= 0).float()

            # Disjoint contrast-band dilated zones for per-band mass preservation (scale-invariant)
            hi_neg = ((tn <= -0.5) & (tn < 0)).float()
            lo_neg = ((tn > -0.5) & (tn < 0)).float()
            hi_pos = (tn >= 0.5).float()
            lo_pos = ((tn > 0) & (tn < 0.5)).float()

        opp_mag = F.relu(-pn) * pos_zone + F.relu(pn) * neg_zone
        opp_prox_w = 1.0 + pos_prox * pos_zone + neg_prox * neg_zone
        sum_l1 = pn.abs().flatten(1).sum(1) + tn.abs().flatten(1).sum(1) + 1e-6
        l_halo = (self.halo_boost * (opp_mag * opp_prox_w).flatten(1).sum(1) / sum_l1).mean()

        w_tot = w_dist + self.halo_boost * (opp_mag > 0).float()
        diff_l1 = ((pn - tn).abs() * w_tot).flatten(1).sum(1)
        l_rel_l1 = (diff_l1 / sum_l1).mean()

        # Signed L1-IoU (Tanimoto) loss: unique global optimum at p == t
        p_aligned = F.relu(pn * torch.sign(tn))
        inter = torch.minimum(p_aligned, tn.abs()).flatten(1).sum(1)
        l_dice = (1.0 - (2.0 * inter + 1e-6) / sum_l1).clamp_min(0.0).mean()

        # Fixed-denominator 3D face-jump loss + contact sharpness + interior body flatness
        dp, dt = _grad3d(pn), _grad3d(tn)
        wz_faces = (0.5 * (w_z[:, :, 1:] + w_z[:, :, :-1]), w_z, w_z)
        fg_pairs = (
            (fg_mask[:, :, 1:, :, :] > 0) & (fg_mask[:, :, :-1, :, :] > 0),
            (fg_mask[:, :, :, 1:, :] > 0) & (fg_mask[:, :, :, :-1, :] > 0),
            (fg_mask[:, :, :, :, 1:] > 0) & (fg_mask[:, :, :, :, :-1] > 0),
        )
        g_diff = sum(((p - t).abs() * wz).flatten(1).sum(1) for p, t, wz in zip(dp, dt, wz_faces))
        g_true = sum((t.abs() * wz).flatten(1).sum(1) for t, wz in zip(dt, wz_faces)) + 1e-4
        c_err = sum(((p - t).abs() * (t != 0).float() * wz).flatten(1).sum(1) for p, t, wz in zip(dp, dt, wz_faces))
        c_cnt = sum(((t != 0).float() * wz).flatten(1).sum(1) for t, wz in zip(dt, wz_faces)).clamp_min(1.0)
        int_err = sum((p.abs() * ((t == 0) & fg_p).float() * wz).flatten(1).sum(1)
                      for p, t, fg_p, wz in zip(dp, dt, fg_pairs, wz_faces))
        int_cnt = sum((((t == 0) & fg_p).float() * wz).flatten(1).sum(1)
                      for t, fg_p, wz in zip(dt, fg_pairs, wz_faces)).clamp_min(1.0)
        l_grad = 0.40 * (g_diff / g_true).mean() + 0.35 * (c_err / c_cnt).mean() + 0.25 * (int_err / int_cnt).mean()

        # Multi-band mass conservation (zero CPU sync, prevents weak rock collapse next to cavities)
        band_errs = []
        for bm in (hi_neg, lo_neg, hi_pos, lo_pos):
            has_b = bm.flatten(1).sum(1) > 0
            if has_b.any():
                b_zone = _dilate3d(bm, 1) * (1.0 - (fg_mask - bm)).clamp_min(0.0)
                t_sgn = torch.where(tn < 0, -torch.ones_like(tn), torch.ones_like(tn))
                m_true_b = (tn.abs() * bm).flatten(1).sum(1)
                m_pred_b = (F.relu(pn * t_sgn) * b_zone).flatten(1).sum(1)
                band_errs.append(torch.where(has_b, (m_pred_b - m_true_b).abs() / m_true_b.clamp_min(1e-6),
                                             torch.zeros_like(m_true_b)).mean())
        l_band = sum(band_errs) / max(len(band_errs), 1)

        # Zhdanov Minimum Gradient Support consistency (wired via lambda_ms)
        l_mgs = torch.zeros((), device=m_pred.device)
        if self.lambda_ms > 0:
            mgs_p = self.zhdanov.minimum_gradient_support(pn, self.e_rel)
            mgs_t = self.zhdanov.minimum_gradient_support(tn, self.e_rel)
            l_mgs = self.lambda_ms * (mgs_p - mgs_t).abs()

        l_sup = 0.23 * (l_fg + l_rel_l1 + l_dice + l_grad) + 0.08 * l_band + l_halo + l_mgs

        l_cls = torch.zeros((), device=m_pred.device)
        if aux is not None and "logits" in aux and self.lambda_cls > 0:
            l_cls = self._lithology_loss(aux, m_true, fg_mask, d3, w_z)

        l_data = torch.zeros((), device=m_pred.device)
        if self.forward_op is not None and d_clean is not None:
            d_pred = self.forward_op(m_pred, z_obs)
            num = ((d_pred - d_clean) ** 2).flatten(1).sum(1)
            den = (d_clean ** 2).flatten(1).sum(1) + 1e-12
            l_amp = _charbonnier_rmse(num, den)
            dx_p, dy_p = d_pred[:, :, 1:, :] - d_pred[:, :, :-1, :], d_pred[:, :, :, 1:] - d_pred[:, :, :, :-1]
            dx_c, dy_c = d_clean[:, :, 1:, :] - d_clean[:, :, :-1, :], d_clean[:, :, :, 1:] - d_clean[:, :, :, :-1]
            g_num = ((dx_p - dx_c) ** 2).flatten(1).sum(1) + ((dy_p - dy_c) ** 2).flatten(1).sum(1)
            g_den = (dx_c ** 2).flatten(1).sum(1) + (dy_c ** 2).flatten(1).sum(1) + 1e-12
            l_dgrad = _charbonnier_rmse(g_num, g_den)
            l_data = 0.65 * l_amp + 0.35 * l_dgrad

        total = l_sup + self.lambda_cls * l_cls + self.lambda_data * l_data
        return total, {
            "loss_total": total.item(),
            "loss_sup": l_sup.item(),
            "loss_data": l_data.item(),
            "loss_ms": l_rel_l1.item(),
            "loss_grad": l_grad.item(),
            "loss_halo": l_halo.item(),
            "loss_cls": l_cls.item(),
        }
