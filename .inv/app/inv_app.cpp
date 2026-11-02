#include "inv/inv.hpp"
#include <iostream>
#include <vector>
#include <iomanip>

int main(int argc, char* argv[]) {
    std::cout << "========================================================\n";
    std::cout << "  .inv: 3D Potential Field Neural Inversion Sub-Project \n";
    std::cout << "  Mod3D Independent Core Physics & Inversion Engine    \n";
    std::cout << "========================================================\n\n";

    // 1. Define subsurface 3D voxel modeling space (e.g. 1 km x 1 km x 500 m)
    const std::size_t nx = 20;
    const std::size_t ny = 20;
    const std::size_t nz = 10;
    const double dx = 50.0;
    const double dy = 50.0;
    const double dz = 50.0;

    inv::VoxelGrid<double> ground_truth(nx, ny, nz, 0.0, 0.0, 0.0, dx, dy, dz, 0.0);
    std::cout << "[1] Initialized 3D Voxel Domain:\n";
    std::cout << "    Dimensions: " << nx << " x " << ny << " x " << nz << " = "
              << ground_truth.total_cells() << " cells\n";
    std::cout << "    Cell spacing: dx=" << dx << " m, dy=" << dy << " m, dz=" << dz << " m\n";
    std::cout << "    Bounding box: X=[0, " << ground_truth.x_max() << "] m, "
              << "Y=[0, " << ground_truth.y_max() << "] m, "
              << "Z=[0, " << ground_truth.z_max() << "] m\n\n";

    // 2. Embed a compact anomalous body (density contrast = +0.4 g/cm^3)
    const std::size_t body_ix_start = 8, body_ix_end = 12;
    const std::size_t body_iy_start = 8, body_iy_end = 12;
    const std::size_t body_iz_start = 2, body_iz_end = 6;
    for (std::size_t ix = body_ix_start; ix <= body_ix_end; ++ix) {
        for (std::size_t iy = body_iy_start; iy <= body_iy_end; ++iy) {
            for (std::size_t iz = body_iz_start; iz <= body_iz_end; ++iz) {
                ground_truth(ix, iy, iz) = 0.4; // g/cm^3
            }
        }
    }
    std::cout << "[2] Embedded compact high-density target (+0.40 g/cm^3) at depth 100m - 300m.\n";
    std::cout << "    Ground Truth L2 Norm: " << ground_truth.l2_norm() << "\n\n";

    // 3. Compute Zhdanov Focusing Metrics on ground truth
    const double ms_true = inv::FocusingEvaluator<double>::compute_minimum_support(ground_truth, 1e-3);
    const double mgs_true = inv::FocusingEvaluator<double>::compute_minimum_gradient_support(ground_truth, 1e-3);
    std::cout << "[3] Zhdanov Focusing Metrics (Ground Truth):\n";
    std::cout << "    Minimum Support (S_MS, anomalous volume measure): " << std::fixed << std::setprecision(2) << ms_true << " cells\n";
    std::cout << "    Minimum Gradient Support (S_MGS, boundary area measure): " << mgs_true << "\n\n";

    // 4. Forward model surface gravity anomaly at z = -1.0 m (airborne / surface survey)
    const std::size_t obs_nx = 21;
    const std::size_t obs_ny = 21;
    std::vector<inv::Point3D<double>> obs_points;
    obs_points.reserve(obs_nx * obs_ny);
    for (std::size_t iy = 0; iy < obs_ny; ++iy) {
        for (std::size_t ix = 0; ix < obs_nx; ++ix) {
            obs_points.push_back({ix * 50.0, iy * 50.0, -1.0});
        }
    }

    std::vector<double> gz_obs;
    inv::ForwardKernel<double>::compute_gravity_field(ground_truth, obs_points, gz_obs);

    double max_gz = 0.0;
    for (double g : gz_obs) {
        if (g > max_gz) max_gz = g;
    }
    std::cout << "[4] Analytical Forward Modeling (Nagy Rectangular Prism Kernel):\n";
    std::cout << "    Observation stations: " << obs_points.size() << " surface points\n";
    std::cout << "    Peak gravity anomaly gz: " << std::setprecision(4) << max_gz << " mGal\n\n";

    // 5. Run Inversion Engine
    inv::InversionConfig cfg;
    cfg.nx = nx; cfg.ny = ny; cfg.nz = nz;
    cfg.dx = dx; cfg.dy = dy; cfg.dz = dz;
    cfg.focusing_e = 1e-3;
    cfg.focusing_e_grad = 1e-3;

    inv::InversionEngine engine(cfg);
    inv::VoxelGrid<double> inverted_model;
    inv::InversionStats stats;

    engine.invert_gravity_field(gz_obs, obs_nx, obs_ny, inverted_model, stats);

    std::cout << "[5] Inversion Execution:\n";
    std::cout << "    Status: " << stats.message << "\n";
    std::cout << "    Elapsed computation time: " << std::setprecision(2) << stats.elapsed_time_ms << " ms\n";
    std::cout << "    Inverted model S_MS: " << stats.minimum_support << "\n";
    std::cout << "    Inverted model S_MGS: " << stats.minimum_gradient_support << "\n";
    std::cout << "    Inverted Peak Density: " << inverted_model.max_value() << " g/cm^3\n\n";

    std::cout << "========================================================\n";
    std::cout << "  Execution completed successfully!                      \n";
    std::cout << "========================================================\n";

    return 0;
}
