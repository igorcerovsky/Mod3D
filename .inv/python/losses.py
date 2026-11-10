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


class CompoundInversionLoss(nn.Module):
    """Scale-normalised supervised + physics loss for isolated-body inversion.

    Every term is normalised PER SAMPLE by ``a = max|m_true|`` and by the active body support, so
    a weak +0.1 g/cm^3 body and a -2.67 g/cm^3 cavity of any size contribute equally without
    collapsing to the trivial zero model:

    1. Foreground L1+L2 error averaged over the true body cells.
    2. Symmetric relative L1 (Bray-Curtis) ``||p - t||_1 / (||p||_1 + ||t||_1)``: penalises
       false-positive halos outside the body at the exact scale of the body volume.
    3. Signed soft Dice ``1 - 2 <p, t> / (||p||_2^2 + ||t||_2^2)``: aligns support, sign, and amplitude.
    4. Relative data misfit ``||G m_pred - d_clean||^2 / ||d_clean||^2`` through the draped-station
       operator ``forward_op(m, z_obs)`` (disabled if None).
    """

    def __init__(self, lambda_data=0.25, lambda_ms=0.05, body_boost=9.0, e_rel=0.05, forward_op=None):
        super().__init__()
        self.lambda_data, self.lambda_ms = lambda_data, lambda_ms
        self.body_boost, self.e_rel = body_boost, e_rel
        self.forward_op = forward_op

    def forward(self, m_pred, m_true, d_clean=None, z_obs=None):
        a = m_true.abs().amax(dim=(1, 2, 3, 4), keepdim=True).clamp_min(0.05)
        pn, tn = m_pred / a, m_true / a
        fg_mask = (m_true != 0).float()
        fg_cnt = fg_mask.flatten(1).sum(1).clamp_min(1.0)
        err = (pn - tn).abs() + 0.5 * (pn - tn) ** 2
        l_fg = ((err * fg_mask).flatten(1).sum(1) / fg_cnt).mean()

        diff_l1 = (pn - tn).abs().flatten(1).sum(1)
        sum_l1 = pn.abs().flatten(1).sum(1) + tn.abs().flatten(1).sum(1) + 1e-6
        l_rel_l1 = (diff_l1 / sum_l1).mean()

        inter = (pn * tn).flatten(1).sum(1)
        denom = (pn * pn).flatten(1).sum(1) + (tn * tn).flatten(1).sum(1) + 1e-6
        l_dice = (1.0 - (2.0 * inter + 1e-6) / denom).clamp_min(0.0).mean()
        l_sup = (l_fg + l_rel_l1 + l_dice) / 3.0

        l_data = torch.zeros((), device=m_pred.device)
        if self.forward_op is not None and d_clean is not None:
            d_pred = self.forward_op(m_pred, z_obs)
            num = ((d_pred - d_clean) ** 2).flatten(1).sum(1)
            den = (d_clean ** 2).flatten(1).sum(1) + 1e-12
            l_data = (num / den).mean()

        total = l_sup + self.lambda_data * l_data
        return total, {"loss_total": total.item(), "loss_sup": l_sup.item(),
                       "loss_data": l_data.item(), "loss_ms": l_rel_l1.item()}
