#include "mod3d/Body.h"
#include "mod3d/Inversion.h"
#include "mod3d/Model.h"
#include "mod3d/Observation.h"
#include "mod3d/Project.h"
#include "inv/voxel_grid.hpp"
#include "inv/inversion_engine.hpp"
#include <nlohmann/json.hpp>
#include <cmath>
#include <gtest/gtest.h>
#include <vector>

using namespace mod3d;

// ============================================================================
// 1. Core 1D Minimization Math Verification
// ============================================================================

TEST(InversionMathTest, BracketMinimumParabolic) {
  // Parabolic function f(x) = (x - 4.5)^2 + 3.0
  auto quadFunc = [](double x) { return (x - 4.5) * (x - 4.5) + 3.0; };

  double a = 0.0;
  double b = 1.0;
  double c = 0.0;
  double fa = 0.0, fb = 0.0, fc = 0.0;

  Inversion1D::bracket_minimum(a, b, c, fa, fb, fc, quadFunc);

  // bracket_minimum must produce bracketed minimum: fb < fa and fb < fc
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
  auto testFunc = [](double x) { return (x - 2.0) * (x - 2.0) + 5.0; };

  // 1. Golden section search
  FitResult resGolden =
      Inversion1D::optimize(0.0, 1.0, testFunc, FitMethod::golden, 1e-5);
  EXPECT_TRUE(resGolden.converged);
  EXPECT_NEAR(resGolden.optimal_parameter, 2.0, 1e-4);
  EXPECT_NEAR(resGolden.minimum_objective, 5.0, 1e-4);

  // 2. Brent's parabolic method
  FitResult resBrent =
      Inversion1D::optimize(0.0, 1.0, testFunc, FitMethod::brent, 1e-5);
  EXPECT_TRUE(resBrent.converged);
  EXPECT_NEAR(resBrent.optimal_parameter, 2.0, 1e-5);
  EXPECT_NEAR(resBrent.minimum_objective, 5.0, 1e-5);

  // Brent method should converge faster than Golden section on parabolic-like
  // function
  EXPECT_LE(resBrent.iterations, resGolden.iterations);
}

TEST(InversionMathTest, BoundedOptimizationConvergence) {
  // Test function on [0, 5] with minimum at x = 2.75
  auto testFunc = [](double x) { return (x - 2.75) * (x - 2.75) + 3.14; };

  // 1. Golden section bounded
  FitResult resGolden = Inversion1D::optimize_bounded(
      0.0, 5.0, testFunc, 1.0, FitMethod::golden, 1e-5);
  EXPECT_TRUE(resGolden.converged);
  EXPECT_NEAR(resGolden.optimal_parameter, 2.75, 1e-4);
  EXPECT_NEAR(resGolden.minimum_objective, 3.14, 1e-4);

  // 2. Brent bounded
  FitResult resBrent = Inversion1D::optimize_bounded(
      0.0, 5.0, testFunc, 1.0, FitMethod::brent, 1e-5);
  EXPECT_TRUE(resBrent.converged);
  EXPECT_NEAR(resBrent.optimal_parameter, 2.75, 1e-5);
  EXPECT_NEAR(resBrent.minimum_objective, 3.14, 1e-5);
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
  obs.set_surface_relief(relief);
  obs.set_gravity_observation(ObservationMode::SensorHeight,
                              50.0); // 50m above ground

  auto ptsSens = obs.observation_points(FieldComponent::GZ);
  ASSERT_EQ(ptsSens.size(), 9u);
  EXPECT_DOUBLE_EQ(ptsSens[0].z, 300.0);                  // 250 + 50
  EXPECT_DOUBLE_EQ(ptsSens[8].z, 250.0 + 20 + 10 + 50.0); // 330.0

  // 2. FlightElevation mode: Z = h_flight (constant 1500m)
  obs.set_gravity_observation(ObservationMode::FlightElevation, 1500.0);
  auto ptsFlight = obs.observation_points(FieldComponent::GZ);
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
  obs.set_gravity_observation_grid(customElev);
  auto ptsGrid = obs.observation_points(FieldComponent::GZ);
  for (const auto &pt : ptsGrid) {
    EXPECT_DOUBLE_EQ(pt.z, 800.0);
  }
}

TEST(ObservationSpaceTest, ModernAccessorsAndPredicates) {
  EXPECT_TRUE(is_gravity_component(FieldComponent::GX));
  EXPECT_TRUE(is_gravity_component(FieldComponent::GZ));
  EXPECT_TRUE(is_gravity_component(FieldComponent::G_TOT));
  EXPECT_FALSE(is_gravity_component(FieldComponent::DELTA_T));

  EXPECT_TRUE(is_magnetic_component(FieldComponent::MX));
  EXPECT_TRUE(is_magnetic_component(FieldComponent::DELTA_T));
  EXPECT_FALSE(is_magnetic_component(FieldComponent::GZZ));

  EXPECT_TRUE(is_tensor_component(FieldComponent::GXX));
  EXPECT_TRUE(is_tensor_component(FieldComponent::GYZ));
  EXPECT_FALSE(is_tensor_component(FieldComponent::GY));

  ObservationSpace obs1(5, 7, 500.0, 600.0, 50.0, 50.0, 45.0);
  EXPECT_EQ(obs1.rows(), 5u);
  EXPECT_EQ(obs1.cols(), 7u);
  EXPECT_DOUBLE_EQ(obs1.x0().value(), 500.0);
  EXPECT_DOUBLE_EQ(obs1.y0().value(), 600.0);
  EXPECT_DOUBLE_EQ(obs1.dx().value(), 50.0);
  EXPECT_DOUBLE_EQ(obs1.dy().value(), 50.0);
  EXPECT_DOUBLE_EQ(obs1.rotation().value(), 45.0);

  obs1.set_reference_density(2750.0);
  EXPECT_DOUBLE_EQ(obs1.reference_density().value(), 2750.0);

  obs1.set_ambient_field_params(65.0, 15.0, 48000.0);
  EXPECT_DOUBLE_EQ(obs1.inclination().value(), 65.0);
  EXPECT_DOUBLE_EQ(obs1.declination().value(), 15.0);
  EXPECT_DOUBLE_EQ(obs1.magnetic_intensity().value(), 48000.0);

  // Swap test
  ObservationSpace obs2(10, 12, 0.0, 0.0, 100.0, 100.0, 0.0);
  obs1.swap(obs2);
  EXPECT_EQ(obs1.rows(), 10u);
  EXPECT_EQ(obs2.rows(), 5u);
  EXPECT_DOUBLE_EQ(obs2.reference_density().value(), 2750.0);
  using std::swap;
  swap(obs1, obs2);
  EXPECT_EQ(obs1.rows(), 5u);
  EXPECT_EQ(obs2.rows(), 10u);
}

// ============================================================================
// 3. Forward Modeling, Total Field, and Difference Grids
// ============================================================================

TEST(ObservationSpaceTest, ForwardModelingAndTotalFieldVector) {
  Model model;
  model.init(3, 3, 0.0, 0.0, 200.0, 200.0, -1000.0, 0.0);

  Body *b = model.create_body();
  b->set_density(2800.0);
  int bId = b->id();

  for (int r = 1; r <= 2; ++r) {
    for (int c = 1; c <= 2; ++c) {
      model.insert_body(r, c, -400.0, 100.0, false, bId);
    }
  }
  model.init_facet_list();

  std::vector<Facet3Pt> facets;
  model.facets_computation(facets);
  ASSERT_GT(facets.size(), 0u);

  ObservationSpace obs(3, 3, 0.0, 0.0, 200.0, 200.0);
  obs.set_gravity_observation(ObservationMode::FlightElevation,
                              100.0); // 100m flight level

  // Forward simulation
  obs.compute_forward_field(facets, true);

  const Grid *pGz = obs.modeled_grid(FieldComponent::GZ);
  const Grid *pGx = obs.modeled_grid(FieldComponent::GX);
  const Grid *pGy = obs.modeled_grid(FieldComponent::GY);
  const Grid *pGTot = obs.modeled_grid(FieldComponent::G_TOT);

  ASSERT_NE(pGz, nullptr);
  ASSERT_NE(pGx, nullptr);
  ASSERT_NE(pGy, nullptr);
  ASSERT_NE(pGTot, nullptr);

  // Gz directly above mass should be positive downwards (or negative according
  // to coordinate system) and G_tot = sqrt(Gx^2 + Gy^2 + Gz^2) >= |Gz|
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

  Body *body = model.create_body();
  body->set_density(trueDensity);
  int bId = body->id();

  for (int r = 1; r <= 2; ++r) {
    for (int c = 1; c <= 2; ++c) {
      model.insert_body(r, c, -400.0, 100.0, false, bId);
    }
  }
  model.init_facet_list();

  std::vector<Facet3Pt> trueFacets;
  model.facets_computation(trueFacets);

  // Set up observation space (4x4 grid at 50m above ground)
  ObservationSpace obs(4, 4, 0.0, 0.0, 100.0, 100.0);
  obs.set_reference_density(refDensity);
  obs.set_gravity_observation(ObservationMode::FlightElevation, 50.0);

  // Generate synthetic observed field at true density (2850.0)
  obs.compute_forward_field(trueFacets, true);
  Grid trueObsGrid = *obs.modeled_grid(FieldComponent::GZ);
  obs.set_observed_grid(FieldComponent::GZ, trueObsGrid);

  // Perturb body to initial guess: 2600.0 (diffDens = -70.0 kg/m^3)
  body->set_density(initialGuess);

  // Run density inversion with Brent's method
  FitResult fitRes = obs.fit_density(body, trueFacets, FieldComponent::GZ,
                                     initialGuess, 1e-4, FitMethod::brent);

  // Gate 3 verification:
  // 1. Converged to true density 2850.0 within 0.1 kg/m^3
  EXPECT_TRUE(fitRes.converged);
  EXPECT_NEAR(fitRes.optimal_parameter, trueDensity, 0.1);
  EXPECT_NEAR(body->density().value(), trueDensity, 0.1);

  // 2. Minimum RMS residual should be virtually zero (< 1e-4 mGal)
  EXPECT_LT(fitRes.minimum_objective, 1e-4);

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

  Body *body = model.create_body();
  body->set_density(2850.0);
  int bId = body->id();

  // Insert body across 2x2 columns (1,1) to (2,2)
  // Center = -750, thickness = 200 -> top is -550, bot is -950
  for (int r = 1; r <= 2; ++r) {
    for (int c = 1; c <= 2; ++c) {
      model.insert_body(r, c, -750.0, 200.0, false, bId);
    }
  }

  // Move vertex (2, 2, 1) to true depth: -707.58
  int vIdx = 1;
  model.move_vertex(vIdx, 2, 2, trueZ, BodyMoveType::Normal);

  model.init_facet_list();
  std::vector<Facet3Pt> trueFacets;
  model.facets_computation(trueFacets);
  ASSERT_GT(trueFacets.size(), 0u);

  // Set up observation space
  ObservationSpace obs(4, 4, 0.0, 0.0, 200.0, 200.0);
  obs.set_gravity_observation(ObservationMode::FlightElevation, 100.0);

  // Generate synthetic observed field at true depth
  obs.compute_forward_field(trueFacets, true);
  Grid trueObsGrid = *obs.modeled_grid(FieldComponent::GZ);
  obs.set_observed_grid(FieldComponent::GZ, trueObsGrid);

  // Perturb vertex to initial guess: -686.82
  vIdx = 1;
  model.move_vertex(vIdx, 2, 2, initialZ, BodyMoveType::Normal);

  // Run vertex depth inversion with Brent's method
  FitResult fitRes = obs.fit_vertex(model, 2, 2, 1, FieldComponent::GZ, 1e-4,
                                    FitMethod::brent);

  // Gate 3 verification:
  // 1. Converged to true depth -707.58 within 0.1 m
  EXPECT_TRUE(fitRes.converged);
  EXPECT_NEAR(fitRes.optimal_parameter, trueZ, 0.1);
  EXPECT_NEAR(model.z(2, 2, 1), trueZ, 0.1);

  // 2. Minimum RMS drops to near zero (< 1e-4 mGal)
  EXPECT_LT(fitRes.minimum_objective, 1e-4);
}

TEST(Gate3InversionTest, DensityRecoveryLinearLeastSquares) {
  const double refDensity = 2670.0;
  const double trueDensity = 2850.0;
  const double initialGuess = 2600.0;

  Model model;
  model.init(4, 4, 0.0, 0.0, 100.0, 100.0, -1000.0, 0.0);

  Body *body = model.create_body();
  body->set_density(trueDensity);
  int bId = body->id();

  for (int r = 1; r <= 2; ++r) {
    for (int c = 1; c <= 2; ++c) {
      model.insert_body(r, c, -400.0, 100.0, false, bId);
    }
  }
  model.init_facet_list();

  std::vector<Facet3Pt> trueFacets;
  model.facets_computation(trueFacets);

  ObservationSpace obs(4, 4, 0.0, 0.0, 100.0, 100.0);
  obs.set_reference_density(refDensity);
  obs.set_gravity_observation(ObservationMode::FlightElevation, 50.0);

  obs.compute_forward_field(trueFacets, true);
  Grid trueObsGrid = *obs.modeled_grid(FieldComponent::GZ);
  obs.set_observed_grid(FieldComponent::GZ, trueObsGrid);

  body->set_density(initialGuess);

  // Run density inversion with exact analytical linear least squares
  FitResult fitRes = obs.fit_density(body, trueFacets, FieldComponent::GZ,
                                     initialGuess, 1e-4, FitMethod::linear);

  EXPECT_TRUE(fitRes.converged);
  EXPECT_EQ(fitRes.iterations, 1);
  EXPECT_NEAR(fitRes.optimal_parameter, trueDensity, 1e-6);
  EXPECT_NEAR(body->density().value(), trueDensity, 1e-6);
  EXPECT_LT(fitRes.minimum_objective, 1e-6);
}

TEST(Gate3InversionTest, VertexDepthRecoverySuperposition) {
  const double trueZ = -707.58;
  const double initialZ = -686.82;

  Model model;
  model.init(4, 4, 0.0, 0.0, 200.0, 200.0, -2000.0, 0.0);

  Body *body = model.create_body();
  body->set_density(2850.0);
  int bId = body->id();

  for (int r = 1; r <= 2; ++r) {
    for (int c = 1; c <= 2; ++c) {
      model.insert_body(r, c, -750.0, 200.0, false, bId);
    }
  }

  int vIdx = 1;
  model.move_vertex(vIdx, 2, 2, trueZ, BodyMoveType::Normal);
  model.init_facet_list();
  std::vector<Facet3Pt> trueFacets;
  model.facets_computation(trueFacets);

  ObservationSpace obs(4, 4, 0.0, 0.0, 200.0, 200.0);
  obs.set_gravity_observation(ObservationMode::FlightElevation, 100.0);
  obs.compute_forward_field(trueFacets, true);
  Grid trueObsGrid = *obs.modeled_grid(FieldComponent::GZ);
  obs.set_observed_grid(FieldComponent::GZ, trueObsGrid);

  // Move vertex to initial guess and compute forward field so modeled_grids_ matches initial model
  model.move_vertex(vIdx, 2, 2, initialZ, BodyMoveType::Normal);
  model.init_facet_list();
  std::vector<Facet3Pt> initialFacets;
  model.facets_computation(initialFacets);
  obs.compute_forward_field(initialFacets, true);

  // Run vertex depth inversion using fast superposition: use_superposition = true
  FitResult fitRes = obs.fit_vertex(model, 2, 2, 1, FieldComponent::GZ, 1e-4,
                                    FitMethod::brent, true);

  EXPECT_TRUE(fitRes.converged);
  EXPECT_NEAR(fitRes.optimal_parameter, trueZ, 0.1);
  EXPECT_NEAR(model.z(2, 2, 1), trueZ, 0.1);
  EXPECT_LT(fitRes.minimum_objective, 1e-4);
}

TEST(Gate3InversionTest, ParallelStationForwardAndVertexInversionBenchmark) {
  // Benchmark with a 25x25 observation grid (625 stations)
  const size_t rows = 25;
  const size_t cols = 25;
  const double trueZ = -600.0;
  const double initialZ = -570.0;

  Model model;
  model.init(rows, cols, 0.0, 0.0, 100.0, 100.0, -2000.0, 0.0);

  Body *body = model.create_body();
  body->set_density(2750.0);
  int bId = body->id();

  for (int r = 10; r <= 15; ++r) {
    for (int c = 10; c <= 15; ++c) {
      model.insert_body(r, c, -650.0, 200.0, false, bId);
    }
  }

  int targetRow = 12;
  int targetCol = 12;
  int vIdx = 1;

  // Set to true depth and compute true observation in parallel
  model.move_vertex(vIdx, targetRow, targetCol, trueZ, BodyMoveType::Normal);
  model.init_facet_list();
  std::vector<Facet3Pt> trueFacets;
  model.facets_computation(trueFacets);

  ObservationSpace obs(rows, cols, 0.0, 0.0, 100.0, 100.0);
  obs.set_gravity_observation(ObservationMode::FlightElevation, 100.0);
  obs.compute_forward_field(trueFacets, true);
  Grid trueObsGrid = *obs.modeled_grid(FieldComponent::GZ);
  obs.set_observed_grid(FieldComponent::GZ, trueObsGrid);

  // Perturb to initial guess and compute initial baseline
  model.move_vertex(vIdx, targetRow, targetCol, initialZ, BodyMoveType::Normal);
  model.init_facet_list();
  std::vector<Facet3Pt> initialFacets;
  model.facets_computation(initialFacets);
  obs.compute_forward_field(initialFacets, true);

  // Fast vertex inversion with Tier 2 zero-copy & Tier 3 jthread parallelism
  FitResult fitRes = obs.fit_vertex(model, targetRow, targetCol, vIdx,
                                    FieldComponent::GZ, 1e-4, FitMethod::brent,
                                    true);

  EXPECT_TRUE(fitRes.converged);
  EXPECT_NEAR(fitRes.optimal_parameter, trueZ, 0.2);
  EXPECT_NEAR(model.z(targetRow, targetCol, vIdx), trueZ, 0.2);
  EXPECT_LT(fitRes.minimum_objective, 1e-4);
  EXPECT_LT(fitRes.elapsed_time_sec, 1.0);
}

// ============================================================================
// 6. Virtual removal of the mean from the measured (observed) field
// ============================================================================

TEST(ObservationSpaceTest, VirtualObservedMeanRemoval) {
  ObservationSpace obs(5, 5, 0.0, 0.0, 200.0, 200.0);
  obs.set_gravity_observation(ObservationMode::FlightElevation, 100.0);

  Grid rawObs(5, 5, 0.0, 0.0, 200.0, 200.0);
  for (size_t r = 0; r < 5; ++r) {
    for (size_t c = 0; c < 5; ++c) {
      rawObs(r, c) = 15.0 + static_cast<double>(r * 2 + c);
    }
  }
  obs.set_observed_grid(FieldComponent::GZ, rawObs);

  const ObservationSpace &cobs = obs;
  const double rawMean = rawObs.mean();
  ASSERT_NE(rawMean, 0.0);

  // Disabled: effective grid is the raw grid, mean is stored
  EXPECT_FALSE(obs.remove_observed_mean());
  EXPECT_EQ(cobs.observed_grid(FieldComponent::GZ),
            cobs.raw_observed_grid(FieldComponent::GZ));
  EXPECT_NEAR(obs.observed_mean(FieldComponent::GZ), rawMean, 1e-12);

  // Enabled: virtual anomaly has zero mean, raw grid is untouched, mean stored
  obs.set_remove_observed_mean(true);
  const Grid *anomaly = cobs.observed_grid(FieldComponent::GZ);
  ASSERT_NE(anomaly, nullptr);
  EXPECT_NE(anomaly, cobs.raw_observed_grid(FieldComponent::GZ));
  EXPECT_NEAR(anomaly->mean(), 0.0, 1e-9);
  EXPECT_NEAR(obs.observed_mean(FieldComponent::GZ), rawMean, 1e-12);
  for (size_t r = 0; r < 5; ++r) {
    for (size_t c = 0; c < 5; ++c) {
      EXPECT_DOUBLE_EQ((*cobs.raw_observed_grid(FieldComponent::GZ))(r, c),
                       rawObs(r, c));
      EXPECT_NEAR((*anomaly)(r, c), rawObs(r, c) - rawMean, 1e-12);
    }
  }

  // Difference grid is computed using the virtual anomaly: modeled - observed
  // Set modeled field equal to the virtual anomaly -> difference should be 0
  Grid modeled = *anomaly;
  obs.set_modeled_grid(FieldComponent::GZ, modeled);
  obs.compute_difference(FieldComponent::GZ);
  EXPECT_NEAR(obs.difference_grid(FieldComponent::GZ)->rms(), 0.0, 1e-9);

  // Updating observed grid recalculates the mean and virtual anomaly
  Grid newRaw = rawObs;
  newRaw += 10.0;
  obs.set_observed_grid(FieldComponent::GZ, newRaw);
  EXPECT_NEAR(obs.observed_mean(FieldComponent::GZ), rawMean + 10.0, 1e-12);
  EXPECT_NEAR(cobs.observed_grid(FieldComponent::GZ)->mean(), 0.0, 1e-9);

  // Disabled again: raw field is shown, stored mean stays available
  obs.set_remove_observed_mean(false);
  EXPECT_EQ(cobs.observed_grid(FieldComponent::GZ),
            cobs.raw_observed_grid(FieldComponent::GZ));
  EXPECT_NEAR(obs.observed_mean(FieldComponent::GZ), rawMean + 10.0, 1e-12);
}

TEST(ObservationSpaceTest, BackgroundTrendModesAndParameters) {
  ObservationSpace obs(11, 11, 0.0, 0.0, 100.0, 100.0);
  Grid raw(11, 11, 0.0, 0.0, 100.0, 100.0);
  // Add regional tilt: 25.0 + 0.02 * x - 0.01 * y
  for (size_t r = 0; r < 11; ++r) {
    for (size_t c = 0; c < 11; ++c) {
      double x = c * 100.0;
      double y = r * 100.0;
      raw(r, c) = 25.0 + 0.02 * x - 0.01 * y;
    }
  }
  obs.set_observed_grid(FieldComponent::GZ, raw);
  const double rawMean = raw.mean();
  const ObservationSpace &cobs = obs;

  // Mode 0: None
  obs.set_background_trend_mode(BackgroundTrendMode::None);
  EXPECT_EQ(obs.background_trend_mode(), 0);
  EXPECT_FALSE(obs.remove_observed_mean());
  EXPECT_EQ(cobs.observed_grid(FieldComponent::GZ), cobs.raw_observed_grid(FieldComponent::GZ));
  auto t0 = obs.observed_trend_parameters(FieldComponent::GZ);
  EXPECT_NEAR(t0.c0, rawMean, 1e-9);

  // Mode 1: ConstantShift
  obs.set_background_trend_mode(BackgroundTrendMode::ConstantShift);
  EXPECT_EQ(obs.background_trend_mode(), 1);
  EXPECT_TRUE(obs.remove_observed_mean());
  const Grid *anom1 = cobs.observed_grid(FieldComponent::GZ);
  ASSERT_NE(anom1, nullptr);
  EXPECT_NEAR(anom1->mean(), 0.0, 1e-9);
  auto t1 = obs.observed_trend_parameters(FieldComponent::GZ);
  EXPECT_NEAR(t1.c0, rawMean, 1e-9);
  EXPECT_DOUBLE_EQ(t1.gx_per_km, 0.0);
  EXPECT_DOUBLE_EQ(t1.gy_per_km, 0.0);

  // Mode 2: PlanarRegionalTrend
  obs.set_background_trend_mode(BackgroundTrendMode::PlanarRegionalTrend);
  EXPECT_EQ(obs.background_trend_mode(), 2);
  EXPECT_TRUE(obs.remove_observed_mean());
  const Grid *anom2 = cobs.observed_grid(FieldComponent::GZ);
  ASSERT_NE(anom2, nullptr);
  EXPECT_NEAR(anom2->mean(), 0.0, 1e-9);
  auto t2 = obs.observed_trend_parameters(FieldComponent::GZ);
  EXPECT_NEAR(t2.gx_per_km, 0.02 * 1000.0, 1e-4); // 0.02 mGal/m = 20 mGal/km
  EXPECT_NEAR(t2.gy_per_km, -0.01 * 1000.0, 1e-4); // -0.01 mGal/m = -10 mGal/km

  // Mode 3: BorderPlanar
  obs.set_background_trend_mode(BackgroundTrendMode::BorderPlanar);
  EXPECT_EQ(obs.background_trend_mode(), 3);
  const Grid *anom3 = cobs.observed_grid(FieldComponent::GZ);
  ASSERT_NE(anom3, nullptr);
  EXPECT_NEAR(anom3->mean(), 0.0, 1e-9);
  auto t3 = obs.observed_trend_parameters(FieldComponent::GZ);
  EXPECT_NEAR(t3.gx_per_km, 20.0, 1e-4);
  EXPECT_NEAR(t3.gy_per_km, -10.0, 1e-4);
}

TEST(ObservationSpaceTest, VoxelGridForwardCalculationDirectGravity) {
  ObservationSpace obs(9, 9, 0.0, 0.0, 100.0, 100.0);
  obs.set_gravity_field_unit(GravityFieldUnit::MilliGal);

  // Create voxel grid with density anomaly at center (cell 4, 4, 2)
  inv::VoxelGrid<double> vg(9, 9, 6, -50.0, -50.0, 0.0, 100.0, 100.0, 50.0, 0.0);
  vg(4, 4, 2) = 0.5; // 0.5 g/cm³ density contrast

  obs.compute_voxel_grid_gravity(vg, 0.0, true);
  const Grid *gz = obs.modeled_grid(FieldComponent::GZ);
  ASSERT_NE(gz, nullptr);

  // Peak anomaly should be at center (row 4, col 4) and positive downward
  const double centerVal = (*gz)(4, 4);
  EXPECT_GT(centerVal, 0.0);
  EXPECT_GT(centerVal, (*gz)(0, 0));
  EXPECT_GT(centerVal, (*gz)(0, 8));
  EXPECT_GT(centerVal, (*gz)(8, 0));
  EXPECT_GT(centerVal, (*gz)(8, 8));
}

TEST(ObservationSpaceTest, CompareWithRoman1026Model) {
  Project proj;
  std::string path = "examples/private/roman_1026.mod3d";
  if (!std::filesystem::exists(path)) {
    path = "../examples/private/roman_1026.mod3d";
  }
  if (!std::filesystem::exists(path)) {
    path = "../../examples/private/roman_1026.mod3d";
  }
  if (!std::filesystem::exists(path)) {
    GTEST_SKIP() << "File not found: " << path;
  }
  ASSERT_TRUE(proj.load_json(path));
  ASSERT_TRUE(proj.has_inversion_result_json());

  auto j = nlohmann::json::parse(proj.inversion_result_json());
  auto invRes = inv::InversionResult::from_json(j);
  ASSERT_FALSE(invRes.modeled_gz.empty());

  proj.observation().compute_voxel_grid_gravity(invRes.density_grid, proj.model().z_max().value(), true);
  const Grid *gz = proj.observation().modeled_grid(FieldComponent::GZ);
  ASSERT_NE(gz, nullptr);

  // Point-by-point comparison between invRes.modeled_gz and computed gz
  double sum_sq_diff = 0.0;
  for (size_t ix = 0; ix < gz->cols(); ++ix) {
    for (size_t iy = 0; iy < gz->rows(); ++iy) {
      size_t col_idx = ix * gz->rows() + iy;
      double v_inv = invRes.modeled_gz[col_idx];
      double v_comp = (*gz)(iy, ix);
      double diff = std::abs(v_inv - v_comp);
      sum_sq_diff += diff * diff;
    }
  }
  double rms_diff = std::sqrt(sum_sq_diff / invRes.modeled_gz.size());
  EXPECT_LT(rms_diff, 0.1);
}

// ============================================================================
// 7. Multi-Body Density Inversion with Shared Contact Interface
// ============================================================================

TEST(ObservationSpaceTest, MultiBodyDensityInversionWithSharedInterface) {
  const double refDensity = 2670.0;
  const double trueDensity1 = 2450.0;
  const double trueDensity2 = 2920.0;

  Model model;
  model.init(4, 4, 0.0, 0.0, 150.0, 150.0, -1500.0, 0.0);

  Body *b1 = model.create_body();
  b1->set_density(trueDensity1);
  int id1 = b1->id();

  Body *b2 = model.create_body();
  b2->set_density(trueDensity2);
  int id2 = b2->id();

  for (int r = 1; r <= 2; ++r) {
    for (int c = 1; c <= 2; ++c) {
      model.insert_body(r, c, -350.0, 150.0, false, id1); // [-200, -500]
      model.insert_body(r, c, -700.0, 200.0, false, id2); // [-500, -900]
    }
  }
  model.init_facet_list();

  std::vector<Facet3Pt> facets;
  model.facets_computation(facets);

  ObservationSpace obs(4, 4, 0.0, 0.0, 150.0, 150.0);
  obs.set_reference_density(refDensity);
  obs.set_gravity_observation(ObservationMode::FlightElevation, 50.0);

  obs.compute_forward_field(facets, true);
  Grid trueGz = *obs.modeled_grid(FieldComponent::GZ);
  obs.set_observed_grid(FieldComponent::GZ, trueGz);

  const double guessDensity2 = 2700.0;
  b2->set_density(guessDensity2);

  FitResult res2 = obs.fit_density(
      b2, facets, FieldComponent::GZ, guessDensity2, 1e-5, FitMethod::brent);

  EXPECT_TRUE(res2.converged);
  EXPECT_NEAR(res2.optimal_parameter, trueDensity2, 1e-2);
  EXPECT_NEAR(b2->density().value(), trueDensity2, 1e-2);
  EXPECT_LT(res2.minimum_objective, 1e-4);
}

// ============================================================================
// 8. Tier 4 Localized Neighborhood Inversion Tests
// ============================================================================

TEST(ObservationSpaceTest, FitNeighborhoodCustomRadius) {
  const int rows = 15;
  const int cols = 15;
  const double dx = 100.0;
  const double dy = 100.0;

  Model model;
  model.init(rows, cols, 0.0, 0.0, dx, dy, -2000.0, 0.0);

  Body *body = model.create_body();
  body->set_density(2750.0);
  int bId = body->id();

  // Create a 5x5 body layer around center (7, 7)
  for (int r = 5; r <= 9; ++r) {
    for (int c = 5; c <= 9; ++c) {
      model.insert_body(r, c, -650.0, 200.0, false, bId);
    }
  }

  // Define true geometry: an uplifted circular patch centered at (7, 7)
  int targetRow = 7;
  int targetCol = 7;
  int vIdx = 1;

  model.move_vertex(vIdx, targetRow, targetCol, -550.0, BodyMoveType::Normal);
  model.move_vertex(vIdx, targetRow - 1, targetCol, -550.0, BodyMoveType::Normal);
  model.move_vertex(vIdx, targetRow + 1, targetCol, -550.0, BodyMoveType::Normal);
  model.move_vertex(vIdx, targetRow, targetCol - 1, -550.0, BodyMoveType::Normal);
  model.move_vertex(vIdx, targetRow, targetCol + 1, -550.0, BodyMoveType::Normal);

  model.init_facet_list();
  std::vector<Facet3Pt> trueFacets;
  model.facets_computation(trueFacets);

  ObservationSpace obs(rows, cols, 0.0, 0.0, dx, dy);
  obs.set_gravity_observation(ObservationMode::FlightElevation, 100.0);
  obs.compute_forward_field(trueFacets, true);
  Grid trueGz = *obs.modeled_grid(FieldComponent::GZ);
  obs.set_observed_grid(FieldComponent::GZ, trueGz);

  // Now perturb all 5 vertices to flat initial guess -650m
  model.move_vertex(vIdx, targetRow, targetCol, -650.0, BodyMoveType::Normal);
  model.move_vertex(vIdx, targetRow - 1, targetCol, -650.0, BodyMoveType::Normal);
  model.move_vertex(vIdx, targetRow + 1, targetCol, -650.0, BodyMoveType::Normal);
  model.move_vertex(vIdx, targetRow, targetCol - 1, -650.0, BodyMoveType::Normal);
  model.move_vertex(vIdx, targetRow, targetCol + 1, -650.0, BodyMoveType::Normal);

  model.init_facet_list();
  std::vector<Facet3Pt> initialFacets;
  model.facets_computation(initialFacets);
  obs.compute_forward_field(initialFacets, true);

  // Run Tier 4 neighborhood fit with custom radius 120m (covers 5 vertices: center + 4 neighbors)
  NeighborhoodFitSettings settings;
  settings.radius_mode = NeighborhoodRadiusMode::Custom;
  settings.custom_radius_m = 120.0;
  settings.smoothness_weight = 0.001;
  settings.tol = 1e-4;
  settings.max_iter = 15;

  NeighborhoodFitResult res = obs.fit_neighborhood(
      model, targetRow, targetCol, vIdx, FieldComponent::GZ, settings);

  EXPECT_TRUE(res.converged);
  EXPECT_EQ(res.vertices_fitted, 5);
  EXPECT_DOUBLE_EQ(res.chosen_radius_m, 120.0);
  EXPECT_LT(res.final_rms, res.initial_rms);
  // Significant RMS reduction (> 70%)
  EXPECT_LT(res.final_rms, 0.3 * res.initial_rms);
  // Center vertex moved from -650m towards -550m
  EXPECT_NEAR(model.z(targetRow, targetCol, vIdx), -550.0, 5.0);
  EXPECT_LT(res.elapsed_time_sec, 1.0);
}

TEST(ObservationSpaceTest, FitNeighborhoodAutomaticRadius) {
  const int rows = 15;
  const int cols = 15;
  const double dx = 100.0;
  const double dy = 100.0;

  Model model;
  model.init(rows, cols, 0.0, 0.0, dx, dy, -2000.0, 0.0);

  Body *body = model.create_body();
  body->set_density(2750.0);
  int bId = body->id();

  for (int r = 4; r <= 10; ++r) {
    for (int c = 4; c <= 10; ++c) {
      model.insert_body(r, c, -650.0, 200.0, false, bId);
    }
  }

  int targetRow = 7;
  int targetCol = 7;
  int vIdx = 1;

  // True uplift in a neighborhood around (7, 7)
  const double trueZ = -400.0;
  const double initialZ = -450.0;
  model.move_vertex(vIdx, targetRow, targetCol, trueZ, BodyMoveType::Normal);
  model.move_vertex(vIdx, targetRow - 1, targetCol, trueZ, BodyMoveType::Normal);
  model.move_vertex(vIdx, targetRow + 1, targetCol, trueZ, BodyMoveType::Normal);
  model.move_vertex(vIdx, targetRow, targetCol - 1, trueZ, BodyMoveType::Normal);
  model.move_vertex(vIdx, targetRow, targetCol + 1, trueZ, BodyMoveType::Normal);

  model.init_facet_list();
  std::vector<Facet3Pt> trueFacets;
  model.facets_computation(trueFacets);

  ObservationSpace obs(rows, cols, 0.0, 0.0, dx, dy);
  obs.set_gravity_observation(ObservationMode::FlightElevation, 100.0);
  obs.compute_forward_field(trueFacets, true);
  Grid trueGz = *obs.modeled_grid(FieldComponent::GZ);
  obs.set_observed_grid(FieldComponent::GZ, trueGz);

  // Reset to initial guess
  model.move_vertex(vIdx, targetRow, targetCol, initialZ, BodyMoveType::Normal);
  model.move_vertex(vIdx, targetRow - 1, targetCol, initialZ, BodyMoveType::Normal);
  model.move_vertex(vIdx, targetRow + 1, targetCol, initialZ, BodyMoveType::Normal);
  model.move_vertex(vIdx, targetRow, targetCol - 1, initialZ, BodyMoveType::Normal);
  model.move_vertex(vIdx, targetRow, targetCol + 1, initialZ, BodyMoveType::Normal);

  model.init_facet_list();
  std::vector<Facet3Pt> initialFacets;
  model.facets_computation(initialFacets);
  obs.compute_forward_field(initialFacets, true);

  // Run with Automatic radius mode
  NeighborhoodFitSettings settings;
  settings.radius_mode = NeighborhoodRadiusMode::Automatic;
  settings.sensitivity_cutoff = 0.05; // 5% decay
  settings.smoothness_weight = 0.001;
  settings.tol = 1e-4;

  NeighborhoodFitResult res = obs.fit_neighborhood(
      model, targetRow, targetCol, vIdx, FieldComponent::GZ, settings);

  EXPECT_TRUE(res.converged);
  EXPECT_GE(res.chosen_radius_m, 100.0);
  EXPECT_GE(res.vertices_fitted, 5);
  EXPECT_LT(res.final_rms, res.initial_rms);
  EXPECT_NEAR(model.z(targetRow, targetCol, vIdx), trueZ, 10.0);
  EXPECT_LT(res.elapsed_time_sec, 10.0);
}

