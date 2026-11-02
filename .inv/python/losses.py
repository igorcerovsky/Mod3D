"""Physics-Informed & Zhdanov Focusing Loss Functions for 3D Potential Field Inversion.

Implements:
1. Data Misfit Loss (via differentiable forward approximation).
2. Zhdanov Minimum Support (MS) loss for compact anomaly recovery.
3. Zhdanov Minimum Gradient Support (MGS) loss for sharp blocky lithological contacts.
4. Total compound objective with curriculum annealing.

References:
- Portniaguine, O. & Zhdanov, M. S. (1999). Focusing geophysical inversion images.
- Zhdanov, M. S. (2002). Geophysical Inverse Theory and Regularization Problems.
"""

import torch
import torch.nn as nn
import torch.nn.functional as F


class ZhdanovFocusingLoss(nn.Module):
    """Computes Portniaguine & Zhdanov's Minimum Support and Minimum Gradient Support."""

    def __init__(self, e=1e-3, e_grad=1e-3):
        super().__init__()
        self.e = e
        self.e_grad = e_grad

    def minimum_support(self, m: torch.Tensor, e: float = None) -> torch.Tensor:
        """S_MS(m) = sum (m^2 / (m^2 + e^2)) / M

        Approximates the anomalous volume (L0 pseudo-norm).
        """
        eps = e if e is not None else self.e
        m2 = m ** 2
        return torch.mean(m2 / (m2 + eps ** 2))

    def minimum_gradient_support(self, m: torch.Tensor, e_grad: float = None) -> torch.Tensor:
        """S_MGS(m) = sum (||grad m||^2 / (||grad m||^2 + e_grad^2)) / M

        Penalizes transition zone area, creating blocky, razor-sharp boundaries.
        """
        eps_grad = e_grad if e_grad is not None else self.e_grad

        # 3D spatial finite differences: (B, C, D, H, W)
        grad_d = m[:, :, 1:, :, :] - m[:, :, :-1, :, :]
        grad_h = m[:, :, :, 1:, :] - m[:, :, :, :-1, :]
        grad_w = m[:, :, :, :, 1:] - m[:, :, :, :, :-1]

        # Pad to match dimensions
        grad_d = F.pad(grad_d, (0, 0, 0, 0, 0, 1))
        grad_h = F.pad(grad_h, (0, 0, 0, 1, 0, 0))
        grad_w = F.pad(grad_w, (0, 1, 0, 0, 0, 0))

        grad_norm_sq = grad_d ** 2 + grad_h ** 2 + grad_w ** 2
        return torch.mean(grad_norm_sq / (grad_norm_sq + eps_grad ** 2))

    def forward(self, m_pred: torch.Tensor, e: float = None, e_grad: float = None):
        ms = self.minimum_support(m_pred, e)
        mgs = self.minimum_gradient_support(m_pred, e_grad)
        return ms, mgs


class CompoundInversionLoss(nn.Module):
    """Compound objective combining:

    1. Supervised Model Loss (L1/L2 vs Ground Truth if available)
    2. Data Misfit Loss (Forward response vs Observed field)
    3. Zhdanov Minimum Support Loss (Compactness)
    4. Zhdanov Minimum Gradient Support Loss (Sharp edges)
    """

    def __init__(self, lambda_sup=1.0, lambda_data=0.5, lambda_ms=0.05, lambda_mgs=0.02):
        super().__init__()
        self.lambda_sup = lambda_sup
        self.lambda_data = lambda_data
        self.lambda_ms = lambda_ms
        self.lambda_mgs = lambda_mgs
        self.focusing = ZhdanovFocusingLoss()

    def forward(self, m_pred, m_true, d_obs=None, d_pred=None, epoch=1, total_epochs=50):
        # 1. Supervised loss
        l_sup = F.smooth_l1_loss(m_pred, m_true)

        # 2. Data misfit loss (if forward field provided)
        l_data = torch.tensor(0.0, device=m_pred.device)
        if d_obs is not None and d_pred is not None:
            l_data = F.mse_loss(d_pred, d_obs)

        # 3. Annealing curriculum for Zhdanov focusing threshold e:
        # e decreases over epochs: 0.1 -> 0.001
        progress = min(1.0, max(0.0, (epoch - 1) / max(1, total_epochs - 1)))
        e_curr = 0.1 * math.pow(0.01, progress) # Exponential decay
        e_grad_curr = 0.1 * math.pow(0.01, progress)

        ms_loss, mgs_loss = self.focusing(m_pred, e=e_curr, e_grad=e_grad_curr)

        total_loss = (
            self.lambda_sup * l_sup
            + self.lambda_data * l_data
            + self.lambda_ms * ms_loss
            + self.lambda_mgs * mgs_loss
        )

        return total_loss, {
            "loss_total": total_loss.item(),
            "loss_sup": l_sup.item(),
            "loss_data": l_data.item(),
            "loss_ms": ms_loss.item(),
            "loss_mgs": mgs_loss.item(),
            "annealed_e": e_curr,
        }
