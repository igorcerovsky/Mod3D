#pragma once

#include <cmath>
#include <vector>

namespace mod3d {

/**
 * @brief 3D Point and Vector class.
 * Faithfully preserves the operations and operator semantics of legacy CPoint3D.
 */
class Point3D {
public:
    double x{0.0};
    double y{0.0};
    double z{0.0};
    bool b{false};

    constexpr Point3D() = default;
    constexpr Point3D(double initX, double initY, double initZ)
        : x(initX), y(initY), z(initZ), b(false) {}

    constexpr Point3D(const Point3D& other) = default;
    constexpr Point3D& operator=(const Point3D& other) = default;

    constexpr bool operator==(const Point3D& point) const {
        return (x == point.x && y == point.y && z == point.z);
    }

    constexpr bool operator!=(const Point3D& point) const {
        return (x != point.x || y != point.y || z != point.z);
    }

    void operator+=(const Point3D& point) {
        x += point.x;
        y += point.y;
        z += point.z;
    }

    void operator-=(const Point3D& point) {
        x -= point.x;
        y -= point.y;
        z -= point.z;
    }

    constexpr Point3D operator-(const Point3D& point) const {
        return Point3D(x - point.x, y - point.y, z - point.z);
    }

    constexpr Point3D operator+(const Point3D& point) const {
        return Point3D(x + point.x, y + point.y, z + point.z);
    }

    // Scalar (dot) product of two vectors
    constexpr double operator*(const Point3D& point) const {
        return (x * point.x + y * point.y + z * point.z);
    }

    // Vector * scalar
    constexpr Point3D operator*(double a) const {
        return Point3D(x * a, y * a, z * a);
    }

    // Vector (cross) product of two vectors (legacy syntax)
    constexpr Point3D operator/(const Point3D& point) const {
        return Point3D(y * point.z - z * point.y,
                       z * point.x - x * point.z,
                       x * point.y - y * point.x);
    }

    // Vector / scalar
    Point3D operator/(double a) const {
        if (a != 0.0) {
            return Point3D(x / a, y / a, z / a);
        }
        return Point3D(0.0, 0.0, 0.0);
    }

    // Offset all components by scalar
    constexpr Point3D operator+(double a) const {
        return Point3D(x + a, y + a, z + a);
    }

    // Dot product explicit method
    constexpr double dot(const Point3D& other) const {
        return *this * other;
    }

    // Cross product explicit method
    constexpr Point3D cross(const Point3D& other) const {
        return *this / other;
    }

    // Vector magnitude (length)
    double Abs() const {
        return std::sqrt(x * x + y * y + z * z);
    }

    // Normalize to unit length
    void Unit() {
        double l = this->Abs();
        if (l != 0.0) {
            x /= l;
            y /= l;
            z /= l;
        } else {
            x = 0.0;
            y = 0.0;
            z = 0.0;
        }
    }

    // Make all components positive
    void Positive() {
        x = std::fabs(x);
        y = std::fabs(y);
        z = std::fabs(z);
    }

    // Invert sign of all components
    void TurnSign() {
        x = -x;
        y = -y;
        z = -z;
    }

    // Reset to origin
    void Zero() {
        x = 0.0;
        y = 0.0;
        z = 0.0;
    }

    // Check if zero
    bool IsZero() const {
        return (x == 0.0 && y == 0.0 && z == 0.0);
    }

    // Angle with another vector
    double Angle(const Point3D& v) const {
        double denom = std::sqrt((v * v) * ((*this) * (*this)));
        if (denom != 0.0) {
            return std::acos((v * (*this)) / denom);
        }
        return 0.0;
    }

    // Euclidean distance to another point
    double Distance(const Point3D& other) const {
        return (*this - other).Abs();
    }
    double distance(const Point3D& other) const {
        return (*this - other).Abs();
    }

    // Angle of vector with Z axis
    double AngleZ() const {
        double l = Abs();
        if (l != 0.0) {
            return z / l;
        }
        return 0.0;
    }

    void Offset(double xOffset, double yOffset, double zOffset) {
        x += xOffset;
        y += yOffset;
        z += zOffset;
    }

    void Offset(const Point3D& point) {
        x += point.x;
        y += point.y;
        z += point.z;
    }
};

// Aliases for modern / legacy compatibility
using Vector3D = Point3D;
using Pt3DArray = std::vector<Point3D>;
using Pt3DArray2D = std::vector<Pt3DArray>;

// Scalar * Point3D
inline Point3D operator*(double a, const Point3D& pt) {
    return pt * a;
}

} // namespace mod3d
