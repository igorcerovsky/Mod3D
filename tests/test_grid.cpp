#include <gtest/gtest.h>
#include "mod3d/Grid.h"
#include <cmath>
#include <cstdio>
#include <limits>

using namespace mod3d;

// ============================================================================
// 1. Grid Construction, Dimensions & Rotation Coordinates
// ============================================================================

TEST(GridComprehensiveTest, ConstructionAndRotatedCoordinates) {
    const size_t rows = 4;
    const size_t cols = 6;
    const double x0 = 1000.0;
    const double y0 = 2000.0;
    const double dx = 100.0;
    const double dy = 100.0;
    const double rotDeg = 30.0; // 30 degrees counter-clockwise rotation

    Grid g(rows, cols, x0, y0, dx, dy, rotDeg);
    EXPECT_EQ(g.rows(), rows);
    EXPECT_EQ(g.cols(), cols);
    EXPECT_EQ(g.size(), rows * cols);
    EXPECT_FALSE(g.empty());
    EXPECT_DOUBLE_EQ(g.rotation(), rotDeg);

    // Analytical verification of rotated coordinates:
    // X = x0 + (col*dx)*cos(rad) - (row*dy)*sin(rad)
    // Y = y0 + (col*dx)*sin(rad) + (row*dy)*cos(rad)
    const double pi = 3.14159265358979323846;
    const double rad = rotDeg * pi / 180.0;
    const double cRot = std::cos(rad);
    const double sRot = std::sin(rad);

    for (size_t r = 0; r < rows; ++r) {
        for (size_t c = 0; c < cols; ++c) {
            double expectedX = x0 + (c * dx) * cRot - (r * dy) * sRot;
            double expectedY = y0 + (c * dx) * sRot + (r * dy) * cRot;

            EXPECT_NEAR(g.getX(r, c), expectedX, 1e-12);
            EXPECT_NEAR(g.getY(r, c), expectedY, 1e-12);

            Point3D pt = g.getPoint(r, c);
            EXPECT_NEAR(pt.x, expectedX, 1e-12);
            EXPECT_NEAR(pt.y, expectedY, 1e-12);
            EXPECT_DOUBLE_EQ(pt.z, 0.0);
        }
    }
}

// ============================================================================
// 2. Statistics & Dummy Handling
// ============================================================================

TEST(GridComprehensiveTest, StatisticsAndDummyExclusion) {
    Grid g(3, 3, 0.0, 0.0, 10.0, 10.0);

    // Fill with values: 1, 2, 3, 4, 5, 6, 7, 8, and one dummy
    double val = 1.0;
    for (size_t r = 0; r < 3; ++r) {
        for (size_t c = 0; c < 3; ++c) {
            g(r, c) = val++;
        }
    }
    g(2, 2) = GRID_DUMMY; // Last cell is dummy

    EXPECT_TRUE(g.isDummy(2, 2));
    EXPECT_FALSE(g.isDummy(0, 0));

    // Stats must strictly exclude dummy:
    // Active values: 1, 2, 3, 4, 5, 6, 7, 8 (count = 8)
    // Min = 1.0, Max = 8.0, Sum = 36.0, Mean = 36 / 8 = 4.5
    // SumSq = 1 + 4 + 9 + 16 + 25 + 36 + 49 + 64 = 204
    // RMS = sqrt(204 / 8) = sqrt(25.5) = 5.049752469
    EXPECT_DOUBLE_EQ(g.getMin(), 1.0);
    EXPECT_DOUBLE_EQ(g.getMax(), 8.0);
    EXPECT_DOUBLE_EQ(g.getMean(), 4.5);
    EXPECT_NEAR(g.getRMS(), std::sqrt(25.5), 1e-10);
}

// ============================================================================
// 3. Scalar and Grid-to-Grid Arithmetic Operators
// ============================================================================

TEST(GridComprehensiveTest, ScalarAndGridArithmetic) {
    Grid g1(2, 2, 0.0, 0.0, 10.0, 10.0);
    g1(0, 0) = 10.0; g1(0, 1) = 20.0;
    g1(1, 0) = 30.0; g1(1, 1) = GRID_DUMMY;

    // Scalar += 5
    g1 += 5.0;
    EXPECT_DOUBLE_EQ(g1(0, 0), 15.0);
    EXPECT_DOUBLE_EQ(g1(0, 1), 25.0);
    EXPECT_DOUBLE_EQ(g1(1, 0), 35.0);
    EXPECT_TRUE(g1.isDummy(1, 1)); // Dummies must remain untouched

    // Scalar *= 2
    g1 *= 2.0;
    EXPECT_DOUBLE_EQ(g1(0, 0), 30.0);
    EXPECT_DOUBLE_EQ(g1(0, 1), 50.0);
    EXPECT_DOUBLE_EQ(g1(1, 0), 70.0);
    EXPECT_TRUE(g1.isDummy(1, 1));

    // Grid-to-Grid subtraction (g1 - g2)
    Grid g2(2, 2, 0.0, 0.0, 10.0, 10.0);
    g2(0, 0) = 5.0;  g2(0, 1) = 10.0;
    g2(1, 0) = 15.0; g2(1, 1) = 20.0;

    g1 -= g2;
    EXPECT_DOUBLE_EQ(g1(0, 0), 25.0);
    EXPECT_DOUBLE_EQ(g1(0, 1), 40.0);
    EXPECT_DOUBLE_EQ(g1(1, 0), 55.0);
    EXPECT_TRUE(g1.isDummy(1, 1));
}

// ============================================================================
// 4. Surfer 6 Binary (DSBB) File Roundtrip
// ============================================================================

TEST(GridComprehensiveTest, Surfer6BinaryRoundtrip) {
    const size_t rows = 5;
    const size_t cols = 7;
    Grid original(rows, cols, 50000.0, 100000.0, 250.0, 250.0, 0.0);

    for (size_t r = 0; r < rows; ++r) {
        for (size_t c = 0; c < cols; ++c) {
            original(r, c) = static_cast<double>(r * 100 + c * 10) + 0.5;
        }
    }

    const std::string tmpBinFile = "scratch_test_grid.bin.grd";
    ASSERT_TRUE(original.saveSrf6Binary(tmpBinFile));

    Grid loaded;
    ASSERT_TRUE(loaded.loadSrf6Binary(tmpBinFile));

    EXPECT_EQ(loaded.rows(), original.rows());
    EXPECT_EQ(loaded.cols(), original.cols());
    EXPECT_DOUBLE_EQ(loaded.x0(), original.x0());
    EXPECT_DOUBLE_EQ(loaded.y0(), original.y0());
    EXPECT_DOUBLE_EQ(loaded.xSize(), original.xSize());
    EXPECT_DOUBLE_EQ(loaded.ySize(), original.ySize());

    // Single precision float storage in Surfer 6 binary: expect 1e-4 relative precision
    for (size_t r = 0; r < rows; ++r) {
        for (size_t c = 0; c < cols; ++c) {
            EXPECT_NEAR(loaded(r, c), original(r, c), 1e-4);
        }
    }

    std::remove(tmpBinFile.c_str());
}
