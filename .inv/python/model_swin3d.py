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
        c0, c1 = max(32, c // 4), max(64, c // 2)
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
        c0 = max(32, c // 4)
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
         both Stage-0 and new Stage-1 (``16x32x32``) 3D attention blocks, plus optional ``Dropout3d``.
      4. Physically calibrated ``17x17`` Nagy-prism coarse forward residual loop at Stage 1.
      5. Sharper foreground occupancy gating (``0.25/0.75``) and bounded voxel-wise aleatoric
         ``log_var`` head (``[-8.0, 4.0]``, masked by terrain).
    """

    def __init__(self, nx=64, ny=64, nz=32, patch=4, embed_dim=128, num_heads=4, depth=6,
                 ch_mid=40, ch_vox=24, n_sub=3, num_classes=7, dropout=0.05):
        super().__init__(
            nx=nx, ny=ny, nz=nz, patch=patch, embed_dim=embed_dim, num_heads=num_heads,
            depth=depth, ch_mid=ch_mid, ch_vox=ch_vox, n_sub=n_sub, num_classes=num_classes,
        )
        self.dropout_rate = float(dropout)
        c = embed_dim
        c0 = max(32, c // 4)
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
        self.coarse_fwd_log_scale = nn.Parameter(torch.zeros(1))

        self.res_feedback_2d = nn.Sequential(
            nn.Conv2d(4, ch_mid, 3, padding=1), nn.GELU(),
            nn.Conv2d(ch_mid, ch_mid, 1),
        )
        self.res_depth_gate = nn.Sequential(
            nn.Conv2d(3, ch_mid, 1),
            nn.Sigmoid(),
        )
        mid_heads = num_heads if ch_mid % num_heads == 0 else (2 if ch_mid % 2 == 0 else 1)
        self.mid_swin = SwinBlock3D(
            ch_mid, mid_heads, wh=4, ww=4, shift=2,
            use_greens_bias=True, dp=nz_mid, dropout=dropout,
        )

        self.vox_drop = nn.Dropout3d(p=dropout) if dropout > 0 else nn.Identity()
        self.logvar_refine = SepConv25DBlock(ch_vox)
        self.logvar_head = nn.Conv2d(ch_vox, 1, 1)
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
        mask_mid = (self.z_top_mid >= terr_mid.unsqueeze(2)).to(u.dtype)
        coarse_raw = self.coarse_rho_head(u.reshape(b, self.ch_mid, nz_mid * nx_mid, ny_mid)).reshape(
            b, 1, nz_mid, nx_mid, ny_mid
        )
        coarse_pred = (RHO_AIR + (RHO_ORE_MAX - RHO_AIR) * torch.sigmoid(coarse_raw)) * mask_mid

        aspect_b = log_aspect.mean(dim=(2, 3), keepdim=True).exp().clamp(0.25, 4.0)
        clear_b = clearance.mean(dim=(2, 3), keepdim=True).clamp(0.0, 4.0)
        atten = 1.0 / (1.0 + 0.35 * clear_b)
        g_coarse_mgal = self.coarse_fwd_conv(coarse_pred.squeeze(1))
        g_hat_mid = (g_coarse_mgal * aspect_b * atten / s) * self.coarse_fwd_log_scale.exp().clamp(0.25, 4.0)
        gn_mid = F.avg_pool2d(gn, 2)
        valid_mid = F.avg_pool2d(valid_mask, 2)
        g_res_mid = gn_mid - g_hat_mid
        res_2d = self.res_feedback_2d(torch.cat([gn_mid, g_hat_mid, g_res_mid, valid_mid], dim=1)).unsqueeze(2)
        z_m = self.z_top_mid.expand(b, 1, nz_mid, nx_mid, ny_mid)
        clear_m = F.avg_pool2d(clearance, 2).unsqueeze(2).expand(b, 1, nz_mid, nx_mid, ny_mid)
        d_gate = self.res_depth_gate(
            torch.cat([d_surf_mid, z_m, clear_m], dim=1).reshape(b, 3, nz_mid * nx_mid, ny_mid)
        ).reshape(b, self.ch_mid, nz_mid, nx_mid, ny_mid)
        u = u + self.res_gate.tanh() * (res_2d * d_gate * mask_mid)
        u = self.mid_swin(u.permute(0, 2, 3, 4, 1)).permute(0, 4, 1, 2, 3)
        u = self.dec_mid2(u)

        # Stage 2 -> (nz, nx, ny)
        u = self.up_vox(u)
        d_surf = (self.z_top - terr.unsqueeze(2)).clamp_min(0.0)
        hv = u + self.skip_vox(f0, d_surf, self.z_top) + self.scale_vox(se).view(b, self.ch_vox, 1, 1, 1)
        hv = self.vox_drop(self.dec_vox2(self.dec_vox1(hv)))

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
            lv_feat = self.logvar_refine(hv).reshape(b, self.ch_vox, self.nz * self.nx, self.ny)
            log_var = self.logvar_head(lv_feat).reshape(b, 1, self.nz, self.nx, self.ny).clamp(-8.0, 4.0) * mask
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


def create_model(nx=64, ny=64, nz=32, **arch):
    """Instantiate v5, v4, v3, v2, or legacy v1 from checkpoint ``arch``."""
    cfg = dict(arch)
    variant = cfg.pop("variant", "v1")
    if variant == "v5":
        return PotentialFieldTransformer3DV5(nx, ny, nz, **cfg)
    if variant == "v4":
        return PotentialFieldTransformer3DV4(nx, ny, nz, **cfg)
    if variant == "v3":
        return PotentialFieldTransformer3DV3(nx, ny, nz, **cfg)
    if variant == "v2":
        return PotentialFieldTransformer3D(nx, ny, nz, **cfg)
    return PotentialFieldTransformer3DV1(nx, ny, nz, **cfg)
