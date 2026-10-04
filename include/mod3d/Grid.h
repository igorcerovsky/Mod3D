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

    // Constructors and Rule of 5
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

    // Dimensions and Geometry
    [[nodiscard]] constexpr size_t rows() const noexcept { return m_rows; }
    [[nodiscard]] constexpr size_t cols() const noexcept { return m_cols; }
    [[nodiscard]] constexpr size_t size() const noexcept { return m_rows * m_cols; }
    [[nodiscard]] constexpr bool empty() const noexcept { return m_rows == 0 || m_cols == 0; }
    [[nodiscard]] constexpr bool is_empty() const noexcept { return empty(); }

    [[nodiscard]] constexpr double x0() const noexcept { return m_x0; }
    [[nodiscard]] constexpr double y0() const noexcept { return m_y0; }
    [[nodiscard]] constexpr double dx() const noexcept { return m_xSize; }
    [[nodiscard]] constexpr double dy() const noexcept { return m_ySize; }
    [[nodiscard]] constexpr double xSize() const noexcept { return m_xSize; }
    [[nodiscard]] constexpr double ySize() const noexcept { return m_ySize; }
    [[nodiscard]] constexpr double rotation() const noexcept { return m_rotDeg; }
    [[nodiscard]] constexpr double rotation_deg() const noexcept { return m_rotDeg; }
    [[nodiscard]] double rotation_rad() const noexcept { return m_rotDeg * std::numbers::pi_v<double> / 180.0; }

    // World coordinate bounding box
    [[nodiscard]] double x_min() const noexcept;
    [[nodiscard]] double x_max() const noexcept;
    [[nodiscard]] double y_min() const noexcept;
    [[nodiscard]] double y_max() const noexcept;
    [[nodiscard]] double z_min() const noexcept { return getMin(); }
    [[nodiscard]] double z_max() const noexcept { return getMax(); }

    // Coordinate conversions
    [[nodiscard]] double getX(size_t row, size_t col) const noexcept;
    [[nodiscard]] double getY(size_t row, size_t col) const noexcept;
    [[nodiscard]] double x(size_t row, size_t col) const noexcept { return getX(row, col); }
    [[nodiscard]] double y(size_t row, size_t col) const noexcept { return getY(row, col); }
    [[nodiscard]] Point3D getPoint(size_t row, size_t col) const noexcept;
    [[nodiscard]] Point3D point(size_t row, size_t col) const noexcept { return getPoint(row, col); }

    [[nodiscard]] bool world_to_grid(double xVal, double yVal, double &row, double &col) const noexcept;
    [[nodiscard]] bool grid_to_world(double row, double col, double &xVal, double &yVal) const noexcept;
    [[nodiscard]] bool contains(double xVal, double yVal) const noexcept;

    // Bilinear interpolation in world coordinates
    [[nodiscard]] double interpolate(double xVal, double yVal) const;
    [[nodiscard]] double sample(double xVal, double yVal) const { return interpolate(xVal, yVal); }

    // Element Access
    [[nodiscard]] double operator()(size_t row, size_t col) const noexcept {
        return m_data[row * m_cols + col];
    }
    [[nodiscard]] double &operator()(size_t row, size_t col) noexcept {
        return m_data[row * m_cols + col];
    }

    [[nodiscard]] double &at(size_t row, size_t col);
    [[nodiscard]] double at(size_t row, size_t col) const;

    [[nodiscard]] double operator[](size_t index) const noexcept { return m_data[index]; }
    [[nodiscard]] double &operator[](size_t index) noexcept { return m_data[index]; }

    [[nodiscard]] double getValue(size_t row, size_t col) const noexcept {
        return operator()(row, col);
    }
    void setValue(size_t row, size_t col, double val) noexcept {
        operator()(row, col) = val;
    }

    // Direct Data and Span Access
    [[nodiscard]] const std::vector<double> &data() const noexcept { return m_data; }
    [[nodiscard]] std::vector<double> &data() noexcept { return m_data; }
    [[nodiscard]] std::span<const double> span() const noexcept { return m_data; }
    [[nodiscard]] std::span<double> span() noexcept { return m_data; }

    // Iterators
    [[nodiscard]] auto begin() noexcept { return m_data.begin(); }
    [[nodiscard]] auto end() noexcept { return m_data.end(); }
    [[nodiscard]] auto begin() const noexcept { return m_data.begin(); }
    [[nodiscard]] auto end() const noexcept { return m_data.end(); }
    [[nodiscard]] auto cbegin() const noexcept { return m_data.cbegin(); }
    [[nodiscard]] auto cend() const noexcept { return m_data.cend(); }

    // Dummy value checks
    [[nodiscard]] static constexpr bool is_dummy_value(double val) noexcept {
        return val >= 1.7e38 || (val != val);
    }
    [[nodiscard]] static bool isDummyValue(double val) noexcept {
        return is_dummy_value(val);
    }
    [[nodiscard]] bool is_dummy(size_t row, size_t col) const noexcept {
        return is_dummy_value(operator()(row, col));
    }
    [[nodiscard]] bool isDummy(size_t row, size_t col) const noexcept {
        return is_dummy(row, col);
    }

    // Mutators
    void fill(double val);
    void zero() { fill(0.0); }
    void zeroData() { zero(); }

    // Statistics
    [[nodiscard]] Stats compute_stats() const noexcept;
    [[nodiscard]] double getMin() const;
    [[nodiscard]] double getMax() const;
    [[nodiscard]] double getMean() const;
    [[nodiscard]] double getRMS() const;
    [[nodiscard]] double min() const { return getMin(); }
    [[nodiscard]] double max() const { return getMax(); }
    [[nodiscard]] double mean() const { return getMean(); }
    [[nodiscard]] double rms() const { return getRMS(); }

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

    // File I/O (Supports std::filesystem::path and std::string)
    bool loadSrf6Binary(const std::filesystem::path &filePath);
    bool saveSrf6Binary(const std::filesystem::path &filePath) const;
    bool loadSrf6Ascii(const std::filesystem::path &filePath);
    bool saveSrf6Ascii(const std::filesystem::path &filePath) const;

    friend std::ostream &operator<<(std::ostream &os, const Grid &grid);

private:
    void updatePrecomputedTrig();

    size_t m_rows{0};
    size_t m_cols{0};
    double m_x0{0.0};
    double m_y0{0.0};
    double m_xSize{0.0};
    double m_ySize{0.0};
    double m_rotDeg{0.0};
    double m_cosRot{1.0};
    double m_sinRot{0.0};
    std::vector<double> m_data;
};

} // namespace mod3d
