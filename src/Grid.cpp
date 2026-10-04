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
    std::swap(rows_, other.rows_);
    std::swap(cols_, other.cols_);
    std::swap(x0_, other.x0_);
    std::swap(y0_, other.y0_);
    std::swap(x_size_, other.x_size_);
    std::swap(y_size_, other.y_size_);
    std::swap(rot_deg_, other.rot_deg_);
    std::swap(cos_rot_, other.cos_rot_);
    std::swap(sin_rot_, other.sin_rot_);
    data_.swap(other.data_);
}

void Grid::resize(size_t rows, size_t cols, double x0, double y0, double xSize, double ySize, double rotDeg)
{
    rows_ = rows;
    cols_ = cols;
    x0_ = x0;
    y0_ = y0;
    x_size_ = xSize;
    y_size_ = ySize;
    rot_deg_ = rotDeg;
    data_.assign(rows * cols, 0.0);
    update_precomputed_trig();
}

void Grid::update_precomputed_trig()
{
    const double rad = rot_deg_ * std::numbers::pi_v<double> / 180.0;
    cos_rot_ = std::cos(rad);
    sin_rot_ = std::sin(rad);
}

double Grid::x(size_t row, size_t col) const noexcept
{
    return x0_ + (col * x_size_) * cos_rot_ - (row * y_size_) * sin_rot_;
}

double Grid::y(size_t row, size_t col) const noexcept
{
    return y0_ + (col * x_size_) * sin_rot_ + (row * y_size_) * cos_rot_;
}

Point3D Grid::point(size_t row, size_t col) const noexcept
{
    return Point3D(x(row, col), y(row, col), operator()(row, col));
}

double &Grid::at(size_t row, size_t col)
{
    if (row >= rows_ || col >= cols_) {
        throw std::out_of_range("Grid::at index out of range");
    }
    return data_[row * cols_ + col];
}

double Grid::at(size_t row, size_t col) const
{
    if (row >= rows_ || col >= cols_) {
        throw std::out_of_range("Grid::at index out of range");
    }
    return data_[row * cols_ + col];
}

double Grid::x_min() const noexcept
{
    if (empty()) return x0_;
    if (rot_deg_ == 0.0) return x0_;
    const double x00 = x(0, 0);
    const double x01 = x(0, cols_ - 1);
    const double x10 = x(rows_ - 1, 0);
    const double x11 = x(rows_ - 1, cols_ - 1);
    return std::min({x00, x01, x10, x11});
}

double Grid::x_max() const noexcept
{
    if (empty()) return x0_;
    if (rot_deg_ == 0.0) return x0_ + (cols_ > 0 ? (cols_ - 1) * x_size_ : 0.0);
    const double x00 = x(0, 0);
    const double x01 = x(0, cols_ - 1);
    const double x10 = x(rows_ - 1, 0);
    const double x11 = x(rows_ - 1, cols_ - 1);
    return std::max({x00, x01, x10, x11});
}

double Grid::y_min() const noexcept
{
    if (empty()) return y0_;
    if (rot_deg_ == 0.0) return y0_;
    const double y00 = y(0, 0);
    const double y01 = y(0, cols_ - 1);
    const double y10 = y(rows_ - 1, 0);
    const double y11 = y(rows_ - 1, cols_ - 1);
    return std::min({y00, y01, y10, y11});
}

double Grid::y_max() const noexcept
{
    if (empty()) return y0_;
    if (rot_deg_ == 0.0) return y0_ + (rows_ > 0 ? (rows_ - 1) * y_size_ : 0.0);
    const double y00 = y(0, 0);
    const double y01 = y(0, cols_ - 1);
    const double y10 = y(rows_ - 1, 0);
    const double y11 = y(rows_ - 1, cols_ - 1);
    return std::max({y00, y01, y10, y11});
}

double Grid::z_min() const noexcept
{
    return min();
}

double Grid::z_max() const noexcept
{
    return max();
}

bool Grid::world_to_grid(double xVal, double yVal, double &row, double &col) const noexcept
{
    if (x_size_ <= 0.0 || y_size_ <= 0.0) return false;

    const double dxVal = xVal - x0_;
    const double dyVal = yVal - y0_;

    // Inverse 2D rotation matrix:
    // col * dx =  dxVal * cos(rot) + dyVal * sin(rot)
    // row * dy = -dxVal * sin(rot) + dyVal * cos(rot)
    const double u =  dxVal * cos_rot_ + dyVal * sin_rot_;
    const double v = -dxVal * sin_rot_ + dyVal * cos_rot_;

    col = u / x_size_;
    row = v / y_size_;
    return true;
}

bool Grid::grid_to_world(double row, double col, double &xVal, double &yVal) const noexcept
{
    xVal = x0_ + (col * x_size_) * cos_rot_ - (row * y_size_) * sin_rot_;
    yVal = y0_ + (col * x_size_) * sin_rot_ + (row * y_size_) * cos_rot_;
    return true;
}

bool Grid::contains(double xVal, double yVal) const noexcept
{
    if (empty()) return false;
    double r = 0.0, c = 0.0;
    if (!world_to_grid(xVal, yVal, r, c)) return false;
    constexpr double eps = 1e-6;
    return (r >= -eps && r <= static_cast<double>(rows_ - 1) + eps &&
            c >= -eps && c <= static_cast<double>(cols_ - 1) + eps);
}

double Grid::interpolate(double xVal, double yVal) const
{
    if (empty() || x_size_ <= 0.0 || y_size_ <= 0.0) {
        return GRID_DUMMY;
    }

    double row_f = 0.0, col_f = 0.0;
    if (!world_to_grid(xVal, yVal, row_f, col_f)) {
        return GRID_DUMMY;
    }

    const double maxR = static_cast<double>(rows_ - 1);
    const double maxC = static_cast<double>(cols_ - 1);
    constexpr double eps = 1e-7;

    if (row_f < -eps || row_f > maxR + eps ||
        col_f < -eps || col_f > maxC + eps) {
        return GRID_DUMMY;
    }

    row_f = std::clamp(row_f, 0.0, maxR);
    col_f = std::clamp(col_f, 0.0, maxC);

    const size_t r0 = static_cast<size_t>(std::floor(row_f));
    const size_t c0 = static_cast<size_t>(std::floor(col_f));
    const size_t r1 = std::min(r0 + 1, rows_ - 1);
    const size_t c1 = std::min(c0 + 1, cols_ - 1);

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
    std::fill(data_.begin(), data_.end(), val);
}

Grid::Stats Grid::compute_stats() const noexcept
{
    Stats s;
    if (empty()) {
        s.min = 0.0;
        s.max = 0.0;
        return s;
    }

    double sum = 0.0;
    double sumSq = 0.0;

    for (double val : data_) {
        if (is_dummy_value(val)) {
            s.dummy_count++;
        } else {
            s.valid_count++;
            if (val < s.min) s.min = val;
            if (val > s.max) s.max = val;
            sum += val;
            sumSq += val * val;
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

double Grid::min() const
{
    return compute_stats().min;
}

double Grid::max() const
{
    return compute_stats().max;
}

double Grid::mean() const
{
    return compute_stats().mean;
}

double Grid::rms() const
{
    return compute_stats().rms;
}

Grid &Grid::operator+=(double val)
{
    for (double &v : data_) {
        if (!is_dummy_value(v)) v += val;
    }
    return *this;
}

Grid &Grid::operator-=(double val)
{
    for (double &v : data_) {
        if (!is_dummy_value(v)) v -= val;
    }
    return *this;
}

Grid &Grid::operator*=(double val)
{
    for (double &v : data_) {
        if (!is_dummy_value(v)) v *= val;
    }
    return *this;
}

Grid &Grid::operator/=(double val)
{
    if (val != 0.0) {
        for (double &v : data_) {
            if (!is_dummy_value(v)) v /= val;
        }
    }
    return *this;
}

Grid &Grid::operator+=(const Grid &other)
{
    if (rows_ != other.rows_ || cols_ != other.cols_) {
        throw std::invalid_argument("Grid dimensions mismatch for operator+=");
    }
    for (size_t i = 0; i < data_.size(); ++i) {
        if (is_dummy_value(data_[i]) || is_dummy_value(other.data_[i])) {
            data_[i] = GRID_DUMMY;
        } else {
            data_[i] += other.data_[i];
        }
    }
    return *this;
}

Grid &Grid::operator-=(const Grid &other)
{
    if (rows_ != other.rows_ || cols_ != other.cols_) {
        throw std::invalid_argument("Grid dimensions mismatch for operator-=");
    }
    for (size_t i = 0; i < data_.size(); ++i) {
        if (is_dummy_value(data_[i]) || is_dummy_value(other.data_[i])) {
            data_[i] = GRID_DUMMY;
        } else {
            data_[i] -= other.data_[i];
        }
    }
    return *this;
}

Grid &Grid::operator*=(const Grid &other)
{
    if (rows_ != other.rows_ || cols_ != other.cols_) {
        throw std::invalid_argument("Grid dimensions mismatch for operator*=");
    }
    for (size_t i = 0; i < data_.size(); ++i) {
        if (is_dummy_value(data_[i]) || is_dummy_value(other.data_[i])) {
            data_[i] = GRID_DUMMY;
        } else {
            data_[i] *= other.data_[i];
        }
    }
    return *this;
}

Grid &Grid::operator/=(const Grid &other)
{
    if (rows_ != other.rows_ || cols_ != other.cols_) {
        throw std::invalid_argument("Grid dimensions mismatch for operator/=");
    }
    for (size_t i = 0; i < data_.size(); ++i) {
        if (is_dummy_value(data_[i]) || is_dummy_value(other.data_[i]) || other.data_[i] == 0.0) {
            data_[i] = GRID_DUMMY;
        } else {
            data_[i] /= other.data_[i];
        }
    }
    return *this;
}

bool Grid::operator==(const Grid &other) const noexcept
{
    if (rows_ != other.rows_ || cols_ != other.cols_ ||
        x0_ != other.x0_ || y0_ != other.y0_ ||
        x_size_ != other.x_size_ || y_size_ != other.y_size_ ||
        rot_deg_ != other.rot_deg_) {
        return false;
    }
    return data_ == other.data_;
}

bool Grid::load_srf6_binary(const std::filesystem::path &filePath)
{
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return false;

    char id[4];
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

    for (size_t r = 0; r < rows_; ++r) {
        for (size_t c = 0; c < cols_; ++c) {
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

bool Grid::save_srf6_binary(const std::filesystem::path &filePath) const
{
    if (empty()) return false;
    std::ofstream file(filePath, std::ios::binary);
    if (!file.is_open()) return false;

    const char id[4] = {'D', 'S', 'B', 'B'};
    file.write(id, 4);

    const int16_t nx = static_cast<int16_t>(cols_);
    const int16_t ny = static_cast<int16_t>(rows_);
    file.write(reinterpret_cast<const char *>(&nx), 2);
    file.write(reinterpret_cast<const char *>(&ny), 2);

    const double xlo = x0_;
    const double xhi = x0_ + (cols_ - 1) * x_size_;
    const double ylo = y0_;
    const double yhi = y0_ + (rows_ - 1) * y_size_;
    const double zlo = min();
    const double zhi = max();

    file.write(reinterpret_cast<const char *>(&xlo), 8);
    file.write(reinterpret_cast<const char *>(&xhi), 8);
    file.write(reinterpret_cast<const char *>(&ylo), 8);
    file.write(reinterpret_cast<const char *>(&yhi), 8);
    file.write(reinterpret_cast<const char *>(&zlo), 8);
    file.write(reinterpret_cast<const char *>(&zhi), 8);

    for (size_t r = 0; r < rows_; ++r) {
        for (size_t c = 0; c < cols_; ++c) {
            float val = static_cast<float>(operator()(r, c));
            file.write(reinterpret_cast<const char *>(&val), 4);
        }
    }
    return file.good();
}

bool Grid::load_srf6_ascii(const std::filesystem::path &filePath)
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

    for (size_t r = 0; r < rows_; ++r) {
        for (size_t c = 0; c < cols_; ++c) {
            double val = 0.0;
            file >> val;
            operator()(r, c) = val;
        }
    }
    return !file.bad();
}

bool Grid::save_srf6_ascii(const std::filesystem::path &filePath) const
{
    if (empty()) return false;
    std::ofstream file(filePath);
    if (!file.is_open()) return false;

    file << "DSAA\n";
    file << cols_ << " " << rows_ << "\n";
    file << std::setprecision(12) << x0_ << " " << (x0_ + (cols_ - 1) * x_size_) << "\n";
    file << std::setprecision(12) << y0_ << " " << (y0_ + (rows_ - 1) * y_size_) << "\n";
    file << std::setprecision(12) << min() << " " << max() << "\n";

    for (size_t r = 0; r < rows_; ++r) {
        for (size_t c = 0; c < cols_; ++c) {
            file << operator()(r, c) << " ";
        }
        file << "\n";
    }
    return file.good();
}

std::ostream &operator<<(std::ostream &os, const Grid &grid)
{
    os << "Grid(rows=" << grid.rows_ << ", cols=" << grid.cols_
       << ", x0=" << grid.x0_ << ", y0=" << grid.y0_
       << ", dx=" << grid.x_size_ << ", dy=" << grid.y_size_
       << ", rot=" << grid.rot_deg_ << " deg)";
    return os;
}

} // namespace mod3d
