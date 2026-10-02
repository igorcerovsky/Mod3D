#include "mod3d/Grid.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>

namespace mod3d {

namespace {
constexpr double PI = 3.14159265358979323846;
}

Grid::Grid() = default;

Grid::Grid(size_t rows, size_t cols, double x0, double y0, double xSize, double ySize, double rotDeg)
{
    resize(rows, cols, x0, y0, xSize, ySize, rotDeg);
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
    const double rad = m_rotDeg * PI / 180.0;
    m_cosRot = std::cos(rad);
    m_sinRot = std::sin(rad);
}

double Grid::getX(size_t row, size_t col) const
{
    return m_x0 + (col * m_xSize) * m_cosRot - (row * m_ySize) * m_sinRot;
}

double Grid::getY(size_t row, size_t col) const
{
    return m_y0 + (col * m_xSize) * m_sinRot + (row * m_ySize) * m_cosRot;
}

Point3D Grid::getPoint(size_t row, size_t col) const
{
    return Point3D(getX(row, col), getY(row, col), operator()(row, col));
}

void Grid::fill(double val)
{
    std::fill(m_data.begin(), m_data.end(), val);
}

double Grid::getMin() const
{
    double minVal = std::numeric_limits<double>::max();
    for (double v : m_data) {
        if (!isDummyValue(v)) {
            minVal = std::min(minVal, v);
        }
    }
    return minVal;
}

double Grid::getMax() const
{
    double maxVal = -std::numeric_limits<double>::max();
    for (double v : m_data) {
        if (!isDummyValue(v)) {
            maxVal = std::max(maxVal, v);
        }
    }
    return maxVal;
}

double Grid::getMean() const
{
    double sum = 0.0;
    size_t count = 0;
    for (double v : m_data) {
        if (!isDummyValue(v)) {
            sum += v;
            count++;
        }
    }
    return (count > 0) ? (sum / count) : 0.0;
}

double Grid::getRMS() const
{
    double sumSq = 0.0;
    size_t count = 0;
    for (double v : m_data) {
        if (!isDummyValue(v)) {
            sumSq += v * v;
            count++;
        }
    }
    return (count > 0) ? std::sqrt(sumSq / count) : 0.0;
}

Grid &Grid::operator+=(double val)
{
    for (double &v : m_data) {
        if (std::abs(v - GRID_DUMMY) > 1e10) v += val;
    }
    return *this;
}

Grid &Grid::operator-=(double val)
{
    for (double &v : m_data) {
        if (std::abs(v - GRID_DUMMY) > 1e10) v -= val;
    }
    return *this;
}

Grid &Grid::operator*=(double val)
{
    for (double &v : m_data) {
        if (std::abs(v - GRID_DUMMY) > 1e10) v *= val;
    }
    return *this;
}

Grid &Grid::operator/=(double val)
{
    if (val != 0.0) {
        for (double &v : m_data) {
            if (std::abs(v - GRID_DUMMY) > 1e10) v /= val;
        }
    }
    return *this;
}

Grid &Grid::operator+=(const Grid &other)
{
    const size_t n = std::min(m_data.size(), other.m_data.size());
    for (size_t i = 0; i < n; ++i) {
        if (std::abs(m_data[i] - GRID_DUMMY) > 1e10 && std::abs(other.m_data[i] - GRID_DUMMY) > 1e10) {
            m_data[i] += other.m_data[i];
        }
    }
    return *this;
}

Grid &Grid::operator-=(const Grid &other)
{
    const size_t n = std::min(m_data.size(), other.m_data.size());
    for (size_t i = 0; i < n; ++i) {
        if (std::abs(m_data[i] - GRID_DUMMY) > 1e10 && std::abs(other.m_data[i] - GRID_DUMMY) > 1e10) {
            m_data[i] -= other.m_data[i];
        }
    }
    return *this;
}

Grid &Grid::operator*=(const Grid &other)
{
    const size_t n = std::min(m_data.size(), other.m_data.size());
    for (size_t i = 0; i < n; ++i) {
        if (std::abs(m_data[i] - GRID_DUMMY) > 1e10 && std::abs(other.m_data[i] - GRID_DUMMY) > 1e10) {
            m_data[i] *= other.m_data[i];
        }
    }
    return *this;
}

Grid &Grid::operator/=(const Grid &other)
{
    const size_t n = std::min(m_data.size(), other.m_data.size());
    for (size_t i = 0; i < n; ++i) {
        if (std::abs(m_data[i] - GRID_DUMMY) > 1e10 && std::abs(other.m_data[i] - GRID_DUMMY) > 1e10 && other.m_data[i] != 0.0) {
            m_data[i] /= other.m_data[i];
        }
    }
    return *this;
}

bool Grid::loadSrf6Binary(const std::string &filePath)
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

    if (nx <= 1 || ny <= 1) return false;

    const double dx = (xhi - xlo) / (nx - 1);
    const double dy = (yhi - ylo) / (ny - 1);

    resize(static_cast<size_t>(ny), static_cast<size_t>(nx), xlo, ylo, dx, dy, 0.0);

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
    return true;
}

bool Grid::saveSrf6Binary(const std::string &filePath) const
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
    return true;
}

bool Grid::loadSrf6Ascii(const std::string &filePath)
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

    if (nx <= 1 || ny <= 1) return false;

    const double dx = (xhi - xlo) / (nx - 1);
    const double dy = (yhi - ylo) / (ny - 1);

    resize(ny, nx, xlo, ylo, dx, dy, 0.0);

    for (size_t r = 0; r < m_rows; ++r) {
        for (size_t c = 0; c < m_cols; ++c) {
            double val = 0.0;
            file >> val;
            operator()(r, c) = val;
        }
    }
    return true;
}

bool Grid::saveSrf6Ascii(const std::string &filePath) const
{
    if (empty()) return false;
    std::ofstream file(filePath);
    if (!file.is_open()) return false;

    file << "DSAA\n";
    file << m_cols << " " << m_rows << "\n";
    file << std::setprecision(10) << m_x0 << " " << (m_x0 + (m_cols - 1) * m_xSize) << "\n";
    file << std::setprecision(10) << m_y0 << " " << (m_y0 + (m_rows - 1) * m_ySize) << "\n";
    file << std::setprecision(10) << getMin() << " " << getMax() << "\n";

    for (size_t r = 0; r < m_rows; ++r) {
        for (size_t c = 0; c < m_cols; ++c) {
            file << operator()(r, c) << " ";
        }
        file << "\n";
    }
    return true;
}

} // namespace mod3d
