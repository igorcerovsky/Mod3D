#include <gtest/gtest.h>
#include "mod3d/Point3D.h"
#include "mod3d/Body.h"
#include "mod3d/ColumnPoint.h"
#include "mod3d/Facet3Pt.h"
#include <cmath>
#include <array>
#include <span>
#include <type_traits>

using namespace mod3d;

// ============================================================================
// 1. Point3D Edge Cases & Extended Math Tests
// ============================================================================

TEST(Point3DTest, ZeroVectorAndDivisionByZero) {
    Point3D zero(0.0, 0.0, 0.0);
    EXPECT_TRUE(zero.IsZero());

    // Normalizing a zero vector should remain zero without crashing or producing NaNs
    zero.Unit();
    EXPECT_TRUE(zero.IsZero());
    EXPECT_FALSE(std::isnan(zero.x));
    EXPECT_FALSE(std::isnan(zero.y));
    EXPECT_FALSE(std::isnan(zero.z));

    // Division by zero scalar must return zero vector safely
    Point3D pt(10.0, 20.0, 30.0);
    Point3D divZero = pt / 0.0;
    EXPECT_TRUE(divZero.IsZero());
}

TEST(Point3DTest, UtilityMethods) {
    Point3D pt(-5.0, 10.0, -15.0);
    EXPECT_FALSE(pt.IsZero());

    // Positive
    pt.Positive();
    EXPECT_EQ(pt, Point3D(5.0, 10.0, 15.0));

    // TurnSign
    pt.TurnSign();
    EXPECT_EQ(pt, Point3D(-5.0, -10.0, -15.0));

    // Zero
    pt.Zero();
    EXPECT_TRUE(pt.IsZero());
    EXPECT_DOUBLE_EQ(pt.Abs(), 0.0);
}

TEST(Point3DTest, DistanceAndAngle) {
    Point3D p1(0.0, 0.0, 0.0);
    Point3D p2(3.0, 4.0, 0.0);

    // Distance in 2D plane (3-4-5 triangle)
    EXPECT_DOUBLE_EQ(p1.Distance(p2), 5.0);
    EXPECT_DOUBLE_EQ(p2.Distance(p1), 5.0);

    // Angle between perpendicular unit vectors
    Point3D ux(1.0, 0.0, 0.0);
    Point3D uy(0.0, 1.0, 0.0);
    const double piHalf = 3.14159265358979323846 / 2.0;
    EXPECT_NEAR(ux.Angle(uy), piHalf, 1e-9);

    // Angle between parallel vectors (0) and anti-parallel vectors (pi)
    EXPECT_NEAR(ux.Angle(ux), 0.0, 1e-9);
    Point3D negUx(-1.0, 0.0, 0.0);
    EXPECT_NEAR(ux.Angle(negUx), 3.14159265358979323846, 1e-9);
}

// ============================================================================
// 2. ColumnPoint Unit Tests
// ============================================================================

TEST(ColumnPointTest, ConstructorsAndDefaults) {
    // Compile-time layout guarantees
    static_assert(std::is_standard_layout_v<ColumnPoint>, "ColumnPoint must be standard layout");
    static_assert(std::is_trivially_copyable_v<ColumnPoint>, "ColumnPoint must be trivially copyable");

    // Compile-time constexpr evaluation
    constexpr ColumnPoint cpConst(Point3D(100.0, 200.0, -500.0), 7);
    static_assert(cpConst.z() == -500.0);
    static_assert(cpConst.x() == 100.0);
    static_assert(cpConst.y() == 200.0);
    static_assert(cpConst.bodyId() == 7);
    static_assert(cpConst.isModified());

    ColumnPoint cpDefault;
    EXPECT_DOUBLE_EQ(cpDefault.z(), 0.0);
    EXPECT_EQ(cpDefault.bodyId(), -1);
    EXPECT_EQ(cpDefault.body(), nullptr);
    EXPECT_TRUE(cpDefault.isModified());
    EXPECT_FALSE(cpDefault.has_body());

    ColumnPoint cpZ(-450.0);
    EXPECT_DOUBLE_EQ(cpZ.z(), -450.0);
    EXPECT_DOUBLE_EQ(cpZ.point().z, -450.0);

    Body testBody(42, "Basement", 2800.0);
    ColumnPoint cpBody(42, -600.0, &testBody);
    EXPECT_EQ(cpBody.bodyId(), 42);
    EXPECT_EQ(cpBody.body(), &testBody);
    EXPECT_DOUBLE_EQ(cpBody.z(), -600.0);
    EXPECT_TRUE(cpBody.has_body());

    // 5-arg coordinate constructor
    ColumnPoint cpCoords(10.0, 20.0, -30.0, 5, &testBody);
    EXPECT_DOUBLE_EQ(cpCoords.x(), 10.0);
    EXPECT_DOUBLE_EQ(cpCoords.y(), 20.0);
    EXPECT_DOUBLE_EQ(cpCoords.z(), -30.0);
    EXPECT_EQ(cpCoords.bodyId(), 5);
}

TEST(ColumnPointTest, SettersAndSync) {
    ColumnPoint cp;
    cp.setModified(false);
    EXPECT_FALSE(cp.isModified());

    // setZ must update both m_z and m_pt.z, and mark modified
    cp.setZ(-720.5);
    EXPECT_DOUBLE_EQ(cp.z(), -720.5);
    EXPECT_DOUBLE_EQ(cp.point().z, -720.5);
    EXPECT_TRUE(cp.isModified());

    // setPoint must update 3D coords and sync z
    cp.setModified(false);
    Point3D newPt(1200.0, 3400.0, -850.0);
    cp.setPoint(newPt);
    EXPECT_EQ(cp.point(), newPt);
    EXPECT_DOUBLE_EQ(cp.z(), -850.0);
    EXPECT_TRUE(cp.isModified());

    // set_coords and individual component setters
    cp.setModified(false);
    cp.set_coords(50.0, 60.0, -100.0);
    EXPECT_DOUBLE_EQ(cp.x(), 50.0);
    EXPECT_DOUBLE_EQ(cp.y(), 60.0);
    EXPECT_DOUBLE_EQ(cp.z(), -100.0);
    EXPECT_TRUE(cp.isModified());

    cp.set_x(75.0);
    cp.set_y(85.0);
    cp.set_z(-120.0);
    EXPECT_DOUBLE_EQ(cp.x(), 75.0);
    EXPECT_DOUBLE_EQ(cp.y(), 85.0);
    EXPECT_DOUBLE_EQ(cp.z(), -120.0);
}

TEST(ColumnPointTest, ComparisonsAndPredicates) {
    ColumnPoint pTop(Point3D(0.0, 0.0, -100.0), 1);
    ColumnPoint pBot(Point3D(0.0, 0.0, -300.0), 1);

    EXPECT_TRUE(pTop == pTop);
    EXPECT_FALSE(pTop == pBot);

    // Relative vertical position
    EXPECT_TRUE(pTop.is_above(pBot));
    EXPECT_FALSE(pTop.is_below(pBot));
    EXPECT_TRUE(pBot.is_below(pTop));

    // Sorting predicates
    ColumnPoint::DepthLess depthLess;
    ColumnPoint::DepthGreater depthGreater;
    EXPECT_TRUE(depthLess(pBot, pTop));      // -300 < -100
    EXPECT_TRUE(depthGreater(pTop, pBot));   // -100 > -300

    // Stream operator formatting
    std::ostringstream oss;
    oss << pTop;
    std::string str = oss.str();
    EXPECT_NE(str.find("ColumnPoint"), std::string::npos);
    EXPECT_NE(str.find("-100"), std::string::npos);
}

// ============================================================================
// 3. Body Unit Tests
// ============================================================================

TEST(BodyTest, PropertiesAndActivation) {
    Body b(1, "Granite Pluton", 2670.0);
    EXPECT_EQ(b.GetID(), 1);
    EXPECT_EQ(b.GetName(), "Granite Pluton");
    EXPECT_TRUE(b.IsActive());
    EXPECT_DOUBLE_EQ(b.GetDensity(), 2670.0);
    EXPECT_DOUBLE_EQ(b.GetRawDensity(), 2670.0);

    // When inactive, effective density for field computation becomes 0
    b.SetActive(false);
    EXPECT_FALSE(b.IsActive());
    EXPECT_DOUBLE_EQ(b.GetDensity(), 0.0);
    EXPECT_DOUBLE_EQ(b.GetRawDensity(), 2670.0); // Raw preserved

    // Re-activate
    b.SetActive(true);
    EXPECT_DOUBLE_EQ(b.GetDensity(), 2670.0);
}

TEST(BodyTest, LinearDensityGradientAtOrigin) {
    Body b(2, "Sedimentary Basin", 2400.0);
    // Gradient: +0.5 kg/m3 per meter depth (Z axis)
    Point3D grad(0.0, 0.0, 0.5);
    Point3D origo(0.0, 0.0, -1000.0);
    b.SetDensityGradient(grad);
    b.SetDensityOrigo(origo);

    // Density at reference origin (0, 0, 0):
    // rho0 = rho - grad * origo = 2400.0 - (0.5 * -1000.0) = 2400.0 - (-500.0) = 2900.0
    EXPECT_DOUBLE_EQ(b.GetDensityAtOrigin(), 2900.0);
}

TEST(BodyTest, MagnetizationVectorComputation) {
    Body b(3, "Basalt Intrusion", 2900.0);
    const double susc = 0.05; // SI susceptibility
    b.SetSusceptibility(susc);

    // Remanent magnetization: Inclination 45 deg, Declination 20 deg, Intensity 2.5 A/m
    const double rI = 45.0, rD = 20.0, rS = 2.5;
    Point3D remAngles(rI, rD, rS);
    b.SetRemanentMagnetization(remAngles);

    // Ambient geomagnetic field: Inclination 60 deg, Declination 10 deg, Intensity 50000 nT
    const double iI = 60.0, iD = 10.0, iS = 50000.0;
    Point3D ambAngles(iI, iD, iS);
    b.ComputeMagnetizationVector(ambAngles);

    // Analytical expectation:
    // vH = AmbientField(iI, iD, iS)
    const double pi = 3.14159265358979323846;
    const double radI = iI * pi / 180.0;
    const double radD = iD * pi / 180.0;
    Point3D vH(
        std::cos(radI) * std::sin(radD) * iS,
        std::cos(radI) * std::cos(radD) * iS,
        -std::sin(radI) * iS
    );
    Point3D vInduced = vH * (susc / (4.0 * pi));

    const double radRI = rI * pi / 180.0;
    const double radRD = rD * pi / 180.0;
    Point3D vRemanent(
        std::cos(radRI) * std::cos(radRD) * rS,
        std::cos(radRI) * std::sin(radRD) * rS,
        -std::sin(radRI) * rS
    );

    Point3D expectedM = vInduced + vRemanent;
    Point3D computedM = b.GetMagnetizationVector();

    EXPECT_NEAR(computedM.x, expectedM.x, 1e-12);
    EXPECT_NEAR(computedM.y, expectedM.y, 1e-12);
    EXPECT_NEAR(computedM.z, expectedM.z, 1e-12);
}

TEST(BodyTest, VisualAndLockStates) {
    Body b;
    EXPECT_TRUE(b.IsVisible());
    EXPECT_FALSE(b.IsLocked());
    EXPECT_TRUE(b.IsFilled());

    b.SetVisible(false);
    b.SetLocked(true);
    b.SetFilled(false);
    EXPECT_FALSE(b.IsVisible());
    EXPECT_TRUE(b.IsLocked());
    EXPECT_FALSE(b.IsFilled());

    BodyColor customColor{220, 100, 50, 200};
    b.SetColor(customColor);
    EXPECT_EQ(b.GetColor().r, 220);
    EXPECT_EQ(b.GetColor().g, 100);
    EXPECT_EQ(b.GetColor().b, 50);
    EXPECT_EQ(b.GetColor().a, 200);

    b.SetTransparency(0.8f);
    EXPECT_FLOAT_EQ(b.GetTransparency(), 0.8f);
}

// ============================================================================
// 4. Facet3Pt Modernized API and Geometry Tests
// ============================================================================

TEST(Facet3PtTest, NonPolymorphicAndSize) {
    // Ensure vtable overhead is removed
    static_assert(!std::is_polymorphic_v<Facet3Pt>, "Facet3Pt must not be polymorphic");

    Facet3Pt f;
    EXPECT_EQ(f.size(), 3);
    EXPECT_FALSE(f.empty());
    EXPECT_FALSE(f.is_null());
    EXPECT_FALSE(f.is_outer());
    EXPECT_EQ(f.type(), FacetType::FCT_NORMAL);
    EXPECT_DOUBLE_EQ(f.sign(), 1.0);
}

TEST(Facet3PtTest, ConstructorsAndAccessors) {
    Point3D p0(0.0, 0.0, 100.0);
    Point3D p1(10.0, 0.0, 100.0);
    Point3D p2(0.0, 10.0, 100.0);

    // Constructor with 3 points
    Facet3Pt f1(p0, p1, p2, 2670.0, 1000.0);
    EXPECT_EQ(f1[0], p0);
    EXPECT_EQ(f1[1], p1);
    EXPECT_EQ(f1[2], p2);
    EXPECT_EQ(f1.points()[0], p0);
    EXPECT_DOUBLE_EQ(f1.density, 2670.0);
    EXPECT_DOUBLE_EQ(f1.densityOpos, 1000.0);

    // Constructor with std::array
    std::array<Point3D, 3> arr{p0, p1, p2};
    Facet3Pt f2(arr);
    EXPECT_EQ(f2, f1);

    // Constructor with std::span
    std::span<const Point3D, 3> sp(arr);
    Facet3Pt f3(sp);
    EXPECT_EQ(f3, f1);
}

TEST(Facet3PtTest, GeometryNormalsAreaAndCentroids) {
    // Counter-clockwise horizontal triangle in XY plane at Z = 100
    Point3D p0(0.0, 0.0, 100.0);
    Point3D p1(10.0, 0.0, 100.0);
    Point3D p2(0.0, 10.0, 100.0);

    Facet3Pt f(p0, p1, p2);

    // Outward normal points up (+Z)
    Point3D n = f.normal();
    EXPECT_NEAR(n.x, 0.0, 1e-12);
    EXPECT_NEAR(n.y, 0.0, 1e-12);
    EXPECT_NEAR(n.z, 1.0, 1e-12);
    EXPECT_EQ(f.Normal(), f.normal());

    // Triangle area: base 10, height 10 => 0.5 * 10 * 10 = 50.0
    EXPECT_NEAR(f.area(), 50.0, 1e-12);

    // Mean elevation
    EXPECT_DOUBLE_EQ(f.mean_elevation(), 100.0);
    EXPECT_DOUBLE_EQ(f.GetMeanElevation(), 100.0);

    // True center (geometric centroid)
    Point3D c = f.center();
    EXPECT_NEAR(c.x, 10.0 / 3.0, 1e-12);
    EXPECT_NEAR(c.y, 10.0 / 3.0, 1e-12);
    EXPECT_NEAR(c.z, 100.0, 1e-12);

    // Legacy Centroid() (unnormalized sum: p0 + p1 + p2)
    Point3D legacyCntr = f.Centroid();
    EXPECT_EQ(legacyCntr, Point3D(10.0, 10.0, 300.0));
    EXPECT_EQ(f.centroid(), legacyCntr);

    // Vertex containment
    EXPECT_TRUE(f.contains_vertex(p0));
    EXPECT_TRUE(f.contains_vertex(p1));
    EXPECT_TRUE(f.contains_vertex(p2));
    EXPECT_FALSE(f.contains_vertex(Point3D(5.0, 5.0, 100.0)));
    EXPECT_NE(f.ContainsVertex(&p1), nullptr);
    EXPECT_EQ(f.ContainsVertex(nullptr), nullptr);
}

TEST(Facet3PtTest, ReverseAndOpposites) {
    Point3D p0(0.0, 0.0, 100.0);
    Point3D p1(10.0, 0.0, 100.0);
    Point3D p2(0.0, 10.0, 100.0);

    Body b1(1, "TopBody", 2500.0);
    Body b2(2, "BotBody", 2700.0);

    Facet3Pt f(p0, p1, p2);
    f.pBody = &b1;
    f.pBodyOpos = &b2;

    EXPECT_NEAR(f.normal().z, 1.0, 1e-12);
    EXPECT_EQ(f.pBody, &b1);
    EXPECT_EQ(f.pBodyOpos, &b2);

    // Reverse facet
    f.Reverse();
    EXPECT_EQ(f[0], p2);
    EXPECT_EQ(f[1], p1);
    EXPECT_EQ(f[2], p0);
    EXPECT_NEAR(f.normal().z, -1.0, 1e-12);
    EXPECT_EQ(f.pBody, &b2);
    EXPECT_EQ(f.pBodyOpos, &b1);

    // Opposite facet test
    Facet3Pt original(p0, p1, p2);
    EXPECT_TRUE(f.is_opposite(original));
    EXPECT_TRUE(f.IsOposit(original));
}

TEST(Facet3PtTest, PfldIntegrationAndFieldConsistency) {
    Point3D p0(0.0, 0.0, 0.0);
    Point3D p1(100.0, 0.0, 0.0);
    Point3D p2(0.0, 100.0, 0.0);

    Facet3Pt f(p0, p1, p2);

    Point3D obs(50.0, 50.0, -100.0); // observation point above facet
    double gzMod3d = 0.0;
    f.Fld_Gz(obs, gzMod3d);

    double gzPfld = f.field_gz(obs);
    EXPECT_NEAR(gzMod3d, gzPfld, 1e-14);

    Point3D gMod3d(0, 0, 0);
    f.Fld_G(obs, gMod3d);

    Point3D gPfld = f.field_g(obs);
    EXPECT_NEAR(gMod3d.x, gPfld.x, 1e-14);
    EXPECT_NEAR(gMod3d.y, gPfld.y, 1e-14);
    EXPECT_NEAR(gMod3d.z, gPfld.z, 1e-14);

    // Ensure pfld_facet() reference matches
    EXPECT_EQ(f.pfld_facet().size(), 3);
}
