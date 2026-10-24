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
    setMinimumWidth(520);

    auto *mainLayout = new QVBoxLayout(this);

    // 1. Creation Mode Group (mirrors legacy CDlgStart)
    auto *modeGroup = new QGroupBox(tr("New Model Creation Mode"), this);
    auto *modeLayout = new QVBoxLayout(modeGroup);
    m_radioDefineObs = new QRadioButton(tr("Define observation area (custom dimensions or from grid geometry)"), modeGroup);
    m_radioImportRelief = new QRadioButton(tr("Import observation area relief grid (surface topography from .grd)"), modeGroup);
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

    // 3. Get From Grid File Row (mirrors legacy IDC_GTF_FROM_GRID + IDC_GRID_FILE)
    auto *gridFileLayout = new QHBoxLayout();
    m_btnGetFromGrid = new QPushButton(tr("Get From Grid File..."), this);
    m_editGridFile = new QLineEdit(this);
    m_editGridFile->setReadOnly(true);
    m_editGridFile->setPlaceholderText(tr("No grid file selected"));
    m_btnClearGrid = new QPushButton(tr("Clear"), this);
    m_btnClearGrid->setEnabled(false);

    gridFileLayout->addWidget(m_btnGetFromGrid);
    gridFileLayout->addWidget(m_editGridFile, 1);
    gridFileLayout->addWidget(m_btnClearGrid);
    mainLayout->addLayout(gridFileLayout);

    // 4. Grid & Observation Area Statistics Group
    auto *statsGroup = new QGroupBox(tr("Grid && Area Statistics"), this);
    auto *statsGrid = new QGridLayout(statsGroup);

    m_lblLengthX = new QLabel(statsGroup);
    m_lblLengthY = new QLabel(statsGroup);
    m_lblGridMinZ = new QLabel(statsGroup);
    m_lblGridMaxZ = new QLabel(statsGroup);

    statsGrid->addWidget(new QLabel(tr("<b>Length X:</b>"), statsGroup), 0, 0);
    statsGrid->addWidget(m_lblLengthX, 0, 1);
    statsGrid->addWidget(new QLabel(tr("<b>Length Y:</b>"), statsGroup), 0, 2);
    statsGrid->addWidget(m_lblLengthY, 0, 3);

    statsGrid->addWidget(new QLabel(tr("<b>Grid Min Z:</b>"), statsGroup), 1, 0);
    statsGrid->addWidget(m_lblGridMinZ, 1, 1);
    statsGrid->addWidget(new QLabel(tr("<b>Grid Max Z:</b>"), statsGroup), 1, 2);
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

    connect(m_btnGetFromGrid, &QPushButton::clicked, this, &DefineObservationDialog::onBrowseGridFile);
    connect(m_btnClearGrid, &QPushButton::clicked, this, &DefineObservationDialog::clearGridFile);

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
    if (m_mode == CreationMode::ImportReliefGrid && m_fromGrid && !m_loadedGrid.empty()) {
        double gridMaxZ = std::ceil(m_loadedGrid.max());
        if (std::isfinite(gridMaxZ) && gridMaxZ > m_spinZMax->value()) {
            m_spinZMax->setValue(gridMaxZ);
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

void DefineObservationDialog::updateControlStates() {
    bool xyEditable = (m_mode == CreationMode::DefineObservation) && !m_fromGrid;
    m_spinX0->setEnabled(xyEditable);
    m_spinXMax->setEnabled(xyEditable);
    m_spinDX->setEnabled(xyEditable);
    m_spinCols->setEnabled(xyEditable);

    m_spinY0->setEnabled(xyEditable);
    m_spinYMax->setEnabled(xyEditable);
    m_spinDY->setEnabled(xyEditable);
    m_spinRows->setEnabled(xyEditable);

    m_btnClearGrid->setEnabled(m_fromGrid);
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
    if (!m_fromGrid || m_loadedGrid.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return m_loadedGrid.min();
}

double DefineObservationDialog::gridMaxZ() const noexcept {
    if (!m_fromGrid || m_loadedGrid.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return m_loadedGrid.max();
}

void DefineObservationDialog::updateStatisticsDisplay() {
    if (m_lblLengthX) {
        m_lblLengthX->setText(QString("%1 m").arg(lengthX(), 0, 'f', 2));
    }
    if (m_lblLengthY) {
        m_lblLengthY->setText(QString("%1 m").arg(lengthY(), 0, 'f', 2));
    }
    if (m_fromGrid && !m_loadedGrid.empty()) {
        if (m_lblGridMinZ) {
            m_lblGridMinZ->setText(QString("%1 m").arg(m_loadedGrid.min(), 0, 'f', 2));
        }
        if (m_lblGridMaxZ) {
            m_lblGridMaxZ->setText(QString("%1 m").arg(m_loadedGrid.max(), 0, 'f', 2));
        }
    } else {
        if (m_lblGridMinZ) {
            m_lblGridMinZ->setText(tr("N/A (no grid loaded)"));
        }
        if (m_lblGridMaxZ) {
            m_lblGridMaxZ->setText(tr("N/A (no grid loaded)"));
        }
    }
}

void DefineObservationDialog::onBrowseGridFile() {
    QString fn = QFileDialog::getOpenFileName(
        this,
        tr("Select Surfer Grid File"),
        QString(),
        tr("Surfer Grid Files (*.grd);;All Files (*)")
    );
    if (!fn.isEmpty()) {
        if (!loadFromGridFile(fn)) {
            QMessageBox::warning(this, tr("Grid Load Failed"),
                                 tr("Failed to read grid file:\n%1").arg(fn));
        }
    }
}

bool DefineObservationDialog::loadFromGridFile(const QString &filePath) {
    mod3d::Grid g;
    if (!g.load_file(filePath.toStdString()) || g.rows() < 2 || g.cols() < 2) {
        return false;
    }

    m_loadedGrid = g;
    m_fromGrid = true;
    m_editGridFile->setText(filePath);

    {
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

    // Legacy behavior: when importing a relief grid, ensure ZMax >= ceil(grid.max())
    if (m_mode == CreationMode::ImportReliefGrid) {
        double gridMaxZ = std::ceil(g.max());
        if (std::isfinite(gridMaxZ) && gridMaxZ > m_spinZMax->value()) {
            m_spinZMax->setValue(gridMaxZ);
        }
    }

    updateControlStates();
    updateStatisticsDisplay();
    return true;
}

void DefineObservationDialog::clearGridFile() {
    m_fromGrid = false;
    m_loadedGrid = mod3d::Grid();
    m_editGridFile->clear();
    updateControlStates();
    updateStatisticsDisplay();
}

QString DefineObservationDialog::gridFilePath() const {
    return m_editGridFile->text();
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

    if (m_mode == CreationMode::ImportReliefGrid) {
        if (!m_fromGrid) {
            onBrowseGridFile();
            if (!m_fromGrid) {
                return false;
            }
        }
        return m_doc->newProjectFromReliefGrid(m_loadedGrid, zMin, m_spinZMax->value());
    }

    if (m_fromGrid) {
        return m_doc->newProject(
            static_cast<int>(m_loadedGrid.rows()),
            static_cast<int>(m_loadedGrid.cols()),
            m_loadedGrid.x0().value(),
            m_loadedGrid.y0().value(),
            m_loadedGrid.dx().value(),
            m_loadedGrid.dy().value(),
            zMin,
            zMax
        );
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

    return m_doc->newProject(rows, cols, x0, y0, dx, dy, zMin, zMax);
}

} // namespace mod3d_gui
