#pragma once

#include "mod3d/Point3D.h"
#include "mod3d/Units.h"
#include <numbers>
#include <cmath>

namespace mod3d {

/**
 * @brief Computes the Earth's geomagnetic ambient field vector (in nT).
 *
 * Coordinate conventions:
 *  X = East  = intensity * cos(inclination) * sin(declination)
 *  Y = North = intensity * cos(inclination) * cos(declination)
 *  Z = Down  = -intensity * sin(inclination) (downward is negative Z in geophysical modeling)
 */
[[nodiscard]] Point3D ambient_field(double inclination_deg = 60.0,
                                    double declination_deg = 0.0,
                                    double intensity_nt = 50000.0) noexcept;

template <units::UnitType IncU, units::UnitType DecU, units::UnitType IntensityU>
    requires (units::SameDimension<IncU, units::tag::Degrees> &&
              units::SameDimension<DecU, units::tag::Degrees> &&
              units::SameDimension<IntensityU, units::tag::NanoTesla>)
[[nodiscard]] inline Point3D ambient_field(const units::Quantity<IncU>& inc,
                                           const units::Quantity<DecU>& dec,
                                           const units::Quantity<IntensityU>& intensity) noexcept {
    return ambient_field(inc.template in<units::tag::Degrees>(),
                         dec.template in<units::tag::Degrees>(),
                         intensity.template in<units::tag::NanoTesla>());
}

[[nodiscard]] inline MagneticVector ambient_field_vector_qty(double inclination_deg = 60.0,
                                                             double declination_deg = 0.0,
                                                             double intensity_nt = 50000.0) noexcept {
    Point3D af = ambient_field(inclination_deg, declination_deg, intensity_nt);
    return MagneticVector{
        .mx = units::NanoTesla(af.x),
        .my = units::NanoTesla(af.y),
        .mz = units::NanoTesla(af.z)
    };
}

template <units::UnitType IncU, units::UnitType DecU, units::UnitType IntensityU>
    requires (units::SameDimension<IncU, units::tag::Degrees> &&
              units::SameDimension<DecU, units::tag::Degrees> &&
              units::SameDimension<IntensityU, units::tag::NanoTesla>)
[[nodiscard]] inline MagneticVector ambient_field_vector_qty(const units::Quantity<IncU>& inc,
                                                             const units::Quantity<DecU>& dec,
                                                             const units::Quantity<IntensityU>& intensity) noexcept {
    return ambient_field_vector_qty(inc.template in<units::tag::Degrees>(),
                                    dec.template in<units::tag::Degrees>(),
                                    intensity.template in<units::tag::NanoTesla>());
}

/**
 * @brief Computes the net magnetization vector (induced + remanent) in A/m.
 */
[[nodiscard]] Point3D magnetization_vector(double susceptibility = 0.01,
                                            double inc_deg = 60.0,
                                            double dec_deg = 0.0,
                                            double intensity_nt = 50000.0,
                                            double rem_inc_deg = 0.0,
                                            double rem_dec_deg = 0.0,
                                            double rem_intensity = 0.0) noexcept;

template <units::UnitType IncU, units::UnitType DecU, units::UnitType IntensityU,
          units::UnitType RemIncU = units::tag::Degrees, units::UnitType RemDecU = units::tag::Degrees>
    requires (units::SameDimension<IncU, units::tag::Degrees> &&
              units::SameDimension<DecU, units::tag::Degrees> &&
              units::SameDimension<IntensityU, units::tag::NanoTesla> &&
              units::SameDimension<RemIncU, units::tag::Degrees> &&
              units::SameDimension<RemDecU, units::tag::Degrees>)
[[nodiscard]] inline Point3D magnetization_vector(double susceptibility,
                                                  const units::Quantity<IncU>& inc,
                                                  const units::Quantity<DecU>& dec,
                                                  const units::Quantity<IntensityU>& intensity,
                                                  const units::Quantity<RemIncU>& rem_inc = units::Degrees(0.0),
                                                  const units::Quantity<RemDecU>& rem_dec = units::Degrees(0.0),
                                                  double rem_intensity = 0.0) noexcept {
    return magnetization_vector(susceptibility,
                                inc.template in<units::tag::Degrees>(),
                                dec.template in<units::tag::Degrees>(),
                                intensity.template in<units::tag::NanoTesla>(),
                                rem_inc.template in<units::tag::Degrees>(),
                                rem_dec.template in<units::tag::Degrees>(),
                                rem_intensity);
}

/**
 * @brief Computes total field magnetic anomaly delta T: |B_0 + dB| - |B_0|.
 */
[[nodiscard]] double mag_delta_t(const Point3D &anomalous_b,
                                 const Point3D &ambient_b,
                                 double ambient_intensity) noexcept;

[[nodiscard]] double mag_delta_t(double mx, double my, double mz,
                                 double afx, double afy, double afz,
                                 double ambient_intensity) noexcept;

template <units::UnitType IntensityU>
    requires units::SameDimension<IntensityU, units::tag::NanoTesla>
[[nodiscard]] inline units::NanoTesla mag_delta_t_qty(const Point3D &anomalous_b,
                                                      const Point3D &ambient_b,
                                                      const units::Quantity<IntensityU>& ambient_intensity) noexcept {
    return units::NanoTesla(mag_delta_t(anomalous_b, ambient_b, ambient_intensity.template in<units::tag::NanoTesla>()));
}

} // namespace mod3d

