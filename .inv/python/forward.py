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

    def __init__(self, nx, ny, nz, dx=50.0, dy=50.0, dz=50.0, z_obs=-0.35, z0=0.0):
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


class TerrainGravityOperator:
    """Differentiable ``d = G m`` for stations DRAPED on a terrain (one station per cell column).

    Station ``(i, j)`` sits at depth ``z_obs[i, j]`` (z down, metres below the grid top). The field
    is evaluated EXACTLY (Nagy prisms, FFT per layer) on a stack of horizontal levels spaced
    ``step`` apart and linearly interpolated to each station's depth. The Nagy formula is valid for
    prisms above or below the level, so levels may cut through the relief. Requirement: no
    NON-ZERO cell may contain a station (true when density is zero above the terrain).

    Accuracy: <1 % vs. brute force for sources >= one level step below the station
    (``test_terrain_operator_matches_brute_force``).
    """

    def __init__(self, nx, ny, nz, dx=50.0, dy=50.0, dz=50.0, h_min=-25.0, h_max=None, step=None):
        self.nx, self.ny, self.nz = nx, ny, nz
        self.dx, self.dy, self.dz = dx, dy, dz
        self.step = float(step if step is not None else dz / 8.0)
        h_max = float(h_max if h_max is not None else 0.25 * nz * dz + 1.0)
        # Anchor level grid at -0.35 m (35 cm ground gravimeter clearance) so flat/layer-aligned
        # ground stations sit directly on a level and no level coincides with a layer boundary (w=0).
        self.h0 = -0.35 - 2.0 * self.step
        self.n_levels = int(np.ceil((h_max - self.h0) / self.step)) + 2
        self.px, self.py = 2 * nx, 2 * ny
        pyh = self.py // 2 + 1

        di = np.arange(-(nx - 1), nx)
        dj = np.arange(-(ny - 1), ny)
        x1 = (di * dx - 0.5 * dx)[:, None]
        y1 = (dj * dy - 0.5 * dy)[None, :]
        ix = (di % self.px)[:, None]
        iy = (dj % self.py)[None, :]
        # w1 = iz*dz - h_k takes few distinct values -> compute each prism plane once
        levels = self.h0 + self.step * np.arange(self.n_levels)
        w1_all = np.arange(nz)[None, :] * dz - levels[:, None]          # (K, nz)
        keys = np.round(w1_all / self.step * 1e3).astype(np.int64)
        uniq, inv = np.unique(keys, return_inverse=True)
        inv = inv.reshape(w1_all.shape)
        khat_u = []
        with np.errstate(divide="ignore", invalid="ignore"):
            for u in uniq:
                w1 = u * self.step / 1e3
                k = prism_gz(x1, x1 + dx, y1, y1 + dy, w1, w1 + dz)      # (2nx-1, 2ny-1)
                kpad = np.zeros((self.px, self.py), dtype=np.float64)
                kpad[ix, iy] = np.nan_to_num(k)
                khat_u.append(np.fft.rfft2(kpad))
        khat_u = np.stack(khat_u)                                        # (U, px, pyh)
        khat = khat_u[inv]                                               # (K, nz, px, pyh)
        # (F, nz, K) real/imag blocks for batched real matmuls (complex bmm is not on every backend)
        kf = khat.reshape(self.n_levels, nz, self.px * pyh).transpose(2, 1, 0)
        self.k_re = torch.from_numpy(np.ascontiguousarray(kf.real, dtype=np.float32))
        self.k_im = torch.from_numpy(np.ascontiguousarray(kf.imag, dtype=np.float32))

    def to(self, device):
        self.k_re, self.k_im = self.k_re.to(device), self.k_im.to(device)
        return self

    def __call__(self, rho, z_obs):
        """rho: (B, 1, nz, nx, ny) g/cm^3; z_obs: (B, nx, ny) station depth [m] -> (B, 1, nx, ny) mGal."""
        b = rho.shape[0]
        pyh = self.py // 2 + 1
        pos = ((z_obs - self.h0) / self.step).clamp(0.0, self.n_levels - 1 - 1e-4)
        i0 = torch.floor(pos)
        frac = (pos - i0).unsqueeze(1)
        i0 = i0.long()
        kmin, kmax = int(i0.min()), int(i0.max()) + 1
        kr = self.k_re[:, :, kmin:kmax + 1]
        ki = self.k_im[:, :, kmin:kmax + 1]

        x = rho.reshape(b, self.nz, self.nx, self.ny)
        x = torch.nn.functional.pad(x, (0, self.py - self.ny, 0, self.px - self.nx))
        xf = torch.fft.rfft2(x).reshape(b, self.nz, -1).permute(2, 0, 1)   # (F, B, nz)
        xr, xi = xf.real.contiguous(), xf.imag.contiguous()
        dr = torch.bmm(xr, kr) - torch.bmm(xi, ki)                         # (F, B, K')
        di = torch.bmm(xr, ki) + torch.bmm(xi, kr)
        df = torch.complex(dr, di).permute(1, 2, 0).reshape(b, -1, self.px, pyh)
        d = torch.fft.irfft2(df, s=(self.px, self.py))[:, :, : self.nx, : self.ny]   # (B, K', nx, ny)
        j0 = (i0 - kmin).unsqueeze(1)
        lo = torch.gather(d, 1, j0)
        hi = torch.gather(d, 1, j0 + 1)
        return lo * (1.0 - frac) + hi * frac

    def forward_numpy(self, rho, z_obs):
        with torch.no_grad():
            dev = self.k_re.device
            r = torch.from_numpy(np.ascontiguousarray(rho, dtype=np.float32))[None, None].to(dev)
            z = torch.from_numpy(np.ascontiguousarray(z_obs, dtype=np.float32))[None].to(dev)
            return self(r, z)[0, 0].cpu().numpy()
