#pragma once

#include <algorithm>
#include <cmath>
#include <compare>
#include <concepts>
#include <numbers>
#include <ostream>
#include <string_view>
#include <type_traits>
#include <utility>

namespace pfld::units {

// ============================================================================
// 1. Compile-Time Dimensions
// ============================================================================

template <int L, int M, int T, int I, int Angle = 0>
struct Dimension {
    static constexpr int length = L;
    static constexpr int mass = M;
    static constexpr int time = T;
    static constexpr int current = I;
    static constexpr int angle = Angle;
};

using ScalarDimension        = Dimension<0, 0, 0, 0, 0>;
using LengthDimension        = Dimension<1, 0, 0, 0, 0>;
using TimeDimension          = Dimension<0, 0, 1, 0, 0>;
using MassDimension          = Dimension<0, 1, 0, 0, 0>;
using CurrentDimension       = Dimension<0, 0, 0, 1, 0>;
using AngleDimension         = Dimension<0, 0, 0, 0, 1>;
using AreaDimension          = Dimension<2, 0, 0, 0, 0>;
using VolumeDimension        = Dimension<3, 0, 0, 0, 0>;
using VelocityDimension      = Dimension<1, 0, -1, 0, 0>;
using AccelerationDimension  = Dimension<1, 0, -2, 0, 0>;
using GradientDimension      = Dimension<0, 0, -2, 0, 0>;  // s^-2 (tensor component)
using DensityDimension       = Dimension<-3, 1, 0, 0, 0>; // kg / m^3
using MagneticFieldDimension = Dimension<0, 1, -2, -1, 0>; // Tesla = kg / (s^2 * A)

// ============================================================================
// 2. Unit Tag Types and Metadata
// ============================================================================

namespace tag {

// --- Length ---
struct Meters {
    using dimension = LengthDimension;
    static constexpr double scale_to_base = 1.0;
    static constexpr std::string_view symbol = "m";
};

struct Kilometers {
    using dimension = LengthDimension;
    static constexpr double scale_to_base = 1000.0;
    static constexpr std::string_view symbol = "km";
};

struct Centimeters {
    using dimension = LengthDimension;
    static constexpr double scale_to_base = 0.01;
    static constexpr std::string_view symbol = "cm";
};

struct Millimeters {
    using dimension = LengthDimension;
    static constexpr double scale_to_base = 0.001;
    static constexpr std::string_view symbol = "mm";
};

struct Feet {
    using dimension = LengthDimension;
    static constexpr double scale_to_base = 0.3048;
    static constexpr std::string_view symbol = "ft";
};

// --- Angle ---
struct Radians {
    using dimension = AngleDimension;
    static constexpr double scale_to_base = 1.0;
    static constexpr std::string_view symbol = "rad";
};

struct Degrees {
    using dimension = AngleDimension;
    static constexpr double scale_to_base = std::numbers::pi_v<double> / 180.0;
    static constexpr std::string_view symbol = "deg";
};

struct ArcMinutes {
    using dimension = AngleDimension;
    static constexpr double scale_to_base = std::numbers::pi_v<double> / 10800.0;
    static constexpr std::string_view symbol = "arcmin";
};

struct ArcSeconds {
    using dimension = AngleDimension;
    static constexpr double scale_to_base = std::numbers::pi_v<double> / 648000.0;
    static constexpr std::string_view symbol = "arcsec";
};

// --- Time ---
struct Seconds {
    using dimension = TimeDimension;
    static constexpr double scale_to_base = 1.0;
    static constexpr std::string_view symbol = "s";
};

struct Milliseconds {
    using dimension = TimeDimension;
    static constexpr double scale_to_base = 1.0e-3;
    static constexpr std::string_view symbol = "ms";
};

struct Microseconds {
    using dimension = TimeDimension;
    static constexpr double scale_to_base = 1.0e-6;
    static constexpr std::string_view symbol = "µs";
};

struct Minutes {
    using dimension = TimeDimension;
    static constexpr double scale_to_base = 60.0;
    static constexpr std::string_view symbol = "min";
};

struct Hours {
    using dimension = TimeDimension;
    static constexpr double scale_to_base = 3600.0;
    static constexpr std::string_view symbol = "h";
};

// --- Mass ---
struct Kilograms {
    using dimension = MassDimension;
    static constexpr double scale_to_base = 1.0;
    static constexpr std::string_view symbol = "kg";
};

struct Grams {
    using dimension = MassDimension;
    static constexpr double scale_to_base = 1.0e-3;
    static constexpr std::string_view symbol = "g";
};

struct Tonnes {
    using dimension = MassDimension;
    static constexpr double scale_to_base = 1000.0;
    static constexpr std::string_view symbol = "t";
};

// --- Density ---
struct KgPerM3 {
    using dimension = DensityDimension;
    static constexpr double scale_to_base = 1.0;
    static constexpr std::string_view symbol = "kg/m³";
};

struct GPerCm3 {
    using dimension = DensityDimension;
    static constexpr double scale_to_base = 1000.0;
    static constexpr std::string_view symbol = "g/cm³";
};

// --- Acceleration / Gravity ---
struct MetersPerSecondSquared {
    using dimension = AccelerationDimension;
    static constexpr double scale_to_base = 1.0;
    static constexpr std::string_view symbol = "m/s²";
};

struct Gal {
    using dimension = AccelerationDimension;
    static constexpr double scale_to_base = 1.0e-2;
    static constexpr std::string_view symbol = "Gal";
};

struct MilliGal {
    using dimension = AccelerationDimension;
    static constexpr double scale_to_base = 1.0e-5;
    static constexpr std::string_view symbol = "mGal";
};

struct GravityUnit {
    using dimension = AccelerationDimension;
    static constexpr double scale_to_base = 1.0e-6;
    static constexpr std::string_view symbol = "g.u.";
};

struct MicroGal {
    using dimension = AccelerationDimension;
    static constexpr double scale_to_base = 1.0e-8;
    static constexpr std::string_view symbol = "µGal";
};

// --- Gradient Tensor (Eötvös / s^-2) ---
struct PerSecondSquared {
    using dimension = GradientDimension;
    static constexpr double scale_to_base = 1.0;
    static constexpr std::string_view symbol = "s⁻²";
};

struct Eotvos {
    using dimension = GradientDimension;
    static constexpr double scale_to_base = 1.0e-9;
    static constexpr std::string_view symbol = "E";
};

// --- Magnetic Field ---
struct Tesla {
    using dimension = MagneticFieldDimension;
    static constexpr double scale_to_base = 1.0;
    static constexpr std::string_view symbol = "T";
};

struct MilliTesla {
    using dimension = MagneticFieldDimension;
    static constexpr double scale_to_base = 1.0e-3;
    static constexpr std::string_view symbol = "mT";
};

struct MicroTesla {
    using dimension = MagneticFieldDimension;
    static constexpr double scale_to_base = 1.0e-6;
    static constexpr std::string_view symbol = "µT";
};

struct NanoTesla {
    using dimension = MagneticFieldDimension;
    static constexpr double scale_to_base = 1.0e-9;
    static constexpr std::string_view symbol = "nT";
};

// --- Area & Volume ---
struct SquareMeters {
    using dimension = AreaDimension;
    static constexpr double scale_to_base = 1.0;
    static constexpr std::string_view symbol = "m²";
};

struct SquareKilometers {
    using dimension = AreaDimension;
    static constexpr double scale_to_base = 1.0e6;
    static constexpr std::string_view symbol = "km²";
};

struct CubicMeters {
    using dimension = VolumeDimension;
    static constexpr double scale_to_base = 1.0;
    static constexpr std::string_view symbol = "m³";
};

struct CubicKilometers {
    using dimension = VolumeDimension;
    static constexpr double scale_to_base = 1.0e9;
    static constexpr std::string_view symbol = "km³";
};

struct CubicCentimeters {
    using dimension = VolumeDimension;
    static constexpr double scale_to_base = 1.0e-6;
    static constexpr std::string_view symbol = "cm³";
};

// --- Velocity ---
struct MetersPerSecond {
    using dimension = VelocityDimension;
    static constexpr double scale_to_base = 1.0;
    static constexpr std::string_view symbol = "m/s";
};

struct KilometersPerHour {
    using dimension = VelocityDimension;
    static constexpr double scale_to_base = 1.0 / 3.6;
    static constexpr std::string_view symbol = "km/h";
};

} // namespace tag

// ============================================================================
// 3. Concepts and Traits
// ============================================================================

template <typename T>
concept UnitType = requires {
    typename T::dimension;
    { T::scale_to_base } -> std::convertible_to<double>;
    { T::symbol } -> std::convertible_to<std::string_view>;
};

template <typename U1, typename U2>
concept SameDimension = UnitType<U1> && UnitType<U2> &&
    std::is_same_v<typename U1::dimension, typename U2::dimension>;

template <UnitType Unit, typename Rep = double>
class Quantity;

template <typename T>
struct is_quantity : std::false_type {};

template <UnitType Unit, typename Rep>
struct is_quantity<Quantity<Unit, Rep>> : std::true_type {};

template <typename T>
concept QuantityType = is_quantity<std::remove_cvref_t<T>>::value;

template <typename T>
struct extract_unit {
    using type = T;
};

template <QuantityType Q>
struct extract_unit<Q> {
    using type = typename std::remove_cvref_t<Q>::unit_type;
};

template <typename T>
using extract_unit_t = typename extract_unit<T>::type;

// ============================================================================
// 4. Quantity Class Template
// ============================================================================

template <UnitType Unit, typename Rep>
class Quantity {
public:
    using unit_type = Unit;
    using dimension_type = typename Unit::dimension;
    using rep = Rep;

    constexpr Quantity() noexcept : value_{0} {}
    constexpr explicit Quantity(Rep val) noexcept : value_{val} {}

    // Implicit conversion from another Quantity of the SAME dimension
    template <UnitType OtherUnit>
        requires SameDimension<Unit, OtherUnit>
    constexpr Quantity(const Quantity<OtherUnit, Rep>& other) noexcept
        : value_{other.value() * (OtherUnit::scale_to_base / Unit::scale_to_base)} {}

    // Assignment from another Quantity of the SAME dimension
    template <UnitType OtherUnit>
        requires SameDimension<Unit, OtherUnit>
    constexpr Quantity& operator=(const Quantity<OtherUnit, Rep>& other) noexcept {
        value_ = other.value() * (OtherUnit::scale_to_base / Unit::scale_to_base);
        return *this;
    }

    [[nodiscard]] constexpr Rep value() const noexcept { return value_; }
    [[nodiscard]] constexpr explicit operator Rep() const noexcept { return value_; }

    [[nodiscard]] static constexpr std::string_view unit_symbol() noexcept {
        return Unit::symbol;
    }

    // Convert to base SI representation value directly
    [[nodiscard]] constexpr Rep to_base() const noexcept {
        return value_ * Unit::scale_to_base;
    }

    // Convert to target Unit or target Quantity type
    template <typename Target>
        requires SameDimension<Unit, extract_unit_t<Target>>
    [[nodiscard]] constexpr Quantity<extract_unit_t<Target>, Rep> as() const noexcept {
        return Quantity<extract_unit_t<Target>, Rep>(*this);
    }

    // Convert to target Unit or target Quantity type and return raw scalar value
    template <typename Target>
        requires SameDimension<Unit, extract_unit_t<Target>>
    [[nodiscard]] constexpr Rep in() const noexcept {
        return as<Target>().value();
    }

    // --- Unary operators ---
    constexpr Quantity operator+() const noexcept { return *this; }
    constexpr Quantity operator-() const noexcept { return Quantity(-value_); }

    // --- Addition & Subtraction (same dimension) ---
    template <UnitType OtherUnit>
        requires SameDimension<Unit, OtherUnit>
    constexpr Quantity operator+(const Quantity<OtherUnit, Rep>& other) const noexcept {
        return Quantity(value_ + other.template in<Unit>());
    }

    template <UnitType OtherUnit>
        requires SameDimension<Unit, OtherUnit>
    constexpr Quantity& operator+=(const Quantity<OtherUnit, Rep>& other) noexcept {
        value_ += other.template in<Unit>();
        return *this;
    }

    template <UnitType OtherUnit>
        requires SameDimension<Unit, OtherUnit>
    constexpr Quantity operator-(const Quantity<OtherUnit, Rep>& other) const noexcept {
        return Quantity(value_ - other.template in<Unit>());
    }

    template <UnitType OtherUnit>
        requires SameDimension<Unit, OtherUnit>
    constexpr Quantity& operator-=(const Quantity<OtherUnit, Rep>& other) noexcept {
        value_ -= other.template in<Unit>();
        return *this;
    }

    // --- Scalar Multiplication & Division ---
    constexpr Quantity operator*(Rep scalar) const noexcept {
        return Quantity(value_ * scalar);
    }

    friend constexpr Quantity operator*(Rep scalar, const Quantity& q) noexcept {
        return Quantity(scalar * q.value_);
    }

    constexpr Quantity& operator*=(Rep scalar) noexcept {
        value_ *= scalar;
        return *this;
    }

    constexpr Quantity operator/(Rep scalar) const noexcept {
        return Quantity(value_ / scalar);
    }

    constexpr Quantity& operator/=(Rep scalar) noexcept {
        value_ /= scalar;
        return *this;
    }

    // --- Ratio division (same dimension -> dimensionless Rep) ---
    template <UnitType OtherUnit>
        requires SameDimension<Unit, OtherUnit>
    constexpr Rep operator/(const Quantity<OtherUnit, Rep>& other) const noexcept {
        return to_base() / other.to_base();
    }

    // --- Three-way Comparison and Equality (normalized to base unit) ---
    template <UnitType OtherUnit>
        requires SameDimension<Unit, OtherUnit>
    constexpr bool operator==(const Quantity<OtherUnit, Rep>& other) const noexcept {
        return to_base() == other.to_base();
    }

    template <UnitType OtherUnit>
        requires SameDimension<Unit, OtherUnit>
    constexpr auto operator<=>(const Quantity<OtherUnit, Rep>& other) const noexcept {
        return to_base() <=> other.to_base();
    }

private:
    Rep value_{0};
};

// Stream insertion operator
template <UnitType U, typename Rep>
inline std::ostream& operator<<(std::ostream& os, const Quantity<U, Rep>& q) {
    os << q.value() << " " << q.unit_symbol();
    return os;
}

// ============================================================================
// 5. Quantity Type Aliases
// ============================================================================

// Length
using Meters            = Quantity<tag::Meters>;
using Meter             = Meters;
using Kilometers        = Quantity<tag::Kilometers>;
using Kilometer         = Kilometers;
using Centimeters       = Quantity<tag::Centimeters>;
using Millimeters       = Quantity<tag::Millimeters>;
using Feet              = Quantity<tag::Feet>;

// Angle
using Radians           = Quantity<tag::Radians>;
using Radian            = Radians;
using Degrees           = Quantity<tag::Degrees>;
using Degree            = Degrees;
using ArcMinutes        = Quantity<tag::ArcMinutes>;
using ArcSeconds        = Quantity<tag::ArcSeconds>;

// Time
using Seconds           = Quantity<tag::Seconds>;
using Second            = Seconds;
using Milliseconds      = Quantity<tag::Milliseconds>;
using Microseconds      = Quantity<tag::Microseconds>;
using Minutes           = Quantity<tag::Minutes>;
using Hours             = Quantity<tag::Hours>;

// Mass
using Kilograms         = Quantity<tag::Kilograms>;
using Kilogram          = Kilograms;
using Grams             = Quantity<tag::Grams>;
using Tonnes            = Quantity<tag::Tonnes>;

// Density
using KgPerM3           = Quantity<tag::KgPerM3>;
using GPerCm3           = Quantity<tag::GPerCm3>;

// Acceleration / Gravity
using MetersPerSecondSquared = Quantity<tag::MetersPerSecondSquared>;
using SI_Acceleration   = MetersPerSecondSquared;
using Gal               = Quantity<tag::Gal>;
using MilliGal          = Quantity<tag::MilliGal>;
using MGal              = MilliGal;
using GravityUnit       = Quantity<tag::GravityUnit>;
using GU                = GravityUnit;
using MicroGal          = Quantity<tag::MicroGal>;
using UGal              = MicroGal;

// Gradient Tensor
using PerSecondSquared  = Quantity<tag::PerSecondSquared>;
using SI_Gradient       = PerSecondSquared;
using Eotvos            = Quantity<tag::Eotvos>;

// Magnetic Field
using Tesla             = Quantity<tag::Tesla>;
using MilliTesla        = Quantity<tag::MilliTesla>;
using MicroTesla        = Quantity<tag::MicroTesla>;
using NanoTesla         = Quantity<tag::NanoTesla>;
using Gamma             = NanoTesla;

// Area & Volume
using SquareMeters      = Quantity<tag::SquareMeters>;
using SquareKilometers  = Quantity<tag::SquareKilometers>;
using CubicMeters       = Quantity<tag::CubicMeters>;
using CubicKilometers   = Quantity<tag::CubicKilometers>;
using CubicCentimeters  = Quantity<tag::CubicCentimeters>;

// Velocity
using MetersPerSecond   = Quantity<tag::MetersPerSecond>;
using KilometersPerHour = Quantity<tag::KilometersPerHour>;

// ============================================================================
// 6. Cross-Dimensional Operations
// ============================================================================

// --- Length * Length = Area ---
template <UnitType U1, UnitType U2, typename Rep = double>
    requires (std::is_same_v<typename U1::dimension, LengthDimension> &&
              std::is_same_v<typename U2::dimension, LengthDimension>)
constexpr Quantity<tag::SquareMeters, Rep> operator*(const Quantity<U1, Rep>& a,
                                                     const Quantity<U2, Rep>& b) noexcept {
    return Quantity<tag::SquareMeters, Rep>(a.to_base() * b.to_base());
}

// --- Area * Length = Volume ---
template <UnitType U1, UnitType U2, typename Rep = double>
    requires (std::is_same_v<typename U1::dimension, AreaDimension> &&
              std::is_same_v<typename U2::dimension, LengthDimension>)
constexpr Quantity<tag::CubicMeters, Rep> operator*(const Quantity<U1, Rep>& a,
                                                    const Quantity<U2, Rep>& b) noexcept {
    return Quantity<tag::CubicMeters, Rep>(a.to_base() * b.to_base());
}

template <UnitType U1, UnitType U2, typename Rep = double>
    requires (std::is_same_v<typename U1::dimension, LengthDimension> &&
              std::is_same_v<typename U2::dimension, AreaDimension>)
constexpr Quantity<tag::CubicMeters, Rep> operator*(const Quantity<U1, Rep>& a,
                                                    const Quantity<U2, Rep>& b) noexcept {
    return Quantity<tag::CubicMeters, Rep>(a.to_base() * b.to_base());
}

// --- Area / Length = Length ---
template <UnitType U1, UnitType U2, typename Rep = double>
    requires (std::is_same_v<typename U1::dimension, AreaDimension> &&
              std::is_same_v<typename U2::dimension, LengthDimension>)
constexpr Quantity<tag::Meters, Rep> operator/(const Quantity<U1, Rep>& a,
                                               const Quantity<U2, Rep>& b) noexcept {
    return Quantity<tag::Meters, Rep>(a.to_base() / b.to_base());
}

// --- Volume / Length = Area ---
template <UnitType U1, UnitType U2, typename Rep = double>
    requires (std::is_same_v<typename U1::dimension, VolumeDimension> &&
              std::is_same_v<typename U2::dimension, LengthDimension>)
constexpr Quantity<tag::SquareMeters, Rep> operator/(const Quantity<U1, Rep>& a,
                                                     const Quantity<U2, Rep>& b) noexcept {
    return Quantity<tag::SquareMeters, Rep>(a.to_base() / b.to_base());
}

// --- Volume / Area = Length ---
template <UnitType U1, UnitType U2, typename Rep = double>
    requires (std::is_same_v<typename U1::dimension, VolumeDimension> &&
              std::is_same_v<typename U2::dimension, AreaDimension>)
constexpr Quantity<tag::Meters, Rep> operator/(const Quantity<U1, Rep>& a,
                                               const Quantity<U2, Rep>& b) noexcept {
    return Quantity<tag::Meters, Rep>(a.to_base() / b.to_base());
}

// --- Volume * Density = Mass ---
template <UnitType U1, UnitType U2, typename Rep = double>
    requires (std::is_same_v<typename U1::dimension, VolumeDimension> &&
              std::is_same_v<typename U2::dimension, DensityDimension>)
constexpr Quantity<tag::Kilograms, Rep> operator*(const Quantity<U1, Rep>& v,
                                                  const Quantity<U2, Rep>& d) noexcept {
    return Quantity<tag::Kilograms, Rep>(v.to_base() * d.to_base());
}

template <UnitType U1, UnitType U2, typename Rep = double>
    requires (std::is_same_v<typename U1::dimension, DensityDimension> &&
              std::is_same_v<typename U2::dimension, VolumeDimension>)
constexpr Quantity<tag::Kilograms, Rep> operator*(const Quantity<U1, Rep>& d,
                                                  const Quantity<U2, Rep>& v) noexcept {
    return Quantity<tag::Kilograms, Rep>(d.to_base() * v.to_base());
}

// --- Mass / Volume = Density ---
template <UnitType U1, UnitType U2, typename Rep = double>
    requires (std::is_same_v<typename U1::dimension, MassDimension> &&
              std::is_same_v<typename U2::dimension, VolumeDimension>)
constexpr Quantity<tag::KgPerM3, Rep> operator/(const Quantity<U1, Rep>& m,
                                                const Quantity<U2, Rep>& v) noexcept {
    return Quantity<tag::KgPerM3, Rep>(m.to_base() / v.to_base());
}

// --- Mass / Density = Volume ---
template <UnitType U1, UnitType U2, typename Rep = double>
    requires (std::is_same_v<typename U1::dimension, MassDimension> &&
              std::is_same_v<typename U2::dimension, DensityDimension>)
constexpr Quantity<tag::CubicMeters, Rep> operator/(const Quantity<U1, Rep>& m,
                                                    const Quantity<U2, Rep>& d) noexcept {
    return Quantity<tag::CubicMeters, Rep>(m.to_base() / d.to_base());
}

// --- Acceleration / Length = Gradient (Tensor) ---
template <UnitType U1, UnitType U2, typename Rep = double>
    requires (std::is_same_v<typename U1::dimension, AccelerationDimension> &&
              std::is_same_v<typename U2::dimension, LengthDimension>)
constexpr Quantity<tag::PerSecondSquared, Rep> operator/(const Quantity<U1, Rep>& a,
                                                         const Quantity<U2, Rep>& l) noexcept {
    return Quantity<tag::PerSecondSquared, Rep>(a.to_base() / l.to_base());
}

// --- Gradient * Length = Acceleration ---
template <UnitType U1, UnitType U2, typename Rep = double>
    requires (std::is_same_v<typename U1::dimension, GradientDimension> &&
              std::is_same_v<typename U2::dimension, LengthDimension>)
constexpr Quantity<tag::MetersPerSecondSquared, Rep> operator*(const Quantity<U1, Rep>& g,
                                                              const Quantity<U2, Rep>& l) noexcept {
    return Quantity<tag::MetersPerSecondSquared, Rep>(g.to_base() * l.to_base());
}

template <UnitType U1, UnitType U2, typename Rep = double>
    requires (std::is_same_v<typename U1::dimension, LengthDimension> &&
              std::is_same_v<typename U2::dimension, GradientDimension>)
constexpr Quantity<tag::MetersPerSecondSquared, Rep> operator*(const Quantity<U1, Rep>& l,
                                                              const Quantity<U2, Rep>& g) noexcept {
    return Quantity<tag::MetersPerSecondSquared, Rep>(l.to_base() * g.to_base());
}

// --- Length / Time = Velocity ---
template <UnitType U1, UnitType U2, typename Rep = double>
    requires (std::is_same_v<typename U1::dimension, LengthDimension> &&
              std::is_same_v<typename U2::dimension, TimeDimension>)
constexpr Quantity<tag::MetersPerSecond, Rep> operator/(const Quantity<U1, Rep>& l,
                                                        const Quantity<U2, Rep>& t) noexcept {
    return Quantity<tag::MetersPerSecond, Rep>(l.to_base() / t.to_base());
}

// --- Velocity * Time = Length ---
template <UnitType U1, UnitType U2, typename Rep = double>
    requires (std::is_same_v<typename U1::dimension, VelocityDimension> &&
              std::is_same_v<typename U2::dimension, TimeDimension>)
constexpr Quantity<tag::Meters, Rep> operator*(const Quantity<U1, Rep>& v,
                                               const Quantity<U2, Rep>& t) noexcept {
    return Quantity<tag::Meters, Rep>(v.to_base() * t.to_base());
}

template <UnitType U1, UnitType U2, typename Rep = double>
    requires (std::is_same_v<typename U1::dimension, TimeDimension> &&
              std::is_same_v<typename U2::dimension, VelocityDimension>)
constexpr Quantity<tag::Meters, Rep> operator*(const Quantity<U1, Rep>& t,
                                               const Quantity<U2, Rep>& v) noexcept {
    return Quantity<tag::Meters, Rep>(t.to_base() * v.to_base());
}

// ============================================================================
// 7. Math & Trigonometry Helpers
// ============================================================================

template <UnitType U, typename Rep = double>
[[nodiscard]] constexpr Quantity<U, Rep> abs(const Quantity<U, Rep>& q) noexcept {
    return Quantity<U, Rep>(q.value() < Rep{0} ? -q.value() : q.value());
}

template <UnitType U1, UnitType U2, typename Rep = double>
    requires SameDimension<U1, U2>
[[nodiscard]] constexpr Quantity<U1, Rep> min(const Quantity<U1, Rep>& a,
                                              const Quantity<U2, Rep>& b) noexcept {
    return (a <= b) ? a : Quantity<U1, Rep>(b);
}

template <UnitType U1, UnitType U2, typename Rep = double>
    requires SameDimension<U1, U2>
[[nodiscard]] constexpr Quantity<U1, Rep> max(const Quantity<U1, Rep>& a,
                                              const Quantity<U2, Rep>& b) noexcept {
    return (a >= b) ? a : Quantity<U1, Rep>(b);
}

template <UnitType U1, UnitType U2, typename Rep = double>
    requires SameDimension<U1, U2>
[[nodiscard]] constexpr bool approx_equal(const Quantity<U1, Rep>& a,
                                          const Quantity<U2, Rep>& b,
                                          Rep rel_tol = 1e-9,
                                          Rep abs_tol = 1e-12) noexcept {
    const Rep a_b = a.to_base();
    const Rep b_b = b.to_base();
    const Rep diff = (a_b > b_b) ? (a_b - b_b) : (b_b - a_b);
    const Rep abs_a = (a_b < Rep{0}) ? -a_b : a_b;
    const Rep abs_b = (b_b < Rep{0}) ? -b_b : b_b;
    const Rep max_val = (abs_a > abs_b) ? abs_a : abs_b;
    const Rep tol = (abs_tol > rel_tol * max_val) ? abs_tol : (rel_tol * max_val);
    return diff <= tol;
}

// Angle Trigonometry Functions
template <UnitType AngleUnit, typename Rep = double>
    requires std::is_same_v<typename AngleUnit::dimension, AngleDimension>
[[nodiscard]] inline double sin(const Quantity<AngleUnit, Rep>& angle) noexcept {
    return std::sin(angle.template in<tag::Radians>());
}

template <UnitType AngleUnit, typename Rep = double>
    requires std::is_same_v<typename AngleUnit::dimension, AngleDimension>
[[nodiscard]] inline double cos(const Quantity<AngleUnit, Rep>& angle) noexcept {
    return std::cos(angle.template in<tag::Radians>());
}

template <UnitType AngleUnit, typename Rep = double>
    requires std::is_same_v<typename AngleUnit::dimension, AngleDimension>
[[nodiscard]] inline double tan(const Quantity<AngleUnit, Rep>& angle) noexcept {
    return std::tan(angle.template in<tag::Radians>());
}

[[nodiscard]] inline Radians asin_rad(double val) noexcept {
    return Radians(std::asin(val));
}

[[nodiscard]] inline Radians acos_rad(double val) noexcept {
    return Radians(std::acos(val));
}

[[nodiscard]] inline Radians atan_rad(double val) noexcept {
    return Radians(std::atan(val));
}

[[nodiscard]] inline Radians atan2_rad(double y, double x) noexcept {
    return Radians(std::atan2(y, x));
}

[[nodiscard]] inline Degrees asin_deg(double val) noexcept {
    return Degrees(asin_rad(val));
}

[[nodiscard]] inline Degrees acos_deg(double val) noexcept {
    return Degrees(acos_rad(val));
}

[[nodiscard]] inline Degrees atan_deg(double val) noexcept {
    return Degrees(atan_rad(val));
}

[[nodiscard]] inline Degrees atan2_deg(double y, double x) noexcept {
    return Degrees(atan2_rad(y, x));
}

// ============================================================================
// 8. Physical Constants
// ============================================================================

namespace constants {
    inline constexpr double SI_TO_MGAL = 1.0e5;       // m/s^2 -> mGal
    inline constexpr double SI_TO_GU = 1.0e6;         // m/s^2 -> g.u. (µm/s^2)
    inline constexpr double SI_TO_EOTVOS = 1.0e9;     // s^-2 -> Eötvös
    inline constexpr double SI_TO_NT = 1.0e9;         // T -> nT
    inline constexpr double G = 6.67430e-11;          // m^3 / (kg * s^2)
    inline constexpr double MU_0 = 4.0 * std::numbers::pi_v<double> * 1.0e-7; // H/m
} // namespace constants

// ============================================================================
// 9. User-Defined Literals
// ============================================================================

namespace literals {

// --- Length ---
constexpr Meters operator""_m(long double val) noexcept { return Meters(static_cast<double>(val)); }
constexpr Meters operator""_m(unsigned long long val) noexcept { return Meters(static_cast<double>(val)); }
constexpr Kilometers operator""_km(long double val) noexcept { return Kilometers(static_cast<double>(val)); }
constexpr Kilometers operator""_km(unsigned long long val) noexcept { return Kilometers(static_cast<double>(val)); }
constexpr Centimeters operator""_cm(long double val) noexcept { return Centimeters(static_cast<double>(val)); }
constexpr Centimeters operator""_cm(unsigned long long val) noexcept { return Centimeters(static_cast<double>(val)); }
constexpr Millimeters operator""_mm(long double val) noexcept { return Millimeters(static_cast<double>(val)); }
constexpr Millimeters operator""_mm(unsigned long long val) noexcept { return Millimeters(static_cast<double>(val)); }
constexpr Feet operator""_ft(long double val) noexcept { return Feet(static_cast<double>(val)); }
constexpr Feet operator""_ft(unsigned long long val) noexcept { return Feet(static_cast<double>(val)); }

// --- Angle ---
constexpr Degrees operator""_deg(long double val) noexcept { return Degrees(static_cast<double>(val)); }
constexpr Degrees operator""_deg(unsigned long long val) noexcept { return Degrees(static_cast<double>(val)); }
constexpr Radians operator""_rad(long double val) noexcept { return Radians(static_cast<double>(val)); }
constexpr Radians operator""_rad(unsigned long long val) noexcept { return Radians(static_cast<double>(val)); }
constexpr ArcMinutes operator""_arcmin(long double val) noexcept { return ArcMinutes(static_cast<double>(val)); }
constexpr ArcMinutes operator""_arcmin(unsigned long long val) noexcept { return ArcMinutes(static_cast<double>(val)); }
constexpr ArcSeconds operator""_arcsec(long double val) noexcept { return ArcSeconds(static_cast<double>(val)); }
constexpr ArcSeconds operator""_arcsec(unsigned long long val) noexcept { return ArcSeconds(static_cast<double>(val)); }

// --- Time ---
constexpr Seconds operator""_s(long double val) noexcept { return Seconds(static_cast<double>(val)); }
constexpr Seconds operator""_s(unsigned long long val) noexcept { return Seconds(static_cast<double>(val)); }
constexpr Milliseconds operator""_ms(long double val) noexcept { return Milliseconds(static_cast<double>(val)); }
constexpr Milliseconds operator""_ms(unsigned long long val) noexcept { return Milliseconds(static_cast<double>(val)); }
constexpr Microseconds operator""_us(long double val) noexcept { return Microseconds(static_cast<double>(val)); }
constexpr Microseconds operator""_us(unsigned long long val) noexcept { return Microseconds(static_cast<double>(val)); }
constexpr Minutes operator""_min(long double val) noexcept { return Minutes(static_cast<double>(val)); }
constexpr Minutes operator""_min(unsigned long long val) noexcept { return Minutes(static_cast<double>(val)); }
constexpr Hours operator""_h(long double val) noexcept { return Hours(static_cast<double>(val)); }
constexpr Hours operator""_h(unsigned long long val) noexcept { return Hours(static_cast<double>(val)); }

// --- Mass ---
constexpr Kilograms operator""_kg(long double val) noexcept { return Kilograms(static_cast<double>(val)); }
constexpr Kilograms operator""_kg(unsigned long long val) noexcept { return Kilograms(static_cast<double>(val)); }
constexpr Grams operator""_g(long double val) noexcept { return Grams(static_cast<double>(val)); }
constexpr Grams operator""_g(unsigned long long val) noexcept { return Grams(static_cast<double>(val)); }
constexpr Tonnes operator""_t(long double val) noexcept { return Tonnes(static_cast<double>(val)); }
constexpr Tonnes operator""_t(unsigned long long val) noexcept { return Tonnes(static_cast<double>(val)); }

// --- Density ---
constexpr KgPerM3 operator""_kg_m3(long double val) noexcept { return KgPerM3(static_cast<double>(val)); }
constexpr KgPerM3 operator""_kg_m3(unsigned long long val) noexcept { return KgPerM3(static_cast<double>(val)); }
constexpr GPerCm3 operator""_g_cm3(long double val) noexcept { return GPerCm3(static_cast<double>(val)); }
constexpr GPerCm3 operator""_g_cm3(unsigned long long val) noexcept { return GPerCm3(static_cast<double>(val)); }

// --- Acceleration / Gravity ---
constexpr SI_Acceleration operator""_ms2(long double val) noexcept { return SI_Acceleration(static_cast<double>(val)); }
constexpr SI_Acceleration operator""_ms2(unsigned long long val) noexcept { return SI_Acceleration(static_cast<double>(val)); }
constexpr Gal operator""_gal(long double val) noexcept { return Gal(static_cast<double>(val)); }
constexpr Gal operator""_gal(unsigned long long val) noexcept { return Gal(static_cast<double>(val)); }
constexpr MilliGal operator""_mgal(long double val) noexcept { return MilliGal(static_cast<double>(val)); }
constexpr MilliGal operator""_mgal(unsigned long long val) noexcept { return MilliGal(static_cast<double>(val)); }
constexpr GravityUnit operator""_gu(long double val) noexcept { return GravityUnit(static_cast<double>(val)); }
constexpr GravityUnit operator""_gu(unsigned long long val) noexcept { return GravityUnit(static_cast<double>(val)); }
constexpr MicroGal operator""_uGal(long double val) noexcept { return MicroGal(static_cast<double>(val)); }
constexpr MicroGal operator""_uGal(unsigned long long val) noexcept { return MicroGal(static_cast<double>(val)); }

// --- Gradient Tensor ---
constexpr SI_Gradient operator""_s2(long double val) noexcept { return SI_Gradient(static_cast<double>(val)); }
constexpr SI_Gradient operator""_s2(unsigned long long val) noexcept { return SI_Gradient(static_cast<double>(val)); }
constexpr Eotvos operator""_E(long double val) noexcept { return Eotvos(static_cast<double>(val)); }
constexpr Eotvos operator""_E(unsigned long long val) noexcept { return Eotvos(static_cast<double>(val)); }

// --- Magnetic Field ---
constexpr Tesla operator""_T(long double val) noexcept { return Tesla(static_cast<double>(val)); }
constexpr Tesla operator""_T(unsigned long long val) noexcept { return Tesla(static_cast<double>(val)); }
constexpr MilliTesla operator""_mT(long double val) noexcept { return MilliTesla(static_cast<double>(val)); }
constexpr MilliTesla operator""_mT(unsigned long long val) noexcept { return MilliTesla(static_cast<double>(val)); }
constexpr MicroTesla operator""_uT(long double val) noexcept { return MicroTesla(static_cast<double>(val)); }
constexpr MicroTesla operator""_uT(unsigned long long val) noexcept { return MicroTesla(static_cast<double>(val)); }
constexpr NanoTesla operator""_nT(long double val) noexcept { return NanoTesla(static_cast<double>(val)); }
constexpr NanoTesla operator""_nT(unsigned long long val) noexcept { return NanoTesla(static_cast<double>(val)); }
constexpr NanoTesla operator""_gamma(long double val) noexcept { return NanoTesla(static_cast<double>(val)); }
constexpr NanoTesla operator""_gamma(unsigned long long val) noexcept { return NanoTesla(static_cast<double>(val)); }

// --- Area & Volume ---
constexpr SquareMeters operator""_m2(long double val) noexcept { return SquareMeters(static_cast<double>(val)); }
constexpr SquareMeters operator""_m2(unsigned long long val) noexcept { return SquareMeters(static_cast<double>(val)); }
constexpr SquareKilometers operator""_km2(long double val) noexcept { return SquareKilometers(static_cast<double>(val)); }
constexpr SquareKilometers operator""_km2(unsigned long long val) noexcept { return SquareKilometers(static_cast<double>(val)); }
constexpr CubicMeters operator""_m3(long double val) noexcept { return CubicMeters(static_cast<double>(val)); }
constexpr CubicMeters operator""_m3(unsigned long long val) noexcept { return CubicMeters(static_cast<double>(val)); }
constexpr CubicKilometers operator""_km3(long double val) noexcept { return CubicKilometers(static_cast<double>(val)); }
constexpr CubicKilometers operator""_km3(unsigned long long val) noexcept { return CubicKilometers(static_cast<double>(val)); }
constexpr CubicCentimeters operator""_cm3(long double val) noexcept { return CubicCentimeters(static_cast<double>(val)); }
constexpr CubicCentimeters operator""_cm3(unsigned long long val) noexcept { return CubicCentimeters(static_cast<double>(val)); }

// --- Velocity ---
constexpr MetersPerSecond operator""_m_s(long double val) noexcept { return MetersPerSecond(static_cast<double>(val)); }
constexpr MetersPerSecond operator""_m_s(unsigned long long val) noexcept { return MetersPerSecond(static_cast<double>(val)); }
constexpr KilometersPerHour operator""_km_h(long double val) noexcept { return KilometersPerHour(static_cast<double>(val)); }
constexpr KilometersPerHour operator""_km_h(unsigned long long val) noexcept { return KilometersPerHour(static_cast<double>(val)); }

} // namespace literals

// Export literals inside units namespace
using namespace literals;

} // namespace pfld::units

namespace pfld {
    namespace literals {
        using namespace ::pfld::units::literals;
    }
}
