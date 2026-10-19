#pragma once

#include "units/units.hpp"
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

} // namespace mod3d

