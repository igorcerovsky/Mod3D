#include "mod3d_gui/dialogs/Inversion3DDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QFontDatabase>
#include <QDateTime>
#include <QCloseEvent>
#include <cmath>

namespace mod3d_gui {

Inversion3DDialog::Inversion3DDialog(ModelDocument *doc, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
{
    setWindowTitle(tr("3D Potential Field Focusing Inversion"));
    setMinimumSize(680, 520);
    resize(740, 580);

    setupUi();
    updateDomainFromDocument();
}

Inversion3DDialog::~Inversion3DDialog() {
    if (m_workerThread.joinable()) {
        m_workerThread.request_stop();
    }
}

void Inversion3DDialog::closeEvent(QCloseEvent *event) {
    if (m_workerThread.joinable()) {
        m_workerThread.request_stop();
    }
    event->accept();
}

void Inversion3DDialog::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);

    m_tabWidget = new QTabWidget(this);

    auto *domainTab = new QWidget(this);
    setupDomainTab(domainTab);
    m_tabWidget->addTab(domainTab, tr("1. Domain && Grid"));

    auto *paramsTab = new QWidget(this);
    setupParametersTab(paramsTab);
    m_tabWidget->addTab(paramsTab, tr("2. Focusing && Regularization"));

    auto *execTab = new QWidget(this);
    setupExecutionTab(execTab);
    m_tabWidget->addTab(execTab, tr("3. Convergence && Execution"));

    mainLayout->addWidget(m_tabWidget);

    // Bottom action buttons
    auto *btnLayout = new QHBoxLayout();
    m_btnRun = new QPushButton(tr("Run Inversion"), this);
    m_btnRun->setDefault(true);
    m_btnRun->setStyleSheet("font-weight: bold; padding: 6px 14px;");

    m_btnCancel = new QPushButton(tr("Cancel"), this);
    m_btnCancel->setEnabled(false);

    m_btnApply = new QPushButton(tr("Apply to Document"), this);
    m_btnApply->setEnabled(false);

    m_btnExportVtk = new QPushButton(tr("Export VTK..."), this);
    m_btnExportVtk->setEnabled(false);

    m_btnExportJson = new QPushButton(tr("Export JSON..."), this);
    m_btnExportJson->setEnabled(false);

    m_btnClose = new QPushButton(tr("Close"), this);

    btnLayout->addWidget(m_btnRun);
    btnLayout->addWidget(m_btnCancel);
    btnLayout->addSpacing(12);
    btnLayout->addWidget(m_btnApply);
    btnLayout->addWidget(m_btnExportVtk);
    btnLayout->addWidget(m_btnExportJson);
    btnLayout->addStretch();
    btnLayout->addWidget(m_btnClose);

    mainLayout->addLayout(btnLayout);

    connect(m_btnRun, &QPushButton::clicked, this, &Inversion3DDialog::onRunClicked);
    connect(m_btnCancel, &QPushButton::clicked, this, &Inversion3DDialog::onCancelClicked);
    connect(m_btnApply, &QPushButton::clicked, this, &Inversion3DDialog::onApplyClicked);
    connect(m_btnExportVtk, &QPushButton::clicked, this, &Inversion3DDialog::onExportVtkClicked);
    connect(m_btnExportJson, &QPushButton::clicked, this, &Inversion3DDialog::onExportJsonClicked);
    connect(m_btnClose, &QPushButton::clicked, this, &QDialog::accept);
}

void Inversion3DDialog::setupDomainTab(QWidget *parent) {
    auto *layout = new QVBoxLayout(parent);

    auto *grpHorizontal = new QGroupBox(tr("Horizontal Model & Observation Domain"), parent);
    auto *formH = new QFormLayout(grpHorizontal);

    m_lblGridInfo = new QLabel(parent);
    formH->addRow(tr("Active Grid:"), m_lblGridInfo);

    m_lblCenteringNotice = new QLabel(parent);
    m_lblCenteringNotice->setWordWrap(true);
    m_lblCenteringNotice->setStyleSheet(
        "background-color: #f0f4f8; color: #1a365d; border: 1px solid #cbd5e1; border-radius: 4px; padding: 8px;");
    m_lblCenteringNotice->setText(
        tr("<b>Profile-Plane Centering (Invariant):</b><br>"
           "Voxel centers lie directly on Mod3D profile planes.<br>"
           "For each grid point (x<sub>i</sub>, y<sub>i</sub>), cell horizontal bounds are "
           "[x<sub>i</sub> &plusmn; &Delta;x/2, y<sub>i</sub> &plusmn; &Delta;y/2] with center at (x<sub>i</sub>, y<sub>i</sub>)."));
    formH->addRow(m_lblCenteringNotice);

    layout->addWidget(grpHorizontal);

    auto *grpVertical = new QGroupBox(tr("Vertical Subsurface Discretization"), parent);
    auto *formV = new QFormLayout(grpVertical);

    m_lblZRange = new QLabel(parent);
    formV->addRow(tr("Model Elevation Range:"), m_lblZRange);

    m_spinNz = new QSpinBox(parent);
    m_spinNz->setRange(2, 256);
    m_spinNz->setValue(24);
    formV->addRow(tr("Vertical Layers (Nz):"), m_spinNz);

    m_spinDz = new QDoubleSpinBox(parent);
    m_spinDz->setRange(1.0, 50000.0);
    m_spinDz->setDecimals(1);
    m_spinDz->setSuffix(" m");
    m_spinDz->setValue(100.0);
    formV->addRow(tr("Cell Thickness (&Delta;z):"), m_spinDz);

    m_spinObsHeight = new QDoubleSpinBox(parent);
    m_spinObsHeight->setRange(0.0, 10000.0);
    m_spinObsHeight->setDecimals(1);
    m_spinObsHeight->setSuffix(" m");
    m_spinObsHeight->setValue(10.0);
    formV->addRow(tr("Sensor Clearance above Ground:"), m_spinObsHeight);

    layout->addWidget(grpVertical);
    layout->addStretch();

    // Link Nz and dz updates
    connect(m_spinNz, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int nz) {
        if (!m_doc) return;
        const double zMin = m_doc->model().z_min().value();
        const double zMax = m_doc->model().z_max().value();
        const double H = std::max(10.0, zMax - zMin);
        if (nz > 0) {
            m_spinDz->blockSignals(true);
            m_spinDz->setValue(H / nz);
            m_spinDz->blockSignals(false);
        }
    });

    connect(m_spinDz, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double dz) {
        if (!m_doc || dz <= 0.0) return;
        const double zMin = m_doc->model().z_min().value();
        const double zMax = m_doc->model().z_max().value();
        const double H = std::max(10.0, zMax - zMin);
        const int nz = std::max(2, static_cast<int>(std::round(H / dz)));
        m_spinNz->blockSignals(true);
        m_spinNz->setValue(nz);
        m_spinNz->blockSignals(false);
    });
}

void Inversion3DDialog::setupParametersTab(QWidget *parent) {
    auto *layout = new QVBoxLayout(parent);

    auto *grpFocus = new QGroupBox(tr("Zhdanov Regularization & Multiscale"), parent);
    auto *formFocus = new QFormLayout(grpFocus);

    m_chkFocusing = new QCheckBox(tr("Enable Portniaguine-Zhdanov Focusing Stabilizers"), parent);
    m_chkFocusing->setChecked(true);
    formFocus->addRow(m_chkFocusing);

    m_comboLevels = new QComboBox(parent);
    m_comboLevels->addItem(tr("1 Level (Direct finest resolution)"), 1);
    m_comboLevels->addItem(tr("2 Levels (Coarse factor 2 -> 1)"), 2);
    m_comboLevels->addItem(tr("3 Levels (Hierarchical 4 -> 2 -> 1, Recommended)"), 3);
    m_comboLevels->setCurrentIndex(2);
    formFocus->addRow(tr("Coarse-to-Fine Hierarchy:"), m_comboLevels);

    m_chkAdaptiveRefine = new QCheckBox(tr("Adaptive Support Refinement (+1 halo on fine levels)"), parent);
    m_chkAdaptiveRefine->setChecked(true);
    formFocus->addRow(m_chkAdaptiveRefine);

    m_spinMsWeight = new QDoubleSpinBox(parent);
    m_spinMsWeight->setRange(0.0, 100.0);
    m_spinMsWeight->setDecimals(2);
    m_spinMsWeight->setSingleStep(0.1);
    m_spinMsWeight->setValue(1.0);
    formFocus->addRow(tr("Minimum Support (S_MS) Weight:"), m_spinMsWeight);

    m_spinMgsWeight = new QDoubleSpinBox(parent);
    m_spinMgsWeight->setRange(0.0, 100.0);
    m_spinMgsWeight->setDecimals(2);
    m_spinMgsWeight->setSingleStep(0.1);
    m_spinMgsWeight->setValue(1.0);
    formFocus->addRow(tr("Minimum Gradient Support (S_MGS) Weight:"), m_spinMgsWeight);

    m_spinDepthExp = new QDoubleSpinBox(parent);
    m_spinDepthExp->setRange(0.0, 2.0);
    m_spinDepthExp->setDecimals(2);
    m_spinDepthExp->setSingleStep(0.05);
    m_spinDepthExp->setValue(0.75);
    formFocus->addRow(tr("Depth Weighting Exponent (&gamma;):"), m_spinDepthExp);

    layout->addWidget(grpFocus);

    auto *grpPhysics = new QGroupBox(tr("Physics & Solver Configuration"), parent);
    auto *formPhys = new QFormLayout(grpPhysics);

    auto *densityLayout = new QHBoxLayout();
    m_spinMinDensity = new QDoubleSpinBox(parent);
    m_spinMinDensity->setRange(-10.0, 10.0);
    m_spinMinDensity->setDecimals(3);
    m_spinMinDensity->setSingleStep(0.05);
    m_spinMinDensity->setSuffix(" g/cm³");
    m_spinMinDensity->setValue(-0.5);

    m_spinMaxDensity = new QDoubleSpinBox(parent);
    m_spinMaxDensity->setRange(-10.0, 10.0);
    m_spinMaxDensity->setDecimals(3);
    m_spinMaxDensity->setSingleStep(0.05);
    m_spinMaxDensity->setSuffix(" g/cm³");
    m_spinMaxDensity->setValue(0.6);

    densityLayout->addWidget(new QLabel(tr("Min:"), parent));
    densityLayout->addWidget(m_spinMinDensity);
    densityLayout->addWidget(new QLabel(tr("Max:"), parent));
    densityLayout->addWidget(m_spinMaxDensity);
    formPhys->addRow(tr("Density Contrast Bounds:"), densityLayout);

    m_comboOperator = new QComboBox(parent);
    m_comboOperator->addItem(tr("pfld::InterfaceMesh (DEM terrain-conforming facets, Primary)"));
    m_comboOperator->addItem(tr("Toeplitz FFT (Matrix-free flat benchmark)"));
    m_comboOperator->setCurrentIndex(0);
    formPhys->addRow(tr("Forward Operator:"), m_comboOperator);

    m_spinNoiseRel = new QDoubleSpinBox(parent);
    m_spinNoiseRel->setRange(0.01, 50.0);
    m_spinNoiseRel->setDecimals(2);
    m_spinNoiseRel->setSuffix(" %");
    m_spinNoiseRel->setValue(2.0);
    formPhys->addRow(tr("Relative Data Noise (&sigma;):"), m_spinNoiseRel);

    auto *iterLayout = new QHBoxLayout();
    m_spinIrlsIters = new QSpinBox(parent);
    m_spinIrlsIters->setRange(1, 50);
    m_spinIrlsIters->setValue(10);

    m_spinCgMaxIters = new QSpinBox(parent);
    m_spinCgMaxIters->setRange(5, 500);
    m_spinCgMaxIters->setValue(80);

    iterLayout->addWidget(new QLabel(tr("IRLS:"), parent));
    iterLayout->addWidget(m_spinIrlsIters);
    iterLayout->addWidget(new QLabel(tr("PCG:"), parent));
    iterLayout->addWidget(m_spinCgMaxIters);
    formPhys->addRow(tr("Max Iterations:"), iterLayout);

    auto *priorLayout = new QHBoxLayout();
    m_txtPriorPath = new QLineEdit(parent);
    m_txtPriorPath->setPlaceholderText(tr("Optional: path to prior ONNX / JSON model"));
    m_btnBrowsePrior = new QPushButton(tr("Browse..."), parent);
    priorLayout->addWidget(m_txtPriorPath);
    priorLayout->addWidget(m_btnBrowsePrior);
    formPhys->addRow(tr("Prior Density Model:"), priorLayout);

    connect(m_btnBrowsePrior, &QPushButton::clicked, this, &Inversion3DDialog::onBrowsePriorClicked);

    layout->addWidget(grpPhysics);
    layout->addStretch();
}

void Inversion3DDialog::setupExecutionTab(QWidget *parent) {
    auto *layout = new QVBoxLayout(parent);

    auto *grpProgress = new QGroupBox(tr("Execution Status & Convergence Monitor"), parent);
    auto *vProgress = new QVBoxLayout(grpProgress);

    m_lblStatus = new QLabel(tr("Ready to run inversion."), parent);
    m_lblStatus->setStyleSheet("font-weight: bold; color: #2d3748;");
    vProgress->addWidget(m_lblStatus);

    m_progressBar = new QProgressBar(parent);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    vProgress->addWidget(m_progressBar);

    m_lblCurrentStats = new QLabel(parent);
    m_lblCurrentStats->setStyleSheet("color: #4a5568; font-size: 11px;");
    m_lblCurrentStats->setText(tr("Level: - | Active Cells: - | Data RMS: - mGal | Chi RMS: - | MS: - | MGS: -"));
    vProgress->addWidget(m_lblCurrentStats);

    layout->addWidget(grpProgress);

    auto *grpLog = new QGroupBox(tr("Convergence Trace"), parent);
    auto *vLog = new QVBoxLayout(grpLog);

    m_txtLog = new QTextEdit(parent);
    m_txtLog->setReadOnly(true);
    QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    monoFont.setPointSize(10);
    m_txtLog->setFont(monoFont);
    vLog->addWidget(m_txtLog);

    layout->addWidget(grpLog);
}

void Inversion3DDialog::updateDomainFromDocument() {
    if (!m_doc) return;

    const mod3d::Grid *obsGrid = m_doc->activeObservedGrid();
    if (!obsGrid || obsGrid->empty()) {
        m_lblGridInfo->setText(tr("<font color='red'>No observed gravity grid loaded!</font>"));
        m_btnRun->setEnabled(false);
        return;
    }

    const size_t nx = obsGrid->cols();
    const size_t ny = obsGrid->rows();
    const double dx = std::abs(obsGrid->dx().value());
    const double dy = std::abs(obsGrid->dy().value());
    const double x0 = obsGrid->x0().value();
    const double y0 = obsGrid->y0().value();

    m_lblGridInfo->setText(tr("%1 &times; %2 nodes (dx = %3 m, dy = %4 m, X: [%5, %6], Y: [%7, %8])")
        .arg(nx).arg(ny)
        .arg(dx, 0, 'f', 1).arg(dy, 0, 'f', 1)
        .arg(x0, 0, 'f', 0).arg(x0 + (nx - 1) * dx, 0, 'f', 0)
        .arg(y0, 0, 'f', 0).arg(y0 + (ny - 1) * dy, 0, 'f', 0));

    const double zMin = m_doc->model().z_min().value();
    const double zMax = m_doc->model().z_max().value();
    const double H = std::max(10.0, zMax - zMin);

    m_lblZRange->setText(tr("Elevation [%1 m, %2 m] (Total thickness: %3 m)")
        .arg(zMin, 0, 'f', 0).arg(zMax, 0, 'f', 0).arg(H, 0, 'f', 0));

    // Choose reasonable initial Nz matching horizontal resolution
    const double dxy = std::min(dx, dy);
    int initNz = std::clamp(static_cast<int>(std::round(H / dxy)), 16, 48);
    m_spinNz->blockSignals(true);
    m_spinNz->setValue(initNz);
    m_spinNz->blockSignals(false);

    m_spinDz->blockSignals(true);
    m_spinDz->setValue(H / initNz);
    m_spinDz->blockSignals(false);

    m_spinObsHeight->setValue(m_doc->observation().gravity_height().value());
    m_btnRun->setEnabled(true);
}

void Inversion3DDialog::setRunningState(bool running) {
    m_btnRun->setEnabled(!running);
    m_btnCancel->setEnabled(running);
    m_btnClose->setEnabled(!running);
    m_tabWidget->tabBar()->setEnabled(!running);
}

void Inversion3DDialog::appendLog(const QString &text) {
    const QString timeStr = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    m_txtLog->append(QString("[%1] %2").arg(timeStr, text));
}

void Inversion3DDialog::onRunClicked() {
    if (!m_doc) return;
    const mod3d::Grid *obsGrid = m_doc->activeObservedGrid();
    if (!obsGrid || obsGrid->empty()) {
        QMessageBox::warning(this, tr("No Data"), tr("No active observed gravity grid found in document."));
        return;
    }

    const size_t nx = obsGrid->cols();
    const size_t ny = obsGrid->rows();
    const size_t nz = static_cast<size_t>(m_spinNz->value());
    const double dx = std::abs(obsGrid->dx().value());
    const double dy = std::abs(obsGrid->dy().value());
    const double dz = m_spinDz->value();

    // Voxel horizontal centering: voxel center is AT (x_i, y_i)
    // so x0_geom = obsGrid->x0 - 0.5 * dx, y0_geom = obsGrid->y0 - 0.5 * dy.
    const double x0_geom = obsGrid->x0().value() - 0.5 * dx;
    const double y0_geom = obsGrid->y0().value() - 0.5 * dy;
    const double z0_geom = 0.0;

    inv::InversionSetup setup;
    setup.config.nx = nx;
    setup.config.ny = ny;
    setup.config.nz = nz;
    setup.config.dx = dx;
    setup.config.dy = dy;
    setup.config.dz = dz;
    setup.config.x0 = x0_geom;
    setup.config.y0 = y0_geom;
    setup.config.z0 = z0_geom;

    setup.config.coarse_to_fine_levels = m_comboLevels->currentData().toInt();
    setup.config.focusing_enabled = m_chkFocusing->isChecked();
    setup.config.adaptive_refinement = m_chkAdaptiveRefine->isChecked();
    setup.config.ms_weight = m_spinMsWeight->value();
    setup.config.mgs_weight = m_spinMgsWeight->value();
    setup.config.depth_weight_exponent = m_spinDepthExp->value();
    setup.config.min_density = m_spinMinDensity->value();
    setup.config.max_density = m_spinMaxDensity->value();
    setup.config.use_fft_solver_benchmark = (m_comboOperator->currentIndex() == 1);
    setup.config.noise_rel = m_spinNoiseRel->value() * 0.01;
    setup.config.irls_iterations = m_spinIrlsIters->value();
    setup.config.cg_max_iterations = m_spinCgMaxIters->value();
    setup.config.obs_height = m_spinObsHeight->value();
    setup.config.onnx_model_path = m_txtPriorPath->text().toStdString();

    // Collect observation raster: index ix * ny + iy
    std::vector<double> d_fine(nx * ny, 0.0);
    for (size_t ix = 0; ix < nx; ++ix) {
        for (size_t iy = 0; iy < ny; ++iy) {
            const double v = (*obsGrid)(iy, ix);
            d_fine[ix * ny + iy] = (v == mod3d::GRID_DUMMY || std::isnan(v)) ? 0.0 : v;
        }
    }

    // Topography surface nodes at cell corners: size (nx + 1) * (ny + 1)
    // In Mod3D, zMax is domain top elevation. Depth z_depth = zDomainTop - elevation >= 0.
    const double zDomainTop = m_doc->model().z_max().value();
    const auto &relief = m_doc->observation().surface_relief();
    const bool hasRelief = !relief.empty() && relief.rows() >= 2 && relief.cols() >= 2;
    std::vector<double> topo_nodes((nx + 1) * (ny + 1), 0.0);
    for (size_t iy = 0; iy <= ny; ++iy) {
        const double y_c = y0_geom + static_cast<double>(iy) * dy;
        for (size_t ix = 0; ix <= nx; ++ix) {
            const double x_c = x0_geom + static_cast<double>(ix) * dx;
            double elev = 0.0;
            if (hasRelief) {
                elev = relief.sample(x_c, y_c);
                if (elev == mod3d::GRID_DUMMY || std::isnan(elev)) elev = relief.mean();
            }
            topo_nodes[iy * (nx + 1) + ix] = std::max(0.0, zDomainTop - elev);
        }
    }
    setup.topo_elevations = std::move(topo_nodes);

    // Switch to execution tab & clear previous stats
    m_tabWidget->setCurrentIndex(2);
    m_txtLog->clear();
    m_progressBar->setValue(0);
    m_currentResult.reset();
    m_btnApply->setEnabled(false);
    m_btnExportVtk->setEnabled(false);
    m_btnExportJson->setEnabled(false);
    setRunningState(true);

    appendLog(tr("Starting 3D Focusing Inversion (%1x%2x%3 voxels)...").arg(nx).arg(ny).arg(nz));
    appendLog(tr("Voxel Centers aligned with profile planes: x0 = %1 m, y0 = %2 m, dx = %3 m, dy = %4 m, dz = %5 m")
        .arg(x0_geom, 0, 'f', 1).arg(y0_geom, 0, 'f', 1).arg(dx, 0, 'f', 1).arg(dy, 0, 'f', 1).arg(dz, 0, 'f', 1));
    appendLog(tr("Forward operator: %1").arg(setup.config.use_fft_solver_benchmark ? "Toeplitz FFT benchmark" : "pfld::InterfaceMesh DEM facets"));

    m_lastSetup = setup;

    // Launch background worker using C++20 std::jthread
    m_workerThread = std::jthread([this, setup = std::move(setup), d_fine = std::move(d_fine)](std::stop_token st) mutable {
        inv::InversionEngine engine(setup.config);
        inv::InversionConfig &cfg = const_cast<inv::InversionConfig&>(engine.config());

        cfg.cancel_check = [&st]() -> bool {
            return st.stop_requested();
        };

        cfg.progress_callback = [this, &st](int level, int total_levels, int iter, int total_iters, double rms, double chi) -> bool {
            if (st.stop_requested()) return false;
            QMetaObject::invokeMethod(this, [this, level, total_levels, iter, total_iters, rms, chi]() {
                const int pct = static_cast<int>(100.0 * ((level - 1) * total_iters + iter) / (total_levels * total_iters));
                m_progressBar->setValue(std::clamp(pct, 0, 99));
                m_lblStatus->setText(tr("Level %1/%2: IRLS Iteration %3/%4 (RMS: %5 mGal, &chi;: %6)")
                    .arg(level).arg(total_levels).arg(iter).arg(total_iters)
                    .arg(rms, 0, 'f', 4).arg(chi, 0, 'f', 3));
                m_lblCurrentStats->setText(tr("Level: %1/%2 | Iter: %3/%4 | Data RMS: %5 mGal | Chi RMS: %6")
                    .arg(level).arg(total_levels).arg(iter).arg(total_iters)
                    .arg(rms, 0, 'f', 4).arg(chi, 0, 'f', 3));
                appendLog(tr("  [Level %1/%2, Iter %3/%4] Data RMS = %5 mGal, Weighted Chi = %6")
                    .arg(level).arg(total_levels).arg(iter).arg(total_iters)
                    .arg(rms, 0, 'f', 4).arg(chi, 0, 'f', 3));
            }, Qt::QueuedConnection);
            return !st.stop_requested();
        };

        inv::InversionResult result;
        const bool ok = engine.invert_columns(d_fine, result.density_grid, result.stats,
                                              setup.topo_elevations, {}, nullptr, &result.modeled_gz);

        QMetaObject::invokeMethod(this, [this, ok, res = std::move(result), st_req = st.stop_requested()]() mutable {
            setRunningState(false);
            if (st_req) {
                m_lblStatus->setText(tr("Inversion cancelled by user."));
                appendLog(tr("==> Inversion cancelled."));
            } else if (ok) {
                m_progressBar->setValue(100);
                m_currentResult = std::move(res);
                m_btnApply->setEnabled(true);
                m_btnExportVtk->setEnabled(true);
                m_btnExportJson->setEnabled(true);
                m_lblStatus->setText(tr("Inversion completed successfully in %1 ms.")
                    .arg(m_currentResult->stats.elapsed_time_ms, 0, 'f', 1));
                m_lblCurrentStats->setText(tr("Levels: %1 | RMS: %2 mGal | Chi: %3 | S_MS: %4% | S_MGS: %5%")
                    .arg(m_currentResult->stats.levels_completed)
                    .arg(m_currentResult->stats.final_data_rms, 0, 'f', 4)
                    .arg(m_currentResult->stats.final_chi_rms, 0, 'f', 3)
                    .arg(m_currentResult->stats.minimum_support * 100.0, 0, 'f', 1)
                    .arg(m_currentResult->stats.minimum_gradient_support * 100.0, 0, 'f', 1));
                appendLog(tr("==> Inversion Finished Successfully!"));
                appendLog(tr("    Initial RMS: %1 mGal -> Final RMS: %2 mGal (Chi: %3)")
                    .arg(m_currentResult->stats.initial_data_rms, 0, 'f', 4)
                    .arg(m_currentResult->stats.final_data_rms, 0, 'f', 4)
                    .arg(m_currentResult->stats.final_chi_rms, 0, 'f', 3));
                appendLog(tr("    Minimum Support (Volume Fraction): %1%")
                    .arg(m_currentResult->stats.minimum_support * 100.0, 0, 'f', 2));
                appendLog(tr("    Minimum Gradient Support (Interface Area Fraction): %1%")
                    .arg(m_currentResult->stats.minimum_gradient_support * 100.0, 0, 'f', 2));
                appendLog(tr("    Elapsed Time: %1 ms").arg(m_currentResult->stats.elapsed_time_ms, 0, 'f', 1));
            } else {
                m_lblStatus->setText(tr("Inversion failed: %1").arg(QString::fromStdString(res.stats.message)));
                appendLog(tr("==> Failed: %1").arg(QString::fromStdString(res.stats.message)));
            }
        }, Qt::QueuedConnection);
    });
}

void Inversion3DDialog::onCancelClicked() {
    m_workerThread.request_stop();
    m_btnCancel->setEnabled(false);
    m_lblStatus->setText(tr("Stopping computation..."));
    appendLog(tr("Cancel requested by user..."));
}

void Inversion3DDialog::onApplyClicked() {
    if (!m_doc || !m_currentResult.has_value()) return;

    // 1. Store the full 3D inversion result in the document
    m_doc->setInversionResult(*m_currentResult);

    // 2. Populate modeled gravity grid and difference grid in document
    const auto *obsGrid = m_doc->activeObservedGrid();
    if (obsGrid && !m_currentResult->modeled_gz.empty()) {
        const size_t rows = obsGrid->rows();
        const size_t cols = obsGrid->cols();
        mod3d::Grid modGrid(rows, cols, obsGrid->x0(), obsGrid->y0(), obsGrid->dx(), obsGrid->dy());
        for (size_t ix = 0; ix < cols; ++ix) {
            for (size_t iy = 0; iy < rows; ++iy) {
                modGrid(iy, ix) = m_currentResult->modeled_gz[ix * rows + iy];
            }
        }

        m_doc->observation().set_modeled_grid(mod3d::FieldComponent::GZ, modGrid);
        m_doc->observation().compute_difference(mod3d::FieldComponent::GZ);
        m_doc->setShowModeledField(true);
        m_doc->setShowDifferenceField(true);
        m_doc->emit computationFinished();
        m_doc->setModified(true);

        QMessageBox::information(this, tr("Applied to Document"),
            tr("3D Inversion result applied successfully.\n\n"
               "Modeled Gz field and difference grid have been updated and are now visible across Map and Profile views."));
    }
}

void Inversion3DDialog::onExportVtkClicked() {
    if (!m_currentResult.has_value()) return;

    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export 3D Density Voxel Grid to VTK"), QString(), tr("VTK Structured Points (*.vtk)"));
    if (path.isEmpty()) return;

    if (m_currentResult->density_grid.export_vtk_structured_points(path.toStdString(), "density_contrast")) {
        QMessageBox::information(this, tr("Export Successful"),
            tr("3D density volume exported successfully to:\n%1").arg(path));
    } else {
        QMessageBox::critical(this, tr("Export Error"), tr("Failed to write VTK file."));
    }
}

void Inversion3DDialog::onExportJsonClicked() {
    if (!m_currentResult.has_value()) return;

    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export Inversion Result to JSON"), QString(), tr("JSON File (*.json)"));
    if (path.isEmpty()) return;

    if (inv::InversionEngine::save_result_to_json(path.toStdString(), *m_currentResult)) {
        QMessageBox::information(this, tr("Export Successful"),
            tr("Inversion result exported successfully to:\n%1").arg(path));
    } else {
        QMessageBox::critical(this, tr("Export Error"), tr("Failed to write JSON file."));
    }
}

void Inversion3DDialog::onBrowsePriorClicked() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Select Density Prior Model"), QString(), tr("All Supported Files (*.json *.onnx *.pt);;JSON (*.json);;ONNX (*.onnx)"));
    if (!path.isEmpty()) {
        m_txtPriorPath->setText(path);
    }
}

} // namespace mod3d_gui
