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


class Attention(nn.Module):
    def __init__(self, dim, heads):
        super().__init__()
        self.h = heads
        self.q = nn.Linear(dim, dim)
        self.kv = nn.Linear(dim, 2 * dim)
        self.o = nn.Linear(dim, dim)

    def forward(self, x, ctx):
        b, n, c = x.shape
        m = ctx.shape[1]
        q = self.q(x).view(b, n, self.h, c // self.h).transpose(1, 2)
        k, v = self.kv(ctx).view(b, m, 2, self.h, c // self.h).permute(2, 0, 3, 1, 4)
        out = F.scaled_dot_product_attention(q, k, v)
        return self.o(out.transpose(1, 2).reshape(b, n, c))


class SwinBlock3D(nn.Module):
    """3D Swin Transformer block: full-depth x (wh, ww) window attention with shifted windows."""

    def __init__(self, dim=96, heads=4, wh=4, ww=4, shift=0):
        super().__init__()
        self.wh, self.ww, self.shift = wh, ww, shift
        self.n1 = nn.LayerNorm(dim)
        self.attn = Attention(dim, heads)
        self.n2 = nn.LayerNorm(dim)
        self.mlp = nn.Sequential(nn.Linear(dim, 3 * dim), nn.GELU(), nn.Linear(3 * dim, dim))

    def forward(self, v):
        b, dp, hp, wp, c = v.shape
        h = self.n1(v)
        if self.shift > 0:
            h = torch.roll(h, shifts=(-self.shift, -self.shift), dims=(2, 3))
        wh, ww = self.wh, self.ww
        hw = (h.view(b, dp, hp // wh, wh, wp // ww, ww, c)
              .permute(0, 2, 4, 1, 3, 5, 6)
              .reshape(-1, dp * wh * ww, c))
        hw = self.attn(hw, hw)
        h = (hw.view(b, hp // wh, wp // ww, dp, wh, ww, c)
             .permute(0, 3, 1, 4, 2, 5, 6)
             .reshape(b, dp, hp, wp, c))
        if self.shift > 0:
            h = torch.roll(h, shifts=(self.shift, self.shift), dims=(2, 3))
        v = v + h
        return v + self.mlp(self.n2(v))


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


def create_model(nx=64, ny=64, nz=32, **arch):
    """Instantiate v2 (when ``variant == 'v2'``) or legacy v1 from checkpoint ``arch`` metadata."""
    cfg = dict(arch)
    variant = cfg.pop("variant", "v1")
    if variant == "v2":
        return PotentialFieldTransformer3D(nx, ny, nz, **cfg)
    return PotentialFieldTransformer3DV1(nx, ny, nz, **cfg)
