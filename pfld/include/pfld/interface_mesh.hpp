#pragma once

#include "point.hpp"
#include "facet.hpp"
#include <vector>
#include <cmath>
#include <cstddef>
#include <concepts>
#include <memory>
#include <string>
#include <algorithm>

namespace pfld {

/**
 * =================================================================================================
 * CRITICAL GEOPHYSICAL & MATHEMATICAL INVARIANT: PLANAR VS. SPHERICAL EARTH MODELING
 * =================================================================================================
 *
 * **VERTICAL FACET ELIMINATION (n_z = 0) IS STRICTLY VALID ONLY FOR PLANAR CARTESIAN MODELING.**
 *
 * In planar modeling, the vertical gravity component g_z is everywhere parallel to the Cartesian
 * Z-axis (pointing strictly downwards along +Z, or upwards along -Z).
 * For any vertical facet parallel to the Z-axis (such as the 4 lateral bounding walls of a rectangular
 * voxel cell or vertical prism):
 *
 *     n_facet = (n_x, n_y, 0)  ===>  n_z = 0
 *
 * Because the vertical gravity attraction in Pohanka's analytical formulation is weighted by n_z:
 *
 *     g_z = gamma * rho * sum_k [ n_{z,k} * FldVlado_k(r) ]
 *
 * the contribution of every vertical facet is IDENTICALLY ZERO:
 *
 *     n_{z,k} * FldVlado_k(r) = 0 * FldVlado_k(r) = 0.
 *
 * Eliminating all vertical facets in planar voxel modeling skips 4 out of 6 faces per cell (a 66.7%
 * reduction in geometric line integrals) with ZERO loss of physical precision.
 *
 * -------------------------------------------------------------------------------------------------
 * **SPHERICAL EARTH MODELING RESTRICTION (MOD3D)**:
 * -------------------------------------------------------------------------------------------------
 * On a SPHERICAL EARTH (as supported by Mod3D spherical mode), the true local plumb line / vertical
 * vector r_hat rotates across geographic latitude and longitude:
 *
 *     r_hat = r / ||r||.
 *
 * Across regional or continental surveys, the local vertical direction at observation station A is
 * NOT parallel to the local vertical direction at station B. Therefore:
 * 1. A lateral facet that is strictly vertical in a local Cartesian projection will have a non-zero
 *    projection onto the radial plumb line of distant observation points (n_z_local != 0).
 * 2. **Vertical facets CANNOT be removed in spherical Earth modeling unless the cells/vortices are
 *    drawn from the spherical Earth surface strictly parallel to the global Z-axis within a localized
 *    projected coordinate system.**
 *
 * For spherical Earth modeling, it is strongly recommended that voxel columns are drawn in from
 * the spherical Earth surface parallel to the Z-axis, or full 6-facet closed polyhedra are retained.
 * =================================================================================================
 */

enum class InterfaceFacetType : uint8_t {
    TopographyTriangle = 0, // Conforms to DEM surface (n_z != 0)
    SubsurfaceQuad     = 1, // Horizontal subsurface cell boundary (z = const, n_z = +1)
    BottomQuad         = 2  // Domain bottom boundary (z = z_max, n_z = +1)
};

/**
 * @brief Shared interface facet representing a density contrast boundary.
 * 
 * Instead of computing adjacent cell faces twice with opposite outward normals,
 * this structure computes the geometric line integral ONCE and scales it by the
 * density contrast across the interface:
 *     Delta_rho = rho_above - rho_below.
 * 
 * If Delta_rho == 0 (e.g. inside a homogeneous geological unit), the facet is SKIPPED.
 */
template <std::floating_point T = double>
struct InterfaceFacet {
    InterfaceFacetType type{InterfaceFacetType::SubsurfaceQuad};
    Facet<T, 4> quad{};     // Used when type == SubsurfaceQuad or BottomQuad
    Facet<T, 3> tri{};      // Used when type == TopographyTriangle
    T delta_rho{0};         // Density contrast in g/cm^3
    T nz{1.0};              // Normal z-component
    int64_t cell_above{-1}; // Index of cell above (-1 if air/DEM surface)
    int64_t cell_below{-1}; // Index of cell below (-1 if regional basement)

    // Scale factor converting SI (m/s^2 per kg/m^3) to mGal with density in g/cm^3:
    // (1000 kg/m^3 / (g/cm^3)) * (1e5 mGal / (m/s^2)) = 1e8
    static constexpr T SCALE_G_CM3_TO_MGAL = static_cast<T>(1.0e8);

    [[nodiscard]] T evaluate_unit_gz(const Point3D<T>& obs) const noexcept {
        // Downward gravity anomaly convention: positive downward in survey meters (mGal)
        if (type == InterfaceFacetType::TopographyTriangle) {
            return -static_cast<T>(SCALE_G_CM3_TO_MGAL * tri.field_gz(obs));
        } else {
            return -static_cast<T>(SCALE_G_CM3_TO_MGAL * quad.field_gz(obs));
        }
    }

    [[nodiscard]] T evaluate_gz(const Point3D<T>& obs) const noexcept {
        if (std::abs(delta_rho) < static_cast<T>(1e-9)) {
            return T{0};
        }
        return delta_rho * evaluate_unit_gz(obs);
    }
};

/**
 * @brief High-performance Shared-Interface Voxel & Topography Mesh for Inversion.
 * 
 * Combines:
 * 1. Triangulated DEM surface facets conforming to topography.
 * 2. Horizontal rectangular quad facets between vertically adjacent cells.
 * 3. Automatic skipping of vertical lateral facets (planar mode optimization).
 * 4. Density contrast (Delta rho) evaluation avoiding redundant calculations.
 */
template <std::floating_point T = double>
class InterfaceMesh {
public:
    using Point = Point3D<T>;

    InterfaceMesh() = default;

    /**
     * @brief Builds an optimized interface mesh from topography and a 3D voxel density grid.
     * 
     * @param nx Number of cells along X
     * @param ny Number of cells along Y
     * @param nz Number of cells along Z
     * @param x0 Domain minimum X (meters)
     * @param y0 Domain minimum Y (meters)
     * @param dx Cell size along X (meters)
     * @param dy Cell size along Y (meters)
     * @param dz Cell size along Z (meters)
     * @param topo_elevations 2D DEM elevations (size: (nx + 1) * (ny + 1) vertices) in meters.
     *                        If empty, a flat topography at z0 is assumed.
     * @param cell_densities 3D cell density contrasts (size: nx * ny * nz) in g/cm^3.
     * @param z0 Surface reference elevation (meters, default 0.0)
     * @param rho_basement Regional basement density below the domain (default 0.0)
     */
    void build(std::size_t nx, std::size_t ny, std::size_t nz,
               T x0, T y0, T dx, T dy, T dz,
               const std::vector<T>& topo_elevations,
               const std::vector<T>& cell_densities,
               T z0 = T{0},
               T rho_basement = T{0})
    {
        interfaces_.clear();
        nx_ = nx; ny_ = ny; nz_ = nz;
        dx_ = dx; dy_ = dy; dz_ = dz;
        x0_ = x0; y0_ = y0; z0_ = z0;

        const std::size_t total_cells = nx * ny * nz;
        if (cell_densities.size() != total_cells) {
            return;
        }

        const bool has_topo = (topo_elevations.size() == (nx + 1) * (ny + 1));

        auto get_topo = [&](std::size_t ix, std::size_t iy) -> T {
            if (has_topo) {
                return topo_elevations[iy * (nx + 1) + ix];
            }
            return z0;
        };

        auto cell_idx = [&](std::size_t ix, std::size_t iy, std::size_t iz) -> std::size_t {
            return (ix * ny + iy) * nz + iz;
        };

        // --------------------------------------------------------------------
        // 1. TOPOGRAPHY SURFACE: Triangulated Facets (2 triangles per column)
        // --------------------------------------------------------------------
        // Normal points outward (upward into air).
        // Delta_rho = 0.0 - rho_top = -rho_top (air density = 0).
        for (std::size_t ix = 0; ix < nx; ++ix) {
            const T x_l = x0 + static_cast<T>(ix) * dx;
            const T x_r = x_l + dx;

            for (std::size_t iy = 0; iy < ny; ++iy) {
                const T y_b = y0 + static_cast<T>(iy) * dy;
                const T y_t = y_b + dy;

                const T z00 = get_topo(ix, iy);
                const T z10 = get_topo(ix + 1, iy);
                const T z11 = get_topo(ix + 1, iy + 1);
                const T z01 = get_topo(ix, iy + 1);

                const Point p00(x_l, y_b, z00);
                const Point p10(x_r, y_b, z10);
                const Point p11(x_r, y_t, z11);
                const Point p01(x_l, y_t, z01);

                const T rho_top = cell_densities[cell_idx(ix, iy, 0)];

                if (std::abs(rho_top) > static_cast<T>(1e-9)) {
                    // Triangle 1: p00 -> p10 -> p11
                    InterfaceFacet<T> f1;
                    f1.type = InterfaceFacetType::TopographyTriangle;
                    f1.tri = Facet<T, 3>({p00, p10, p11}, true);
                    f1.delta_rho = -rho_top; // outward normal points into air
                    f1.nz = f1.tri.normal().z;
                    f1.cell_above = -1;
                    f1.cell_below = static_cast<int64_t>(cell_idx(ix, iy, 0));
                    interfaces_.push_back(std::move(f1));

                    // Triangle 2: p00 -> p11 -> p01
                    InterfaceFacet<T> f2;
                    f2.type = InterfaceFacetType::TopographyTriangle;
                    f2.tri = Facet<T, 3>({p00, p11, p01}, true);
                    f2.delta_rho = -rho_top;
                    f2.nz = f2.tri.normal().z;
                    f2.cell_above = -1;
                    f2.cell_below = static_cast<int64_t>(cell_idx(ix, iy, 0));
                    interfaces_.push_back(std::move(f2));
                }
            }
        }

        // --------------------------------------------------------------------
        // 2. SUBSURFACE INTERNAL HORIZONTAL INTERFACES (Rectangular Quads)
        // --------------------------------------------------------------------
        // Shared between cell (ix, iy, iz) and cell (ix, iy, iz+1).
        // Normal points downwards (+Z). Delta_rho = rho_above - rho_below.
        // Lateral vertical walls are SKIPPED (n_z = 0 in planar modeling).
        for (std::size_t iz = 0; iz + 1 < nz; ++iz) {
            const T z_interface = z0 + static_cast<T>(iz + 1) * dz;

            for (std::size_t ix = 0; ix < nx; ++ix) {
                const T x_l = x0 + static_cast<T>(ix) * dx;
                const T x_r = x_l + dx;

                for (std::size_t iy = 0; iy < ny; ++iy) {
                    const T y_b = y0 + static_cast<T>(iy) * dy;
                    const T y_t = y_b + dy;

                    const T rho_above = cell_densities[cell_idx(ix, iy, iz)];
                    const T rho_below = cell_densities[cell_idx(ix, iy, iz + 1)];
                    const T delta_rho = rho_above - rho_below;

                    // Skip internal interface if densities match (homogeneous interior)
                    if (std::abs(delta_rho) < static_cast<T>(1e-9)) {
                        continue;
                    }

                    // Quad oriented with downward normal (+Z):
                    // p0(x_l, y_b) -> p1(x_r, y_b) -> p2(x_r, y_t) -> p3(x_l, y_t)
                    const Point p0(x_l, y_b, z_interface);
                    const Point p1(x_r, y_b, z_interface);
                    const Point p2(x_r, y_t, z_interface);
                    const Point p3(x_l, y_t, z_interface);

                    InterfaceFacet<T> quad_facet;
                    quad_facet.type = InterfaceFacetType::SubsurfaceQuad;
                    quad_facet.quad = Facet<T, 4>({p0, p1, p2, p3}, true);
                    quad_facet.delta_rho = delta_rho;
                    quad_facet.nz = quad_facet.quad.normal().z;
                    quad_facet.cell_above = static_cast<int64_t>(cell_idx(ix, iy, iz));
                    quad_facet.cell_below = static_cast<int64_t>(cell_idx(ix, iy, iz + 1));
                    interfaces_.push_back(std::move(quad_facet));
                }
            }
        }

        // --------------------------------------------------------------------
        // 3. BOTTOM BOUNDARY (Rectangular Quads at z = z_max)
        // --------------------------------------------------------------------
        // Normal points downwards (+Z). Delta_rho = rho_bottom - rho_basement.
        const T z_bottom = z0 + static_cast<T>(nz) * dz;
        for (std::size_t ix = 0; ix < nx; ++ix) {
            const T x_l = x0 + static_cast<T>(ix) * dx;
            const T x_r = x_l + dx;

            for (std::size_t iy = 0; iy < ny; ++iy) {
                const T y_b = y0 + static_cast<T>(iy) * dy;
                const T y_t = y_b + dy;

                const T rho_bottom = cell_densities[cell_idx(ix, iy, nz - 1)];
                const T delta_rho = rho_bottom - rho_basement;

                if (std::abs(delta_rho) < static_cast<T>(1e-9)) {
                    continue;
                }

                const Point p0(x_l, y_b, z_bottom);
                const Point p1(x_r, y_b, z_bottom);
                const Point p2(x_r, y_t, z_bottom);
                const Point p3(x_l, y_t, z_bottom);

                InterfaceFacet<T> b_facet;
                b_facet.type = InterfaceFacetType::BottomQuad;
                b_facet.quad = Facet<T, 4>({p0, p1, p2, p3}, true);
                b_facet.delta_rho = delta_rho;
                b_facet.nz = b_facet.quad.normal().z;
                b_facet.cell_above = static_cast<int64_t>(cell_idx(ix, iy, nz - 1));
                b_facet.cell_below = -1;
                interfaces_.push_back(std::move(b_facet));
            }
        }
    }

    /**
     * @brief Builds full interface geometry for inversion modeling without filtering by density.
     * Records cell_above and cell_below indices on every facet.
     *
     * In planar Cartesian modeling, vertical lateral facets (nx, ny, 0) have nz = 0 and thus
     * contribute zero to vertical gravity gz. Skipping them eliminates 66.7% of line integrals
     * with exact mathematical precision.
     */
    void build_geometry(std::size_t nx, std::size_t ny, std::size_t nz,
                        T x0, T y0, T dx, T dy, T dz,
                        const std::vector<T>& topo_elevations,
                        T z0 = T{0})
    {
        interfaces_.clear();
        nx_ = nx; ny_ = ny; nz_ = nz;
        dx_ = dx; dy_ = dy; dz_ = dz;
        x0_ = x0; y0_ = y0; z0_ = z0;

        const bool has_topo = (topo_elevations.size() == (nx + 1) * (ny + 1));

        auto get_topo = [&](std::size_t ix, std::size_t iy) -> T {
            if (has_topo) {
                return topo_elevations[iy * (nx + 1) + ix];
            }
            return z0;
        };

        auto cell_idx = [&](std::size_t ix, std::size_t iy, std::size_t iz) -> int64_t {
            return static_cast<int64_t>((ix * ny + iy) * nz + iz);
        };

        // 1. TOPOGRAPHY SURFACE: Triangulated Facets (2 triangles per column)
        for (std::size_t ix = 0; ix < nx; ++ix) {
            const T x_l = x0 + static_cast<T>(ix) * dx;
            const T x_r = x_l + dx;

            for (std::size_t iy = 0; iy < ny; ++iy) {
                const T y_b = y0 + static_cast<T>(iy) * dy;
                const T y_t = y_b + dy;

                const T z00 = get_topo(ix, iy);
                const T z10 = get_topo(ix + 1, iy);
                const T z11 = get_topo(ix + 1, iy + 1);
                const T z01 = get_topo(ix, iy + 1);

                const Point p00(x_l, y_b, z00);
                const Point p10(x_r, y_b, z10);
                const Point p11(x_r, y_t, z11);
                const Point p01(x_l, y_t, z01);

                const int64_t c_top = cell_idx(ix, iy, 0);

                // Triangle 1: p00 -> p10 -> p11
                InterfaceFacet<T> f1;
                f1.type = InterfaceFacetType::TopographyTriangle;
                f1.tri = Facet<T, 3>({p00, p10, p11}, true);
                f1.delta_rho = T{0};
                f1.nz = f1.tri.normal().z;
                f1.cell_above = -1;
                f1.cell_below = c_top;
                interfaces_.push_back(std::move(f1));

                // Triangle 2: p00 -> p11 -> p01
                InterfaceFacet<T> f2;
                f2.type = InterfaceFacetType::TopographyTriangle;
                f2.tri = Facet<T, 3>({p00, p11, p01}, true);
                f2.delta_rho = T{0};
                f2.nz = f2.tri.normal().z;
                f2.cell_above = -1;
                f2.cell_below = c_top;
                interfaces_.push_back(std::move(f2));
            }
        }

        // 2. SUBSURFACE INTERNAL HORIZONTAL INTERFACES (Rectangular Quads)
        for (std::size_t iz = 0; iz + 1 < nz; ++iz) {
            const T z_interface = z0 + static_cast<T>(iz + 1) * dz;

            for (std::size_t ix = 0; ix < nx; ++ix) {
                const T x_l = x0 + static_cast<T>(ix) * dx;
                const T x_r = x_l + dx;

                for (std::size_t iy = 0; iy < ny; ++iy) {
                    const T y_b = y0 + static_cast<T>(iy) * dy;
                    const T y_t = y_b + dy;

                    const Point p0(x_l, y_b, z_interface);
                    const Point p1(x_r, y_b, z_interface);
                    const Point p2(x_r, y_t, z_interface);
                    const Point p3(x_l, y_t, z_interface);

                    InterfaceFacet<T> quad_facet;
                    quad_facet.type = InterfaceFacetType::SubsurfaceQuad;
                    quad_facet.quad = Facet<T, 4>({p0, p1, p2, p3}, true);
                    quad_facet.delta_rho = T{0};
                    quad_facet.nz = quad_facet.quad.normal().z;
                    quad_facet.cell_above = cell_idx(ix, iy, iz);
                    quad_facet.cell_below = cell_idx(ix, iy, iz + 1);
                    interfaces_.push_back(std::move(quad_facet));
                }
            }
        }

        // 3. BOTTOM BOUNDARY (Rectangular Quads at z = z_max)
        const T z_bottom = z0 + static_cast<T>(nz) * dz;
        for (std::size_t ix = 0; ix < nx; ++ix) {
            const T x_l = x0 + static_cast<T>(ix) * dx;
            const T x_r = x_l + dx;

            for (std::size_t iy = 0; iy < ny; ++iy) {
                const T y_b = y0 + static_cast<T>(iy) * dy;
                const T y_t = y_b + dy;

                const Point p0(x_l, y_b, z_bottom);
                const Point p1(x_r, y_b, z_bottom);
                const Point p2(x_r, y_t, z_bottom);
                const Point p3(x_l, y_t, z_bottom);

                InterfaceFacet<T> b_facet;
                b_facet.type = InterfaceFacetType::BottomQuad;
                b_facet.quad = Facet<T, 4>({p0, p1, p2, p3}, true);
                b_facet.delta_rho = T{0};
                b_facet.nz = b_facet.quad.normal().z;
                b_facet.cell_above = cell_idx(ix, iy, nz - 1);
                b_facet.cell_below = -1;
                interfaces_.push_back(std::move(b_facet));
            }
        }
    }

    /**
     * @brief Evaluates vertical gravity gz across observation points in mGal.
     */
    void compute_gravity_z(const std::vector<Point>& obs_pts,
                           std::vector<T>& out_gz) const noexcept
    {
        out_gz.assign(obs_pts.size(), T{0});

        for (const auto& facet : interfaces_) {
            for (std::size_t i = 0; i < obs_pts.size(); ++i) {
                out_gz[i] += facet.evaluate_gz(obs_pts[i]);
            }
        }
    }

    [[nodiscard]] std::size_t total_active_interfaces() const noexcept {
        return interfaces_.size();
    }

    [[nodiscard]] const std::vector<InterfaceFacet<T>>& interfaces() const noexcept {
        return interfaces_;
    }

private:
    std::size_t nx_{0};
    std::size_t ny_{0};
    std::size_t nz_{0};
    T dx_{0};
    T dy_{0};
    T dz_{0};
    T x0_{0};
    T y0_{0};
    T z0_{0};
    std::vector<InterfaceFacet<T>> interfaces_;
};

} // namespace pfld
