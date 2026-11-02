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
    """Supervised + physics data misfit + Zhdanov focusing, with a curriculum on e.

    1. Weighted L1 to the ground truth (anomalous cells weighted ``1 + anomaly_boost``): L1, not
       smooth-L1/L2, so the network is not pulled towards blurred means.
    2. Relative data misfit ``||G m_pred - d_obs||^2 / ||d_obs||^2`` through the physical forward
       operator ``forward_op`` (per sample, averaged over the batch). Disabled if forward_op is None.
    3. S_MS and S_MGS with e annealed geometrically e_start -> e_end over the epochs [g/cm^3].
    """

    def __init__(self, lambda_sup=1.0, lambda_data=1.0, lambda_ms=0.02, lambda_mgs=0.01,
                 e_start=0.1, e_end=0.005, anomaly_boost=4.0, spacing=(1.0, 1.0, 1.0),
                 forward_op=None):
        super().__init__()
        self.lambda_sup, self.lambda_data = lambda_sup, lambda_data
        self.lambda_ms, self.lambda_mgs = lambda_ms, lambda_mgs
        self.e_start, self.e_end = e_start, e_end
        self.anomaly_boost = anomaly_boost
        self.forward_op = forward_op
        self.focusing = ZhdanovFocusingLoss(spacing)

    def e_at(self, epoch, total_epochs):
        progress = min(1.0, max(0.0, (epoch - 1) / max(1, total_epochs - 1)))
        return self.e_start * math.pow(self.e_end / self.e_start, progress)

    def forward(self, m_pred, m_true, d_obs=None, epoch=1, total_epochs=50):
        weights = 1.0 + self.anomaly_boost * (m_true.abs() > 0.05).float()
        l_sup = torch.mean(weights * (m_pred - m_true).abs())

        l_data = torch.zeros((), device=m_pred.device)
        if self.forward_op is not None and d_obs is not None:
            d_pred = self.forward_op(m_pred)
            num = ((d_pred - d_obs) ** 2).flatten(1).sum(dim=1)
            den = (d_obs ** 2).flatten(1).sum(dim=1) + 1e-12
            l_data = torch.mean(num / den)

        e_curr = self.e_at(epoch, total_epochs)
        ms_loss, mgs_loss = self.focusing(m_pred, e_curr)

        total = (self.lambda_sup * l_sup + self.lambda_data * l_data
                 + self.lambda_ms * ms_loss + self.lambda_mgs * mgs_loss)
        return total, {
            "loss_total": total.item(), "loss_sup": l_sup.item(), "loss_data": l_data.item(),
            "loss_ms": ms_loss.item(), "loss_mgs": mgs_loss.item(), "annealed_e": e_curr,
        }
