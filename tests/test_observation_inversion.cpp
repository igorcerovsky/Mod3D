#include <gtest/gtest.h>
#include "mod3d/Inversion.h"
#include "mod3d/Observation.h"
#include "mod3d/Model.h"
#include "mod3d/Body.h"
#include <cmath>
#include <vector>

using namespace mod3d;

// ============================================================================
// 1. Core 1D Minimization Math Verification
// ============================================================================

TEST(InversionMathTest, MnbrakParabolicMinimum) {
    // Parabolic function f(x) = (x - 4.5)^2 + 3.0
    auto quadFunc = [](double x) {
        return (x - 4.5) * (x - 4.5) + 3.0;
    };

    double a = 0.0;
    double b = 1.0;
    double c = 0.0;
    double fa = 0.0, fb = 0.0, fc = 0.0;

    Inversion1D::mnbrak(a, b, c, fa, fb, fc, quadFunc);

    // Mnbrak must produce bracketed minimum: fb < fa and fb < fc
    EXPECT_LT(fb, fa);
    EXPECT_LT(fb, fc);
    // True minimum (4.5) must lie strictly inside interval [min(a, c), max(a, c)]
    double minInt = std::min(a, c);
    double maxInt = std::max(a, c);
    EXPECT_LE(minInt, 4.5);
    EXPECT_GE(maxInt, 4.5);
}

TEST(InversionMathTest, GoldenAndBrentConvergence) {
    // Non-linear function with known minimum at x = 2.0
    auto testFunc = [](double x) {
        return (x - 2.0) * (x - 2.0) + 5.0;
    };

    // 1. Golden section search
    FitResult resGolden = Inversion1D::optimize(0.0, 1.0, testFunc, FitMethod::Golden, 1e-5);
    EXPECT_TRUE(resGolden.converged);
    EXPECT_NEAR(resGolden.optimalParameter, 2.0, 1e-4);
    EXPECT_NEAR(resGolden.minimumObjective, 5.0, 1e-4);

    // 2. Brent's parabolic method
    FitResult resBrent = Inversion1D::optimize(0.0, 1.0, testFunc, FitMethod::Brent, 1e-5);
    EXPECT_TRUE(resBrent.converged);
    EXPECT_NEAR(resBrent.optimalParameter, 2.0, 1e-5);
    EXPECT_NEAR(resBrent.minimumObjective, 5.0, 1e-5);

    // Brent method should converge faster than Golden section on parabolic-like function
    EXPECT_LE(resBrent.iterations, resGolden.iterations);
}

// ============================================================================
// 2. Observation Space Geometry & Elevation Modes
// ============================================================================

TEST(ObservationSpaceTest, ElevationModesAndRotatedPoints) {
    const size_t rows = 3;
    const size_t cols = 3;
    const double x0 = 1000.0;
    const double y0 = 2000.0;
    const double dx = 100.0;
    const double dy = 100.0;

    ObservationSpace obs(rows, cols, x0, y0, dx, dy, 0.0);

    // 1. SensorHeight mode: Z = Relief(r, c) + h_sensor
    Grid relief(rows, cols, x0, y0, dx, dy, 0.0);
    for (size_t r = 0; r < rows; ++r) {
        for (size_t c = 0; c < cols; ++c) {
            relief(r, c) = 250.0 + static_cast<double>(r * 10 + c * 5);
        }
    }
    obs.setSurfaceRelief(relief);
    obs.setGravityObservation(ObservationMode::SensorHeight, 50.0); // 50m above ground

    auto ptsSens = obs.getObservationPoints(FieldComponent::GZ);
    ASSERT_EQ(ptsSens.size(), 9u);
    EXPECT_DOUBLE_EQ(ptsSens[0].z, 300.0); // 250 + 50
    EXPECT_DOUBLE_EQ(ptsSens[8].z, 250.0 + 20 + 10 + 50.0); // 330.0

    // 2. FlightElevation mode: Z = h_flight (constant 1500m)
    obs.setGravityObservation(ObservationMode::FlightElevation, 1500.0);
    auto ptsFlight = obs.getObservationPoints(FieldComponent::GZ);
    for (const auto &pt : ptsFlight) {
        EXPECT_DOUBLE_EQ(pt.z, 1500.0);
    }

    // 3. ElevationGrid mode
    Grid customElev(rows, cols, x0, y0, dx, dy, 0.0);
    for (size_t r = 0; r < rows; ++r) {
        for (size_t c = 0; c < cols; ++c) {
            customElev(r, c) = 800.0;
        }
    }
    obs.setGravityObservationGrid(customElev);
    auto ptsGrid = obs.getObservationPoints(FieldComponent::GZ);
    for (const auto &pt : ptsGrid) {
        EXPECT_DOUBLE_EQ(pt.z, 800.0);
    }
}

// ============================================================================
// 3. Forward Modeling, Total Field, and Difference Grids
// ============================================================================

TEST(ObservationSpaceTest, ForwardModelingAndTotalFieldVector) {
    Model model;
    model.init(3, 3, 0.0, 0.0, 200.0, 200.0, -1000.0, 0.0);

    Body *b = model.newBody();
    b->SetDensity(2800.0);
    int bId = b->GetID();

    for (int r = 1; r <= 2; ++r) {
        for (int c = 1; c <= 2; ++c) {
            model.insertBody(r, c, -400.0, 100.0, false, bId);
        }
    }
    model.initFacetList();

    std::vector<Facet3Pt> facets;
    model.getFacetsComputation(facets);
    ASSERT_GT(facets.size(), 0u);

    ObservationSpace obs(3, 3, 0.0, 0.0, 200.0, 200.0);
    obs.setGravityObservation(ObservationMode::FlightElevation, 100.0); // 100m flight level

    // Forward simulation
    obs.computeForwardField(facets, true);

    const Grid *pGz = obs.getModeledGrid(FieldComponent::GZ);
    const Grid *pGx = obs.getModeledGrid(FieldComponent::GX);
    const Grid *pGy = obs.getModeledGrid(FieldComponent::GY);
    const Grid *pGTot = obs.getModeledGrid(FieldComponent::G_TOT);

    ASSERT_NE(pGz, nullptr);
    ASSERT_NE(pGx, nullptr);
    ASSERT_NE(pGy, nullptr);
    ASSERT_NE(pGTot, nullptr);

    // Gz directly above mass should be positive downwards (or negative according to coordinate system)
    // and G_tot = sqrt(Gx^2 + Gy^2 + Gz^2) >= |Gz|
    for (size_t r = 0; r < 3; ++r) {
        for (size_t c = 0; c < 3; ++c) {
            double gz = (*pGz)(r, c);
            double gTot = (*pGTot)(r, c);
            EXPECT_GE(gTot, std::fabs(gz) - 1e-12);
        }
    }
}

// ============================================================================
// 4. Gate 3: Density Inversion Matching FitLogDens.dat
// ============================================================================

TEST(Gate3InversionTest, DensityRecoveryMatchesFitLogDens) {
    // In FitLogDens.dat:
    // Reference density = 2670.0 kg/m^3
    // True density = 2850.0 kg/m^3 (diffDens = +180.0 kg/m^3)
    // Starting guess = 2600.0 kg/m^3 (diffDens = -70.0 kg/m^3)
    const double refDensity = 2670.0;
    const double trueDensity = 2850.0;
    const double initialGuess = 2600.0;

    Model model;
    model.init(4, 4, 0.0, 0.0, 100.0, 100.0, -1000.0, 0.0);

    Body *body = model.newBody();
    body->SetDensity(trueDensity);
    int bId = body->GetID();

    for (int r = 1; r <= 2; ++r) {
        for (int c = 1; c <= 2; ++c) {
            model.insertBody(r, c, -400.0, 100.0, false, bId);
        }
    }
    model.initFacetList();

    std::vector<Facet3Pt> trueFacets;
    model.getFacetsComputation(trueFacets);

    // Set up observation space (4x4 grid at 50m above ground)
    ObservationSpace obs(4, 4, 0.0, 0.0, 100.0, 100.0);
    obs.setReferenceDensity(refDensity);
    obs.setGravityObservation(ObservationMode::FlightElevation, 50.0);

    // Generate synthetic observed field at true density (2850.0)
    obs.computeForwardField(trueFacets, true);
    Grid trueObsGrid = *obs.getModeledGrid(FieldComponent::GZ);
    obs.setObservedGrid(FieldComponent::GZ, trueObsGrid);

    // Perturb body to initial guess: 2600.0 (diffDens = -70.0 kg/m^3)
    body->SetDensity(initialGuess);

    // Run density inversion with Brent's method
    FitResult fitRes = obs.fitDensity(
        body, trueFacets, FieldComponent::GZ, initialGuess, 1e-4, FitMethod::Brent);

    // Gate 3 verification:
    // 1. Converged to true density 2850.0 within 0.1 kg/m^3
    EXPECT_TRUE(fitRes.converged);
    EXPECT_NEAR(fitRes.optimalParameter, trueDensity, 0.1);
    EXPECT_NEAR(body->GetDensity(), trueDensity, 0.1);

    // 2. Minimum RMS residual should be virtually zero (< 1e-8)
    EXPECT_LT(fitRes.minimumObjective, 1e-8);

    // 3. Iteration count is reasonable (< 30 iterations)
    EXPECT_LT(fitRes.iterations, 30);
    EXPECT_FALSE(fitRes.log.empty());
}

// ============================================================================
// 5. Gate 3: Vertex Depth Inversion Matching FitLogVrtx.dat
// ============================================================================

TEST(Gate3InversionTest, VertexDepthRecoveryMatchesFitLogVrtx) {
    // In FitLogVrtx.dat:
    // Target depth z = -707.58 m
    // Initial guess z = -686.82 m
    const double trueZ = -707.58;
    const double initialZ = -686.82;

    Model model;
    model.init(4, 4, 0.0, 0.0, 200.0, 200.0, -2000.0, 0.0);

    Body *body = model.newBody();
    body->SetDensity(2850.0);
    int bId = body->GetID();

    // Insert body across 2x2 columns (1,1) to (2,2)
    // Center = -750, thickness = 200 -> top is -550, bot is -950
    for (int r = 1; r <= 2; ++r) {
        for (int c = 1; c <= 2; ++c) {
            model.insertBody(r, c, -750.0, 200.0, false, bId);
        }
    }

    // Move vertex (2, 2, 1) to true depth: -707.58
    int vIdx = 1;
    model.moveVertex(vIdx, 2, 2, trueZ, BodyMoveType::Normal);

    model.initFacetList();
    std::vector<Facet3Pt> trueFacets;
    model.getFacetsComputation(trueFacets);
    ASSERT_GT(trueFacets.size(), 0u);


    // Set up observation space
    ObservationSpace obs(4, 4, 0.0, 0.0, 200.0, 200.0);
    obs.setGravityObservation(ObservationMode::FlightElevation, 100.0);

    // Generate synthetic observed field at true depth
    obs.computeForwardField(trueFacets, true);
    Grid trueObsGrid = *obs.getModeledGrid(FieldComponent::GZ);
    obs.setObservedGrid(FieldComponent::GZ, trueObsGrid);

    // Perturb vertex to initial guess: -686.82
    vIdx = 1;
    model.moveVertex(vIdx, 2, 2, initialZ, BodyMoveType::Normal);

    // Run vertex depth inversion with Brent's method
    FitResult fitRes = obs.fitVertex(
        model, 2, 2, 1, FieldComponent::GZ, 1e-4, FitMethod::Brent);

    // Gate 3 verification:
    // 1. Converged to true depth -707.58 within 0.1 m
    EXPECT_TRUE(fitRes.converged);
    EXPECT_NEAR(fitRes.optimalParameter, trueZ, 0.1);
    EXPECT_NEAR(model.getZ(2, 2, 1), trueZ, 0.1);

    // 2. Minimum RMS drops to near zero (< 1e-8)
    EXPECT_LT(fitRes.minimumObjective, 1e-8);
}
