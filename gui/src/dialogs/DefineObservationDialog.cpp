#include "mod3d_gui/dialogs/DefineObservationDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QSignalBlocker>
#include <cmath>
#include <algorithm>

namespace mod3d_gui {

DefineObservationDialog::DefineObservationDialog(ModelDocument *doc, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
{
    setWindowTitle(tr("Define Observation Area"));
    setMinimumWidth(560);

    auto *mainLayout = new QVBoxLayout(this);

    // 1. Creation Mode Group (mirrors legacy CDlgStart)
    auto *modeGroup = new QGroupBox(tr("New Model Creation Mode"), this);
    auto *modeLayout = new QVBoxLayout(modeGroup);
    m_radioDefineObs = new QRadioButton(tr("Define observation area (custom dimensions or from grid geometry)"), modeGroup);
    m_radioImportRelief = new QRadioButton(tr("Import observation area (topography and/or measured field from grid)"), modeGroup);
    m_radioDefineObs->setChecked(true);
    modeLayout->addWidget(m_radioDefineObs);
    modeLayout->addWidget(m_radioImportRelief);
    mainLayout->addWidget(modeGroup);

    // 2. Dimensions Group (mirrors legacy CDlgDefineObs 3-column x / y / z table)
    auto *dimGroup = new QGroupBox(tr("Observation Area & Vertical Range"), this);
    auto *grid = new QGridLayout(dimGroup);

    auto *lblX = new QLabel(tr("<b>x [m]</b>"), dimGroup);
    auto *lblY = new QLabel(tr("<b>y [m]</b>"), dimGroup);
    auto *lblZ = new QLabel(tr("<b>z [m]</b>"), dimGroup);
    lblX->setAlignment(Qt::AlignCenter);
    lblY->setAlignment(Qt::AlignCenter);
    lblZ->setAlignment(Qt::AlignCenter);
    grid->addWidget(lblX, 0, 1);
    grid->addWidget(lblY, 0, 2);
    grid->addWidget(lblZ, 0, 3);

    const auto &m = m_doc->model();
    const int initRows = (m.rows() > 2) ? (m.rows() - 2) : 21;
    const int initCols = (m.cols() > 2) ? (m.cols() - 2) : 21;
    const double initX0 = m.x0_val();
    const double initY0 = m.y0_val();
    const double initDX = (m.dx_val() > 0.0) ? m.dx_val() : 500.0;
    const double initDY = (m.dy_val() > 0.0) ? m.dy_val() : 500.0;
    const double initXMax = initX0 + (initCols - 1) * initDX;
    const double initYMax = initY0 + (initRows - 1) * initDY;
    const double initZMin = (m.z_min_val() != 0.0 || m.z_max_val() != 0.0) ? m.z_min_val() : -4000.0;
    const double initZMax = (m.z_min_val() != 0.0 || m.z_max_val() != 0.0) ? m.z_max_val() : 1000.0;

    auto makeCoordSpin = [dimGroup](double minVal, double maxVal, double val) {
        auto *sb = new QDoubleSpinBox(dimGroup);
        sb->setRange(minVal, maxVal);
        sb->setDecimals(2);
        sb->setValue(val);
        return sb;
    };

    // Row 1: Minimum
    grid->addWidget(new QLabel(tr("Minimum:"), dimGroup), 1, 0);
    m_spinX0 = makeCoordSpin(-1e7, 1e7, initX0);
    m_spinY0 = makeCoordSpin(-1e7, 1e7, initY0);
    m_spinZMin = makeCoordSpin(-1e6, 1e6, initZMin);
    grid->addWidget(m_spinX0, 1, 1);
    grid->addWidget(m_spinY0, 1, 2);
    grid->addWidget(m_spinZMin, 1, 3);

    // Row 2: Maximum
    grid->addWidget(new QLabel(tr("Maximum:"), dimGroup), 2, 0);
    m_spinXMax = makeCoordSpin(-1e7, 1e7, initXMax);
    m_spinYMax = makeCoordSpin(-1e7, 1e7, initYMax);
    m_spinZMax = makeCoordSpin(-1e6, 1e6, initZMax);
    grid->addWidget(m_spinXMax, 2, 1);
    grid->addWidget(m_spinYMax, 2, 2);
    grid->addWidget(m_spinZMax, 2, 3);

    // Row 3: Increment
    grid->addWidget(new QLabel(tr("Increment:"), dimGroup), 3, 0);
    m_spinDX = makeCoordSpin(0.01, 1e6, initDX);
    m_spinDY = makeCoordSpin(0.01, 1e6, initDY);
    grid->addWidget(m_spinDX, 3, 1);
    grid->addWidget(m_spinDY, 3, 2);

    // Row 4: Grid Nodes (Cols / Rows)
    grid->addWidget(new QLabel(tr("Grid Nodes:"), dimGroup), 4, 0);
    m_spinCols = new QSpinBox(dimGroup);
    m_spinCols->setRange(2, 2000);
    m_spinCols->setValue(initCols);
    m_spinCols->setPrefix(tr("Cols: "));
    m_spinRows = new QSpinBox(dimGroup);
    m_spinRows->setRange(2, 2000);
    m_spinRows->setValue(initRows);
    m_spinRows->setPrefix(tr("Rows: "));
    grid->addWidget(m_spinCols, 4, 1);
    grid->addWidget(m_spinRows, 4, 2);

    mainLayout->addWidget(dimGroup);

    // 3. Import Grid Files Group (Topography and Measured Field)
    auto *gridGroup = new QGroupBox(tr("Import Grid Files (Topography & Measured Field)"), this);
    auto *gridGroupLayout = new QGridLayout(gridGroup);

    // Row 0: Topography Grid
    m_btnGetTopoGrid = new QPushButton(tr("Get Topography From Grid..."), gridGroup);
    m_editTopoGridFile = new QLineEdit(gridGroup);
    m_editTopoGridFile->setReadOnly(true);
    m_editTopoGridFile->setPlaceholderText(tr("No topography grid file selected"));
    m_btnClearTopoGrid = new QPushButton(tr("Clear"), gridGroup);
    m_btnClearTopoGrid->setEnabled(false);

    gridGroupLayout->addWidget(m_btnGetTopoGrid, 0, 0);
    gridGroupLayout->addWidget(m_editTopoGridFile, 0, 1);
    gridGroupLayout->addWidget(m_btnClearTopoGrid, 0, 2);

    // Row 1: Measured Field Grid
    m_btnGetFieldGrid = new QPushButton(tr("Get Field From Grid..."), gridGroup);
    m_editFieldGridFile = new QLineEdit(gridGroup);
    m_editFieldGridFile->setReadOnly(true);
    m_editFieldGridFile->setPlaceholderText(tr("No measured field grid file selected"));

    m_comboFieldComp = new QComboBox(gridGroup);
    m_comboFieldComp->addItem(tr("Gz - Gravity Vertical (mGal)"), static_cast<int>(mod3d::FieldComponent::GZ));
    m_comboFieldComp->addItem(tr("ΔT - Total Magnetic Field (nT)"), static_cast<int>(mod3d::FieldComponent::DELTA_T));
    m_comboFieldComp->addItem(tr("Gx - Gravity East (mGal)"), static_cast<int>(mod3d::FieldComponent::GX));
    m_comboFieldComp->addItem(tr("Gy - Gravity North (mGal)"), static_cast<int>(mod3d::FieldComponent::GY));
    m_comboFieldComp->addItem(tr("Gxx - Gravity Gradient (E)"), static_cast<int>(mod3d::FieldComponent::GXX));
    m_comboFieldComp->addItem(tr("Gxy - Gravity Gradient (E)"), static_cast<int>(mod3d::FieldComponent::GXY));
    m_comboFieldComp->addItem(tr("Gxz - Gravity Gradient (E)"), static_cast<int>(mod3d::FieldComponent::GXZ));
    m_comboFieldComp->addItem(tr("Gyy - Gravity Gradient (E)"), static_cast<int>(mod3d::FieldComponent::GYY));
    m_comboFieldComp->addItem(tr("Gyz - Gravity Gradient (E)"), static_cast<int>(mod3d::FieldComponent::GYZ));
    m_comboFieldComp->addItem(tr("Gzz - Gravity Gradient (E)"), static_cast<int>(mod3d::FieldComponent::GZZ));
    m_comboFieldComp->addItem(tr("Mx - Magnetic East (nT)"), static_cast<int>(mod3d::FieldComponent::MX));
    m_comboFieldComp->addItem(tr("My - Magnetic North (nT)"), static_cast<int>(mod3d::FieldComponent::MY));
    m_comboFieldComp->addItem(tr("Mz - Magnetic Vertical (nT)"), static_cast<int>(mod3d::FieldComponent::MZ));

    m_btnClearFieldGrid = new QPushButton(tr("Clear"), gridGroup);
    m_btnClearFieldGrid->setEnabled(false);

    auto *fieldSubLayout = new QHBoxLayout();
    fieldSubLayout->addWidget(m_editFieldGridFile, 1);
    fieldSubLayout->addWidget(m_comboFieldComp);

    gridGroupLayout->addWidget(m_btnGetFieldGrid, 1, 0);
    gridGroupLayout->addLayout(fieldSubLayout, 1, 1);
    gridGroupLayout->addWidget(m_btnClearFieldGrid, 1, 2);

    mainLayout->addWidget(gridGroup);

    // 4. Grid & Observation Area Statistics Group
    auto *statsGroup = new QGroupBox(tr("Grid && Area Statistics"), this);
    auto *statsGrid = new QGridLayout(statsGroup);

    m_lblLengthX = new QLabel(statsGroup);
    m_lblLengthY = new QLabel(statsGroup);
    m_lblGridMinZTitle = new QLabel(tr("<b>Topography Min Z:</b>"), statsGroup);
    m_lblGridMinZ = new QLabel(statsGroup);
    m_lblGridMaxZTitle = new QLabel(tr("<b>Topography Max Z:</b>"), statsGroup);
    m_lblGridMaxZ = new QLabel(statsGroup);

    statsGrid->addWidget(new QLabel(tr("<b>Length X:</b>"), statsGroup), 0, 0);
    statsGrid->addWidget(m_lblLengthX, 0, 1);
    statsGrid->addWidget(new QLabel(tr("<b>Length Y:</b>"), statsGroup), 0, 2);
    statsGrid->addWidget(m_lblLengthY, 0, 3);

    statsGrid->addWidget(m_lblGridMinZTitle, 1, 0);
    statsGrid->addWidget(m_lblGridMinZ, 1, 1);
    statsGrid->addWidget(m_lblGridMaxZTitle, 1, 2);
    statsGrid->addWidget(m_lblGridMaxZ, 1, 3);

    mainLayout->addWidget(statsGroup);

    // Connections for synchronized Min / Max / Increment / Nodes
    connect(m_spinX0, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this]() { syncColsFromXBounds(); });
    connect(m_spinXMax, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this]() { syncColsFromXBounds(); });
    connect(m_spinDX, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this]() { syncColsFromXBounds(); });
    connect(m_spinCols, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { syncXMaxFromCols(); });

    connect(m_spinY0, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this]() { syncRowsFromYBounds(); });
    connect(m_spinYMax, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this]() { syncRowsFromYBounds(); });
    connect(m_spinDY, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this]() { syncRowsFromYBounds(); });
    connect(m_spinRows, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { syncYMaxFromRows(); });

    connect(m_radioDefineObs, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) setCreationMode(CreationMode::DefineObservation);
    });
    connect(m_radioImportRelief, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) setCreationMode(CreationMode::ImportReliefGrid);
    });

    connect(m_btnGetTopoGrid, &QPushButton::clicked, this, &DefineObservationDialog::onBrowseTopoGridFile);
    connect(m_btnClearTopoGrid, &QPushButton::clicked, this, &DefineObservationDialog::clearTopoGrid);
    connect(m_btnGetFieldGrid, &QPushButton::clicked, this, &DefineObservationDialog::onBrowseFieldGridFile);
    connect(m_btnClearFieldGrid, &QPushButton::clicked, this, &DefineObservationDialog::clearFieldGrid);

    // 5. Dialog Buttons
    auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(btnBox, &QDialogButtonBox::accepted, this, [this]() {
        if (applyToDocument()) {
            accept();
        }
    });
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    mainLayout->addWidget(btnBox);
    updateControlStates();
    updateStatisticsDisplay();
}

void DefineObservationDialog::setCreationMode(CreationMode mode) {
    m_mode = mode;
    {
        QSignalBlocker b1(m_radioDefineObs);
        QSignalBlocker b2(m_radioImportRelief);
        m_radioDefineObs->setChecked(mode == CreationMode::DefineObservation);
        m_radioImportRelief->setChecked(mode == CreationMode::ImportReliefGrid);
    }
    if (m_mode == CreationMode::ImportReliefGrid && m_fromTopoGrid && !m_loadedTopoGrid.empty()) {
        double topoMaxZ = std::ceil(m_loadedTopoGrid.max());
        if (std::isfinite(topoMaxZ) && topoMaxZ > m_spinZMax->value()) {
            m_spinZMax->setValue(topoMaxZ);
        }
    }
    updateControlStates();
    updateStatisticsDisplay();
}

void DefineObservationDialog::syncColsFromXBounds() {
    double dx = m_spinDX->value();
    double span = m_spinXMax->value() - m_spinX0->value();
    if (dx > 0.0 && span > 0.0) {
        int cols = std::clamp(static_cast<int>(std::ceil(span / dx - 1e-9)) + 1,
                              m_spinCols->minimum(), m_spinCols->maximum());
        QSignalBlocker blocker(m_spinCols);
        m_spinCols->setValue(cols);
    }
    updateStatisticsDisplay();
}

void DefineObservationDialog::syncXMaxFromCols() {
    QSignalBlocker blocker(m_spinXMax);
    m_spinXMax->setValue(m_spinX0->value() + (m_spinCols->value() - 1) * m_spinDX->value());
    updateStatisticsDisplay();
}

void DefineObservationDialog::syncRowsFromYBounds() {
    double dy = m_spinDY->value();
    double span = m_spinYMax->value() - m_spinY0->value();
    if (dy > 0.0 && span > 0.0) {
        int rows = std::clamp(static_cast<int>(std::ceil(span / dy - 1e-9)) + 1,
                              m_spinRows->minimum(), m_spinRows->maximum());
        QSignalBlocker blocker(m_spinRows);
        m_spinRows->setValue(rows);
    }
    updateStatisticsDisplay();
}

void DefineObservationDialog::syncYMaxFromRows() {
    QSignalBlocker blocker(m_spinYMax);
    m_spinYMax->setValue(m_spinY0->value() + (m_spinRows->value() - 1) * m_spinDY->value());
    updateStatisticsDisplay();
}

void DefineObservationDialog::syncDimensionsFromGrid(const mod3d::Grid &g) {
    QSignalBlocker bx0(m_spinX0), bxm(m_spinXMax), bdx(m_spinDX), bc(m_spinCols);
    QSignalBlocker by0(m_spinY0), bym(m_spinYMax), bdy(m_spinDY), br(m_spinRows);

    const double gx0 = g.x0().value();
    const double gdx = g.dx().value();
    const double gy0 = g.y0().value();
    const double gdy = g.dy().value();

    m_spinX0->setValue(gx0);
    m_spinDX->setValue(gdx);
    m_spinCols->setValue(static_cast<int>(g.cols()));
    m_spinXMax->setValue(gx0 + static_cast<double>(g.cols() - 1) * gdx);

    m_spinY0->setValue(gy0);
    m_spinDY->setValue(gdy);
    m_spinRows->setValue(static_cast<int>(g.rows()));
    m_spinYMax->setValue(gy0 + static_cast<double>(g.rows() - 1) * gdy);
}

void DefineObservationDialog::updateControlStates() {
    const bool hasAnyGrid = m_fromTopoGrid || m_fromFieldGrid;
    const bool xyEditable = (m_mode == CreationMode::DefineObservation) && !hasAnyGrid;
    m_spinX0->setEnabled(xyEditable);
    m_spinXMax->setEnabled(xyEditable);
    m_spinDX->setEnabled(xyEditable);
    m_spinCols->setEnabled(xyEditable);

    m_spinY0->setEnabled(xyEditable);
    m_spinYMax->setEnabled(xyEditable);
    m_spinDY->setEnabled(xyEditable);
    m_spinRows->setEnabled(xyEditable);

    m_btnClearTopoGrid->setEnabled(m_fromTopoGrid);
    m_btnClearFieldGrid->setEnabled(m_fromFieldGrid);
}

double DefineObservationDialog::lengthX() const noexcept {
    if (!m_spinX0 || !m_spinXMax) return 0.0;
    return std::max(0.0, m_spinXMax->value() - m_spinX0->value());
}

double DefineObservationDialog::lengthY() const noexcept {
    if (!m_spinY0 || !m_spinYMax) return 0.0;
    return std::max(0.0, m_spinYMax->value() - m_spinY0->value());
}

double DefineObservationDialog::gridMinZ() const noexcept {
    if (!m_fromTopoGrid || m_loadedTopoGrid.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return m_loadedTopoGrid.min();
}

double DefineObservationDialog::gridMaxZ() const noexcept {
    if (!m_fromTopoGrid || m_loadedTopoGrid.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return m_loadedTopoGrid.max();
}

void DefineObservationDialog::updateStatisticsDisplay() {
    if (m_lblLengthX) {
        m_lblLengthX->setText(QString("%1 m").arg(lengthX(), 0, 'f', 2));
    }
    if (m_lblLengthY) {
        m_lblLengthY->setText(QString("%1 m").arg(lengthY(), 0, 'f', 2));
    }

    const bool topoAvail = m_fromTopoGrid && !m_loadedTopoGrid.empty();
    if (m_lblGridMinZTitle) m_lblGridMinZTitle->setEnabled(topoAvail);
    if (m_lblGridMinZ) {
        m_lblGridMinZ->setEnabled(topoAvail);
        m_lblGridMinZ->setText(topoAvail ? QString("%1 m").arg(m_loadedTopoGrid.min(), 0, 'f', 2)
                                         : tr("N/A"));
    }
    if (m_lblGridMaxZTitle) m_lblGridMaxZTitle->setEnabled(topoAvail);
    if (m_lblGridMaxZ) {
        m_lblGridMaxZ->setEnabled(topoAvail);
        m_lblGridMaxZ->setText(topoAvail ? QString("%1 m").arg(m_loadedTopoGrid.max(), 0, 'f', 2)
                                         : tr("N/A"));
    }
}

void DefineObservationDialog::onBrowseTopoGridFile() {
    QString fn = QFileDialog::getOpenFileName(
        this,
        tr("Select Topography Surfer Grid File"),
        QString(),
        tr("Surfer Grid Files (*.grd);;All Files (*)")
    );
    if (!fn.isEmpty()) {
        if (!loadFromTopoGridFile(fn)) {
            QMessageBox::warning(this, tr("Grid Load Failed"),
                                 tr("Failed to read topography grid file:\n%1").arg(fn));
        }
    }
}

void DefineObservationDialog::onBrowseFieldGridFile() {
    QString fn = QFileDialog::getOpenFileName(
        this,
        tr("Select Measured Field Surfer Grid File"),
        QString(),
        tr("Surfer Grid Files (*.grd);;All Files (*)")
    );
    if (!fn.isEmpty()) {
        if (!loadFromFieldGridFile(fn)) {
            QMessageBox::warning(this, tr("Grid Load Failed"),
                                 tr("Failed to read measured field grid file:\n%1").arg(fn));
        }
    }
}

bool DefineObservationDialog::loadFromTopoGridFile(const QString &filePath) {
    mod3d::Grid g;
    if (!g.load_file(filePath.toStdString()) || g.rows() < 2 || g.cols() < 2) {
        return false;
    }

    m_loadedTopoGrid = g;
    m_fromTopoGrid = true;
    m_editTopoGridFile->setText(filePath);

    // Sync dimensions from topography grid
    syncDimensionsFromGrid(g);

    // Vertical range from topography: bottom = zmin - 0.4*L, top = zmax + 0.1*L (rounded to 10/100/1000 m)
    double sugZMin = 0.0, sugZMax = 0.0;
    ModelDocument::suggestVerticalRange(g, sugZMin, sugZMax);
    if (std::isfinite(sugZMin) && std::isfinite(sugZMax) && sugZMin < sugZMax) {
        m_spinZMin->setValue(sugZMin);
        m_spinZMax->setValue(sugZMax);
    }

    updateControlStates();
    updateStatisticsDisplay();
    return true;
}

bool DefineObservationDialog::loadFromFieldGridFile(const QString &filePath) {
    mod3d::Grid g;
    if (!g.load_file(filePath.toStdString()) || g.rows() < 2 || g.cols() < 2) {
        return false;
    }

    m_loadedFieldGrid = g;
    m_fromFieldGrid = true;
    m_editFieldGridFile->setText(filePath);

    // If only measured field is imported, use its grid dimensions for observation area definition
    if (!m_fromTopoGrid) {
        syncDimensionsFromGrid(g);
    }

    updateControlStates();
    updateStatisticsDisplay();
    return true;
}

void DefineObservationDialog::clearTopoGrid() {
    m_fromTopoGrid = false;
    m_loadedTopoGrid = mod3d::Grid();
    if (m_editTopoGridFile) m_editTopoGridFile->clear();

    if (m_fromFieldGrid && !m_loadedFieldGrid.empty()) {
        syncDimensionsFromGrid(m_loadedFieldGrid);
    }

    updateControlStates();
    updateStatisticsDisplay();
}

void DefineObservationDialog::clearFieldGrid() {
    m_fromFieldGrid = false;
    m_loadedFieldGrid = mod3d::Grid();
    if (m_editFieldGridFile) m_editFieldGridFile->clear();

    if (m_fromTopoGrid && !m_loadedTopoGrid.empty()) {
        syncDimensionsFromGrid(m_loadedTopoGrid);
    }

    updateControlStates();
    updateStatisticsDisplay();
}

void DefineObservationDialog::clearGridFile() {
    clearTopoGrid();
    clearFieldGrid();
}

QString DefineObservationDialog::topoGridFilePath() const {
    return m_editTopoGridFile ? m_editTopoGridFile->text() : QString();
}

QString DefineObservationDialog::fieldGridFilePath() const {
    return m_editFieldGridFile ? m_editFieldGridFile->text() : QString();
}

QString DefineObservationDialog::gridFilePath() const {
    if (m_editTopoGridFile && !m_editTopoGridFile->text().isEmpty()) {
        return m_editTopoGridFile->text();
    }
    if (m_editFieldGridFile && !m_editFieldGridFile->text().isEmpty()) {
        return m_editFieldGridFile->text();
    }
    return QString();
}

mod3d::FieldComponent DefineObservationDialog::fieldComponent() const noexcept {
    if (!m_comboFieldComp) return mod3d::FieldComponent::GZ;
    return static_cast<mod3d::FieldComponent>(m_comboFieldComp->currentData().toInt());
}

void DefineObservationDialog::setFieldComponent(mod3d::FieldComponent comp) {
    if (!m_comboFieldComp) return;
    int idx = m_comboFieldComp->findData(static_cast<int>(comp));
    if (idx >= 0) {
        m_comboFieldComp->setCurrentIndex(idx);
    }
}

bool DefineObservationDialog::applyToDocument() {
    if (!m_doc) return false;

    const double zMin = m_spinZMin->value();
    const double zMax = m_spinZMax->value();
    if (zMin >= zMax) {
        QMessageBox::warning(this, tr("Invalid Vertical Range"),
                             tr("Minimum Z must be strictly less than Maximum Z."));
        return false;
    }

    const mod3d::Grid *pTopo = m_fromTopoGrid ? &m_loadedTopoGrid : nullptr;
    const mod3d::Grid *pField = m_fromFieldGrid ? &m_loadedFieldGrid : nullptr;

    if (m_mode == CreationMode::ImportReliefGrid) {
        if (!pTopo && !pField) {
            onBrowseTopoGridFile();
            pTopo = m_fromTopoGrid ? &m_loadedTopoGrid : nullptr;
            pField = m_fromFieldGrid ? &m_loadedFieldGrid : nullptr;
            if (!pTopo && !pField) {
                return false;
            }
        }
        return m_doc->newProjectFromImportedGrids(pTopo, pField, fieldComponent(), zMin, m_spinZMax->value());
    }

    const double x0 = m_spinX0->value();
    const double xMax = m_spinXMax->value();
    const double dx = m_spinDX->value();
    const double y0 = m_spinY0->value();
    const double yMax = m_spinYMax->value();
    const double dy = m_spinDY->value();

    if (x0 >= xMax || y0 >= yMax || dx <= 0.0 || dy <= 0.0) {
        QMessageBox::warning(this, tr("Invalid Dimensions"),
                             tr("Minimum coordinates must be less than Maximum coordinates and increments must be positive."));
        return false;
    }

    const int cols = static_cast<int>(std::ceil((xMax - x0) / dx - 1e-9)) + 1;
    const int rows = static_cast<int>(std::ceil((yMax - y0) / dy - 1e-9)) + 1;

    if (!m_doc->newProject(rows, cols, x0, y0, dx, dy, zMin, zMax)) {
        return false;
    }

    if (pField) {
        m_doc->observation().set_observed_grid(fieldComponent(), *pField);
        m_doc->observation().compute_difference(fieldComponent());
        m_doc->setActiveField(fieldComponent());
        m_doc->setShowMeasuredField(true);
        m_doc->setShowDifferenceField(true);
        m_doc->computeAll();
    }
    return true;
}

} // namespace mod3d_gui
