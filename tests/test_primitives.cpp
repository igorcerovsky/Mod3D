#include <gtest/gtest.h>
#include "mod3d/Point3D.h"
#include "mod3d/Body.h"
#include "mod3d/ColumnPoint.h"
#include <cmath>

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
    ColumnPoint cpDefault;
    EXPECT_DOUBLE_EQ(cpDefault.z(), 0.0);
    EXPECT_EQ(cpDefault.bodyId(), -1);
    EXPECT_EQ(cpDefault.body(), nullptr);
    EXPECT_TRUE(cpDefault.isModified());

    ColumnPoint cpZ(-450.0);
    EXPECT_DOUBLE_EQ(cpZ.z(), -450.0);
    EXPECT_DOUBLE_EQ(cpZ.point().z, -450.0);

    Body testBody(42, "Basement", 2800.0);
    ColumnPoint cpBody(42, -600.0, &testBody);
    EXPECT_EQ(cpBody.bodyId(), 42);
    EXPECT_EQ(cpBody.body(), &testBody);
    EXPECT_DOUBLE_EQ(cpBody.z(), -600.0);
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
