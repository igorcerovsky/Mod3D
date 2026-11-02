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
