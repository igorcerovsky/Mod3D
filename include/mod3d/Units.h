#pragma once

#include "units/units.hpp"
#include "mod3d/Point3D.h"
#include <concepts>
#include <cmath>
#include <numbers>

namespace mod3d {

namespace units = ::units;

namespace literals {
    using namespace ::units::literals;
}

// ============================================================================
// Core Geophysical Domain Aliases
// ============================================================================

// Length & Distance
using Length          = units::Meters;
using Kilometers      = units::Kilometers;
using Millimeters     = units::Millimeters;

// Area & Volume
using Area            = units::SquareMeters;
using Volume          = units::CubicMeters;

// Density & Mass
using Density         = units::KgPerM3;
using DensityGPerCm3  = units::GPerCm3;

// Gravity Acceleration & Anomalies
using Acceleration    = units::SI_Acceleration; // m/s^2
using GravityField    = units::MilliGal;         // mGal = 1e-5 m/s^2
using GravityUnit     = units::GravityUnit;      // g.u. = 1e-6 m/s^2
using MicroGal        = units::MicroGal;

// Gravity Gradient Tensor
using GradientTensor  = units::Eotvos;           // E = 1e-9 s^-2
using GradientSI      = units::SI_Gradient;      // s^-2

// Magnetic Field & Anomaly
using MagneticField   = units::NanoTesla;        // nT = 1e-9 T
using MagneticFieldSI = units::Tesla;

// Angles & Rotation
using Angle           = units::Degrees;
using AngleRad        = units::Radians;

// Time
using Time            = units::Seconds;

// ============================================================================
// Strongly-Typed Multi-Component Geophysical Structures
// ============================================================================

/**
 * @brief Full 3D Gravity Gradient Tensor with compile-time units in Eötvös.
 * Represents the 6 independent components of the symmetric Marussi tensor:
 * [ Vxx  Vxy  Vxz ]
 * [ Vxy  Vyy  Vyz ]
 * [ Vxz  Vyz  Vzz ]
 */
struct GravityGradientTensor {
    units::Eotvos gxx{0.0};
    units::Eotvos gyy{0.0};
    units::Eotvos gzz{0.0};
    units::Eotvos gxy{0.0};
    units::Eotvos gxz{0.0};
    units::Eotvos gyz{0.0};

    [[nodiscard]] constexpr units::Eotvos trace() const noexcept {
        return gxx + gyy + gzz;
    }

    [[nodiscard]] constexpr units::SI_Gradient gxx_si() const noexcept { return gxx.as<units::tag::PerSecondSquared>(); }
    [[nodiscard]] constexpr units::SI_Gradient gyy_si() const noexcept { return gyy.as<units::tag::PerSecondSquared>(); }
    [[nodiscard]] constexpr units::SI_Gradient gzz_si() const noexcept { return gzz.as<units::tag::PerSecondSquared>(); }
    [[nodiscard]] constexpr units::SI_Gradient gxy_si() const noexcept { return gxy.as<units::tag::PerSecondSquared>(); }
    [[nodiscard]] constexpr units::SI_Gradient gxz_si() const noexcept { return gxz.as<units::tag::PerSecondSquared>(); }
    [[nodiscard]] constexpr units::SI_Gradient gyz_si() const noexcept { return gyz.as<units::tag::PerSecondSquared>(); }

    [[nodiscard]] constexpr bool operator==(const GravityGradientTensor &) const noexcept = default;
};

/**
 * @brief 3D Gravity Vector with compile-time units in mGal.
 */
struct GravityVector {
    units::MilliGal gx{0.0};
    units::MilliGal gy{0.0};
    units::MilliGal gz{0.0};

    [[nodiscard]] units::MilliGal total() const noexcept {
        return units::MilliGal(std::sqrt(gx.value() * gx.value() +
                                         gy.value() * gy.value() +
                                         gz.value() * gz.value()));
    }

    [[nodiscard]] constexpr bool operator==(const GravityVector &) const noexcept = default;
};

/**
 * @brief 3D Magnetic Vector with compile-time units in nT.
 */
struct MagneticVector {
    units::NanoTesla mx{0.0};
    units::NanoTesla my{0.0};
    units::NanoTesla mz{0.0};

    [[nodiscard]] units::NanoTesla total() const noexcept {
        return units::NanoTesla(std::sqrt(mx.value() * mx.value() +
                                          my.value() * my.value() +
                                          mz.value() * mz.value()));
    }

    [[nodiscard]] constexpr bool operator==(const MagneticVector &) const noexcept = default;
};

/**
 * @brief 3D Spatial Position with compile-time units in Meters.
 * Represents a physical point in 3D Cartesian coordinates (x, y, z) in meters.
 * Trivially copyable, standard layout (24 bytes), constexpr-ready.
 */
struct Position3D {
    units::Meters x{0.0};
    units::Meters y{0.0};
    units::Meters z{0.0};

    constexpr Position3D() noexcept = default;
    constexpr Position3D(units::Meters x_, units::Meters y_, units::Meters z_) noexcept
        : x(x_), y(y_), z(z_) {}

    template <units::UnitType Ux, units::UnitType Uy, units::UnitType Uz>
        requires (units::SameDimension<Ux, units::tag::Meters> &&
                  units::SameDimension<Uy, units::tag::Meters> &&
                  units::SameDimension<Uz, units::tag::Meters>)
    constexpr Position3D(const units::Quantity<Ux>& qx,
                         const units::Quantity<Uy>& qy,
                         const units::Quantity<Uz>& qz) noexcept
        : x(qx.template as<units::tag::Meters>()),
          y(qy.template as<units::tag::Meters>()),
          z(qz.template as<units::tag::Meters>()) {}

    // Explicit constructor from raw numeric values (interpreted in meters)
    constexpr explicit Position3D(double x_m, double y_m, double z_m) noexcept
        : x(units::Meters(x_m)), y(units::Meters(y_m)), z(units::Meters(z_m)) {}

    // Conversion constructor from Point3D (raw values in meters)
    constexpr explicit Position3D(const Point3D& pt) noexcept
        : x(units::Meters(pt.x)), y(units::Meters(pt.y)), z(units::Meters(pt.z)) {}

    [[nodiscard]] constexpr Point3D to_point3d() const noexcept {
        return Point3D(x.value(), y.value(), z.value());
    }

    constexpr operator Point3D() const noexcept {
        return to_point3d();
    }

    // Arithmetic operators
    constexpr Position3D operator+(const Position3D& other) const noexcept {
        return Position3D(x + other.x, y + other.y, z + other.z);
    }

    constexpr Position3D operator-(const Position3D& other) const noexcept {
        return Position3D(x - other.x, y - other.y, z - other.z);
    }

    constexpr Position3D operator-() const noexcept {
        return Position3D(-x, -y, -z);
    }

    constexpr Position3D& operator+=(const Position3D& other) noexcept {
        x += other.x; y += other.y; z += other.z;
        return *this;
    }

    constexpr Position3D& operator-=(const Position3D& other) noexcept {
        x -= other.x; y -= other.y; z -= other.z;
        return *this;
    }

    constexpr Position3D operator*(double scalar) const noexcept {
        return Position3D(x * scalar, y * scalar, z * scalar);
    }

    friend constexpr Position3D operator*(double scalar, const Position3D& p) noexcept {
        return p * scalar;
    }

    constexpr Position3D operator/(double scalar) const noexcept {
        if (scalar != 0.0) {
            return Position3D(x / scalar, y / scalar, z / scalar);
        }
        return Position3D(units::Meters(0.0), units::Meters(0.0), units::Meters(0.0));
    }

    [[nodiscard]] units::Meters distance_to(const Position3D& other) const noexcept {
        const double dx = (x - other.x).value();
        const double dy = (y - other.y).value();
        const double dz = (z - other.z).value();
        return units::Meters(std::sqrt(dx * dx + dy * dy + dz * dz));
    }

    [[nodiscard]] constexpr bool operator==(const Position3D&) const noexcept = default;
};

static_assert(sizeof(Position3D) == 24, "Position3D must be exactly 24 bytes");
static_assert(std::is_standard_layout_v<Position3D>, "Position3D must be standard layout");
static_assert(std::is_trivially_copyable_v<Position3D>, "Position3D must be trivially copyable");

} // namespace mod3d

