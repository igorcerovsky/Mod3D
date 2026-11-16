"""3D Swin / U-Net Transformers for gravity inversion of cavities and rock bodies below terrain.

Provides:
* ``PotentialFieldTransformer3D`` (v2, default): Multi-resolution 2D U-Net + scale-calibrated
  3D Swin Transformer + learnable 7-class Lithology Prototype Embeddings (background, air cavity
  at exact -2.67 g/cm^3, water cavity at exact -1.67 g/cm^3, light rock/coal [-1.55, -0.85],
  sediment/waste [-0.80, -0.10], mafic/ultramafic [+0.15, +0.75], high-grade ore [+0.80, +1.85])
  + progressive trilinear & separable 2.5D spatial decoder.
* ``PotentialFieldTransformer3DV1``: Legacy amplitude-equivariant architecture retained for
  evaluating or exporting v1 checkpoints.
* ``create_model``: Dispatches cleanly from ``arch`` config dicts saved in checkpoints.

Input  (B, 2, H=64, W=64):  channel 0 = gz [mGal] on draped stations (rescaled to 50 m equivalent),
                            channel 1 = terrain depth below the grid top / domain depth (0 ... 0.25).
Output (B, 1, D=32, H, W):  density contrast [g/cm^3] for the TRAINING geometry (50 m cells),
                            strictly masked to zero above the terrain surface.
"""

import math

import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F

from dataset import (
    RHO_AIR,
    RHO_LIGHT_MAX,
    RHO_LIGHT_MIN,
    RHO_MAFIC_MAX,
    RHO_MAFIC_MIN,
    RHO_ORE_MAX,
    RHO_ORE_MIN,
    RHO_SEDIMENT_MAX,
    RHO_SEDIMENT_MIN,
    RHO_WATER,
)
from forward import prism_gz

G_MGAL = 6.6743e-3
REF_CELL = 50.0
SLAB = 2.0 * math.pi * G_MGAL * REF_CELL     # mGal of a 1 g/cm^3 infinite slab one cell thick
CLASS_NAMES = ("bg", "air", "water", "light", "sediment", "mafic", "ore")
_ROCK_BOUNDS = (
    (RHO_LIGHT_MIN, RHO_LIGHT_MAX),
    (RHO_SEDIMENT_MIN, RHO_SEDIMENT_MAX),
    (RHO_MAFIC_MIN, RHO_MAFIC_MAX),
    (RHO_ORE_MIN, RHO_ORE_MAX),
)
# V6 closes the blind density gap in [-0.90, -0.80] and extends ore range to +1.95
RHO_MAX_V6 = 1.95
_ROCK_BOUNDS_V6 = (
    (-1.60, -0.80),   # light (covers negative felsic / deep lows)
    (-0.85, -0.03),   # sediment (covers landfill [-0.85, -0.05] & soft sediments without 0.0 collapse)
    (0.03, 0.80),     # mafic (covers weak mafic [+0.05, +0.80] without 0.0 collapse)
    (0.75, 1.95),     # ore / dense ultramafic
)


class GreensVerticalBias(nn.Module):
    """Learnable Green's-function vertical distance decay bias for 3D depth attention.

    Initialized to ``gain / (1 + 0.1 * dz^2)`` along the vertical axis so attention
    respects the vertical 1/r^2 decay of potential-field kernels while remaining trainable.
    """

    def __init__(self, heads: int, dp: int, wh: int = 1, ww: int = 1):
        super().__init__()
        z_1d = torch.arange(dp, dtype=torch.float32)
        z_blk = z_1d[:, None, None].expand(dp, wh, ww).reshape(-1)
        self.register_buffer("dz2", (z_blk[:, None] - z_blk[None, :]) ** 2, persistent=False)
        self.register_buffer("dz_signed", (z_blk[None, :] - z_blk[:, None]) / max(float(dp), 1.0),
                             persistent=False)
        init_raw = math.log(math.exp(0.1) - 1.0)
        self.raw_decay = nn.Parameter(torch.full((heads, 1, 1), init_raw))
        self.gain = nn.Parameter(torch.full((heads, 1, 1), 0.5))
        self.asym = nn.Parameter(torch.zeros((heads, 1, 1)))

    def forward(self) -> torch.Tensor:
        gamma = F.softplus(self.raw_decay)
        decay = 1.0 / (1.0 + gamma * self.dz2.unsqueeze(0))
        return (self.gain + self.asym * self.dz_signed.unsqueeze(0)) * decay


class Attention(nn.Module):
    def __init__(self, dim, heads, dropout: float = 0.0):
        super().__init__()
        self.h = heads
        self.dropout = float(dropout)
        self.q = nn.Linear(dim, dim)
        self.kv = nn.Linear(dim, 2 * dim)
        self.o = nn.Linear(dim, dim)

    def forward(self, x, ctx, attn_bias: torch.Tensor | None = None):
        b, n, c = x.shape
        m = ctx.shape[1]
        dh = c // self.h
        q = self.q(x).view(b, n, self.h, dh).transpose(1, 2)
        k, v = self.kv(ctx).view(b, m, 2, self.h, dh).permute(2, 0, 3, 1, 4)
        if attn_bias is None:
            out = F.scaled_dot_product_attention(q, k, v, dropout_p=self.dropout if self.training else 0.0)
        else:
            scores = torch.matmul(q, k.transpose(-2, -1)) * (dh ** -0.5)
            scores = scores + (attn_bias.unsqueeze(0) if attn_bias.dim() == 3 else attn_bias)
            attn = F.softmax(scores, dim=-1)
            if self.dropout > 0.0 and self.training:
                attn = F.dropout(attn, p=self.dropout, training=True)
            out = torch.matmul(attn, v)
        return self.o(out.transpose(1, 2).reshape(b, n, c))


class SwinBlock3D(nn.Module):
    """3D Swin Transformer block: full-depth x (wh, ww) window attention with shifted windows."""

    def __init__(self, dim=96, heads=4, wh=4, ww=4, shift=0, use_greens_bias=False, dp=8, dropout=0.0):
        super().__init__()
        self.wh, self.ww, self.shift = wh, ww, shift
        self.dropout = float(dropout)
        self.n1 = nn.LayerNorm(dim)
        self.attn = Attention(dim, heads, dropout=dropout)
        self.greens_bias = GreensVerticalBias(heads, dp, wh, ww) if use_greens_bias else None
        self.n2 = nn.LayerNorm(dim)
        self.mlp = nn.Sequential(
            nn.Linear(dim, 3 * dim), nn.GELU(), nn.Dropout(dropout) if dropout > 0 else nn.Identity(),
            nn.Linear(3 * dim, dim),
        )

    def forward(self, v):
        b, dp, hp, wp, c = v.shape
        h = self.n1(v.reshape(-1, c)).view(b, dp, hp, wp, c)
        if self.shift > 0:
            h = torch.roll(h, shifts=(-self.shift, -self.shift), dims=(2, 3))
        wh, ww = self.wh, self.ww
        hw = (h.view(b, dp, hp // wh, wh, wp // ww, ww, c)
              .permute(0, 2, 4, 1, 3, 5, 6)
              .reshape(-1, dp * wh * ww, c))
        bias = self.greens_bias() if self.greens_bias is not None else None
        hw = self.attn(hw, hw, attn_bias=bias)
        h = (hw.view(b, hp // wh, wp // ww, dp, wh, ww, c)
             .permute(0, 3, 1, 4, 2, 5, 6)
             .reshape(b, dp, hp, wp, c))
        if self.shift > 0:
            h = torch.roll(h, shifts=(self.shift, self.shift), dims=(2, 3))
        v = v + h
        m = self.mlp(self.n2(v.reshape(-1, c))).view(b, dp, hp, wp, c)
        return v + m


class SepConv25DBlock(nn.Module):
    """MPS- & ONNX-friendly 3D refinement: 2D XY 3x3 conv + vertical Z 3x1 conv with residual skip."""

    def __init__(self, ch: int):
        super().__init__()
        groups = 8 if ch % 8 == 0 else (4 if ch % 4 == 0 else 1)
        self.c_xy = nn.Conv2d(ch, ch, 3, padding=1)
        self.g1 = nn.GroupNorm(groups, ch)
        self.c_z = nn.Conv2d(ch, ch, (3, 1), padding=(1, 0))
        self.g2 = nn.GroupNorm(groups, ch)

    def forward(self, x):
        # x: (B, C, D, H, W)
        b, c, d, h, w = x.shape
        y = F.gelu(self.g1(self.c_xy(x.permute(0, 2, 1, 3, 4).reshape(b * d, c, h, w))))
        y = y.reshape(b, d, c, h, w).permute(0, 3, 2, 1, 4).reshape(b * h, c, d, w)
        y = F.gelu(self.g2(self.c_z(y)))
        y = y.reshape(b, h, c, d, w).permute(0, 2, 3, 1, 4)
        return x + y


class AxialGlobalBlock3D(nn.Module):
    """Global horizontal XY attention per depth layer + full-depth vertical Z column attention."""

    def __init__(self, dim=128, heads=4, use_greens_bias=False, dp=8, dropout=0.0):
        super().__init__()
        self.n1 = nn.LayerNorm(dim)
        self.attn_xy = Attention(dim, heads, dropout=dropout)
        self.nz = nn.LayerNorm(dim)
        self.attn_z = Attention(dim, heads, dropout=dropout)
        self.greens_bias = GreensVerticalBias(heads, dp, 1, 1) if use_greens_bias else None
        self.n2 = nn.LayerNorm(dim)
        self.mlp = nn.Sequential(
            nn.Linear(dim, 3 * dim), nn.GELU(), nn.Dropout(dropout) if dropout > 0 else nn.Identity(),
            nn.Linear(3 * dim, dim),
        )

    def forward(self, v):
        b, dp, hp, wp, c = v.shape
        h = self.n1(v.reshape(-1, c)).reshape(b * dp, hp * wp, c)
        v = v + self.attn_xy(h, h).view(b, dp, hp, wp, c)
        hz = self.nz(v.reshape(-1, c)).view(b, dp, hp, wp, c).permute(0, 2, 3, 1, 4).reshape(b * hp * wp, dp, c)
        zb = self.greens_bias() if self.greens_bias is not None else None
        v = v + self.attn_z(hz, hz, attn_bias=zb).view(b, hp, wp, dp, c).permute(0, 3, 1, 2, 4)
        return v + self.mlp(self.n2(v.reshape(-1, c))).view(b, dp, hp, wp, c)


class SepSubpixelUp3D(nn.Module):
    """ONNX/MPS-native 2x (D, H, W) upsampler: trilinear base + learned 2.5D subpixel edge detail."""

    def __init__(self, c_in: int, c_out: int):
        super().__init__()
        self.c_out = c_out
        self.base = nn.Conv2d(c_in, c_out, 1)
        self.xy = nn.Conv2d(c_in, 4 * c_out, 3, padding=1)
        self.z = nn.Conv2d(c_out, 2 * c_out, (3, 1), padding=(1, 0))

    def forward(self, x):
        b, c_in, d, h, w = x.shape
        co = self.c_out
        xb = self.base(x.reshape(b, c_in, d * h, w)).reshape(b, co, d, h, w)
        xb = F.interpolate(xb, size=(2 * d, 2 * h, 2 * w), mode="trilinear", align_corners=False)

        xy = F.pixel_shuffle(self.xy(x.permute(0, 2, 1, 3, 4).reshape(b * d, c_in, h, w)), 2)
        xz = xy.reshape(b, d, co, 2 * h, 2 * w).permute(0, 3, 2, 1, 4).reshape(b * 2 * h, co, d, 2 * w)
        xz = self.z(xz)
        detail = (xz.reshape(b, 2 * h, 2, co, d, 2 * w)
                  .permute(0, 3, 4, 2, 1, 5)
                  .reshape(b, co, 2 * d, 2 * h, 2 * w))
        return xb + 0.5 * F.gelu(detail)


class DepthConditionedSkip(nn.Module):
    """Project a 2D U-Net skip to 3D modulated dynamically by terrain-relative depth (z - terr)."""

    def __init__(self, c_2d: int, ch_out: int, nz_stage: int):
        super().__init__()
        self.ch_out = ch_out
        self.proj = nn.Conv2d(c_2d, ch_out, 1)
        self.z_gate = nn.Parameter(torch.ones(1, ch_out, nz_stage, 1, 1))
        self.d_gate = nn.Sequential(nn.Conv2d(2, ch_out, 1), nn.GELU(), nn.Conv2d(ch_out, ch_out, 1))
        self.d_bias = nn.Conv2d(2, ch_out, 1)

    def forward(self, f_2d, d_surf, z_top_stage):
        b, _, d, h, w = d_surf.shape
        dt_flat = torch.cat([d_surf, z_top_stage.expand(b, 1, d, h, w)], dim=1).reshape(b, 2, d * h, w)
        gate = torch.sigmoid(self.d_gate(dt_flat)).reshape(b, self.ch_out, d, h, w)
        bias = self.d_bias(dt_flat).reshape(b, self.ch_out, d, h, w)
        return self.proj(f_2d).unsqueeze(2) * self.z_gate * gate + bias


class PotentialFieldTransformer3DV1(nn.Module):
    """Legacy amplitude-equivariant v1 3D Swin Transformer (retained for older checkpoints)."""

    def __init__(self, nx=64, ny=64, nz=32, embed_dim=96, num_heads=4, depth=4, patch=4):
        super().__init__()
        assert nx % (4 * patch) == 0 and ny % (4 * patch) == 0 and nz % 4 == 0
        self.nx, self.ny, self.nz, self.p = nx, ny, nz, patch
        self.hp, self.wp, self.dp = nx // patch, ny // patch, nz // 4
        c = embed_dim
        self.embed = nn.Sequential(
            nn.Conv2d(4, c // 2, 3, padding=1), nn.GELU(),
            nn.Conv2d(c // 2, c, patch, stride=patch),
        )
        self.pos2d = nn.Parameter(torch.randn(1, self.hp * self.wp, c) * 0.02)
        self.n2d_1 = nn.LayerNorm(c)
        self.attn2d = Attention(c, num_heads)
        self.n2d_2 = nn.LayerNorm(c)
        self.mlp2d = nn.Sequential(nn.Linear(c, 3 * c), nn.GELU(), nn.Linear(3 * c, c))

        self.depth_lift = nn.Sequential(nn.LayerNorm(c), nn.Linear(c, self.dp * c))
        self.pos3d = nn.Parameter(torch.randn(1, self.dp, self.hp, self.wp, c) * 0.02)
        self.blocks = nn.ModuleList([
            SwinBlock3D(c, num_heads, wh=4, ww=4, shift=(2 if i % 2 else 0))
            for i in range(depth)
        ])
        self.norm = nn.LayerNorm(c)
        ch = 6
        self.ch = ch
        self.up = nn.Linear(c, 4 * ch * patch * patch)
        self.spatial1 = nn.Conv2d(nz * ch, nz * ch, 3, padding=1, groups=nz)
        self.depth_mix = nn.Conv2d(nz * ch, nz * ch, 1)
        self.out_conv = nn.Conv2d(nz * ch, nz, 3, padding=1, groups=nz)
        self.register_buffer("z_top", (torch.arange(nz, dtype=torch.float32) / nz).view(1, 1, nz, 1, 1),
                             persistent=False)

    def forward(self, x, return_aux: bool = False):
        gz, terr = x[:, 0:1], x[:, 1:2]
        s = gz.abs().amax(dim=(2, 3), keepdim=True).clamp_min(1e-6)        # (B,1,1,1)
        gn = gz / s
        smooth3 = F.avg_pool2d(F.pad(gn, (1, 1, 1, 1), mode="replicate"), 3, stride=1)
        smooth9 = F.avg_pool2d(F.pad(gn, (4, 4, 4, 4), mode="replicate"), 9, stride=1)
        lap = (gn - smooth3) * 6.0
        res = (gn - smooth9) * 2.0
        h = torch.cat([gn, lap, res, terr * 4.0], dim=1)
        b = h.shape[0]
        t2 = self.embed(h).flatten(2).transpose(1, 2) + self.pos2d
        t2 = t2 + self.attn2d(self.n2d_1(t2), self.n2d_1(t2))
        t2 = t2 + self.mlp2d(self.n2d_2(t2))
        v = self.depth_lift(t2).view(b, self.hp, self.wp, self.dp, -1).permute(0, 3, 1, 2, 4) + self.pos3d
        for blk in self.blocks:
            v = blk(v)
        u = self.up(self.norm(v)).view(b, self.dp, self.hp, self.wp, 4, self.ch, self.p, self.p)
        u = u.permute(0, 1, 4, 5, 2, 6, 3, 7).reshape(b, self.nz * self.ch, self.nx, self.ny)
        u = u + self.depth_mix(F.gelu(self.spatial1(F.gelu(u))))
        y = self.out_conv(F.gelu(u)).unsqueeze(1)
        mask = (self.z_top >= terr.unsqueeze(2)).to(y.dtype)
        pred = y * mask * (s.unsqueeze(2) / SLAB)
        return (pred, {"mask": mask}) if return_aux else pred


class PotentialFieldTransformer3D(nn.Module):
    """V2 U-Net + 2.5D Swin3D + Scale-Calibrated Lithology Prototype Embedding Inversion Network."""

    def __init__(self, nx=64, ny=64, nz=32, embed_dim=128, num_heads=4, depth=6, patch=4,
                 n_sub=3, ch_mid=24, ch_vox=16, num_classes=len(CLASS_NAMES), **_unused):
        super().__init__()
        assert patch == 4 and nx % (4 * patch) == 0 and ny % (4 * patch) == 0 and nz % 4 == 0
        self.nx, self.ny, self.nz, self.p, self.n_sub = nx, ny, nz, patch, n_sub
        self.num_classes = num_classes
        self.hp, self.wp, self.dp = nx // patch, ny // patch, nz // 4
        c = embed_dim
        c0, c1 = max(24, c // 4), max(48, c // 2)
        self.ch_mid, self.ch_vox = ch_mid, ch_vox

        # Scale conditioning MLP: maps physical [log(s/SLAB), s/(s+SLAB), max(gz)/s, min(gz)/s]
        self.scale_mlp = nn.Sequential(nn.Linear(4, c), nn.GELU(), nn.Linear(c, c))

        # 2D U-Net encoder (64 -> 32 -> 16) over multi-scale potential-field & dynamic-range maps
        self.enc0 = nn.Sequential(
            nn.Conv2d(7, c0, 3, padding=1), nn.GELU(),
            nn.Conv2d(c0, c0, 3, padding=1), nn.GELU(),
        )
        self.enc1 = nn.Sequential(
            nn.Conv2d(c0, c1, 3, stride=2, padding=1), nn.GELU(),
            nn.Conv2d(c1, c1, 3, padding=1), nn.GELU(),
        )
        self.enc2 = nn.Sequential(
            nn.Conv2d(c1, c, 3, stride=2, padding=1), nn.GELU(),
            nn.Conv2d(c, c, 3, padding=1),
        )
        self.pos2d = nn.Parameter(torch.randn(1, self.hp * self.wp, c) * 0.02)
        self.n2d_1 = nn.LayerNorm(c)
        self.attn2d = Attention(c, num_heads)
        self.n2d_2 = nn.LayerNorm(c)
        self.mlp2d = nn.Sequential(nn.Linear(c, 3 * c), nn.GELU(), nn.Linear(3 * c, c))

        # 3D Swin Transformer backbone
        self.depth_lift = nn.Sequential(nn.LayerNorm(c), nn.Linear(c, self.dp * c))
        self.pos3d = nn.Parameter(torch.randn(1, self.dp, self.hp, self.wp, c) * 0.02)
        self.blocks = nn.ModuleList([
            SwinBlock3D(c, num_heads, wh=4, ww=4, shift=(2 if i % 2 else 0))
            for i in range(depth)
        ])

        # Learnable Lithology Prototype Embeddings (7 semantic classes x n_sub sub-prototypes)
        # 0=bg (0.0), 1=air (-2.67), 2=water (-1.67), 3=light [-1.55,-0.85], 4=sediment [-0.80,-0.10],
        # 5=mafic [+0.15,+0.75], 6=ore [+0.80,+1.85]
        self.proto_emb = nn.Parameter(torch.randn(1, num_classes * n_sub, c) * 0.05)
        self.proto_scale = nn.Linear(c, c)
        self.proto_nq = nn.LayerNorm(c)
        self.proto_nk = nn.LayerNorm(c)
        self.proto_attn = Attention(c, num_heads)
        self.proto_mlp_n = nn.LayerNorm(c)
        self.proto_mlp = nn.Sequential(nn.Linear(c, 2 * c), nn.GELU(), nn.Linear(2 * c, c))
        self.norm3d = nn.LayerNorm(c)

        # Progressive 2-stage trilinear + separable 2.5D U-Net decoder
        self.proj_mid = nn.Linear(c, ch_mid)
        self.skip1 = nn.Conv2d(c1, ch_mid, 1)
        self.z_gate_mid = nn.Parameter(torch.ones(1, ch_mid, nz // 2, 1, 1))
        self.dec_mid = SepConv25DBlock(ch_mid)

        self.proj_vox = nn.Conv2d(ch_mid, ch_vox, 1)
        self.skip0 = nn.Conv2d(c0, ch_vox, 1)
        self.z_gate_vox = nn.Parameter(torch.ones(1, ch_vox, nz, 1, 1))
        self.scale_vox = nn.Linear(c, ch_vox)
        self.dec_vox1 = SepConv25DBlock(ch_vox)
        self.dec_vox2 = SepConv25DBlock(ch_vox)

        # Prototype classification head & bounded multi-band rock regression head
        self.proto_head = nn.Linear(c, ch_vox)
        self.proto_bias = nn.Linear(c, num_classes * n_sub)
        self.rock_head = nn.Conv2d(ch_vox, len(_ROCK_BOUNDS), 1)
        self.register_buffer("z_top", (torch.arange(nz, dtype=torch.float32) / nz).view(1, 1, nz, 1, 1),
                             persistent=False)

    @staticmethod
    def _avg(g, k: int):
        p = k // 2
        return F.avg_pool2d(F.pad(g, (p, p, p, p), mode="replicate"), k, stride=1)

    def forward(self, x, return_aux: bool = False):
        b = x.shape[0]
        gz, terr = x[:, 0:1], x[:, 1:2]
        s = gz.abs().amax(dim=(2, 3), keepdim=True).clamp_min(1e-6)
        gn = gz / s
        u8 = 8.0 * gn
        gn_log = torch.log(u8 + torch.sqrt(u8 * u8 + 1.0)) / math.asinh(8.0)

        h_in = torch.cat([
            gn,
            gn_log,
            (gn - self._avg(gn, 3)) * 6.0,
            (gn - self._avg(gn, 9)) * 2.5,
            (gn - self._avg(gn, 17)) * 1.5,
            (gn - self._avg(gn, 25)) * 1.2,
            terr * 4.0,
        ], dim=1)

        s_flat = s.view(b, 1)
        s_slab = s_flat / SLAB
        s_vec = torch.cat([
            torch.log(s_slab + 1e-4) * 0.25,
            s_slab / (s_slab + 1.0),
            gz.amax(dim=(2, 3)).view(b, 1) / s_flat,
            gz.amin(dim=(2, 3)).view(b, 1) / s_flat,
        ], dim=1)
        se = self.scale_mlp(s_vec)                                          # (B, C)

        # Multi-scale 2D U-Net encoder
        f0 = self.enc0(h_in)                                                # (B, c0, nx, ny)
        f1 = self.enc1(f0)                                                  # (B, c1, nx/2, ny/2)
        f2 = self.enc2(f1)                                                  # (B, C, hp, wp)

        t2 = f2.flatten(2).transpose(1, 2) + self.pos2d + se.unsqueeze(1)
        t2 = t2 + self.attn2d(self.n2d_1(t2), self.n2d_1(t2))
        t2 = t2 + self.mlp2d(self.n2d_2(t2))

        v = self.depth_lift(t2).view(b, self.hp, self.wp, self.dp, -1).permute(0, 3, 1, 2, 4)
        v = v + self.pos3d + se.view(b, 1, 1, 1, -1)
        for blk in self.blocks:
            v = blk(v)

        # Cross-attention between 3D spatial tokens and scale-conditioned Lithology Embeddings
        vf = v.reshape(b, self.dp * self.hp * self.wp, -1)
        p_ctx = self.proto_emb + self.proto_scale(se).unsqueeze(1)          # (B, 5*n_sub, C)
        vf = vf + self.proto_attn(self.proto_nq(vf), self.proto_nk(p_ctx))
        vf = vf + self.proto_mlp(self.proto_mlp_n(vf))
        v = self.norm3d(vf).view(b, self.dp, self.hp, self.wp, -1)

        # Stage 1 -> (nz/2, nx/2, ny/2)
        u = self.proj_mid(v).permute(0, 4, 1, 2, 3)
        u = F.interpolate(u, size=(self.nz // 2, self.nx // 2, self.ny // 2), mode="trilinear", align_corners=False)
        u = self.dec_mid(u + self.skip1(f1).unsqueeze(2) * self.z_gate_mid)

        # Stage 2 -> (nz, nx, ny)
        ud = u.permute(0, 2, 1, 3, 4).reshape(b * (self.nz // 2), self.ch_mid, self.nx // 2, self.ny // 2)
        u = self.proj_vox(ud).reshape(b, self.nz // 2, self.ch_vox, self.nx // 2, self.ny // 2).permute(0, 2, 1, 3, 4)
        u = F.interpolate(u, size=(self.nz, self.nx, self.ny), mode="trilinear", align_corners=False)
        hv = u + self.skip0(f0).unsqueeze(2) * self.z_gate_vox + self.scale_vox(se).view(b, self.ch_vox, 1, 1, 1)
        hv = self.dec_vox2(self.dec_vox1(hv))                               # (B, ch_vox, nz, nx, ny)

        # Voxel-prototype scaled dot-product classification -> 7 semantic classes
        p_head = self.proto_head(p_ctx)                                     # (B, K*n_sub, ch_vox)
        hv_flat = hv.reshape(b, self.ch_vox, self.nz * self.nx * self.ny)
        sim = torch.bmm(p_head, hv_flat) * (self.ch_vox ** -0.5)
        sim = (sim.view(b, self.num_classes, self.n_sub, self.nz, self.nx, self.ny)
               + self.proto_bias(se).view(b, self.num_classes, self.n_sub, 1, 1, 1))
        logits = torch.logsumexp(sim, dim=2)                                # (B, 7, nz, nx, ny)
        probs = F.softmax(logits, dim=1)

        # Bounded continuous density regression within physical rock lithology bands
        rock = self.rock_head(hv.permute(0, 2, 1, 3, 4).reshape(b * self.nz, self.ch_vox, self.nx, self.ny))
        rock_sig = torch.sigmoid(
            rock.reshape(b, self.nz, len(_ROCK_BOUNDS), self.nx, self.ny).permute(0, 2, 1, 3, 4)
        )
        rho_light, rho_sediment, rho_mafic, rho_ore = [
            lo + (hi - lo) * rock_sig[:, i:i + 1] for i, (lo, hi) in enumerate(_ROCK_BOUNDS)
        ]

        y = (
            probs[:, 1:2] * RHO_AIR
            + probs[:, 2:3] * RHO_WATER
            + probs[:, 3:4] * rho_light
            + probs[:, 4:5] * rho_sediment
            + probs[:, 5:6] * rho_mafic
            + probs[:, 6:7] * rho_ore
        )
        mask = (self.z_top >= terr.unsqueeze(2)).to(y.dtype)
        pred = y * mask
        if return_aux:
            return pred, {
                "logits": logits,
                "probs": probs,
                "rho_light": rho_light * mask,
                "rho_sediment": rho_sediment * mask,
                "rho_mafic": rho_mafic * mask,
                "rho_ore": rho_ore * mask,
                "proto_emb": p_ctx,
                "mask": mask,
            }
        return pred


class PotentialFieldTransformer3DV3(nn.Module):
    """V3 Terrain-Relative Axial+Swin3D Transformer with Subpixel Decoder & Surface-Distance Auxiliary Head."""

    def __init__(self, nx=64, ny=64, nz=32, embed_dim=128, num_heads=4, depth=6, patch=4,
                 n_sub=3, ch_mid=40, ch_vox=24, num_classes=len(CLASS_NAMES), **_unused):
        super().__init__()
        assert patch == 4 and nx % (4 * patch) == 0 and ny % (4 * patch) == 0 and nz % 4 == 0
        self.nx, self.ny, self.nz, self.p, self.n_sub = nx, ny, nz, patch, n_sub
        self.num_classes = num_classes
        self.hp, self.wp, self.dp = nx // patch, ny // patch, nz // 4
        c = embed_dim
        c0, c1 = max(24, c // 4), max(48, c // 2)
        self.ch_mid, self.ch_vox = ch_mid, ch_vox

        self.scale_mlp = nn.Sequential(nn.Linear(4, c), nn.GELU(), nn.Linear(c, c))

        # 9-channel physics encoder: linear + asinh + 4 high-pass + |grad_h| + tilt + terrain
        self.enc0 = nn.Sequential(
            nn.Conv2d(9, c0, 3, padding=1), nn.GELU(),
            nn.Conv2d(c0, c0, 3, padding=1), nn.GELU(),
        )
        self.enc1 = nn.Sequential(
            nn.Conv2d(c0, c1, 3, stride=2, padding=1), nn.GELU(),
            nn.Conv2d(c1, c1, 3, padding=1), nn.GELU(),
        )
        self.enc2 = nn.Sequential(
            nn.Conv2d(c1, c, 3, stride=2, padding=1), nn.GELU(),
            nn.Conv2d(c, c, 3, padding=1),
        )
        self.pos2d = nn.Parameter(torch.randn(1, self.hp * self.wp, c) * 0.02)
        self.n2d_1 = nn.LayerNorm(c)
        self.attn2d = Attention(c, num_heads)
        self.n2d_2 = nn.LayerNorm(c)
        self.mlp2d = nn.Sequential(nn.Linear(c, 3 * c), nn.GELU(), nn.Linear(3 * c, c))

        # Alternating 3D Swin (local joint 3D windows) + Axial Global (full-XY + full-Z column) blocks
        self.depth_lift = nn.Sequential(nn.LayerNorm(c), nn.Linear(c, self.dp * c))
        self.pos3d = nn.Parameter(torch.randn(1, self.dp, self.hp, self.wp, c) * 0.02)
        self.blocks = nn.ModuleList([
            (AxialGlobalBlock3D(c, num_heads) if i % 2 else SwinBlock3D(c, num_heads, wh=4, ww=4, shift=(2 if (i // 2) % 2 else 0)))
            for i in range(depth)
        ])

        # Bidirectional Lithology Prototype Cross-Attention (voxels <-> scene-adapted prototypes)
        self.proto_emb = nn.Parameter(torch.randn(1, num_classes * n_sub, c) * 0.05)
        self.proto_scale = nn.Linear(c, c)
        self.proto_nq = nn.LayerNorm(c)
        self.proto_nk = nn.LayerNorm(c)
        self.proto_attn = Attention(c, num_heads)
        self.proto_mlp_n = nn.LayerNorm(c)
        self.proto_mlp = nn.Sequential(nn.Linear(c, 2 * c), nn.GELU(), nn.Linear(2 * c, c))
        self.s2p_nq = nn.LayerNorm(c)
        self.s2p_nk = nn.LayerNorm(c)
        self.s2p_attn = Attention(c, num_heads)
        self.s2p_mlp_n = nn.LayerNorm(c)
        self.s2p_mlp = nn.Sequential(nn.Linear(c, 2 * c), nn.GELU(), nn.Linear(2 * c, c))
        self.norm3d = nn.LayerNorm(c)

        # Dual-stage terrain-conditioned subpixel 2.5D U-Net decoder
        self.proj_mid = nn.Conv2d(c, c, 1)
        self.up_mid = SepSubpixelUp3D(c, ch_mid)
        self.skip_mid = DepthConditionedSkip(c1, ch_mid, nz // 2)
        self.dec_mid1 = SepConv25DBlock(ch_mid)
        self.dec_mid2 = SepConv25DBlock(ch_mid)

        self.up_vox = SepSubpixelUp3D(ch_mid, ch_vox)
        self.skip_vox = DepthConditionedSkip(c0, ch_vox, nz)
        self.scale_vox = nn.Linear(c, ch_vox)
        self.dec_vox1 = SepConv25DBlock(ch_vox)
        self.dec_vox2 = SepConv25DBlock(ch_vox)

        # Sharp prototype head + bounded rock head + training-only surface-distance head
        self.proto_head = nn.Linear(c, ch_vox)
        self.proto_bias = nn.Linear(c, num_classes * n_sub)
        self.logit_log_scale = nn.Parameter(torch.zeros(1))
        self.rock_head = nn.Conv2d(ch_vox, len(_ROCK_BOUNDS), 1)
        self.edt_head = nn.Conv2d(ch_vox, 1, 1)

        self.register_buffer("z_top_mid", (torch.arange(nz // 2, dtype=torch.float32) / (nz // 2)).view(1, 1, nz // 2, 1, 1),
                             persistent=False)
        self.register_buffer("z_top", (torch.arange(nz, dtype=torch.float32) / nz).view(1, 1, nz, 1, 1),
                             persistent=False)

    @staticmethod
    def _avg(g, k: int):
        p = k // 2
        return F.avg_pool2d(F.pad(g, (p, p, p, p), mode="replicate"), k, stride=1)

    def forward(self, x, return_aux: bool = False):
        b = x.shape[0]
        gz, terr = x[:, 0:1], x[:, 1:2]
        s = gz.abs().amax(dim=(2, 3), keepdim=True).clamp_min(1e-6)
        gn = gz / s
        u8 = 8.0 * gn
        gn_log = torch.log(u8 + torch.sqrt(u8 * u8 + 1.0)) / math.asinh(8.0)

        gp = F.pad(gn, (1, 1, 1, 1), mode="replicate")
        gx = 0.5 * (gp[:, :, 2:, 1:-1] - gp[:, :, :-2, 1:-1])
        gy = 0.5 * (gp[:, :, 1:-1, 2:] - gp[:, :, 1:-1, :-2])
        grad_mag = torch.sqrt(gx * gx + gy * gy + 1e-8)
        dz_proxy = gn - self._avg(gn, 5)
        tilt_sin = dz_proxy / torch.sqrt(dz_proxy * dz_proxy + grad_mag * grad_mag + 1e-4)

        h_in = torch.cat([
            gn,
            gn_log,
            (gn - self._avg(gn, 3)) * 6.0,
            (gn - self._avg(gn, 9)) * 2.5,
            (gn - self._avg(gn, 17)) * 1.5,
            (gn - self._avg(gn, 25)) * 1.2,
            grad_mag * 6.0,
            tilt_sin,
            terr * 4.0,
        ], dim=1)

        s_flat = s.view(b, 1)
        s_slab = s_flat / SLAB
        s_vec = torch.cat([
            torch.log(s_slab + 1e-4) * 0.25,
            s_slab / (s_slab + 1.0),
            gz.amax(dim=(2, 3)).view(b, 1) / s_flat,
            gz.amin(dim=(2, 3)).view(b, 1) / s_flat,
        ], dim=1)
        se = self.scale_mlp(s_vec)

        f0 = self.enc0(h_in)
        f1 = self.enc1(f0)
        f2 = self.enc2(f1)

        t2 = f2.flatten(2).transpose(1, 2) + self.pos2d + se.unsqueeze(1)
        t2 = t2 + self.attn2d(self.n2d_1(t2), self.n2d_1(t2))
        t2 = t2 + self.mlp2d(self.n2d_2(t2))

        v = self.depth_lift(t2).view(b, self.hp, self.wp, self.dp, -1).permute(0, 3, 1, 2, 4)
        v = v + self.pos3d + se.view(b, 1, 1, 1, -1)
        for blk in self.blocks:
            v = blk(v)

        vf = v.reshape(b, self.dp * self.hp * self.wp, -1)
        p_ctx = self.proto_emb + self.proto_scale(se).unsqueeze(1)
        vf = vf + self.proto_attn(self.proto_nq(vf), self.proto_nk(p_ctx))
        vf = vf + self.proto_mlp(self.proto_mlp_n(vf))
        p_ctx = p_ctx + self.s2p_attn(self.s2p_nq(p_ctx), self.s2p_nk(vf))
        p_ctx = p_ctx + self.s2p_mlp(self.s2p_mlp_n(p_ctx))
        v = self.norm3d(vf).view(b, self.dp, self.hp, self.wp, -1)

        # Stage 1 -> (nz/2, nx/2, ny/2)
        vc = self.proj_mid(v.permute(0, 4, 1, 2, 3).reshape(b, -1, self.dp * self.hp, self.wp)).reshape(
            b, -1, self.dp, self.hp, self.wp
        )
        u = self.up_mid(vc)
        terr_mid = F.avg_pool2d(terr, 2)
        d_surf_mid = (self.z_top_mid - terr_mid.unsqueeze(2)).clamp_min(0.0)
        u = u + self.skip_mid(f1, d_surf_mid, self.z_top_mid)
        u = self.dec_mid2(self.dec_mid1(u))

        # Stage 2 -> (nz, nx, ny)
        u = self.up_vox(u)
        d_surf = (self.z_top - terr.unsqueeze(2)).clamp_min(0.0)
        hv = u + self.skip_vox(f0, d_surf, self.z_top) + self.scale_vox(se).view(b, self.ch_vox, 1, 1, 1)
        hv = self.dec_vox2(self.dec_vox1(hv))

        p_head = self.proto_head(p_ctx)
        hv_flat = hv.reshape(b, self.ch_vox, self.nz * self.nx * self.ny)
        logit_scale = self.logit_log_scale.exp().clamp(0.5, 4.0)
        sim = torch.bmm(p_head, hv_flat) * (self.ch_vox ** -0.5) * logit_scale
        sim = (sim.view(b, self.num_classes, self.n_sub, self.nz, self.nx, self.ny)
               + self.proto_bias(se).view(b, self.num_classes, self.n_sub, 1, 1, 1))
        logits = torch.logsumexp(sim, dim=2)
        probs = F.softmax(logits, dim=1)

        hv_1x1 = hv.reshape(b, self.ch_vox, self.nz * self.nx, self.ny)
        rock_sig = torch.sigmoid(self.rock_head(hv_1x1).reshape(b, len(_ROCK_BOUNDS), self.nz, self.nx, self.ny))
        rho_light, rho_sediment, rho_mafic, rho_ore = [
            lo + (hi - lo) * rock_sig[:, i:i + 1] for i, (lo, hi) in enumerate(_ROCK_BOUNDS)
        ]

        y = (
            probs[:, 1:2] * RHO_AIR
            + probs[:, 2:3] * RHO_WATER
            + probs[:, 3:4] * rho_light
            + probs[:, 4:5] * rho_sediment
            + probs[:, 5:6] * rho_mafic
            + probs[:, 6:7] * rho_ore
        )
        mask = (self.z_top >= terr.unsqueeze(2)).to(y.dtype)
        pred = y * mask
        if return_aux:
            edt = F.softplus(self.edt_head(hv_1x1)).reshape(b, 1, self.nz, self.nx, self.ny) * mask
            return pred, {
                "logits": logits,
                "probs": probs,
                "edt": edt,
                "rho_light": rho_light * mask,
                "rho_sediment": rho_sediment * mask,
                "rho_mafic": rho_mafic * mask,
                "rho_ore": rho_ore * mask,
                "proto_emb": p_ctx,
                "mask": mask,
            }
        return pred


class PotentialFieldTransformer3DV4(PotentialFieldTransformer3DV3):
    """Physics- & Structural-Geology-Informed 3D Swin Transformer (v4).

    Upgrades over v3:
      1. 16-channel physics & structural geology surface stem:
         Laplace vertical second derivatives (g_zz = -(g_xx + g_yy)), Euler deconvolution
         depth proxy, 2D structure-tensor strike & coherence, and terrain slope/curvature.
      2. Terrain-depth & physical sensitivity modulation at the coarse 3D bottleneck.
      3. Coarse Stage-1 physical gravity forward residual feedback block (adjoint backprojection).
      4. Depth-conditioned lithology log-odds prior + sharpened occupancy / conditional
         lithology mixture head to prevent background probability dilution.
    """

    def __init__(self, nx=64, ny=64, nz=32, patch=4, embed_dim=128, num_heads=4, depth=6,
                 ch_mid=40, ch_vox=24, n_sub=3, num_classes=7):
        super().__init__(
            nx=nx, ny=ny, nz=nz, patch=patch, embed_dim=embed_dim, num_heads=num_heads,
            depth=depth, ch_mid=ch_mid, ch_vox=ch_vox, n_sub=n_sub, num_classes=num_classes,
        )
        c = embed_dim
        c0 = max(24, c // 4)
        self.scale_mlp = nn.Sequential(nn.Linear(6, c), nn.GELU(), nn.Linear(c, c))
        self.enc0 = nn.Sequential(
            nn.Conv2d(16, c0, 3, padding=1), nn.GELU(),
            nn.Conv2d(c0, c0, 3, padding=1), nn.GELU(),
        )
        self.register_buffer(
            "z_top_coarse",
            (torch.arange(self.dp, dtype=torch.float32) / self.dp).view(1, 1, self.dp, 1, 1),
            persistent=False,
        )
        self.depth_cond = nn.Sequential(
            nn.Conv2d(3, c, 1),
            nn.GELU(),
            nn.Conv2d(c, c, 1),
        )

        # Coarse Stage-1 physics residual feedback
        nz_mid = nz // 2
        self.coarse_rho_head = nn.Conv2d(ch_mid, 1, 1)
        self.coarse_fwd_conv = nn.Conv2d(nz_mid, 1, 5, padding=2, bias=False)
        with torch.no_grad():
            zk = torch.linspace(0.5 / nz_mid, 1.0 - 0.5 / nz_mid, nz_mid).view(1, nz_mid, 1, 1)
            offs = torch.arange(-2, 3, dtype=torch.float32) * (2.0 / nx)
            dy, dx = torch.meshgrid(offs, offs, indexing="ij")
            r2 = dx.unsqueeze(0).unsqueeze(0) ** 2 + dy.unsqueeze(0).unsqueeze(0) ** 2 + (zk + 0.08) ** 2
            w_kern = (zk + 0.08) / (r2 ** 1.5)
            w_kern = w_kern / w_kern.sum().clamp_min(1e-6)
            self.coarse_fwd_conv.weight.copy_(w_kern)

        self.res_feedback_2d = nn.Sequential(
            nn.Conv2d(3, ch_mid, 3, padding=1), nn.GELU(),
            nn.Conv2d(ch_mid, ch_mid, 1),
        )
        self.res_depth_gate = nn.Sequential(
            nn.Conv2d(2, ch_mid, 1),
            nn.Sigmoid(),
        )
        self.res_gate = nn.Parameter(torch.tensor(0.25))

        # Depth-conditioned class prior + sharpened foreground occupancy mixture
        self.cls_depth_prior = nn.Sequential(
            nn.Conv2d(2, 16, 1),
            nn.GELU(),
            nn.Conv2d(16, num_classes, 1),
        )
        self.occ_sharpness = nn.Parameter(torch.tensor(1.35))
        self.cls_sharpness = nn.Parameter(torch.tensor(1.25))
        self.occ_bias = nn.Parameter(torch.tensor(0.15))

    def _stem_features(self, gn: torch.Tensor, terr: torch.Tensor) -> torch.Tensor:
        u8 = 8.0 * gn
        gn_log = torch.log(u8 + torch.sqrt(u8 * u8 + 1.0)) / math.asinh(8.0)
        lp3 = self._avg(gn, 3)
        lp9 = self._avg(gn, 9)
        lp17 = self._avg(gn, 17)
        lp25 = self._avg(gn, 25)
        r3, r9, r17, r25 = (gn - lp3) * 6.0, (gn - lp9) * 2.5, (gn - lp17) * 1.5, (gn - lp25) * 1.2

        gp = F.pad(gn, (1, 1, 1, 1), mode="replicate")
        gx = 0.5 * (gp[:, :, 2:, 1:-1] - gp[:, :, :-2, 1:-1])
        gy = 0.5 * (gp[:, :, 1:-1, 2:] - gp[:, :, 1:-1, :-2])
        gxx = gp[:, :, 2:, 1:-1] - 2.0 * gn + gp[:, :, :-2, 1:-1]
        gyy = gp[:, :, 1:-1, 2:] - 2.0 * gn + gp[:, :, 1:-1, :-2]
        gzz_fine = -(gxx + gyy) * 4.0

        lp3_p = F.pad(lp3, (1, 1, 1, 1), mode="replicate")
        gzz_mid = -(
            (lp3_p[:, :, 2:, 1:-1] - 2.0 * lp3 + lp3_p[:, :, :-2, 1:-1])
            + (lp3_p[:, :, 1:-1, 2:] - 2.0 * lp3 + lp3_p[:, :, 1:-1, :-2])
        ) * 6.0

        grad_mag = torch.sqrt(gx * gx + gy * gy + 1e-8)
        dz_proxy = gn - self._avg(gn, 5)
        tilt_sin = dz_proxy / torch.sqrt(dz_proxy * dz_proxy + grad_mag * grad_mag + 1e-4)
        tot_grad = torch.sqrt(grad_mag * grad_mag + dz_proxy * dz_proxy + 1e-5)
        euler_depth = torch.tanh((gn.abs() + (gn - lp17).abs()) / (tot_grad * 4.0 + 0.05))

        jxx = self._avg(gx * gx, 3)
        jyy = self._avg(gy * gy, 3)
        jxy = self._avg(gx * gy, 3)
        anis = torch.sqrt((jxx - jyy) ** 2 + 4.0 * jxy * jxy + 1e-8)
        coherence = anis / (jxx + jyy + 1e-5)
        strike_sin2 = (2.0 * jxy) / (anis + 1e-5)

        tp = F.pad(terr, (1, 1, 1, 1), mode="replicate")
        tx = 0.5 * (tp[:, :, 2:, 1:-1] - tp[:, :, :-2, 1:-1])
        ty = 0.5 * (tp[:, :, 1:-1, 2:] - tp[:, :, 1:-1, :-2])
        terr_slope = torch.sqrt(tx * tx + ty * ty + 1e-8) * 16.0
        terr_curv = (
            (tp[:, :, 2:, 1:-1] - 2.0 * terr + tp[:, :, :-2, 1:-1])
            + (tp[:, :, 1:-1, 2:] - 2.0 * terr + tp[:, :, 1:-1, :-2])
        ) * 16.0

        return torch.cat([
            gn, gn_log, r3, r9, r17, r25,
            gzz_fine, gzz_mid, grad_mag * 6.0, tilt_sin, euler_depth,
            coherence, strike_sin2,
            terr * 4.0, terr_slope, terr_curv,
        ], dim=1)

    def forward(self, x: torch.Tensor, return_aux: bool = False):
        b = x.shape[0]
        gz, terr = x[:, 0:1], x[:, 1:2]

        s = gz.abs().amax(dim=(2, 3), keepdim=True).clamp_min(1e-6)
        gn = gz / s
        h_in = self._stem_features(gn, terr)

        s_flat = s.view(b, 1)
        s_slab = s_flat / SLAB
        s_vec = torch.cat([
            torch.log(s_slab + 1e-4) * 0.25,
            s_slab / (s_slab + 1.0),
            gz.amax(dim=(2, 3)).view(b, 1) / s_flat,
            gz.amin(dim=(2, 3)).view(b, 1) / s_flat,
            terr.mean(dim=(2, 3)).view(b, 1) * 4.0,
            terr.std(dim=(2, 3)).view(b, 1) * 8.0,
        ], dim=1)
        se = self.scale_mlp(s_vec)

        f0 = self.enc0(h_in)
        f1 = self.enc1(f0)
        f2 = self.enc2(f1)

        t2 = f2.flatten(2).transpose(1, 2) + self.pos2d + se.unsqueeze(1)
        t2 = t2 + self.attn2d(self.n2d_1(t2), self.n2d_1(t2))
        t2 = t2 + self.mlp2d(self.n2d_2(t2))

        v = self.depth_lift(t2).view(b, self.hp, self.wp, self.dp, -1).permute(0, 3, 1, 2, 4)
        terr_coarse = F.avg_pool2d(terr, 4)
        d_surf_coarse = (self.z_top_coarse - terr_coarse.unsqueeze(2)).clamp_min(0.0)
        z_c = self.z_top_coarse.expand(b, 1, self.dp, self.hp, self.wp)
        sens_c = 0.04 / ((d_surf_coarse + 0.18) ** 2)
        dc_in = torch.cat([d_surf_coarse, z_c, sens_c], dim=1).reshape(b, 3, self.dp * self.hp, self.wp)
        dc_mod = self.depth_cond(dc_in).reshape(b, -1, self.dp, self.hp, self.wp).permute(0, 2, 3, 4, 1)
        v = v + self.pos3d + dc_mod + se.view(b, 1, 1, 1, -1)

        for blk in self.blocks:
            v = blk(v)

        vf = v.reshape(b, self.dp * self.hp * self.wp, -1)
        p_ctx = self.proto_emb.expand(b, -1, -1) + self.proto_scale(se).unsqueeze(1)
        vf = vf + self.proto_attn(self.proto_nq(vf), self.proto_nk(p_ctx))
        vf = vf + self.proto_mlp(self.proto_mlp_n(vf))
        p_ctx = p_ctx + self.s2p_attn(self.s2p_nq(p_ctx), self.s2p_nk(vf))
        p_ctx = p_ctx + self.s2p_mlp(self.s2p_mlp_n(p_ctx))
        v = self.norm3d(vf).view(b, self.dp, self.hp, self.wp, -1)

        # Stage 1 -> (nz/2, nx/2, ny/2) + Coarse Physics Residual Feedback
        vc = self.proj_mid(v.permute(0, 4, 1, 2, 3).reshape(b, -1, self.dp * self.hp, self.wp)).reshape(
            b, -1, self.dp, self.hp, self.wp
        )
        u = self.up_mid(vc)
        terr_mid = F.avg_pool2d(terr, 2)
        d_surf_mid = (self.z_top_mid - terr_mid.unsqueeze(2)).clamp_min(0.0)
        u = u + self.skip_mid(f1, d_surf_mid, self.z_top_mid)
        u = self.dec_mid1(u)

        nz_mid, nx_mid, ny_mid = self.nz // 2, self.nx // 2, self.ny // 2
        mask_mid = (self.z_top_mid >= terr_mid.unsqueeze(2)).to(u.dtype)
        coarse_raw = self.coarse_rho_head(u.reshape(b, self.ch_mid, nz_mid * nx_mid, ny_mid)).reshape(
            b, 1, nz_mid, nx_mid, ny_mid
        )
        coarse_pred = (RHO_AIR + (RHO_ORE_MAX - RHO_AIR) * torch.sigmoid(coarse_raw)) * mask_mid
        g_hat_mid = self.coarse_fwd_conv(coarse_pred.squeeze(1) / 2500.0)
        gn_mid = F.avg_pool2d(gn, 2)
        g_res_mid = gn_mid - g_hat_mid
        res_2d = self.res_feedback_2d(torch.cat([gn_mid, g_hat_mid, g_res_mid], dim=1)).unsqueeze(2)
        z_m = self.z_top_mid.expand(b, 1, nz_mid, nx_mid, ny_mid)
        d_gate = self.res_depth_gate(
            torch.cat([d_surf_mid, z_m], dim=1).reshape(b, 2, nz_mid * nx_mid, ny_mid)
        ).reshape(b, self.ch_mid, nz_mid, nx_mid, ny_mid)
        u = u + self.res_gate.tanh() * (res_2d * d_gate * mask_mid)
        u = self.dec_mid2(u)

        # Stage 2 -> (nz, nx, ny)
        u = self.up_vox(u)
        d_surf = (self.z_top - terr.unsqueeze(2)).clamp_min(0.0)
        hv = u + self.skip_vox(f0, d_surf, self.z_top) + self.scale_vox(se).view(b, self.ch_vox, 1, 1, 1)
        hv = self.dec_vox2(self.dec_vox1(hv))

        p_head = self.proto_head(p_ctx)
        hv_flat = hv.reshape(b, self.ch_vox, self.nz * self.nx * self.ny)
        logit_scale = self.logit_log_scale.exp().clamp(0.5, 4.0)
        sim = torch.bmm(p_head, hv_flat) * (self.ch_vox ** -0.5) * logit_scale
        sim = (sim.view(b, self.num_classes, self.n_sub, self.nz, self.nx, self.ny)
               + self.proto_bias(se).view(b, self.num_classes, self.n_sub, 1, 1, 1))
        z_full = self.z_top.expand(b, 1, self.nz, self.nx, self.ny)
        depth_prior = self.cls_depth_prior(
            torch.cat([d_surf, z_full], dim=1).reshape(b, 2, self.nz * self.nx, self.ny)
        ).reshape(b, self.num_classes, self.nz, self.nx, self.ny)
        logits = torch.logsumexp(sim, dim=2) + depth_prior
        probs = F.softmax(logits, dim=1)

        # Sharpened foreground occupancy + conditional lithology mixture
        fg_lse = torch.logsumexp(logits[:, 1:], dim=1, keepdim=True)
        occ_sharp = self.occ_sharpness.clamp(0.5, 3.0)
        cls_sharp = self.cls_sharpness.clamp(0.5, 3.0)
        occ = torch.sigmoid(occ_sharp * (fg_lse - logits[:, 0:1]) + self.occ_bias)
        active_probs = F.softmax(logits[:, 1:] * cls_sharp, dim=1)
        w_active = 0.45 * probs[:, 1:] + 0.55 * (occ * active_probs)

        hv_1x1 = hv.reshape(b, self.ch_vox, self.nz * self.nx, self.ny)
        rock_sig = torch.sigmoid(self.rock_head(hv_1x1).reshape(b, len(_ROCK_BOUNDS), self.nz, self.nx, self.ny))
        rho_light, rho_sediment, rho_mafic, rho_ore = [
            lo + (hi - lo) * rock_sig[:, i:i + 1] for i, (lo, hi) in enumerate(_ROCK_BOUNDS)
        ]

        y = (
            w_active[:, 0:1] * RHO_AIR
            + w_active[:, 1:2] * RHO_WATER
            + w_active[:, 2:3] * rho_light
            + w_active[:, 3:4] * rho_sediment
            + w_active[:, 4:5] * rho_mafic
            + w_active[:, 5:6] * rho_ore
        )
        mask = (self.z_top >= terr.unsqueeze(2)).to(y.dtype)
        pred = y * mask
        if return_aux:
            edt = F.softplus(self.edt_head(hv_1x1)).reshape(b, 1, self.nz, self.nx, self.ny) * mask
            return pred, {
                "logits": logits,
                "probs": probs,
                "edt": edt,
                "rho_light": rho_light * mask,
                "rho_sediment": rho_sediment * mask,
                "rho_mafic": rho_mafic * mask,
                "rho_ore": rho_ore * mask,
                "proto_emb": p_ctx,
                "mask": mask,
                "coarse_pred": coarse_pred,
                "occ": occ * mask,
            }
        return pred


class PotentialFieldTransformer3DV5(PotentialFieldTransformer3DV4):
    """V5 Physics-Calibrated, Uncertainty-Aware & Multi-Survey Terrain-Relative 3D Transformer.

    Key upgrades over v4:
      1. 5-channel survey-aware input: ``[gz_scaled, terr_norm, valid_mask, log_aspect, clearance]``
         with backward-compatible handling of 2..5 channel inputs at inference.
      2. Complete 2D structure-tensor stem (``strike_cos2`` + ``strike_sin2`` + ``valid_mask``),
         expanded 8D scale/survey vector, and clearance/aspect-conditioned depth modulation.
      3. Learnable Green's function vertical distance decay bias (``GreensVerticalBias``) in
         Stage-0 3D attention blocks, plus optional ``Dropout3d``.
      4. Physically calibrated frozen ``17x17`` Nagy-prism coarse forward residual loop at Stage 1
         (with zero-density bias init on ``coarse_rho_head``).
      5. Sharper foreground occupancy gating (``0.25/0.75``) and bounded mid-stage aleatoric
         ``log_var`` head (``[-8.0, 4.0]``, trilinearly upsampled & masked by terrain).
    """

    def __init__(self, nx=64, ny=64, nz=32, patch=4, embed_dim=128, num_heads=4, depth=6,
                 ch_mid=40, ch_vox=24, n_sub=3, num_classes=7, dropout=0.05, dec_depth=2, **_unused):
        super().__init__(
            nx=nx, ny=ny, nz=nz, patch=patch, embed_dim=embed_dim, num_heads=num_heads,
            depth=depth, ch_mid=ch_mid, ch_vox=ch_vox, n_sub=n_sub, num_classes=num_classes,
        )
        self.dropout_rate = float(dropout)
        self.dec_depth = max(2, int(dec_depth))
        c = embed_dim
        c0 = max(24, c // 4)
        self.scale_mlp = nn.Sequential(nn.Linear(8, c), nn.GELU(), nn.Linear(c, c))
        self.enc0 = nn.Sequential(
            nn.Conv2d(18, c0, 3, padding=1), nn.GELU(),
            nn.Conv2d(c0, c0, 3, padding=1), nn.GELU(),
        )
        self.depth_cond = nn.Sequential(
            nn.Conv2d(5, c, 1),
            nn.GELU(),
            nn.Conv2d(c, c, 1),
        )
        self.blocks = nn.ModuleList([
            (
                AxialGlobalBlock3D(c, num_heads, use_greens_bias=True, dp=self.dp, dropout=dropout)
                if i % 2
                else SwinBlock3D(
                    c, num_heads, wh=4, ww=4, shift=(2 if (i // 2) % 2 else 0),
                    use_greens_bias=True, dp=self.dp, dropout=dropout,
                )
            )
            for i in range(depth)
        ])

        nz_mid = nz // 2
        with torch.no_grad():
            self.coarse_rho_head.weight.mul_(0.05)
            self.coarse_rho_head.bias.fill_(math.log(-RHO_AIR / RHO_ORE_MAX))
        self.coarse_fwd_conv = nn.Conv2d(nz_mid, 1, 17, padding=8, bias=False)
        with torch.no_grad():
            cell_mid = 2.0 * REF_CELL
            offs = np.arange(-8, 9, dtype=np.float64)
            u1 = (offs * cell_mid - 0.5 * cell_mid)[None, :, None]
            u2 = u1 + cell_mid
            v1 = (offs * cell_mid - 0.5 * cell_mid)[None, None, :]
            v2 = v1 + cell_mid
            iz = np.arange(nz_mid, dtype=np.float64)
            w1 = (iz * cell_mid + 0.35)[:, None, None]
            w2 = w1 + cell_mid
            k_np = prism_gz(u1, u2, v1, v2, w1, w2).astype(np.float32)
            self.coarse_fwd_conv.weight.copy_(torch.from_numpy(k_np).unsqueeze(0))
        self.coarse_fwd_conv.weight.requires_grad_(False)
        self.coarse_fwd_log_scale = nn.Parameter(torch.full((1,), math.log(1.35)))

        self.res_feedback_2d = nn.Sequential(
            nn.Conv2d(4, ch_mid, 3, padding=1), nn.GELU(),
            nn.Conv2d(ch_mid, ch_mid, 1),
        )
        self.res_depth_gate = nn.Sequential(
            nn.Conv2d(3, ch_mid, 1),
            nn.Sigmoid(),
        )

        self.vox_drop = nn.Dropout3d(p=dropout) if dropout > 0 else nn.Identity()
        if self.dec_depth > 2:
            self.extra_dec_mid = nn.ModuleList([SepConv25DBlock(ch_mid) for _ in range(self.dec_depth - 2)])
            self.extra_dec_vox = nn.ModuleList([SepConv25DBlock(ch_vox) for _ in range(self.dec_depth - 2)])
        lv_ch = max(8, ch_mid // 2)
        self.logvar_proj = nn.Sequential(nn.Conv2d(ch_mid, lv_ch, 1), nn.GELU())
        self.logvar_head = nn.Conv2d(lv_ch, 1, 1)
        with torch.no_grad():
            self.logvar_head.bias.fill_(-2.5)

    def _stem_features_v5(self, gn: torch.Tensor, terr: torch.Tensor,
                          valid_mask: torch.Tensor) -> torch.Tensor:
        u8 = 8.0 * gn
        gn_log = torch.log(u8 + torch.sqrt(u8 * u8 + 1.0)) / math.asinh(8.0)
        lp3 = self._avg(gn, 3)
        lp9 = self._avg(gn, 9)
        lp17 = self._avg(gn, 17)
        lp25 = self._avg(gn, 25)
        r3, r9, r17, r25 = (gn - lp3) * 6.0, (gn - lp9) * 2.5, (gn - lp17) * 1.5, (gn - lp25) * 1.2

        gp = F.pad(gn, (1, 1, 1, 1), mode="replicate")
        gx = 0.5 * (gp[:, :, 2:, 1:-1] - gp[:, :, :-2, 1:-1])
        gy = 0.5 * (gp[:, :, 1:-1, 2:] - gp[:, :, 1:-1, :-2])
        gxx = gp[:, :, 2:, 1:-1] - 2.0 * gn + gp[:, :, :-2, 1:-1]
        gyy = gp[:, :, 1:-1, 2:] - 2.0 * gn + gp[:, :, 1:-1, :-2]
        gzz_fine = -(gxx + gyy) * 4.0

        lp3_p = F.pad(lp3, (1, 1, 1, 1), mode="replicate")
        gzz_mid = -(
            (lp3_p[:, :, 2:, 1:-1] - 2.0 * lp3 + lp3_p[:, :, :-2, 1:-1])
            + (lp3_p[:, :, 1:-1, 2:] - 2.0 * lp3 + lp3_p[:, :, 1:-1, :-2])
        ) * 6.0

        grad_mag = torch.sqrt(gx * gx + gy * gy + 1e-8)
        dz_proxy = gn - self._avg(gn, 5)
        tilt_sin = dz_proxy / torch.sqrt(dz_proxy * dz_proxy + grad_mag * grad_mag + 1e-4)
        tot_grad = torch.sqrt(grad_mag * grad_mag + dz_proxy * dz_proxy + 1e-5)
        euler_depth = torch.tanh((gn.abs() + (gn - lp17).abs()) / (tot_grad * 4.0 + 0.05))

        jxx = self._avg(gx * gx, 3)
        jyy = self._avg(gy * gy, 3)
        jxy = self._avg(gx * gy, 3)
        anis = torch.sqrt((jxx - jyy) ** 2 + 4.0 * jxy * jxy + 1e-8)
        coherence = anis / (jxx + jyy + 1e-5)
        strike_cos2 = (jxx - jyy) / (anis + 1e-5)
        strike_sin2 = (2.0 * jxy) / (anis + 1e-5)

        tp = F.pad(terr, (1, 1, 1, 1), mode="replicate")
        tx = 0.5 * (tp[:, :, 2:, 1:-1] - tp[:, :, :-2, 1:-1])
        ty = 0.5 * (tp[:, :, 1:-1, 2:] - tp[:, :, 1:-1, :-2])
        terr_slope = torch.sqrt(tx * tx + ty * ty + 1e-8) * 16.0
        terr_curv = (
            (tp[:, :, 2:, 1:-1] - 2.0 * terr + tp[:, :, :-2, 1:-1])
            + (tp[:, :, 1:-1, 2:] - 2.0 * terr + tp[:, :, 1:-1, :-2])
        ) * 16.0

        return torch.cat([
            gn, gn_log, r3, r9, r17, r25,
            gzz_fine, gzz_mid, grad_mag * 6.0, tilt_sin, euler_depth,
            coherence, strike_cos2, strike_sin2, valid_mask * 2.0 - 1.0,
            terr * 4.0, terr_slope, terr_curv,
        ], dim=1)

    def _unpack_input(self, x: torch.Tensor):
        c_in = x.shape[1]
        gz = x[:, 0:1]
        terr = x[:, 1:2]
        valid_mask = x[:, 2:3].clamp(0.0, 1.0) if c_in >= 3 else torch.ones_like(gz)
        log_aspect = x[:, 3:4] if c_in >= 4 else torch.zeros_like(gz)
        clearance = x[:, 4:5].clamp_min(0.0) if c_in >= 5 else torch.full_like(gz, 0.35 / REF_CELL)
        return gz, terr, valid_mask, log_aspect, clearance

    def enable_mc_dropout(self, enabled: bool = True):
        """Enable or disable Dropout / Dropout3d layers for MC Dropout uncertainty."""
        for m in self.modules():
            if isinstance(m, (nn.Dropout, nn.Dropout3d)):
                m.train(enabled)

    def forward_uncertainty(self, x: torch.Tensor):
        """Return ``(density, log_var)`` tensors (used by ONNX export & uncertainty inference)."""
        pred, aux = self.forward(x, return_aux=True)
        return pred, aux["log_var"]

    def forward(self, x: torch.Tensor, return_aux: bool = False):
        b = x.shape[0]
        gz, terr, valid_mask, log_aspect, clearance = self._unpack_input(x)

        s = gz.abs().amax(dim=(2, 3), keepdim=True).clamp_min(1e-6)
        gn = gz / s
        h_in = self._stem_features_v5(gn, terr, valid_mask)

        s_flat = s.view(b, 1)
        s_slab = s_flat / SLAB
        log_asp_mean = log_aspect.mean(dim=(2, 3)).view(b, 1)
        clear_mean = clearance.mean(dim=(2, 3)).view(b, 1)
        s_vec = torch.cat([
            torch.log(s_slab + 1e-4) * 0.25,
            s_slab / (s_slab + 1.0),
            gz.amax(dim=(2, 3)).view(b, 1) / s_flat,
            gz.amin(dim=(2, 3)).view(b, 1) / s_flat,
            terr.mean(dim=(2, 3)).view(b, 1) * 4.0,
            terr.std(dim=(2, 3)).view(b, 1) * 8.0,
            log_asp_mean,
            torch.log1p(clear_mean),
        ], dim=1)
        se = self.scale_mlp(s_vec)

        f0 = self.enc0(h_in)
        f1 = self.enc1(f0)
        f2 = self.enc2(f1)

        t2 = f2.flatten(2).transpose(1, 2) + self.pos2d + se.unsqueeze(1)
        t2 = t2 + self.attn2d(self.n2d_1(t2), self.n2d_1(t2))
        t2 = t2 + self.mlp2d(self.n2d_2(t2))

        v = self.depth_lift(t2).view(b, self.hp, self.wp, self.dp, -1).permute(0, 3, 1, 2, 4)
        terr_coarse = F.avg_pool2d(terr, 4)
        clear_coarse = F.avg_pool2d(clearance, 4).unsqueeze(2).expand(b, 1, self.dp, self.hp, self.wp)
        asp_coarse = F.avg_pool2d(log_aspect, 4).unsqueeze(2).expand(b, 1, self.dp, self.hp, self.wp)
        d_surf_coarse = (self.z_top_coarse - terr_coarse.unsqueeze(2)).clamp_min(0.0)
        z_c = self.z_top_coarse.expand(b, 1, self.dp, self.hp, self.wp)
        sens_c = 0.04 / ((d_surf_coarse + clear_coarse * 0.08 + 0.12) ** 2)
        dc_in = torch.cat([d_surf_coarse, z_c, sens_c, clear_coarse, asp_coarse], dim=1).reshape(
            b, 5, self.dp * self.hp, self.wp
        )
        dc_mod = self.depth_cond(dc_in).reshape(b, -1, self.dp, self.hp, self.wp).permute(0, 2, 3, 4, 1)
        v = v + self.pos3d + dc_mod + se.view(b, 1, 1, 1, -1)

        for blk in self.blocks:
            v = blk(v)

        vf = v.reshape(b, self.dp * self.hp * self.wp, -1)
        p_ctx = self.proto_emb.expand(b, -1, -1) + self.proto_scale(se).unsqueeze(1)
        vf = vf + self.proto_attn(self.proto_nq(vf), self.proto_nk(p_ctx))
        vf = vf + self.proto_mlp(self.proto_mlp_n(vf))
        p_ctx = p_ctx + self.s2p_attn(self.s2p_nq(p_ctx), self.s2p_nk(vf))
        p_ctx = p_ctx + self.s2p_mlp(self.s2p_mlp_n(p_ctx))
        v = self.norm3d(vf).view(b, self.dp, self.hp, self.wp, -1)

        # Stage 1 -> (nz/2, nx/2, ny/2) + Calibrated Coarse Physics Residual + Stage-1 Swin3D
        vc = self.proj_mid(v.permute(0, 4, 1, 2, 3).reshape(b, -1, self.dp * self.hp, self.wp)).reshape(
            b, -1, self.dp, self.hp, self.wp
        )
        u = self.up_mid(vc)
        terr_mid = F.avg_pool2d(terr, 2)
        d_surf_mid = (self.z_top_mid - terr_mid.unsqueeze(2)).clamp_min(0.0)
        u = u + self.skip_mid(f1, d_surf_mid, self.z_top_mid)
        u = self.dec_mid1(u)

        nz_mid, nx_mid, ny_mid = self.nz // 2, self.nx // 2, self.ny // 2
        mask_mid = (self.z_top_mid >= terr_mid.unsqueeze(2)).float()
        coarse_raw = self.coarse_rho_head(u.reshape(b, self.ch_mid, nz_mid * nx_mid, ny_mid)).reshape(
            b, 1, nz_mid, nx_mid, ny_mid
        ).float()
        coarse_pred = (RHO_AIR + (RHO_ORE_MAX - RHO_AIR) * torch.sigmoid(coarse_raw)) * mask_mid

        aspect_b = (0.25 * log_aspect.float().mean(dim=(2, 3), keepdim=True)).exp().clamp(0.5, 2.0)
        clear_b = clearance.float().mean(dim=(2, 3), keepdim=True).clamp(0.0, 4.0)
        atten = 1.0 / (1.0 + 0.12 * clear_b)
        g_coarse_mgal = self.coarse_fwd_conv(coarse_pred.squeeze(1))
        g_hat_mid = (g_coarse_mgal * aspect_b * atten / s.float()) * self.coarse_fwd_log_scale.float().exp().clamp(0.25, 4.0)
        gn_mid = F.avg_pool2d(gn.float(), 2)
        valid_mid = F.avg_pool2d(valid_mask.float(), 2)
        g_res_mid = gn_mid - g_hat_mid
        res_in = torch.cat([gn_mid, g_hat_mid, g_res_mid, valid_mid], dim=1).to(u.dtype)
        res_2d = self.res_feedback_2d(res_in).unsqueeze(2)
        z_m = self.z_top_mid.expand(b, 1, nz_mid, nx_mid, ny_mid)
        clear_m = F.avg_pool2d(clearance, 2).unsqueeze(2).expand(b, 1, nz_mid, nx_mid, ny_mid)
        d_gate = self.res_depth_gate(
            torch.cat([d_surf_mid, z_m, clear_m], dim=1).reshape(b, 3, nz_mid * nx_mid, ny_mid)
        ).reshape(b, self.ch_mid, nz_mid, nx_mid, ny_mid)
        u = u + self.res_gate.tanh() * (res_2d * d_gate * mask_mid.to(u.dtype))
        u_mid = self.dec_mid2(u)
        if getattr(self, "dec_depth", 2) > 2:
            for blk in self.extra_dec_mid:
                u_mid = blk(u_mid)

        # Stage 2 -> (nz, nx, ny)
        u = self.up_vox(u_mid)
        d_surf = (self.z_top - terr.unsqueeze(2)).clamp_min(0.0)
        hv = u + self.skip_vox(f0, d_surf, self.z_top) + self.scale_vox(se).view(b, self.ch_vox, 1, 1, 1)
        hv = self.dec_vox2(self.dec_vox1(hv))
        if getattr(self, "dec_depth", 2) > 2:
            for blk in self.extra_dec_vox:
                hv = blk(hv)
        hv = self.vox_drop(hv)

        p_head = self.proto_head(p_ctx)
        hv_flat = hv.reshape(b, self.ch_vox, self.nz * self.nx * self.ny)
        logit_scale = self.logit_log_scale.exp().clamp(0.5, 4.0)
        sim = torch.bmm(p_head, hv_flat) * (self.ch_vox ** -0.5) * logit_scale
        sim = (sim.view(b, self.num_classes, self.n_sub, self.nz, self.nx, self.ny)
               + self.proto_bias(se).view(b, self.num_classes, self.n_sub, 1, 1, 1))
        z_full = self.z_top.expand(b, 1, self.nz, self.nx, self.ny)
        depth_prior = self.cls_depth_prior(
            torch.cat([d_surf, z_full], dim=1).reshape(b, 2, self.nz * self.nx, self.ny)
        ).reshape(b, self.num_classes, self.nz, self.nx, self.ny)
        logits = torch.logsumexp(sim, dim=2) + depth_prior
        probs = F.softmax(logits, dim=1)

        fg_lse = torch.logsumexp(logits[:, 1:], dim=1, keepdim=True)
        occ_sharp = self.occ_sharpness.clamp(0.5, 3.0)
        cls_sharp = self.cls_sharpness.clamp(0.5, 3.0)
        occ = torch.sigmoid(occ_sharp * (fg_lse - logits[:, 0:1]) + self.occ_bias)
        active_probs = F.softmax(logits[:, 1:] * cls_sharp, dim=1)
        w_active = 0.25 * probs[:, 1:] + 0.75 * (occ * active_probs)

        hv_1x1 = hv.reshape(b, self.ch_vox, self.nz * self.nx, self.ny)
        rock_sig = torch.sigmoid(self.rock_head(hv_1x1).reshape(b, len(_ROCK_BOUNDS), self.nz, self.nx, self.ny))
        rho_light, rho_sediment, rho_mafic, rho_ore = [
            lo + (hi - lo) * rock_sig[:, i:i + 1] for i, (lo, hi) in enumerate(_ROCK_BOUNDS)
        ]

        y = (
            w_active[:, 0:1] * RHO_AIR
            + w_active[:, 1:2] * RHO_WATER
            + w_active[:, 2:3] * rho_light
            + w_active[:, 3:4] * rho_sediment
            + w_active[:, 4:5] * rho_mafic
            + w_active[:, 5:6] * rho_ore
        )
        mask = (self.z_top >= terr.unsqueeze(2)).to(y.dtype)
        pred = y * mask
        if return_aux:
            edt = F.softplus(self.edt_head(hv_1x1)).reshape(b, 1, self.nz, self.nx, self.ny) * mask
            lv_in = u_mid.reshape(b, self.ch_mid, nz_mid * nx_mid, ny_mid)
            lv_mid = self.logvar_head(self.logvar_proj(lv_in)).reshape(b, 1, nz_mid, nx_mid, ny_mid)
            log_var = F.interpolate(
                lv_mid.float(), size=(self.nz, self.nx, self.ny), mode="trilinear", align_corners=False,
            ).clamp(-8.0, 4.0) * mask.float()
            return pred, {
                "logits": logits,
                "probs": probs,
                "edt": edt,
                "log_var": log_var,
                "rho_light": rho_light * mask,
                "rho_sediment": rho_sediment * mask,
                "rho_mafic": rho_mafic * mask,
                "rho_ore": rho_ore * mask,
                "proto_emb": p_ctx,
                "mask": mask,
                "coarse_pred": coarse_pred,
                "occ": occ * mask,
                "valid_mask": valid_mask,
            }
        return pred


V5_SIZE_PRESETS = {
    "slim": {
        "variant": "v5",
        "model_size": "slim",
        "embed_dim": 96,
        "num_heads": 4,
        "depth": 4,
        "ch_mid": 28,
        "ch_vox": 16,
        "n_sub": 2,
        "dec_depth": 2,
        "num_classes": 7,
        "dropout": 0.05,
    },
    "base": {
        "variant": "v5",
        "model_size": "base",
        "embed_dim": 128,
        "num_heads": 4,
        "depth": 6,
        "ch_mid": 40,
        "ch_vox": 24,
        "n_sub": 3,
        "dec_depth": 2,
        "num_classes": 7,
        "dropout": 0.05,
    },
    "heavy": {
        "variant": "v5",
        "model_size": "heavy",
        "embed_dim": 192,
        "num_heads": 6,
        "depth": 8,
        "ch_mid": 64,
        "ch_vox": 36,
        "n_sub": 4,
        "dec_depth": 3,
        "num_classes": 7,
        "dropout": 0.05,
    },
}


V6_SIZE_PRESETS = {
    "slim": {
        "variant": "v6",
        "model_size": "slim",
        "embed_dim": 96,
        "num_heads": 4,
        "depth": 4,
        "ch_mid": 28,
        "ch_vox": 16,
        "n_sub": 2,
        "dec_depth": 2,
        "num_classes": 7,
        "dropout": 0.05,
    },
    "base": {
        "variant": "v6",
        "model_size": "base",
        "embed_dim": 128,
        "num_heads": 4,
        "depth": 6,
        "ch_mid": 40,
        "ch_vox": 24,
        "n_sub": 3,
        "dec_depth": 2,
        "num_classes": 7,
        "dropout": 0.05,
    },
    "heavy": {
        "variant": "v6",
        "model_size": "heavy",
        "embed_dim": 192,
        "num_heads": 6,
        "depth": 8,
        "ch_mid": 64,
        "ch_vox": 36,
        "n_sub": 4,
        "dec_depth": 3,
        "num_classes": 7,
        "dropout": 0.05,
    },
}


V7_SIZE_PRESETS = {
    "slim": {
        "variant": "v7",
        "model_size": "slim",
        "in_channels": 6,
        "embed_dim": 96,
        "num_heads": 4,
        "depth": 4,
        "ch_mid": 28,
        "ch_vox": 24,
        "n_sub": 2,
        "dec_depth": 2,
        "num_classes": 7,
        "dropout": 0.05,
    },
    "base": {
        "variant": "v7",
        "model_size": "base",
        "in_channels": 6,
        "embed_dim": 128,
        "num_heads": 4,
        "depth": 6,
        "ch_mid": 40,
        "ch_vox": 32,
        "n_sub": 3,
        "dec_depth": 2,
        "num_classes": 7,
        "dropout": 0.05,
    },
    "heavy": {
        "variant": "v7",
        "model_size": "heavy",
        "in_channels": 6,
        "embed_dim": 192,
        "num_heads": 6,
        "depth": 8,
        "ch_mid": 64,
        "ch_vox": 40,
        "n_sub": 4,
        "dec_depth": 3,
        "num_classes": 7,
        "dropout": 0.05,
    },
}


def _rep_pad1(t: torch.Tensor) -> torch.Tensor:
    """CoreML-safe 1-pixel replicate padding for 4D ``(B, C, H, W)`` via Slice+Concat (no edge Pad)."""
    t_h = torch.cat([t[:, :, :1, :], t, t[:, :, -1:, :]], dim=2)
    return torch.cat([t_h[:, :, :, :1], t_h, t_h[:, :, :, -1:]], dim=3)


def _abs_coreml(t: torch.Tensor) -> torch.Tensor:
    """Exact ``|t|`` using ReLU+Sub (avoids ONNX ``Abs`` and ``Neg`` ops for CoreML)."""
    return F.relu(t) + F.relu(0.0 - t)


def _trilinear_coreml(x: torch.Tensor, size: tuple[int, int, int]) -> torch.Tensor:
    """CoreML-safe separable 3D trilinear resize via 4D bilinear ``F.interpolate`` (avoids 5D Resize)."""
    d_out, h_out, w_out = int(size[0]), int(size[1]), int(size[2])
    b, c, d, h, w = x.shape
    cur = x
    if d != d_out:
        cur = F.interpolate(
            cur.reshape(b, c, d, h * w), size=(d_out, h * w), mode="bilinear", align_corners=False
        ).reshape(b, c, d_out, h, w)
    if h != h_out or w != w_out:
        cur = F.interpolate(
            cur.reshape(b, c * d_out, h, w), size=(h_out, w_out), mode="bilinear", align_corners=False
        ).reshape(b, c, d_out, h_out, w_out)
    return cur


def _build_knot_weights_v6(w: torch.Tensor, knots: torch.Tensor) -> torch.Tensor:
    """Piecewise-linear hat basis weights along last dim -> ``(..., M)``."""
    M = knots.numel()
    k0 = knots[:-1]
    dk = (knots[1:] - knots[:-1]).clamp_min(1e-6)
    u = ((w.unsqueeze(-1) - k0) / dk).clamp(0.0, 1.0)
    first = 1.0 - u[..., :1]
    if M > 2:
        mid = u[..., :-1] - u[..., 1:]
        last = u[..., -1:]
        return torch.cat([first, mid, last], dim=-1)
    return torch.cat([first, u], dim=-1)


def _prism_gz_exact_t(u1: torch.Tensor, u2: torch.Tensor, v1: torch.Tensor, v2: torch.Tensor,
                      w1: torch.Tensor, w2: torch.Tensor) -> torch.Tensor:
    """Evaluate exact Nagy prism gravity in float32 via ``forward.prism_gz``."""
    out = prism_gz(
        u1.cpu().numpy(), u2.cpu().numpy(),
        v1.cpu().numpy(), v2.cpu().numpy(),
        w1.cpu().numpy(), w2.cpu().numpy(),
    )
    return torch.as_tensor(out, dtype=torch.float32)


def _rebin_matrix_relu(z_k_idx: torch.Tensor, aspect: torch.Tensor, dz_ref: float,
                       z_ref_lo: torch.Tensor, z_ref_hi: torch.Tensor,
                       z_ref_dz: torch.Tensor, z0_ref: float = 0.0) -> torch.Tensor:
    """Overlap fraction matrix ``(B, D_ref, nz)`` using ReLU only (avoids ONNX Min/Max)."""
    dz_b = aspect.view(-1, 1, 1) * dz_ref
    z0_b = aspect.view(-1, 1, 1) * z0_ref
    src_lo = z0_b + z_k_idx * dz_b
    src_hi = src_lo + dz_b
    hi_min = src_hi - F.relu(src_hi - z_ref_hi)
    lo_max = src_lo + F.relu(z_ref_lo - src_lo)
    ov = F.relu(hi_min - lo_max)
    return ov / z_ref_dz


class BorderProjectorConv(nn.Module):
    """Differentiable CoreML-safe border-perimeter affine plane projector ``P_BP(g)``.

    Annihilates constant offsets, global OLS planar ramps, and 1-cell border-perimeter planes,
    making residual gravity invariant to all Mod3D regional detrending modes.
    """

    def __init__(self, h: int, w: int):
        super().__init__()
        yy, xx = torch.meshgrid(
            torch.linspace(-1.0, 1.0, h, dtype=torch.float32),
            torch.linspace(-1.0, 1.0, w, dtype=torch.float32),
            indexing="ij",
        )
        phi = torch.stack([torch.ones_like(xx), xx, yy], dim=0)  # (3, H, W)
        mask = torch.zeros(h, w, dtype=torch.bool)
        mask[0, :] = True
        mask[-1, :] = True
        mask[:, 0] = True
        mask[:, -1] = True
        phi_b = phi[:, mask].T.double()
        q_b = torch.linalg.pinv(phi_b)
        w_border = torch.zeros(3, 1, h, w, dtype=torch.float32)
        for k in range(3):
            buf = torch.zeros(h, w, dtype=torch.float32)
            buf[mask] = q_b[k].float()
            w_border[k, 0] = buf
        self.register_buffer("w_border", w_border, persistent=False)
        self.register_buffer("phi", phi.unsqueeze(0), persistent=False)

    def forward(self, g: torch.Tensor) -> torch.Tensor:
        coeffs = F.conv2d(g, self.w_border)
        return g - (coeffs * self.phi).sum(dim=1, keepdim=True)


class OddPyramidGravity(nn.Module):
    """Multi-aspect, variable-clearance pyramid gravity operator + exact transpose adjoint.

    Uses exact 5-aspect near-field ``9x9`` convolutions across all ``ASPECT_MULTI`` knots,
    32-knot rational self-column weights ``u = w / (w + 180)``, border-complete ``3x3`` mid-field
    and ``9x9`` stride-3 tent-upsampled far-field convolutions, exporting as 1 CoreML MLProgram
    partition with 0 CPU fallbacks. Optional ``z0_ref`` offsets the slab top depth in metres
    for V7 multi-tier grids.
    """

    def __init__(self, nx: int, ny: int, nz: int, cell_xy: float = 50.0, dz_ref: float = 50.0,
                 z0_ref: float = 0.0):
        super().__init__()
        self.nx, self.ny, self.nz = int(nx), int(ny), int(nz)
        self.cell_xy = float(cell_xy)
        self.dz_ref = float(dz_ref)
        self.z0_ref = float(z0_ref)

        aspect_knots = torch.tensor([0.55, 0.75, 1.00, 1.20, 1.40], dtype=torch.float32)
        self.n_a = int(aspect_knots.numel())
        self.self_c = 180.0
        near_w = torch.tensor([0.05, 0.25, 0.70, 1.50, 3.0, 6.0, 12.0, 22.0, 40.0], dtype=torch.float32)
        u_near = near_w / (near_w + self.self_c)
        u_rest = torch.linspace(40.0 / (40.0 + self.self_c), 2800.0 / (2800.0 + self.self_c), 24, dtype=torch.float32)[1:]
        u_self_knots = torch.cat([u_near, u_rest])
        self_knots = self.self_c * u_self_knots / (1.0 - u_self_knots)
        # Receiver elevation knots clipped strictly to [-500, +500] m with dense near-relief coverage
        h_knots = torch.tensor(
            [-500.0, -350.0, -220.0, -120.0, -50.0, 0.0, 25.0, 50.0, 75.0, 100.0,
             130.0, 160.0, 195.0, 230.0, 270.0, 310.0, 355.0, 400.0, 450.0, 500.0],
            dtype=torch.float32,
        )
        self.register_buffer("aspect_knots", aspect_knots, persistent=False)
        self.register_buffer("u_self_knots", u_self_knots, persistent=False)
        self.register_buffer("self_knots", self_knots, persistent=False)
        self.register_buffer("h_knots", h_knots, persistent=False)
        L = len(h_knots)
        self.L = L

        k_self = torch.zeros(self.n_a, len(self_knots), dtype=torch.float32)
        u1s = torch.full_like(self_knots, -0.5 * self.cell_xy)
        u2s = torch.full_like(self_knots, 0.5 * self.cell_xy)
        for ia, a in enumerate(aspect_knots):
            k_self[ia] = _prism_gz_exact_t(u1s, u2s, u1s, u2s, self_knots, self_knots + float(a) * self.dz_ref)
        self.register_buffer("k_self", k_self, persistent=False)

        offs = torch.arange(-4, 5, dtype=torch.float32)
        dv, du = torch.meshgrid(offs, offs, indexing="ij")
        near_mask = ~((du == 0) & (dv == 0))
        mid_mask = ~((du.abs() <= 1) & (dv.abs() <= 1))
        u1n, u2n = (du - 0.5) * self.cell_xy, (du + 0.5) * self.cell_xy
        v1n, v2n = (dv - 0.5) * self.cell_xy, (dv + 0.5) * self.cell_xy

        k_near = torch.zeros(self.n_a, L, nz, 9, 9, dtype=torch.float32)
        for ia, a in enumerate(aspect_knots):
            dz_a = float(a) * self.dz_ref
            z0_a = float(a) * self.z0_ref
            for il, hl in enumerate(h_knots):
                hv = float(hl)
                for k in range(nz):
                    w1 = z0_a + k * dz_a - hv
                    w2 = z0_a + (k + 1) * dz_a - hv
                    if abs(w1) < 1e-4:
                        w1 = 1e-4 if w1 >= 0 else -1e-4
                    if abs(w2) < 1e-4:
                        w2 = 1e-4 if w2 >= 0 else -1e-4
                    k_near[ia, il, k] = torch.where(
                        near_mask,
                        _prism_gz_exact_t(u1n, u2n, v1n, v2n, torch.full_like(u1n, w1), torch.full_like(u1n, w2)),
                        0.0,
                    ).flip(-2, -1)
        k_near_flat = k_near.reshape(self.n_a * L, nz, 9, 9)
        self.register_buffer("k_near", k_near_flat, persistent=False)
        self.register_buffer("k_near_t", k_near_flat.flip(-2, -1).transpose(0, 1).contiguous(), persistent=False)
        self.register_buffer("k_near_col_sum", k_near_flat.sum(dim=(-2, -1)).transpose(0, 1).contiguous(), persistent=False)

        z_end = self.z0_ref + nz * self.dz_ref
        z_max = z_end * 1.45
        edge_parts = []
        if self.z0_ref > 0.0:
            edge_parts.append(torch.linspace(0.50 * self.z0_ref, self.z0_ref, max(2, nz // 2) + 1, dtype=torch.float32)[:-1])
        edge_parts.append(self.z0_ref + torch.arange(nz + 1, dtype=torch.float32) * self.dz_ref)
        edge_parts.append(torch.linspace(z_end, z_max, max(4, nz // 4) + 1, dtype=torch.float32)[1:])
        z_ref_edges = torch.cat(edge_parts, dim=0)
        D_ref = z_ref_edges.numel() - 1
        self.D_ref = D_ref
        self.register_buffer("z_ref_lo", z_ref_edges[:-1].view(1, D_ref, 1), persistent=False)
        self.register_buffer("z_ref_hi", z_ref_edges[1:].view(1, D_ref, 1), persistent=False)
        self.register_buffer("z_ref_dz", (z_ref_edges[1:] - z_ref_edges[:-1]).view(1, D_ref, 1), persistent=False)
        self.register_buffer("z_k_idx", torch.arange(nz, dtype=torch.float32).view(1, 1, nz), persistent=False)
        self.register_buffer("z_idx", torch.arange(nz, dtype=torch.float32).view(1, nz, 1, 1), persistent=False)

        u1m, u2m = (du * 3.0 - 1.5) * self.cell_xy, (du * 3.0 + 1.5) * self.cell_xy
        v1m, v2m = (dv * 3.0 - 1.5) * self.cell_xy, (dv * 3.0 + 1.5) * self.cell_xy
        k_mid = torch.zeros(L, D_ref, 9, 9, dtype=torch.float32)

        self.use_far_stride3 = max(nx, ny) >= 32
        if self.use_far_stride3:
            self.far_dil = 3
            self.far_nc_h = (nx - 1 + 2) // 3 + 1
            self.far_nc_w = (ny - 1 + 2) // 3 + 1
            self.far_pad_rb_h = ((self.far_nc_h + 1) * 3 + 4) - (nx - 1)
            self.far_pad_rb_w = ((self.far_nc_w + 1) * 3 + 4) - (ny - 1)
            self.far_crop_h = ((self.far_nc_h - 1) * 3 + 1) - nx
            self.far_crop_w = ((self.far_nc_w - 1) * 3 + 1) - ny
            t1 = torch.tensor([1.0 / 3.0, 2.0 / 3.0, 1.0, 2.0 / 3.0, 1.0 / 3.0], dtype=torch.float32)
            tent2d = (t1[:, None] * t1[None, :]).view(1, 1, 5, 5).repeat(L, 1, 1, 1)
            self.register_buffer("tent_up_w", tent2d, persistent=False)
            self.register_buffer("box_far_k", torch.full((D_ref, 1, 9, 9), 1.0 / 81.0, dtype=torch.float32), persistent=False)
        else:
            self.far_dil = 9

        r_far = int(math.ceil(max(nx, ny) / 9.0))
        self.r_far = r_far
        kf_sz = 2 * r_far + 1
        offs_f = torch.arange(-r_far, r_far + 1, dtype=torch.float32)
        dvf, duf = torch.meshgrid(offs_f, offs_f, indexing="ij")
        far_mask = ~((duf.abs() <= 1) & (dvf.abs() <= 1))
        u1f, u2f = (duf * 9.0 - 4.5) * self.cell_xy, (duf * 9.0 + 4.5) * self.cell_xy
        v1f, v2f = (dvf * 9.0 - 4.5) * self.cell_xy, (dvf * 9.0 + 4.5) * self.cell_xy
        k_far = torch.zeros(L, D_ref, kf_sz, kf_sz, dtype=torch.float32)

        for il, hl in enumerate(h_knots):
            hv = float(hl)
            for j in range(D_ref):
                w1 = float(z_ref_edges[j]) - hv
                w2 = float(z_ref_edges[j + 1]) - hv
                if abs(w1) < 1e-4:
                    w1 = 1e-4 if w1 >= 0 else -1e-4
                if abs(w2) < 1e-4:
                    w2 = 1e-4 if w2 >= 0 else -1e-4
                k_mid[il, j] = torch.where(
                    mid_mask,
                    _prism_gz_exact_t(u1m, u2m, v1m, v2m, torch.full_like(u1m, w1), torch.full_like(u1m, w2)),
                    0.0,
                ).flip(-2, -1)
                k_far[il, j] = torch.where(
                    far_mask,
                    _prism_gz_exact_t(u1f, u2f, v1f, v2f, torch.full_like(u1f, w1), torch.full_like(u1f, w2)),
                    0.0,
                ).flip(-2, -1)

        self.register_buffer("k_mid", k_mid, persistent=False)
        self.register_buffer("k_far", k_far, persistent=False)
        self.register_buffer("k_mid_t", k_mid.flip(-2, -1).transpose(0, 1).contiguous(), persistent=False)
        self.register_buffer("k_far_t", k_far.flip(-2, -1).transpose(0, 1).contiguous(), persistent=False)
        k_mf_col_sum = (k_mid.sum(dim=(-2, -1)) + k_far.sum(dim=(-2, -1))).transpose(0, 1).contiguous()
        self.register_buffer("k_mf_col_sum", k_mf_col_sum, persistent=False)
        il0 = int(torch.argmin(h_knots.abs()))
        eye_y = torch.zeros((1, L, nx, ny), dtype=torch.float32)
        eye_y[0, il0] = 1.0
        am0 = F.pad(F.conv2d(eye_y, self.k_mid_t, padding=13, dilation=3), (2, 2, 2, 2), mode="constant", value=0.0)
        adj_mid_0 = F.avg_pool2d(am0, 3, stride=1, padding=0)[:, :, 2:-2, 2:-2]
        if self.use_far_stride3:
            yf0 = F.pad(eye_y, (0, self.far_crop_w, 0, self.far_crop_h), mode="constant", value=0.0)
            yc0 = F.conv2d(yf0, self.tent_up_w, stride=3, padding=2, groups=L)
            afc0 = F.conv2d(yc0, self.k_far_t, padding=self.far_dil * self.r_far + 2, dilation=self.far_dil)
            adj_far_0 = F.conv_transpose2d(
                afc0, self.box_far_k, stride=3, padding=0, groups=D_ref
            )[:, :, 10:10 + nx, 10:10 + ny]
        else:
            af0 = F.pad(
                F.conv2d(eye_y, self.k_far_t, padding=9 * self.r_far, dilation=9),
                (8, 8, 8, 8), mode="constant", value=0.0,
            )
            adj_far_0 = F.avg_pool2d(af0, 9, stride=1, padding=0)[:, :, 4:-4, 4:-4]
        mf_taper = ((adj_mid_0 + adj_far_0)[0] / k_mf_col_sum[:, il0].view(-1, 1, 1).clamp_min(1e-6)).clamp(0.0, 1.0)
        self.register_buffer("mf_taper", mf_taper.unsqueeze(0), persistent=False)

    def _aspect_and_h_weights(self, z_obs_clip: torch.Tensor, aspect: torch.Tensor):
        wa = _build_knot_weights_v6(aspect.reshape(-1).clamp(0.55, 1.40), self.aspect_knots).view(-1, self.n_a, 1, 1)
        wh = _build_knot_weights_v6(z_obs_clip.squeeze(1), self.h_knots).permute(0, 3, 1, 2).contiguous()
        return wa, wh

    def _self_kernel_voxel(self, z_obs_clip: torch.Tensor, aspect: torch.Tensor, wa: torch.Tensor) -> torch.Tensor:
        w_top = ((self.z0_ref + self.z_idx * self.dz_ref) * aspect - z_obs_clip).clamp(0.05, 2800.0)
        u_top = w_top / (w_top + self.self_c)
        ws_self = _build_knot_weights_v6(u_top, self.u_self_knots)
        k_self_b = (wa.reshape(-1, self.n_a, 1) * self.k_self.unsqueeze(0)).sum(dim=1)
        return (ws_self * k_self_b.view(wa.shape[0], 1, 1, 1, -1)).sum(dim=-1)

    def forward(self, rho: torch.Tensor, z_obs_m: torch.Tensor, aspect: torch.Tensor) -> torch.Tensor:
        if rho.dim() == 5:
            rho = rho.squeeze(1)
        B, D, H, W = rho.shape
        z_obs_clip = z_obs_m.clamp(-500.0, 500.0)
        wa, wh = self._aspect_and_h_weights(z_obs_clip, aspect)
        wah = (wa.unsqueeze(2) * wh.unsqueeze(1)).reshape(B, self.n_a * self.L, H, W)

        g_self = (rho * self._self_kernel_voxel(z_obs_clip, aspect, wa)).sum(dim=1, keepdim=True)
        g_near = (F.conv2d(rho, self.k_near, padding=4) * wah).sum(dim=1, keepdim=True)

        Wz = _rebin_matrix_relu(
            self.z_k_idx, aspect, self.dz_ref, self.z_ref_lo, self.z_ref_hi, self.z_ref_dz, self.z0_ref
        )
        rho_ref = torch.bmm(Wz, rho.reshape(B, D, H * W)).reshape(B, self.D_ref, H, W)
        p_mid = F.avg_pool2d(F.pad(rho_ref, (2, 2, 2, 2), mode="constant", value=0.0), 3, stride=1, padding=0)
        g_mid_pl = F.conv2d(p_mid, self.k_mid, padding=11, dilation=3)

        if self.use_far_stride3:
            r_pad = F.pad(
                rho_ref, (10, self.far_pad_rb_w, 10, self.far_pad_rb_h), mode="constant", value=0.0
            )
            p_far = F.conv2d(r_pad, self.box_far_k, stride=3, padding=0, groups=self.D_ref)
            g_far_c = F.conv2d(p_far, self.k_far, padding=self.far_dil * self.r_far - 2, dilation=self.far_dil)
            g_far_pl = F.conv_transpose2d(g_far_c, self.tent_up_w, stride=3, padding=2, groups=self.L)[:, :, :H, :W]
        else:
            p_far = F.avg_pool2d(F.pad(rho_ref, (4, 4, 4, 4), mode="constant", value=0.0), 9, stride=1, padding=0)
            g_far_pl = F.conv2d(p_far, self.k_far, padding=9 * self.r_far, dilation=9)

        return g_self + g_near + ((g_mid_pl + g_far_pl) * wh).sum(dim=1, keepdim=True)

    def sensitivity_norm(self, valid_mask: torch.Tensor, z_obs_m: torch.Tensor, aspect: torch.Tensor) -> torch.Tensor:
        """Fast CoreML-safe analytical column sensitivity ``~ G^T(valid_mask)`` without 2nd conv adjoint."""
        B, _, H, W = valid_mask.shape
        D = self.nz
        z_obs_clip = z_obs_m.clamp(-500.0, 500.0)
        wa, wh = self._aspect_and_h_weights(z_obs_clip, aspect)
        wah = (wa.unsqueeze(2) * wh.unsqueeze(1)).reshape(B, self.n_a * self.L, H, W)
        s_self = self._self_kernel_voxel(z_obs_clip, aspect, wa) * valid_mask
        vm_near = F.avg_pool2d(valid_mask, 5, stride=1, padding=2)
        s_near = torch.matmul(self.k_near_col_sum, wah.reshape(B, self.n_a * self.L, H * W)).reshape(B, D, H, W) * vm_near
        vm_mf = F.avg_pool2d(valid_mask, 9, stride=1, padding=4)
        s_mf_ref = (
            torch.matmul(self.k_mf_col_sum, wh.reshape(B, self.L, H * W)).reshape(B, self.D_ref, H, W)
            * self.mf_taper
            * vm_mf
        )
        Wz = _rebin_matrix_relu(
            self.z_k_idx, aspect, self.dz_ref, self.z_ref_lo, self.z_ref_hi, self.z_ref_dz, self.z0_ref
        )
        s_mf = torch.bmm(Wz.transpose(1, 2), s_mf_ref.reshape(B, self.D_ref, H * W)).reshape(B, D, H, W)
        return s_self + s_near + s_mf

    def adjoint(self, y: torch.Tensor, z_obs_m: torch.Tensor, aspect: torch.Tensor) -> torch.Tensor:
        B, _, H, W = y.shape
        D = self.nz
        z_obs_clip = z_obs_m.clamp(-500.0, 500.0)
        wa, wh = self._aspect_and_h_weights(z_obs_clip, aspect)
        wah = (wa.unsqueeze(2) * wh.unsqueeze(1)).reshape(B, self.n_a * self.L, H, W)

        adj_self = y * self._self_kernel_voxel(z_obs_clip, aspect, wa)
        adj_near = F.conv2d(y * wah, self.k_near_t, padding=4)

        y_pl = y * wh
        am_pad = F.pad(F.conv2d(y_pl, self.k_mid_t, padding=13, dilation=3), (2, 2, 2, 2), mode="constant", value=0.0)
        adj_mid_ref = F.avg_pool2d(am_pad, 3, stride=1, padding=0)[:, :, 2:-2, 2:-2]

        if self.use_far_stride3:
            y_full = F.pad(y_pl, (0, self.far_crop_w, 0, self.far_crop_h), mode="constant", value=0.0)
            y_c = F.conv2d(y_full, self.tent_up_w, stride=3, padding=2, groups=self.L)
            adj_far_c = F.conv2d(y_c, self.k_far_t, padding=self.far_dil * self.r_far + 2, dilation=self.far_dil)
            adj_far_ref = F.conv_transpose2d(
                adj_far_c, self.box_far_k, stride=3, padding=0, groups=self.D_ref
            )[:, :, 10:10 + H, 10:10 + W]
        else:
            af_pad = F.pad(
                F.conv2d(y_pl, self.k_far_t, padding=9 * self.r_far, dilation=9),
                (8, 8, 8, 8), mode="constant", value=0.0,
            )
            adj_far_ref = F.avg_pool2d(af_pad, 9, stride=1, padding=0)[:, :, 4:-4, 4:-4]

        Wz = _rebin_matrix_relu(
            self.z_k_idx, aspect, self.dz_ref, self.z_ref_lo, self.z_ref_hi, self.z_ref_dz, self.z0_ref
        )
        adj_mf = torch.bmm(
            Wz.transpose(1, 2), (adj_mid_ref + adj_far_ref).reshape(B, self.D_ref, H * W)
        ).reshape(B, D, H, W)
        return adj_self + adj_near + adj_mf


class CoreMLSepConv25DBlock(nn.Module):
    """Residual depthwise-2D + vertical-1D + pointwise block using 2D Convs only (1-partition CoreML)."""

    def __init__(self, ch: int):
        super().__init__()
        self.spatial = nn.Conv2d(ch, ch, kernel_size=3, padding=1, groups=ch)
        self.vert = nn.Conv2d(ch, ch, kernel_size=(3, 1), padding=(1, 0), groups=ch)
        self.pw1 = nn.Conv2d(ch, ch * 2, 1)
        self.pw2 = nn.Conv2d(ch * 2, ch, 1)

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        b, c, d, h, w = x.shape
        xs = self.spatial(x.permute(0, 2, 1, 3, 4).reshape(b * d, c, h, w))
        xv = self.vert(xs.reshape(b, d, c, h * w).transpose(1, 2))
        xg = F.gelu(xv).reshape(b, c, d, h, w).permute(0, 2, 1, 3, 4).reshape(b * d, c, h, w)
        xp = self.pw2(F.gelu(self.pw1(xg))).reshape(b, d, c, h, w).permute(0, 2, 1, 3, 4)
        return x + xp


class CoreMLSwinBlock3D(nn.Module):
    """Cyclic 3D window attention without ``torch.roll`` and without ``>5D`` tensors."""

    def __init__(self, dim: int = 128, heads: int = 4, wh: int = 4, ww: int = 4,
                 shift: int = 0, dropout: float = 0.0):
        super().__init__()
        self.wh, self.ww, self.shift = wh, ww, shift
        self.n1 = nn.LayerNorm(dim)
        self.attn = Attention(dim, heads, dropout=dropout)
        self.n2 = nn.LayerNorm(dim)
        self.mlp = nn.Sequential(
            nn.Linear(dim, 3 * dim),
            nn.GELU(),
            nn.Dropout(dropout) if dropout > 0 else nn.Identity(),
            nn.Linear(3 * dim, dim),
        )

    def forward(self, v: torch.Tensor) -> torch.Tensor:
        b, d, h, w, c = v.shape
        nh, nw = h // self.wh, w // self.ww
        x = v
        if self.shift:
            x = torch.cat([x[:, :, self.shift:, :, :], x[:, :, :self.shift, :, :]], dim=2)
            x = torch.cat([x[:, :, :, self.shift:, :], x[:, :, :, :self.shift, :]], dim=3)
        # Window partition keeping rank <= 5D throughout
        win = (
            x.reshape(b, d, nh, self.wh, w * c)
            .permute(0, 2, 1, 3, 4)
            .reshape(b * nh, d * self.wh, nw, self.ww, c)
            .permute(0, 2, 1, 3, 4)
            .reshape(b * nh * nw, d * self.wh * self.ww, c)
        )
        q = self.n1(win)
        win = win + self.attn(q, q)
        x = (
            win.reshape(b * nh, nw, d * self.wh, self.ww, c)
            .permute(0, 2, 1, 3, 4)
            .reshape(b, nh, d, self.wh, w * c)
            .permute(0, 2, 1, 3, 4)
            .reshape(b, d, h, w, c)
        )
        if self.shift:
            x = torch.cat([x[:, :, -self.shift:, :, :], x[:, :, :-self.shift, :, :]], dim=2)
            x = torch.cat([x[:, :, :, -self.shift:, :], x[:, :, :, :-self.shift, :]], dim=3)
        return x + self.mlp(self.n2(x))


class CoreMLAxialGlobalBlock3D(nn.Module):
    """Global row -> column -> vertical attention with optional Green's vertical distance bias."""

    def __init__(self, dim: int = 128, heads: int = 4, dropout: float = 0.0,
                 dp: int = 8, use_greens_bias: bool = True):
        super().__init__()
        self.n_row = nn.LayerNorm(dim)
        self.attn_row = Attention(dim, heads, dropout=dropout)
        self.n_col = nn.LayerNorm(dim)
        self.attn_col = Attention(dim, heads, dropout=dropout)
        self.n_z = nn.LayerNorm(dim)
        self.attn_z = Attention(dim, heads, dropout=dropout)
        self.greens_bias = GreensVerticalBias(heads, dp, 1, 1) if use_greens_bias else None
        self.n_mlp = nn.LayerNorm(dim)
        self.mlp = nn.Sequential(
            nn.Linear(dim, 3 * dim),
            nn.GELU(),
            nn.Dropout(dropout) if dropout > 0 else nn.Identity(),
            nn.Linear(3 * dim, dim),
        )

    def forward(self, v: torch.Tensor) -> torch.Tensor:
        b, d, h, w, c = v.shape
        xr = self.n_row(v).reshape(b * d * h, w, c)
        v = v + self.attn_row(xr, xr).reshape(b, d, h, w, c)

        xc = self.n_col(v).permute(0, 1, 3, 2, 4).reshape(b * d * w, h, c)
        v = v + self.attn_col(xc, xc).reshape(b, d, w, h, c).permute(0, 1, 3, 2, 4)

        xz = self.n_z(v).permute(0, 2, 3, 1, 4).reshape(b * h * w, d, c)
        z_bias = self.greens_bias() if self.greens_bias is not None else None
        v = v + self.attn_z(xz, xz, attn_bias=z_bias).reshape(b, h, w, d, c).permute(0, 3, 1, 2, 4)

        return v + self.mlp(self.n_mlp(v))


class CoreMLSepSubpixelUp3D(nn.Module):
    """CoreML-safe 2x 3D upsampler using 4D Resize and 2D Convs only (eliminates Conv3D & 5D Resize)."""

    def __init__(self, c_in: int, c_out: int):
        super().__init__()
        self.conv_hw = nn.Conv2d(c_in, c_out, 3, padding=1)
        self.conv_z = nn.Conv2d(c_out, c_out, kernel_size=(3, 1), padding=(1, 0), groups=c_out)
        self.proj = nn.Conv2d(c_out, c_out, 1)

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        b, c, d, h, w = x.shape
        # 2x Z via 4D bilinear on (B, C, D, H*W)
        xz = F.interpolate(
            x.reshape(b, c, d, h * w), scale_factor=(2.0, 1.0), mode="bilinear", align_corners=False
        ).reshape(b, c * 2 * d, h, w)
        # 2x H, W via 4D bilinear on (B, C*2*D, H, W)
        xhw = F.interpolate(xz, scale_factor=2.0, mode="bilinear", align_corners=False)
        d2, h2, w2 = 2 * d, 2 * h, 2 * w
        hw_f = F.gelu(
            self.conv_hw(
                xhw.reshape(b, c, d2, h2, w2).permute(0, 2, 1, 3, 4).reshape(b * d2, c, h2, w2)
            )
        )
        c_out = hw_f.shape[1]
        zf = self.conv_z(hw_f.reshape(b, d2, c_out, h2 * w2).transpose(1, 2))
        out = F.gelu(self.proj(zf)).reshape(b, c_out, d2, h2, w2)
        return out


class CoreMLDepthConditionedSkip5(nn.Module):
    """Terrain + clearance depth-conditioned 2D->3D skip fusion without ONNX ``Expand``."""

    def __init__(self, c_2d: int, ch_out: int):
        super().__init__()
        self.proj_2d = nn.Conv2d(c_2d, ch_out, 1)
        self.gate_2d = nn.Conv2d(c_2d + 1, ch_out, 1)
        self.gate_ds = nn.Conv2d(1, ch_out, 1, bias=False)
        self.gate_z = nn.Conv2d(1, ch_out, 1, bias=False)
        self.gate_out = nn.Conv2d(ch_out, ch_out, 1)

    def forward(self, f_2d: torch.Tensor, clr_s_2d: torch.Tensor,
                d_surf: torch.Tensor, z_top_stage: torch.Tensor) -> torch.Tensor:
        b, _, d, h, w = d_surf.shape
        base = self.proj_2d(f_2d).unsqueeze(2)
        g2d = self.gate_2d(torch.cat([f_2d, clr_s_2d], dim=1)).unsqueeze(2)
        gds = self.gate_ds(d_surf.reshape(b, 1, d * h, w)).reshape(b, -1, d, h, w)
        gz = self.gate_z(z_top_stage.reshape(1, 1, d, 1)).reshape(1, -1, d, 1, 1)
        gate = torch.sigmoid(self.gate_out(F.gelu(g2d + gds + gz).reshape(b, -1, d * h, w))).reshape(
            b, -1, d, h, w
        )
        return base * gate


def _compute_v6_stem_19ch(gn: torch.Tensor, terr: torch.Tensor,
                          valid_mask: torch.Tensor, clr_s: torch.Tensor) -> torch.Tensor:
    """Compute the 19-channel CoreML-safe V6/V7 geophysical surface stem features."""
    def _avg(g: torch.Tensor, k: int) -> torch.Tensor:
        return F.avg_pool2d(g, k, stride=1, padding=k // 2, count_include_pad=False)

    gn_log = (4.0 * gn) / (1.0 + 2.0 * torch.sqrt(gn * gn + 1e-6))
    lp3 = _avg(gn, 3)
    lp9 = _avg(gn, 9)
    lp17 = _avg(gn, 17)
    lp25 = _avg(gn, 25)
    r3, r9, r17, r25 = (gn - lp3) * 6.0, (gn - lp9) * 2.5, (gn - lp17) * 1.5, (gn - lp25) * 1.2

    gp = _rep_pad1(gn)
    gx = 0.5 * (gp[:, :, 2:, 1:-1] - gp[:, :, :-2, 1:-1])
    gy = 0.5 * (gp[:, :, 1:-1, 2:] - gp[:, :, 1:-1, :-2])
    gxx = gp[:, :, 2:, 1:-1] - 2.0 * gn + gp[:, :, :-2, 1:-1]
    gyy = gp[:, :, 1:-1, 2:] - 2.0 * gn + gp[:, :, 1:-1, :-2]
    gzz_fine = (0.0 - (gxx + gyy)) * 4.0

    lp3_p = _rep_pad1(lp3)
    gzz_mid = (0.0 - (
        (lp3_p[:, :, 2:, 1:-1] - 2.0 * lp3 + lp3_p[:, :, :-2, 1:-1])
        + (lp3_p[:, :, 1:-1, 2:] - 2.0 * lp3 + lp3_p[:, :, 1:-1, :-2])
    )) * 6.0

    grad_mag = torch.sqrt(gx * gx + gy * gy + 1e-8)
    dz_proxy = gn - _avg(gn, 5)
    tilt_sin = dz_proxy / torch.sqrt(dz_proxy * dz_proxy + grad_mag * grad_mag + 1e-4)
    tot_grad = torch.sqrt(grad_mag * grad_mag + dz_proxy * dz_proxy + 1e-5)
    gn_mag = torch.sqrt(gn * gn + 1e-8)
    r17_mag = torch.sqrt((gn - lp17) * (gn - lp17) + 1e-8)
    euler_depth = torch.tanh((gn_mag + r17_mag) / (tot_grad * 4.0 + 0.05))

    jxx = _avg(gx * gx, 3)
    jyy = _avg(gy * gy, 3)
    jxy = _avg(gx * gy, 3)
    anis = torch.sqrt((jxx - jyy) ** 2 + 4.0 * jxy * jxy + 1e-8)
    coherence = anis / (jxx + jyy + 1e-5)
    strike_cos2 = (jxx - jyy) / (anis + 1e-5)
    strike_sin2 = (2.0 * jxy) / (anis + 1e-5)

    tp = _rep_pad1(terr)
    tx = 0.5 * (tp[:, :, 2:, 1:-1] - tp[:, :, :-2, 1:-1])
    ty = 0.5 * (tp[:, :, 1:-1, 2:] - tp[:, :, 1:-1, :-2])
    terr_slope = torch.sqrt(tx * tx + ty * ty + 1e-8) * 16.0
    terr_curv = (
        (tp[:, :, 2:, 1:-1] - 2.0 * terr + tp[:, :, :-2, 1:-1])
        + (tp[:, :, 1:-1, 2:] - 2.0 * terr + tp[:, :, 1:-1, :-2])
    ) * 16.0

    return torch.cat([
        gn, gn_log, r3, r9, r17, r25,
        gzz_fine, gzz_mid, grad_mag * 6.0, tilt_sin, euler_depth,
        coherence, strike_cos2, strike_sin2, valid_mask * 2.0 - 1.0,
        terr * 4.0, terr_slope, terr_curv, clr_s * 2.0 - 1.0,
    ], dim=1)


def _decode_v6_density(
    hv: torch.Tensor,
    p_head: torch.Tensor,
    mask: torch.Tensor,
    ch_vox: int,
    num_classes: int,
    n_sub: int,
    logit_log_scale: torch.Tensor,
    cls_log_sharp: torch.Tensor,
    cls_depth_prior: nn.Module,
    rock_head: nn.Module,
    res_head: nn.Module,
    rock_lo: torch.Tensor,
    rock_span: torch.Tensor,
    cav_vals: torch.Tensor,
    d_surf: torch.Tensor | None = None,
):
    """Shared CoreML-safe V6/V7 prototype + bounded rock + residual density decoder."""
    b, _, d, h, w = hv.shape
    hv_4d = hv.permute(0, 2, 1, 3, 4).reshape(b * d, ch_vox, h, w)
    hv_flat = hv.reshape(b, ch_vox, d * h * w)
    logit_scale = logit_log_scale.exp().clamp(0.5, 4.0)
    sub_logits = torch.bmm(p_head, hv_flat) * logit_scale  # (B, K*n_sub, D*H*W)
    # Reshape without 5D softmax: pool sub-prototypes then run 4D softmax over classes
    sub_4d = (
        sub_logits.reshape(b, num_classes, n_sub, d, h * w)
        .amax(dim=2)
        .permute(0, 2, 1, 3)
        .reshape(b * d, num_classes, h, w)
    )
    if d_surf is not None:
        ds_4d = d_surf.permute(0, 2, 1, 3, 4).reshape(b * d, 1, h, w)
        sub_4d = sub_4d + cls_depth_prior(ds_4d)
    probs_4d = F.softmax(sub_4d, dim=1)
    p_bg = probs_4d[:, 0:1]
    p_fg = probs_4d[:, 1:]
    p_fg_sum = p_fg.sum(dim=1, keepdim=True)
    occ_denom = (p_fg_sum + p_bg).clamp_min(1e-6)
    occ_raw = p_fg_sum / occ_denom
    occ_4d = 0.35 * occ_raw + 0.65 * ((occ_raw - 0.15) * 1.45).clamp(0.0, 1.0)

    cls_sharp = cls_log_sharp.exp().clamp(0.5, 4.0)
    active_probs = F.softmax(sub_4d[:, 1:] * cls_sharp, dim=1)

    rock_u = torch.sigmoid(rock_head(hv_4d))
    rock_dens = rock_lo + rock_span * rock_u
    cav_dens = cav_vals + 0.0 * rock_u[:, :1]  # clean broadcast without Expand
    cls_dens = torch.cat([cav_dens, rock_dens], dim=1)
    rho_base = (active_probs * cls_dens).sum(dim=1, keepdim=True)
    rho_res = 0.20 * torch.tanh(res_head(hv_4d))
    rho_4d = occ_4d * (rho_base + rho_res)

    rho = rho_4d.reshape(b, d, h, w).unsqueeze(1) * mask
    probs = probs_4d.reshape(b, d, num_classes, h, w).permute(0, 2, 1, 3, 4)
    logits = sub_4d.reshape(b, d, num_classes, h, w).permute(0, 2, 1, 3, 4)
    occ = occ_4d.reshape(b, d, h, w).unsqueeze(1)
    return rho, probs, logits, occ, probs_4d, hv_4d


def _compute_v6_logvar(
    hv_4d: torch.Tensor,
    pred: torch.Tensor,
    probs_4d: torch.Tensor,
    mask: torch.Tensor,
    d_surf: torch.Tensor,
    bp_norm: torch.Tensor,
    cues_2d: torch.Tensor,
    logvar_hv: nn.Module,
    logvar_3d: nn.Module,
    logvar_2d: nn.Module,
    logvar_out: nn.Module,
) -> torch.Tensor:
    """Shared CoreML-safe Gaussian log_var head across V6 full-res and V7 slab tiers."""
    b, _, d, h, w = pred.shape
    entropy_4d = (1.0 - (probs_4d * probs_4d).sum(dim=1, keepdim=True)) * mask.reshape(b * d, 1, h, w)
    p_pad = _rep_pad1(pred.reshape(b * d, 1, h, w)).reshape(b, d, h + 2, w + 2)
    dx_r = 0.5 * (p_pad[:, :, 2:, 1:-1] - p_pad[:, :, :-2, 1:-1])
    dy_r = 0.5 * (p_pad[:, :, 1:-1, 2:] - p_pad[:, :, 1:-1, :-2])
    p_zpad = torch.cat([pred[:, :, :1], pred, pred[:, :, -1:]], dim=2)
    dz_r = 0.5 * (p_zpad[:, :, 2:] - p_zpad[:, :, :-2]).squeeze(1)
    grad_rho_3d = torch.sqrt(dx_r * dx_r + dy_r * dy_r + dz_r * dz_r + 1e-6).unsqueeze(1)

    cues_3d = torch.cat([
        d_surf,
        entropy_4d.reshape(b, 1, d, h, w),
        grad_rho_3d,
        bp_norm,
    ], dim=1).permute(0, 2, 1, 3, 4).reshape(b * d, 4, h, w)
    lv_2d = logvar_2d(cues_2d).unsqueeze(1)  # (B, 1, lv_ch, H, W) -> broadcasts cleanly across D
    lv_hv_3d = (logvar_hv(hv_4d) + logvar_3d(cues_3d)).reshape(b, d, -1, h, w)
    lv_feat = F.gelu((lv_hv_3d + lv_2d).reshape(b * d, -1, h, w))
    log_var = logvar_out(lv_feat).reshape(b, d, 1, h, w).permute(0, 2, 1, 3, 4)
    return log_var.clamp(-8.0, 4.0) * mask


def _adapt_state_dict_tensor(src: torch.Tensor, base_tgt: torch.Tensor, zero_pad_new: bool = True) -> torch.Tensor:
    """Adapt a checkpoint tensor to ``base_tgt.shape`` by copying overlapping slices."""
    if src.shape == base_tgt.shape:
        return src
    if src.ndim != base_tgt.ndim:
        return base_tgt.detach().clone()
    out = torch.zeros_like(base_tgt) if zero_pad_new else base_tgt.detach().clone()
    slices = tuple(slice(0, min(s, d)) for s, d in zip(src.shape, base_tgt.shape))
    out[slices] = src[slices].to(dtype=out.dtype)
    return out


class TierProlongation3D(nn.Module):
    """Option I CoreML-safe prolongation tail mapping 3 depth slabs back to ``(nz, nx, ny)``.

    Uses exact separable 4D bilinear interpolation (numerically equivalent to
    ``F.interpolate(..., mode="trilinear", align_corners=False)`` without unsupported 5D
    ONNX ``Resize`` for CoreML MLProgram single-partition compilation) plus lightweight
    ``1x1`` and vertical-contact refinement across tier seams.
    """

    def __init__(self, nx: int, ny: int, nz: int, d0: int, n1_fine: int, n2_fine: int):
        super().__init__()
        self.nx, self.ny, self.nz = int(nx), int(ny), int(nz)
        self.d0, self.n1_fine, self.n2_fine = int(d0), int(n1_fine), int(n2_fine)
        self.fine_depths = (self.d0, self.n1_fine, self.n2_fine)
        self.rho_vert = nn.Conv2d(1, 8, kernel_size=(3, 1), padding=(1, 0))
        self.rho_pw = nn.Conv2d(9, 1, kernel_size=1)
        self.lv_pw1 = nn.Conv2d(1, 4, kernel_size=1)
        self.lv_pw2 = nn.Conv2d(4, 1, kernel_size=1)
        with torch.no_grad():
            self.rho_pw.weight.zero_()
            self.rho_pw.bias.zero_()
            self.lv_pw2.weight.zero_()
            self.lv_pw2.bias.zero_()

    def prolong_tensor(self, slabs: tuple[torch.Tensor, ...], mask: torch.Tensor | None = None) -> torch.Tensor:
        parts = [
            _trilinear_coreml(t, (d_fine, self.nx, self.ny))
            for t, d_fine in zip(slabs, self.fine_depths)
        ]
        out = torch.cat(parts, dim=2)
        if mask is not None:
            out = out * mask
        return out

    def forward(
        self,
        tier_rhos: tuple[torch.Tensor, ...],
        tier_lvs: tuple[torch.Tensor, ...] | None = None,
        mask: torch.Tensor | None = None,
        rho_min: float = RHO_AIR - 0.20,
        rho_max: float = RHO_MAX_V6 + 0.20,
    ) -> tuple[torch.Tensor, torch.Tensor | None]:
        rho_cat = self.prolong_tensor(tier_rhos)
        b = rho_cat.shape[0]
        rz = F.gelu(self.rho_vert(rho_cat.reshape(b, 1, self.nz, self.nx * self.ny)))
        r_in = torch.cat([
            rho_cat.permute(0, 2, 1, 3, 4).reshape(b * self.nz, 1, self.nx, self.ny),
            rz.reshape(b, 8, self.nz, self.nx, self.ny).permute(0, 2, 1, 3, 4).reshape(b * self.nz, 8, self.nx, self.ny),
        ], dim=1)
        rho_delta = self.rho_pw(r_in).reshape(b, self.nz, 1, self.nx, self.ny).permute(0, 2, 1, 3, 4)
        rho = (rho_cat + 0.25 * torch.tanh(rho_delta)).clamp(float(rho_min), float(rho_max))
        if mask is not None:
            rho = rho * mask

        if tier_lvs is None:
            return rho, None

        lv_cat = self.prolong_tensor(tier_lvs)
        lv_in = lv_cat.permute(0, 2, 1, 3, 4).reshape(b * self.nz, 1, self.nx, self.ny)
        lv_delta = self.lv_pw2(F.gelu(self.lv_pw1(lv_in)))
        lv_delta = lv_delta.reshape(b, self.nz, 1, self.nx, self.ny).permute(0, 2, 1, 3, 4)
        log_var = (lv_cat + 0.5 * torch.tanh(lv_delta)).clamp(-8.0, 4.0)
        if mask is not None:
            log_var = log_var * mask
        return rho, log_var


class PotentialFieldTransformer3DV6(nn.Module):
    """V6 Detrend-Invariant, Variable-Clearance, Two-Stage Physics-Feedback 3D Inversion Network.

    Designed for CoreML MLProgram single-partition static export (0 CPU fallbacks) and FP16 safety:
      * 5-channel input: ``[gz_50m_eq, terr_norm, valid_mask, log(aspect), clr_norm_clamped_8]``.
      * Algebraic/ReLU/Slice+Concat stem & masking: eliminates Abs, Neg, Log, Pad(edge), Expand,
        GreaterOrEqual, ReduceLogSumExp, torch.roll, >5D Swin, and 5D Resize.
      * Stage-1 ``OddPyramidGravity(32x32x16)`` + ``BorderProjectorConv`` + ``D1^{-1} G_mid^T(r_bp)``.
      * Stage-2 ``OddPyramidGravity(64x64x32)`` full-resolution physics residual on ``rho_init``
        feeding ``dec_vox2``, closed ``_ROCK_BOUNDS_V6`` + ``±0.08*tanh`` graded density head, and
        7-cue full-resolution Gaussian ``logvar_head`` bounded to ``[-8.0, 4.0]``.
    """

    def __init__(self, nx=64, ny=64, nz=32, patch=4, embed_dim=128, num_heads=4, depth=6,
                 ch_mid=40, ch_vox=24, n_sub=3, num_classes=7, dropout=0.05, dec_depth=2, **_unused):
        super().__init__()
        assert patch == 4 and nx % (4 * patch) == 0 and ny % (4 * patch) == 0 and nz % 4 == 0
        self.nx, self.ny, self.nz, self.p, self.n_sub = nx, ny, nz, patch, n_sub
        self.num_classes = num_classes
        self.hp, self.wp, self.dp = nx // patch, ny // patch, nz // 4
        self.ch_mid, self.ch_vox = ch_mid, ch_vox
        self.dropout_rate = float(dropout)
        self.dec_depth = max(2, int(dec_depth))
        c = embed_dim
        c0, c1 = max(24, c // 4), max(48, c // 2)

        # 8-dim algebraic scale/geometry conditioning MLP (zero Log ops)
        self.scale_mlp = nn.Sequential(nn.Linear(8, c), nn.GELU(), nn.Linear(c, c))

        # 2D U-Net encoder (18-channel CoreML-safe geophysical stem + clearance channel = 19)
        self.enc0 = nn.Sequential(
            nn.Conv2d(19, c0, 3, padding=1), nn.GELU(),
            nn.Conv2d(c0, c0, 3, padding=1), nn.GELU(),
        )
        self.enc1 = nn.Sequential(
            nn.Conv2d(c0, c1, 3, stride=2, padding=1), nn.GELU(),
            nn.Conv2d(c1, c1, 3, padding=1), nn.GELU(),
        )
        self.enc2 = nn.Sequential(
            nn.Conv2d(c1, c, 3, stride=2, padding=1), nn.GELU(),
            nn.Conv2d(c, c, 3, padding=1),
        )
        self.pos2d = nn.Parameter(torch.randn(1, self.hp * self.wp, c) * 0.02)
        self.n2d_1 = nn.LayerNorm(c)
        self.attn2d = Attention(c, num_heads, dropout=dropout)
        self.n2d_2 = nn.LayerNorm(c)
        self.mlp2d = nn.Sequential(nn.Linear(c, 3 * c), nn.GELU(), nn.Linear(3 * c, c))

        # Depth lift + non-expanding 3D depth conditioning
        self.depth_lift = nn.Sequential(nn.LayerNorm(c), nn.Linear(c, self.dp * c))
        self.pos3d = nn.Parameter(torch.randn(1, self.dp, self.hp, self.wp, c) * 0.02)
        self.dc_2d = nn.Conv2d(2, c, 1)  # [clear_s_c, log_aspect_c]
        self.dc_ds = nn.Conv2d(1, c, 1, bias=False)
        self.dc_z = nn.Conv2d(1, c, 1, bias=False)
        self.dc_sens = nn.Conv2d(1, c, 1, bias=False)
        self.dc_out = nn.Sequential(nn.GELU(), nn.Conv2d(c, c, 1))

        # 3D CoreML-safe Transformer backbone (alternating Swin + AxialGlobal)
        self.blocks = nn.ModuleList([
            (
                CoreMLAxialGlobalBlock3D(c, num_heads, dropout=dropout, dp=self.dp)
                if i % 2
                else CoreMLSwinBlock3D(c, num_heads, wh=4, ww=4, shift=(2 if (i // 2) % 2 else 0), dropout=dropout)
            )
            for i in range(depth)
        ])

        # Lithology prototype cross-attention (7 classes x n_sub)
        n_proto = num_classes * n_sub
        self.proto_emb = nn.Parameter(torch.randn(1, n_proto, c) * 0.02)
        self.proto_scale = nn.Linear(c, c)
        self.proto_nq = nn.LayerNorm(c)
        self.proto_nk = nn.LayerNorm(c)
        self.proto_attn = Attention(c, num_heads, dropout=dropout)
        self.proto_mlp_n = nn.LayerNorm(c)
        self.proto_mlp = nn.Sequential(nn.Linear(c, 3 * c), nn.GELU(), nn.Linear(3 * c, c))
        self.s2p_nq = nn.LayerNorm(c)
        self.s2p_nk = nn.LayerNorm(c)
        self.s2p_attn = Attention(c, num_heads, dropout=dropout)
        self.s2p_mlp_n = nn.LayerNorm(c)
        self.s2p_mlp = nn.Sequential(nn.Linear(c, 3 * c), nn.GELU(), nn.Linear(3 * c, c))
        self.norm3d = nn.LayerNorm(c)

        # Stage 1 -> (nz/2, nx/2, ny/2) + Exact Pyramid Gravity & Detrend-Invariant Adjoint Feedback
        self.proj_mid = nn.Conv2d(c, ch_mid, 1)
        self.up_mid = CoreMLSepSubpixelUp3D(ch_mid, ch_mid)
        self.skip_mid = CoreMLDepthConditionedSkip5(c1, ch_mid)
        self.dec_mid1 = CoreMLSepConv25DBlock(ch_mid)
        self.coarse_rho_head = nn.Conv2d(ch_mid, 1, 1)
        with torch.no_grad():
            self.coarse_rho_head.weight.mul_(0.05)
            self.coarse_rho_head.bias.fill_(math.log(-RHO_AIR / RHO_MAX_V6))

        nz_mid, nx_mid, ny_mid = nz // 2, nx // 2, ny // 2
        self.g_mid = OddPyramidGravity(nx_mid, ny_mid, nz_mid, cell_xy=2.0 * REF_CELL, dz_ref=2.0 * REF_CELL)
        self.bp_mid = BorderProjectorConv(nx_mid, ny_mid)
        for p in self.g_mid.parameters():
            p.requires_grad_(False)
        for p in self.bp_mid.parameters():
            p.requires_grad_(False)

        self.res_mid_2d = nn.Sequential(
            nn.Conv2d(4, ch_mid, 3, padding=1), nn.GELU(),
            nn.Conv2d(ch_mid, ch_mid, 1),
        )
        self.res_mid_ds = nn.Conv2d(1, ch_mid, 1)
        self.res_mid_bp = nn.Conv2d(1, ch_mid, 1)
        self.res_mid_gate = nn.Parameter(torch.zeros(1))
        self.dec_mid2 = CoreMLSepConv25DBlock(ch_mid)
        if self.dec_depth > 2:
            self.extra_dec_mid = nn.ModuleList([CoreMLSepConv25DBlock(ch_mid) for _ in range(self.dec_depth - 2)])

        # Stage 2 -> (nz, nx, ny) + Full-Res Pyramid Gravity Feedback on rho_init
        self.up_vox = CoreMLSepSubpixelUp3D(ch_mid, ch_vox)
        self.skip_vox = CoreMLDepthConditionedSkip5(c0, ch_vox)
        self.scale_vox = nn.Linear(c, ch_vox)
        self.dec_vox1 = CoreMLSepConv25DBlock(ch_vox)

        self.g_64 = OddPyramidGravity(nx, ny, nz, cell_xy=REF_CELL, dz_ref=REF_CELL)
        self.bp_64 = BorderProjectorConv(nx, ny)
        for p in self.g_64.parameters():
            p.requires_grad_(False)
        for p in self.bp_64.parameters():
            p.requires_grad_(False)

        self.res_vox_2d = nn.Sequential(
            nn.Conv2d(4, ch_vox, 3, padding=1), nn.GELU(),
            nn.Conv2d(ch_vox, ch_vox, 1),
        )
        self.res_vox_3d = nn.Conv2d(2, ch_vox, 1)  # [bp_64_norm_z, rho_init]
        self.res_vox_gate = nn.Parameter(torch.zeros(1))
        self.bp_z_beta = nn.Parameter(torch.tensor(0.85))

        self.dec_vox2 = CoreMLSepConv25DBlock(ch_vox)
        if self.dec_depth > 2:
            self.extra_dec_vox = nn.ModuleList([CoreMLSepConv25DBlock(ch_vox) for _ in range(self.dec_depth - 2)])
        self.vox_drop = nn.Dropout3d(p=dropout) if dropout > 0 else nn.Identity()

        # Decoupled Stage-1 (rho_init) and Stage-2 final prototype/rock/residual heads
        self.proto_head_init = nn.Linear(c, ch_vox)
        self.rock_head_init = nn.Conv2d(ch_vox, len(_ROCK_BOUNDS_V6), 1)
        self.res_head_init = nn.Conv2d(ch_vox, 1, 1)
        self.proto_head = nn.Linear(c, ch_vox)
        self.logit_log_scale = nn.Parameter(torch.zeros(1))
        self.cls_log_sharp = nn.Parameter(torch.zeros(1))
        self.rock_head = nn.Conv2d(ch_vox, len(_ROCK_BOUNDS_V6), 1)
        self.res_head = nn.Conv2d(ch_vox, 1, 1)
        self.cls_depth_prior = nn.Conv2d(1, num_classes, 1)
        with torch.no_grad():
            self.res_head_init.weight.mul_(0.25)
            self.res_head_init.bias.zero_()
            self.res_head.weight.mul_(0.25)
            self.res_head.bias.zero_()
            self.cls_depth_prior.weight.zero_()
            self.cls_depth_prior.bias.zero_()
        self.edt_head = nn.Conv2d(ch_vox, 1, 1)

        # Stage-2 full-resolution 7-cue Gaussian log_var head
        lv_ch = max(12, ch_vox)
        self.logvar_hv = nn.Conv2d(ch_vox, lv_ch, 1)
        self.logvar_3d = nn.Conv2d(4, lv_ch, 1, bias=False)  # [d_surf, entropy_proxy, grad_rho_3d, bp_64_norm]
        self.logvar_2d = nn.Conv2d(3, lv_ch, 1, bias=False)  # [clr_smooth, 1-valid_mask, r_bp_64_energy]
        self.logvar_out = nn.Conv2d(lv_ch, 1, 1)
        with torch.no_grad():
            self.logvar_out.bias.fill_(-4.0)

        # Geometry and class density buffers
        self.register_buffer("z_top_coarse", (torch.arange(nz // 4, dtype=torch.float32) / (nz // 4)).view(1, 1, nz // 4, 1, 1), persistent=False)
        self.register_buffer("z_top_mid", (torch.arange(nz // 2, dtype=torch.float32) / (nz // 2)).view(1, 1, nz // 2, 1, 1), persistent=False)
        self.register_buffer("z_top", (torch.arange(nz, dtype=torch.float32) / nz).view(1, 1, nz, 1, 1), persistent=False)
        self.register_buffer("cav_vals", torch.tensor([RHO_AIR, RHO_WATER], dtype=torch.float32).view(1, 2, 1, 1), persistent=False)
        lo_t = torch.tensor([b[0] for b in _ROCK_BOUNDS_V6], dtype=torch.float32).view(1, len(_ROCK_BOUNDS_V6), 1, 1)
        span_t = torch.tensor([b[1] - b[0] for b in _ROCK_BOUNDS_V6], dtype=torch.float32).view(1, len(_ROCK_BOUNDS_V6), 1, 1)
        self.register_buffer("rock_lo", lo_t, persistent=False)
        self.register_buffer("rock_span", span_t, persistent=False)

    @staticmethod
    def _avg(g: torch.Tensor, k: int) -> torch.Tensor:
        return F.avg_pool2d(g, k, stride=1, padding=k // 2, count_include_pad=False)

    def _stem_features_v6(self, gn: torch.Tensor, terr: torch.Tensor,
                          valid_mask: torch.Tensor, clr_s: torch.Tensor) -> torch.Tensor:
        return _compute_v6_stem_19ch(gn, terr, valid_mask, clr_s)

    def _unpack_input(self, x: torch.Tensor):
        c_in = x.shape[1]
        gz = x[:, 0:1]
        terr = x[:, 1:2].clamp(0.0, 0.25)
        valid_mask = x[:, 2:3].clamp(0.0, 1.0) if c_in >= 3 else torch.ones_like(gz)
        log_aspect = x[:, 3:4] if c_in >= 4 else torch.zeros_like(gz)
        clearance = x[:, 4:5].clamp(0.0, 8.0) if c_in >= 5 else torch.full_like(gz, 0.35 / REF_CELL)
        return gz, terr, valid_mask, log_aspect, clearance

    @staticmethod
    def _air_mask(z_top: torch.Tensor, terr_5d: torch.Tensor) -> torch.Tensor:
        """FP16-safe terrain subsurface mask without ONNX ``GreaterOrEqual`` or ``1e5`` overflow."""
        return ((z_top - terr_5d).clamp(-0.001, 0.0) * 32768.0 + 1.0).clamp(0.0, 1.0)

    def _load_from_state_dict(self, state_dict, prefix, local_metadata, strict,
                              missing_keys, unexpected_keys, error_msgs):
        """Auto-migrate v6.0 checkpoints cleanly on strict=True into v6.1 modules."""
        if f"{prefix}proto_head.weight" in state_dict and f"{prefix}proto_head_init.weight" not in state_dict:
            for src, dst in (
                ("proto_head.weight", "proto_head_init.weight"),
                ("proto_head.bias", "proto_head_init.bias"),
                ("rock_head.weight", "rock_head_init.weight"),
                ("rock_head.bias", "rock_head_init.bias"),
                ("res_head.weight", "res_head_init.weight"),
                ("res_head.bias", "res_head_init.bias"),
            ):
                sk, dk = f"{prefix}{src}", f"{prefix}{dst}"
                if sk in state_dict and dk not in state_dict:
                    state_dict[dk] = state_dict[sk].detach().clone()
            for name, tgt in (
                ("cls_depth_prior.weight", self.cls_depth_prior.weight),
                ("cls_depth_prior.bias", self.cls_depth_prior.bias),
                ("bp_z_beta", self.bp_z_beta),
            ):
                fk = f"{prefix}{name}"
                if fk not in state_dict:
                    state_dict[fk] = torch.zeros_like(tgt)
            for idx, blk in enumerate(self.blocks):
                gb = getattr(blk, "greens_bias", None)
                if gb is not None and f"{prefix}blocks.{idx}.greens_bias.gain" not in state_dict:
                    state_dict[f"{prefix}blocks.{idx}.greens_bias.raw_decay"] = gb.raw_decay.detach().clone()
                    state_dict[f"{prefix}blocks.{idx}.greens_bias.gain"] = torch.zeros_like(gb.gain)
                    state_dict[f"{prefix}blocks.{idx}.greens_bias.asym"] = torch.zeros_like(gb.asym)
        super()._load_from_state_dict(
            state_dict, prefix, local_metadata, strict, missing_keys, unexpected_keys, error_msgs
        )

    def _decode_density(self, hv: torch.Tensor, p_head: torch.Tensor, mask: torch.Tensor,
                        d_surf: torch.Tensor | None = None, rock_head=None, res_head=None):
        return _decode_v6_density(
            hv, p_head, mask, self.ch_vox, self.num_classes, self.n_sub,
            self.logit_log_scale, self.cls_log_sharp, self.cls_depth_prior,
            self.rock_head if rock_head is None else rock_head,
            self.res_head if res_head is None else res_head,
            self.rock_lo, self.rock_span, self.cav_vals, d_surf=d_surf,
        )

    def enable_mc_dropout(self, enabled: bool = True):
        """Enable or disable Dropout / Dropout3d layers for MC Dropout uncertainty."""
        for m in self.modules():
            if isinstance(m, (nn.Dropout, nn.Dropout3d)):
                m.train(enabled)

    def forward_uncertainty(self, x: torch.Tensor):
        """Return ``(density, log_var)`` tensors (used by ONNX export & CoreML inference)."""
        pred, aux = self.forward(x, return_aux=True)
        return pred, aux["log_var"]

    def forward(self, x: torch.Tensor, return_aux: bool = False):
        b = x.shape[0]
        gz, terr, valid_mask, log_aspect, clearance = self._unpack_input(x)
        clr_s = clearance / (clearance + 1.0)
        aspect = torch.exp(log_aspect.mean(dim=(2, 3), keepdim=True)).clamp(0.55, 1.40)

        s = _abs_coreml(gz).amax(dim=(2, 3), keepdim=True).clamp_min(1e-6)
        gn = gz / s
        h_in = self._stem_features_v6(gn, terr, valid_mask, clr_s)

        s_flat = s.view(b, 1)
        s_slab = s_flat / SLAB
        terr_span = terr.amax(dim=(2, 3)).view(b, 1) - terr.amin(dim=(2, 3)).view(b, 1)
        s_vec = torch.cat([
            s_slab / (s_slab + 1.0),
            s_slab / (s_slab + 0.15),
            gz.amax(dim=(2, 3)).view(b, 1) / s_flat,
            gz.amin(dim=(2, 3)).view(b, 1) / s_flat,
            terr.mean(dim=(2, 3)).view(b, 1) * 4.0,
            terr_span * 4.0,
            log_aspect.mean(dim=(2, 3)).view(b, 1),
            clr_s.mean(dim=(2, 3)).view(b, 1),
        ], dim=1)
        se = self.scale_mlp(s_vec)

        f0 = self.enc0(h_in)
        f1 = self.enc1(f0)
        f2 = self.enc2(f1)

        t2 = f2.flatten(2).transpose(1, 2) + self.pos2d + se.unsqueeze(1)
        t2 = t2 + self.attn2d(self.n2d_1(t2), self.n2d_1(t2))
        t2 = t2 + self.mlp2d(self.n2d_2(t2))

        v = self.depth_lift(t2).view(b, self.hp, self.wp, self.dp, -1).permute(0, 3, 1, 2, 4)

        # Non-expanding depth + clearance conditioning
        terr_c = F.avg_pool2d(terr, 4)
        clr_s_c = F.avg_pool2d(clr_s, 4)
        asp_c = F.avg_pool2d(log_aspect, 4)
        d_surf_c = F.relu(self.z_top_coarse - terr_c.unsqueeze(2))  # (B, 1, dp, hp, wp)
        sens_c = 0.04 / ((d_surf_c + clr_s_c.unsqueeze(2) * 0.16 + 0.12) ** 2)
        dc_f = (
            self.dc_2d(torch.cat([clr_s_c, asp_c], dim=1)).unsqueeze(2)
            + self.dc_ds(d_surf_c.reshape(b, 1, self.dp * self.hp, self.wp)).reshape(b, -1, self.dp, self.hp, self.wp)
            + self.dc_z(self.z_top_coarse.reshape(1, 1, self.dp, 1)).reshape(1, -1, self.dp, 1, 1)
            + self.dc_sens(sens_c.reshape(b, 1, self.dp * self.hp, self.wp)).reshape(b, -1, self.dp, self.hp, self.wp)
        )
        dc_mod = self.dc_out(dc_f.reshape(b, -1, self.dp * self.hp, self.wp)).reshape(
            b, -1, self.dp, self.hp, self.wp
        ).permute(0, 2, 3, 4, 1)
        v = v + self.pos3d + dc_mod + se.view(b, 1, 1, 1, -1)

        for blk in self.blocks:
            v = blk(v)

        vf = v.reshape(b, self.dp * self.hp * self.wp, -1)
        p_ctx = self.proto_emb + self.proto_scale(se).unsqueeze(1)
        vf = vf + self.proto_attn(self.proto_nq(vf), self.proto_nk(p_ctx))
        vf = vf + self.proto_mlp(self.proto_mlp_n(vf))
        p_ctx = p_ctx + self.s2p_attn(self.s2p_nq(p_ctx), self.s2p_nk(vf))
        p_ctx = p_ctx + self.s2p_mlp(self.s2p_mlp_n(p_ctx))
        v = self.norm3d(vf).view(b, self.dp, self.hp, self.wp, -1)

        # Observation elevation in physical 50m-equivalent metres
        total_depth_m = self.nz * REF_CELL * aspect
        z_obs_m = terr * total_depth_m - clearance * REF_CELL
        z_obs_mid_m = F.avg_pool2d(z_obs_m, 2)

        # Stage 1 -> (nz/2, nx/2, ny/2) + Exact Pyramid Gravity & P_BP Adjoint Feedback
        vc = self.proj_mid(v.permute(0, 4, 1, 2, 3).reshape(b, -1, self.dp * self.hp, self.wp)).reshape(
            b, -1, self.dp, self.hp, self.wp
        )
        u = self.up_mid(vc)
        terr_mid = F.avg_pool2d(terr, 2)
        clr_s_mid = F.avg_pool2d(clr_s, 2)
        d_surf_mid = F.relu(self.z_top_mid - terr_mid.unsqueeze(2))
        u = u + self.skip_mid(f1, clr_s_mid, d_surf_mid, self.z_top_mid)
        u = self.dec_mid1(u)

        nz_mid, nx_mid, ny_mid = self.nz // 2, self.nx // 2, self.ny // 2
        mask_mid = self._air_mask(self.z_top_mid, terr_mid.unsqueeze(2))
        coarse_raw = self.coarse_rho_head(u.reshape(b, self.ch_mid, nz_mid * nx_mid, ny_mid)).reshape(
            b, 1, nz_mid, nx_mid, ny_mid
        )
        coarse_pred = (RHO_AIR + (RHO_MAX_V6 - RHO_AIR) * torch.sigmoid(coarse_raw)) * mask_mid

        g_coarse_mgal = self.g_mid(coarse_pred.squeeze(1), z_obs_mid_m, aspect)
        gn_mid = F.avg_pool2d(gn, 2)
        valid_mid = F.avg_pool2d(valid_mask, 2)
        gn_bp_mid = self.bp_mid(gn_mid)
        g_hat_bp_mid = self.bp_mid(g_coarse_mgal / s)
        r_bp_mid = (gn_bp_mid - g_hat_bp_mid) * valid_mid

        d1_mid = (self.g_mid.adjoint(valid_mid, z_obs_mid_m, aspect) * mask_mid.squeeze(1)).clamp_min(0.15)
        d1_mid_safe = d1_mid + (1.0 - mask_mid.squeeze(1))
        bp_mid_norm = ((self.g_mid.adjoint(r_bp_mid, z_obs_mid_m, aspect) * mask_mid.squeeze(1)) / d1_mid_safe).unsqueeze(1)

        res_mid_in = torch.cat([gn_bp_mid, g_hat_bp_mid, r_bp_mid, valid_mid], dim=1)
        f_mid_2d = self.res_mid_2d(res_mid_in).unsqueeze(2)
        f_mid_3d = (
            self.res_mid_ds(d_surf_mid.reshape(b, 1, nz_mid * nx_mid, ny_mid))
            + self.res_mid_bp(bp_mid_norm.reshape(b, 1, nz_mid * nx_mid, ny_mid))
        ).reshape(b, self.ch_mid, nz_mid, nx_mid, ny_mid)
        u = u + torch.tanh(self.res_mid_gate) * (f_mid_2d + f_mid_3d) * mask_mid
        u_mid = self.dec_mid2(u)
        if getattr(self, "dec_depth", 2) > 2:
            for blk in self.extra_dec_mid:
                u_mid = blk(u_mid)

        # Stage 2 -> (nz, nx, ny) + Full-Res Residual Physics Feedback on rho_init
        u2 = self.up_vox(u_mid)
        d_surf = F.relu(self.z_top - terr.unsqueeze(2))
        hv = u2 + self.skip_vox(f0, clr_s, d_surf, self.z_top) + self.scale_vox(se).view(b, self.ch_vox, 1, 1, 1)
        hv = self.dec_vox1(hv)

        mask = self._air_mask(self.z_top, terr.unsqueeze(2))
        p_head_init = self.proto_head_init(p_ctx)
        rho_init, _, _, _, _, _ = self._decode_density(
            hv, p_head_init, mask, d_surf=None, rock_head=self.rock_head_init, res_head=self.res_head_init,
        )

        g_init_mgal = self.g_64(rho_init.squeeze(1), z_obs_m, aspect)
        gn_bp = self.bp_64(gn)
        g_init_bp = self.bp_64(g_init_mgal / s)
        r_bp_64 = (gn_bp - g_init_bp) * valid_mask

        d1_64 = (self.g_64.sensitivity_norm(valid_mask, z_obs_m, aspect) * mask.squeeze(1)).clamp_min(0.15)
        d1_64_safe = d1_64 + (1.0 - mask.squeeze(1))
        bp_64_norm = ((self.g_64.adjoint(r_bp_64, z_obs_m, aspect) * mask.squeeze(1)) / d1_64_safe).unsqueeze(1)
        w_bp_z = 1.0 + F.relu(self.bp_z_beta) * d_surf * (1.0 + 2.0 * d_surf)
        bp_64_z = bp_64_norm * w_bp_z

        res_vox_in = torch.cat([gn_bp, g_init_bp, r_bp_64, valid_mask], dim=1)
        f_vox_2d = self.res_vox_2d(res_vox_in).unsqueeze(2)
        f_vox_3d = self.res_vox_3d(
            torch.cat([bp_64_z, rho_init], dim=1).reshape(b, 2, self.nz * self.nx, self.ny)
        ).reshape(b, self.ch_vox, self.nz, self.nx, self.ny)
        hv = hv + torch.tanh(self.res_vox_gate) * (f_vox_2d + f_vox_3d) * mask

        hv = self.dec_vox2(hv)
        if getattr(self, "dec_depth", 2) > 2:
            for blk in self.extra_dec_vox:
                hv = blk(hv)
        hv = self.vox_drop(hv)

        p_head = self.proto_head(p_ctx)
        pred, probs, logits, occ, probs_4d, hv_4d = self._decode_density(
            hv, p_head, mask, d_surf=d_surf, rock_head=self.rock_head, res_head=self.res_head,
        )
        edt = torch.sigmoid(self.edt_head(hv_4d)).reshape(b, 1, self.nz, self.nx, self.ny) * mask

        # Full-resolution 7-cue Gaussian log_var head
        r_bp_energy = (r_bp_64 * r_bp_64) / (0.04 + r_bp_64 * r_bp_64)
        cues_2d = torch.cat([clr_s, 1.0 - valid_mask, r_bp_energy], dim=1)
        log_var = _compute_v6_logvar(
            hv_4d, pred, probs_4d, mask, d_surf, bp_64_norm, cues_2d,
            self.logvar_hv, self.logvar_3d, self.logvar_2d, self.logvar_out,
        )

        if not return_aux:
            return pred

        denom_m = valid_mid.sum(dim=(2, 3), keepdim=True).clamp_min(1.0)
        denom_f = valid_mask.sum(dim=(2, 3), keepdim=True).clamp_min(1.0)
        coarse_res_rms = torch.sqrt(((r_bp_mid * r_bp_mid).sum(dim=(2, 3), keepdim=True) / denom_m) + 1e-8)
        res_rms = torch.sqrt(((r_bp_64 * r_bp_64).sum(dim=(2, 3), keepdim=True) / denom_f) + 1e-8)
        return pred, {
            "is_v6": True,
            "log_var": log_var,
            "coarse_pred": coarse_pred,
            "rho_init": rho_init,
            "mask": mask,
            "occ": occ,
            "probs": probs,
            "logits": logits,
            "edt": edt,
            "res_rms": res_rms,
            "coarse_res_rms": coarse_res_rms,
            "terr": terr.unsqueeze(2),
            "clr": clearance.unsqueeze(2),
            "aspect": aspect.unsqueeze(2),
            "g_init": g_init_mgal,
        }


class PotentialFieldTransformer3DV7(nn.Module):
    """V7 Depth-Progressive Stratified Multi-Resolution 3D Gravity Inversion Network.

    Implements the confirmed V7 Option A + Option I + [P2] specification:
      * 6-channel input: ``[gz, terr_norm, valid_mask, log_aspect, clearance, detrend_mode]``
        (automatically pads ch 5 with ``1.0`` when fed legacy 5-channel inputs).
      * Option A internal 3-tier slab hierarchy (``41,984`` voxels on ``64x64x32``):
        Tier 0 ``(d0, H, W)`` @ 50 m, Tier 1 ``(d1, H/2, W/2)`` @ 100x100x75 m,
        Tier 2 ``(d2, H/4, W/4)`` @ 200x200x150 m.
      * Tier-native ``OddPyramidGravity`` physics feedback, ``CoreMLSepConv25DBlock`` refinement
        with cross-tier skip bridges + boundary-coupled seams, and ``TierProlongation3D``
        (Option I) returning standard ``(B, 1, nz, nx, ny)`` density and ``log_var`` volumes.
      * Wider Stage-2 channels (``slim: 24, base: 32, heavy: 40``) and V6/V6.1 checkpoint migration.
    """

    def __init__(self, nx=64, ny=64, nz=32, patch=4, embed_dim=128, num_heads=4, depth=6,
                 ch_mid=40, ch_vox=32, n_sub=3, num_classes=7, dropout=0.05, dec_depth=2,
                 in_channels=6, **_unused):
        super().__init__()
        assert patch == 4 and nx % (4 * patch) == 0 and ny % (4 * patch) == 0 and nz % 4 == 0
        self.variant = "v7"
        self.in_channels = int(in_channels)
        self.nx, self.ny, self.nz, self.p, self.n_sub = nx, ny, nz, patch, n_sub
        self.num_classes = num_classes
        self.hp, self.wp, self.dp = nx // patch, ny // patch, nz // 4
        self.ch_mid, self.ch_vox = ch_mid, ch_vox
        self.dropout_rate = float(dropout)
        self.dec_depth = max(2, int(dec_depth))

        # Option A 3-tier slab dimensions
        self.d0 = max(1, nz // 4)
        self.n1_fine = (nz - self.d0) // 2
        self.n2_fine = nz - self.d0 - self.n1_fine
        self.d1 = max(1, nz // 4)
        self.d2 = max(1, nz // 8)
        self.tier_depths = (self.d0, self.d1, self.d2)
        self.tier_fine_depths = (self.d0, self.n1_fine, self.n2_fine)
        self.tier_hw = ((nx, ny), (nx // 2, ny // 2), (nx // 4, ny // 4))

        c = embed_dim
        c0, c1 = max(24, c // 4), max(48, c // 2)

        # 9-dim algebraic scale/geometry + detrend-mode conditioning MLP
        self.scale_mlp = nn.Sequential(nn.Linear(9, c), nn.GELU(), nn.Linear(c, c))

        # 2D U-Net encoder (19-ch V6 stem + 1 detrend-mode channel = 20)
        self.enc0 = nn.Sequential(
            nn.Conv2d(20, c0, 3, padding=1), nn.GELU(),
            nn.Conv2d(c0, c0, 3, padding=1), nn.GELU(),
        )
        self.enc1 = nn.Sequential(
            nn.Conv2d(c0, c1, 3, stride=2, padding=1), nn.GELU(),
            nn.Conv2d(c1, c1, 3, padding=1), nn.GELU(),
        )
        self.enc2 = nn.Sequential(
            nn.Conv2d(c1, c, 3, stride=2, padding=1), nn.GELU(),
            nn.Conv2d(c, c, 3, padding=1),
        )
        self.pos2d = nn.Parameter(torch.randn(1, self.hp * self.wp, c) * 0.02)
        self.n2d_1 = nn.LayerNorm(c)
        self.attn2d = Attention(c, num_heads, dropout=dropout)
        self.n2d_2 = nn.LayerNorm(c)
        self.mlp2d = nn.Sequential(nn.Linear(c, 3 * c), nn.GELU(), nn.Linear(3 * c, c))

        # Depth lift + non-expanding 3D depth conditioning
        self.depth_lift = nn.Sequential(nn.LayerNorm(c), nn.Linear(c, self.dp * c))
        self.pos3d = nn.Parameter(torch.randn(1, self.dp, self.hp, self.wp, c) * 0.02)
        self.dc_2d = nn.Conv2d(2, c, 1)
        self.dc_ds = nn.Conv2d(1, c, 1, bias=False)
        self.dc_z = nn.Conv2d(1, c, 1, bias=False)
        self.dc_sens = nn.Conv2d(1, c, 1, bias=False)
        self.dc_out = nn.Sequential(nn.GELU(), nn.Conv2d(c, c, 1))

        # 3D CoreML-safe Transformer backbone (alternating Swin + AxialGlobal with GreensVerticalBias)
        self.blocks = nn.ModuleList([
            (
                CoreMLAxialGlobalBlock3D(c, num_heads, dropout=dropout, dp=self.dp, use_greens_bias=True)
                if i % 2
                else CoreMLSwinBlock3D(c, num_heads, wh=4, ww=4, shift=(2 if (i // 2) % 2 else 0), dropout=dropout)
            )
            for i in range(depth)
        ])

        # Lithology prototype cross-attention (7 classes x n_sub)
        n_proto = num_classes * n_sub
        self.proto_emb = nn.Parameter(torch.randn(1, n_proto, c) * 0.02)
        self.proto_scale = nn.Linear(c, c)
        self.proto_nq = nn.LayerNorm(c)
        self.proto_nk = nn.LayerNorm(c)
        self.proto_attn = Attention(c, num_heads, dropout=dropout)
        self.proto_mlp_n = nn.LayerNorm(c)
        self.proto_mlp = nn.Sequential(nn.Linear(c, 3 * c), nn.GELU(), nn.Linear(3 * c, c))
        self.s2p_nq = nn.LayerNorm(c)
        self.s2p_nk = nn.LayerNorm(c)
        self.s2p_attn = Attention(c, num_heads, dropout=dropout)
        self.s2p_mlp_n = nn.LayerNorm(c)
        self.s2p_mlp = nn.Sequential(nn.Linear(c, 3 * c), nn.GELU(), nn.Linear(3 * c, c))
        self.norm3d = nn.LayerNorm(c)

        # Stage 1 -> (nz/2, nx/2, ny/2) + Exact Pyramid Gravity & P_BP Adjoint Feedback
        self.proj_mid = nn.Conv2d(c, ch_mid, 1)
        self.up_mid = CoreMLSepSubpixelUp3D(ch_mid, ch_mid)
        self.skip_mid = CoreMLDepthConditionedSkip5(c1, ch_mid)
        self.dec_mid1 = CoreMLSepConv25DBlock(ch_mid)
        self.coarse_rho_head = nn.Conv2d(ch_mid, 1, 1)
        with torch.no_grad():
            self.coarse_rho_head.weight.mul_(0.05)
            self.coarse_rho_head.bias.fill_(math.log(-RHO_AIR / RHO_MAX_V6))

        nz_mid, nx_mid, ny_mid = nz // 2, nx // 2, ny // 2
        self.g_mid = OddPyramidGravity(nx_mid, ny_mid, nz_mid, cell_xy=2.0 * REF_CELL, dz_ref=2.0 * REF_CELL)
        self.bp_mid = BorderProjectorConv(nx_mid, ny_mid)
        for p in self.g_mid.parameters():
            p.requires_grad_(False)
        for p in self.bp_mid.parameters():
            p.requires_grad_(False)

        self.res_mid_2d = nn.Sequential(
            nn.Conv2d(5, ch_mid, 3, padding=1), nn.GELU(),
            nn.Conv2d(ch_mid, ch_mid, 1),
        )
        self.res_mid_ds = nn.Conv2d(1, ch_mid, 1)
        self.res_mid_bp = nn.Conv2d(1, ch_mid, 1)
        self.res_mid_gate = nn.Parameter(torch.zeros(1))
        self.dec_mid2 = CoreMLSepConv25DBlock(ch_mid)
        if self.dec_depth > 2:
            self.extra_dec_mid = nn.ModuleList([CoreMLSepConv25DBlock(ch_mid) for _ in range(self.dec_depth - 2)])

        # Stage 2 Option A 3-Tier Slab Hierarchy (41,984 internal voxels on 64x64x32)
        self.up_vox = CoreMLSepSubpixelUp3D(ch_mid, ch_vox)
        self.proj_vox_t1 = nn.Conv2d(ch_mid, ch_vox, 1)
        self.proj_vox_t2 = nn.Conv2d(ch_mid, ch_vox, 1)
        self.skip_vox = CoreMLDepthConditionedSkip5(c0, ch_vox)
        self.skip_vox_t1 = CoreMLDepthConditionedSkip5(c1, ch_vox)
        self.skip_vox_t2 = CoreMLDepthConditionedSkip5(c, ch_vox)
        self.scale_vox = nn.Linear(c, ch_vox)

        # Cross-tier skip bridges (Tier 0 -> Tier 1 -> Tier 2) for continuous dipping faults
        self.bridge_0to1 = nn.Conv2d(2 * ch_vox, ch_vox, 3, stride=2, padding=1)
        self.bridge_1to2 = nn.Conv2d(2 * ch_vox, ch_vox, 3, stride=2, padding=1)
        self.bridge_skip_0to1 = CoreMLDepthConditionedSkip5(ch_vox, ch_vox)
        self.bridge_skip_1to2 = CoreMLDepthConditionedSkip5(ch_vox, ch_vox)

        self.dec_vox1_tiers = nn.ModuleList([CoreMLSepConv25DBlock(ch_vox) for _ in range(3)])
        self.dec_vox2_tiers = nn.ModuleList([CoreMLSepConv25DBlock(ch_vox) for _ in range(3)])
        if self.dec_depth > 2:
            self.extra_dec_vox_tiers = nn.ModuleList([
                nn.ModuleList([CoreMLSepConv25DBlock(ch_vox) for _ in range(self.dec_depth - 2)])
                for _ in range(3)
            ])

        # Tier-native OddPyramidGravity operators & Stage-2 physics feedback
        dz1_ref = (float(self.n1_fine) / float(self.d1)) * REF_CELL
        dz2_ref = (float(self.n2_fine) / float(self.d2)) * REF_CELL
        self.g_tiers = nn.ModuleList([
            OddPyramidGravity(nx, ny, self.d0, cell_xy=REF_CELL, dz_ref=REF_CELL, z0_ref=0.0),
            OddPyramidGravity(nx // 2, ny // 2, self.d1, cell_xy=2.0 * REF_CELL, dz_ref=dz1_ref,
                              z0_ref=float(self.d0) * REF_CELL),
            OddPyramidGravity(nx // 4, ny // 4, self.d2, cell_xy=4.0 * REF_CELL, dz_ref=dz2_ref,
                              z0_ref=float(self.d0 + self.n1_fine) * REF_CELL),
        ])
        self.bp_64 = BorderProjectorConv(nx, ny)
        for p in self.g_tiers.parameters():
            p.requires_grad_(False)
        for p in self.bp_64.parameters():
            p.requires_grad_(False)

        self.res_vox_2d = nn.Sequential(
            nn.Conv2d(5, ch_vox, 3, padding=1), nn.GELU(),
            nn.Conv2d(ch_vox, ch_vox, 1),
        )
        self.res_vox_3d_tiers = nn.ModuleList([nn.Conv2d(2, ch_vox, 1) for _ in range(3)])
        self.res_vox_gate = nn.Parameter(torch.zeros(1))
        self.bp_z_beta = nn.Parameter(torch.tensor(0.85))
        self.vox_drop = nn.Dropout3d(p=dropout) if dropout > 0 else nn.Identity()

        # Decoupled Stage-2 initial and final density heads (shared across the 3 depth tiers)
        self.proto_head_init = nn.Linear(c, ch_vox)
        self.rock_head_init = nn.Conv2d(ch_vox, len(_ROCK_BOUNDS_V6), 1)
        self.res_head_init = nn.Conv2d(ch_vox, 1, 1)
        self.proto_head = nn.Linear(c, ch_vox)
        self.logit_log_scale = nn.Parameter(torch.zeros(1))
        self.cls_log_sharp = nn.Parameter(torch.zeros(1))
        self.rock_head = nn.Conv2d(ch_vox, len(_ROCK_BOUNDS_V6), 1)
        self.res_head = nn.Conv2d(ch_vox, 1, 1)
        self.cls_depth_prior = nn.Conv2d(1, num_classes, 1)
        with torch.no_grad():
            self.res_head_init.weight.mul_(0.25)
            self.res_head_init.bias.zero_()
            self.res_head.weight.mul_(0.25)
            self.res_head.bias.zero_()
            self.cls_depth_prior.weight.zero_()
            self.cls_depth_prior.bias.zero_()
        self.edt_head = nn.Conv2d(ch_vox, 1, 1)

        # Tier-native Gaussian log_var head (8 cues incl. detrend mode)
        lv_ch = max(12, ch_vox)
        self.logvar_hv = nn.Conv2d(ch_vox, lv_ch, 1)
        self.logvar_3d = nn.Conv2d(4, lv_ch, 1, bias=False)
        self.logvar_2d = nn.Conv2d(4, lv_ch, 1, bias=False)  # [clr_smooth, 1-valid, r_bp_energy, detrend_mode]
        self.logvar_out = nn.Conv2d(lv_ch, 1, 1)
        with torch.no_grad():
            self.logvar_out.bias.fill_(-4.0)

        # Option I Prolongation Tail -> standard (B, 1, nz, nx, ny)
        self.prolong = TierProlongation3D(nx, ny, nz, self.d0, self.n1_fine, self.n2_fine)

        # Geometry & class density buffers
        z_t0 = torch.arange(self.d0, dtype=torch.float32) / float(nz)
        z_t1 = (self.d0 + torch.arange(self.d1, dtype=torch.float32) * (float(self.n1_fine) / float(self.d1))) / float(nz)
        z_t2 = (
            self.d0 + self.n1_fine + torch.arange(self.d2, dtype=torch.float32) * (float(self.n2_fine) / float(self.d2))
        ) / float(nz)
        z_rel_t1 = (z_t1 - float(self.d0) / float(nz)) / max(float(self.n1_fine) / float(nz), 1e-4)
        z_rel_t2 = (z_t2 - float(self.d0 + self.n1_fine) / float(nz)) / max(float(self.n2_fine) / float(nz), 1e-4)

        self.register_buffer("z_top_coarse", (torch.arange(nz // 4, dtype=torch.float32) / (nz // 4)).view(1, 1, nz // 4, 1, 1), persistent=False)
        self.register_buffer("z_top_mid", (torch.arange(nz // 2, dtype=torch.float32) / (nz // 2)).view(1, 1, nz // 2, 1, 1), persistent=False)
        self.register_buffer("z_top", (torch.arange(nz, dtype=torch.float32) / nz).view(1, 1, nz, 1, 1), persistent=False)
        self.register_buffer("z_top_t0", z_t0.view(1, 1, self.d0, 1, 1), persistent=False)
        self.register_buffer("z_top_t1", z_t1.view(1, 1, self.d1, 1, 1), persistent=False)
        self.register_buffer("z_top_t2", z_t2.view(1, 1, self.d2, 1, 1), persistent=False)
        self.register_buffer("z_rel_t1", z_rel_t1.view(1, 1, self.d1, 1, 1), persistent=False)
        self.register_buffer("z_rel_t2", z_rel_t2.view(1, 1, self.d2, 1, 1), persistent=False)
        self.register_buffer("cav_vals", torch.tensor([RHO_AIR, RHO_WATER], dtype=torch.float32).view(1, 2, 1, 1), persistent=False)
        lo_t = torch.tensor([b[0] for b in _ROCK_BOUNDS_V6], dtype=torch.float32).view(1, len(_ROCK_BOUNDS_V6), 1, 1)
        span_t = torch.tensor([b[1] - b[0] for b in _ROCK_BOUNDS_V6], dtype=torch.float32).view(1, len(_ROCK_BOUNDS_V6), 1, 1)
        self.register_buffer("rock_lo", lo_t, persistent=False)
        self.register_buffer("rock_span", span_t, persistent=False)

    def _unpack_input(self, x: torch.Tensor):
        c_in = x.shape[1]
        gz = x[:, 0:1]
        terr = x[:, 1:2].clamp(0.0, 0.25)
        valid_mask = x[:, 2:3].clamp(0.0, 1.0) if c_in >= 3 else torch.ones_like(gz)
        log_aspect = x[:, 3:4] if c_in >= 4 else torch.zeros_like(gz)
        clearance = x[:, 4:5].clamp(0.0, 8.0) if c_in >= 5 else torch.full_like(gz, 0.35 / REF_CELL)
        detrend_mode = x[:, 5:6].clamp(0.0, 1.0) if c_in >= 6 else torch.ones_like(gz)
        return gz, terr, valid_mask, log_aspect, clearance, detrend_mode

    @staticmethod
    def _air_mask(z_top: torch.Tensor, terr_5d: torch.Tensor) -> torch.Tensor:
        return PotentialFieldTransformer3DV6._air_mask(z_top, terr_5d)

    def _load_from_state_dict(self, state_dict, prefix, local_metadata, strict,
                              missing_keys, unexpected_keys, error_msgs):
        """Auto-migrate v6/v6.1 checkpoints (5->6 ch zero-pad, ch_vox expansion, tiered blocks)."""
        own_sd = self.state_dict()
        enc0_k = f"{prefix}enc0.0.weight"
        is_v6_ckpt = (
            enc0_k in state_dict
            and f"{prefix}blocks.0.n1.weight" in state_dict
            and f"{prefix}prolong.rho_pw.weight" not in state_dict
        )

        # V6.0 -> V6.1 sub-module expansion if migrating an older v6.0 checkpoint
        if f"{prefix}proto_head.weight" in state_dict and f"{prefix}proto_head_init.weight" not in state_dict:
            for src, dst in (
                ("proto_head.weight", "proto_head_init.weight"),
                ("proto_head.bias", "proto_head_init.bias"),
                ("rock_head.weight", "rock_head_init.weight"),
                ("rock_head.bias", "rock_head_init.bias"),
                ("res_head.weight", "res_head_init.weight"),
                ("res_head.bias", "res_head_init.bias"),
            ):
                sk, dk = f"{prefix}{src}", f"{prefix}{dst}"
                if sk in state_dict and dk not in state_dict:
                    state_dict[dk] = state_dict[sk].detach().clone()
            for name, tgt in (
                ("cls_depth_prior.weight", self.cls_depth_prior.weight),
                ("cls_depth_prior.bias", self.cls_depth_prior.bias),
                ("bp_z_beta", self.bp_z_beta),
            ):
                fk = f"{prefix}{name}"
                if fk not in state_dict:
                    state_dict[fk] = torch.zeros_like(tgt)
            for idx, blk in enumerate(self.blocks):
                gb = getattr(blk, "greens_bias", None)
                if gb is not None and f"{prefix}blocks.{idx}.greens_bias.gain" not in state_dict:
                    state_dict[f"{prefix}blocks.{idx}.greens_bias.raw_decay"] = gb.raw_decay.detach().clone()
                    state_dict[f"{prefix}blocks.{idx}.greens_bias.gain"] = torch.zeros_like(gb.gain)
                    state_dict[f"{prefix}blocks.{idx}.greens_bias.asym"] = torch.zeros_like(gb.asym)

        # Map un-tiered V6 Stage-2 refinement modules onto all 3 V7 tiers when absent
        tier_src_maps = [
            ("dec_vox1.", [f"dec_vox1_tiers.{i}." for i in range(3)]),
            ("dec_vox2.", [f"dec_vox2_tiers.{i}." for i in range(3)]),
            ("res_vox_3d.", [f"res_vox_3d_tiers.{i}." for i in range(3)]),
        ]
        if self.dec_depth > 2:
            tier_src_maps.append(("extra_dec_vox.", [f"extra_dec_vox_tiers.{i}." for i in range(3)]))
        for src_p, dst_list in tier_src_maps:
            full_src_p = f"{prefix}{src_p}"
            src_keys = [k for k in list(state_dict.keys()) if k.startswith(full_src_p)]
            if src_keys:
                for sk in src_keys:
                    suffix = sk[len(full_src_p):]
                    for dst_p in dst_list:
                        dk = f"{prefix}{dst_p}{suffix}"
                        if dk not in state_dict and f"{dst_p}{suffix}" in own_sd:
                            state_dict[dk] = state_dict[sk].detach().clone()
                    state_dict.pop(sk, None)

        # Zero-pad newly added ch-5 / head input dimensions; preserve base_init on expanded hidden ch_vox
        zero_pad_heads = (
            "enc0.0.weight", "scale_mlp.0.weight", "res_mid_2d.0.weight",
            "proto_head.weight", "proto_head.bias", "proto_head_init.weight", "proto_head_init.bias",
            "rock_head.weight", "rock_head_init.weight", "res_head.weight", "res_head_init.weight",
            "edt_head.weight", "logvar_hv.weight", "logvar_out.weight",
        )
        ch5_mixed_weights = ("res_vox_2d.0.weight", "logvar_2d.weight")
        for name, base_t in own_sd.items():
            fk = f"{prefix}{name}"
            if fk in state_dict and state_dict[fk].shape != base_t.shape:
                src_t = state_dict[fk]
                if name in ch5_mixed_weights and src_t.ndim == base_t.ndim:
                    out = base_t.detach().clone()
                    min_o = min(src_t.shape[0], base_t.shape[0])
                    min_i = min(src_t.shape[1], base_t.shape[1])
                    out[:min_o, :min_i] = src_t[:min_o, :min_i].to(dtype=out.dtype)
                    if base_t.shape[1] > src_t.shape[1]:
                        out[:, src_t.shape[1]:] = 0.0
                    state_dict[fk] = out
                else:
                    state_dict[fk] = _adapt_state_dict_tensor(
                        src_t, base_t, zero_pad_new=(name in zero_pad_heads),
                    )
            elif is_v6_ckpt and fk not in state_dict:
                state_dict[fk] = base_t.detach().clone()

        super()._load_from_state_dict(
            state_dict, prefix, local_metadata, strict, missing_keys, unexpected_keys, error_msgs
        )

    def _decode_tier_density(self, hv: torch.Tensor, p_head: torch.Tensor, mask: torch.Tensor,
                             d_surf: torch.Tensor | None = None, rock_head=None, res_head=None):
        return _decode_v6_density(
            hv, p_head, mask, self.ch_vox, self.num_classes, self.n_sub,
            self.logit_log_scale, self.cls_log_sharp, self.cls_depth_prior,
            self.rock_head if rock_head is None else rock_head,
            self.res_head if res_head is None else res_head,
            self.rock_lo, self.rock_span, self.cav_vals, d_surf=d_surf,
        )

    def enable_mc_dropout(self, enabled: bool = True):
        """Enable or disable Dropout / Dropout3d layers for MC Dropout uncertainty."""
        for m in self.modules():
            if isinstance(m, (nn.Dropout, nn.Dropout3d)):
                m.train(enabled)

    def forward_uncertainty(self, x: torch.Tensor):
        """Return ``(density, log_var)`` tensors (used by ONNX export & CoreML inference)."""
        pred, log_var, _ = self._forward_impl(x, return_aux=False, return_logvar=True)
        return pred, log_var

    def forward(self, x: torch.Tensor, return_aux: bool = False):
        pred, _, aux = self._forward_impl(x, return_aux=return_aux, return_logvar=return_aux)
        if not return_aux:
            return pred
        return pred, aux

    @staticmethod
    def _last_two_slices(hv: torch.Tensor) -> torch.Tensor:
        b, c, d, h, w = hv.shape
        if d >= 2:
            return hv[:, :, -2:].reshape(b, 2 * c, h, w)
        last = hv[:, :, -1:].reshape(b, c, h, w)
        return torch.cat([last, last], dim=1)

    def _run_seamed_dec2(self, hv_tiers: list[torch.Tensor], blocks_tiers: nn.ModuleList) -> list[torch.Tensor]:
        h0, w0 = self.tier_hw[0]
        h1, w1 = self.tier_hw[1]
        s0_bot = F.avg_pool2d(hv_tiers[0][:, :, -1:].squeeze(2), 2).unsqueeze(2)
        s1_top = F.interpolate(hv_tiers[1][:, :, :1].squeeze(2), size=(h0, w0), mode="bilinear", align_corners=False).unsqueeze(2)
        s1_bot = F.avg_pool2d(hv_tiers[1][:, :, -1:].squeeze(2), 2).unsqueeze(2)
        s2_top = F.interpolate(hv_tiers[2][:, :, :1].squeeze(2), size=(h1, w1), mode="bilinear", align_corners=False).unsqueeze(2)
        out0 = blocks_tiers[0](torch.cat([hv_tiers[0], s1_top], dim=2))[:, :, :self.d0]
        out1 = blocks_tiers[1](torch.cat([s0_bot, hv_tiers[1], s2_top], dim=2))[:, :, 1:1 + self.d1]
        out2 = blocks_tiers[2](torch.cat([s1_bot, hv_tiers[2]], dim=2))[:, :, 1:]
        return [out0, out1, out2]

    def _forward_impl(self, x: torch.Tensor, return_aux: bool = False, return_logvar: bool = True):
        b = x.shape[0]
        gz, terr, valid_mask, log_aspect, clearance, detrend_mode = self._unpack_input(x)
        clr_s = clearance / (clearance + 1.0)
        aspect = torch.exp(log_aspect.mean(dim=(2, 3), keepdim=True)).clamp(0.55, 1.40)

        s = _abs_coreml(gz).amax(dim=(2, 3), keepdim=True).clamp_min(1e-6)
        gn = gz / s
        h_in = torch.cat([
            _compute_v6_stem_19ch(gn, terr, valid_mask, clr_s),
            detrend_mode * 2.0 - 1.0,
        ], dim=1)

        s_flat = s.view(b, 1)
        s_slab = s_flat / SLAB
        terr_span = terr.amax(dim=(2, 3)).view(b, 1) - terr.amin(dim=(2, 3)).view(b, 1)
        s_vec = torch.cat([
            s_slab / (s_slab + 1.0),
            s_slab / (s_slab + 0.15),
            gz.amax(dim=(2, 3)).view(b, 1) / s_flat,
            gz.amin(dim=(2, 3)).view(b, 1) / s_flat,
            terr.mean(dim=(2, 3)).view(b, 1) * 4.0,
            terr_span * 4.0,
            log_aspect.mean(dim=(2, 3)).view(b, 1),
            clr_s.mean(dim=(2, 3)).view(b, 1),
            detrend_mode.mean(dim=(2, 3)).view(b, 1),
        ], dim=1)
        se = self.scale_mlp(s_vec)

        f0 = self.enc0(h_in)
        f1 = self.enc1(f0)
        f2 = self.enc2(f1)

        t2 = f2.flatten(2).transpose(1, 2) + self.pos2d + se.unsqueeze(1)
        t2 = t2 + self.attn2d(self.n2d_1(t2), self.n2d_1(t2))
        t2 = t2 + self.mlp2d(self.n2d_2(t2))

        v = self.depth_lift(t2).view(b, self.hp, self.wp, self.dp, -1).permute(0, 3, 1, 2, 4)

        terr_c = F.avg_pool2d(terr, 4)
        clr_s_c = F.avg_pool2d(clr_s, 4)
        asp_c = F.avg_pool2d(log_aspect, 4)
        d_surf_c = F.relu(self.z_top_coarse - terr_c.unsqueeze(2))
        sens_c = 0.04 / ((d_surf_c + clr_s_c.unsqueeze(2) * 0.16 + 0.12) ** 2)
        dc_f = (
            self.dc_2d(torch.cat([clr_s_c, asp_c], dim=1)).unsqueeze(2)
            + self.dc_ds(d_surf_c.reshape(b, 1, self.dp * self.hp, self.wp)).reshape(b, -1, self.dp, self.hp, self.wp)
            + self.dc_z(self.z_top_coarse.reshape(1, 1, self.dp, 1)).reshape(1, -1, self.dp, 1, 1)
            + self.dc_sens(sens_c.reshape(b, 1, self.dp * self.hp, self.wp)).reshape(b, -1, self.dp, self.hp, self.wp)
        )
        dc_mod = self.dc_out(dc_f.reshape(b, -1, self.dp * self.hp, self.wp)).reshape(
            b, -1, self.dp, self.hp, self.wp
        ).permute(0, 2, 3, 4, 1)
        v = v + self.pos3d + dc_mod + se.view(b, 1, 1, 1, -1)

        for blk in self.blocks:
            v = blk(v)

        vf = v.reshape(b, self.dp * self.hp * self.wp, -1)
        p_ctx = self.proto_emb + self.proto_scale(se).unsqueeze(1)
        vf = vf + self.proto_attn(self.proto_nq(vf), self.proto_nk(p_ctx))
        vf = vf + self.proto_mlp(self.proto_mlp_n(vf))
        p_ctx = p_ctx + self.s2p_attn(self.s2p_nq(p_ctx), self.s2p_nk(vf))
        p_ctx = p_ctx + self.s2p_mlp(self.s2p_mlp_n(p_ctx))
        v = self.norm3d(vf).view(b, self.dp, self.hp, self.wp, -1)

        total_depth_m = self.nz * REF_CELL * aspect
        z_obs_m = terr * total_depth_m - clearance * REF_CELL
        z_obs_mid_m = F.avg_pool2d(z_obs_m, 2)

        # Stage 1 -> (nz/2, nx/2, ny/2) + Exact Pyramid Gravity & P_BP Adjoint Feedback
        vc = self.proj_mid(v.permute(0, 4, 1, 2, 3).reshape(b, -1, self.dp * self.hp, self.wp)).reshape(
            b, -1, self.dp, self.hp, self.wp
        )
        u = self.up_mid(vc)
        terr_mid = F.avg_pool2d(terr, 2)
        clr_s_mid = F.avg_pool2d(clr_s, 2)
        detrend_mid = F.avg_pool2d(detrend_mode, 2)
        d_surf_mid = F.relu(self.z_top_mid - terr_mid.unsqueeze(2))
        u = u + self.skip_mid(f1, clr_s_mid, d_surf_mid, self.z_top_mid)
        u = self.dec_mid1(u)

        nz_mid, nx_mid, ny_mid = self.nz // 2, self.nx // 2, self.ny // 2
        mask_mid = self._air_mask(self.z_top_mid, terr_mid.unsqueeze(2))
        coarse_raw = self.coarse_rho_head(u.reshape(b, self.ch_mid, nz_mid * nx_mid, ny_mid)).reshape(
            b, 1, nz_mid, nx_mid, ny_mid
        )
        coarse_pred = (RHO_AIR + (RHO_MAX_V6 - RHO_AIR) * torch.sigmoid(coarse_raw)) * mask_mid

        g_coarse_mgal = self.g_mid(coarse_pred.squeeze(1), z_obs_mid_m, aspect)
        gn_mid = F.avg_pool2d(gn, 2)
        valid_mid = F.avg_pool2d(valid_mask, 2)
        gn_bp_mid = self.bp_mid(gn_mid)
        g_hat_bp_mid = self.bp_mid(g_coarse_mgal / s)
        r_bp_mid = (gn_bp_mid - g_hat_bp_mid) * valid_mid

        d1_mid = (self.g_mid.adjoint(valid_mid, z_obs_mid_m, aspect) * mask_mid.squeeze(1)).clamp_min(0.15)
        d1_mid_safe = d1_mid + (1.0 - mask_mid.squeeze(1))
        bp_mid_norm = ((self.g_mid.adjoint(r_bp_mid, z_obs_mid_m, aspect) * mask_mid.squeeze(1)) / d1_mid_safe).unsqueeze(1)

        res_mid_in = torch.cat([gn_bp_mid, g_hat_bp_mid, r_bp_mid, valid_mid, detrend_mid], dim=1)
        f_mid_2d = self.res_mid_2d(res_mid_in).unsqueeze(2)
        f_mid_3d = (
            self.res_mid_ds(d_surf_mid.reshape(b, 1, nz_mid * nx_mid, ny_mid))
            + self.res_mid_bp(bp_mid_norm.reshape(b, 1, nz_mid * nx_mid, ny_mid))
        ).reshape(b, self.ch_mid, nz_mid, nx_mid, ny_mid)
        u = u + torch.tanh(self.res_mid_gate) * (f_mid_2d + f_mid_3d) * mask_mid
        u_mid = self.dec_mid2(u)
        if self.dec_depth > 2:
            for blk in self.extra_dec_mid:
                u_mid = blk(u_mid)

        # Stage 2 Option A 3-Tier Pyramid Geometry & Conditioning
        terr_t2 = F.avg_pool2d(terr, 4)
        clr_s_t2 = F.avg_pool2d(clr_s, 4)
        valid_t2 = F.avg_pool2d(valid_mask, 4)
        detrend_t2 = F.avg_pool2d(detrend_mode, 4)
        z_obs_t2_m = F.avg_pool2d(z_obs_m, 4)

        terr_tiers = (terr, terr_mid, terr_t2)
        clr_tiers = (clr_s, clr_s_mid, clr_s_t2)
        valid_tiers = (valid_mask, valid_mid, valid_t2)
        detrend_tiers = (detrend_mode, detrend_mid, detrend_t2)
        z_obs_tiers = (z_obs_m, z_obs_mid_m, z_obs_t2_m)
        z_top_tiers = (self.z_top_t0, self.z_top_t1, self.z_top_t2)
        d_surf_tiers = [F.relu(zt - tr.unsqueeze(2)) for zt, tr in zip(z_top_tiers, terr_tiers)]
        mask_tiers = [self._air_mask(zt, tr.unsqueeze(2)) for zt, tr in zip(z_top_tiers, terr_tiers)]

        se_vox = self.scale_vox(se).view(b, self.ch_vox, 1, 1, 1)

        # Tier 0: (B, ch_vox, d0, H, W)
        if self.d0 % 2 == 0:
            u0 = self.up_vox(u_mid[:, :, :self.d0 // 2])
        else:
            u0 = _trilinear_coreml(self.up_vox(u_mid[:, :, :(self.d0 + 1) // 2]), (self.d0, self.nx, self.ny))
        hv0 = self.dec_vox1_tiers[0](u0 + self.skip_vox(f0, clr_s, d_surf_tiers[0], self.z_top_t0) + se_vox)

        # Tier 1: (B, ch_vox, d1, H/2, W/2) with cross-tier 0 -> 1 bridge
        s1_lo = self.d0 // 2
        s1_hi = max(s1_lo + 1, (self.d0 + self.n1_fine + 1) // 2)
        h1, w1 = self.tier_hw[1]
        u1_src = _trilinear_coreml(u_mid[:, :, s1_lo:s1_hi], (self.d1, h1, w1))
        u1 = self.proj_vox_t1(u1_src.reshape(b, self.ch_mid, self.d1 * h1, w1)).reshape(
            b, self.ch_vox, self.d1, h1, w1
        )
        b0_2d = F.gelu(self.bridge_0to1(self._last_two_slices(hv0)))
        hv1 = self.dec_vox1_tiers[1](
            u1 + self.skip_vox_t1(f1, clr_s_mid, d_surf_tiers[1], self.z_top_t1)
            + self.bridge_skip_0to1(b0_2d, clr_s_mid, d_surf_tiers[1], self.z_rel_t1) + se_vox
        )

        # Tier 2: (B, ch_vox, d2, H/4, W/4) with cross-tier 1 -> 2 bridge
        s2_lo = min(nz_mid - 1, (self.d0 + self.n1_fine) // 2)
        h2, w2 = self.tier_hw[2]
        u2_src = _trilinear_coreml(u_mid[:, :, s2_lo:], (self.d2, h2, w2))
        u2 = self.proj_vox_t2(u2_src.reshape(b, self.ch_mid, self.d2 * h2, w2)).reshape(
            b, self.ch_vox, self.d2, h2, w2
        )
        b1_2d = F.gelu(self.bridge_1to2(self._last_two_slices(hv1)))
        hv2 = self.dec_vox1_tiers[2](
            u2 + self.skip_vox_t2(f2, clr_s_t2, d_surf_tiers[2], self.z_top_t2)
            + self.bridge_skip_1to2(b1_2d, clr_s_t2, d_surf_tiers[2], self.z_rel_t2) + se_vox
        )
        hv_tiers = [hv0, hv1, hv2]

        # Per-tier initial density + Multi-Tier OddPyramidGravity Physics Feedback
        p_head_init = self.proto_head_init(p_ctx)
        rho_init_tiers = [
            self._decode_tier_density(
                hv_tiers[i], p_head_init, mask_tiers[i], d_surf=None,
                rock_head=self.rock_head_init, res_head=self.res_head_init,
            )[0]
            for i in range(3)
        ]
        g0 = self.g_tiers[0](rho_init_tiers[0].squeeze(1), z_obs_tiers[0], aspect)
        g1 = F.interpolate(
            self.g_tiers[1](rho_init_tiers[1].squeeze(1), z_obs_tiers[1], aspect),
            size=(self.nx, self.ny), mode="bilinear", align_corners=False,
        )
        g2 = F.interpolate(
            self.g_tiers[2](rho_init_tiers[2].squeeze(1), z_obs_tiers[2], aspect),
            size=(self.nx, self.ny), mode="bilinear", align_corners=False,
        )
        g_init_mgal = g0 + g1 + g2
        gn_bp = self.bp_64(gn)
        g_init_bp = self.bp_64(g_init_mgal / s)
        r_bp_64 = (gn_bp - g_init_bp) * valid_mask

        f_vox_2d_0 = self.res_vox_2d(torch.cat([gn_bp, g_init_bp, r_bp_64, valid_mask, detrend_mode], dim=1))
        bp_norm_tiers = []
        for i in range(3):
            d_i = self.tier_depths[i]
            h_i, w_i = self.tier_hw[i]
            r_bp_i = r_bp_64 if i == 0 else F.avg_pool2d(r_bp_64, 2 ** i)
            d1_i = (
                self.g_tiers[i].sensitivity_norm(valid_tiers[i], z_obs_tiers[i], aspect) * mask_tiers[i].squeeze(1)
            ).clamp_min(0.10)
            d1_i_safe = d1_i + (1.0 - mask_tiers[i].squeeze(1))
            bp_i_norm = (
                (self.g_tiers[i].adjoint(r_bp_i, z_obs_tiers[i], aspect) * mask_tiers[i].squeeze(1)) / d1_i_safe
            ).unsqueeze(1)
            bp_norm_tiers.append(bp_i_norm)
            w_bp_z_i = 1.0 + F.relu(self.bp_z_beta) * d_surf_tiers[i] * (1.0 + 2.0 * d_surf_tiers[i])
            bp_i_z = bp_i_norm * w_bp_z_i
            f_2d_i = (f_vox_2d_0 if i == 0 else F.avg_pool2d(f_vox_2d_0, 2 ** i)).unsqueeze(2)
            f_3d_i = self.res_vox_3d_tiers[i](
                torch.cat([bp_i_z, rho_init_tiers[i]], dim=1).reshape(b, 2, d_i * h_i, w_i)
            ).reshape(b, self.ch_vox, d_i, h_i, w_i)
            hv_tiers[i] = hv_tiers[i] + torch.tanh(self.res_vox_gate) * (f_2d_i + f_3d_i) * mask_tiers[i]

        # Seamed Stage-2 3D refinement (vertical boundary sharing across z=400m & z=1000m)
        hv_tiers = self._run_seamed_dec2(hv_tiers, self.dec_vox2_tiers)
        if self.dec_depth > 2:
            for extra_idx in range(self.dec_depth - 2):
                step_blocks = nn.ModuleList([self.extra_dec_vox_tiers[t][extra_idx] for t in range(3)])
                hv_tiers = self._run_seamed_dec2(hv_tiers, step_blocks)
        hv_tiers = [self.vox_drop(hv_i) for hv_i in hv_tiers]

        # Per-tier density + optional log_var decoding & Option I prolongation
        p_head = self.proto_head(p_ctx)
        dec_tiers = [
            self._decode_tier_density(
                hv_tiers[i], p_head, mask_tiers[i], d_surf=d_surf_tiers[i],
                rock_head=self.rock_head, res_head=self.res_head,
            )
            for i in range(3)
        ]
        rho_tiers = tuple(dt[0] for dt in dec_tiers)
        mask = self._air_mask(self.z_top, terr.unsqueeze(2))

        if not return_logvar and not return_aux:
            pred, _ = self.prolong(rho_tiers, None, mask=mask)
            return pred, None, None

        r_bp_energy = (r_bp_64 * r_bp_64) / (0.04 + r_bp_64 * r_bp_64)
        lv_tiers = tuple(
            _compute_v6_logvar(
                dec_tiers[i][5], rho_tiers[i], dec_tiers[i][4], mask_tiers[i],
                d_surf_tiers[i], bp_norm_tiers[i],
                torch.cat([
                    clr_tiers[i], 1.0 - valid_tiers[i],
                    r_bp_energy if i == 0 else F.avg_pool2d(r_bp_energy, 2 ** i),
                    detrend_tiers[i],
                ], dim=1),
                self.logvar_hv, self.logvar_3d, self.logvar_2d, self.logvar_out,
            )
            for i in range(3)
        )
        pred, log_var = self.prolong(rho_tiers, lv_tiers, mask=mask)
        if not return_aux:
            return pred, log_var, None

        # Auxiliary full-resolution volumes for CompoundInversionLoss
        rho_init = self.prolong.prolong_tensor(tuple(rho_init_tiers), mask=mask)
        logits = self.prolong.prolong_tensor(tuple(dt[2] for dt in dec_tiers))
        probs = F.softmax(
            logits.permute(0, 2, 1, 3, 4).reshape(b * self.nz, self.num_classes, self.nx, self.ny), dim=1,
        ).reshape(b, self.nz, self.num_classes, self.nx, self.ny).permute(0, 2, 1, 3, 4)
        occ = self.prolong.prolong_tensor(tuple(dt[3] for dt in dec_tiers))
        edt_tiers = tuple(
            torch.sigmoid(self.edt_head(dec_tiers[i][5])).reshape(
                b, 1, self.tier_depths[i], self.tier_hw[i][0], self.tier_hw[i][1]
            ) * mask_tiers[i]
            for i in range(3)
        )
        edt = self.prolong.prolong_tensor(edt_tiers, mask=mask)

        denom_m = valid_mid.sum(dim=(2, 3), keepdim=True).clamp_min(1.0)
        denom_f = valid_mask.sum(dim=(2, 3), keepdim=True).clamp_min(1.0)
        coarse_res_rms = torch.sqrt(((r_bp_mid * r_bp_mid).sum(dim=(2, 3), keepdim=True) / denom_m) + 1e-8)
        res_rms = torch.sqrt(((r_bp_64 * r_bp_64).sum(dim=(2, 3), keepdim=True) / denom_f) + 1e-8)
        return pred, log_var, {
            "is_v7": True,
            "tier_preds": rho_tiers,
            "log_var": log_var,
            "coarse_pred": coarse_pred,
            "rho_init": rho_init,
            "mask": mask,
            "occ": occ,
            "probs": probs,
            "logits": logits,
            "edt": edt,
            "res_rms": res_rms,
            "coarse_res_rms": coarse_res_rms,
            "terr": terr.unsqueeze(2),
            "clr": clearance.unsqueeze(2),
            "aspect": aspect.unsqueeze(2),
            "g_init": g_init_mgal,
        }


def create_model(nx=64, ny=64, nz=32, **arch):
    """Instantiate v7, v6, v5, v4, v3, v2, or legacy v1 from checkpoint ``arch``."""
    cfg = dict(arch)
    variant = cfg.pop("variant", "v1")
    presets_and_cls = {
        "v7": (V7_SIZE_PRESETS, PotentialFieldTransformer3DV7),
        "v6": (V6_SIZE_PRESETS, PotentialFieldTransformer3DV6),
        "v5": (V5_SIZE_PRESETS, PotentialFieldTransformer3DV5),
    }
    if variant in presets_and_cls:
        presets, cls = presets_and_cls[variant]
        model_size = cfg.pop("model_size", None)
        if model_size in presets:
            merged = dict(presets[model_size])
            merged.pop("variant", None)
            merged.pop("model_size", None)
            merged.update(cfg)
            cfg = merged
        return cls(nx, ny, nz, **cfg)
    if variant == "v4":
        return PotentialFieldTransformer3DV4(nx, ny, nz, **cfg)
    if variant == "v3":
        return PotentialFieldTransformer3DV3(nx, ny, nz, **cfg)
    if variant == "v2":
        return PotentialFieldTransformer3D(nx, ny, nz, **cfg)
    return PotentialFieldTransformer3DV1(nx, ny, nz, **cfg)
