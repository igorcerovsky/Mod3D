#include "mod3d_gui/dialogs/FitNeighborhoodDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QMessageBox>

namespace mod3d_gui {

FitNeighborhoodDialog::FitNeighborhoodDialog(ModelDocument *doc, int lineIdx,
                                             int vertexIdx, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
    , m_lineIdx(lineIdx)
    , m_vertexIdx(vertexIdx)
{
    setWindowTitle(tr("Localized Neighborhood Inversion Properties"));
    setMinimumWidth(440);

    auto *mainLayout = new QVBoxLayout(this);

    // Footprint / Radius group box
    auto *grpFootprint = new QGroupBox(tr("Neighborhood Footprint (Diameter / Radius)"), this);
    auto *vboxFootprint = new QVBoxLayout(grpFootprint);

    m_rbAutoRadius = new QRadioButton(
        tr("Automatic (Expands until facet sensitivity decays below threshold)"), grpFootprint);
    m_rbCustomRadius = new QRadioButton(
        tr("Custom Diameter / Radius (Fixed circular footprint)"), grpFootprint);

    vboxFootprint->addWidget(m_rbAutoRadius);

    auto *formAuto = new QFormLayout();
    formAuto->setContentsMargins(20, 0, 0, 8);
    m_spinCutoff = new QDoubleSpinBox(grpFootprint);
    m_spinCutoff->setRange(0.1, 50.0);
    m_spinCutoff->setSingleStep(0.5);
    m_spinCutoff->setDecimals(1);
    m_spinCutoff->setValue(2.0);
    m_spinCutoff->setSuffix(tr(" %"));
    m_spinCutoff->setToolTip(tr("Stop expanding circle when candidate facet response falls below this fraction of peak seed response."));
    formAuto->addRow(tr("Sensitivity Cutoff:"), m_spinCutoff);
    vboxFootprint->addLayout(formAuto);

    vboxFootprint->addWidget(m_rbCustomRadius);

    auto *formCustom = new QFormLayout();
    formCustom->setContentsMargins(20, 0, 0, 0);
    m_spinCustomRadius = new QDoubleSpinBox(grpFootprint);
    m_spinCustomRadius->setRange(10.0, 50000.0);
    m_spinCustomRadius->setSingleStep(50.0);
    m_spinCustomRadius->setDecimals(1);
    m_spinCustomRadius->setValue(1000.0);
    m_spinCustomRadius->setSuffix(tr(" m"));
    m_spinCustomRadius->setToolTip(tr("Search radius from seed vertex in horizontal meters (diameter = 2 * radius)."));
    formCustom->addRow(tr("Search Radius:"), m_spinCustomRadius);
    vboxFootprint->addLayout(formCustom);

    mainLayout->addWidget(grpFootprint);

    connect(m_rbAutoRadius, &QRadioButton::toggled, this, &FitNeighborhoodDialog::onRadiusModeChanged);
    connect(m_rbCustomRadius, &QRadioButton::toggled, this, &FitNeighborhoodDialog::onRadiusModeChanged);

    // Regularization and Solver group box
    auto *grpSolver = new QGroupBox(tr("Multi-Parameter Solver & Regularization"), this);
    auto *formSolver = new QFormLayout(grpSolver);

    m_spinSmoothness = new QDoubleSpinBox(grpSolver);
    m_spinSmoothness->setRange(0.0, 10.0);
    m_spinSmoothness->setSingleStep(0.01);
    m_spinSmoothness->setDecimals(4);
    m_spinSmoothness->setValue(0.05);
    m_spinSmoothness->setToolTip(tr("Surface graph Laplacian regularization penalty weight to preserve smooth curvature."));
    formSolver->addRow(tr("Smoothness Weight (α):"), m_spinSmoothness);

    m_spinTolerance = new QDoubleSpinBox(grpSolver);
    m_spinTolerance->setRange(1e-8, 1e-2);
    m_spinTolerance->setDecimals(6);
    m_spinTolerance->setValue(1e-4);
    formSolver->addRow(tr("Convergence Tolerance:"), m_spinTolerance);

    m_spinMaxIter = new QSpinBox(grpSolver);
    m_spinMaxIter->setRange(1, 100);
    m_spinMaxIter->setValue(15);
    formSolver->addRow(tr("Max LM Iterations:"), m_spinMaxIter);

    mainLayout->addWidget(grpSolver);

    // Target vertex action section if invoked from specific vertex
    if (m_doc && m_lineIdx >= 0 && m_vertexIdx >= 0) {
        auto *grpAction = new QGroupBox(tr("Selected Target Vertex Action"), this);
        auto *vboxAction = new QVBoxLayout(grpAction);

        // Seed coordinate info
        auto *lblInfo = new QLabel(
            tr("Target Seed: Profile Line %1, Vertex Index %2").arg(m_lineIdx).arg(m_vertexIdx),
            grpAction);
        lblInfo->setStyleSheet("font-weight: bold;");
        vboxAction->addWidget(lblInfo);

        m_btnFitNow = new QPushButton(tr("⚡ Fit Neighborhood Now"), grpAction);
        m_btnFitNow->setStyleSheet("font-weight: bold; padding: 6px;");
        connect(m_btnFitNow, &QPushButton::clicked, this, &FitNeighborhoodDialog::onFitNowClicked);
        vboxAction->addWidget(m_btnFitNow);

        m_lblStatus = new QLabel(tr("Ready to fit neighborhood."), grpAction);
        m_lblStatus->setWordWrap(true);
        m_lblStatus->setStyleSheet("color: #666;");
        vboxAction->addWidget(m_lblStatus);

        mainLayout->addWidget(grpAction);
    }

    loadSettingsToUI();
    onRadiusModeChanged();

    auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(btnBox, &QDialogButtonBox::accepted, this, [this]() {
        applySettingsFromUI();
        accept();
    });
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    mainLayout->addWidget(btnBox);
}

void FitNeighborhoodDialog::onRadiusModeChanged() {
    bool isCustom = m_rbCustomRadius->isChecked();
    m_spinCustomRadius->setEnabled(isCustom);
    m_spinCutoff->setEnabled(!isCustom);
}

void FitNeighborhoodDialog::loadSettingsToUI() {
    if (!m_doc) return;
    const auto &st = m_doc->observation().neighborhood_fit_settings();
    if (st.radius_mode == mod3d::NeighborhoodRadiusMode::Custom) {
        m_rbCustomRadius->setChecked(true);
    } else {
        m_rbAutoRadius->setChecked(true);
    }
    m_spinCustomRadius->setValue(st.custom_radius_m);
    m_spinCutoff->setValue(st.sensitivity_cutoff * 100.0);
    m_spinSmoothness->setValue(st.smoothness_weight);
    m_spinTolerance->setValue(st.tol);
    m_spinMaxIter->setValue(st.max_iter);
}

void FitNeighborhoodDialog::applySettingsFromUI() {
    if (!m_doc) return;
    mod3d::NeighborhoodFitSettings st;
    st.radius_mode = m_rbCustomRadius->isChecked()
                         ? mod3d::NeighborhoodRadiusMode::Custom
                         : mod3d::NeighborhoodRadiusMode::Automatic;
    st.custom_radius_m = m_spinCustomRadius->value();
    st.sensitivity_cutoff = m_spinCutoff->value() / 100.0;
    st.smoothness_weight = m_spinSmoothness->value();
    st.tol = m_spinTolerance->value();
    st.max_iter = m_spinMaxIter->value();
    m_doc->observation().set_neighborhood_fit_settings(st);
}

void FitNeighborhoodDialog::onFitNowClicked() {
    if (!m_doc || m_lineIdx < 0 || m_vertexIdx < 0) return;
    applySettingsFromUI();

    int row = 0, col = 0;
    int nCols = static_cast<int>(m_doc->model().cols());
    int nRows = static_cast<int>(m_doc->model().rows());

    // Profile orientation coordinate mapping
    // Default fallback: treat lineIdx as East-West (row) or North-South (col)
    // If lineIdx <= nRows, row = lineIdx; else row = 1
    if (m_lineIdx < nRows) {
        row = m_lineIdx;
        col = nCols / 2;
    } else {
        row = nRows / 2;
        col = std::min(m_lineIdx, nCols - 1);
    }

    auto res = m_doc->observation().fit_neighborhood(
        m_doc->model(), row, col, m_vertexIdx, m_doc->activeField(),
        m_doc->observation().neighborhood_fit_settings());

    m_doc->setModified(true);
    m_doc->computeAll();

    if (m_lblStatus) {
        if (res.converged || res.final_rms < res.initial_rms) {
            double red = (res.initial_rms > 1e-12)
                             ? ((res.initial_rms - res.final_rms) / res.initial_rms * 100.0)
                             : 0.0;
            m_lblStatus->setText(
                tr("✅ Inverted %1 vertices (Radius: %2 m)\n"
                   "Initial RMS: %3, Final RMS: %4 (-%5%)\n"
                   "Completed in %6 iterations (%7s)")
                    .arg(res.vertices_fitted)
                    .arg(res.chosen_radius_m, 0, 'f', 1)
                    .arg(res.initial_rms, 0, 'f', 5)
                    .arg(res.final_rms, 0, 'f', 5)
                    .arg(red, 0, 'f', 1)
                    .arg(res.iterations)
                    .arg(res.elapsed_time_sec, 0, 'f', 3));
            m_lblStatus->setStyleSheet("color: #007700; font-weight: bold;");
        } else {
            m_lblStatus->setText(tr("⚠️ Inversion finished without RMS improvement."));
            m_lblStatus->setStyleSheet("color: #cc6600;");
        }
    }
}

} // namespace mod3d_gui
