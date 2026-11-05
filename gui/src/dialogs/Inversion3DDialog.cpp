#include "mod3d_gui/dialogs/Inversion3DDialog.h"
#include "inv/neural_prior.hpp"
#include "mod3d/Units.h"

#include <QCloseEvent>
#include <QCoreApplication>
#include <QDateTime>
#include <QFileDialog>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QVBoxLayout>
#include <cmath>

namespace mod3d_gui {

Inversion3DDialog::Inversion3DDialog(ModelDocument *doc, QWidget *parent)
    : QDialog(parent), m_doc(doc) {
  setWindowTitle(tr("3D Potential Field Focusing Inversion"));
  setMinimumSize(700, 560);
  resize(760, 620);

  setupUi();
  updateDomainFromDocument();
  loadSettingsFromDocument();

  if (m_doc) {
    connect(m_doc, &ModelDocument::backgroundTrendModeChanged, this,
            [this](int mode) {
              if (m_comboTrendMode) {
                for (int i = 0; i < m_comboTrendMode->count(); ++i) {
                  if (m_comboTrendMode->itemData(i).toInt() == mode) {
                    m_comboTrendMode->setCurrentIndex(i);
                    break;
                  }
                }
              }
            });
  }
}

Inversion3DDialog::~Inversion3DDialog() {
  m_stopRequested.store(true, std::memory_order_relaxed);
  if (m_workerThread.joinable()) {
    m_workerThread.join();
  }
}

void Inversion3DDialog::waitForCompletion() {
  while (m_btnCancel && m_btnCancel->isEnabled()) {
    QCoreApplication::processEvents();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
}

void Inversion3DDialog::closeEvent(QCloseEvent *event) {
  saveSettingsToDocument();
  m_stopRequested.store(true, std::memory_order_relaxed);
  if (m_workerThread.joinable()) {
    m_workerThread.join();
  }
  event->accept();
}

void Inversion3DDialog::done(int r) {
  saveSettingsToDocument();
  m_stopRequested.store(true, std::memory_order_relaxed);
  if (m_workerThread.joinable()) {
    m_workerThread.join();
  }
  QDialog::done(r);
}

void Inversion3DDialog::setupUi() {
  auto *mainLayout = new QVBoxLayout(this);

  m_tabWidget = new QTabWidget(this);

  auto *domainTab = new QWidget(this);
  setupDomainTab(domainTab);
  m_tabWidget->addTab(domainTab, tr("1. Domain && Grid"));

  auto *focusTab = new QWidget(this);
  setupFocusingTab(focusTab);
  m_tabWidget->addTab(focusTab, tr("2. Focusing && Solver"));

  auto *priorsTab = new QWidget(this);
  setupPriorsTab(priorsTab);
  m_tabWidget->addTab(priorsTab, tr("3. Priors && Refinement"));

  auto *execTab = new QWidget(this);
  setupExecutionTab(execTab);
  m_tabWidget->addTab(execTab, tr("4. Convergence && Monitor"));

  mainLayout->addWidget(m_tabWidget);

  // Bottom action buttons: organized in two rows so all labels are fully
  // legible and unclipped
  auto *bottomContainer = new QVBoxLayout();
  bottomContainer->setSpacing(8);

  // Row 1: Execution and solver run actions
  auto *row1Layout = new QHBoxLayout();
  m_btnRun = new QPushButton(tr("Run Inversion"), this);
  m_btnRun->setDefault(true);
  m_btnRun->setStyleSheet("font-weight: bold; padding: 6px 16px;");

  m_btnColdStart = new QPushButton(tr("Run Cold Start"), this);
  m_btnColdStart->setToolTip(
      tr("Run a fresh inversion from scratch across all coarse-to-fine levels "
         "(ignores previous model)"));
  m_btnColdStart->setStyleSheet("padding: 6px 14px;");

  m_btnCancel = new QPushButton(tr("Cancel"), this);
  m_btnCancel->setEnabled(false);
  m_btnCancel->setStyleSheet("padding: 6px 16px;");

  row1Layout->addWidget(m_btnRun);
  row1Layout->addWidget(m_btnColdStart);
  row1Layout->addWidget(m_btnCancel);
  row1Layout->addStretch();

  // Row 2: Model application, export, and dialog close
  auto *row2Layout = new QHBoxLayout();
  m_btnApply = new QPushButton(tr("Apply to Document"), this);
  m_btnApply->setEnabled(false);
  m_btnApply->setStyleSheet("font-weight: bold; padding: 6px 16px;");

  m_btnExportVtk = new QPushButton(tr("Export VTK..."), this);
  m_btnExportVtk->setEnabled(false);
  m_btnExportVtk->setStyleSheet("padding: 6px 12px;");

  m_btnExportJson = new QPushButton(tr("Export JSON..."), this);
  m_btnExportJson->setEnabled(false);
  m_btnExportJson->setStyleSheet("padding: 6px 12px;");

  m_btnClose = new QPushButton(tr("Close"), this);
  m_btnClose->setStyleSheet("padding: 6px 18px;");

  row2Layout->addWidget(m_btnApply);
  row2Layout->addWidget(m_btnExportVtk);
  row2Layout->addWidget(m_btnExportJson);
  row2Layout->addStretch();
  row2Layout->addWidget(m_btnClose);

  bottomContainer->addLayout(row1Layout);
  bottomContainer->addLayout(row2Layout);
  mainLayout->addLayout(bottomContainer);

  connect(m_btnRun, &QPushButton::clicked, this,
          &Inversion3DDialog::onRunClicked);
  connect(m_btnColdStart, &QPushButton::clicked, this,
          &Inversion3DDialog::onRunColdStartClicked);
  connect(m_btnCancel, &QPushButton::clicked, this,
          &Inversion3DDialog::onCancelClicked);
  connect(m_btnApply, &QPushButton::clicked, this,
          &Inversion3DDialog::onApplyClicked);
  connect(m_btnExportVtk, &QPushButton::clicked, this,
          &Inversion3DDialog::onExportVtkClicked);
  connect(m_btnExportJson, &QPushButton::clicked, this,
          &Inversion3DDialog::onExportJsonClicked);
  connect(m_btnClose, &QPushButton::clicked, this, &QDialog::accept);
}

void Inversion3DDialog::setupDomainTab(QWidget *parent) {
  auto *layout = new QVBoxLayout(parent);

  auto *grpHorizontal =
      new QGroupBox(tr("Horizontal Model & Observation Domain"), parent);
  auto *formH = new QFormLayout(grpHorizontal);

  m_lblGridInfo = new QLabel(parent);
  formH->addRow(tr("Active Grid:"), m_lblGridInfo);

  m_lblCenteringNotice = new QLabel(parent);
  m_lblCenteringNotice->setWordWrap(true);
  m_lblCenteringNotice->setStyleSheet(
      "background-color: #f0f4f8; color: #1a365d; border: 1px solid #cbd5e1; "
      "border-radius: 4px; padding: 8px;");
  m_lblCenteringNotice->setText(
      tr("<b>Profile-Plane Centering (Invariant):</b><br>"
         "Voxel centers lie directly on Mod3D profile planes.<br>"
         "For each grid point (x<sub>i</sub>, y<sub>i</sub>), cell horizontal "
         "bounds are "
         "[x<sub>i</sub> &plusmn; &Delta;x/2, y<sub>i</sub> &plusmn; "
         "&Delta;y/2] with center at (x<sub>i</sub>, y<sub>i</sub>)."));
  formH->addRow(m_lblCenteringNotice);

  layout->addWidget(grpHorizontal);

  auto *grpVertical =
      new QGroupBox(tr("Vertical Subsurface Discretization"), parent);
  auto *formV = new QFormLayout(grpVertical);

  m_lblZRange = new QLabel(parent);
  formV->addRow(tr("Model Elevation Range:"), m_lblZRange);

  m_spinNz = new QSpinBox(parent);
  m_spinNz->setRange(2, 256);
  m_spinNz->setValue(24);
  formV->addRow(tr("Vertical Layers (Nz):"), m_spinNz);

  m_spinDz = new QDoubleSpinBox(parent);
  m_spinDz->setRange(0.01, 50000.0);
  m_spinDz->setDecimals(3);
  m_spinDz->setSuffix(" m");
  m_spinDz->setValue(100.0);
  formV->addRow(tr("Cell Thickness (&Delta;z):"), m_spinDz);

  m_spinObsHeight = new QDoubleSpinBox(parent);
  m_spinObsHeight->setRange(0.0, 50000.0);
  m_spinObsHeight->setDecimals(2);
  m_spinObsHeight->setSuffix(" m");
  m_spinObsHeight->setValue(0.0);
  m_spinObsHeight->setEnabled(false);
  m_spinObsHeight->setToolTip(
      tr("Current Gravity Sensor Height configured in Settings Pane -> Physics "
         "tab (read-only here to preserve synchronization)."));
  formV->addRow(tr("Gravity Sensor Height:"), m_spinObsHeight);

  layout->addWidget(grpVertical);
  layout->addStretch();

  // Link Nz and dz updates
  connect(m_spinNz, QOverload<int>::of(&QSpinBox::valueChanged), this,
          [this](int nz) {
            if (!m_doc)
              return;
            const double zMin = m_doc->model().z_min().value();
            const double zMax = m_doc->model().z_max().value();
            const double H = std::max(10.0, zMax - zMin);
            if (nz > 0) {
              m_spinDz->blockSignals(true);
              m_spinDz->setValue(H / nz);
              m_spinDz->blockSignals(false);
            }
            saveSettingsToDocument();
          });

  connect(m_spinDz, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          [this](double dz) {
            if (!m_doc || dz <= 0.0)
              return;
            const double zMin = m_doc->model().z_min().value();
            const double zMax = m_doc->model().z_max().value();
            const double H = std::max(10.0, zMax - zMin);
            const int nz = std::max(2, static_cast<int>(std::round(H / dz)));
            m_spinNz->blockSignals(true);
            m_spinNz->setValue(nz);
            m_spinNz->blockSignals(false);
            saveSettingsToDocument();
          });
}

void Inversion3DDialog::setupFocusingTab(QWidget *parent) {
  auto *layout = new QVBoxLayout(parent);

  auto *grpFocus =
      new QGroupBox(tr("Zhdanov Regularization & Multiscale"), parent);
  auto *formFocus = new QFormLayout(grpFocus);

  m_chkFocusing = new QCheckBox(
      tr("Enable Portniaguine-Zhdanov Focusing Stabilizers"), parent);
  m_chkFocusing->setChecked(true);
  formFocus->addRow(m_chkFocusing);

  m_comboLevels = new QComboBox(parent);
  m_comboLevels->addItem(tr("1 Level (Direct finest resolution)"), 1);
  m_comboLevels->addItem(tr("2 Levels (Coarse factor 2 -> 1)"), 2);
  m_comboLevels->addItem(tr("3 Levels (Hierarchical 4 -> 2 -> 1, Recommended)"),
                         3);
  m_comboLevels->setCurrentIndex(2);
  formFocus->addRow(tr("Coarse-to-Fine Hierarchy:"), m_comboLevels);

  m_chkAdaptiveRefine = new QCheckBox(
      tr("Adaptive Support Refinement (+1 halo on fine levels)"), parent);
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
  formFocus->addRow(tr("Minimum Gradient Support (S_MGS) Weight:"),
                    m_spinMgsWeight);

  m_spinDepthExp = new QDoubleSpinBox(parent);
  m_spinDepthExp->setRange(0.0, 2.0);
  m_spinDepthExp->setDecimals(2);
  m_spinDepthExp->setSingleStep(0.05);
  m_spinDepthExp->setValue(0.75);
  formFocus->addRow(tr("Depth Weighting Exponent (&gamma;):"), m_spinDepthExp);

  layout->addWidget(grpFocus);

  auto *grpPhysics =
      new QGroupBox(tr("Physics & Solver Configuration"), parent);
  auto *formPhys = new QFormLayout(grpPhysics);

  auto *densityLayout = new QHBoxLayout();
  m_spinMinDensity = new QDoubleSpinBox(parent);
  m_spinMinDensity->setRange(-10000.0, 10000.0);
  m_spinMinDensity->setDecimals(1);
  m_spinMinDensity->setSingleStep(50.0);
  m_spinMinDensity->setSuffix(QString(" %1").arg(
      QString::fromUtf8(units::KgPerM3::unit_symbol().data())));
  m_spinMinDensity->setValue(-500.0);

  m_spinMaxDensity = new QDoubleSpinBox(parent);
  m_spinMaxDensity->setRange(-10000.0, 10000.0);
  m_spinMaxDensity->setDecimals(1);
  m_spinMaxDensity->setSingleStep(50.0);
  m_spinMaxDensity->setSuffix(QString(" %1").arg(
      QString::fromUtf8(units::KgPerM3::unit_symbol().data())));
  m_spinMaxDensity->setValue(600.0);

  densityLayout->addWidget(new QLabel(tr("Min:"), parent));
  densityLayout->addWidget(m_spinMinDensity);
  densityLayout->addWidget(new QLabel(tr("Max:"), parent));
  densityLayout->addWidget(m_spinMaxDensity);

  m_btnCheckBottleneck = new QPushButton(tr("Check"), parent);
  m_btnCheckBottleneck->setToolTip(
      tr("Automatically analyze observed anomaly range, domain thickness, and "
         "3D voxel geometry to detect bottlenecks and set reasonable density "
         "bounds."));
  densityLayout->addWidget(m_btnCheckBottleneck);
  formPhys->addRow(tr("Density Contrast Bounds:"), densityLayout);

  connect(m_btnCheckBottleneck, &QPushButton::clicked, this,
          &Inversion3DDialog::onCheckDensityBottleneckClicked);
  connect(m_spinMinDensity,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          [this](double) { saveSettingsToDocument(); });
  connect(m_spinMaxDensity,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          [this](double) { saveSettingsToDocument(); });

  m_comboOperator = new QComboBox(parent);
  m_comboOperator->addItem(
      tr("pfld::InterfaceMesh (DEM terrain-conforming facets, Primary)"));
  m_comboOperator->addItem(tr("Toeplitz FFT (Matrix-free flat benchmark)"));
  m_comboOperator->setCurrentIndex(0);
  formPhys->addRow(tr("Forward Operator:"), m_comboOperator);

  m_comboTrendMode = new QComboBox(parent);
  m_comboTrendMode->addItem(tr("None (Assume zero background datum)"), 0);
  m_comboTrendMode->addItem(
      tr("Constant Shift c0 (Variable Projection, Recommended)"), 1);
  m_comboTrendMode->addItem(
      tr("Planar Regional Trend (c0 + gx·x + gy·y Joint Projection)"), 2);
  m_comboTrendMode->addItem(
      tr("Border-Fitted Regional Plane + Constant Shift c0"), 3);
  m_comboTrendMode->setCurrentIndex(1);
  m_comboTrendMode->setEnabled(false);
  m_comboTrendMode->setToolTip(
      tr("Current trend removal method configured in Mod3D Settings Pane -> "
         "Physics tab (read-only here to preserve synchronization)."));
  formPhys->addRow(tr("Background / Trend Removal:"), m_comboTrendMode);

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

  m_chkAutoTune = new QCheckBox(tr("Auto-Tune Hyperparameters (Nz, Density "
                                   "Bounds, \xce\xb3, \xce\xb1_reg, e_end)"),
                                parent);
  m_chkAutoTune->setChecked(false);
  m_chkAutoTune->setToolTip(
      tr("Runs a fast multi-stage coarse-grid tournament to optimize vertical "
         "layers (Nz), density contrast bounds, depth exponent (\xce\xb3), "
         "regularization (\xce\xb1), and focusing sharpness (e_end)"));
  formPhys->addRow(m_chkAutoTune);

  layout->addWidget(grpPhysics);
  layout->addStretch();
}

void Inversion3DDialog::setupPriorsTab(QWidget *parent) {
  auto *layout = new QVBoxLayout(parent);

  // Iterative Model Refinement (Warm Start) Group
  m_grpRefinement =
      new QGroupBox(tr("Iterative Model Refinement (Warm Start)"), parent);
  auto *vRefine = new QVBoxLayout(m_grpRefinement);

  m_chkRefinePrevious = new QCheckBox(
      tr("Refine previous inversion result (warm start and reference prior)"),
      parent);
  m_chkRefinePrevious->setChecked(true);
  vRefine->addWidget(m_chkRefinePrevious);

  m_lblRefineInfo = new QLabel(parent);
  m_lblRefineInfo->setWordWrap(true);
  m_lblRefineInfo->setStyleSheet("color: #4b5563; font-size: 11px;");
  vRefine->addWidget(m_lblRefineInfo);

  layout->addWidget(m_grpRefinement);

  // AI Neural Prior (3D Swin Transformer) Group
  m_grpNeural =
      new QGroupBox(tr("AI Neural Model (3D Swin Transformer)"), parent);
  auto *vNeural = new QVBoxLayout(m_grpNeural);

  m_chkUseNeuralPrior = new QCheckBox(
      tr("Enable 3D Swin Transformer as Inversion Prior"), parent);
  m_chkUseNeuralPrior->setChecked(inv::NeuralPriorInference::is_available());
  vNeural->addWidget(m_chkUseNeuralPrior);

  m_lblNeuralStatus = new QLabel(parent);
  if (inv::NeuralPriorInference::is_available()) {
    m_lblNeuralStatus->setText(tr(
        "✔ ONNX Runtime acceleration active (Apple Silicon / CoreML / CPU)."));
    m_lblNeuralStatus->setStyleSheet(
        "color: #15803d; font-weight: bold; font-size: 11px;");
  } else {
    m_lblNeuralStatus->setText(
        tr("✖ ONNX Runtime not available in this build."));
    m_lblNeuralStatus->setStyleSheet("color: #b91c1c; font-size: 11px;");
    m_chkUseNeuralPrior->setEnabled(false);
  }
  vNeural->addWidget(m_lblNeuralStatus);

  auto *priorPathLayout = new QHBoxLayout();
  m_txtPriorPath = new QLineEdit(parent);
  std::string defaultModelPath =
      inv::NeuralPriorInference::default_model_path();
  m_txtPriorPath->setText(QString::fromStdString(defaultModelPath));
  m_txtPriorPath->setPlaceholderText(tr("Path to inv_swin3d.onnx model"));
  m_btnBrowsePrior = new QPushButton(tr("Browse..."), parent);
  priorPathLayout->addWidget(new QLabel(tr("Model File:"), parent));
  priorPathLayout->addWidget(m_txtPriorPath);
  priorPathLayout->addWidget(m_btnBrowsePrior);
  vNeural->addLayout(priorPathLayout);

  auto *aiActionLayout = new QHBoxLayout();
  m_btnInstantAi =
      new QPushButton(tr("⚡ Run Instant AI Prediction (~50 ms)"), parent);
  m_btnInstantAi->setStyleSheet(
      "font-weight: bold; color: #1e3a8a; background-color: #e0f2fe; border: "
      "1px solid #7dd3fc; padding: 6px 12px; border-radius: 4px;");
  m_btnInstantAi->setEnabled(inv::NeuralPriorInference::is_available());
  aiActionLayout->addWidget(m_btnInstantAi);

  m_chkPriorAsRef =
      new QCheckBox(tr("Focusing reference model (S_MS / S_MGS)"), parent);
  m_chkPriorAsRef->setChecked(true);
  aiActionLayout->addSpacing(16);
  aiActionLayout->addWidget(m_chkPriorAsRef);
  aiActionLayout->addStretch();
  vNeural->addLayout(aiActionLayout);

  connect(m_btnBrowsePrior, &QPushButton::clicked, this,
          &Inversion3DDialog::onBrowsePriorClicked);
  connect(m_btnInstantAi, &QPushButton::clicked, this,
          &Inversion3DDialog::onInstantAiClicked);

  connect(m_chkRefinePrevious, &QCheckBox::toggled, this, [this](bool checked) {
    if (m_comboLevels) {
      m_comboLevels->setEnabled(!checked);
    }
    if (m_btnRun) {
      m_btnRun->setText(checked ? tr("Refine Previous Result")
                                : tr("Run Inversion"));
    }
    if (checked && m_chkUseNeuralPrior && m_chkUseNeuralPrior->isChecked()) {
      m_chkUseNeuralPrior->setChecked(false);
    }
  });
  connect(m_chkUseNeuralPrior, &QCheckBox::toggled, this, [this](bool checked) {
    if (checked && m_chkRefinePrevious && m_chkRefinePrevious->isEnabled() &&
        m_chkRefinePrevious->isChecked()) {
      m_chkRefinePrevious->setChecked(false);
    }
  });

  layout->addWidget(m_grpNeural);
  layout->addStretch();
}

void Inversion3DDialog::setupExecutionTab(QWidget *parent) {
  auto *layout = new QVBoxLayout(parent);

  auto *grpProgress =
      new QGroupBox(tr("Execution Status & Convergence Monitor"), parent);
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
  m_lblCurrentStats->setText(tr("Level: - | Active Cells: - | Data RMS: - mGal "
                                "| Chi RMS: - | MS: - | MGS: -"));
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
  if (!m_doc)
    return;

  const mod3d::Grid *obsGrid = m_doc->activeObservedGrid();
  if (!obsGrid || obsGrid->empty()) {
    m_lblGridInfo->setText(
        tr("<font color='red'>No observed gravity grid loaded!</font>"));
    m_btnRun->setEnabled(false);
    return;
  }

  const size_t nx = obsGrid->cols();
  const size_t ny = obsGrid->rows();
  const double dx = std::abs(obsGrid->dx().value());
  const double dy = std::abs(obsGrid->dy().value());
  const double x0 = obsGrid->x0().value();
  const double y0 = obsGrid->y0().value();

  m_lblGridInfo->setText(
      tr("%1 &times; %2 nodes (dx = %3 m, dy = %4 m, X: [%5, %6], Y: [%7, %8])")
          .arg(nx)
          .arg(ny)
          .arg(dx, 0, 'f', 1)
          .arg(dy, 0, 'f', 1)
          .arg(x0, 0, 'f', 0)
          .arg(x0 + (nx - 1) * dx, 0, 'f', 0)
          .arg(y0, 0, 'f', 0)
          .arg(y0 + (ny - 1) * dy, 0, 'f', 0));

  const double zMin = m_doc->model().z_min().value();
  const double zMax = m_doc->model().z_max().value();
  const double H = std::max(10.0, zMax - zMin);

  m_lblZRange->setText(tr("Elevation [%1 m, %2 m] (Total thickness: %3 m)")
                           .arg(zMin, 0, 'f', 0)
                           .arg(zMax, 0, 'f', 0)
                           .arg(H, 0, 'f', 0));

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
  if (m_btnColdStart) {
    m_btnColdStart->setEnabled(true);
  }
  updateRefinementState();
}

void Inversion3DDialog::updateRefinementState() {
  if (!m_chkRefinePrevious || !m_lblRefineInfo)
    return;

  const inv::InversionResult *prev = nullptr;
  if (m_currentResult.has_value()) {
    prev = &(*m_currentResult);
  } else if (m_doc && m_doc->hasInversionResult()) {
    prev = &(*m_doc->inversionResult());
  }

  if (prev && prev->density_grid.total_cells() > 0) {
    m_chkRefinePrevious->setEnabled(true);
    m_chkRefinePrevious->setChecked(
        m_doc ? m_doc->inversionSettings().refine_previous : true);
    m_lblRefineInfo->setText(
        tr("✔ Previous inversion result available: %1 &times; %2 &times; %3 "
           "(%4 cells, RMS = %5 mGal).\n"
           "When checked, re-inversion refines only the final level directly "
           "with your updated parameters.")
            .arg(prev->density_grid.nx())
            .arg(prev->density_grid.ny())
            .arg(prev->density_grid.nz())
            .arg(prev->density_grid.total_cells())
            .arg(prev->stats.final_data_rms, 0, 'f', 3));
    m_lblRefineInfo->setStyleSheet(
        "color: #15803d; font-weight: bold; font-size: 11px;");
    if (m_comboLevels) {
      m_comboLevels->setEnabled(!m_chkRefinePrevious->isChecked());
    }
    if (m_btnRun) {
      m_btnRun->setText(m_chkRefinePrevious->isChecked()
                            ? tr("Refine Previous Result")
                            : tr("Run Inversion"));
    }
    if (m_btnColdStart) {
      m_btnColdStart->setEnabled(true);
    }
  } else {
    m_chkRefinePrevious->setEnabled(false);
    m_chkRefinePrevious->setChecked(false);
    m_lblRefineInfo->setText(
        tr("No previous inversion result available in document to refine."));
    m_lblRefineInfo->setStyleSheet(
        "color: #6b7280; font-size: 11px; font-style: italic;");
    if (m_comboLevels) {
      m_comboLevels->setEnabled(true);
    }
    if (m_btnRun) {
      m_btnRun->setText(tr("Run Inversion"));
    }
    if (m_btnColdStart) {
      m_btnColdStart->setEnabled(true);
    }
  }
}

void Inversion3DDialog::loadSettingsFromDocument() {
  if (!m_doc)
    return;
  m_isLoadingSettings = true;
  const auto &s = m_doc->inversionSettings();
  const double zMin = m_doc->model().z_min().value();
  const double zMax = m_doc->model().z_max().value();
  const double H = std::max(10.0, zMax - zMin);
  if (s.nz > 0) {
    m_spinNz->blockSignals(true);
    m_spinNz->setValue(s.nz);
    m_spinNz->blockSignals(false);
    m_spinDz->blockSignals(true);
    m_spinDz->setValue(H / static_cast<double>(s.nz));
    m_spinDz->blockSignals(false);
  } else if (s.dz > 0.0) {
    m_spinDz->blockSignals(true);
    m_spinDz->setValue(s.dz);
    m_spinDz->blockSignals(false);
  }
  const double docGrvHeight =
      m_doc ? m_doc->observation().gravity_height().value() : s.obs_height;
  m_spinObsHeight->setValue(docGrvHeight);
  m_chkFocusing->setChecked(s.focusing_enabled);

  for (int i = 0; i < m_comboLevels->count(); ++i) {
    if (m_comboLevels->itemData(i).toInt() == s.coarse_to_fine_levels) {
      m_comboLevels->setCurrentIndex(i);
      break;
    }
  }

  m_chkAdaptiveRefine->setChecked(s.adaptive_refinement);
  m_spinMsWeight->setValue(s.ms_weight);
  m_spinMgsWeight->setValue(s.mgs_weight);
  m_spinDepthExp->setValue(s.depth_weight_exponent);
  m_spinMinDensity->setValue(s.min_density);
  m_spinMaxDensity->setValue(s.max_density);
  if (s.forward_operator >= 0 &&
      s.forward_operator < m_comboOperator->count()) {
    m_comboOperator->setCurrentIndex(s.forward_operator);
  }
  if (m_comboTrendMode) {
    const int docTrend =
        m_doc ? m_doc->backgroundTrendMode() : s.background_trend_mode;
    for (int i = 0; i < m_comboTrendMode->count(); ++i) {
      if (m_comboTrendMode->itemData(i).toInt() == docTrend) {
        m_comboTrendMode->setCurrentIndex(i);
        break;
      }
    }
  }
  m_spinNoiseRel->setValue(s.noise_rel);
  m_spinIrlsIters->setValue(s.irls_iterations);
  m_spinCgMaxIters->setValue(s.cg_max_iterations);
  if (m_chkAutoTune) {
    const bool alreadyAutoTuned = m_doc->hasInversionResult() &&
                                  m_doc->inversionResult()->stats.auto_tuned;
    m_chkAutoTune->setChecked(s.auto_tune_params && !alreadyAutoTuned);
  }
  m_chkUseNeuralPrior->setChecked(s.use_neural_prior &&
                                  inv::NeuralPriorInference::is_available());
  if (!s.onnx_model_path.empty()) {
    m_txtPriorPath->setText(QString::fromStdString(s.onnx_model_path));
  }
  m_chkPriorAsRef->setChecked(s.prior_as_reference);

  updateRefinementState();
  if (m_chkRefinePrevious->isEnabled()) {
    m_chkRefinePrevious->setChecked(s.refine_previous);
  }
  m_isLoadingSettings = false;
}

void Inversion3DDialog::saveSettingsToDocument() {
  if (m_isLoadingSettings || !m_doc)
    return;
  const double zMin = m_doc->model().z_min().value();
  const double zMax = m_doc->model().z_max().value();
  const double H = std::max(10.0, zMax - zMin);
  mod3d::InversionSettings3D s = m_doc->inversionSettings();
  s.nz = m_spinNz->value();
  s.dz = (s.nz > 0) ? (H / static_cast<double>(s.nz)) : m_spinDz->value();
  s.obs_height = m_doc ? m_doc->observation().gravity_height().value()
                       : m_spinObsHeight->value();
  s.focusing_enabled = m_chkFocusing->isChecked();
  s.coarse_to_fine_levels = m_comboLevels->currentData().toInt();
  s.adaptive_refinement = m_chkAdaptiveRefine->isChecked();
  s.ms_weight = m_spinMsWeight->value();
  s.mgs_weight = m_spinMgsWeight->value();
  s.depth_weight_exponent = m_spinDepthExp->value();
  s.min_density = m_spinMinDensity->value();
  s.max_density = m_spinMaxDensity->value();
  s.forward_operator = m_comboOperator->currentIndex();
  s.background_trend_mode =
      m_comboTrendMode ? m_comboTrendMode->currentData().toInt() : 1;
  s.noise_rel = m_spinNoiseRel->value();
  s.irls_iterations = m_spinIrlsIters->value();
  s.cg_max_iterations = m_spinCgMaxIters->value();
  s.auto_tune_params = m_chkAutoTune ? m_chkAutoTune->isChecked() : false;
  s.use_neural_prior = m_chkUseNeuralPrior->isChecked();
  s.onnx_model_path = m_txtPriorPath->text().toStdString();
  s.prior_as_reference = m_chkPriorAsRef->isChecked();
  if (m_chkRefinePrevious && m_chkRefinePrevious->isEnabled()) {
    s.refine_previous = m_chkRefinePrevious->isChecked();
  }
  m_doc->setInversionSettings(s);
}

void Inversion3DDialog::setRunningState(bool running) {
  m_btnRun->setEnabled(!running);
  if (m_btnColdStart) {
    m_btnColdStart->setEnabled(!running);
  }
  m_btnCancel->setEnabled(running);
  m_btnClose->setEnabled(!running);
  m_tabWidget->tabBar()->setEnabled(!running);
}

void Inversion3DDialog::appendLog(const QString &text) {
  const QString timeStr = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
  m_txtLog->append(QString("[%1] %2").arg(timeStr, text));
}

void Inversion3DDialog::onRunClicked() { startInversion(false); }

void Inversion3DDialog::onRunColdStartClicked() { startInversion(true); }

void Inversion3DDialog::startInversion(bool forceColdStart) {
  if (!m_doc)
    return;
  const mod3d::Grid *obsGrid = m_doc->activeObservedGrid();
  if (!obsGrid || obsGrid->empty()) {
    QMessageBox::warning(
        this, tr("No Data"),
        tr("No active observed gravity grid found in document."));
    return;
  }

  const size_t nx = obsGrid->cols();
  const size_t ny = obsGrid->rows();
  const size_t nz = static_cast<size_t>(m_spinNz->value());
  const double dx = std::abs(obsGrid->dx().value());
  const double dy = std::abs(obsGrid->dy().value());
  const double zMin = m_doc->model().z_min().value();
  const double zMax = m_doc->model().z_max().value();
  const double H = std::max(10.0, zMax - zMin);
  const double dz =
      (nz > 0) ? (H / static_cast<double>(nz)) : m_spinDz->value();

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
  // Use units library for conversion safety: GUI kg/m³ -> solver g/cm³
  setup.config.min_density =
      units::KgPerM3(m_spinMinDensity->value()).in<units::GPerCm3>();
  setup.config.max_density =
      units::KgPerM3(m_spinMaxDensity->value()).in<units::GPerCm3>();
  setup.config.use_fft_solver_benchmark =
      (m_comboOperator->currentIndex() == 1);
  // Observed grid from Mod3D (activeObservedGrid) is already detrended
  // according to Mod3D's Physics tab. Pass background_trend_mode = 0 to the
  // solver so G * m fits the detrended Mod3D field directly without
  // double-detrending or introducing an unmodeled baseline shift.
  setup.config.background_trend_mode = 0;
  setup.config.auto_tune_params =
      m_chkAutoTune ? m_chkAutoTune->isChecked() : false;
  setup.config.noise_rel = m_spinNoiseRel->value() * 0.01;
  setup.config.irls_iterations = m_spinIrlsIters->value();
  setup.config.cg_max_iterations = m_spinCgMaxIters->value();
  const double rawObsH =
      m_doc ? m_doc->observation().gravity_height().value()
            : units::Meters(m_spinObsHeight->value()).in<units::Meters>();
  setup.config.obs_height = (m_doc->observation().gravity_mode() ==
                                 mod3d::ObservationMode::SensorHeight &&
                             rawObsH <= 0.0)
                                ? 0.1
                                : rawObsH;
  setup.config.dz = units::Meters(dz).in<units::Meters>();
  setup.config.onnx_model_path = m_txtPriorPath->text().toStdString();
  if (!setup.config.auto_tune_params) {
    const inv::InversionResult *prevTuned =
        m_currentResult.has_value()
            ? &(*m_currentResult)
            : (m_doc->hasInversionResult() ? &(*m_doc->inversionResult())
                                           : nullptr);
    if (prevTuned && prevTuned->stats.auto_tuned) {
      if (prevTuned->stats.tuned_alpha_reg > 0.0) {
        setup.config.alpha_reg = prevTuned->stats.tuned_alpha_reg;
      }
      if (prevTuned->stats.tuned_focusing_e_end_rel > 0.0) {
        setup.config.focusing_e_end_rel =
            prevTuned->stats.tuned_focusing_e_end_rel;
      }
    }
  }

  // Collect observation raster: index ix * ny + iy
  std::vector<double> d_fine(nx * ny, 0.0);
  for (size_t ix = 0; ix < nx; ++ix) {
    for (size_t iy = 0; iy < ny; ++iy) {
      const double v = (*obsGrid)(iy, ix);
      d_fine[ix * ny + iy] =
          (v == mod3d::GRID_DUMMY || std::isnan(v)) ? 0.0 : v;
    }
  }

  // Topography surface nodes at cell corners: size (nx + 1) * (ny + 1)
  // In Mod3D, zMax is domain top elevation. Depth z_depth = zDomainTop -
  // elevation >= 0.
  const double zDomainTop = m_doc->model().z_max().value();
  const auto &relief = m_doc->observation().surface_relief();
  const bool hasRelief =
      !relief.empty() && relief.rows() >= 2 && relief.cols() >= 2;
  std::vector<double> topo_nodes((nx + 1) * (ny + 1), 0.0);
  for (size_t iy = 0; iy <= ny; ++iy) {
    const double y_c = y0_geom + static_cast<double>(iy) * dy;
    for (size_t ix = 0; ix <= nx; ++ix) {
      const double x_c = x0_geom + static_cast<double>(ix) * dx;
      double elev = 0.0;
      if (hasRelief) {
        elev = relief.sample(x_c, y_c);
        if (elev == mod3d::GRID_DUMMY || std::isnan(elev))
          elev = relief.mean();
      }
      topo_nodes[iy * (nx + 1) + ix] = std::max(0.0, zDomainTop - elev);
    }
  }
  setup.topo_elevations = std::move(topo_nodes);

  // Optional per-column station depths for FlightElevation or ElevationGrid
  // modes
  std::vector<double> obs_z_cols;
  const auto grvMode = m_doc->observation().gravity_mode();
  if (grvMode == mod3d::ObservationMode::FlightElevation) {
    obs_z_cols.assign(nx * ny, zDomainTop - setup.config.obs_height);
  } else if (grvMode == mod3d::ObservationMode::ElevationGrid &&
             !m_doc->observation().gravity_elevation_grid().empty()) {
    const auto &eg = m_doc->observation().gravity_elevation_grid();
    obs_z_cols.resize(nx * ny, 0.0);
    for (size_t ix = 0; ix < nx; ++ix) {
      for (size_t iy = 0; iy < ny; ++iy) {
        double ez = (iy < eg.rows() && ix < eg.cols())
                        ? eg(iy, ix)
                        : setup.config.obs_height;
        if (ez == mod3d::GRID_DUMMY || std::isnan(ez))
          ez = setup.config.obs_height;
        obs_z_cols[ix * ny + iy] = zDomainTop - ez;
      }
    }
  }

  // Switch to execution tab & clear previous stats
  m_tabWidget->setCurrentIndex(3);
  m_txtLog->clear();
  m_progressBar->setValue(0);
  m_btnApply->setEnabled(false);
  m_btnExportVtk->setEnabled(false);
  m_btnExportJson->setEnabled(false);
  setRunningState(true);

  appendLog(tr("Starting 3D Focusing Inversion (%1x%2x%3 voxels)...")
                .arg(nx)
                .arg(ny)
                .arg(nz));
  appendLog(tr("Voxel Centers aligned with profile planes: x0 = %1 m, y0 = %2 "
               "m, dx = %3 m, dy = %4 m, dz = %5 m")
                .arg(x0_geom, 0, 'f', 1)
                .arg(y0_geom, 0, 'f', 1)
                .arg(dx, 0, 'f', 1)
                .arg(dy, 0, 'f', 1)
                .arg(dz, 0, 'f', 3));
  appendLog(tr("Forward operator: %1")
                .arg(setup.config.use_fft_solver_benchmark
                         ? "Toeplitz FFT benchmark"
                         : "pfld::InterfaceMesh DEM facets"));

  m_lastSetup = setup;
  saveSettingsToDocument();

  // Check density bound bottleneck before starting
  const auto bottleneckReport = checkDensityBottleneck(/*applyBounds=*/false);
  if (bottleneckReport.has_bottleneck) {
    appendLog(tr("⚠ WARNING: Density bound bottleneck detected! Bounds [%1, "
                 "%2] kg/m³ may restrict reproduction of observed anomaly.")
                  .arg(bottleneckReport.current_min_density_kg, 0, 'f', 0)
                  .arg(bottleneckReport.current_max_density_kg, 0, 'f', 0));
    appendLog(tr("    Recommended bounds: [%1, %2] kg/m³. Click 'Check Density "
                 "Bottleneck' on the Physics tab to auto-set.")
                  .arg(bottleneckReport.recommended_min_density_kg, 0, 'f', 0)
                  .arg(bottleneckReport.recommended_max_density_kg, 0, 'f', 0));
  }

  const bool refinePrev = !forceColdStart && m_chkRefinePrevious &&
                          m_chkRefinePrevious->isChecked() &&
                          m_chkRefinePrevious->isEnabled();
  const inv::InversionResult *prevRes = nullptr;
  if (refinePrev) {
    if (m_currentResult.has_value()) {
      prevRes = &(*m_currentResult);
    } else if (m_doc && m_doc->hasInversionResult()) {
      prevRes = &(*m_doc->inversionResult());
    }
  }

  std::vector<double> prior_values;
  bool isRefining = false;
  if (refinePrev && prevRes && prevRes->density_grid.total_cells() > 0) {
    isRefining = true;
    inv::VoxelGrid<double> resampled = prevRes->density_grid.resample(
        nx, ny, nz, x0_geom, y0_geom, z0_geom, dx, dy, dz);
    prior_values = std::move(resampled.data());
    setup.config.scale_prior = false;
    setup.config.prior_as_reference = true;
    setup.config.refine_previous = true;
    setup.config.coarse_to_fine_levels =
        1; // Don't apply all levels; refine only last level with new params
    appendLog(tr("==> Refinement Mode: Warm start initialized from previous "
                 "inversion result (%1 cells resampled to %2x%3x%4).")
                  .arg(prevRes->density_grid.total_cells())
                  .arg(nx)
                  .arg(ny)
                  .arg(nz));
    appendLog(tr("    Refining only final level directly with updated "
                 "parameters (coarse-to-fine skipped)."));
    appendLog(tr("    Previous RMS: %1 mGal -> Starting from previous solution "
                 "and focusing relative to it.")
                  .arg(prevRes->stats.final_data_rms, 0, 'f', 4));
  } else if (forceColdStart) {
    appendLog(tr("==> Cold Start Mode: Starting fresh inversion from scratch "
                 "across all %1 coarse-to-fine levels (zero initial model).")
                  .arg(setup.config.coarse_to_fine_levels));
  }

  m_currentResult.reset();

  if (setup.config.auto_tune_params && !isRefining) {
    appendLog(tr("==> Hyperparameter Auto-Tuning active: optimizing Nz, "
                 "bounds, gamma, alpha_reg, and e_end before main inversion."));
  }

  const bool useNeural =
      (m_chkUseNeuralPrior && m_chkUseNeuralPrior->isChecked() &&
       inv::NeuralPriorInference::is_available());

  if (m_workerThread.joinable()) {
    m_workerThread.join();
  }
  m_stopRequested.store(false, std::memory_order_relaxed);

  // Launch background worker using inv::jthread
  m_workerThread = inv::jthread([this, setup = std::move(setup),
                                 d_fine = std::move(d_fine),
                                 obs_z_cols = std::move(obs_z_cols),
                                 prior_values = std::move(prior_values),
                                 isRefining, useNeural]() mutable {
    if (!isRefining && useNeural) {
      std::string modelPath = setup.config.onnx_model_path;
      if (modelPath.empty()) {
        modelPath = inv::NeuralPriorInference::default_model_path();
      }
      inv::VoxelGrid<double> prior_grid;
      std::string err;
      if (inv::NeuralPriorInference::predict(
              d_fine, setup.config.nx, setup.config.ny, setup.config.nz,
              setup.config.x0, setup.config.y0, setup.config.z0,
              setup.config.dx, setup.config.dy, setup.config.dz, prior_grid,
              err, modelPath)) {
        prior_values = std::move(prior_grid.data());
        QMetaObject::invokeMethod(
            this,
            [this, n = prior_values.size()]() {
              appendLog(tr("  [AI Prior] Swin 3D Transformer prior evaluated "
                           "(%1 cells) and loaded as reference.")
                            .arg(n));
            },
            Qt::QueuedConnection);
      } else {
        QMetaObject::invokeMethod(
            this,
            [this, msg = QString::fromStdString(err)]() {
              appendLog(tr("  [AI Prior Warning] Could not evaluate ONNX "
                           "prior: %1. Proceeding with standard focusing.")
                            .arg(msg));
            },
            Qt::QueuedConnection);
      }
    }

    inv::InversionEngine engine(setup.config);
    inv::InversionConfig &cfg =
        const_cast<inv::InversionConfig &>(engine.config());

    cfg.cancel_check = [this]() -> bool {
      return m_stopRequested.load(std::memory_order_relaxed);
    };

    cfg.tuning_progress_callback = [this](int trial, int total_trials,
                                          const std::string &desc, double score,
                                          double best_score) -> bool {
      if (m_stopRequested.load(std::memory_order_relaxed))
        return false;
      QMetaObject::invokeMethod(
          this,
          [this, trial, total_trials, desc, score, best_score]() {
            const int pct = static_cast<int>(100.0 * (trial - 1) /
                                             std::max(1, total_trials));
            m_progressBar->setValue(std::clamp(pct, 0, 99));
            m_lblStatus->setText(tr("Auto-Tuning [%1/%2]: %3")
                                     .arg(trial)
                                     .arg(total_trials)
                                     .arg(QString::fromStdString(desc)));
            if (score >= 0.0) {
              m_lblCurrentStats->setText(
                  tr("Tuning Trial %1/%2 | Score: %3 (Best: %4)")
                      .arg(trial)
                      .arg(total_trials)
                      .arg(score, 0, 'f', 3)
                      .arg(best_score < 1e200
                               ? QString::number(best_score, 'f', 3)
                               : QString("-")));
              appendLog(tr("  [Auto-Tune %1/%2] %3 -> Score: %4 (Best: %5)")
                            .arg(trial)
                            .arg(total_trials)
                            .arg(QString::fromStdString(desc))
                            .arg(score, 0, 'f', 3)
                            .arg(best_score < 1e200
                                     ? QString::number(best_score, 'f', 3)
                                     : QString("-")));
            } else {
              m_lblCurrentStats->setText(
                  tr("Tuning Trial %1/%2 | Evaluating %3...")
                      .arg(trial)
                      .arg(total_trials)
                      .arg(QString::fromStdString(desc)));
            }
          },
          Qt::QueuedConnection);
      return !m_stopRequested.load(std::memory_order_relaxed);
    };

    cfg.progress_callback = [this](int level, int total_levels, int iter,
                                   int total_iters, double rms,
                                   double chi) -> bool {
      if (m_stopRequested.load(std::memory_order_relaxed))
        return false;
      QMetaObject::invokeMethod(
          this,
          [this, level, total_levels, iter, total_iters, rms, chi]() {
            const int pct =
                static_cast<int>(100.0 * ((level - 1) * total_iters + iter) /
                                 (total_levels * total_iters));
            m_progressBar->setValue(std::clamp(pct, 0, 99));
            m_lblStatus->setText(tr("Level %1/%2: IRLS Iteration %3/%4 (RMS: "
                                    "%5 mGal, &chi;: %6)")
                                     .arg(level)
                                     .arg(total_levels)
                                     .arg(iter)
                                     .arg(total_iters)
                                     .arg(rms, 0, 'f', 4)
                                     .arg(chi, 0, 'f', 3));
            m_lblCurrentStats->setText(tr("Level: %1/%2 | Iter: %3/%4 | Data "
                                          "RMS: %5 mGal | Chi RMS: %6")
                                           .arg(level)
                                           .arg(total_levels)
                                           .arg(iter)
                                           .arg(total_iters)
                                           .arg(rms, 0, 'f', 4)
                                           .arg(chi, 0, 'f', 3));
            appendLog(tr("  [Level %1/%2, Iter %3/%4] Data RMS = %5 mGal, "
                         "Weighted Chi = %6")
                          .arg(level)
                          .arg(total_levels)
                          .arg(iter)
                          .arg(total_iters)
                          .arg(rms, 0, 'f', 4)
                          .arg(chi, 0, 'f', 3));
          },
          Qt::QueuedConnection);
      return !m_stopRequested.load(std::memory_order_relaxed);
    };

    inv::InversionResult result;
    const bool ok = engine.invert_columns(
        d_fine, result.density_grid, result.stats, setup.topo_elevations,
        obs_z_cols, prior_values.empty() ? nullptr : &prior_values,
        &result.modeled_gz);

    const bool st_req = m_stopRequested.load(std::memory_order_relaxed);
    QMetaObject::invokeMethod(
        this,
        [this, ok, res = std::move(result), st_req]() mutable {
          setRunningState(false);
          if (st_req) {
            m_lblStatus->setText(tr("Inversion cancelled by user."));
            appendLog(tr("==> Inversion cancelled."));
          } else if (ok) {
            m_progressBar->setValue(100);
            m_currentResult = std::move(res);
            updateRefinementState();
            m_btnApply->setEnabled(true);
            m_btnExportVtk->setEnabled(true);
            m_btnExportJson->setEnabled(true);
            m_lblStatus->setText(
                tr("Inversion completed successfully in %1 ms.")
                    .arg(m_currentResult->stats.elapsed_time_ms, 0, 'f', 1));
            m_lblCurrentStats->setText(
                tr("Levels: %1 | RMS: %2 mGal | Chi: %3 | S_MS: %4% | S_MGS: "
                   "%5%")
                    .arg(m_currentResult->stats.levels_completed)
                    .arg(m_currentResult->stats.final_data_rms, 0, 'f', 4)
                    .arg(m_currentResult->stats.final_chi_rms, 0, 'f', 3)
                    .arg(m_currentResult->stats.minimum_support * 100.0, 0, 'f',
                         1)
                    .arg(m_currentResult->stats.minimum_gradient_support *
                             100.0,
                         0, 'f', 1));
            appendLog(tr("==> Inversion Finished Successfully!"));
            appendLog(
                tr("    Initial RMS: %1 mGal -> Final RMS: %2 mGal (Chi: %3)")
                    .arg(m_currentResult->stats.initial_data_rms, 0, 'f', 4)
                    .arg(m_currentResult->stats.final_data_rms, 0, 'f', 4)
                    .arg(m_currentResult->stats.final_chi_rms, 0, 'f', 3));
            appendLog(tr("    Minimum Support (Volume Fraction): %1%")
                          .arg(m_currentResult->stats.minimum_support * 100.0,
                               0, 'f', 2));
            appendLog(tr("    Minimum Gradient Support (Interface Area "
                         "Fraction): %1%")
                          .arg(m_currentResult->stats.minimum_gradient_support *
                                   100.0,
                               0, 'f', 2));
            if (m_currentResult->stats.auto_tuned) {
              const double tunedMinKg =
                  units::GPerCm3(m_currentResult->stats.tuned_min_density)
                      .in<units::KgPerM3>();
              const double tunedMaxKg =
                  units::GPerCm3(m_currentResult->stats.tuned_max_density)
                      .in<units::KgPerM3>();
              const QString densUnit =
                  QString::fromUtf8(units::KgPerM3::unit_symbol().data());
              m_spinNz->blockSignals(true);
              m_spinNz->setValue(
                  static_cast<int>(m_currentResult->stats.tuned_nz));
              m_spinNz->blockSignals(false);
              m_spinDz->blockSignals(true);
              m_spinDz->setValue(m_currentResult->stats.tuned_dz);
              m_spinDz->blockSignals(false);
              m_spinMinDensity->setValue(tunedMinKg);
              m_spinMaxDensity->setValue(tunedMaxKg);
              m_spinDepthExp->setValue(
                  m_currentResult->stats.tuned_depth_exponent);
              if (m_chkAutoTune) {
                m_chkAutoTune->setChecked(false);
              }
              saveSettingsToDocument();
              appendLog(
                  tr("    Auto-Tuned Parameters (%1 trials): Nz = %2 (dz = %3 "
                     "m), Bounds = [%4, %5] %6, \xce\xb3 = %7, \xce\xb1_reg = "
                     "%8, e_end = %9")
                      .arg(m_currentResult->stats.tuning_trials_evaluated)
                      .arg(m_currentResult->stats.tuned_nz)
                      .arg(m_currentResult->stats.tuned_dz, 0, 'f', 3)
                      .arg(tunedMinKg, 0, 'f', 1)
                      .arg(tunedMaxKg, 0, 'f', 1)
                      .arg(densUnit)
                      .arg(m_currentResult->stats.tuned_depth_exponent, 0, 'f',
                           2)
                      .arg(m_currentResult->stats.tuned_alpha_reg, 0, 'f', 2)
                      .arg(m_currentResult->stats.tuned_focusing_e_end_rel, 0,
                           'f', 4));
            }
            if (std::abs(m_currentResult->stats.baseline_shift_mgal) > 1e-6 ||
                std::abs(m_currentResult->stats.trend_gx_mgal_per_km) > 1e-6 ||
                std::abs(m_currentResult->stats.trend_gy_mgal_per_km) > 1e-6) {
              appendLog(tr("    Background Alignment: Shift c0 = %1 mGal, "
                           "Regional Slope = (%2, %3) mGal/km")
                            .arg(m_currentResult->stats.baseline_shift_mgal, 0,
                                 'f', 4)
                            .arg(m_currentResult->stats.trend_gx_mgal_per_km, 0,
                                 'f', 4)
                            .arg(m_currentResult->stats.trend_gy_mgal_per_km, 0,
                                 'f', 4));
            }
            const double minDensKg =
                units::GPerCm3(m_currentResult->density_grid.min_value())
                    .in<units::KgPerM3>();
            const double maxDensKg =
                units::GPerCm3(m_currentResult->density_grid.max_value())
                    .in<units::KgPerM3>();
            const QString densUnit =
                QString::fromUtf8(units::KgPerM3::unit_symbol().data());
            appendLog(tr("    Recovered Density Contrast: [%1, %2] %3")
                          .arg(minDensKg, 0, 'f', 1)
                          .arg(maxDensKg, 0, 'f', 1)
                          .arg(densUnit));
            appendLog(
                tr("    Elapsed Time: %1 ms")
                    .arg(m_currentResult->stats.elapsed_time_ms, 0, 'f', 1));
          } else {
            m_lblStatus->setText(
                tr("Inversion failed: %1")
                    .arg(QString::fromStdString(res.stats.message)));
            appendLog(tr("==> Failed: %1")
                          .arg(QString::fromStdString(res.stats.message)));
          }
        },
        Qt::QueuedConnection);
  });
}

void Inversion3DDialog::onCancelClicked() {
  m_stopRequested.store(true, std::memory_order_relaxed);
  m_btnCancel->setEnabled(false);
  m_lblStatus->setText(tr("Stopping computation..."));
  appendLog(tr("Cancel requested by user..."));
}

void Inversion3DDialog::onApplyClicked() {
  if (!m_doc || !m_currentResult.has_value())
    return;

  // 1. Store the full 3D inversion result in the document
  m_doc->setInversionResult(*m_currentResult);
  saveSettingsToDocument();
  updateRefinementState();

  // 2. Populate modeled gravity grid and difference grid in document
  const auto *obsGrid = m_doc->activeObservedGrid();
  if (obsGrid && !m_currentResult->modeled_gz.empty()) {
    const size_t rows = obsGrid->rows();
    const size_t cols = obsGrid->cols();
    mod3d::Grid modGrid(rows, cols, obsGrid->x0(), obsGrid->y0(), obsGrid->dx(),
                        obsGrid->dy());
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

    if (isVisible()) {
      QMessageBox::information(
          this, tr("Applied to Document"),
          tr("3D Inversion result applied successfully.\n\n"
             "Modeled Gz field and difference grid have been updated and are "
             "now visible across Map and Profile views."));
    }
  }
}

void Inversion3DDialog::onExportVtkClicked() {
  if (!m_currentResult.has_value())
    return;

  const QString path = QFileDialog::getSaveFileName(
      this, tr("Export 3D Density Voxel Grid to VTK"), QString(),
      tr("VTK Structured Points (*.vtk)"));
  if (path.isEmpty())
    return;

  if (m_currentResult->density_grid.export_vtk_structured_points(
          path.toStdString(), "density_contrast")) {
    if (isVisible()) {
      QMessageBox::information(
          this, tr("Export Successful"),
          tr("3D density volume exported successfully to:\n%1").arg(path));
    }
  } else {
    if (isVisible()) {
      QMessageBox::critical(this, tr("Export Error"),
                            tr("Failed to write VTK file."));
    }
  }
}

void Inversion3DDialog::onExportJsonClicked() {
  if (!m_currentResult.has_value())
    return;

  const QString path =
      QFileDialog::getSaveFileName(this, tr("Export Inversion Result to JSON"),
                                   QString(), tr("JSON File (*.json)"));
  if (path.isEmpty())
    return;

  if (inv::InversionEngine::save_result_to_json(path.toStdString(),
                                                *m_currentResult)) {
    if (isVisible()) {
      QMessageBox::information(
          this, tr("Export Successful"),
          tr("Inversion result exported successfully to:\n%1").arg(path));
    }
  } else {
    if (isVisible()) {
      QMessageBox::critical(this, tr("Export Error"),
                            tr("Failed to write JSON file."));
    }
  }
}

void Inversion3DDialog::onBrowsePriorClicked() {
  const QString path = QFileDialog::getOpenFileName(
      this, tr("Select Density Prior Model"), QString(),
      tr("All Supported Files (*.json *.onnx *.pt);;JSON (*.json);;ONNX "
         "(*.onnx)"));
  if (!path.isEmpty()) {
    m_txtPriorPath->setText(path);
  }
}

void Inversion3DDialog::onInstantAiClicked() {
  if (!m_doc)
    return;

  // Check observation grid (already detrended by Mod3D Physics tab)
  const auto *obsGrid = m_doc->activeObservedGrid();
  if (!obsGrid || obsGrid->empty()) {
    QMessageBox::warning(this, tr("Observation Missing"),
                         tr("3D Inversion requires an active observed gravity "
                            "grid (gz or g_tot)."));
    return;
  }

  const size_t nx = obsGrid->cols();
  const size_t ny = obsGrid->rows();
  const size_t nz = static_cast<size_t>(m_spinNz->value());
  const double dx = std::abs(obsGrid->dx().value());
  const double dy = std::abs(obsGrid->dy().value());
  const double zMin = m_doc->model().z_min().value();
  const double zMax = m_doc->model().z_max().value();
  const double H = std::max(10.0, zMax - zMin);
  const double dz =
      (nz > 0) ? (H / static_cast<double>(nz)) : m_spinDz->value();

  const double x0_geom = obsGrid->x0().value() - 0.5 * dx;
  const double y0_geom = obsGrid->y0().value() - 0.5 * dy;
  const double z0_geom = 0.0;

  std::vector<double> d_fine(nx * ny, 0.0);
  double dmax = 0.0, dsum2 = 0.0;
  for (size_t ix = 0; ix < nx; ++ix) {
    for (size_t iy = 0; iy < ny; ++iy) {
      const double v = (*obsGrid)(iy, ix);
      double val = (v == mod3d::GRID_DUMMY || std::isnan(v)) ? 0.0 : v;
      d_fine[ix * ny + iy] = val;
      dsum2 += val * val;
      dmax = std::max(dmax, std::abs(val));
    }
  }
  const double init_rms = std::sqrt(dsum2 / (nx * ny));

  // Switch to execution tab & reset
  m_tabWidget->setCurrentIndex(3);
  m_txtLog->clear();
  m_progressBar->setValue(0);
  m_currentResult.reset();
  m_btnApply->setEnabled(false);
  m_btnExportVtk->setEnabled(false);
  m_btnExportJson->setEnabled(false);

  appendLog(tr("⚡ Running Instant 3D Swin Transformer Inference (%1x%2x%3 "
               "voxels)...")
                .arg(nx)
                .arg(ny)
                .arg(nz));

  const auto t_start = std::chrono::high_resolution_clock::now();
  inv::VoxelGrid<double> pred_grid;
  std::string err;
  std::string modelPath = m_txtPriorPath->text().toStdString();
  if (modelPath.empty()) {
    modelPath = inv::NeuralPriorInference::default_model_path();
  }

  bool ok = inv::NeuralPriorInference::predict(d_fine, nx, ny, nz, x0_geom,
                                               y0_geom, z0_geom, dx, dy, dz,
                                               pred_grid, err, modelPath);

  const auto t_end = std::chrono::high_resolution_clock::now();
  const double elapsed_ms =
      std::chrono::duration<double, std::milli>(t_end - t_start).count();

  if (!ok) {
    m_progressBar->setValue(0);
    m_lblStatus->setText(
        tr("Neural inference failed: %1").arg(QString::fromStdString(err)));
    appendLog(tr("==> Failed: %1").arg(QString::fromStdString(err)));
    QMessageBox::critical(this, tr("AI Inference Error"),
                          QString::fromStdString(err));
    return;
  }

  inv::InversionConfig cfg;
  cfg.nx = nx;
  cfg.ny = ny;
  cfg.nz = nz;
  cfg.dx = dx;
  cfg.dy = dy;
  cfg.dz = dz;
  cfg.x0 = x0_geom;
  cfg.y0 = y0_geom;
  cfg.z0 = z0_geom;
  const double rawObsH = m_doc ? m_doc->observation().gravity_height().value()
                               : m_spinObsHeight->value();
  cfg.obs_height = (m_doc->observation().gravity_mode() ==
                        mod3d::ObservationMode::SensorHeight &&
                    rawObsH <= 0.0)
                       ? 0.1
                       : rawObsH;
  cfg.use_fft_solver_benchmark = (m_comboOperator->currentIndex() == 1);

  inv::InversionResult result;
  result.density_grid = std::move(pred_grid);
  result.stats.elapsed_time_ms = elapsed_ms;
  result.stats.levels_completed = 1;
  result.stats.initial_data_rms = init_rms;
  result.stats.used_neural_transformer = true;
  result.stats.message = "Instant 3D Swin Transformer Inference";

  // Compute forward response using pfld::InterfaceMesh with relief & background
  // alignment
  inv::GridGeometry g{nx, ny, nz, x0_geom, y0_geom, z0_geom, dx, dy, dz};
  const double zDomainTop = m_doc->model().z_max().value();
  const auto &relief = m_doc->observation().surface_relief();
  const bool hasRelief =
      !relief.empty() && relief.rows() >= 2 && relief.cols() >= 2;
  std::vector<double> topo_nodes((nx + 1) * (ny + 1), 0.0);
  for (size_t iy = 0; iy <= ny; ++iy) {
    const double y_c = y0_geom + static_cast<double>(iy) * dy;
    for (size_t ix = 0; ix <= nx; ++ix) {
      const double x_c = x0_geom + static_cast<double>(ix) * dx;
      double elev = 0.0;
      if (hasRelief) {
        elev = relief.sample(x_c, y_c);
        if (elev == mod3d::GRID_DUMMY || std::isnan(elev))
          elev = relief.mean();
      }
      topo_nodes[iy * (nx + 1) + ix] = std::max(0.0, zDomainTop - elev);
    }
  }

  std::vector<char> active(g.cells(), 1);
  std::vector<double> col_surface(g.columns(), z0_geom);
  double zmin = 1e300, zmax = -1e300;
  for (std::size_t ix = 0; ix < g.nx; ++ix) {
    for (std::size_t iy = 0; iy < g.ny; ++iy) {
      const double zs =
          inv::column_surface_z(topo_nodes, g.nx, g.ny, ix, iy, z0_geom);
      col_surface[ix * g.ny + iy] = zs;
      zmin = std::min(zmin, zs);
      zmax = std::max(zmax, zs);
      for (std::size_t iz = 0; iz < g.nz; ++iz) {
        if (g.zc(iz) < zs) {
          active[g.cell_index(ix, iy, iz)] = 0;
          result.density_grid.data()[g.cell_index(ix, iy, iz)] = 0.0;
        }
      }
    }
  }
  const bool flat = (zmax - zmin) < 1e-6;

  std::unique_ptr<inv::LinearOperator> op;
  if (cfg.use_fft_solver_benchmark && flat) {
    op = std::make_unique<inv::ToeplitzGravityOperator>(g,
                                                        zmin - cfg.obs_height);
  } else {
    std::vector<inv::Point3D<double>> obs;
    obs.reserve(g.columns());
    for (std::size_t ix = 0; ix < g.nx; ++ix) {
      for (std::size_t iy = 0; iy < g.ny; ++iy) {
        obs.emplace_back(g.xc(ix), g.yc(iy),
                         col_surface[ix * g.ny + iy] - cfg.obs_height);
      }
    }
    op = std::make_unique<inv::PfldInterfaceMeshOperator>(
        g, obs, topo_nodes, cfg.dense_max_entries);
  }

  if (op) {
    op->set_active(active);
    result.modeled_gz.resize(nx * ny, 0.0);
    op->apply(result.density_grid.data(), result.modeled_gz);

    double fit2 = 0.0;
    for (size_t i = 0; i < nx * ny; ++i) {
      double diff = result.modeled_gz[i] - d_fine[i];
      fit2 += diff * diff;
    }
    result.stats.final_data_rms = std::sqrt(fit2 / (nx * ny));
    result.stats.final_chi_rms =
        (init_rms > 1e-9) ? (result.stats.final_data_rms / init_rms) : 1.0;
  }

  result.stats.minimum_support =
      (result.density_grid.total_cells() > 0)
          ? (inv::FocusingEvaluator<double>::compute_minimum_support(
                 result.density_grid, 1e-4) /
             static_cast<double>(result.density_grid.total_cells()))
          : 0.0;
  result.stats.minimum_gradient_support =
      (result.density_grid.total_cells() > 0)
          ? (inv::FocusingEvaluator<double>::compute_minimum_gradient_support(
                 result.density_grid, 1e-4) /
             static_cast<double>(result.density_grid.total_cells()))
          : 0.0;

  m_currentResult = std::move(result);
  updateRefinementState();
  m_progressBar->setValue(100);
  m_btnApply->setEnabled(true);
  m_btnExportVtk->setEnabled(true);
  m_btnExportJson->setEnabled(true);

  m_lblStatus->setText(
      tr("AI Inference completed in %1 ms.").arg(elapsed_ms, 0, 'f', 1));
  const double minDensKg =
      units::GPerCm3(m_currentResult->density_grid.min_value())
          .in<units::KgPerM3>();
  const double maxDensKg =
      units::GPerCm3(m_currentResult->density_grid.max_value())
          .in<units::KgPerM3>();
  const QString densUnit =
      QString::fromUtf8(units::KgPerM3::unit_symbol().data());

  m_lblCurrentStats->setText(
      tr("Instant AI | Time: %1 ms | Data RMS: %2 mGal -> %3 mGal | Density: "
         "[%4, %5] %6")
          .arg(elapsed_ms, 0, 'f', 1)
          .arg(m_currentResult->stats.initial_data_rms, 0, 'f', 3)
          .arg(m_currentResult->stats.final_data_rms, 0, 'f', 3)
          .arg(minDensKg, 0, 'f', 1)
          .arg(maxDensKg, 0, 'f', 1)
          .arg(densUnit));

  appendLog(tr("==> AI Inference Finished Successfully in %1 ms!")
                .arg(elapsed_ms, 0, 'f', 1));
  appendLog(tr("    Model: 3D Swin Transformer (4-stage hierarchical "
               "shifted-window cross-attention)"));
  appendLog(tr("    Density Range: [%1, %2] %3")
                .arg(minDensKg, 0, 'f', 1)
                .arg(maxDensKg, 0, 'f', 1)
                .arg(densUnit));
  appendLog(tr("    Observed Data RMS: %1 mGal -> Modeled RMS: %2 mGal")
                .arg(m_currentResult->stats.initial_data_rms, 0, 'f', 4)
                .arg(m_currentResult->stats.final_data_rms, 0, 'f', 4));
  appendLog(tr("    Click 'Apply to Document' to inspect 3D density in "
               "Profile, Horizontal Slice, and 3D views."));
}

Inversion3DDialog::DensityBottleneckReport
Inversion3DDialog::checkDensityBottleneck(bool applyBounds) {
  DensityBottleneckReport report;
  if (!m_doc) {
    report.message = tr("No active document available.");
    return report;
  }

  const mod3d::Grid *obsGrid = m_doc->activeObservedGrid();
  if (!obsGrid || obsGrid->empty()) {
    report.message = tr("No active observed gravity grid found.");
    return report;
  }

  // 1. Analyze observed anomaly raster
  double obsMin = 1e30, obsMax = -1e30;
  size_t validCount = 0;
  for (size_t r = 0; r < obsGrid->rows(); ++r) {
    for (size_t c = 0; c < obsGrid->cols(); ++c) {
      double v = (*obsGrid)(r, c);
      if (!mod3d::Grid::is_dummy_value(v) && !std::isnan(v)) {
        obsMin = std::min(obsMin, v);
        obsMax = std::max(obsMax, v);
        validCount++;
      }
    }
  }
  if (validCount == 0 || obsMin > obsMax) {
    report.message =
        tr("Observed gravity grid contains no valid numerical values.");
    return report;
  }

  const double obsSpan = obsMax - obsMin;
  report.observed_min_mgal = obsMin;
  report.observed_max_mgal = obsMax;
  report.observed_span_mgal = obsSpan;

  // 2. Analyze domain geometry and 3D voxel attenuation
  const double zDomainTop = m_doc->model().z_max().value();
  const double zMin = m_doc->model().z_min().value();
  const double H_total = std::max(10.0, zDomainTop - zMin);
  const double dx = std::abs(obsGrid->dx().value());
  const double dy = std::abs(obsGrid->dy().value());
  const double Lx = static_cast<double>(obsGrid->cols()) * dx;
  const double Ly = static_cast<double>(obsGrid->rows()) * dy;

  // Effective thickness below mean terrain if surface relief exists
  double meanElev = zDomainTop;
  const auto &relief = m_doc->observation().surface_relief();
  if (!relief.empty() && relief.rows() >= 2 && relief.cols() >= 2) {
    meanElev = relief.mean();
  }
  const double H_eff = std::clamp(meanElev - zMin, 10.0, H_total);
  report.domain_thickness_m = H_eff;

  // 3D Geometric factor for finite prism vs infinite Bouguer slab:
  // Radius R = half-width of survey footprint.
  // For a cylinder/prism of radius R and thickness H, on-axis surface gravity
  // is: Delta_g = 2*pi*G*rho*(H + R - sqrt(H^2 + R^2)) = 2*pi*G*rho*H * eta
  const double R = 0.5 * std::min(Lx, Ly);
  const double eta =
      (R > 0.0)
          ? std::clamp((H_eff + R - std::sqrt(H_eff * H_eff + R * R)) / H_eff,
                       0.15, 0.95)
          : 0.5;

  // In focusing inversion, compact structures occupy an effective volume
  // fraction V_frac ~ 0.40 - 0.50 twopiG = 2 * pi * G = 4.1934e-5 mGal /
  // (kg/m^3 * m)
  constexpr double TWO_PI_G = 4.1934e-5;
  constexpr double V_FRAC = 0.40;
  const double G_eff = TWO_PI_G * H_eff * eta * V_FRAC; // mGal per (kg/m^3)

  // Current bounds in kg/m³
  const double curMinKg = m_spinMinDensity->value();
  const double curMaxKg = m_spinMaxDensity->value();
  report.current_min_density_kg = curMinKg;
  report.current_max_density_kg = curMaxKg;

  // Maximum field achievable with current bounds
  report.max_achievable_pos_mgal = std::max(0.0, curMaxKg) * G_eff;
  report.max_achievable_neg_mgal = std::min(0.0, curMinKg) * G_eff;

  // 3. Inspect previous inversion result for boundary saturation
  const inv::InversionResult *prevRes =
      m_currentResult.has_value()
          ? &(*m_currentResult)
          : (m_doc->hasInversionResult() ? &(*m_doc->inversionResult())
                                         : nullptr);

  double prevMinKg = 0.0;
  double prevMaxKg = 0.0;
  if (prevRes && prevRes->density_grid.total_cells() > 0) {
    const auto &data = prevRes->density_grid.data();
    const double prevMinG = prevRes->density_grid.min_value();
    const double prevMaxG = prevRes->density_grid.max_value();
    prevMinKg = units::GPerCm3(prevMinG).in<units::KgPerM3>();
    prevMaxKg = units::GPerCm3(prevMaxG).in<units::KgPerM3>();
    const double tolMin = std::max(1e-4, std::abs(prevMinG) * 0.015);
    const double tolMax = std::max(1e-4, std::abs(prevMaxG) * 0.015);
    size_t countMin = 0, countMax = 0, activeCount = 0;
    for (double v : data) {
      if (std::abs(v) > 1e-12) {
        activeCount++;
        if (v <= prevMinG + tolMin)
          countMin++;
        if (v >= prevMaxG - tolMax)
          countMax++;
      }
    }
    if (activeCount > 0) {
      report.saturated_min_percent = 100.0 * static_cast<double>(countMin) /
                                     static_cast<double>(activeCount);
      report.saturated_max_percent = 100.0 * static_cast<double>(countMax) /
                                     static_cast<double>(activeCount);
    }
  }

  // 4. Target gravity requirements
  const double targetPos = std::max(0.0, obsMax);
  const double targetNeg = std::min(0.0, obsMin);

  // Bottleneck conditions:
  // (A) Current achievable field falls short of the target by > 10%
  // (B) Previous inversion had > 5% of active cells saturated against the
  // boundary AND current bounds have not been enlarged past that saturated
  // value
  if (targetPos > 0.05 &&
      (report.max_achievable_pos_mgal < 0.90 * targetPos ||
       (report.saturated_max_percent > 5.0 && curMaxKg <= prevMaxKg + 10.0))) {
    report.max_bottleneck = true;
  }
  if (targetNeg < -0.05 &&
      (report.max_achievable_neg_mgal > 0.90 * targetNeg ||
       (report.saturated_min_percent > 5.0 && curMinKg >= prevMinKg - 10.0))) {
    report.min_bottleneck = true;
  }
  report.has_bottleneck = report.max_bottleneck || report.min_bottleneck;

  // 5. Compute reasonable recommended bounds with safety factor F = 1.5
  constexpr double SAFETY_F = 1.5;
  double recMaxKg = 300.0;
  if (targetPos > 0.02) {
    recMaxKg = (SAFETY_F * targetPos) / G_eff;
  }
  double recMinKg = -300.0;
  if (targetNeg < -0.02) {
    recMinKg = (SAFETY_F * targetNeg) / G_eff;
  }

  // Round outward to clean multiples of 50 kg/m³
  recMaxKg = std::max(300.0, std::ceil(recMaxKg / 50.0) * 50.0);
  recMinKg = std::min(-300.0, std::floor(recMinKg / 50.0) * 50.0);

  report.recommended_min_density_kg = recMinKg;
  report.recommended_max_density_kg = recMaxKg;

  // 6. Apply if requested
  if (applyBounds) {
    m_spinMinDensity->blockSignals(true);
    m_spinMinDensity->setValue(recMinKg);
    m_spinMinDensity->blockSignals(false);

    m_spinMaxDensity->blockSignals(true);
    m_spinMaxDensity->setValue(recMaxKg);
    m_spinMaxDensity->blockSignals(false);

    saveSettingsToDocument();
  }

  // 7. Format summary text
  QStringList lines;
  if (report.has_bottleneck) {
    lines << tr("=== DENSITY BOUND BOTTLENECK DETECTED ===");
  } else {
    lines << tr("=== DENSITY BOUNDS CHECK: SUFFICIENT ===");
  }

  lines
      << tr("• Observed Anomaly: min = %1 mGal, max = %2 mGal (Span: %3 mGal)")
             .arg(obsMin, 0, 'f', 2)
             .arg(obsMax, 0, 'f', 2)
             .arg(obsSpan, 0, 'f', 2);
  lines << tr("• Model Domain: effective thickness H = %1 m, footprint = %2 × "
              "%3 m, 3D geometric factor η = %4")
               .arg(H_eff, 0, 'f', 1)
               .arg(Lx, 0, 'f', 0)
               .arg(Ly, 0, 'f', 0)
               .arg(eta, 0, 'f', 2);
  lines << tr("• Current Bounds: [%1, %2] kg/m³")
               .arg(curMinKg, 0, 'f', 0)
               .arg(curMaxKg, 0, 'f', 0);
  lines << tr("  - Max achievable gravity with current bounds: [%1, %2] mGal")
               .arg(report.max_achievable_neg_mgal, 0, 'f', 2)
               .arg(report.max_achievable_pos_mgal, 0, 'f', 2);

  if (report.saturated_max_percent > 0.0 ||
      report.saturated_min_percent > 0.0) {
    lines << tr("• Previous Inversion Saturation:");
    if (report.saturated_max_percent > 0.0) {
      lines << tr("  - Upper bound: %1% of active cells pegged at max density!")
                   .arg(report.saturated_max_percent, 0, 'f', 1);
    }
    if (report.saturated_min_percent > 0.0) {
      lines << tr("  - Lower bound: %1% of active cells pegged at min density!")
                   .arg(report.saturated_min_percent, 0, 'f', 1);
    }
  }

  if (report.has_bottleneck) {
    if (report.max_bottleneck) {
      lines << tr("⚠ Upper bound (+%1 kg/m³) is too narrow to reproduce the "
                  "+%2 mGal positive anomaly peak without causing a downward "
                  "DC shift!")
                   .arg(curMaxKg, 0, 'f', 0)
                   .arg(targetPos, 0, 'f', 2);
    }
    if (report.min_bottleneck) {
      lines << tr("⚠ Lower bound (%1 kg/m³) is too narrow to reproduce the %2 "
                  "mGal negative anomaly trough!")
                   .arg(curMinKg, 0, 'f', 0)
                   .arg(targetNeg, 0, 'f', 2);
    }
  }

  lines << tr("✔ Recommended Bounds: [%1, %2] kg/m³")
               .arg(recMinKg, 0, 'f', 0)
               .arg(recMaxKg, 0, 'f', 0);
  if (applyBounds) {
    lines << tr(
        "  (Bounds automatically updated in dialog and document settings)");
  }

  report.message = lines.join("\n");
  return report;
}

void Inversion3DDialog::onCheckDensityBottleneckClicked() {
  auto report = checkDensityBottleneck(/*applyBounds=*/true);
  appendLog(report.message);

  if (report.has_bottleneck) {
    QMessageBox::warning(this, tr("Density Bottleneck Detected & Fixed"),
                         report.message);
  } else {
    QMessageBox::information(this, tr("Density Bounds Check"), report.message);
  }
}

} // namespace mod3d_gui
