#include "mod3d/Magnetics.h"
#include <numbers>
#include <cmath>

namespace mod3d {

Point3D ambient_field(double inclination_deg, double declination_deg, double intensity_nt) noexcept
{
    constexpr double deg2rad = std::numbers::pi_v<double> / 180.0;
    const double inc_rad = inclination_deg * deg2rad;
    const double dec_rad = declination_deg * deg2rad;

    Point3D af;
    af.x = std::cos(inc_rad) * std::sin(dec_rad) * intensity_nt;
    af.y = std::cos(inc_rad) * std::cos(dec_rad) * intensity_nt;
    af.z = -std::sin(inc_rad) * intensity_nt;
    return af;
}

Point3D magnetization_vector(double susceptibility,
                             double inc_deg,
                             double dec_deg,
                             double intensity_nt,
                             double rem_inc_deg,
                             double rem_dec_deg,
                             double rem_intensity) noexcept
{
    constexpr double deg2rad = std::numbers::pi_v<double> / 180.0;
    constexpr double four_pi = 4.0 * std::numbers::pi_v<double>;

    const Point3D vH = ambient_field(inc_deg, dec_deg, intensity_nt);
    const Point3D vI = vH * (susceptibility / four_pi);

    const double rem_inc_rad = rem_inc_deg * deg2rad;
    const double rem_dec_rad = rem_dec_deg * deg2rad;

    Point3D vR;
    vR.x = std::cos(rem_inc_rad) * std::cos(rem_dec_rad) * rem_intensity;
    vR.y = std::cos(rem_inc_rad) * std::sin(rem_dec_rad) * rem_intensity;
    vR.z = -std::sin(rem_inc_rad) * rem_intensity;

    return vI + vR;
}

double mag_delta_t(const Point3D &anomalous_b, const Point3D &ambient_b, double ambient_intensity) noexcept
{
    const double dx = anomalous_b.x + ambient_b.x;
    const double dy = anomalous_b.y + ambient_b.y;
    const double dz = anomalous_b.z + ambient_b.z;
    const double total_mag = std::sqrt(dx * dx + dy * dy + dz * dz);
    return total_mag - ambient_intensity;
}

double mag_delta_t(double mx, double my, double mz,
                   double afx, double afy, double afz,
                   double ambient_intensity) noexcept
{
    const double dx = mx + afx;
    const double dy = my + afy;
    const double dz = mz + afz;
    const double total_mag = std::sqrt(dx * dx + dy * dy + dz * dz);
    return total_mag - ambient_intensity;
}

} // namespace mod3d
