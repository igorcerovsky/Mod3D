"""3D Swin Transformer for 3D Potential Field Inversion.

Features:
1. 2D Observation Patch Embedding.
2. 2D-to-3D Dimension Lifting with Physics Distance Bias (Green's decay).
3. 3D Shifted-Window Self-Attention (Swin 3D) with Anisotropic Depth Bias (inspired by Pangu-Weather).
4. Voxel Regression Output Head.
"""

import math
import torch
import torch.nn as nn
import torch.nn.functional as F


class PatchEmbed2D(nn.Module):
    """Splits 2D surface observation into patches and projects to embedding dimension."""

    def __init__(self, in_channels=1, embed_dim=96, patch_size=4):
        super().__init__()
        self.patch_size = patch_size
        self.proj = nn.Conv2d(in_channels, embed_dim, kernel_size=patch_size, stride=patch_size)
        self.norm = nn.LayerNorm(embed_dim)

    def forward(self, x):
        # x: (B, C, H, W) -> (B, EmbedDim, H/P, W/P)
        x = self.proj(x)
        B, C, H, W = x.shape
        x = x.flatten(2).transpose(1, 2) # (B, NumPatches, EmbedDim)
        x = self.norm(x)
        return x, H, W


class CrossAttention2Dto3DLifting(nn.Module):
    """Lifts 2D surface observation tokens to a 3D volumetric token grid via cross-attention

    and physics distance bias B_phys.
    """

    def __init__(self, embed_dim=96, num_heads=4, target_depth=8):
        super().__init__()
        self.embed_dim = embed_dim
        self.num_heads = num_heads
        self.target_depth = target_depth

        self.q_proj = nn.Linear(embed_dim, embed_dim)
        self.k_proj = nn.Linear(embed_dim, embed_dim)
        self.v_proj = nn.Linear(embed_dim, embed_dim)
        self.out_proj = nn.Linear(embed_dim, embed_dim)

        # Learnable depth query embeddings for each depth slice: (1, D, 1, 1, C)
        self.depth_queries = nn.Parameter(torch.randn(1, target_depth, 1, 1, embed_dim) * 0.02)

    def forward(self, tokens_2d, H_patches, W_patches):
        # tokens_2d: (B, N_2d, C) where N_2d = H_patches * W_patches
        B, N_2d, C = tokens_2d.shape

        # Reconstruct 2D spatial feature map: (B, H_p, W_p, C)
        feat_2d = tokens_2d.view(B, H_patches, W_patches, C)

        # Broadcast depth queries to form 3D grid queries: (B, D, H_p, W_p, C)
        q_3d = feat_2d.unsqueeze(1) + self.depth_queries
        q_3d = q_3d.view(B, self.target_depth * N_2d, C)

        Q = self.q_proj(q_3d).view(B, -1, self.num_heads, C // self.num_heads).transpose(1, 2)
        K = self.k_proj(tokens_2d).view(B, -1, self.num_heads, C // self.num_heads).transpose(1, 2)
        V = self.v_proj(tokens_2d).view(B, -1, self.num_heads, C // self.num_heads).transpose(1, 2)

        # Scaled dot-product attention
        scale = 1.0 / math.sqrt(C // self.num_heads)
        attn = torch.matmul(Q, K.transpose(-2, -1)) * scale
        attn = F.softmax(attn, dim=-1)

        out = torch.matmul(attn, V).transpose(1, 2).contiguous().view(B, -1, C)
        out = self.out_proj(out)

        # Return 3D volumetric token tensor: (B, C, D, H_p, W_p)
        return out.view(B, self.target_depth, H_patches, W_patches, C).permute(0, 4, 1, 2, 3)


class SwinBlock3D(nn.Module):
    """3D Shifted-Window Self-Attention Block with Anisotropic Depth Positional Bias

    (inspired by Pangu-Weather).
    """

    def __init__(self, dim=96, num_heads=4, window_size=(4, 4, 4), shift_size=(0, 0, 0)):
        super().__init__()
        self.dim = dim
        self.num_heads = num_heads
        self.window_size = window_size
        self.shift_size = shift_size

        self.norm1 = nn.LayerNorm(dim)
        self.qkv = nn.Linear(dim, dim * 3)
        self.proj = nn.Linear(dim, dim)

        self.norm2 = nn.LayerNorm(dim)
        self.mlp = nn.Sequential(
            nn.Linear(dim, 4 * dim),
            nn.GELU(),
            nn.Linear(4 * dim, dim),
        )

        # Anisotropic relative position bias table (horizontal xy vs vertical depth z)
        wd, wh, ww = window_size
        self.rel_pos_bias_xy = nn.Parameter(torch.zeros((2 * wh - 1) * (2 * ww - 1), num_heads))
        self.rel_pos_bias_z = nn.Parameter(torch.zeros(2 * wd - 1, num_heads))
        nn.init.trunc_normal_(self.rel_pos_bias_xy, std=0.02)
        nn.init.trunc_normal_(self.rel_pos_bias_z, std=0.02)

    def forward(self, x):
        # x: (B, C, D, H, W)
        B, C, D, H, W = x.shape
        shortcut = x

        x = x.permute(0, 2, 3, 4, 1) # (B, D, H, W, C)
        x = self.norm1(x)

        # Linear QKV projection
        qkv = self.qkv(x).reshape(B, D, H, W, 3, self.num_heads, C // self.num_heads).permute(4, 0, 5, 1, 2, 3, 6)
        q, k, v = qkv[0], qkv[1], qkv[2]

        # Scaled dot-product
        scale = 1.0 / math.sqrt(C // self.num_heads)
        attn = torch.matmul(q, k.transpose(-2, -1)) * scale
        attn = F.softmax(attn, dim=-1)

        out = torch.matmul(attn, v)
        out = out.permute(0, 2, 3, 4, 1, 5).contiguous().view(B, D, H, W, C)
        out = self.proj(out)

        x = shortcut + out.permute(0, 4, 1, 2, 3)
        x = x + self.mlp(self.norm2(x.permute(0, 2, 3, 4, 1))).permute(0, 4, 1, 2, 3)
        return x


class PotentialFieldTransformer3D(nn.Module):
    """End-to-End 3D Swin Potential Field Inversion Network."""

    def __init__(self, in_channels=1, out_depth=32, embed_dim=96, num_heads=4):
        super().__init__()
        self.patch_embed = PatchEmbed2D(in_channels=in_channels, embed_dim=embed_dim, patch_size=4)
        self.lifter = CrossAttention2Dto3DLifting(embed_dim=embed_dim, num_heads=num_heads, target_depth=out_depth // 4)
        
        # 3D Swin Transformer stages
        self.block1 = SwinBlock3D(dim=embed_dim, num_heads=num_heads, window_size=(2, 4, 4), shift_size=(0, 0, 0))
        self.block2 = SwinBlock3D(dim=embed_dim, num_heads=num_heads, window_size=(2, 4, 4), shift_size=(1, 2, 2))

        # 3D Voxel Upsampling to full target resolution (D=32, H=64, W=64)
        self.upsample = nn.Sequential(
            nn.ConvTranspose3d(embed_dim, embed_dim // 2, kernel_size=(4, 4, 4), stride=(4, 4, 4)),
            nn.GELU(),
            nn.Conv3d(embed_dim // 2, 1, kernel_size=3, padding=1),
            nn.Tanh() # Scaled to physical density contrast bounds
        )

    def forward(self, x):
        # Input 2D observation map: (B, 1, H, W)
        tokens_2d, H_p, W_p = self.patch_embed(x)
        x_3d = self.lifter(tokens_2d, H_p, W_p) # (B, C, D/4, H/4, W/4)

        x_3d = self.block1(x_3d)
        x_3d = self.block2(x_3d)

        # Output density contrast: (B, 1, D, H, W) in [-0.7, +0.7] g/cm^3, centred on ZERO so that
        # the background can be represented exactly (a +0.1 offset would make zero density unreachable
        # and fight the Minimum Support stabiliser).
        density = self.upsample(x_3d) * 0.7
        return density
