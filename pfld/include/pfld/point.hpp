#pragma once

#include <cmath>
#include <limits>
#include <type_traits>
#include <ostream>

namespace pfld {

/**
 * @brief Lightweight 3D point and vector primitive.
 * Trivially copyable, standard layout, and constexpr-ready.
 */
template <typename T = double>
class Point3D
{
public:
	using value_type = T;

	T x{0};
	T y{0};
	T z{0};

	// Constructors
	constexpr Point3D() noexcept = default;
	constexpr Point3D(T x_, T y_, T z_) noexcept : x(x_), y(y_), z(z_) {}

	// Standard special member functions (trivial copy, move, destruction)
	constexpr Point3D(const Point3D&) noexcept = default;
	constexpr Point3D& operator=(const Point3D&) noexcept = default;
	constexpr Point3D(Point3D&&) noexcept = default;
	constexpr Point3D& operator=(Point3D&&) noexcept = default;
	~Point3D() = default;

	// C++20 comparison operators
	constexpr bool operator==(const Point3D& other) const noexcept = default;

	// Component-wise vector addition & subtraction
	constexpr Point3D operator+(const Point3D& pt) const noexcept {
		return Point3D(x + pt.x, y + pt.y, z + pt.z);
	}
	constexpr Point3D operator-(const Point3D& pt) const noexcept {
		return Point3D(x - pt.x, y - pt.y, z - pt.z);
	}
	constexpr Point3D operator-() const noexcept {
		return Point3D(-x, -y, -z);
	}

	constexpr Point3D& operator+=(const Point3D& pt) noexcept {
		x += pt.x; y += pt.y; z += pt.z;
		return *this;
	}
	constexpr Point3D& operator-=(const Point3D& pt) noexcept {
		x -= pt.x; y -= pt.y; z -= pt.z;
		return *this;
	}

	// Scalar multiplication (dot product)
	constexpr T operator*(const Point3D& pt) const noexcept {
		return x * pt.x + y * pt.y + z * pt.z;
	}
	constexpr T dot(const Point3D& pt) const noexcept {
		return *this * pt;
	}

	// Vector * scalar
	constexpr Point3D operator*(const T a) const noexcept {
		return Point3D(a * x, a * y, a * z);
	}
	constexpr Point3D& operator*=(const T a) noexcept {
		x *= a; y *= a; z *= a;
		return *this;
	}
	friend constexpr Point3D operator*(const T a, const Point3D& pt) noexcept {
		return pt * a;
	}

	// Vector / scalar
	constexpr Point3D operator/(const T a) const noexcept {
		if (a != T{0}) {
			return Point3D(x / a, y / a, z / a);
		}
		return Point3D(T{0}, T{0}, T{0});
	}
	constexpr Point3D& operator/=(const T a) noexcept {
		if (a != T{0}) {
			x /= a; y /= a; z /= a;
		} else {
			x = T{0}; y = T{0}; z = T{0};
		}
		return *this;
	}

	// Vector cross product (operator/ preserved for legacy Mod3D/pfld compatibility)
	constexpr Point3D operator/(const Point3D& pt) const noexcept {
		return Point3D(y * pt.z - z * pt.y,
		               z * pt.x - x * pt.z,
		               x * pt.y - y * pt.x);
	}
	constexpr Point3D cross(const Point3D& pt) const noexcept {
		return *this / pt;
	}

	// Vector length / norm
	constexpr T norm_squared() const noexcept {
		return x * x + y * y + z * z;
	}
	T norm() const noexcept {
		return std::sqrt(norm_squared());
	}
	T Abs() const noexcept {
		return norm();
	}

	// In-place normalization
	void Unit() noexcept {
		const T l = Abs();
		if (l > std::numeric_limits<T>::epsilon()) {
			x /= l; y /= l; z /= l;
		} else {
			x = T{0}; y = T{0}; z = T{0};
		}
	}

	// Returns normalized copy
	Point3D unit() const noexcept {
		Point3D copy = *this;
		copy.Unit();
		return copy;
	}

	// Offset all components by scalar
	constexpr Point3D operator+(const T a) const noexcept {
		return Point3D(x + a, y + a, z + a);
	}

	// Make all components positive
	void Positive() noexcept {
		x = std::abs(x);
		y = std::abs(y);
		z = std::abs(z);
	}

	// Invert sign of all components
	constexpr void TurnSign() noexcept {
		x = -x;
		y = -y;
		z = -z;
	}

	// Reset to origin
	constexpr void Zero() noexcept {
		x = T{0};
		y = T{0};
		z = T{0};
	}

	// Check if zero
	[[nodiscard]] constexpr bool IsZero() const noexcept {
		return (x == T{0} && y == T{0} && z == T{0});
	}

	// Angle with another vector (in radians)
	[[nodiscard]] T Angle(const Point3D& v) const noexcept {
		const T denom = std::sqrt((v * v) * ((*this) * (*this)));
		if (denom > std::numeric_limits<T>::epsilon()) {
			const T cosVal = std::clamp((*this * v) / denom, T{-1}, T{1});
			return std::acos(cosVal);
		}
		return T{0};
	}

	// Euclidean distance to another point
	[[nodiscard]] T Distance(const Point3D& other) const noexcept {
		return (*this - other).Abs();
	}
	[[nodiscard]] T distance(const Point3D& other) const noexcept {
		return (*this - other).Abs();
	}

	// Direction cosine with Z axis
	[[nodiscard]] T AngleZ() const noexcept {
		const T l = Abs();
		if (l > std::numeric_limits<T>::epsilon()) {
			return z / l;
		}
		return T{0};
	}

	// Component-wise offset
	constexpr void Offset(T xOffset, T yOffset, T zOffset) noexcept {
		x += xOffset;
		y += yOffset;
		z += zOffset;
	}
	constexpr void Offset(const Point3D& pt) noexcept {
		x += pt.x;
		y += pt.y;
		z += pt.z;
	}

	// Legacy static helpers
	static constexpr void Add(const Point3D& pt1, const Point3D& pt2, Point3D& res) noexcept {
		res.x = pt1.x + pt2.x; res.y = pt1.y + pt2.y; res.z = pt1.z + pt2.z;
	}
	static constexpr void Sub(const Point3D& pt1, const Point3D& pt2, Point3D& res) noexcept {
		res.x = pt1.x - pt2.x; res.y = pt1.y - pt2.y; res.z = pt1.z - pt2.z;
	}
	static constexpr void Cross(const Point3D& pt1, const Point3D& pt2, Point3D& res) noexcept {
		res.x = pt1.y * pt2.z - pt1.z * pt2.y;
		res.y = pt1.z * pt2.x - pt1.x * pt2.z;
		res.z = pt1.x * pt2.y - pt1.y * pt2.x;
	}

	// Stream output helper
	friend std::ostream& operator<<(std::ostream& os, const Point3D& pt) {
		return os << '(' << pt.x << ", " << pt.y << ", " << pt.z << ')';
	}
};

// Aliases for common floating point precisions
using Point3Dd = Point3D<double>;
using Point3Df = Point3D<float>;
using point = Point3D<double>;
using ptvec = std::vector<point>;

// Compile-time checks for standard layout and trivial copyability
static_assert(std::is_standard_layout_v<Point3D<double>>, "Point3D<double> must be standard layout");
static_assert(std::is_trivially_copyable_v<Point3D<double>>, "Point3D<double> must be trivially copyable");
static_assert(std::is_standard_layout_v<Point3D<float>>, "Point3D<float> must be standard layout");
static_assert(std::is_trivially_copyable_v<Point3D<float>>, "Point3D<float> must be trivially copyable");
static_assert(sizeof(Point3D<double>) == 3 * sizeof(double), "Point3D<double> must have no vtable or padding overhead");
static_assert(sizeof(Point3D<float>) == 3 * sizeof(float), "Point3D<float> must have no vtable or padding overhead");

} // namespace pfld