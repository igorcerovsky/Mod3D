#include "mod3d/Grid.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <numbers>
#include <ostream>

namespace mod3d {

Grid::Grid(size_t rows, size_t cols, double x0, double y0, double xSize, double ySize, double rotDeg)
{
    resize(rows, cols, x0, y0, xSize, ySize, rotDeg);
}

void Grid::swap(Grid &other) noexcept
{
    std::swap(m_rows, other.m_rows);
    std::swap(m_cols, other.m_cols);
    std::swap(m_x0, other.m_x0);
    std::swap(m_y0, other.m_y0);
    std::swap(m_xSize, other.m_xSize);
    std::swap(m_ySize, other.m_ySize);
    std::swap(m_rotDeg, other.m_rotDeg);
    std::swap(m_cosRot, other.m_cosRot);
    std::swap(m_sinRot, other.m_sinRot);
    m_data.swap(other.m_data);
}

void Grid::resize(size_t rows, size_t cols, double x0, double y0, double xSize, double ySize, double rotDeg)
{
    m_rows = rows;
    m_cols = cols;
    m_x0 = x0;
    m_y0 = y0;
    m_xSize = xSize;
    m_ySize = ySize;
    m_rotDeg = rotDeg;
    m_data.assign(rows * cols, 0.0);
    updatePrecomputedTrig();
}

void Grid::updatePrecomputedTrig()
{
    const double rad = m_rotDeg * std::numbers::pi_v<double> / 180.0;
    m_cosRot = std::cos(rad);
    m_sinRot = std::sin(rad);
}

double Grid::getX(size_t row, size_t col) const noexcept
{
    return m_x0 + (col * m_xSize) * m_cosRot - (row * m_ySize) * m_sinRot;
}

double Grid::getY(size_t row, size_t col) const noexcept
{
    return m_y0 + (col * m_xSize) * m_sinRot + (row * m_ySize) * m_cosRot;
}

Point3D Grid::getPoint(size_t row, size_t col) const noexcept
{
    return Point3D(getX(row, col), getY(row, col), operator()(row, col));
}

double &Grid::at(size_t row, size_t col)
{
    if (row >= m_rows || col >= m_cols) {
        throw std::out_of_range("Grid::at index out of range");
    }
    return m_data[row * m_cols + col];
}

double Grid::at(size_t row, size_t col) const
{
    if (row >= m_rows || col >= m_cols) {
        throw std::out_of_range("Grid::at index out of range");
    }
    return m_data[row * m_cols + col];
}

double Grid::x_min() const noexcept
{
    if (empty()) return m_x0;
    if (m_rotDeg == 0.0) return m_x0;
    const double x00 = getX(0, 0);
    const double x01 = getX(0, m_cols - 1);
    const double x10 = getX(m_rows - 1, 0);
    const double x11 = getX(m_rows - 1, m_cols - 1);
    return std::min({x00, x01, x10, x11});
}

double Grid::x_max() const noexcept
{
    if (empty()) return m_x0;
    if (m_rotDeg == 0.0) return m_x0 + (m_cols > 0 ? (m_cols - 1) * m_xSize : 0.0);
    const double x00 = getX(0, 0);
    const double x01 = getX(0, m_cols - 1);
    const double x10 = getX(m_rows - 1, 0);
    const double x11 = getX(m_rows - 1, m_cols - 1);
    return std::max({x00, x01, x10, x11});
}

double Grid::y_min() const noexcept
{
    if (empty()) return m_y0;
    if (m_rotDeg == 0.0) return m_y0;
    const double y00 = getY(0, 0);
    const double y01 = getY(0, m_cols - 1);
    const double y10 = getY(m_rows - 1, 0);
    const double y11 = getY(m_rows - 1, m_cols - 1);
    return std::min({y00, y01, y10, y11});
}

double Grid::y_max() const noexcept
{
    if (empty()) return m_y0;
    if (m_rotDeg == 0.0) return m_y0 + (m_rows > 0 ? (m_rows - 1) * m_ySize : 0.0);
    const double y00 = getY(0, 0);
    const double y01 = getY(0, m_cols - 1);
    const double y10 = getY(m_rows - 1, 0);
    const double y11 = getY(m_rows - 1, m_cols - 1);
    return std::max({y00, y01, y10, y11});
}

bool Grid::world_to_grid(double xVal, double yVal, double &row, double &col) const noexcept
{
    if (m_xSize <= 0.0 || m_ySize <= 0.0) return false;
    const double dxVal = xVal - m_x0;
    const double dyVal = yVal - m_y0;
    col = (dxVal * m_cosRot + dyVal * m_sinRot) / m_xSize;
    row = (-dxVal * m_sinRot + dyVal * m_cosRot) / m_ySize;
    return true;
}

bool Grid::grid_to_world(double row, double col, double &xVal, double &yVal) const noexcept
{
    xVal = m_x0 + (col * m_xSize) * m_cosRot - (row * m_ySize) * m_sinRot;
    yVal = m_y0 + (col * m_xSize) * m_sinRot + (row * m_ySize) * m_cosRot;
    return true;
}

bool Grid::contains(double xVal, double yVal) const noexcept
{
    if (empty() || m_xSize <= 0.0 || m_ySize <= 0.0) return false;
    double r = 0.0, c = 0.0;
    if (!world_to_grid(xVal, yVal, r, c)) return false;
    constexpr double eps = 1e-7;
    return (r >= -eps && r <= static_cast<double>(m_rows - 1) + eps &&
            c >= -eps && c <= static_cast<double>(m_cols - 1) + eps);
}

double Grid::interpolate(double xVal, double yVal) const
{
    if (empty() || m_xSize <= 0.0 || m_ySize <= 0.0) {
        return GRID_DUMMY;
    }

    double row_f = 0.0, col_f = 0.0;
    if (!world_to_grid(xVal, yVal, row_f, col_f)) {
        return GRID_DUMMY;
    }

    const double maxR = static_cast<double>(m_rows - 1);
    const double maxC = static_cast<double>(m_cols - 1);
    constexpr double eps = 1e-7;

    if (row_f < -eps || row_f > maxR + eps ||
        col_f < -eps || col_f > maxC + eps) {
        return GRID_DUMMY;
    }

    row_f = std::clamp(row_f, 0.0, maxR);
    col_f = std::clamp(col_f, 0.0, maxC);

    const size_t r0 = static_cast<size_t>(std::floor(row_f));
    const size_t c0 = static_cast<size_t>(std::floor(col_f));
    const size_t r1 = std::min(r0 + 1, m_rows - 1);
    const size_t c1 = std::min(c0 + 1, m_cols - 1);

    const double dr = row_f - static_cast<double>(r0);
    const double dc = col_f - static_cast<double>(c0);

    const double v00 = operator()(r0, c0);
    const double v01 = operator()(r0, c1);
    const double v10 = operator()(r1, c0);
    const double v11 = operator()(r1, c1);

    double valSum = 0.0;
    double weightSum = 0.0;

    auto add_weight = [&](double val, double w) {
        if (!is_dummy_value(val) && w > 0.0) {
            valSum += val * w;
            weightSum += w;
        }
    };

    add_weight(v00, (1.0 - dc) * (1.0 - dr));
    add_weight(v01, dc * (1.0 - dr));
    add_weight(v10, (1.0 - dc) * dr);
    add_weight(v11, dc * dr);

    if (weightSum > 1e-9) {
        return valSum / weightSum;
    }
    return GRID_DUMMY;
}

void Grid::fill(double val)
{
    std::fill(m_data.begin(), m_data.end(), val);
}

Grid::Stats Grid::compute_stats() const noexcept
{
    Stats s;
    if (m_data.empty()) {
        s.min = 0.0;
        s.max = 0.0;
        return s;
    }

    double sum = 0.0;
    double sumSq = 0.0;
    for (const double v : m_data) {
        if (!is_dummy_value(v)) {
            if (v < s.min) s.min = v;
            if (v > s.max) s.max = v;
            sum += v;
            sumSq += v * v;
            s.valid_count++;
        } else {
            s.dummy_count++;
        }
    }

    if (s.valid_count > 0) {
        s.mean = sum / static_cast<double>(s.valid_count);
        s.rms = std::sqrt(sumSq / static_cast<double>(s.valid_count));
    } else {
        s.min = 0.0;
        s.max = 0.0;
    }
    return s;
}

double Grid::getMin() const
{
    return compute_stats().min;
}

double Grid::getMax() const
{
    return compute_stats().max;
}

double Grid::getMean() const
{
    return compute_stats().mean;
}

double Grid::getRMS() const
{
    return compute_stats().rms;
}

Grid &Grid::operator+=(double val)
{
    for (double &v : m_data) {
        if (!is_dummy_value(v)) v += val;
    }
    return *this;
}

Grid &Grid::operator-=(double val)
{
    for (double &v : m_data) {
        if (!is_dummy_value(v)) v -= val;
    }
    return *this;
}

Grid &Grid::operator*=(double val)
{
    for (double &v : m_data) {
        if (!is_dummy_value(v)) v *= val;
    }
    return *this;
}

Grid &Grid::operator/=(double val)
{
    if (val != 0.0) {
        for (double &v : m_data) {
            if (!is_dummy_value(v)) v /= val;
        }
    }
    return *this;
}

Grid &Grid::operator+=(const Grid &other)
{
    const size_t n = std::min(m_data.size(), other.m_data.size());
    for (size_t i = 0; i < n; ++i) {
        if (!is_dummy_value(m_data[i]) && !is_dummy_value(other.m_data[i])) {
            m_data[i] += other.m_data[i];
        }
    }
    return *this;
}

Grid &Grid::operator-=(const Grid &other)
{
    const size_t n = std::min(m_data.size(), other.m_data.size());
    for (size_t i = 0; i < n; ++i) {
        if (!is_dummy_value(m_data[i]) && !is_dummy_value(other.m_data[i])) {
            m_data[i] -= other.m_data[i];
        }
    }
    return *this;
}

Grid &Grid::operator*=(const Grid &other)
{
    const size_t n = std::min(m_data.size(), other.m_data.size());
    for (size_t i = 0; i < n; ++i) {
        if (!is_dummy_value(m_data[i]) && !is_dummy_value(other.m_data[i])) {
            m_data[i] *= other.m_data[i];
        }
    }
    return *this;
}

Grid &Grid::operator/=(const Grid &other)
{
    const size_t n = std::min(m_data.size(), other.m_data.size());
    for (size_t i = 0; i < n; ++i) {
        if (!is_dummy_value(m_data[i]) && !is_dummy_value(other.m_data[i]) && other.m_data[i] != 0.0) {
            m_data[i] /= other.m_data[i];
        }
    }
    return *this;
}

bool Grid::operator==(const Grid &other) const noexcept
{
    if (m_rows != other.m_rows || m_cols != other.m_cols) return false;
    if (m_x0 != other.m_x0 || m_y0 != other.m_y0) return false;
    if (m_xSize != other.m_xSize || m_ySize != other.m_ySize) return false;
    if (m_rotDeg != other.m_rotDeg) return false;
    return m_data == other.m_data;
}

bool Grid::loadSrf6Binary(const std::filesystem::path &filePath)
{
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return false;

    char id[5] = {0};
    file.read(id, 4);
    if (std::strncmp(id, "DSBB", 4) != 0) return false;

    int16_t nx = 0, ny = 0;
    file.read(reinterpret_cast<char *>(&nx), 2);
    file.read(reinterpret_cast<char *>(&ny), 2);

    double xlo = 0, xhi = 0, ylo = 0, yhi = 0, zlo = 0, zhi = 0;
    file.read(reinterpret_cast<char *>(&xlo), 8);
    file.read(reinterpret_cast<char *>(&xhi), 8);
    file.read(reinterpret_cast<char *>(&ylo), 8);
    file.read(reinterpret_cast<char *>(&yhi), 8);
    file.read(reinterpret_cast<char *>(&zlo), 8);
    file.read(reinterpret_cast<char *>(&zhi), 8);

    if (!file || nx <= 1 || ny <= 1) return false;

    const double dxVal = (xhi - xlo) / (nx - 1);
    const double dyVal = (yhi - ylo) / (ny - 1);

    resize(static_cast<size_t>(ny), static_cast<size_t>(nx), xlo, ylo, dxVal, dyVal, 0.0);

    for (size_t r = 0; r < m_rows; ++r) {
        for (size_t c = 0; c < m_cols; ++c) {
            float val = 0.0f;
            file.read(reinterpret_cast<char *>(&val), 4);
            if (val >= 1.7e38f) {
                operator()(r, c) = GRID_DUMMY;
            } else {
                operator()(r, c) = static_cast<double>(val);
            }
        }
    }
    return !file.bad();
}

bool Grid::saveSrf6Binary(const std::filesystem::path &filePath) const
{
    if (empty()) return false;
    std::ofstream file(filePath, std::ios::binary);
    if (!file.is_open()) return false;

    const char id[4] = {'D', 'S', 'B', 'B'};
    file.write(id, 4);

    const int16_t nx = static_cast<int16_t>(m_cols);
    const int16_t ny = static_cast<int16_t>(m_rows);
    file.write(reinterpret_cast<const char *>(&nx), 2);
    file.write(reinterpret_cast<const char *>(&ny), 2);

    const double xlo = m_x0;
    const double xhi = m_x0 + (m_cols - 1) * m_xSize;
    const double ylo = m_y0;
    const double yhi = m_y0 + (m_rows - 1) * m_ySize;
    const double zlo = getMin();
    const double zhi = getMax();

    file.write(reinterpret_cast<const char *>(&xlo), 8);
    file.write(reinterpret_cast<const char *>(&xhi), 8);
    file.write(reinterpret_cast<const char *>(&ylo), 8);
    file.write(reinterpret_cast<const char *>(&yhi), 8);
    file.write(reinterpret_cast<const char *>(&zlo), 8);
    file.write(reinterpret_cast<const char *>(&zhi), 8);

    for (size_t r = 0; r < m_rows; ++r) {
        for (size_t c = 0; c < m_cols; ++c) {
            float val = static_cast<float>(operator()(r, c));
            file.write(reinterpret_cast<const char *>(&val), 4);
        }
    }
    return file.good();
}

bool Grid::loadSrf6Ascii(const std::filesystem::path &filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open()) return false;

    std::string tag;
    file >> tag;
    if (tag != "DSAA") return false;

    size_t nx = 0, ny = 0;
    file >> nx >> ny;

    double xlo = 0, xhi = 0, ylo = 0, yhi = 0, zlo = 0, zhi = 0;
    file >> xlo >> xhi;
    file >> ylo >> yhi;
    file >> zlo >> zhi;

    if (!file || nx <= 1 || ny <= 1) return false;

    const double dxVal = (xhi - xlo) / (nx - 1);
    const double dyVal = (yhi - ylo) / (ny - 1);

    resize(ny, nx, xlo, ylo, dxVal, dyVal, 0.0);

    for (size_t r = 0; r < m_rows; ++r) {
        for (size_t c = 0; c < m_cols; ++c) {
            double val = 0.0;
            file >> val;
            operator()(r, c) = val;
        }
    }
    return !file.bad();
}

bool Grid::saveSrf6Ascii(const std::filesystem::path &filePath) const
{
    if (empty()) return false;
    std::ofstream file(filePath);
    if (!file.is_open()) return false;

    file << "DSAA\n";
    file << m_cols << " " << m_rows << "\n";
    file << std::setprecision(12) << m_x0 << " " << (m_x0 + (m_cols - 1) * m_xSize) << "\n";
    file << std::setprecision(12) << m_y0 << " " << (m_y0 + (m_rows - 1) * m_ySize) << "\n";
    file << std::setprecision(12) << getMin() << " " << getMax() << "\n";

    for (size_t r = 0; r < m_rows; ++r) {
        for (size_t c = 0; c < m_cols; ++c) {
            file << operator()(r, c) << " ";
        }
        file << "\n";
    }
    return file.good();
}

std::ostream &operator<<(std::ostream &os, const Grid &grid)
{
    os << "Grid(rows=" << grid.m_rows << ", cols=" << grid.m_cols
       << ", x0=" << grid.m_x0 << ", y0=" << grid.m_y0
       << ", dx=" << grid.m_xSize << ", dy=" << grid.m_ySize
       << ", rot=" << grid.m_rotDeg << " deg)";
    return os;
}

} // namespace mod3d
