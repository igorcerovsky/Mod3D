#include <gtest/gtest.h>
#include "mod3d/Units.h"
#include <sstream>

using namespace mod3d::units;
using namespace mod3d::units::literals;

// ============================================================================
// 1. Static and Compile-Time Traits
// ============================================================================

TEST(UnitsTest, ZeroOverheadAndTriviallyCopyable) {
    static_assert(sizeof(Meters) == sizeof(double));
    static_assert(sizeof(Kilometers) == sizeof(double));
    static_assert(sizeof(MGal) == sizeof(double));
    static_assert(sizeof(Eotvos) == sizeof(double));
    static_assert(sizeof(NanoTesla) == sizeof(double));
    static_assert(sizeof(KgPerM3) == sizeof(double));

    static_assert(std::is_trivially_copyable_v<Meters>);
    static_assert(std::is_standard_layout_v<Meters>);
    static_assert(std::is_default_constructible_v<Meters>);

    // Verify constexpr evaluations
    constexpr Meters m1{1000.0};
    constexpr Kilometers km1{m1};
    static_assert(km1.value() == 1.0);

    constexpr auto sum = 1000.0_m + 2.0_km;
    static_assert(sum.value() == 3000.0);

    constexpr auto diff = 5.0_km - 2000.0_m;
    static_assert(diff.value() == 3.0);

    constexpr double ratio = 2.0_km / 500.0_m;
    static_assert(ratio == 4.0);

    SUCCEED();
}

// ============================================================================
// 2. Length Conversions and Arithmetic
// ============================================================================

TEST(UnitsTest, LengthConversionsAndLiterals) {
    auto km = 2.5_km;
    EXPECT_DOUBLE_EQ(km.value(), 2.5);
    EXPECT_DOUBLE_EQ(km.to_base(), 2500.0);
    EXPECT_DOUBLE_EQ(km.in<Meters>(), 2500.0);
    EXPECT_DOUBLE_EQ(km.in<Centimeters>(), 250000.0);
    EXPECT_DOUBLE_EQ(km.in<Millimeters>(), 2500000.0);

    Meters m = km; // Implicit conversion between same dimension
    EXPECT_DOUBLE_EQ(m.value(), 2500.0);

    Feet ft = 1.0_ft;
    EXPECT_DOUBLE_EQ(ft.in<Meters>(), 0.3048);

    // Integer literals support
    auto m_int = 10_m;
    EXPECT_DOUBLE_EQ(m_int.value(), 10.0);
}

TEST(UnitsTest, LengthArithmetic) {
    auto total_m = 500_m + 1.5_km;
    EXPECT_DOUBLE_EQ(total_m.value(), 2000.0);
    EXPECT_STREQ(total_m.unit_symbol().data(), "m");

    auto total_km = 1.5_km + 500_m;
    EXPECT_DOUBLE_EQ(total_km.value(), 2.0);
    EXPECT_STREQ(total_km.unit_symbol().data(), "km");

    auto diff = 1_km - 250_m;
    EXPECT_DOUBLE_EQ(diff.value(), 0.75);

    auto scaled = 2_km * 3.5;
    EXPECT_DOUBLE_EQ(scaled.value(), 7.0);

    auto scaled_left = 3.5 * 2_km;
    EXPECT_DOUBLE_EQ(scaled_left.value(), 7.0);

    auto divided = 10_km / 4.0;
    EXPECT_DOUBLE_EQ(divided.value(), 2.5);

    // Ratio returns dimensionless double
    double count = 5_km / 250_m;
    EXPECT_DOUBLE_EQ(count, 20.0);
}

// ============================================================================
// 3. Angle Conversions, Arithmetic, and Trigonometry
// ============================================================================

TEST(UnitsTest, AngleConversionsAndTrig) {
    auto deg = 180.0_deg;
    EXPECT_NEAR(deg.in<Radians>(), std::numbers::pi_v<double>, 1e-12);

    auto rad = Radians(std::numbers::pi_v<double> / 2.0);
    EXPECT_NEAR(rad.in<Degrees>(), 90.0, 1e-12);

    EXPECT_NEAR(sin(90.0_deg), 1.0, 1e-12);
    EXPECT_NEAR(sin(0.0_deg), 0.0, 1e-12);
    EXPECT_NEAR(cos(0.0_deg), 1.0, 1e-12);
    EXPECT_NEAR(cos(90.0_deg), 0.0, 1e-12);
    EXPECT_NEAR(tan(45.0_deg), 1.0, 1e-12);

    EXPECT_NEAR(asin_rad(1.0).in<Degrees>(), 90.0, 1e-12);
    EXPECT_NEAR(asin_deg(1.0).value(), 90.0, 1e-12);
    EXPECT_NEAR(acos_deg(1.0).value(), 0.0, 1e-12);
    EXPECT_NEAR(atan2_deg(1.0, 1.0).value(), 45.0, 1e-12);

    // ArcMinutes and ArcSeconds
    auto one_deg = 60.0_arcmin;
    EXPECT_NEAR(one_deg.in<Degrees>(), 1.0, 1e-12);

    auto arcsec = 3600.0_arcsec;
    EXPECT_NEAR(arcsec.in<Degrees>(), 1.0, 1e-12);
}

// ============================================================================
// 4. Density Conversions
// ============================================================================

TEST(UnitsTest, DensityConversions) {
    auto crustal = 2670.0_kg_m3;
    EXPECT_DOUBLE_EQ(crustal.value(), 2670.0);
    EXPECT_DOUBLE_EQ(crustal.in<GPerCm3>(), 2.67);

    auto sediment = 2.2_g_cm3;
    EXPECT_DOUBLE_EQ(sediment.in<KgPerM3>(), 2200.0);

    GPerCm3 converted = crustal;
    EXPECT_DOUBLE_EQ(converted.value(), 2.67);
}

// ============================================================================
// 5. Geophysical Gravity Acceleration (mGal, g.u., µGal, m/s²)
// ============================================================================

TEST(UnitsTest, GravityAccelerationConversions) {
    // 1 mGal = 10^-5 m/s^2
    auto mgal = 1.0_mgal;
    EXPECT_DOUBLE_EQ(mgal.to_base(), 1.0e-5);

    // 1 mGal = 10 g.u. = 1000 µGal
    EXPECT_DOUBLE_EQ(mgal.in<GU>(), 10.0);
    EXPECT_DOUBLE_EQ(mgal.in<MicroGal>(), 1000.0);
    EXPECT_DOUBLE_EQ(mgal.in<Gal>(), 0.001);

    // 1 g.u. = 0.1 mGal = 10^-6 m/s^2
    auto gu = 1.0_gu;
    EXPECT_DOUBLE_EQ(gu.to_base(), 1.0e-6);
    EXPECT_DOUBLE_EQ(gu.in<MilliGal>(), 0.1);

    // Arithmetic between mGal and g.u.
    auto total = 12.5_mgal + 25.0_gu;
    EXPECT_DOUBLE_EQ(total.value(), 15.0); // 12.5 + 2.5 mGal
    EXPECT_STREQ(total.unit_symbol().data(), "mGal");

    auto diff = 100.0_gu - 5.0_mgal;
    EXPECT_DOUBLE_EQ(diff.value(), 50.0); // 100 - 50 g.u.
    EXPECT_STREQ(diff.unit_symbol().data(), "g.u.");
}

// ============================================================================
// 6. Gravity Gradient Tensor (Eötvös / s⁻²)
// ============================================================================

TEST(UnitsTest, GravityGradientTensor) {
    auto eotvos = 15.0_E;
    EXPECT_DOUBLE_EQ(eotvos.value(), 15.0);
    EXPECT_DOUBLE_EQ(eotvos.to_base(), 15.0e-9);

    auto si_grad = 1.0e-9_s2;
    EXPECT_DOUBLE_EQ(si_grad.in<Eotvos>(), 1.0);

    Eotvos converted = si_grad;
    EXPECT_DOUBLE_EQ(converted.value(), 1.0);
}

// ============================================================================
// 7. Magnetic Field (Tesla, nT, µT)
// ============================================================================

TEST(UnitsTest, MagneticField) {
    auto ambient = 50000.0_nT;
    EXPECT_DOUBLE_EQ(ambient.value(), 50000.0);
    EXPECT_DOUBLE_EQ(ambient.in<MicroTesla>(), 50.0);
    EXPECT_DOUBLE_EQ(ambient.to_base(), 5.0e-5); // 50 µT = 5e-5 T

    auto gamma = 50000.0_gamma;
    EXPECT_DOUBLE_EQ(gamma.in<NanoTesla>(), 50000.0);

    auto tesla = 1.0_T;
    EXPECT_DOUBLE_EQ(tesla.in<NanoTesla>(), 1.0e9);
    EXPECT_DOUBLE_EQ(tesla.in<MilliTesla>(), 1000.0);
}

// ============================================================================
// 8. Cross-Dimensional Geophysical Operations
// ============================================================================

TEST(UnitsTest, CrossDimensionalFormulas) {
    // 1) Acceleration / Length = Gradient (Tensor)
    // 10 mGal over 1 km = 10^-4 m/s^2 / 1000 m = 10^-7 s^-2 = 100 Eötvös
    auto grad = 10.0_mgal / 1.0_km;
    EXPECT_DOUBLE_EQ(grad.in<Eotvos>(), 100.0);

    // Standard textbook relation: 1 Eötvös = 0.1 mGal / km
    auto e1 = 0.1_mgal / 1.0_km;
    EXPECT_NEAR(e1.in<Eotvos>(), 1.0, 1e-12);

    // 2) Gradient * Length = Acceleration
    auto accel = 100.0_E * 1.0_km;
    EXPECT_DOUBLE_EQ(accel.in<MGal>(), 10.0);

    // 3) Geometry: Length * Length = Area, Area * Length = Volume
    auto area = 10.0_m * 20.0_m;
    EXPECT_DOUBLE_EQ(area.in<SquareMeters>(), 200.0);

    auto vol = area * 5.0_m;
    EXPECT_DOUBLE_EQ(vol.in<CubicMeters>(), 1000.0);

    auto l = vol / area;
    EXPECT_DOUBLE_EQ(l.in<Meters>(), 5.0);

    // 4) Mass, Volume, Density
    // Mass = Volume * Density
    auto mass = 2.0_m3 * 2670.0_kg_m3;
    EXPECT_DOUBLE_EQ(mass.in<Kilograms>(), 5340.0);
    EXPECT_DOUBLE_EQ(mass.in<Tonnes>(), 5.34);

    // Density = Mass / Volume
    auto rho = 5340.0_kg / 2.0_m3;
    EXPECT_DOUBLE_EQ(rho.in<KgPerM3>(), 2670.0);

    // Volume = Mass / Density
    auto v_rec = 5340.0_kg / 2670.0_kg_m3;
    EXPECT_DOUBLE_EQ(v_rec.in<CubicMeters>(), 2.0);

    // Density Gradient: Density / Length = DensityGradient (kg/m4)
    auto dens_grad = 200.0_kg_m3 / 1000.0_m;
    EXPECT_DOUBLE_EQ(dens_grad.in<KgPerM4>(), 0.2);
    EXPECT_EQ(dens_grad, 0.2_kg_m4);

    // Delta Density: DensityGradient * Length = Density (kg/m3)
    auto delta_rho = dens_grad * 500.0_m;
    EXPECT_DOUBLE_EQ(delta_rho.in<KgPerM3>(), 100.0);
    EXPECT_EQ(delta_rho, 100.0_kg_m3);

    // Commutative Length * DensityGradient = Density
    auto delta_rho_comm = 500.0_m * 0.2_kg_m4;
    EXPECT_DOUBLE_EQ(delta_rho_comm.in<KgPerM3>(), 100.0);
    EXPECT_EQ(delta_rho_comm, 100.0_kg_m3);

    // 5) Kinematics: Length / Time = Velocity
    auto vel = 100.0_m / 10.0_s;
    EXPECT_DOUBLE_EQ(vel.in<MetersPerSecond>(), 10.0);
    EXPECT_DOUBLE_EQ(vel.in<KilometersPerHour>(), 36.0);

    auto dist = vel * 5.0_s;
    EXPECT_DOUBLE_EQ(dist.in<Meters>(), 50.0);
}

// ============================================================================
// 9. Comparison and Approximate Equality
// ============================================================================

TEST(UnitsTest, Comparisons) {
    EXPECT_TRUE(1.0_km == 1000.0_m);
    EXPECT_TRUE(1000.0_m == 1.0_km);
    EXPECT_TRUE(500.0_m < 1.0_km);
    EXPECT_TRUE(1.0_km > 500.0_m);
    EXPECT_TRUE(1.0_km <= 1000.0_m);
    EXPECT_TRUE(1.0_km >= 1000.0_m);
    EXPECT_TRUE(approx_equal(1.0_mgal, 10.0_gu));
    EXPECT_FALSE(approx_equal(1.0_mgal, 5.0_gu));

    // Approx equal
    auto a = 180.0_deg;
    auto b = Radians(std::numbers::pi_v<double>);
    EXPECT_TRUE(approx_equal(a, b));

    auto c = 10.0000000001_m;
    auto d = 10.0_m;
    EXPECT_TRUE(approx_equal(c, d, 1e-8));

    // Min and max
    auto min_l = min(500.0_m, 1.0_km);
    EXPECT_DOUBLE_EQ(min_l.value(), 500.0);

    auto max_l = max(500.0_m, 1.0_km);
    EXPECT_DOUBLE_EQ(max_l.value(), 1000.0);

    // Abs
    auto neg = -25.0_mgal;
    EXPECT_DOUBLE_EQ(abs(neg).value(), 25.0);
}

// ============================================================================
// 10. Stream Formatting
// ============================================================================

TEST(UnitsTest, StreamFormatting) {
    {
        std::ostringstream oss;
        oss << 5.0_km;
        EXPECT_EQ(oss.str(), "5 km");
    }
    {
        std::ostringstream oss;
        oss << 12.5_mgal;
        EXPECT_EQ(oss.str(), "12.5 mGal");
    }
    {
        std::ostringstream oss;
        oss << 2670.0_kg_m3;
        EXPECT_EQ(oss.str(), "2670 kg/m³");
    }
    {
        std::ostringstream oss;
        oss << 15.0_E;
        EXPECT_EQ(oss.str(), "15 E");
    }
    {
        std::ostringstream oss;
        oss << 50000.0_nT;
        EXPECT_EQ(oss.str(), "50000 nT");
    }
    {
        std::ostringstream oss;
        oss << 60.0_deg;
        EXPECT_EQ(oss.str(), "60 deg");
    }
}

// ============================================================================
// 11. Geophysical Constants Verification
// ============================================================================

TEST(UnitsTest, Constants) {
    EXPECT_DOUBLE_EQ(constants::SI_TO_MGAL, 1.0e5);
    EXPECT_DOUBLE_EQ(constants::SI_TO_GU, 1.0e6);
    EXPECT_DOUBLE_EQ(constants::SI_TO_EOTVOS, 1.0e9);
    EXPECT_DOUBLE_EQ(constants::SI_TO_NT, 1.0e9);
    EXPECT_DOUBLE_EQ(constants::G, 6.67430e-11);
    EXPECT_NEAR(constants::MU_0, 1.2566370614359173e-6, 1e-15);
}
