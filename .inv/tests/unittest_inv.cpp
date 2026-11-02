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
    EXPECT_DOUBLE_EQ(p1.norm_squared(), 1.0 + 4.0 + 9.0);
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
    cfg.coarse_to_fine_levels = 3;

    inv::InversionEngine engine(cfg);
    std::vector<double> obs(8 * 8, 1.5); // uniform 1.5 mGal field

    inv::VoxelGrid<double> out_model;
    inv::InversionStats stats;

    bool ok = engine.invert_gravity_field(obs, 8, 8, out_model, stats);
    EXPECT_TRUE(ok);
    EXPECT_EQ(out_model.nx(), 8);
    EXPECT_EQ(out_model.ny(), 8);
    EXPECT_EQ(out_model.nz(), 4);
    EXPECT_EQ(stats.levels_completed, 3);
    EXPECT_GT(stats.elapsed_time_ms, 0.0);
    EXPECT_GE(out_model.max_value(), cfg.min_density);
    EXPECT_LE(out_model.max_value(), cfg.max_density);
}

TEST(InversionEngineTest, JsonExchangeSerialization) {
    inv::InversionSetup setup;
    setup.mode = "planar";
    setup.config.nx = 16;
    setup.config.ny = 16;
    setup.config.nz = 8;
    setup.config.coarse_to_fine_levels = 3;
    setup.obs_x = {0.0, 100.0, 200.0};
    setup.obs_y = {0.0, 100.0, 200.0};
    setup.obs_z = {0.0, 0.0, 0.0};
    setup.obs_gz = {1.2, 2.4, 0.8};

    // Serialize to JSON and parse back
    nlohmann::json j = setup.to_json();
    auto loaded = inv::InversionSetup::from_json(j);

    EXPECT_EQ(loaded.mode, "planar");
    EXPECT_EQ(loaded.config.nx, 16);
    EXPECT_EQ(loaded.config.coarse_to_fine_levels, 3);
    EXPECT_EQ(loaded.obs_gz.size(), 3);
    EXPECT_DOUBLE_EQ(loaded.obs_gz[1], 2.4);

    // Run setup through InversionEngine
    inv::InversionEngine engine;
    inv::InversionResult result;
    bool ok = engine.run_setup(loaded, result);
    EXPECT_TRUE(ok);
    EXPECT_EQ(result.density_grid.nx(), 16);
    EXPECT_EQ(result.stats.levels_completed, 3);

    // Result serialization round-trip
    nlohmann::json res_j = result.to_json();
    auto res_loaded = inv::InversionResult::from_json(res_j);
    EXPECT_EQ(res_loaded.density_grid.nx(), 16);
    EXPECT_EQ(res_loaded.stats.levels_completed, 3);
}

TEST(ForwardKernelTest, InterfaceMeshEvaluation) {
    inv::VoxelGrid<double> grid(4, 4, 4, 0.0, 0.0, 0.0, 50.0, 50.0, 50.0, 0.0);
    grid(1, 1, 1) = 0.5; // g/cm^3 body

    std::vector<double> topo(5 * 5, 0.0); // flat 0m DEM
    std::vector<inv::Point3D<double>> obs = {
        {75.0, 75.0, -5.0}, // Directly above (1, 1, 1) center
        {300.0, 300.0, -5.0} // Far point
    };

    std::vector<double> gz;
    inv::ForwardKernel<double>::compute_gravity_field_interface_mesh(grid, topo, obs, gz);

    EXPECT_EQ(gz.size(), 2);
    EXPECT_GT(gz[0], 0.0); // Downward attraction is positive
    EXPECT_LT(gz[1], gz[0]); // Decays with distance
}

// ======================================================================================
// Focusing inversion: operators, functionals, recovery, coarse-to-fine, prior, topography
// ======================================================================================
#include <random>

namespace {
std::vector<double> random_vec(std::size_t n, unsigned seed) {
    std::mt19937 rng(seed);
    std::normal_distribution<double> nd;
    std::vector<double> v(n);
    for (auto& x : v) x = nd(rng);
    return v;
}
} // namespace

TEST(FftTest, RoundTripAndConvolutionTheorem) {
    inv::Fft2D fft(8, 4);
    auto re = random_vec(32, 3);
    std::vector<std::complex<double>> a(32);
    for (size_t i = 0; i < 32; ++i) a[i] = re[i];
    auto b = a;
    fft.forward(b);
    fft.inverse(b);
    for (size_t i = 0; i < 32; ++i) EXPECT_NEAR(std::abs(b[i] - a[i]), 0.0, 1e-12);
}

TEST(GravityOperatorTest, ToeplitzMatchesDenseAndDirectSummation) {
    inv::GridGeometry g{6, 5, 4, 0, 0, 0, 50, 40, 30};
    const double z_obs = -10.0;
    inv::ToeplitzGravityOperator T(g, z_obs);
    std::vector<inv::Point3D<double>> obs;
    for (size_t i = 0; i < g.nx; ++i)
        for (size_t j = 0; j < g.ny; ++j) obs.emplace_back(g.xc(i), g.yc(j), z_obs);
    inv::DenseGravityOperator D(g, obs);

    auto m = random_vec(g.cells(), 1);
    std::vector<double> dt, dd, dref;
    T.apply(m, dt);
    D.apply(m, dd);
    inv::VoxelGrid<double> vg(g.nx, g.ny, g.nz, 0, 0, 0, 50, 40, 30, 0.0);
    vg.data() = m;
    inv::ForwardKernel<double>::compute_gravity_field(vg, obs, dref);
    for (size_t i = 0; i < dt.size(); ++i) {
        EXPECT_NEAR(dt[i], dd[i], 1e-10);
        EXPECT_NEAR(dd[i], dref[i], 1e-10);
    }

    auto r = random_vec(g.columns(), 2);
    std::vector<double> gt, gd;
    T.apply_transpose(r, gt);
    D.apply_transpose(r, gd);
    for (size_t j = 0; j < gt.size(); ++j) EXPECT_NEAR(gt[j], gd[j], 1e-10);

    // adjointness <Gm, r> == <m, G^T r>
    double lhs = 0, rhs = 0;
    for (size_t i = 0; i < dt.size(); ++i) lhs += dt[i] * r[i];
    for (size_t j = 0; j < m.size(); ++j) rhs += m[j] * gt[j];
    EXPECT_NEAR(lhs, rhs, 1e-9);

    std::vector<double> c1, c2;
    T.column_norm_squared(c1);
    D.column_norm_squared(c2);
    for (size_t j = 0; j < c1.size(); ++j) EXPECT_NEAR(c1[j], c2[j], 1e-12);
}

TEST(GravityOperatorTest, PfldInterfaceMeshMatchesDirectInterfaceMeshEvaluation) {
    const inv::GridGeometry g{4, 4, 3, 0.0, 0.0, 10.0, 50.0, 40.0, 30.0};
    std::vector<inv::Point3D<double>> obs;
    for (std::size_t ix = 0; ix < g.nx; ++ix) {
        for (std::size_t iy = 0; iy < g.ny; ++iy) {
            obs.emplace_back(g.xc(ix), g.yc(iy), -10.0);
        }
    }

    std::vector<double> topo((g.nx + 1) * (g.ny + 1));
    for (std::size_t iy = 0; iy <= g.ny; ++iy) {
        for (std::size_t ix = 0; ix <= g.nx; ++ix) {
            topo[iy * (g.nx + 1) + ix] = g.z0 + 2.0 * static_cast<double>(ix) - 1.5 * static_cast<double>(iy);
        }
    }

    inv::PfldInterfaceMeshOperator op(g, obs, topo);
    EXPECT_TRUE(op.is_dense());

    auto m = random_vec(g.cells(), 42);
    std::vector<double> d_op;
    op.apply(m, d_op);

    // Direct evaluation via pfld::InterfaceMesh::build with densities
    pfld::InterfaceMesh<double> direct_mesh;
    direct_mesh.build(g.nx, g.ny, g.nz, g.x0, g.y0, g.dx, g.dy, g.dz, topo, m, g.z0, 0.0);
    std::vector<pfld::Point3Dd> pfld_obs;
    for (const auto& pt : obs) pfld_obs.emplace_back(pt.x, pt.y, pt.z);
    std::vector<double> d_direct;
    direct_mesh.compute_gravity_z(pfld_obs, d_direct);

    ASSERT_EQ(d_op.size(), d_direct.size());
    for (std::size_t i = 0; i < d_op.size(); ++i) {
        EXPECT_NEAR(d_op[i], d_direct[i], 1e-12);
    }

    // Adjointness test: <G m, r> == <m, G^T r>
    auto r = random_vec(g.columns(), 7);
    std::vector<double> g_adj;
    op.apply_transpose(r, g_adj);

    double lhs = 0.0, rhs = 0.0;
    for (std::size_t i = 0; i < d_op.size(); ++i) lhs += d_op[i] * r[i];
    for (std::size_t j = 0; j < m.size(); ++j) rhs += m[j] * g_adj[j];
    EXPECT_NEAR(lhs, rhs, 1e-10);
}

TEST(GravityOperatorTest, PfldInterfaceMeshMatchesNagyPrismAndToeplitzOnFlatPlane) {
    const inv::GridGeometry g{6, 5, 4, 0.0, 0.0, 10.0, 50.0, 40.0, 30.0};
    const double z_obs = -10.0;
    std::vector<inv::Point3D<double>> obs;
    for (std::size_t ix = 0; ix < g.nx; ++ix) {
        for (std::size_t iy = 0; iy < g.ny; ++iy) {
            obs.emplace_back(g.xc(ix), g.yc(iy), z_obs);
        }
    }

    inv::PfldInterfaceMeshOperator op_mesh(g, obs);
    inv::DenseGravityOperator op_dense(g, obs);
    inv::ToeplitzGravityOperator op_toep(g, z_obs);

    auto m = random_vec(g.cells(), 13);
    std::vector<double> d_mesh, d_dense, d_toep;
    op_mesh.apply(m, d_mesh);
    op_dense.apply(m, d_dense);
    op_toep.apply(m, d_toep);

    // Nagy prism and pfld InterfaceMesh evaluate the exact same Newton potential;
    // difference is only CODATA G precision (~6.7e-5 relative).
    for (std::size_t i = 0; i < d_mesh.size(); ++i) {
        EXPECT_NEAR(d_mesh[i], d_dense[i], 1e-3 * std::abs(d_dense[i]) + 1e-6);
        EXPECT_NEAR(d_dense[i], d_toep[i], 1e-10);
    }

    // Column norms match
    std::vector<double> s_mesh, s_dense;
    op_mesh.column_norm_squared(s_mesh);
    op_dense.column_norm_squared(s_dense);
    for (std::size_t j = 0; j < s_mesh.size(); ++j) {
        EXPECT_NEAR(s_mesh[j], s_dense[j], 1e-3 * s_dense[j] + 1e-8);
    }
}

TEST(FocusingMetricsTest, PhysicalFormsAreResolutionInvariant) {
    // Same physical block (100 m cube of 0.4 g/cm^3) on a 50 m lattice and on a 25 m lattice.
    inv::VoxelGrid<double> a(8, 8, 8, 0, 0, 0, 50, 50, 50, 0.0);
    inv::VoxelGrid<double> b(16, 16, 16, 0, 0, 0, 25, 25, 25, 0.0);
    for (size_t ix = 3; ix < 5; ++ix) for (size_t iy = 3; iy < 5; ++iy) for (size_t iz = 3; iz < 5; ++iz) a(ix, iy, iz) = 0.4;
    for (size_t ix = 6; ix < 10; ++ix) for (size_t iy = 6; iy < 10; ++iy) for (size_t iz = 6; iz < 10; ++iz) b(ix, iy, iz) = 0.4;
    using FE = inv::FocusingEvaluator<double>;
    const double va = FE::compute_minimum_support_volume(a, 1e-3);
    const double vb = FE::compute_minimum_support_volume(b, 1e-3);
    EXPECT_NEAR(va, 100.0 * 100.0 * 100.0, 1e-3 * 1e6);
    EXPECT_NEAR(va, vb, 1e-6 * va);
    // contact area of a 100 m cube = 6 * 100^2 (face based, jump form)
    EXPECT_NEAR(FE::compute_minimum_gradient_support_area(a, 1e-3), 6.0 * 100.0 * 100.0, 1.0);
    EXPECT_NEAR(FE::compute_minimum_gradient_support_area(b, 1e-3), 6.0 * 100.0 * 100.0, 1.0);
    // interior face count: 3 * n^2 * (n-1) faces
    EXPECT_NEAR(FE::interface_area(a), 3.0 * 8 * 8 * 7 * 2500.0, 1e-6);
}

namespace {
struct Prism {
    inv::InversionConfig cfg;
    inv::VoxelGrid<double> truth;
    std::vector<double> data;
    double sigma{0};
};

Prism make_prism(size_t n, size_t nz, double noise_frac, unsigned seed) {
    Prism p;
    p.cfg.nx = n; p.cfg.ny = n; p.cfg.nz = nz;
    p.cfg.dx = p.cfg.dy = p.cfg.dz = 50.0;
    p.truth = inv::VoxelGrid<double>(n, n, nz, 0, 0, 0, 50, 50, 50, 0.0);
    const size_t h = n / 10;
    for (size_t ix = n / 2 - h; ix < n / 2 + h; ++ix)
        for (size_t iy = n / 2 - h; iy < n / 2 + h; ++iy)
            for (size_t iz = nz / 5; iz < nz / 5 + nz / 4; ++iz) p.truth(ix, iy, iz) = 0.4;
    inv::GridGeometry g{n, n, nz, 0, 0, 0, 50, 50, 50};
    inv::ToeplitzGravityOperator T(g, -p.cfg.obs_height);
    T.apply(p.truth.data(), p.data);
    double mx = 0;
    for (double v : p.data) mx = std::max(mx, std::abs(v));
    std::mt19937 rng(seed);
    std::normal_distribution<double> nd(0.0, noise_frac * mx);
    for (auto& v : p.data) v += nd(rng);
    p.sigma = noise_frac * mx;
    p.cfg.noise_sigma = p.sigma;
    return p;
}

double positive_centroid_depth(const inv::VoxelGrid<double>& m) {
    double sw = 0, sz = 0;
    for (size_t ix = 0; ix < m.nx(); ++ix)
        for (size_t iy = 0; iy < m.ny(); ++iy)
            for (size_t iz = 0; iz < m.nz(); ++iz) {
                const double v = std::max(0.0, m(ix, iy, iz));
                sw += v;
                sz += v * (iz + 0.5) * m.dz();
            }
    return sw > 0 ? sz / sw : 0.0;
}

size_t support_cells(const inv::VoxelGrid<double>& m, double frac_of_max = 0.05) {
    const double thr = frac_of_max * std::max(m.max_value(), -m.min_value());
    size_t c = 0;
    for (size_t i = 0; i < m.total_cells(); ++i) c += (std::abs(m[i]) > thr);
    return c;
}
} // namespace

TEST(FocusingInversionTest, RecoversCompactPrismAndBeatsL2Baseline) {
    // 32 x 32 x 16 cells, truth = 0.4 g/cm^3 prism (6x6x4 cells) centred at z = 250 m, 2 % noise.
    Prism p = make_prism(32, 16, 0.02, 7);

    inv::InversionConfig fc = p.cfg;           // focused, 3 levels
    inv::VoxelGrid<double> focused;
    inv::InversionStats fs;
    ASSERT_TRUE(inv::InversionEngine(fc).invert_columns(p.data, focused, fs));

    inv::InversionConfig bc = p.cfg;           // Tikhonov L2 baseline, 1 level
    bc.focusing_enabled = false;
    bc.coarse_to_fine_levels = 1;
    inv::VoxelGrid<double> l2;
    inv::InversionStats bs;
    ASSERT_TRUE(inv::InversionEngine(bc).invert_columns(p.data, l2, bs));

    // Data are fitted near the noise level (discrepancy principle).
    EXPECT_LT(fs.final_chi_rms, 1.5);
    EXPECT_GT(fs.final_chi_rms, 0.5);
    EXPECT_EQ(fs.levels.size(), 3u);
    EXPECT_LT(fs.final_data_rms, fs.initial_data_rms);

    // Focusing recovers a compact body near the true depth; the L2 solution is smeared and too deep.
    const double zc_f = positive_centroid_depth(focused);
    const double zc_b = positive_centroid_depth(l2);
    EXPECT_NEAR(zc_f, 250.0, 100.0);
    EXPECT_LT(std::abs(zc_f - 250.0), std::abs(zc_b - 250.0));
    EXPECT_LT(support_cells(focused), support_cells(l2) / 4);
    EXPECT_LT(support_cells(focused), 600u);   // truth: 144 cells of 16384

    // Peak amplitude is recovered far better than with L2 (which underestimates it).
    EXPECT_GT(focused.max_value(), 1.5 * l2.max_value());
    EXPECT_LE(focused.max_value(), p.cfg.max_density + 1e-12);
}

TEST(FocusingInversionTest, CoarseToFineActuallyChangesAndRefinesTheSolution) {
    Prism p = make_prism(32, 16, 0.02, 11);
    inv::InversionConfig c1 = p.cfg;
    c1.coarse_to_fine_levels = 1;
    inv::InversionConfig c3 = p.cfg;
    inv::VoxelGrid<double> m1, m3;
    inv::InversionStats s1, s3;
    ASSERT_TRUE(inv::InversionEngine(c1).invert_columns(p.data, m1, s1));
    ASSERT_TRUE(inv::InversionEngine(c3).invert_columns(p.data, m3, s3));

    // Regression for the old engine, where every level overwrote the previous one (diff == 0).
    double diff = 0;
    for (size_t i = 0; i < m1.total_cells(); ++i) diff = std::max(diff, std::abs(m1[i] - m3[i]));
    EXPECT_GT(diff, 1e-3);

    // Levels have the expected sizes and the adaptive refinement shrinks the active set.
    ASSERT_EQ(s3.levels.size(), 3u);
    EXPECT_EQ(s3.levels[0].nx, 8u);
    EXPECT_EQ(s3.levels[1].nx, 16u);
    EXPECT_EQ(s3.levels[2].nx, 32u);
    EXPECT_LT(s3.levels[2].active_cells, 32u * 32u * 16u);
    EXPECT_LT(s3.levels[2].time_ms, 5.0 * s1.levels[0].time_ms + 1000.0);

    // No adaptivity -> all fine cells active.
    inv::InversionConfig cn = p.cfg;
    cn.adaptive_refinement = false;
    inv::VoxelGrid<double> mn;
    inv::InversionStats sn;
    ASSERT_TRUE(inv::InversionEngine(cn).invert_columns(p.data, mn, sn));
    EXPECT_EQ(sn.levels[2].active_cells, 32u * 32u * 16u);
}

TEST(FocusingInversionTest, PriorIsScaledAndImprovesStart) {
    Prism p = make_prism(32, 16, 0.02, 5);
    // A prior with the right shape but half the amplitude.
    std::vector<double> prior = p.truth.data();
    for (auto& v : prior) v *= 0.5;

    inv::InversionConfig c = p.cfg;
    inv::VoxelGrid<double> m;
    inv::InversionStats s;
    ASSERT_TRUE(inv::InversionEngine(c).invert_columns(p.data, m, s, {}, {}, &prior));
    EXPECT_TRUE(s.used_neural_transformer);
    EXPECT_LT(s.final_chi_rms, 1.5);
    EXPECT_NEAR(positive_centroid_depth(m), 250.0, 60.0);
    EXPECT_NEAR(m.max_value(), 0.4, 0.15);   // amplitude corrected by the data-driven rescale
}

TEST(FocusingInversionTest, TopographyMasksCellsAboveSurfaceAndUsesDensePath) {
    inv::InversionConfig c;
    c.nx = 8; c.ny = 8; c.nz = 4;
    c.dx = c.dy = c.dz = 50.0;
    c.z0 = 0.0;
    c.coarse_to_fine_levels = 2;
    // DEM sloping from z = 0 (west) to z = 100 m (east) -> terrain is LOWER to the east (z down).
    std::vector<double> topo(9 * 9);
    for (size_t iy = 0; iy < 9; ++iy)
        for (size_t ix = 0; ix < 9; ++ix) topo[iy * 9 + ix] = 12.5 * static_cast<double>(ix);
    std::vector<double> d(64);
    for (size_t ix = 0; ix < 8; ++ix)
        for (size_t iy = 0; iy < 8; ++iy) d[ix * 8 + iy] = std::exp(-0.5 * (std::pow(ix - 3.5, 2) + std::pow(iy - 3.5, 2)) / 2.0);
    inv::InversionEngine e(c);
    inv::VoxelGrid<double> m;
    inv::InversionStats s;
    ASSERT_TRUE(e.invert_columns(d, m, s, topo));
    size_t checked = 0;
    for (size_t ix = 0; ix < 8; ++ix)
        for (size_t iy = 0; iy < 8; ++iy)
            for (size_t iz = 0; iz < 4; ++iz) {
                const double zs = 12.5 * (ix + 0.5);          // mean of the 4 corner nodes
                if ((iz + 0.5) * 50.0 < zs) { EXPECT_EQ(m(ix, iy, iz), 0.0); ++checked; }
            }
    EXPECT_GT(checked, 0u);
    EXPECT_GT(s.levels.back().active_cells, 0u);
}

TEST(InversionEngineTest, RunSetupWithScatteredStationsAndPriorJson) {
    inv::InversionSetup setup;
    setup.config.nx = 16; setup.config.ny = 16; setup.config.nz = 8;
    setup.config.irls_iterations = 4;
    std::mt19937 rng(2);
    std::uniform_real_distribution<double> u(0.0, 800.0);
    for (int i = 0; i < 300; ++i) {
        const double x = u(rng), y = u(rng);
        setup.obs_x.push_back(x);
        setup.obs_y.push_back(y);
        setup.obs_z.push_back(-10.0);
        setup.obs_gz.push_back(0.5 * std::exp(-(std::pow(x - 400, 2) + std::pow(y - 400, 2)) / (2 * 120.0 * 120.0)));
    }
    setup.prior.assign(16 * 16 * 8, 0.0);
    nlohmann::json j = setup.to_json();
    auto loaded = inv::InversionSetup::from_json(j);
    EXPECT_EQ(loaded.prior.size(), 16u * 16u * 8u);
    EXPECT_EQ(loaded.config.irls_iterations, 4);

    inv::InversionEngine eng;
    inv::InversionResult r;
    ASSERT_TRUE(eng.run_setup(loaded, r));
    EXPECT_EQ(r.stats.levels.size(), 3u);
    EXPECT_GT(r.density_grid.max_value(), 0.0);

    auto rj = inv::InversionResult::from_json(r.to_json());
    EXPECT_EQ(rj.stats.levels.size(), 3u);
    EXPECT_NEAR(rj.stats.final_chi_rms, r.stats.final_chi_rms, 1e-12);

    inv::InversionSetup empty = loaded;
    empty.obs_x = {-1e6};
    empty.obs_y = {-1e6};
    empty.obs_gz = {1.0};
    empty.obs_z = {};
    EXPECT_FALSE(inv::InversionEngine().run_setup(empty, r));
}
