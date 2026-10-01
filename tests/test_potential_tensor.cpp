#include <gtest/gtest.h>
#include "mod3d/PotField.h"
#include "mod3d/Facet3Pt.h"
#include "mod3d/FieldCompute.h"
#include "mod3d/Body.h"
#include <cmath>
#include <vector>

using namespace mod3d;

// ============================================================================
// 1. Geomagnetic Ambient Field & Magnetization Analytical Formulas
// ============================================================================

TEST(PotentialTensorTest, AmbientFieldTrigonometricProjection) {
    // Inclination = 60 deg, Declination = 30 deg, Intensity = 50000 nT
    const double iI = 60.0;
    const double iD = 30.0;
    const double iS = 50000.0;

    Point3D af = AmbientField(iI, iD, iS);

    const double iI_rad = iI * PI_VAL / 180.0;
    const double iD_rad = iD * PI_VAL / 180.0;

    // Geophysical coordinate convention:
    // Y = North = iS * cos(I) * cos(D)
    // X = East  = iS * cos(I) * sin(D)
    // Z = Downward negative = -iS * sin(I)
    double expectedY = iS * std::cos(iI_rad) * std::cos(iD_rad);
    double expectedX = iS * std::cos(iI_rad) * std::sin(iD_rad);
    double expectedZ = -iS * std::sin(iI_rad);

    EXPECT_NEAR(af.x, expectedX, 1e-10);
    EXPECT_NEAR(af.y, expectedY, 1e-10);
    EXPECT_NEAR(af.z, expectedZ, 1e-10);
    EXPECT_NEAR(af.Abs(), iS, 1e-10);
}

TEST(PotentialTensorTest, MagnetizationVectorInducedAndRemanent) {
    // Susceptibility kappa = 0.05 SI
    const double susc = 0.05;
    const double iI = 60.0;
    const double iD = 30.0;
    const double iS = 50000.0;

    // Remanent: Inc = 45 deg, Dec = 90 deg, Int = 1.5 A/m
    const double rI = 45.0;
    const double rD = 90.0;
    const double rS = 1.5;

    Point3D vM = MagnetizationVector(susc, iI, iD, iS, rI, rD, rS);

    Point3D vH = AmbientField(iI, iD, iS);
    Point3D vI = vH * (susc / (4.0 * PI_VAL));

    Point3D vR;
    const double rI_rad = rI * PI_VAL / 180.0;
    const double rD_rad = rD * PI_VAL / 180.0;
    vR.x = std::cos(rI_rad) * std::cos(rD_rad) * rS;
    vR.y = std::cos(rI_rad) * std::sin(rD_rad) * rS;
    vR.z = -std::sin(rI_rad) * rS;

    Point3D expectedM = vI + vR;

    EXPECT_NEAR(vM.x, expectedM.x, 1e-10);
    EXPECT_NEAR(vM.y, expectedM.y, 1e-10);
    EXPECT_NEAR(vM.z, expectedM.z, 1e-10);
}

TEST(PotentialTensorTest, TotalFieldAnomalyMag_dT) {
    Point3D af = AmbientField(60.0, 0.0, 50000.0);
    Point3D afUnit = af;
    afUnit.Unit();

    // Secondary anomalous field of 100 nT aligned with ambient field
    Point3D anomalousField = afUnit * 100.0;
    double dT_parallel = Mag_dT(anomalousField, af, 50000.0);
    // |B_0 + dB| - |B_0| = 50100 - 50000 = 100
    EXPECT_NEAR(dT_parallel, 100.0, 1e-10);

    // Overload with individual coordinates
    double dT_coords = Mag_dT(anomalousField.x, anomalousField.y, anomalousField.z,
                              af.x, af.y, af.z, 50000.0);
    EXPECT_DOUBLE_EQ(dT_parallel, dT_coords);

    // Zero anomaly should yield 0.0
    EXPECT_NEAR(Mag_dT(Point3D(0, 0, 0), af, 50000.0), 0.0, 1e-10);
}

// ============================================================================
// 2. Gravity Gradient Tensor & Laplace Equation Trace Invariance
// ============================================================================

// Helper to construct a closed cube with 12 triangular facets with outward normals
static std::vector<Facet3Pt> CreateClosedCube(double x0, double x1,
                                             double y0, double y1,
                                             double z0, double z1,
                                             double density)
{
    // 8 vertices of cube
    Point3D p0(x0, y0, z0); // 0: bottom SW
    Point3D p1(x1, y0, z0); // 1: bottom SE
    Point3D p2(x1, y1, z0); // 2: bottom NE
    Point3D p3(x0, y1, z0); // 3: bottom NW
    Point3D p4(x0, y0, z1); // 4: top SW
    Point3D p5(x1, y0, z1); // 5: top SE
    Point3D p6(x1, y1, z1); // 6: top NE
    Point3D p7(x0, y1, z1); // 7: top NW

    std::vector<Facet3Pt> facets;
    facets.reserve(12);

    auto addQuad = [&](const Point3D &a, const Point3D &b, const Point3D &c, const Point3D &d) {
        Facet3Pt f1, f2;
        f1.Init(a, b, c, density, 0.0);
        f2.Init(a, c, d, density, 0.0);
        facets.push_back(f1);
        facets.push_back(f2);
    };

    // Bottom face (-Z normal): viewed from below, vertices ordered CCW: p0, p3, p2, p1
    addQuad(p0, p3, p2, p1);
    // Top face (+Z normal): viewed from above, vertices CCW: p4, p5, p6, p7
    addQuad(p4, p5, p6, p7);
    // South face (-Y normal): p0, p1, p5, p4
    addQuad(p0, p1, p5, p4);
    // East face (+X normal): p1, p2, p6, p5
    addQuad(p1, p2, p6, p5);
    // North face (+Y normal): p2, p3, p7, p6
    addQuad(p2, p3, p7, p6);
    // West face (-X normal): p3, p0, p4, p7
    addQuad(p3, p0, p4, p7);

    return facets;
}

TEST(PotentialTensorTest, GravityGradientTensorLaplaceInvarianceInFreeSpace) {
    // 1 km cube centered from x=0..1000, y=0..1000, z=0..1000
    // Density = 2670 kg/m^3
    const double rho = 2670.0;
    auto cube = CreateClosedCube(0.0, 1000.0, 0.0, 1000.0, 0.0, 1000.0, rho);
    ASSERT_EQ(cube.size(), 12);

    // Verify all facet normals point outward from cube center (500, 500, 500)
    Point3D center(500, 500, 500);
    for (const auto &fct : cube) {
        Point3D fctCenter = (fct.pts[0] + fct.pts[1] + fct.pts[2]) / 3.0;
        Point3D outward = fctCenter - center;
        EXPECT_GT(outward * fct.Normal(), 0.0);
    }

    // Test observation points in external free space
    std::vector<Point3D> obsPoints = {
        Point3D(500.0, 500.0, 2000.0),   // Directly above center
        Point3D(2000.0, 2000.0, 2000.0), // Off-axis distance
        Point3D(500.0, -1500.0, 500.0),  // South of cube
        Point3D(-3000.0, 500.0, 500.0)   // West of cube
    };

    for (const auto &obs : obsPoints) {
        double gxx = 0.0, gyy = 0.0, gzz = 0.0;
        double gxy = 0.0, gxz = 0.0, gyz = 0.0;

        for (const auto &fct : cube) {
            double fxx = 0, fyy = 0, fzz = 0, fxy = 0, fxz = 0, fyz = 0;
            fct.FldVladoGrd(obs, 0.0, fxx, fyy, fzz, fxy, fxz, fyz);
            gxx += fxx;
            gyy += fyy;
            gzz += fzz;
            gxy += fxy;
            gxz += fxz;
            gyz += fyz;
        }

        // Laplace Equation: trace(Gamma) = gxx + gyy + gzz = 0 in free space
        double trace = gxx + gyy + gzz;
        EXPECT_NEAR(trace, 0.0, 1e-14)
            << "Laplace condition failed at obs (" << obs.x << ", " << obs.y << ", " << obs.z << ")";

        // Symmetry above center: for point (500, 500, 2000) directly above symmetric cube,
        // gxx should equal gyy, and cross-terms gxy, gxz, gyz should be near 0
        if (obs.x == 500.0 && obs.y == 500.0) {
            EXPECT_NEAR(gxx, gyy, 1e-15);
            EXPECT_NEAR(gxy, 0.0, 1e-15);
            EXPECT_NEAR(gxz, 0.0, 1e-15);
            EXPECT_NEAR(gyz, 0.0, 1e-15);
            // gzz = -2 * gxx because trace is 0
            EXPECT_NEAR(gzz, -2.0 * gxx, 1e-15);
        }
    }
}

// ============================================================================
// 3. FieldCompute Linear Density Gradient Parallel vs Serial
// ============================================================================

TEST(PotentialTensorTest, LinearDensityGradientParallelVsSerial) {
    auto cube = CreateClosedCube(0.0, 1000.0, 0.0, 1000.0, 0.0, 1000.0, 2670.0);

    std::vector<Point3D> obsPoints;
    for (int ix = -2; ix <= 2; ++ix) {
        for (int iy = -2; iy <= 2; ++iy) {
            obsPoints.emplace_back(500.0 + ix * 500.0, 500.0 + iy * 500.0, 1500.0);
        }
    }

    Point3D gradRho(0.05, -0.02, 0.1); // Density gradient (kg/m^3 / m)
    double rho0 = 2500.0;

    std::vector<double> outParallel, outSerial;

    // Parallel with 4 threads
    FieldCompute::ComputeGzLinearParallel(cube, obsPoints, gradRho, rho0, outParallel, 4);

    // Serial with 1 thread
    FieldCompute::ComputeGzLinearParallel(cube, obsPoints, gradRho, rho0, outSerial, 1);

    ASSERT_EQ(outParallel.size(), obsPoints.size());
    ASSERT_EQ(outSerial.size(), obsPoints.size());

    for (size_t i = 0; i < obsPoints.size(); ++i) {
        EXPECT_NEAR(outParallel[i], outSerial[i], 1e-14);
        EXPECT_NE(outParallel[i], 0.0);
    }
}
