"""Physical gravity forward operator (Nagy prisms, FFT block-Toeplitz) for training and inference.

This is the Python twin of ``inv::ToeplitzGravityOperator`` (C++): one observation station above
the centre of every cell column, flat observation plane, exact right-rectangular-prism response
(Nagy 1966) per depth layer and horizontal offset, applied with 2-D FFTs.

Conventions (identical to the C++ engine)
-----------------------------------------
* ``z`` points DOWN; the grid top is ``z0``; stations sit at ``z_obs < z0`` (default 10 m above).
* Density contrast in g/cm^3, gravity ``gz`` in mGal, downward-positive.
* Tensors: density ``(B, 1, D=nz, H=nx, W=ny)``, observations ``(B, 1, H=nx, W=ny)``.

NOTE: PLANAR modelling only (uniform plumb line), same restriction as the C++ kernels.
"""

import numpy as np
import torch

G_MGAL = 6.6743e-3  # mGal per (g/cm^3 * m) -- same constant as inv::ForwardKernel


def prism_gz(u1, u2, v1, v2, w1, w2):
    """Vertical attraction (mGal per g/cm^3) of prisms with corner offsets from the station.

    Arguments are broadcastable arrays of (x, y, z) offsets of the two prism corners relative to the
    station (z down; requires w1, w2 > 0, i.e. the station is above the prism).
    """
    u = (np.asarray(u1, dtype=np.float64), np.asarray(u2, dtype=np.float64))
    v = (np.asarray(v1, dtype=np.float64), np.asarray(v2, dtype=np.float64))
    w = (np.asarray(w1, dtype=np.float64), np.asarray(w2, dtype=np.float64))
    total = 0.0
    for i in range(2):
        for j in range(2):
            for k in range(2):
                # index 0 -> prism minimum corner (sign -1), index 1 -> maximum corner (+1)
                s = (1.0 if i else -1.0) * (1.0 if j else -1.0) * (1.0 if k else -1.0)
                uu, vv, ww = u[i], v[j], w[k]
                r = np.sqrt(uu * uu + vv * vv + ww * ww)
                term = uu * np.log(r + vv) + vv * np.log(r + uu) - ww * np.arctan(uu * vv / (ww * r))
                total = total + s * term
    return -G_MGAL * total


class GravityFFTOperator:
    """Differentiable ``d = G m`` for a regular grid, evaluated with ``torch.fft`` (O(nz N log N))."""

    def __init__(self, nx, ny, nz, dx=50.0, dy=50.0, dz=50.0, z_obs=-10.0, z0=0.0):
        if z_obs >= z0:
            raise ValueError("stations must be above the grid top (z_obs < z0)")
        self.nx, self.ny, self.nz = nx, ny, nz
        self.dx, self.dy, self.dz, self.z_obs, self.z0 = dx, dy, dz, z_obs, z0
        self.px, self.py = 2 * nx, 2 * ny  # >= 2n-1, avoids circular wrap-around

        di = np.arange(-(nx - 1), nx)
        dj = np.arange(-(ny - 1), ny)
        iz = np.arange(nz)
        x1 = (di * dx - 0.5 * dx)[None, :, None]
        x2 = x1 + dx
        y1 = (dj * dy - 0.5 * dy)[None, None, :]
        y2 = y1 + dy
        w1 = (z0 + iz * dz - z_obs)[:, None, None]
        w2 = w1 + dz
        k = prism_gz(x1, x2, y1, y2, w1, w2)  # (nz, 2nx-1, 2ny-1)

        kpad = np.zeros((nz, self.px, self.py), dtype=np.float64)
        ix = (di % self.px)[:, None]
        iy = (dj % self.py)[None, :]
        kpad[:, ix, iy] = k
        self.kernel = torch.from_numpy(kpad.astype(np.float32))
        self.khat = torch.fft.rfft2(self.kernel)  # (nz, px, py//2+1)
        self._device = torch.device("cpu")

    def to(self, device):
        self.khat = self.khat.to(device)
        self._device = torch.device(device)
        return self

    def __call__(self, rho):
        """rho: (B, 1, nz, nx, ny) -> (B, 1, nx, ny) in mGal."""
        b = rho.shape[0]
        x = rho.reshape(b, self.nz, self.nx, self.ny)
        x = torch.nn.functional.pad(x, (0, self.py - self.ny, 0, self.px - self.nx))
        xf = torch.fft.rfft2(x)  # (B, nz, px, py//2+1)
        df = (xf * self.khat.unsqueeze(0)).sum(dim=1)
        d = torch.fft.irfft2(df, s=(self.px, self.py))
        return d[:, : self.nx, : self.ny].unsqueeze(1)

    def forward_numpy(self, rho):
        """Convenience for a single (nz, nx, ny) numpy model -> (nx, ny) numpy field."""
        with torch.no_grad():
            t = torch.from_numpy(np.ascontiguousarray(rho, dtype=np.float32))[None, None]
            return self(t.to(self._device))[0, 0].cpu().numpy()
