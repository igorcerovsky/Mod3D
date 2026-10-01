#pragma once

#include "mod3d/Point3D.h"
#include <vector>
#include <string>
#include <limits>
#include <cmath>
#include <cstddef>

namespace mod3d {

constexpr double GRID_DUMMY = 1.7976931348623158e+308;

/**
 * @brief 2D Geophysical Regular Grid.
 * 
 * Used for topography/relief, flight altitude, observed fields, modeled fields,
 * and residuals. Supports Surfer 6 (DSBB binary and DSAA ASCII) import and export.
 */
class Grid {
public:
    Grid();
    Grid(size_t rows, size_t cols, double x0, double y0, double xSize, double ySize, double rotDeg = 0.0);

    void resize(size_t rows, size_t cols, double x0, double y0, double xSize, double ySize, double rotDeg = 0.0);

    size_t rows() const { return m_rows; }
    size_t cols() const { return m_cols; }
    size_t size() const { return m_rows * m_cols; }
    bool empty() const { return m_rows == 0 || m_cols == 0; }

    double x0() const { return m_x0; }
    double y0() const { return m_y0; }
    double xSize() const { return m_xSize; }
    double ySize() const { return m_ySize; }
    double rotation() const { return m_rotDeg; }

    double getX(size_t row, size_t col) const;
    double getY(size_t row, size_t col) const;
    Point3D getPoint(size_t row, size_t col) const;

    double operator()(size_t row, size_t col) const {
        return m_data[row * m_cols + col];
    }
    double &operator()(size_t row, size_t col) {
        return m_data[row * m_cols + col];
    }

    double getValue(size_t row, size_t col) const {
        return operator()(row, col);
    }
    void setValue(size_t row, size_t col, double val) {
        operator()(row, col) = val;
    }

    const std::vector<double> &data() const { return m_data; }
    std::vector<double> &data() { return m_data; }

    bool isDummy(size_t row, size_t col) const {
        return std::abs(operator()(row, col) - GRID_DUMMY) < 1e10 || std::isnan(operator()(row, col));
    }

    void fill(double val);
    void zero() { fill(0.0); }

    // Statistics
    double getMin() const;
    double getMax() const;
    double getMean() const;
    double getRMS() const;

    // Grid arithmetic operators
    Grid &operator+=(double val);
    Grid &operator-=(double val);
    Grid &operator*=(double val);
    Grid &operator/=(double val);

    Grid &operator+=(const Grid &other);
    Grid &operator-=(const Grid &other);
    Grid &operator*=(const Grid &other);
    Grid &operator/=(const Grid &other);

    // File I/O
    bool loadSrf6Binary(const std::string &filePath);
    bool saveSrf6Binary(const std::string &filePath) const;
    bool loadSrf6Ascii(const std::string &filePath);
    bool saveSrf6Ascii(const std::string &filePath) const;

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
