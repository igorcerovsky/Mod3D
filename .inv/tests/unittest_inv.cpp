#include <gtest/gtest.h>
#include "inv/inv.hpp"
#include <cmath>

TEST(Point3DTest, BasicVectorOperations) {
    inv::Point3D<double> p1(1.0, 2.0, 3.0);
    inv::Point3D<double> p2(4.0, 5.0, 6.0);

    auto sum = p1 + p2;
    EXPECT_DOUBLE_EQ(sum.x, 5.0);
    EXPECT_DOUBLE_EQ(sum.y, 7.0);
    EXPECT_DOUBLE_EQ(sum.z, 9.0);

    auto diff = p2 - p1;
    EXPECT_DOUBLE_EQ(diff.x, 3.0);
    EXPECT_DOUBLE_EQ(diff.y, 3.0);
    EXPECT_DOUBLE_EQ(diff.z, 3.0);

    EXPECT_DOUBLE_EQ(p1.dot(p2), 1.0 * 4.0 + 2.0 * 5.0 + 3.0 * 6.0);
    EXPECT_DOUBLE_EQ(p1.squared_norm(), 1.0 + 4.0 + 9.0);
    EXPECT_DOUBLE_EQ(p1.norm(), std::sqrt(14.0));

    auto cross = p1.cross(p2);
    // (2*6 - 3*5, 3*4 - 1*6, 1*5 - 2*4) = (-3, 6, -3)
    EXPECT_DOUBLE_EQ(cross.x, -3.0);
    EXPECT_DOUBLE_EQ(cross.y, 6.0);
    EXPECT_DOUBLE_EQ(cross.z, -3.0);
}

TEST(VoxelGridTest, IndexingAndCoordinates) {
    const std::size_t nx = 10, ny = 8, nz = 5;
    const double dx = 20.0, dy = 25.0, dz = 10.0;
    const double x0 = 100.0, y0 = 200.0, z0 = 50.0;

    inv::VoxelGrid<double> grid(nx, ny, nz, x0, y0, z0, dx, dy, dz);

    EXPECT_EQ(grid.nx(), nx);
    EXPECT_EQ(grid.ny(), ny);
    EXPECT_EQ(grid.nz(), nz);
    EXPECT_EQ(grid.total_cells(), nx * ny * nz);

    EXPECT_DOUBLE_EQ(grid.x_max(), x0 + nx * dx);
    EXPECT_DOUBLE_EQ(grid.y_max(), y0 + ny * dy);
    EXPECT_DOUBLE_EQ(grid.z_max(), z0 + nz * dz);
    EXPECT_DOUBLE_EQ(grid.cell_volume(), dx * dy * dz);

    // Test coordinate encoding & decoding
    for (std::size_t ix = 0; ix < nx; ++ix) {
        for (std::size_t iy = 0; iy < ny; ++iy) {
            for (std::size_t iz = 0; iz < nz; ++iz) {
                std::size_t idx = grid.index(ix, iy, iz);
                std::size_t dec_ix, dec_iy, dec_iz;
                grid.decode_index(idx, dec_ix, dec_iy, dec_iz);
                EXPECT_EQ(dec_ix, ix);
                EXPECT_EQ(dec_iy, iy);
                EXPECT_EQ(dec_iz, iz);
            }
        }
    }

    // Cell center check
    auto center000 = grid.cell_center(0, 0, 0);
    EXPECT_DOUBLE_EQ(center000.x, x0 + 0.5 * dx);
    EXPECT_DOUBLE_EQ(center000.y, y0 + 0.5 * dy);
    EXPECT_DOUBLE_EQ(center000.z, z0 + 0.5 * dz);
}

TEST(VoxelGridTest, ValueAccessAndStatistics) {
    inv::VoxelGrid<double> grid(4, 4, 4, 0.0, 0.0, 0.0, 10.0, 10.0, 10.0, 0.0);
    EXPECT_DOUBLE_EQ(grid.min_value(), 0.0);
    EXPECT_DOUBLE_EQ(grid.max_value(), 0.0);
    EXPECT_DOUBLE_EQ(grid.l2_norm(), 0.0);

    grid(1, 2, 3) = 2.5;
    grid(3, 1, 0) = -1.5;

    EXPECT_DOUBLE_EQ(grid(1, 2, 3), 2.5);
    EXPECT_DOUBLE_EQ(grid(3, 1, 0), -1.5);
    EXPECT_DOUBLE_EQ(grid.max_value(), 2.5);
    EXPECT_DOUBLE_EQ(grid.min_value(), -1.5);
    EXPECT_DOUBLE_EQ(grid.l2_norm(), std::sqrt(2.5 * 2.5 + 1.5 * 1.5));
}

TEST(ForwardKernelTest, AnalyticalPrismGravityProperties) {
    inv::Point3D<double> p_min(0.0, 0.0, 100.0);
    inv::Point3D<double> p_max(100.0, 100.0, 200.0);

    // Observation at surface directly above center
    inv::Point3D<double> obs_center(50.0, 50.0, 0.0);

    // Zero density contrast gives zero gravity
    double gz_zero = inv::ForwardKernel<double>::prism_gravity_gz(obs_center, p_min, p_max, 0.0);
    EXPECT_DOUBLE_EQ(gz_zero, 0.0);

    // Positive density contrast produces downward (positive gz) gravity anomaly
    double gz_1 = inv::ForwardKernel<double>::prism_gravity_gz(obs_center, p_min, p_max, 0.5);
    EXPECT_GT(gz_1, 0.0);

    // Linearity: doubling density contrast exactly doubles gz
    double gz_2 = inv::ForwardKernel<double>::prism_gravity_gz(obs_center, p_min, p_max, 1.0);
    EXPECT_NEAR(gz_2, 2.0 * gz_1, 1e-12);

    // Spatial symmetry: points placed symmetrically around the center have identical response
    inv::Point3D<double> obs_east(150.0, 50.0, 0.0);
    inv::Point3D<double> obs_west(-50.0, 50.0, 0.0);
    double gz_east = inv::ForwardKernel<double>::prism_gravity_gz(obs_east, p_min, p_max, 0.5);
    double gz_west = inv::ForwardKernel<double>::prism_gravity_gz(obs_west, p_min, p_max, 0.5);
    EXPECT_NEAR(gz_east, gz_west, 1e-12);

    // Distance attenuation: an observation point farther away has smaller gz
    inv::Point3D<double> obs_far(500.0, 50.0, 0.0);
    double gz_far = inv::ForwardKernel<double>::prism_gravity_gz(obs_far, p_min, p_max, 0.5);
    EXPECT_LT(gz_far, gz_1);
}

TEST(FocusingMetricsTest, MinimumSupportAndGradientSupport) {
    inv::VoxelGrid<double> grid(10, 10, 10, 0.0, 0.0, 0.0, 10.0, 10.0, 10.0, 0.0);

    // Zero grid has S_MS = 0 and S_MGS = 0
    double ms_zero = inv::FocusingEvaluator<double>::compute_minimum_support(grid, 1e-3);
    double mgs_zero = inv::FocusingEvaluator<double>::compute_minimum_gradient_support(grid, 1e-3);
    EXPECT_DOUBLE_EQ(ms_zero, 0.0);
    EXPECT_DOUBLE_EQ(mgs_zero, 0.0);

    // A single cell with high value (m >> e) should contribute approx 1.0 to S_MS
    grid(5, 5, 5) = 1.0;
    double ms_one = inv::FocusingEvaluator<double>::compute_minimum_support(grid, 1e-3);
    // (1.0)^2 / (1.0^2 + 1e-6) ~= 1.0 - 1e-6
    EXPECT_NEAR(ms_one, 1.0, 1e-4);

    // Gradient support should be non-zero because of the sharp step around cell (5,5,5)
    double mgs_one = inv::FocusingEvaluator<double>::compute_minimum_gradient_support(grid, 1e-3);
    EXPECT_GT(mgs_one, 0.0);

    // IRLS weights
    std::vector<double> weights;
    inv::FocusingEvaluator<double>::compute_irls_weights(grid, weights, 1e-3);
    EXPECT_EQ(weights.size(), grid.total_cells());

    // In background cells (value 0), weight = 1/e = 1000
    std::size_t idx_bg = grid.index(0, 0, 0);
    EXPECT_NEAR(weights[idx_bg], 1000.0, 1e-5);

    // In anomalous cell (value 1.0), weight = 1 / sqrt(1^2 + e^2) ~= 1.0
    std::size_t idx_anom = grid.index(5, 5, 5);
    EXPECT_NEAR(weights[idx_anom], 1.0, 1e-3);
}

TEST(InversionEngineTest, RunInversionExecution) {
    inv::InversionConfig cfg;
    cfg.nx = 8;
    cfg.ny = 8;
    cfg.nz = 4;
    cfg.dx = 50.0;
    cfg.dy = 50.0;
    cfg.dz = 50.0;

    inv::InversionEngine engine(cfg);
    std::vector<double> obs(8 * 8, 1.5); // uniform 1.5 mGal field

    inv::VoxelGrid<double> out_model;
    inv::InversionStats stats;

    bool ok = engine.invert_gravity_field(obs, 8, 8, out_model, stats);
    EXPECT_TRUE(ok);
    EXPECT_EQ(out_model.nx(), 8);
    EXPECT_EQ(out_model.ny(), 8);
    EXPECT_EQ(out_model.nz(), 4);
    EXPECT_GT(stats.elapsed_time_ms, 0.0);
    EXPECT_GE(out_model.max_value(), cfg.min_density);
    EXPECT_LE(out_model.max_value(), cfg.max_density);
}
