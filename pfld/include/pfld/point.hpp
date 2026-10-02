#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <type_traits>
#include <vector>

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
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;
	using reference = T&;
	using const_reference = const T&;
	using pointer = T*;
	using const_pointer = const T*;
	using iterator = T*;
	using const_iterator = const T*;

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

	// --- Vector Norm and Length (STL style) ---
	[[nodiscard]] constexpr T norm_squared() const noexcept {
		return x * x + y * y + z * z;
	}
	[[nodiscard]] constexpr T length_squared() const noexcept {
		return norm_squared();
	}
	[[nodiscard]] T norm() const noexcept {
		return std::sqrt(norm_squared());
	}
	[[nodiscard]] T length() const noexcept {
		return norm();
	}
	[[nodiscard]] T abs() const noexcept {
		return norm();
	}

	// --- Normalization (STL style: normalize() in-place, normalized()/unit() copy) ---
	void normalize() noexcept {
		const T l = norm();
		if (l > std::numeric_limits<T>::epsilon()) {
			x /= l; y /= l; z /= l;
		} else {
			x = T{0}; y = T{0}; z = T{0};
		}
	}
	[[nodiscard]] Point3D normalized() const noexcept {
		Point3D copy = *this;
		copy.normalize();
		return copy;
	}
	[[nodiscard]] Point3D unit() const noexcept {
		return normalized();
	}

	// --- Sign and Zero Manipulation (STL style) ---
	void positive() noexcept {
		x = std::abs(x);
		y = std::abs(y);
		z = std::abs(z);
	}

	constexpr void turn_sign() noexcept {
		x = -x;
		y = -y;
		z = -z;
	}
	constexpr void negate() noexcept {
		turn_sign();
	}

	constexpr void zero() noexcept {
		x = T{0};
		y = T{0};
		z = T{0};
	}
	constexpr void reset() noexcept {
		zero();
	}

	[[nodiscard]] constexpr bool is_zero() const noexcept {
		return (x == T{0} && y == T{0} && z == T{0});
	}
	[[nodiscard]] constexpr bool empty() const noexcept {
		return is_zero();
	}

	// --- Geometry Queries (STL style) ---
	[[nodiscard]] T angle(const Point3D& v) const noexcept {
		const T denom = std::sqrt((v * v) * ((*this) * (*this)));
		if (denom > std::numeric_limits<T>::epsilon()) {
			const T cos_val = std::clamp((*this * v) / denom, T{-1}, T{1});
			return std::acos(cos_val);
		}
		return T{0};
	}

	[[nodiscard]] T distance(const Point3D& other) const noexcept {
		return (*this - other).norm();
	}

	[[nodiscard]] T angle_z() const noexcept {
		const T l = norm();
		if (l > std::numeric_limits<T>::epsilon()) {
			return z / l;
		}
		return T{0};
	}

	// --- Offsets (STL style) ---
	constexpr void offset(T x_offset, T y_offset, T z_offset) noexcept {
		x += x_offset;
		y += y_offset;
		z += z_offset;
	}
	constexpr void offset(const Point3D& pt) noexcept {
		x += pt.x;
		y += pt.y;
		z += pt.z;
	}

	// --- Array-like Element Access & Iteration (STL style) ---
	[[nodiscard]] static constexpr size_type size() noexcept { return 3; }
	[[nodiscard]] static constexpr size_type max_size() noexcept { return 3; }

	[[nodiscard]] constexpr reference operator[](size_type i) noexcept {
		if (i == 0) return x;
		if (i == 1) return y;
		return z;
	}
	[[nodiscard]] constexpr const_reference operator[](size_type i) const noexcept {
		if (i == 0) return x;
		if (i == 1) return y;
		return z;
	}

	[[nodiscard]] constexpr reference at(size_type i) {
		if (i >= 3) {
			throw std::out_of_range("pfld::Point3D::at: index out of range");
		}
		return (*this)[i];
	}
	[[nodiscard]] constexpr const_reference at(size_type i) const {
		if (i >= 3) {
			throw std::out_of_range("pfld::Point3D::at: index out of range");
		}
		return (*this)[i];
	}

	[[nodiscard]] constexpr pointer data() noexcept { return &x; }
	[[nodiscard]] constexpr const_pointer data() const noexcept { return &x; }

	[[nodiscard]] constexpr iterator begin() noexcept { return &x; }
	[[nodiscard]] constexpr const_iterator begin() const noexcept { return &x; }
	[[nodiscard]] constexpr const_iterator cbegin() const noexcept { return &x; }

	[[nodiscard]] constexpr iterator end() noexcept { return &x + 3; }
	[[nodiscard]] constexpr const_iterator end() const noexcept { return &x + 3; }
	[[nodiscard]] constexpr const_iterator cend() const noexcept { return &x + 3; }

	[[nodiscard]] constexpr reference front() noexcept { return x; }
	[[nodiscard]] constexpr const_reference front() const noexcept { return x; }
	[[nodiscard]] constexpr reference back() noexcept { return z; }
	[[nodiscard]] constexpr const_reference back() const noexcept { return z; }

	constexpr void fill(const T& val) noexcept {
		x = val; y = val; z = val;
	}

	// --- Static Helpers (STL style) ---
	static constexpr void add(const Point3D& pt1, const Point3D& pt2, Point3D& res) noexcept {
		res.x = pt1.x + pt2.x; res.y = pt1.y + pt2.y; res.z = pt1.z + pt2.z;
	}
	static constexpr void sub(const Point3D& pt1, const Point3D& pt2, Point3D& res) noexcept {
		res.x = pt1.x - pt2.x; res.y = pt1.y - pt2.y; res.z = pt1.z - pt2.z;
	}
	static constexpr void cross(const Point3D& pt1, const Point3D& pt2, Point3D& res) noexcept {
		res.x = pt1.y * pt2.z - pt1.z * pt2.y;
		res.y = pt1.z * pt2.x - pt1.x * pt2.z;
		res.z = pt1.x * pt2.y - pt1.y * pt2.x;
	}

	// --- Legacy Compatibility Aliases ---
	[[nodiscard]] T Abs() const noexcept { return norm(); }
	void Unit() noexcept { normalize(); }
	void Positive() noexcept { positive(); }
	constexpr void TurnSign() noexcept { turn_sign(); }
	constexpr void Zero() noexcept { zero(); }
	[[nodiscard]] constexpr bool IsZero() const noexcept { return is_zero(); }
	[[nodiscard]] T Angle(const Point3D& v) const noexcept { return angle(v); }
	[[nodiscard]] T Distance(const Point3D& other) const noexcept { return distance(other); }
	[[nodiscard]] T AngleZ() const noexcept { return angle_z(); }
	constexpr void Offset(T x_offset, T y_offset, T z_offset) noexcept { offset(x_offset, y_offset, z_offset); }
	constexpr void Offset(const Point3D& pt) noexcept { offset(pt); }
	static constexpr void Add(const Point3D& pt1, const Point3D& pt2, Point3D& res) noexcept { add(pt1, pt2, res); }
	static constexpr void Sub(const Point3D& pt1, const Point3D& pt2, Point3D& res) noexcept { sub(pt1, pt2, res); }
	static constexpr void Cross(const Point3D& pt1, const Point3D& pt2, Point3D& res) noexcept { cross(pt1, pt2, res); }

	// Offset all components by scalar
	constexpr Point3D operator+(const T a) const noexcept {
		return Point3D(x + a, y + a, z + a);
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