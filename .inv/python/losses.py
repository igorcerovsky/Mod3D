"""Physics-informed & Zhdanov focusing losses for 3D potential-field inversion.

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

import math

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


class CompoundInversionLoss(nn.Module):
    """Scale-normalised supervised + distance-weighted + opposite-halo + sharp-boundary + physics loss.

    Every term is normalised PER SAMPLE by ``a = max|m_true|`` and by the active body support, so
    a weak +0.1 g/cm^3 body and a -2.67 g/cm^3 cavity of any size contribute equally without
    collapsing to the trivial zero model:

    1. Depth-weighted foreground L1+L2 error averaged over the true body cells (``w_z = 1 + 0.5*z/z_max``
       compensates 1/z^2 gravity decay so deep bodies and bottom contacts stay sharp).
    2. Distance- & sign-weighted symmetric relative L1: multi-scale 3D Chebyshev distance shells
       (r = 1, 3, 7 voxels) increase background penalty from 1x on the body up to 10x far from any
       body, with an extra penalty on opposite-sign compensating halos.
    3. Opposite-sign compensating halo penalty ``l_halo``: specifically penalises negative density
       voxels placed around a positive body (or positive density voxels placed around a negative
       body/cavity) to artificially cancel gravity tails.
    4. Signed soft Dice ``1 - 2 <p, t> / (||p||_2^2 + ||t||_2^2)``: aligns support, sign, and amplitude.
    5. Depth-weighted 3D contact-edge & face-jump loss ``l_grad``: matches 3D discrete gradients
       ``(dz, dx, dy)`` across all interior voxel faces and explicitly penalises smeared jumps on
       the true body contact surfaces (especially bottom boundaries).
    6. Relative data misfit ``||G m_pred - d_clean||^2 / ||d_clean||^2`` through the draped-station
       operator ``forward_op(m, z_obs)`` (disabled if None).
    """

    def __init__(self, lambda_data=0.25, lambda_ms=0.05, body_boost=9.0, e_rel=0.05,
                 depth_boost=0.5, halo_boost=3.0, forward_op=None):
        super().__init__()
        self.lambda_data, self.lambda_ms = lambda_data, lambda_ms
        self.body_boost, self.e_rel = body_boost, e_rel
        self.depth_boost, self.halo_boost = depth_boost, halo_boost
        self.forward_op = forward_op

    def forward(self, m_pred, m_true, d_clean=None, z_obs=None):
        D = m_true.shape[2]
        a = m_true.abs().amax(dim=(1, 2, 3, 4), keepdim=True).clamp_min(0.05)
        pn, tn = m_pred / a, m_true / a
        fg_mask = (m_true != 0).float()

        # Depth weighting w_z in [1.0, 1.0 + depth_boost] to sharpen bottom contacts & deep bodies
        z_rel = torch.linspace(0.0, 1.0, D, device=m_true.device, dtype=m_true.dtype).view(1, 1, D, 1, 1)
        w_z = 1.0 + self.depth_boost * z_rel

        # Per-voxel local contrast normalisation on foreground cells so weak rock bodies in
        # multi-body scenes are weighted equally with cavities, plus explicit peak overshoot penalty
        t_loc = tn.abs().clamp_min(0.05)
        e_loc = (pn - tn).abs() / t_loc
        overshoot = F.relu(pn.abs() - tn.abs()) / t_loc
        fg_w = fg_mask * w_z
        fg_cnt = fg_w.flatten(1).sum(1).clamp_min(1.0)
        err = e_loc + 0.5 * e_loc ** 2 + 0.5 * overshoot
        l_fg = ((err * fg_w).flatten(1).sum(1) / fg_cnt).mean()

        with torch.no_grad():
            d1 = _dilate3d(fg_mask, 1)
            d3 = _dilate3d(fg_mask, 3)
            d7 = _dilate3d(fg_mask, 7)
            # Distance penalty: 1.0 on body -> 2.5 (r=1) -> 4.0 (r=2..3) -> 6.0 (r=4..7) -> 10.0 (r>7)
            w_dist = 1.0 + 1.5 * (1.0 - fg_mask) + 1.5 * (1.0 - d1) + 2.0 * (1.0 - d3) + 4.0 * (1.0 - d7)

            # Sign-aware proximity zones around positive and negative bodies
            pos_mask = (tn > 0).float()
            neg_mask = (tn < 0).float()
            pos_prox = _dilate3d(pos_mask, 1) + _dilate3d(pos_mask, 3) + _dilate3d(pos_mask, 7)
            neg_prox = _dilate3d(neg_mask, 1) + _dilate3d(neg_mask, 3) + _dilate3d(neg_mask, 7)
            only_pos = (neg_mask.amax(dim=(1, 2, 3, 4), keepdim=True) == 0).float()
            only_neg = (pos_mask.amax(dim=(1, 2, 3, 4), keepdim=True) == 0).float()
            pos_zone = (((pos_prox > neg_prox).float() + only_pos).clamp_max(1.0)) * (tn >= 0).float()
            neg_zone = (((neg_prox > pos_prox).float() + only_neg).clamp_max(1.0)) * (tn <= 0).float()

        # Opposite-sign compensating halo: negative voxels around a positive body or vice versa
        opp_mag = F.relu(-pn) * pos_zone + F.relu(pn) * neg_zone
        opp_prox_w = 1.0 + pos_prox * pos_zone + neg_prox * neg_zone
        sum_l1 = pn.abs().flatten(1).sum(1) + tn.abs().flatten(1).sum(1) + 1e-6
        l_halo = (self.halo_boost * (opp_mag * opp_prox_w).flatten(1).sum(1) / sum_l1).mean()

        w_tot = w_dist + self.halo_boost * (opp_mag > 0).float()
        diff_l1 = ((pn - tn).abs() * w_tot).flatten(1).sum(1)
        l_rel_l1 = (diff_l1 / sum_l1).mean()

        # Signed L1-IoU (Tanimoto) loss: unique global optimum at p == t (no 1/sqrt(f) core overshoot)
        p_aligned = F.relu(pn * torch.sign(tn))
        inter = torch.minimum(p_aligned, tn.abs()).flatten(1).sum(1)
        l_dice = (1.0 - (2.0 * inter + 1e-6) / sum_l1).clamp_min(0.0).mean()

        dp, dt = _grad3d(pn), _grad3d(tn)
        wz_faces = (0.5 * (w_z[:, :, 1:] + w_z[:, :, :-1]), w_z, w_z)
        g_diff = sum(((p - t).abs() * wz).flatten(1).sum(1) for p, t, wz in zip(dp, dt, wz_faces))
        g_sum = sum(((p.abs() + t.abs()) * wz).flatten(1).sum(1) for p, t, wz in zip(dp, dt, wz_faces)) + 1e-6
        c_err = sum(((p - t).abs() * (t != 0).float() * wz).flatten(1).sum(1) for p, t, wz in zip(dp, dt, wz_faces))
        c_cnt = sum(((t != 0).float() * wz).flatten(1).sum(1) for t, wz in zip(dt, wz_faces)).clamp_min(1.0)
        l_grad = 0.5 * ((g_diff / g_sum).mean() + (c_err / c_cnt).mean())

        l_sup = 0.25 * (l_fg + l_rel_l1 + l_dice + l_grad) + l_halo

        l_data = torch.zeros((), device=m_pred.device)
        if self.forward_op is not None and d_clean is not None:
            d_pred = self.forward_op(m_pred, z_obs)
            num = ((d_pred - d_clean) ** 2).flatten(1).sum(1)
            den = (d_clean ** 2).flatten(1).sum(1) + 1e-12
            l_data = (num / den).mean()

        total = l_sup + self.lambda_data * l_data
        return total, {"loss_total": total.item(), "loss_sup": l_sup.item(),
                       "loss_data": l_data.item(), "loss_ms": l_rel_l1.item(),
                       "loss_grad": l_grad.item(), "loss_halo": l_halo.item()}
