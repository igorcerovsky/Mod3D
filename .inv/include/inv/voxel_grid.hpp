#pragma once

#include "point.hpp"
#include <vector>
#include <cstddef>
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <string>

namespace inv {

/**
 * @brief 3D Regular Cartesian Voxel Grid.
 * Represents discretized subsurface modeling domain partitioned into cells.
 * Contiguous row-major memory layout (z fastest or x/y/z flat).
 */
template <typename T = double>
class VoxelGrid {
public:
    using value_type = T;
    using size_type = std::size_t;

    VoxelGrid() = default;

    VoxelGrid(size_type nx, size_type ny, size_type nz,
              T x0, T y0, T z0,
              T dx, T dy, T dz,
              T initial_val = T{0})
        : nx_(nx), ny_(ny), nz_(nz),
          x0_(x0), y0_(y0), z0_(z0),
          dx_(dx), dy_(dy), dz_(dz),
          cells_(nx * ny * nz, initial_val)
    {
        if (nx == 0 || ny == 0 || nz == 0) {
            throw std::invalid_argument("VoxelGrid dimensions must be non-zero");
        }
        if (dx <= T{0} || dy <= T{0} || dz <= T{0}) {
            throw std::invalid_argument("VoxelGrid cell spacing (dx, dy, dz) must be strictly positive");
        }
    }

    [[nodiscard]] constexpr size_type nx() const noexcept { return nx_; }
    [[nodiscard]] constexpr size_type ny() const noexcept { return ny_; }
    [[nodiscard]] constexpr size_type nz() const noexcept { return nz_; }
    [[nodiscard]] constexpr size_type total_cells() const noexcept { return cells_.size(); }

    [[nodiscard]] constexpr T x0() const noexcept { return x0_; }
    [[nodiscard]] constexpr T y0() const noexcept { return y0_; }
    [[nodiscard]] constexpr T z0() const noexcept { return z0_; }

    [[nodiscard]] constexpr T dx() const noexcept { return dx_; }
    [[nodiscard]] constexpr T dy() const noexcept { return dy_; }
    [[nodiscard]] constexpr T dz() const noexcept { return dz_; }

    [[nodiscard]] constexpr T cell_volume() const noexcept { return dx_ * dy_ * dz_; }

    [[nodiscard]] constexpr T x_max() const noexcept { return x0_ + static_cast<T>(nx_) * dx_; }
    [[nodiscard]] constexpr T y_max() const noexcept { return y0_ + static_cast<T>(ny_) * dy_; }
    [[nodiscard]] constexpr T z_max() const noexcept { return z0_ + static_cast<T>(nz_) * dz_; }

    [[nodiscard]] constexpr size_type index(size_type ix, size_type iy, size_type iz) const noexcept {
        return (ix * ny_ + iy) * nz_ + iz;
    }

    constexpr void decode_index(size_type idx, size_type& ix, size_type& iy, size_type& iz) const noexcept {
        iz = idx % nz_;
        size_type tmp = idx / nz_;
        iy = tmp % ny_;
        ix = tmp / ny_;
    }

    [[nodiscard]] T& operator()(size_type ix, size_type iy, size_type iz) {
        return cells_[index(ix, iy, iz)];
    }

    [[nodiscard]] const T& operator()(size_type ix, size_type iy, size_type iz) const {
        return cells_[index(ix, iy, iz)];
    }

    [[nodiscard]] T& operator[](size_type idx) noexcept { return cells_[idx]; }
    [[nodiscard]] const T& operator[](size_type idx) const noexcept { return cells_[idx]; }

    [[nodiscard]] std::vector<T>& data() noexcept { return cells_; }
    [[nodiscard]] const std::vector<T>& data() const noexcept { return cells_; }

    [[nodiscard]] Point3D<T> cell_center(size_type ix, size_type iy, size_type iz) const noexcept {
        return {
            x0_ + (static_cast<T>(ix) + T{0.5}) * dx_,
            y0_ + (static_cast<T>(iy) + T{0.5}) * dy_,
            z0_ + (static_cast<T>(iz) + T{0.5}) * dz_
        };
    }

    void cell_bounds(size_type ix, size_type iy, size_type iz,
                     Point3D<T>& p_min, Point3D<T>& p_max) const noexcept {
        p_min.x = x0_ + static_cast<T>(ix) * dx_;
        p_min.y = y0_ + static_cast<T>(iy) * dy_;
        p_min.z = z0_ + static_cast<T>(iz) * dz_;

        p_max.x = p_min.x + dx_;
        p_max.y = p_min.y + dy_;
        p_max.z = p_min.z + dz_;
    }

    void fill(T val) noexcept {
        std::fill(cells_.begin(), cells_.end(), val);
    }

    [[nodiscard]] T min_value() const noexcept {
        if (cells_.empty()) return T{0};
        return *std::min_element(cells_.begin(), cells_.end());
    }

    [[nodiscard]] T max_value() const noexcept {
        if (cells_.empty()) return T{0};
        return *std::max_element(cells_.begin(), cells_.end());
    }

    [[nodiscard]] T mean_value() const noexcept {
        if (cells_.empty()) return T{0};
        T sum = std::accumulate(cells_.begin(), cells_.end(), T{0});
        return sum / static_cast<T>(cells_.size());
    }

    [[nodiscard]] T l2_norm() const noexcept {
        T sum = T{0};
        for (const auto& v : cells_) {
            sum += v * v;
        }
        return std::sqrt(sum);
    }

    [[nodiscard]] VoxelGrid<T> resample(size_type target_nx, size_type target_ny, size_type target_nz,
                                        T target_x0, T target_y0, T target_z0,
                                        T target_dx, T target_dy, T target_dz) const
    {
        if (nx_ == target_nx && ny_ == target_ny && nz_ == target_nz &&
            std::abs(x0_ - target_x0) < 1e-6 && std::abs(y0_ - target_y0) < 1e-6 && std::abs(z0_ - target_z0) < 1e-6 &&
            std::abs(dx_ - target_dx) < 1e-6 && std::abs(dy_ - target_dy) < 1e-6 && std::abs(dz_ - target_dz) < 1e-6) {
            return *this;
        }

        VoxelGrid<T> dst(target_nx, target_ny, target_nz,
                         target_x0, target_y0, target_z0,
                         target_dx, target_dy, target_dz, T{0});
        if (nx_ == 0 || ny_ == 0 || nz_ == 0) return dst;

        for (size_type ix = 0; ix < target_nx; ++ix) {
            double u = (target_nx > 1) ? static_cast<double>(ix) / (target_nx - 1) : 0.0;
            double src_x = u * (nx_ - 1);
            size_type sx0 = std::clamp(static_cast<size_type>(src_x), size_type{0}, nx_ - 1);
            size_type sx1 = std::min(sx0 + 1, nx_ - 1);
            double fx = src_x - sx0;

            for (size_type iy = 0; iy < target_ny; ++iy) {
                double v = (target_ny > 1) ? static_cast<double>(iy) / (target_ny - 1) : 0.0;
                double src_y = v * (ny_ - 1);
                size_type sy0 = std::clamp(static_cast<size_type>(src_y), size_type{0}, ny_ - 1);
                size_type sy1 = std::min(sy0 + 1, ny_ - 1);
                double fy = src_y - sy0;

                for (size_type iz = 0; iz < target_nz; ++iz) {
                    double w = (target_nz > 1) ? static_cast<double>(iz) / (target_nz - 1) : 0.0;
                    double src_z = w * (nz_ - 1);
                    size_type sz0 = std::clamp(static_cast<size_type>(src_z), size_type{0}, nz_ - 1);
                    size_type sz1 = std::min(sz0 + 1, nz_ - 1);
                    double fz = src_z - sz0;

                    double c000 = (*this)(sx0, sy0, sz0);
                    double c100 = (*this)(sx1, sy0, sz0);
                    double c010 = (*this)(sx0, sy1, sz0);
                    double c110 = (*this)(sx1, sy1, sz0);
                    double c001 = (*this)(sx0, sy0, sz1);
                    double c101 = (*this)(sx1, sy0, sz1);
                    double c011 = (*this)(sx0, sy1, sz1);
                    double c111 = (*this)(sx1, sy1, sz1);

                    double c00 = c000 * (1.0 - fx) + c100 * fx;
                    double c10 = c010 * (1.0 - fx) + c110 * fx;
                    double c01 = c001 * (1.0 - fx) + c101 * fx;
                    double c11 = c011 * (1.0 - fx) + c111 * fx;

                    double c0 = c00 * (1.0 - fy) + c10 * fy;
                    double c1 = c01 * (1.0 - fy) + c11 * fy;

                    dst(ix, iy, iz) = static_cast<T>(c0 * (1.0 - fz) + c1 * fz);
                }
            }
        }
        return dst;
    }

    bool export_vtk_structured_points(const std::string& filepath, const std::string& field_name = "property") const {
        std::ofstream out(filepath);
        if (!out) return false;

        out << "# vtk DataFile Version 3.0\n";
        out << "Mod3D Inversion Voxel Grid\n";
        out << "ASCII\n";
        out << "DATASET STRUCTURED_POINTS\n";
        out << "DIMENSIONS " << nx_ << " " << ny_ << " " << nz_ << "\n";
        out << "ORIGIN " << x0_ << " " << y0_ << " " << z0_ << "\n";
        out << "SPACING " << dx_ << " " << dy_ << " " << dz_ << "\n";
        out << "POINT_DATA " << cells_.size() << "\n";
        out << "SCALARS " << field_name << " float 1\n";
        out << "LOOKUP_TABLE default\n";

        for (size_t iz = 0; iz < nz_; ++iz) {
            for (size_t iy = 0; iy < ny_; ++iy) {
                for (size_t ix = 0; ix < nx_; ++ix) {
                    out << static_cast<float>((*this)(ix, iy, iz)) << "\n";
                }
            }
        }
        return true;
    }

private:
    size_type nx_{0};
    size_type ny_{0};
    size_type nz_{0};
    T x0_{0};
    T y0_{0};
    T z0_{0};
    T dx_{1};
    T dy_{1};
    T dz_{1};
    std::vector<T> cells_{};
};

} // namespace inv
