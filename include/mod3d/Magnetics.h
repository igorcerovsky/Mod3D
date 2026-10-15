#pragma once

#include "mod3d/Point3D.h"
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

/**
 * @brief Computes total field magnetic anomaly delta T: |B_0 + dB| - |B_0|.
 */
[[nodiscard]] double mag_delta_t(const Point3D &anomalous_b,
                                 const Point3D &ambient_b,
                                 double ambient_intensity) noexcept;

[[nodiscard]] double mag_delta_t(double mx, double my, double mz,
                                 double afx, double afy, double afz,
                                 double ambient_intensity) noexcept;

// Compatibility aliases
[[nodiscard]] inline Point3D AmbientField(double iI = 60.0, double iD = 0.0, double iS = 50000.0) noexcept {
    return ambient_field(iI, iD, iS);
}

[[nodiscard]] inline Point3D MagnetizationVector(double susc, double iI, double iD, double iS,
                                                 double rI = 0.0, double rD = 0.0, double rS = 0.0) noexcept {
    return magnetization_vector(susc, iI, iD, iS, rI, rD, rS);
}

[[nodiscard]] inline double Mag_dT(const Point3D &v_mag, const Point3D &v_af, double hInt) noexcept {
    return mag_delta_t(v_mag, v_af, hInt);
}

[[nodiscard]] inline double Mag_dT(double mx, double my, double mz,
                                   double afx, double afy, double afz, double hInt) noexcept {
    return mag_delta_t(mx, my, mz, afx, afy, afz, hInt);
}

} // namespace mod3d
