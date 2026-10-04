#pragma once

#include "mod3d/Point3D.h"
#include <vector>
#include <string>
#include <limits>
#include <cmath>
#include <cstddef>
#include <span>
#include <numbers>
#include <filesystem>
#include <iosfwd>
#include <stdexcept>
#include <algorithm>

namespace mod3d {

constexpr double GRID_DUMMY = 1.7976931348623158e+308;

/**
 * @brief 2D Geophysical Regular Grid.
 * 
 * Used for topography/relief, flight altitude, observed fields, modeled fields,
 * and residuals. Supports Surfer 6 (DSBB binary and DSAA ASCII) import and export,
 * rotated coordinate transformations, bilinear interpolation, and grid algebra.
 */
class Grid {
public:
    struct Stats {
        double min{std::numeric_limits<double>::max()};
        double max{-std::numeric_limits<double>::max()};
        double mean{0.0};
        double rms{0.0};
        size_t valid_count{0};
        size_t dummy_count{0};
    };

    // ========================================================================
    // Lifecycle & Rule of 5
    // ========================================================================
    Grid() = default;
    Grid(size_t rows, size_t cols, double x0, double y0, double xSize, double ySize, double rotDeg = 0.0);
    Grid(const Grid &) = default;
    Grid &operator=(const Grid &) = default;
    Grid(Grid &&) noexcept = default;
    Grid &operator=(Grid &&) noexcept = default;
    ~Grid() = default;

    void swap(Grid &other) noexcept;
    friend void swap(Grid &a, Grid &b) noexcept { a.swap(b); }

    void resize(size_t rows, size_t cols, double x0, double y0, double xSize, double ySize, double rotDeg = 0.0);

    // ========================================================================
    // Modern C++20 API (STL snake_case convention, noexcept, [[nodiscard]])
    // ========================================================================

    // Dimensions and Geometry
    [[nodiscard]] constexpr size_t rows() const noexcept { return rows_; }
    [[nodiscard]] constexpr size_t cols() const noexcept { return cols_; }
    [[nodiscard]] constexpr size_t size() const noexcept { return rows_ * cols_; }
    [[nodiscard]] constexpr bool empty() const noexcept { return rows_ == 0 || cols_ == 0; }
    [[nodiscard]] constexpr bool is_empty() const noexcept { return empty(); }

    [[nodiscard]] constexpr double x0() const noexcept { return x0_; }
    [[nodiscard]] constexpr double y0() const noexcept { return y0_; }
    [[nodiscard]] constexpr double dx() const noexcept { return x_size_; }
    [[nodiscard]] constexpr double dy() const noexcept { return y_size_; }
    [[nodiscard]] constexpr double x_size() const noexcept { return x_size_; }
    [[nodiscard]] constexpr double y_size() const noexcept { return y_size_; }
    [[nodiscard]] constexpr double rotation_deg() const noexcept { return rot_deg_; }
    [[nodiscard]] double rotation_rad() const noexcept { return rot_deg_ * std::numbers::pi_v<double> / 180.0; }

    // World coordinate bounding box
    [[nodiscard]] double x_min() const noexcept;
    [[nodiscard]] double x_max() const noexcept;
    [[nodiscard]] double y_min() const noexcept;
    [[nodiscard]] double y_max() const noexcept;
    [[nodiscard]] double z_min() const noexcept;
    [[nodiscard]] double z_max() const noexcept;

    // Coordinate conversions
    [[nodiscard]] double x(size_t row, size_t col) const noexcept;
    [[nodiscard]] double y(size_t row, size_t col) const noexcept;
    [[nodiscard]] Point3D point(size_t row, size_t col) const noexcept;

    [[nodiscard]] bool world_to_grid(double xVal, double yVal, double &row, double &col) const noexcept;
    [[nodiscard]] bool grid_to_world(double row, double col, double &xVal, double &yVal) const noexcept;
    [[nodiscard]] bool contains(double xVal, double yVal) const noexcept;

    // Bilinear interpolation in world coordinates
    [[nodiscard]] double interpolate(double xVal, double yVal) const;
    [[nodiscard]] double sample(double xVal, double yVal) const { return interpolate(xVal, yVal); }

    // Element Access
    [[nodiscard]] double operator()(size_t row, size_t col) const noexcept {
        return data_[row * cols_ + col];
    }
    [[nodiscard]] double &operator()(size_t row, size_t col) noexcept {
        return data_[row * cols_ + col];
    }

    [[nodiscard]] double &at(size_t row, size_t col);
    [[nodiscard]] double at(size_t row, size_t col) const;

    [[nodiscard]] double operator[](size_t index) const noexcept { return data_[index]; }
    [[nodiscard]] double &operator[](size_t index) noexcept { return data_[index]; }

    // Direct Data and Span Access
    [[nodiscard]] const std::vector<double> &data() const noexcept { return data_; }
    [[nodiscard]] std::vector<double> &data() noexcept { return data_; }
    [[nodiscard]] std::span<const double> span() const noexcept { return data_; }
    [[nodiscard]] std::span<double> span() noexcept { return data_; }

    // Iterators
    [[nodiscard]] auto begin() noexcept { return data_.begin(); }
    [[nodiscard]] auto end() noexcept { return data_.end(); }
    [[nodiscard]] auto begin() const noexcept { return data_.begin(); }
    [[nodiscard]] auto end() const noexcept { return data_.end(); }
    [[nodiscard]] auto cbegin() const noexcept { return data_.cbegin(); }
    [[nodiscard]] auto cend() const noexcept { return data_.cend(); }

    // Dummy value checks
    [[nodiscard]] static constexpr bool is_dummy_value(double val) noexcept {
        return val >= 1.7e38 || (val != val);
    }
    [[nodiscard]] bool is_dummy(size_t row, size_t col) const noexcept {
        return is_dummy_value(operator()(row, col));
    }

    // Mutators
    void fill(double val);
    void zero() { fill(0.0); }

    // Statistics
    [[nodiscard]] Stats compute_stats() const noexcept;
    [[nodiscard]] double min() const;
    [[nodiscard]] double max() const;
    [[nodiscard]] double mean() const;
    [[nodiscard]] double rms() const;

    // Grid Arithmetic (In-place)
    Grid &operator+=(double val);
    Grid &operator-=(double val);
    Grid &operator*=(double val);
    Grid &operator/=(double val);

    Grid &operator+=(const Grid &other);
    Grid &operator-=(const Grid &other);
    Grid &operator*=(const Grid &other);
    Grid &operator/=(const Grid &other);

    // Non-member Grid Arithmetic
    friend Grid operator+(Grid lhs, double rhs) { lhs += rhs; return lhs; }
    friend Grid operator+(double lhs, Grid rhs) { rhs += lhs; return rhs; }
    friend Grid operator-(Grid lhs, double rhs) { lhs -= rhs; return lhs; }
    friend Grid operator*(Grid lhs, double rhs) { lhs *= rhs; return lhs; }
    friend Grid operator*(double lhs, Grid rhs) { rhs *= lhs; return rhs; }
    friend Grid operator/(Grid lhs, double rhs) { lhs /= rhs; return lhs; }

    friend Grid operator+(Grid lhs, const Grid &rhs) { lhs += rhs; return lhs; }
    friend Grid operator-(Grid lhs, const Grid &rhs) { lhs -= rhs; return lhs; }
    friend Grid operator*(Grid lhs, const Grid &rhs) { lhs *= rhs; return lhs; }
    friend Grid operator/(Grid lhs, const Grid &rhs) { lhs /= rhs; return lhs; }

    // Comparisons
    [[nodiscard]] bool operator==(const Grid &other) const noexcept;
    [[nodiscard]] bool operator!=(const Grid &other) const noexcept { return !(*this == other); }

    // File I/O
    bool load_srf6_binary(const std::filesystem::path &filePath);
    bool save_srf6_binary(const std::filesystem::path &filePath) const;
    bool load_srf6_ascii(const std::filesystem::path &filePath);
    bool save_srf6_ascii(const std::filesystem::path &filePath) const;

    friend std::ostream &operator<<(std::ostream &os, const Grid &grid);

    // ========================================================================
    // Legacy API (CamelCase compatibility wrappers for MFC Mod3D codebase)
    // ========================================================================
    [[nodiscard]] constexpr double xSize() const noexcept { return dx(); }
    [[nodiscard]] constexpr double ySize() const noexcept { return dy(); }
    [[nodiscard]] constexpr double rotation() const noexcept { return rotation_deg(); }

    [[nodiscard]] double getX(size_t row, size_t col) const noexcept { return x(row, col); }
    [[nodiscard]] double getY(size_t row, size_t col) const noexcept { return y(row, col); }
    [[nodiscard]] Point3D getPoint(size_t row, size_t col) const noexcept { return point(row, col); }

    [[nodiscard]] double getValue(size_t row, size_t col) const noexcept { return operator()(row, col); }
    void setValue(size_t row, size_t col, double val) noexcept { operator()(row, col) = val; }

    [[nodiscard]] static bool isDummyValue(double val) noexcept { return is_dummy_value(val); }
    [[nodiscard]] bool isDummy(size_t row, size_t col) const noexcept { return is_dummy(row, col); }

    void zeroData() { zero(); }

    [[nodiscard]] double getMin() const { return min(); }
    [[nodiscard]] double getMax() const { return max(); }
    [[nodiscard]] double getMean() const { return mean(); }
    [[nodiscard]] double getRMS() const { return rms(); }

    bool loadSrf6Binary(const std::filesystem::path &filePath) { return load_srf6_binary(filePath); }
    bool saveSrf6Binary(const std::filesystem::path &filePath) const { return save_srf6_binary(filePath); }
    bool loadSrf6Ascii(const std::filesystem::path &filePath) { return load_srf6_ascii(filePath); }
    bool saveSrf6Ascii(const std::filesystem::path &filePath) const { return save_srf6_ascii(filePath); }

private:
    void update_precomputed_trig();
    void updatePrecomputedTrig() { update_precomputed_trig(); }

    size_t rows_{0};
    size_t cols_{0};
    double x0_{0.0};
    double y0_{0.0};
    double x_size_{0.0};
    double y_size_{0.0};
    double rot_deg_{0.0};
    double cos_rot_{1.0};
    double sin_rot_{0.0};
    std::vector<double> data_;
};

} // namespace mod3d
