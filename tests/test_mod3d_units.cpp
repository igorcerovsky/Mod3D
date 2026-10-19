#include <gtest/gtest.h>
#include "mod3d/Units.h"
#include "mod3d/Body.h"
#include "mod3d/Facet3Pt.h"
#include "mod3d/ColumnPoint.h"
#include "mod3d/Model.h"
#include "mod3d/Observation.h"
#include "mod3d/Grid.h"
#include "mod3d/Magnetics.h"
#include <cmath>
#include <type_traits>

using namespace mod3d;
using namespace mod3d::units;
using namespace mod3d::literals;

// ============================================================================
// 1. Compile-Time Dimensional Safety Verifications
// ============================================================================

TEST(Mod3DUnitsSafety, CompileTimeDimensionalTypeSafety) {
    // 1. Ensure quantities have zero overhead over double
    static_assert(sizeof(Length) == sizeof(double));
    static_assert(sizeof(Density) == sizeof(double));
    static_assert(sizeof(GravityField) == sizeof(double));
    static_assert(sizeof(GradientTensor) == sizeof(double));
    static_assert(sizeof(MagneticField) == sizeof(double));

    // 2. Ensure dimensional incompatibility is caught by concepts
    static_assert(!units::SameDimension<units::tag::Meters, units::tag::KgPerM3>);
    static_assert(!units::SameDimension<units::tag::MilliGal, units::tag::Eotvos>);
    static_assert(!units::SameDimension<units::tag::NanoTesla, units::tag::Meters>);
    static_assert(!units::SameDimension<units::tag::Degrees, units::tag::Seconds>);

    // 3. Dimensional arithmetic relationships
    constexpr Density rho = 2670.0_kg_m3;
    constexpr Volume vol = 1000.0_m3;
    constexpr auto mass = rho * vol;
    static_assert(std::is_same_v<std::decay_t<decltype(mass)>, units::Kilograms>);
    EXPECT_DOUBLE_EQ(mass.value(), 2670000.0);

    constexpr Acceleration acc = 1.0_ms2;
    constexpr Length len = 1000.0_m;
    constexpr auto grad = acc / len;
    static_assert(std::is_same_v<std::decay_t<decltype(grad)>, units::PerSecondSquared>);
    EXPECT_DOUBLE_EQ(grad.value(), 0.001);

    SUCCEED();
}

// ============================================================================
// 2. Body & Density Units Safety
// ============================================================================

TEST(Mod3DUnitsSafety, BodyDensityUnitsSafetyAndConversions) {
    // Default constructor
    Body b1(1, "Granite Intrusion", 2750.0);
    EXPECT_DOUBLE_EQ(b1.density_qty().value(), 2750.0);
    EXPECT_DOUBLE_EQ(b1.density_qty().in<GPerCm3>(), 2.75);

    // Construct with unit literal in g/cm3
    Body b2(2, "Diabase Dike", 2.95_g_cm3);
    EXPECT_DOUBLE_EQ(b2.raw_density(), 2950.0);
    EXPECT_DOUBLE_EQ(b2.density_qty().value(), 2950.0);

    // Update density using g/cm3 and kg/m3
    b1.set_density(3.1_g_cm3);
    EXPECT_DOUBLE_EQ(b1.raw_density(), 3100.0);
    EXPECT_DOUBLE_EQ(b1.density_qty().value(), 3100.0);

    b1.set_density(2650.0_kg_m3);
    EXPECT_DOUBLE_EQ(b1.raw_density(), 2650.0);

    // Inactive body returns 0.0 density contrast
    b1.set_active(false);
    EXPECT_DOUBLE_EQ(b1.density_qty().value(), 0.0);
    EXPECT_DOUBLE_EQ(b1.raw_density_qty().value(), 2650.0);
    b1.set_active(true);

    // Density gradient evaluation with units
    b1.set_density_origin(Point3D(0.0_m, 0.0_m, 0.0_m));
    b1.set_density_gradient(Point3D(0.0, 0.0, -0.05)); // -0.05 kg/m3 per meter depth
    Point3D depthPt(0.0_m, 0.0_m, -1000.0_m);
    EXPECT_DOUBLE_EQ(b1.density_at_qty(depthPt).value(), 2700.0); // 2650 - (-0.05 * -1000) = 2650 + 50
}

TEST(Mod3DUnitsSafety, BodyMagneticsWithUnits) {
    Body b(1, "Magnetite Skarn");
    b.set_susceptibility(0.08); // 0.08 SI

    // Compute inducing magnetization using unit-aware angles and field intensity
    b.compute_magnetization_vector(65.0_deg, 12.0_deg, 48500.0_nT);
    const Point3D mv = b.magnetization_vector();

    // Verify consistency with ambient_field
    Point3D af = ambient_field(65.0_deg, 12.0_deg, 48500.0_nT);
    Point3D expected_mv = af * (0.08 / (4.0 * std::numbers::pi_v<double>));
    EXPECT_NEAR(mv.x, expected_mv.x, 1e-9);
    EXPECT_NEAR(mv.y, expected_mv.y, 1e-9);
    EXPECT_NEAR(mv.z, expected_mv.z, 1e-9);
}

// ============================================================================
// 3. ColumnPoint Units Safety
// ============================================================================

TEST(Mod3DUnitsSafety, ColumnPointUnitsSafety) {
    // Construct with length literals
    ColumnPoint cp1(100.0_m, 200.0_m, -500.0_m, 5);
    EXPECT_EQ(cp1.x(), 100.0_m);
    EXPECT_EQ(cp1.y(), 200.0_m);
    EXPECT_EQ(cp1.z(), -500.0_m);

    EXPECT_DOUBLE_EQ(cp1.x().value(), 100.0);
    EXPECT_DOUBLE_EQ(cp1.y().value(), 200.0);
    EXPECT_DOUBLE_EQ(cp1.z().value(), -500.0);

    EXPECT_DOUBLE_EQ(cp1.x().in<Kilometers>(), 0.1);
    EXPECT_DOUBLE_EQ(cp1.z().in<Kilometers>(), -0.5);

    // Position3D structured access
    EXPECT_EQ(cp1.position().x, 100.0_m);
    EXPECT_EQ(cp1.position().y, 200.0_m);
    EXPECT_EQ(cp1.position().z, -500.0_m);

    // Mutate with units in kilometers and meters
    cp1.set_z(-1.2_km);
    EXPECT_EQ(cp1.z(), -1200.0_m);
    EXPECT_DOUBLE_EQ(cp1.z().value(), -1200.0);

    cp1.set_coords(0.5_km, 1.5_km, -800.0_m);
    EXPECT_EQ(cp1.x(), 500.0_m);
    EXPECT_EQ(cp1.y(), 1500.0_m);
    EXPECT_EQ(cp1.z(), -800.0_m);

    // Relative vertical position queries
    ColumnPoint cp2(500.0_m, 1500.0_m, -200.0_m, 5);
    EXPECT_TRUE(cp2.is_above(cp1));
    EXPECT_TRUE(cp1.is_below(cp2));
}

// ============================================================================
// 4. Facet3Pt Units Safety & Potential Field Evaluation
// ============================================================================

TEST(Mod3DUnitsSafety, FacetGeometryAndFieldEvaluationUnitsSafety) {
    // Construct a triangular facet in the XY plane at z = -100 m
    // Triangle: (0,0,-100), (100,0,-100), (0,100,-100)
    Point3D p0(0.0_m, 0.0_m, -100.0_m);
    Point3D p1(100.0_m, 0.0_m, -100.0_m);
    Point3D p2(0.0_m, 100.0_m, -100.0_m);

    Body body(1, "Prism Body", 2670.0_kg_m3);
    Facet3Pt fct(p0, p1, p2, &body);

    // Geometric queries
    EXPECT_DOUBLE_EQ(fct.mean_elevation_qty().value(), -100.0);
    EXPECT_DOUBLE_EQ(fct.area_qty().value(), 5000.0); // 0.5 * 100 * 100
    EXPECT_DOUBLE_EQ(fct.density_qty().value(), 2670.0);

    // Gravity field evaluation at observation station (50, 50, 0)
    Point3D station(50.0_m, 50.0_m, 0.0_m);
    units::SI_Acceleration gz_si = fct.field_gz_si(station);
    units::MilliGal gz_mgal = fct.field_gz_mgal(station);
    units::GravityUnit gz_gu = fct.field_gz_gu(station);

    // Conversion verification: 1 m/s^2 = 10^5 mGal = 10^6 g.u.
    EXPECT_NEAR(gz_mgal.value(), gz_si.value() * 1.0e5, 1e-9);
    EXPECT_NEAR(gz_gu.value(), gz_si.value() * 1.0e6, 1e-9);

    // Gravity gradient tensor in Eötvös
    GravityGradientTensor tens = fct.gravity_gradient_tensor(station, 2670.0_kg_m3);
    // In source-free free air above facet, Laplace equation holds: trace = 0
    EXPECT_NEAR(tens.trace().value(), 0.0, 1e-5);
}

// ============================================================================
// 5. Model Dimension & Coordinate Units Safety
// ============================================================================

TEST(Mod3DUnitsSafety, ModelDimensionsAndGridQuantities) {
    Model m;
    m.init(10, 15, 0.0_m, 0.0_m, 100.0_m, 150.0_m, -2000.0_m, 500.0_m);

    EXPECT_DOUBLE_EQ(m.x0_qty().value(), 0.0);
    EXPECT_DOUBLE_EQ(m.y0_qty().value(), 0.0);
    EXPECT_DOUBLE_EQ(m.dx_qty().value(), 100.0);
    EXPECT_DOUBLE_EQ(m.dy_qty().value(), 150.0);
    EXPECT_DOUBLE_EQ(m.z_min_qty().value(), -2000.0);
    EXPECT_DOUBLE_EQ(m.z_max_qty().value(), 500.0);

    // Default thickness query
    EXPECT_DOUBLE_EQ(m.default_thickness_qty().value(), 250.0); // (500 - (-2000)) / 10 = 250 m

    // Mutate vertical range with kilometer quantities
    m.set_z_min(-3.0_km);
    m.set_z_max(1.0_km);
    EXPECT_DOUBLE_EQ(m.z_min(), -3000.0);
    EXPECT_DOUBLE_EQ(m.z_max(), 1000.0);
    EXPECT_DOUBLE_EQ(m.default_thickness_qty().value(), 400.0);
}

// ============================================================================
// 6. ObservationSpace Units Safety
// ============================================================================

TEST(Mod3DUnitsSafety, ObservationSpaceParametersWithUnits) {
    ObservationSpace obs(20, 20, 0.0, 0.0, 50.0, 50.0, 15.0);

    EXPECT_DOUBLE_EQ(obs.dx_qty().value(), 50.0);
    EXPECT_DOUBLE_EQ(obs.dy_qty().value(), 50.0);
    EXPECT_DOUBLE_EQ(obs.rotation_deg_qty().value(), 15.0);

    // Observation height settings
    obs.set_gravity_observation(ObservationMode::SensorHeight, 150.0_m);
    EXPECT_DOUBLE_EQ(obs.gravity_height_qty().value(), 150.0);

    obs.set_magnetic_observation(ObservationMode::FlightElevation, 1.2_km);
    EXPECT_DOUBLE_EQ(obs.magnetic_height_qty().value(), 1200.0);

    // Ambient geomagnetic field
    obs.set_ambient_field_params(62.5_deg, 3.5_deg, 51200.0_nT);
    EXPECT_DOUBLE_EQ(obs.inclination_qty().value(), 62.5);
    EXPECT_DOUBLE_EQ(obs.declination_qty().value(), 3.5);
    EXPECT_DOUBLE_EQ(obs.magnetic_intensity_qty().value(), 51200.0);

    MagneticVector mv = obs.ambient_field_vector_qty();
    EXPECT_NEAR(mv.total().value(), 51200.0, 1e-6);

    // Reference density
    obs.set_reference_density(2.67_g_cm3);
    EXPECT_DOUBLE_EQ(obs.reference_density(), 2670.0);
    EXPECT_DOUBLE_EQ(obs.reference_density_qty().value(), 2670.0);
}

// ============================================================================
// 7. Grid Units Safety & Spatial Queries
// ============================================================================

TEST(Mod3DUnitsSafety, GridUnitsSafetyAndInterpolation) {
    // Construct 4x4 grid spanning [0, 300] x [0, 300] m
    Grid g(4, 4, 0.0_m, 0.0_m, 100.0_m, 100.0_m, 0.0_deg);

    EXPECT_DOUBLE_EQ(g.dx_qty().value(), 100.0);
    EXPECT_DOUBLE_EQ(g.dy_qty().value(), 100.0);
    EXPECT_DOUBLE_EQ(g.x_min_qty().value(), 0.0);
    EXPECT_DOUBLE_EQ(g.x_max_qty().value(), 300.0);

    // Populate planar slope: z = 0.5 * x + 0.25 * y
    for (size_t r = 0; r < 4; ++r) {
        for (size_t c = 0; c < 4; ++c) {
            g(r, c) = 0.5 * g.x(r, c) + 0.25 * g.y(r, c);
        }
    }

    // Spatial containment with units
    EXPECT_TRUE(g.contains(150.0_m, 150.0_m));
    EXPECT_FALSE(g.contains(500.0_m, 150.0_m));
    EXPECT_TRUE(g.contains(0.15_km, 0.15_km));

    // Bilinear interpolation with unit quantities
    double val = g.interpolate(150.0_m, 150.0_m);
    double expected = 0.5 * 150.0 + 0.25 * 150.0; // 75 + 37.5 = 112.5
    EXPECT_NEAR(val, expected, 1e-9);

    // Equivalent call in kilometers
    double val_km = g.interpolate(0.15_km, 0.15_km);
    EXPECT_NEAR(val_km, expected, 1e-9);
}
