#pragma once

#include <cmath>
#include <cstddef>
#include <ostream>

namespace inv {

/**
 * @brief Lightweight 3D point and vector primitive for the inversion engine.
 * Fully standard layout, constexpr-ready, and zero-allocation.
 */
template <typename T = double>
struct Point3D {
    T x{0};
    T y{0};
    T z{0};

    constexpr Point3D() noexcept = default;
    constexpr Point3D(T x_, T y_, T z_) noexcept : x(x_), y(y_), z(z_) {}

    constexpr Point3D operator+(const Point3D& o) const noexcept {
        return {x + o.x, y + o.y, z + o.z};
    }

    constexpr Point3D operator-(const Point3D& o) const noexcept {
        return {x - o.x, y - o.y, z - o.z};
    }

    constexpr Point3D operator*(T s) const noexcept {
        return {x * s, y * s, z * s};
    }

    constexpr Point3D operator/(T s) const noexcept {
        return {x / s, y / s, z / s};
    }

    constexpr Point3D& operator+=(const Point3D& o) noexcept {
        x += o.x; y += o.y; z += o.z;
        return *this;
    }

    constexpr Point3D& operator-=(const Point3D& o) noexcept {
        x -= o.x; y -= o.y; z -= o.z;
        return *this;
    }

    [[nodiscard]] constexpr T dot(const Point3D& o) const noexcept {
        return x * o.x + y * o.y + z * o.z;
    }

    [[nodiscard]] constexpr Point3D cross(const Point3D& o) const noexcept {
        return {
            y * o.z - z * o.y,
            z * o.x - x * o.z,
            x * o.y - y * o.x
        };
    }

    [[nodiscard]] constexpr T squared_norm() const noexcept {
        return x * x + y * y + z * z;
    }

    [[nodiscard]] T norm() const noexcept {
        return std::sqrt(squared_norm());
    }

    [[nodiscard]] Point3D normalized() const noexcept {
        const T n = norm();
        return (n > T{0}) ? (*this / n) : Point3D{};
    }

    constexpr bool operator==(const Point3D& o) const noexcept = default;
};

template <typename T>
inline std::ostream& operator<<(std::ostream& os, const Point3D<T>& p) {
    return os << "(" << p.x << ", " << p.y << ", " << p.z << ")";
}

} // namespace inv
