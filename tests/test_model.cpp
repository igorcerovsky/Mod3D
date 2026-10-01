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
    EXPECT_TRUE(model.isInitialized());

    // Internal model dimensions have 1 border cell on each side
    EXPECT_EQ(model.getRows(), nRows + 2);
    EXPECT_EQ(model.getCols(), nCols + 2);

    // Each column point must initially have 2 points (relief top and hell bottom)
    for (int r = 0; r < model.getRows(); ++r) {
        for (int c = 0; c < model.getCols(); ++c) {
            EXPECT_EQ(model.getCount(r, c), 2u);
            EXPECT_DOUBLE_EQ(model.getZ(r, c, 0), zMax);
            EXPECT_DOUBLE_EQ(model.getZ(r, c, 1), zMin);
        }
    }
}

TEST(ModelTest, InsertBodyAndStratigraphy) {
    Model model;
    model.init(4, 4, 0.0, 0.0, 200.0, 200.0, -2000.0, 0.0);

    // Insert body into column (2, 2)
    const double zCenter = -500.0;
    const double thickness = 100.0; // t = 100 -> top = -400, bot = -600
    int idx = model.insertBody(2, 2, zCenter, thickness, true);
    EXPECT_GT(idx, 0);

    // Column (2, 2) should now have 4 points: relief (0), body top (1), body bot (2), hell (3)
    EXPECT_EQ(model.getCount(2, 2), 4u);
    EXPECT_DOUBLE_EQ(model.getZ(2, 2, 0), 0.0);
    EXPECT_DOUBLE_EQ(model.getZ(2, 2, 1), -400.0);
    EXPECT_DOUBLE_EQ(model.getZ(2, 2, 2), -600.0);
    EXPECT_DOUBLE_EQ(model.getZ(2, 2, 3), -2000.0);
    EXPECT_EQ(model.getThickness(2, 2, 1), 200.0);
}

TEST(ModelTest, MoveVertexNormal) {
    Model model;
    model.init(4, 4, 0.0, 0.0, 200.0, 200.0, -2000.0, 0.0);

    int idx = model.insertBody(2, 2, -500.0, 100.0, true);
    ASSERT_EQ(idx, 1);

    // Move top vertex from -400 to -350
    int moveRes = model.moveVertex(idx, 2, 2, -350.0, BodyMoveType::Normal);
    EXPECT_EQ(moveRes, 0);
    EXPECT_DOUBLE_EQ(model.getZ(2, 2, 1), -350.0);
}

TEST(ModelTest, FacetGenerationMultiColumnPrism) {
    Model model;
    model.init(3, 3, 0.0, 0.0, 100.0, 100.0, -1000.0, 0.0);

    // Insert body across a 2x2 grid of columns
    Body *body = model.newBody();
    ASSERT_NE(body, nullptr);
    const int bId = body->GetID();

    for (int r = 1; r <= 2; ++r) {
        for (int c = 1; c <= 2; ++c) {
            model.insertBody(r, c, -400.0, 50.0, false, bId);
        }
    }

    // Generate all facets
    model.initFacetList();

    std::vector<Facet3Pt> allFacets;
    int count = model.getFacetsComputation(allFacets);
    EXPECT_GT(count, 0);

    // Verify all generated facets have valid outward normals
    for (const auto &fct : allFacets) {
        Point3D n = fct.Normal();
        EXPECT_NEAR(n.Abs(), 1.0, 1e-6);
    }
}

TEST(ModelTest, RealTimeDeltaFacetTracking) {
    Model model;
    model.init(4, 4, 0.0, 0.0, 100.0, 100.0, -1000.0, 0.0);

    Body *body = model.newBody();
    const int bId = body->GetID();
    for (int r = 1; r <= 3; ++r) {
        for (int c = 1; c <= 3; ++c) {
            model.insertBody(r, c, -400.0, 50.0, false, bId);
        }
    }

    model.initFacetList();

    // Enable real-time computation tracking
    model.setComputeRealTime(true);
    model.clearFacetsUpdate();

    int vIdx = 1;
    model.moveVertex(vIdx, 2, 2, -350.0, BodyMoveType::Normal);

    const auto &deltas = model.getFacetsUpdate();
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

    EXPECT_DOUBLE_EQ(grd.getMin(), 0.0);
    EXPECT_DOUBLE_EQ(grd.getMax(), 100.0);

    // Surfer ASCII roundtrip
    std::string tmpFile = "scratch_test_grid.grd";
    EXPECT_TRUE(grd.saveSrf6Ascii(tmpFile));

    Grid loaded;
    EXPECT_TRUE(loaded.loadSrf6Ascii(tmpFile));
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

            // Centroid sum check: Centroid() = pt0 + pt1 + pt2
            Point3D cntr = fct.Centroid();
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
        fct.Fld_Gz(obsPt, gz);
    }
    // Gz should be a finite non-zero value
    EXPECT_TRUE(std::isfinite(gz));
    EXPECT_NE(gz, 0.0);
}
