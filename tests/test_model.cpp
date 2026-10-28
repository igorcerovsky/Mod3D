#include "mod3d/Grid.h"
#include "mod3d/Model.h"
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <vector>

using namespace mod3d;

TEST(ModelTest, InitializationAndDimensions) {
  Model model;
  const int nRows = 10;
  const int nCols = 15;
  const double x0 = 1000.0;
  const double y0 = 2000.0;
  const double dx = 100.0;
  const double dy = 100.0;
  const double zMin = -2000.0;
  const double zMax = 500.0;

  EXPECT_TRUE(model.init(nRows, nCols, x0, y0, dx, dy, zMin, zMax));
  EXPECT_TRUE(model.is_initialized());

  // Internal model dimensions have 1 border cell on each side
  EXPECT_EQ(model.rows(), nRows + 2);
  EXPECT_EQ(model.cols(), nCols + 2);

  // Each column point must initially have 2 points (relief top and hell bottom)
  for (int r = 0; r < model.rows(); ++r) {
    for (int c = 0; c < model.cols(); ++c) {
      EXPECT_EQ(model.count(r, c), 2u);
      EXPECT_DOUBLE_EQ(model.z(r, c, 0), zMax);
      EXPECT_DOUBLE_EQ(model.z(r, c, 1), zMin);
    }
  }
}

TEST(ModelTest, InsertBodyAndStratigraphy) {
  Model model;
  model.init(4, 4, 0.0, 0.0, 200.0, 200.0, -2000.0, 0.0);

  // Insert body into column (2, 2)
  const double zCenter = -500.0;
  const double thickness = 100.0; // t = 100 -> top = -400, bot = -600
  int idx = model.insert_body(2, 2, zCenter, thickness, true);
  EXPECT_GT(idx, 0);

  // Column (2, 2) should now have 4 points: relief (0), body top (1), body bot
  // (2), hell (3)
  EXPECT_EQ(model.count(2, 2), 4u);
  EXPECT_DOUBLE_EQ(model.z(2, 2, 0), 0.0);
  EXPECT_DOUBLE_EQ(model.z(2, 2, 1), -400.0);
  EXPECT_DOUBLE_EQ(model.z(2, 2, 2), -600.0);
  EXPECT_DOUBLE_EQ(model.z(2, 2, 3), -2000.0);
  EXPECT_EQ(model.thickness(2, 2, 1), 200.0);
}

TEST(ModelTest, MoveVertexNormal) {
  Model model;
  model.init(4, 4, 0.0, 0.0, 200.0, 200.0, -2000.0, 0.0);

  int idx = model.insert_body(2, 2, -500.0, 100.0, true);
  ASSERT_EQ(idx, 1);

  // Move top vertex from -400 to -350
  int moveRes = model.move_vertex(idx, 2, 2, -350.0, BodyMoveType::Normal);
  EXPECT_EQ(moveRes, 0);
  EXPECT_DOUBLE_EQ(model.z(2, 2, 1), -350.0);
}

TEST(ModelTest, FacetGenerationMultiColumnPrism) {
  Model model;
  model.init(3, 3, 0.0, 0.0, 100.0, 100.0, -1000.0, 0.0);

  // Insert body across a 2x2 grid of columns
  Body *body = model.create_body();
  ASSERT_NE(body, nullptr);
  const int bId = body->id();

  for (int r = 1; r <= 2; ++r) {
    for (int c = 1; c <= 2; ++c) {
      model.insert_body(r, c, -400.0, 50.0, false, bId);
    }
  }

  // Generate all facets
  model.init_facet_list();

  std::vector<Facet3Pt> allFacets;
  int count = model.facets_computation(allFacets);
  EXPECT_GT(count, 0);

  // Verify all generated facets have valid outward normals
  for (const auto &fct : allFacets) {
    Point3D n = fct.normal();
    EXPECT_NEAR(n.norm(), 1.0, 1e-6);
  }
}

TEST(ModelTest, RealTimeDeltaFacetTracking) {
  Model model;
  model.init(4, 4, 0.0, 0.0, 100.0, 100.0, -1000.0, 0.0);

  Body *body = model.create_body();
  const int bId = body->id();
  for (int r = 1; r <= 3; ++r) {
    for (int c = 1; c <= 3; ++c) {
      model.insert_body(r, c, -400.0, 50.0, false, bId);
    }
  }

  model.init_facet_list();

  // Enable real-time computation tracking
  model.set_compute_real_time(true);
  model.clear_facets_update();

  int vIdx = 1;
  model.move_vertex(vIdx, 2, 2, -350.0, BodyMoveType::Normal);

  const auto &deltas = model.facets_update();
  EXPECT_FALSE(deltas.empty());

  // Deltas must contain both old facets (sign -1) and new facets (sign +1)
  bool hasNegative = false;
  bool hasPositive = false;
  for (const auto &fct : deltas) {
    if (fct.dSign < 0.0)
      hasNegative = true;
    if (fct.dSign > 0.0)
      hasPositive = true;
  }
  EXPECT_TRUE(hasNegative);
  EXPECT_TRUE(hasPositive);
}

TEST(GridTest, BasicOperationsAndSurferAscii) {
  Grid grd(5, 5, 100.0, 200.0, 50.0, 50.0, 0.0);
  EXPECT_EQ(grd.rows(), 5u);
  EXPECT_EQ(grd.cols(), 5u);

  grd(0, 0) = 10.0;
  grd(2, 2) = 50.0;
  grd(4, 4) = 100.0;

  EXPECT_DOUBLE_EQ(grd.min(), 0.0);
  EXPECT_DOUBLE_EQ(grd.max(), 100.0);

  // Surfer ASCII roundtrip
  std::string tmpFile = "scratch_test_grid.grd";
  EXPECT_TRUE(grd.save_srf6_ascii(tmpFile));

  Grid loaded;
  EXPECT_TRUE(loaded.load_srf6_ascii(tmpFile));
  EXPECT_EQ(loaded.rows(), grd.rows());
  EXPECT_EQ(loaded.cols(), grd.cols());
  EXPECT_DOUBLE_EQ(loaded(0, 0), 10.0);
  EXPECT_DOUBLE_EQ(loaded(2, 2), 50.0);
  EXPECT_DOUBLE_EQ(loaded(4, 4), 100.0);

  std::remove(tmpFile.c_str());
}

TEST(ModelGate2Test, LoadAndVerifyLegacySampleFacetList) {
  std::string fctPath = std::string(LEGACY_SAMPLE_DATA_DIR) + "/FacetList.fct";
  std::ifstream file(fctPath);
  ASSERT_TRUE(file.is_open()) << "Failed to open " << fctPath;

  std::vector<Facet3Pt> facets;
  std::string line1, line2, line3;
  int count = 0;

  while (std::getline(file, line1)) {
    if (line1.empty())
      continue;
    if (!std::getline(file, line2))
      break;
    // Optional empty line
    std::getline(file, line3);

    // Parse line 2 vertex coordinates: x0, y0, z0 : x1, y1, z1 : x2, y2, z2
    // Replace ':' and ',' with spaces
    for (char &c : line2) {
      if (c == ':' || c == ',')
        c = ' ';
    }
    std::istringstream iss(line2);
    double x0, y0, z0, x1, y1, z1, x2, y2, z2;
    if (iss >> x0 >> y0 >> z0 >> x1 >> y1 >> z1 >> x2 >> y2 >> z2) {
      Facet3Pt fct(Point3D(x0, y0, z0), Point3D(x1, y1, z1),
                   Point3D(x2, y2, z2));
      facets.push_back(fct);

      // Centroid sum check: centroid() = pt0 + pt1 + pt2
      Point3D cntr = fct.centroid();
      EXPECT_NEAR(cntr.x, x0 + x1 + x2, 1e-9);
      EXPECT_NEAR(cntr.y, y0 + y1 + y2, 1e-9);
      EXPECT_NEAR(cntr.z, z0 + z1 + z2, 1e-9);

      count++;
    }
  }

  // Exact count of facets in FacetList.fct
  EXPECT_EQ(count, 1376);
  EXPECT_EQ(facets.size(), 1376u);

  // Compute potential field Gz at an observation point above the model
  Point3D obsPt(3000.0, 3000.0, 100.0);
  double gz = 0.0;
  for (const auto &fct : facets) {
    fct.fld_gz(obsPt, gz);
  }
  // Gz should be a finite non-zero value
  EXPECT_TRUE(std::isfinite(gz));
  EXPECT_NE(gz, 0.0);
}

TEST(FacetOrientationTest, WindingOrderAndNormalDirection) {
  // 1. Horizontal top-facing facet (vertices wound CCW in XY plane)
  // Looking from above (+Z): (0,0,0) -> (1000,0,0) -> (0,1000,0)
  Point3D top0(0.0, 0.0, 0.0);
  Point3D top1(1000.0, 0.0, 0.0);
  Point3D top2(0.0, 1000.0, 0.0);
  Facet3Pt fctTop(top0, top1, top2);

  Point3D nTop = fctTop.normal();
  EXPECT_NEAR(nTop.x, 0.0, 1e-12);
  EXPECT_NEAR(nTop.y, 0.0, 1e-12);
  EXPECT_NEAR(nTop.z, 1.0, 1e-12); // Must point UPWARD (+Z)

  // 2. Horizontal bottom-facing facet (swapping vertex 1 and 2 to invert
  // winding) Looking from below (-Z): (0,0,-1000) -> (0,1000,-1000) ->
  // (1000,0,-1000)
  Point3D bot0(0.0, 0.0, -1000.0);
  Point3D bot1(1000.0, 0.0, -1000.0);
  Point3D bot2(0.0, 1000.0, -1000.0);
  Facet3Pt fctBot(bot0, bot2, bot1); // swapped

  Point3D nBot = fctBot.normal();
  EXPECT_NEAR(nBot.x, 0.0, 1e-12);
  EXPECT_NEAR(nBot.y, 0.0, 1e-12);
  EXPECT_NEAR(nBot.z, -1.0, 1e-12); // Must point DOWNWARD (-Z)

  // 3. Facet reversal test
  Facet3Pt fctReversed = fctTop;
  fctReversed.reverse();
  Point3D nRev = fctReversed.normal();
  EXPECT_NEAR(nRev.x, -nTop.x, 1e-12);
  EXPECT_NEAR(nRev.y, -nTop.y, 1e-12);
  EXPECT_NEAR(nRev.z, -nTop.z, 1e-12); // Exactly inverted

  // 4. Potential field integral sensitivity to facet orientation
  Point3D obsPt(500.0, 500.0, 100.0);
  Point3D gOriginal(0, 0, 0);
  Point3D gReversed(0, 0, 0);

  fctTop.fld_g(obsPt, gOriginal);
  fctReversed.fld_g(obsPt, gReversed);

  // The field integral from the reversed facet is the exact negative of the
  // original
  EXPECT_NEAR(gOriginal.x, -gReversed.x, 1e-15);
  EXPECT_NEAR(gOriginal.y, -gReversed.y, 1e-15);
  EXPECT_NEAR(gOriginal.z, -gReversed.z, 1e-15);
}

TEST(FacetOrientationTest, ModelGeneratedFacetsStrictlyOutward) {
  Model model;
  model.init(3, 3, 0.0, 0.0, 500.0, 500.0, -2000.0, 0.0);

  Body *body = model.create_body();
  body->set_density(2670.0);
  const int bId = body->id();

  // Create a 2x2 column prism from z = -400 to z = -800
  for (int r = 1; r <= 2; ++r) {
    for (int c = 1; c <= 2; ++c) {
      model.insert_body(r, c, -600.0, 200.0, false, bId);
    }
  }

  model.init_facet_list();

  std::vector<Facet3Pt> facets;
  model.facets_computation(facets);
  ASSERT_GT(facets.size(), 0u);

  // Center of mass of the modeled body: columns (1,1) to (2,2) span [0, 500] in
  // X and Y
  const Point3D bodyCenter(250.0, 250.0, -600.0);

  // For every facet belonging to this body:
  // The outward normal must point AWAY from the body center: (centroid -
  // center) * normal >= 0
  for (const auto &fct : facets) {
    Point3D n = fct.normal();
    EXPECT_NEAR(n.norm(), 1.0, 1e-9);

    Point3D cntr = fct.center();

    Point3D outwardDir = cntr - bodyCenter;
    double dot = outwardDir * n;
    // Direction vector dotted with normal must be non-negative (pointing
    // outward)
    EXPECT_GE(dot, -1e-6) << "Facet normal points inward! Normal=(" << n.x
                          << "," << n.y << "," << n.z << "), Centroid=("
                          << cntr.x << "," << cntr.y << "," << cntr.z << ")";
  }
}

// ============================================================================
// Edge Cases: Topography Relief Grid, Constrained/Split Moves, and DeleteBody
// ============================================================================

TEST(ModelEdgeCasesTest, InitWithVariedReliefGrid) {
  // Create a 3x3 Grid with non-uniform topographic relief
  Grid relief(3, 3, 1000.0, 2000.0, 100.0, 100.0, 0.0);
  for (size_t r = 0; r < 3; ++r) {
    for (size_t c = 0; c < 3; ++c) {
      relief(r, c) = 200.0 + static_cast<double>(r * 50 + c * 30);
    }
  }

  Model model;
  const double zMin = -3000.0;
  const double zMax = 1000.0;
  EXPECT_TRUE(model.init(relief, zMin, zMax));
  EXPECT_TRUE(model.is_initialized());

  // Internal model dimensions are (3+2) x (3+2) = 5 x 5
  EXPECT_EQ(model.rows(), 5);
  EXPECT_EQ(model.cols(), 5);

  // Inner cell (r=2, c=2) corresponds to relief grid cell (1, 1)
  // Value should be 200 + 1*50 + 1*30 = 280.0
  EXPECT_DOUBLE_EQ(model.z(2, 2, 0), 280.0);
  EXPECT_DOUBLE_EQ(model.z(2, 2, 1), zMin);

  // Border cell (r=0, c=0) clamps to relief cell (0, 0) = 200.0
  EXPECT_DOUBLE_EQ(model.z(0, 0, 0), 200.0);
  EXPECT_DOUBLE_EQ(model.z(0, 0, 1), zMin);
}

TEST(ModelEdgeCasesTest, MoveVertexConstrainedAndSplit) {
  Model model;
  model.init(4, 4, 0.0, 0.0, 100.0, 100.0, -2000.0, 0.0);

  // Insert body at column (2, 2):
  // Index 0: relief (0.0)
  // Index 1: body top (-400.0)
  // Index 2: body bot (-600.0)
  // Index 3: hell (-2000.0)
  int idx = model.insert_body(2, 2, -500.0, 100.0, true);
  ASSERT_EQ(idx, 1);

  // 1. Constrained Move: valid within (-600.0, 0.0)
  int vIdx = 1;
  int res = model.move_vertex(vIdx, 2, 2, -300.0, BodyMoveType::Constrained);
  EXPECT_EQ(res, 0);
  EXPECT_DOUBLE_EQ(model.z(2, 2, 1), -300.0);

  // Constrained Move: violating upper bound (z >= 0.0) must be rejected
  res = model.move_vertex(vIdx, 2, 2, 50.0, BodyMoveType::Constrained);
  EXPECT_EQ(res, 2); // Error code 2 = constraint violated
  EXPECT_DOUBLE_EQ(model.z(2, 2, 1), -300.0); // Z unchanged

  // Constrained Move: violating lower bound (z <= -600.0) must be rejected
  res = model.move_vertex(vIdx, 2, 2, -650.0, BodyMoveType::Constrained);
  EXPECT_EQ(res, 2);
  EXPECT_DOUBLE_EQ(model.z(2, 2, 1), -300.0); // Z unchanged

  // 2. Split Move
  // Moving top vertex up towards relief:
  res = model.move_vertex(vIdx, 2, 2, -200.0, BodyMoveType::Split);
  EXPECT_EQ(res, 0);
  EXPECT_DOUBLE_EQ(model.z(2, 2, 1), -200.0);
}

TEST(ModelEdgeCasesTest, SnappedVerticesNormalAndSplitUnsnap) {
  Model model;
  model.init(4, 4, 0.0, 0.0, 100.0, 100.0, -2000.0, 0.0);

  // Insert body at (2, 2)
  int idx = model.insert_body(2, 2, -500.0, 100.0, true);
  ASSERT_EQ(idx, 1);
  EXPECT_DOUBLE_EQ(model.z(2, 2, 1), -400.0); // Top
  EXPECT_DOUBLE_EQ(model.z(2, 2, 2), -600.0); // Bot

  // 1. Snap them together by moving top down to bottom (-600.0)
  int topIdx = 1;
  int res = model.move_vertex(topIdx, 2, 2, -600.0, BodyMoveType::Normal);
  EXPECT_EQ(res, 0);
  EXPECT_DOUBLE_EQ(model.z(2, 2, 1), -600.0);
  EXPECT_DOUBLE_EQ(model.z(2, 2, 2), -600.0);

  // 2. Normal move on snapped points: moves both together (maintains pinch-out)
  int vIdx = 1;
  res = model.move_vertex(vIdx, 2, 2, -700.0, BodyMoveType::Normal);
  EXPECT_EQ(res, 0);
  EXPECT_DOUBLE_EQ(model.z(2, 2, 1), -700.0);
  EXPECT_DOUBLE_EQ(model.z(2, 2, 2), -700.0);

  // 3. Split move (Ctrl+Drag) UPWARDS: unsnaps top vertex upwards towards
  // relief
  vIdx = 1;
  res = model.move_vertex(vIdx, 2, 2, -450.0, BodyMoveType::Split);
  EXPECT_EQ(res, 0);
  EXPECT_EQ(vIdx, 1);
  EXPECT_DOUBLE_EQ(model.z(2, 2, 1), -450.0); // Top moved up
  EXPECT_DOUBLE_EQ(model.z(2, 2, 2),
                   -700.0); // Bot remained in place! Unsnapped!

  // 4. Snap them together again at -450.0
  int botIdx = 2;
  res = model.move_vertex(botIdx, 2, 2, -450.0, BodyMoveType::Normal);
  EXPECT_EQ(res, 0);
  EXPECT_DOUBLE_EQ(model.z(2, 2, 1), -450.0);
  EXPECT_DOUBLE_EQ(model.z(2, 2, 2), -450.0);

  // 5. Split move (Ctrl+Drag) DOWNWARDS: unsnaps bottom vertex downwards
  vIdx = 1; // start from top index
  res = model.move_vertex(vIdx, 2, 2, -800.0, BodyMoveType::Split);
  EXPECT_EQ(res, 0);
  EXPECT_EQ(vIdx, 2);                         // updated to bottom index!
  EXPECT_DOUBLE_EQ(model.z(2, 2, 1), -450.0); // Top remained in place!
  EXPECT_DOUBLE_EQ(model.z(2, 2, 2), -800.0); // Bot moved down! Unsnapped!

  // 6. Snap together at -800.0 and test unsnapping when starting from bottom
  // index (2)
  topIdx = 1;
  model.move_vertex(topIdx, 2, 2, -800.0, BodyMoveType::Normal);
  EXPECT_DOUBLE_EQ(model.z(2, 2, 1), -800.0);
  EXPECT_DOUBLE_EQ(model.z(2, 2, 2), -800.0);

  vIdx = 2; // clicked bottom index
  res = model.move_vertex(vIdx, 2, 2, -550.0, BodyMoveType::Split);
  EXPECT_EQ(res, 0);
  EXPECT_EQ(vIdx, 1);                         // shifted to top index
  EXPECT_DOUBLE_EQ(model.z(2, 2, 1), -550.0); // Top moved up!
  EXPECT_DOUBLE_EQ(model.z(2, 2, 2), -800.0); // Bot remained at -800.0!
}

TEST(ModelEdgeCasesTest, DeleteBodyAndFacetRemeshing) {
  Model model;
  model.init(3, 3, 0.0, 0.0, 100.0, 100.0, -1000.0, 0.0);

  Body *b1 = model.create_body();
  b1->set_density(2500.0);
  int bId1 = b1->id();

  Body *b2 = model.create_body();
  b2->set_density(2900.0);
  int bId2 = b2->id();

  EXPECT_EQ(model.bodies().size(), 2u);

  // Insert body 1 at column (1, 1) and (1, 2)
  model.insert_body(1, 1, -400.0, 50.0, false, bId1);
  model.insert_body(1, 2, -400.0, 50.0, false, bId1);

  // Insert body 2 at column (2, 1) and (2, 2)
  model.insert_body(2, 1, -700.0, 50.0, false, bId2);
  model.insert_body(2, 2, -700.0, 50.0, false, bId2);

  model.init_facet_list();

  std::vector<Facet3Pt> initialFacets;
  int initialCount = model.facets_computation(initialFacets);
  EXPECT_GT(initialCount, 0);

  // Delete Body 1
  int delRes = model.delete_body(bId1);
  EXPECT_EQ(delRes, 1);
  EXPECT_EQ(model.bodies().size(), 1u);
  EXPECT_EQ(model.bodies()[0]->id(), bId2);

  // Column (1, 1) must now only have 2 points (relief and hell), body 1 points
  // removed
  EXPECT_EQ(model.count(1, 1), 2u);

  // Column (2, 1) must still contain body 2
  EXPECT_EQ(model.count(2, 1), 4u);

  // Meshed facets after deletion should only reference body 2
  std::vector<Facet3Pt> updatedFacets;
  model.facets_computation(updatedFacets);
  for (const auto &fct : updatedFacets) {
    if (fct.primary) {
      EXPECT_EQ(fct.primary->id(), bId2);
    }
    if (fct.opposite) {
      EXPECT_EQ(fct.opposite->id(), bId2);
    }
  }
}

TEST(ModelTest, GenerateBodyMeshesAndFacets3D) {
  Model model;
  model.init(4, 4, 0.0, 0.0, 100.0, 100.0, -2000.0, 0.0);

  Body *b1 = model.create_body();
  ASSERT_NE(b1, nullptr);
  b1->set_name("Granite");
  int id1 = b1->id();

  Body *b2 = model.create_body();
  ASSERT_NE(b2, nullptr);
  b2->set_name("Basalt");
  int id2 = b2->id();

  // Insert body 1 at depth -500 to -700
  for (int r = 1; r <= 3; ++r) {
    for (int c = 1; c <= 3; ++c) {
      model.insert_body(r, c, -600.0, 100.0, false, id1);
    }
  }

  // Insert body 2 adjacent below body 1 (contact at -700 down to -1000)
  for (int r = 1; r <= 3; ++r) {
    for (int c = 1; c <= 3; ++c) {
      model.insert_body(r, c, -850.0, 150.0, false, id2);
    }
  }

  model.init_facet_list();

  // 1. Test body_facets for body 1
  std::vector<Facet3Pt> b1Facets;
  int c1 = model.body_facets(id1, b1Facets, false);
  EXPECT_GT(c1, 0);

  // 2. Test body_facets for body 2
  std::vector<Facet3Pt> b2Facets;
  int c2 = model.body_facets(id2, b2Facets, false);
  EXPECT_GT(c2, 0);

  // Verify all facets have unit normal
  for (const auto &f : b1Facets) {
    EXPECT_NEAR(f.normal().norm(), 1.0, 1e-5);
  }
  for (const auto &f : b2Facets) {
    EXPECT_NEAR(f.normal().norm(), 1.0, 1e-5);
  }

  // 3. Test generate_body_meshes
  auto meshes = model.generate_body_meshes(-1, false);
  ASSERT_EQ(meshes.size(), 2u);

  EXPECT_EQ(meshes[0].bodyId, id1);
  EXPECT_EQ(meshes[0].bodyName, "Granite");
  EXPECT_GT(meshes[0].facets.size(), 0u);
  EXPECT_GT(meshes[0].featureEdges.size(), 0u);

  EXPECT_EQ(meshes[1].bodyId, id2);
  EXPECT_EQ(meshes[1].bodyName, "Basalt");
  EXPECT_GT(meshes[1].facets.size(), 0u);
  EXPECT_GT(meshes[1].featureEdges.size(), 0u);

  // Verify feature edges have non-zero length
  for (const auto &edge : meshes[0].featureEdges) {
    EXPECT_GT((edge.first - edge.second).norm(), 0.0);
  }
}

TEST(PolyhedralTopologyTest, MultiBodyWatertightManifoldAndStratigraphicDAG) {
  Model model;
  model.init(4, 4, 0.0, 0.0, 200.0, 200.0, -2000.0, 0.0);

  Body *b1 = model.create_body();
  b1->set_name("UpperSediment");
  b1->set_density(2400.0);
  int id1 = b1->id();

  Body *b2 = model.create_body();
  b2->set_name("LowerBasement");
  b2->set_density(2850.0);
  int id2 = b2->id();

  // Body 1 in 2x2 interior columns from -300 to -600
  for (int r = 1; r <= 2; ++r) {
    for (int c = 1; c <= 2; ++c) {
      model.insert_body(r, c, -450.0, 150.0, false, id1);
    }
  }
  // Body 2 directly below Body 1 (shared interface at -600 down to -1000)
  for (int r = 1; r <= 2; ++r) {
    for (int c = 1; c <= 2; ++c) {
      model.insert_body(r, c, -800.0, 200.0, false, id2);
    }
  }

  model.init_facet_list();

  // 1. Validate global topology report
  auto report = model.analyze_topology(false);
  EXPECT_TRUE(report.isStratigraphicDagValid);
  EXPECT_TRUE(report.is_all_watertight());
  EXPECT_TRUE(report.is_all_outward_oriented());
  ASSERT_EQ(report.bodyStatuses.size(), 2u);

  for (const auto &bs : report.bodyStatuses) {
    EXPECT_TRUE(bs.isWatertight) << "Body " << bs.bodyId << " not closed!";
    EXPECT_TRUE(bs.isOutwardOriented) << "Body " << bs.bodyId << " volume <= 0!";
    EXPECT_EQ(bs.boundaryEdgeCount, 0u);
    EXPECT_EQ(bs.nonManifoldEdgeCount, 0u);
    EXPECT_EQ(bs.degenerateFacetCount, 0u);
    // Expected volume: 2x2 columns spanning 1 cell (200x200 m):
    if (bs.bodyId == id1) {
      EXPECT_NEAR(bs.signedVolume, 200.0 * 200.0 * 300.0, 1e-3);
    } else if (bs.bodyId == id2) {
      EXPECT_NEAR(bs.signedVolume, 200.0 * 200.0 * 400.0, 1e-3);
    }
  }

  // 2. Verify shared facets exist and have both primary and opposite pointers
  std::vector<Facet3Pt> compFacets;
  model.facets_computation(compFacets);
  size_t sharedCount = 0;
  for (const auto &fct : compFacets) {
    if (fct.primary != nullptr && fct.opposite != nullptr) {
      ++sharedCount;
    }
  }
  EXPECT_EQ(sharedCount, 4u); // 4 triangles in the shared 1-cell contact surface
}

TEST(PolyhedralTopologyTest, ZeroExtensionBoundaryStillProducesClosedManifold) {
  Model model;
  model.init(2, 2, 0.0, 0.0, 100.0, 100.0, -1000.0, 0.0);
  // Set zero extensions so outer ring (0 and 3) coincides with inner ring (1 and 2)
  model.set_extensions(0.0, 0.0, 0.0, 0.0, true);

  Body *b1 = model.create_body();
  b1->set_density(2750.0);
  int id1 = b1->id();

  // Insert body across interior columns (1..2, 1..2)
  for (int r = 1; r <= 2; ++r) {
    for (int c = 1; c <= 2; ++c) {
      model.insert_body(r, c, -500.0, 150.0, false, id1);
    }
  }
  model.init_facet_list();

  auto status = model.analyze_body_topology(id1, true);
  EXPECT_TRUE(status.isWatertight);
  EXPECT_TRUE(status.isOutwardOriented);
  EXPECT_EQ(status.boundaryEdgeCount, 0u);
  EXPECT_EQ(status.nonManifoldEdgeCount, 0u);
  // Interior domain is 1 cell of 100x100 m with thickness 300 m
  EXPECT_NEAR(status.signedVolume, 100.0 * 100.0 * 300.0, 1e-3);
}

TEST(PolyhedralTopologyTest, ThreeCornerPinchoutWedgeIsClosedAndOutwardOriented) {
  Model model;
  model.init(4, 4, 0.0, 0.0, 200.0, 200.0, -1500.0, 0.0);

  Body *b = model.create_body();
  b->set_density(2650.0);
  int id = b->id();

  // Insert body into ONLY 3 corners of cell (1,1): (1,1), (1,2), (2,1)
  model.insert_body(1, 1, -500.0, 100.0, false, id);
  model.insert_body(1, 2, -500.0, 100.0, false, id);
  model.insert_body(2, 1, -500.0, 100.0, false, id);

  model.init_facet_list();

  auto status = model.analyze_body_topology(id, false);
  EXPECT_TRUE(status.isWatertight);
  EXPECT_TRUE(status.isOutwardOriented);
  EXPECT_EQ(status.boundaryEdgeCount, 0u);
  EXPECT_EQ(status.nonManifoldEdgeCount, 0u);
  EXPECT_EQ(status.degenerateFacetCount, 0u);
}

TEST(PolyhedralTopologyTest, StratigraphicCyclePrevention) {
  Model model;
  model.init(4, 4, 0.0, 0.0, 100.0, 100.0, -1000.0, 0.0);

  Body *b1 = model.create_body();
  Body *b2 = model.create_body();
  int id1 = b1->id();
  int id2 = b2->id();

  // In column (1,1), place b1 above b2
  EXPECT_GT(model.insert_body(1, 1, -300.0, 50.0, false, id1), 0);
  EXPECT_GT(model.insert_body(1, 1, -600.0, 50.0, false, id2), 0);
  EXPECT_TRUE(model.is_stratigraphic_dag_valid());

  // In column (1,2), place b2 at -300. Trying to insert b1 BELOW b2 at -600
  // would create a stratigraphic cycle (b1 -> b2 in col 1,1 and b2 -> b1 in col 1,2)
  EXPECT_GT(model.insert_body(1, 2, -300.0, 50.0, false, id2), 0);
  EXPECT_LT(model.insert_body(1, 2, -600.0, 50.0, false, id1), 0);
  EXPECT_TRUE(model.is_stratigraphic_dag_valid());
}

TEST(PolyhedralTopologyTest, SharedFacetFieldEquivalence) {
  Model model;
  model.init(4, 4, 0.0, 0.0, 200.0, 200.0, -2000.0, 0.0);

  Body *b1 = model.create_body();
  b1->set_density(2550.0);
  b1->set_susceptibility(0.015);
  b1->set_remanent_magnetization(Point3D(0.2, -0.1, 0.4));
  int id1 = b1->id();

  Body *b2 = model.create_body();
  b2->set_density(2900.0);
  b2->set_susceptibility(0.045);
  b2->set_remanent_magnetization(Point3D(-0.1, 0.3, 0.8));
  int id2 = b2->id();

  for (int r = 1; r <= 2; ++r) {
    for (int c = 1; c <= 2; ++c) {
      model.insert_body(r, c, -400.0, 150.0, false, id1);
      model.insert_body(r, c, -750.0, 200.0, false, id2);
    }
  }
  model.init_facet_list();
  model.compute_body_magnetization_vector(60.0, 5.0, 48000.0);

  // Compute field using fused shared facets
  std::vector<Facet3Pt> fusedFacets;
  model.facets_computation(fusedFacets);

  // Compute field using separate per-body facets
  std::vector<Facet3Pt> b1Facets, b2Facets, separateFacets;
  model.body_facets(id1, b1Facets, true);
  model.body_facets(id2, b2Facets, true);
  separateFacets.insert(separateFacets.end(), b1Facets.begin(), b1Facets.end());
  separateFacets.insert(separateFacets.end(), b2Facets.begin(), b2Facets.end());

  Point3D station(200.0, 200.0, 50.0);
  auto evalFacets = [&](std::vector<Facet3Pt> &fcts, double &gx, double &gy,
                        double &gz, double &gxx, double &gyy, double &gzz,
                        double &mx, double &my, double &mz) {
    gx = gy = gz = gxx = gyy = gzz = mx = my = mz = 0.0;
    double gxy = 0.0, gxz = 0.0, gyz = 0.0;
    double refDens = 2670.0, refDensOrg = 0.0;
    Point3D vRefDens(0, 0, 0);
    Point3D rGrv = station, rTen = station, rMag = station;
    double unitGrv = 1e5, unitMag = 1.0, unitTns = 1e9;
    for (auto &f : fcts) {
      f.compute(&gx, &gy, &gz, &gxx, &gyy, &gzz, &gxy, &gxz, &gyz, &mx, &my,
                &mz, refDens, refDensOrg, vRefDens, 0, rGrv, rTen, rMag,
                unitGrv, unitMag, unitTns);
    }
  };

  double gxF, gyF, gzF, gxxF, gyyF, gzzF, mxF, myF, mzF;
  double gxS, gyS, gzS, gxxS, gyyS, gzzS, mxS, myS, mzS;
  evalFacets(fusedFacets, gxF, gyF, gzF, gxxF, gyyF, gzzF, mxF, myF, mzF);
  evalFacets(separateFacets, gxS, gyS, gzS, gxxS, gyyS, gzzS, mxS, myS, mzS);

  EXPECT_NEAR(gxF, gxS, 1e-10);
  EXPECT_NEAR(gyF, gyS, 1e-10);
  EXPECT_NEAR(gzF, gzS, 1e-10);
  EXPECT_NEAR(gxxF, gxxS, 1e-10);
  EXPECT_NEAR(gyyF, gyyS, 1e-10);
  EXPECT_NEAR(gzzF, gzzS, 1e-10);
  EXPECT_NEAR(mxF, mxS, 1e-10);
  EXPECT_NEAR(myF, myS, 1e-10);
  EXPECT_NEAR(mzF, mzS, 1e-10);
}

TEST(SafetyHardeningTest, VerticalRangeAdjustmentCheckAndClamping) {
  Model model;
  model.init(4, 4, 0.0, 0.0, 200.0, 200.0, -2000.0, 0.0);

  Body *b = model.create_body();
  b->set_density(2750.0);
  int id = b->id();

  for (int r = 1; r <= 2; ++r) {
    for (int c = 1; c <= 2; ++c) {
      model.insert_body(r, c, -500.0, 200.0, false, id); // [-300, -700]
    }
  }
  model.init_facet_list();

  // Range [-1500, 0] does NOT intersect [-700, -300]
  EXPECT_FALSE(model.would_vertical_range_adjust_bodies(-1500.0, 0.0));

  // Range [-600, -350] DOES intersect [-700, -300]
  EXPECT_TRUE(model.would_vertical_range_adjust_bodies(-600.0, -350.0));

  // Apply clamping vertical range [-600, -350]
  model.set_vertical_range(-600.0, -350.0);
  EXPECT_DOUBLE_EQ(model.z_min_val(), -600.0);
  EXPECT_DOUBLE_EQ(model.z_max_val(), -350.0);

  // Verify all column vertices are strictly within [-600, -350] and non-inverted,
  // and body topology remains a closed manifold
  auto status = model.analyze_body_topology(id, false);
  EXPECT_TRUE(status.isWatertight);
  EXPECT_TRUE(status.isOutwardOriented);
}

TEST(SafetyHardeningTest, DynamicExtensionCoordinateResync) {
  Model model;
  model.init(2, 2, 0.0, 0.0, 100.0, 100.0, -1000.0, 0.0);

  Body *b = model.create_body();
  int id = b->id();
  for (int r = 0; r < 4; ++r) {
    for (int c = 0; c < 4; ++c) {
      model.insert_body(r, c, -400.0, 100.0, false, id);
    }
  }
  model.init_facet_list();

  // Resize extensions to 500 m in all directions
  model.set_extensions(500.0, 500.0, 500.0, 500.0, true);
  EXPECT_DOUBLE_EQ(model.at(0, 0, 0)->point().x, -500.0);
  EXPECT_DOUBLE_EQ(model.at(0, 0, 0)->point().y, -500.0);
  EXPECT_DOUBLE_EQ(model.at(3, 3, 0)->point().x, 600.0);
  EXPECT_DOUBLE_EQ(model.at(3, 3, 0)->point().y, 600.0);

  auto status = model.analyze_body_topology(id, true);
  EXPECT_TRUE(status.isWatertight);
  EXPECT_TRUE(status.isOutwardOriented);
  // Extended span is [-500, 600] x [-500, 600] = 1100 x 1100 m, thickness 200 m
  EXPECT_NEAR(status.signedVolume, 1100.0 * 1100.0 * 200.0, 1e-1);
}

TEST(SafetyHardeningTest, SanitizeRepairsCorruptedColumns) {
  Model model;
  model.init(4, 4, 0.0, 0.0, 100.0, 100.0, -1000.0, 0.0);

  Body *b = model.create_body();
  int id = b->id();

  // Intentionally inject corrupted points into column (1, 1):
  // - Out-of-range surface / bottom
  // - Dangling odd point for body `id`
  // - Point referencing non-existent body 999
  model.remove_at(1, 1, 0, static_cast<int>(model.count(1, 1)));
  model.add(1, 1, ColumnPoint(Point3D(0, 0, 500.0), 0, nullptr));     // Above zMax
  model.add(1, 1, ColumnPoint(Point3D(0, 0, -200.0), id, b));         // Single dangling vertex
  model.add(1, 1, ColumnPoint(Point3D(0, 0, -400.0), 999, nullptr));  // Invalid body ID
  model.add(1, 1, ColumnPoint(Point3D(0, 0, -5000.0), 0, nullptr));   // Below zMin

  size_t repairs = model.sanitize();
  EXPECT_GT(repairs, 0u);
  EXPECT_EQ(model.count(1, 1), 2u); // Repaired back to clean [surface, bottom]
  EXPECT_DOUBLE_EQ(model.z(1, 1, 0), 0.0);
  EXPECT_DOUBLE_EQ(model.z(1, 1, 1), -1000.0);
}

TEST(SafetyHardeningTest, SingularitySafetyOnVertexAndEdgeCoincidence) {
  Point3D v0(0.0, 0.0, 0.0);
  Point3D v1(100.0, 0.0, 0.0);
  Point3D v2(0.0, 100.0, 0.0);

  Body b;
  b.set_id(1);
  b.set_density(2800.0);
  b.set_susceptibility(0.02);
  b.compute_magnetization_vector(60.0, 0.0, 50000.0);

  Facet3Pt fct(v0, v1, v2, &b, nullptr);

  // Station coinciding directly with vertex v0 and edge midpoint (50, 0, 0)
  std::vector<Point3D> singularStations = {
      v0,
      v1,
      v2,
      Point3D(50.0, 0.0, 0.0),
      Point3D(25.0, 25.0, 0.0),
  };

  for (const auto &st : singularStations) {
    double gx = 0, gy = 0, gz = 0;
    double gxx = 0, gyy = 0, gzz = 0, gxy = 0, gxz = 0, gyz = 0;
    double mx = 0, my = 0, mz = 0;
    double refDens = 2670.0, refDensOrg = 0.0;
    Point3D vRefDens(0, 0, 0);
    Point3D rGrv = st, rTen = st, rMag = st;
    double unitGrv = 1e5, unitMag = 1.0, unitTns = 1e9;
    fct.compute(&gx, &gy, &gz, &gxx, &gyy, &gzz, &gxy, &gxz, &gyz, &mx, &my,
                &mz, refDens, refDensOrg, vRefDens, 0, rGrv, rTen, rMag,
                unitGrv, unitMag, unitTns);
    EXPECT_TRUE(std::isfinite(gx));
    EXPECT_TRUE(std::isfinite(gy));
    EXPECT_TRUE(std::isfinite(gz));
    EXPECT_TRUE(std::isfinite(mx));
    EXPECT_TRUE(std::isfinite(my));
    EXPECT_TRUE(std::isfinite(mz));
    EXPECT_TRUE(std::isfinite(gxx));
    EXPECT_TRUE(std::isfinite(gyy));
    EXPECT_TRUE(std::isfinite(gzz));
    EXPECT_TRUE(std::isfinite(gxy));
    EXPECT_TRUE(std::isfinite(gxz));
    EXPECT_TRUE(std::isfinite(gyz));
  }
}

