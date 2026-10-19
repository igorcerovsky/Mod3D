#include <gtest/gtest.h>
#include "mod3d/Model.h"
#include "mod3d/Grid.h"
#include <vector>
#include <fstream>
#include <sstream>

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

    // Column (2, 2) should now have 4 points: relief (0), body top (1), body bot (2), hell (3)
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
        if (fct.dSign < 0.0) hasNegative = true;
        if (fct.dSign > 0.0) hasPositive = true;
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
        if (line1.empty()) continue;
        if (!std::getline(file, line2)) break;
        // Optional empty line
        std::getline(file, line3);

        // Parse line 2 vertex coordinates: x0, y0, z0 : x1, y1, z1 : x2, y2, z2
        // Replace ':' and ',' with spaces
        for (char &c : line2) {
            if (c == ':' || c == ',') c = ' ';
        }
        std::istringstream iss(line2);
        double x0, y0, z0, x1, y1, z1, x2, y2, z2;
        if (iss >> x0 >> y0 >> z0 >> x1 >> y1 >> z1 >> x2 >> y2 >> z2) {
            Facet3Pt fct(Point3D(x0, y0, z0), Point3D(x1, y1, z1), Point3D(x2, y2, z2));
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

    // 2. Horizontal bottom-facing facet (swapping vertex 1 and 2 to invert winding)
    // Looking from below (-Z): (0,0,-1000) -> (0,1000,-1000) -> (1000,0,-1000)
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

    // The field integral from the reversed facet is the exact negative of the original
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

    // Center of mass of the modeled body: columns (1,1) to (2,2) span [0, 500] in X and Y
    const Point3D bodyCenter(250.0, 250.0, -600.0);

    // For every facet belonging to this body:
    // The outward normal must point AWAY from the body center: (centroid - center) * normal >= 0
    for (const auto &fct : facets) {
        Point3D n = fct.normal();
        EXPECT_NEAR(n.norm(), 1.0, 1e-9);

        Point3D cntr = fct.center();

        Point3D outwardDir = cntr - bodyCenter;
        double dot = outwardDir * n;
        // Direction vector dotted with normal must be non-negative (pointing outward)
        EXPECT_GE(dot, -1e-6)
            << "Facet normal points inward! Normal=(" << n.x << "," << n.y << "," << n.z
            << "), Centroid=(" << cntr.x << "," << cntr.y << "," << cntr.z << ")";
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

    // 3. Split move (Ctrl+Drag) UPWARDS: unsnaps top vertex upwards towards relief
    vIdx = 1;
    res = model.move_vertex(vIdx, 2, 2, -450.0, BodyMoveType::Split);
    EXPECT_EQ(res, 0);
    EXPECT_EQ(vIdx, 1);
    EXPECT_DOUBLE_EQ(model.z(2, 2, 1), -450.0); // Top moved up
    EXPECT_DOUBLE_EQ(model.z(2, 2, 2), -700.0); // Bot remained in place! Unsnapped!

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
    EXPECT_EQ(vIdx, 2); // updated to bottom index!
    EXPECT_DOUBLE_EQ(model.z(2, 2, 1), -450.0); // Top remained in place!
    EXPECT_DOUBLE_EQ(model.z(2, 2, 2), -800.0); // Bot moved down! Unsnapped!

    // 6. Snap together at -800.0 and test unsnapping when starting from bottom index (2)
    topIdx = 1;
    model.move_vertex(topIdx, 2, 2, -800.0, BodyMoveType::Normal);
    EXPECT_DOUBLE_EQ(model.z(2, 2, 1), -800.0);
    EXPECT_DOUBLE_EQ(model.z(2, 2, 2), -800.0);

    vIdx = 2; // clicked bottom index
    res = model.move_vertex(vIdx, 2, 2, -550.0, BodyMoveType::Split);
    EXPECT_EQ(res, 0);
    EXPECT_EQ(vIdx, 1); // shifted to top index
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

    // Column (1, 1) must now only have 2 points (relief and hell), body 1 points removed
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

