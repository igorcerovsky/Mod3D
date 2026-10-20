#include "mod3d_gui/SettingsPane.h"
#include "mod3d_gui/MainWindow.h"
#include "mod3d_gui/ProfileView.h"
#include "mod3d_gui/MapView.h"
#include "mod3d_gui/GL3DView.h"
#include "mod3d_gui/dialogs/BodyPropertiesDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QScrollArea>
#include <QColorDialog>
#include <QFrame>
#include <QPixmap>
#include <QPainter>
#include <QMessageBox>
#include <cmath>

namespace mod3d_gui {

namespace {

QIcon createColorIcon(const QColor &color, int size = 16) {
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setBrush(color);
    p.setPen(color.darker(150));
    p.drawRoundedRect(1, 1, size - 2, size - 2, 3, 3);
    return QIcon(pix);
}

} // namespace

SettingsPane::SettingsPane(ModelDocument *doc, MainWindow *mainWindow, QWidget *parent)
    : QDockWidget(tr("Settings & Inspector"), parent)
    , m_doc(doc)
    , m_mainWindow(mainWindow)
{
    setObjectName("SettingsPane");
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setMinimumWidth(340);
    toggleViewAction()->setText(tr("Settings && Inspector Pane"));

    setupUi();

    connect(m_doc, &ModelDocument::modelGeometryChanged, this, [this]() {
        updateBodyList();
        loadBodyData();
        loadPhysicsData();
    });
    connect(m_doc, &ModelDocument::documentReset, this, [this]() {
        refreshAll();
    });
    connect(m_doc, &ModelDocument::computationFinished, this, [this]() {
        m_lblRmsStatus->setText(QString("RMS: %1 %2 | DRV: %3")
            .arg(m_doc->currentRms(), 0, 'f', 2)
            .arg(m_doc->activeFieldUnit())
            .arg(m_doc->currentDerivative(), 0, 'f', 2));
    });
    connect(m_doc, &ModelDocument::activeFieldChanged, this, [this](mod3d::FieldComponent) {
        updateViewContext();
    });
    connect(m_doc, &ModelDocument::fieldDisplayToggled, this, [this]() {
        updateViewContext();
    });
    connect(m_doc, &ModelDocument::profileChanged, this, [this](int idx, ProfileOrientation orient) {
        if (!m_blockSync) {
            m_blockSync = true;
            if (m_spinProfTabIdx) m_spinProfTabIdx->setValue(idx);
            if (m_btnProfTabOrientEW) m_btnProfTabOrientEW->setChecked(orient == ProfileOrientation::EastWest);
            if (m_btnProfTabOrientSN) m_btnProfTabOrientSN->setChecked(orient == ProfileOrientation::SouthNorth);
            double coord = (orient == ProfileOrientation::EastWest)
                ? m_doc->model().yd_val(idx + 1)
                : m_doc->model().xd_val(idx + 1);
            QString coordName = (orient == ProfileOrientation::EastWest) ? "Y" : "X";
            if (m_lblProfTabCoord) {
                m_lblProfTabCoord->setText(QString("%1 = %2 m").arg(coordName).arg(coord, 0, 'f', 1));
            }
            m_blockSync = false;
        }
    });

    refreshAll();
}

void SettingsPane::selectDisplayTab() {
    if (m_tabs) m_tabs->setCurrentIndex(0);
}

void SettingsPane::selectProfilesTab() {
    if (m_tabs) m_tabs->setCurrentIndex(1);
}

void SettingsPane::selectBodyEditorTab() {
    if (m_tabs) m_tabs->setCurrentIndex(2);
}

void SettingsPane::selectPhysicsTab() {
    if (m_tabs) m_tabs->setCurrentIndex(3);
}

void SettingsPane::setupUi() {
    auto *centralWidget = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);

    m_tabs = new QTabWidget(centralWidget);
    m_tabs->setStyleSheet(R"(
        QTabWidget::pane {
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            background: #ffffff;
            top: -1px;
        }
        QTabBar::tab {
            background: #f1f5f9;
            color: #475569;
            font-weight: bold;
            font-size: 11px;
            padding: 5px 6px;
            border: 1px solid #cbd5e1;
            border-bottom: none;
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
            margin-right: 1px;
        }
        QTabBar::tab:selected {
            background: #ffffff;
            color: #1e293b;
            border-bottom: 2px solid #2563eb;
        }
        QTabBar::tab:hover:!selected {
            background: #e2e8f0;
        }
    )");

    m_tabDisplay = new QWidget();
    m_tabProfiles = new QWidget();
    m_tabBodies = new QWidget();
    m_tabPhysics = new QWidget();

    setupDisplayTab(m_tabDisplay);
    setupProfilesTab(m_tabProfiles);
    setupBodyEditorTab(m_tabBodies);
    setupPhysicsTab(m_tabPhysics);

    m_tabs->addTab(m_tabDisplay, tr("📊 Display"));
    m_tabs->addTab(m_tabProfiles, tr("📏 Profiles"));
    m_tabs->addTab(m_tabBodies, tr("🪨 Bodies"));
    m_tabs->addTab(m_tabPhysics, tr("⚙ Physics"));

    mainLayout->addWidget(m_tabs, 1);

    setupComputeCard(centralWidget);
    mainLayout->addWidget(m_grpCompute);

    setWidget(centralWidget);
}

// ============================================================================
// TAB 1: Display Tab
// ============================================================================
void SettingsPane::setupDisplayTab(QWidget *container) {
    auto *scroll = new QScrollArea(container);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *inner = new QWidget(scroll);
    auto *layout = new QVBoxLayout(inner);
    layout->setContentsMargins(6, 8, 6, 8);
    layout->setSpacing(6);
    layout->setAlignment(Qt::AlignTop);

    // Active View Context Banner
    m_lblViewContext = new QLabel(tr("Active View: None"), inner);
    m_lblViewContext->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_lblViewContext->setFixedHeight(26);
    m_lblViewContext->setAlignment(Qt::AlignCenter);
    m_lblViewContext->setStyleSheet("font-weight: bold; color: #1e293b; padding: 4px 6px; background-color: #e2e8f0; border-radius: 4px;");
    layout->addWidget(m_lblViewContext);

    // --- Sub-panel 1: Profile View Display Controls ---
    m_paneProfileFields = new QWidget(inner);
    auto *pvLayout = new QVBoxLayout(m_paneProfileFields);
    pvLayout->setContentsMargins(0, 0, 0, 0);
    pvLayout->setSpacing(6);
    pvLayout->setAlignment(Qt::AlignTop);

    auto *formPv = new QFormLayout();
    formPv->setContentsMargins(0, 0, 0, 0);

    m_comboProfileMainField = new QComboBox(m_paneProfileFields);
    m_comboProfileMainField->addItem(tr("Gz - Vertical Gravity"), static_cast<int>(mod3d::FieldComponent::GZ));
    m_comboProfileMainField->addItem(tr("ΔT - Total Magnetic Field"), static_cast<int>(mod3d::FieldComponent::DELTA_T));
    m_comboProfileMainField->addItem(tr("Gt - Total Gravity Field"), static_cast<int>(mod3d::FieldComponent::G_TOT));
    m_comboProfileMainField->addItem(tr("Gx - East-West Gravity"), static_cast<int>(mod3d::FieldComponent::GX));
    m_comboProfileMainField->addItem(tr("Gy - North-South Gravity"), static_cast<int>(mod3d::FieldComponent::GY));
    m_comboProfileMainField->addItem(tr("Mz - Vertical Magnetics"), static_cast<int>(mod3d::FieldComponent::MZ));
    m_comboProfileMainField->addItem(tr("Mx - East-West Magnetics"), static_cast<int>(mod3d::FieldComponent::MX));
    m_comboProfileMainField->addItem(tr("My - North-South Magnetics"), static_cast<int>(mod3d::FieldComponent::MY));
    m_comboProfileMainField->addItem(tr("Tzz - Vertical Gradient"), static_cast<int>(mod3d::FieldComponent::GZZ));
    m_comboProfileMainField->addItem(tr("Txx - East-West Gradient"), static_cast<int>(mod3d::FieldComponent::GXX));
    m_comboProfileMainField->addItem(tr("Tyy - North-South Gradient"), static_cast<int>(mod3d::FieldComponent::GYY));
    m_comboProfileMainField->addItem(tr("Txy - Horizontal Gradient"), static_cast<int>(mod3d::FieldComponent::GXY));
    m_comboProfileMainField->addItem(tr("Txz - Cross Gradient XZ"), static_cast<int>(mod3d::FieldComponent::GXZ));
    m_comboProfileMainField->addItem(tr("Tyz - Cross Gradient YZ"), static_cast<int>(mod3d::FieldComponent::GYZ));
    formPv->addRow(tr("Primary Field:"), m_comboProfileMainField);

    m_lblProfileFieldInfo = new QLabel(tr("Y-Axis: Gz (mGal)"), m_paneProfileFields);
    m_lblProfileFieldInfo->setStyleSheet("color: #64748b; font-size: 11px;");
    formPv->addRow(QString(), m_lblProfileFieldInfo);
    pvLayout->addLayout(formPv);

    connect(m_comboProfileMainField, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (m_blockSync) return;
        auto comp = static_cast<mod3d::FieldComponent>(m_comboProfileMainField->itemData(idx).toInt());
        if (auto *pv = qobject_cast<ProfileView*>(m_activeView.data())) {
            pv->setMainFieldComponent(comp);
            m_lblProfileFieldInfo->setText(QString("Y-Axis: %1 (%2)").arg(ModelDocument::componentName(comp)).arg(ModelDocument::componentUnit(comp)));
        }
    });

    // Profile Curves Checkboxes
    auto *boxCurves = new QGroupBox(tr("Curves to Display (Multiple)"), m_paneProfileFields);
    auto *curvesLayout = new QVBoxLayout(boxCurves);
    curvesLayout->setContentsMargins(8, 8, 8, 8);
    curvesLayout->setSpacing(4);

    m_chkProfModeled = new QCheckBox(tr("Modeled Field (Solid Blue)"), boxCurves);
    m_chkProfModeled->setChecked(true);
    m_chkProfMeasured = new QCheckBox(tr("Measured / Observed (Red +)"), boxCurves);
    m_chkProfMeasured->setChecked(false);
    m_chkProfMeasured->setEnabled(false);
    m_chkProfDiff = new QCheckBox(tr("Difference Obs-Mod (Magenta --)"), boxCurves);
    m_chkProfDiff->setChecked(false);
    m_chkProfDiff->setEnabled(false);

    curvesLayout->addWidget(m_chkProfModeled);
    curvesLayout->addWidget(m_chkProfMeasured);
    curvesLayout->addWidget(m_chkProfDiff);

    auto *secRow = new QHBoxLayout();
    secRow->setContentsMargins(0, 0, 0, 0);
    m_chkProfSecondary = new QCheckBox(tr("Secondary:"), boxCurves);
    m_comboProfileSecField = new QComboBox(boxCurves);
    m_comboProfileSecField->addItem(tr("ΔT (Mag)"), static_cast<int>(mod3d::FieldComponent::DELTA_T));
    m_comboProfileSecField->addItem(tr("Gz (Grav)"), static_cast<int>(mod3d::FieldComponent::GZ));
    m_comboProfileSecField->addItem(tr("Gt (Grav)"), static_cast<int>(mod3d::FieldComponent::G_TOT));
    m_comboProfileSecField->addItem(tr("Tzz (Grad)"), static_cast<int>(mod3d::FieldComponent::GZZ));
    secRow->addWidget(m_chkProfSecondary);
    secRow->addWidget(m_comboProfileSecField);
    curvesLayout->addLayout(secRow);

    pvLayout->addWidget(boxCurves);

    auto updatePvCurves = [this]() {
        if (m_blockSync) return;
        if (auto *pv = qobject_cast<ProfileView*>(m_activeView.data())) {
            bool hasMeasured = pv->hasMeasuredField();
            pv->setShowModeledCurve(m_chkProfModeled->isChecked());
            pv->setShowMeasuredCurve(hasMeasured && m_chkProfMeasured->isChecked());
            pv->setShowDifferenceCurve(hasMeasured && m_chkProfDiff->isChecked());
            pv->setShowSecondaryCurve(m_chkProfSecondary->isChecked());
            auto secComp = static_cast<mod3d::FieldComponent>(m_comboProfileSecField->currentData().toInt());
            pv->setSecondaryFieldComponent(secComp);
        }
    };
    connect(m_chkProfModeled, &QCheckBox::toggled, this, updatePvCurves);
    connect(m_chkProfMeasured, &QCheckBox::toggled, this, updatePvCurves);
    connect(m_chkProfDiff, &QCheckBox::toggled, this, updatePvCurves);
    connect(m_chkProfSecondary, &QCheckBox::toggled, this, updatePvCurves);
    connect(m_comboProfileSecField, QOverload<int>::of(&QComboBox::currentIndexChanged), this, updatePvCurves);

    // Profile Visual Overlays & Show Checks
    auto *boxPvOverlays = new QGroupBox(tr("Profile Visual Overlays"), m_paneProfileFields);
    auto *pvOverlaysLayout = new QVBoxLayout(boxPvOverlays);
    pvOverlaysLayout->setContentsMargins(8, 8, 8, 8);
    pvOverlaysLayout->setSpacing(4);

    m_chkProfShowStationPts = new QCheckBox(tr("Observation Points"), boxPvOverlays);
    m_chkProfShowStationPts->setChecked(true);
    m_chkProfGrid = new QCheckBox(tr("Grid Lines"), boxPvOverlays);
    m_chkProfGrid->setChecked(true);
    m_chkProfShowRelief = new QCheckBox(tr("Relief Profile / DEM Surface"), boxPvOverlays);
    m_chkProfShowRelief->setChecked(true);
    m_chkProfShowIntersects = new QCheckBox(tr("Cross-Profile Intersections"), boxPvOverlays);
    m_chkProfShowIntersects->setChecked(true);
    m_chkProfGhostPrev = new QCheckBox(tr("Ghost Outlines Prev (-1)"), boxPvOverlays);
    m_chkProfGhostNext = new QCheckBox(tr("Ghost Outlines Next (+1)"), boxPvOverlays);

    pvOverlaysLayout->addWidget(m_chkProfShowStationPts);
    pvOverlaysLayout->addWidget(m_chkProfGrid);
    pvOverlaysLayout->addWidget(m_chkProfShowRelief);
    pvOverlaysLayout->addWidget(m_chkProfShowIntersects);
    pvOverlaysLayout->addWidget(m_chkProfGhostPrev);
    pvOverlaysLayout->addWidget(m_chkProfGhostNext);
    pvLayout->addWidget(boxPvOverlays);

    auto updatePvOverlays = [this]() {
        if (m_blockSync) return;
        if (auto *pv = qobject_cast<ProfileView*>(m_activeView.data())) {
            pv->setShowObservationPoints(m_chkProfShowStationPts->isChecked());
            pv->setShowGridLines(m_chkProfGrid->isChecked());
            pv->setShowRelief(m_chkProfShowRelief->isChecked());
            pv->setShowProfileIntersections(m_chkProfShowIntersects->isChecked());
            pv->setShowPreviousProfile(m_chkProfGhostPrev->isChecked());
            pv->setShowNextProfile(m_chkProfGhostNext->isChecked());
        }
        // Keep Profiles Tab Show Checks synchronized
        m_blockSync = true;
        m_chkProfTabStationPts->setChecked(m_chkProfShowStationPts->isChecked());
        m_chkProfTabGrid->setChecked(m_chkProfGrid->isChecked());
        m_chkProfTabRelief->setChecked(m_chkProfShowRelief->isChecked());
        m_chkProfTabIntersects->setChecked(m_chkProfShowIntersects->isChecked());
        m_chkProfTabGhostPrev->setChecked(m_chkProfGhostPrev->isChecked());
        m_chkProfTabGhostNext->setChecked(m_chkProfGhostNext->isChecked());
        m_blockSync = false;
    };
    connect(m_chkProfShowStationPts, &QCheckBox::toggled, this, updatePvOverlays);
    connect(m_chkProfGrid, &QCheckBox::toggled, this, updatePvOverlays);
    connect(m_chkProfShowRelief, &QCheckBox::toggled, this, updatePvOverlays);
    connect(m_chkProfShowIntersects, &QCheckBox::toggled, this, updatePvOverlays);
    connect(m_chkProfGhostPrev, &QCheckBox::toggled, this, updatePvOverlays);
    connect(m_chkProfGhostNext, &QCheckBox::toggled, this, updatePvOverlays);

    layout->addWidget(m_paneProfileFields);

    // --- Sub-panel 2: Map View Fields ---
    m_paneMapFields = new QWidget(inner);
    auto *mvLayout = new QVBoxLayout(m_paneMapFields);
    mvLayout->setContentsMargins(0, 0, 0, 0);
    mvLayout->setSpacing(6);

    auto *formMv = new QFormLayout();
    m_comboMapField = new QComboBox(m_paneMapFields);
    m_comboMapField->addItem(tr("Gz - Vertical Gravity"), static_cast<int>(mod3d::FieldComponent::GZ));
    m_comboMapField->addItem(tr("ΔT - Total Magnetic Field"), static_cast<int>(mod3d::FieldComponent::DELTA_T));
    m_comboMapField->addItem(tr("Gt - Total Gravity Field"), static_cast<int>(mod3d::FieldComponent::G_TOT));
    m_comboMapField->addItem(tr("Gx - East-West Gravity"), static_cast<int>(mod3d::FieldComponent::GX));
    m_comboMapField->addItem(tr("Gy - North-South Gravity"), static_cast<int>(mod3d::FieldComponent::GY));
    m_comboMapField->addItem(tr("Mz - Vertical Magnetics"), static_cast<int>(mod3d::FieldComponent::MZ));
    m_comboMapField->addItem(tr("Mx - East-West Magnetics"), static_cast<int>(mod3d::FieldComponent::MX));
    m_comboMapField->addItem(tr("My - North-South Magnetics"), static_cast<int>(mod3d::FieldComponent::MY));
    formMv->addRow(tr("Field Component:"), m_comboMapField);
    mvLayout->addLayout(formMv);

    connect(m_comboMapField, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (m_blockSync) return;
        auto comp = static_cast<mod3d::FieldComponent>(m_comboMapField->itemData(idx).toInt());
        if (auto *mv = qobject_cast<MapView*>(m_activeView.data())) {
            mv->setFieldComponent(comp);
            updateViewContext();
        }
    });

    // Single Field Bitmap Radio Group
    auto *boxRaster = new QGroupBox(tr("Single Field Bitmap (Map Display)"), m_paneMapFields);
    auto *rasterLayout = new QVBoxLayout(boxRaster);
    rasterLayout->setContentsMargins(8, 8, 8, 8);
    rasterLayout->setSpacing(4);

    m_grpMapRaster = new QButtonGroup(boxRaster);
    m_rbMapModeled = new QRadioButton(tr("Modeled Field Bitmap"), boxRaster);
    m_rbMapObserved = new QRadioButton(tr("Measured / Observed Bitmap"), boxRaster);
    m_rbMapDiff = new QRadioButton(tr("Difference Field Bitmap"), boxRaster);
    m_rbMapRelief = new QRadioButton(tr("Topography / DEM Bitmap"), boxRaster);
    m_rbMapNone = new QRadioButton(tr("None (Grid Only)"), boxRaster);

    m_grpMapRaster->addButton(m_rbMapModeled, static_cast<int>(MapView::MapRasterMode::Modeled));
    m_grpMapRaster->addButton(m_rbMapObserved, static_cast<int>(MapView::MapRasterMode::Observed));
    m_grpMapRaster->addButton(m_rbMapDiff, static_cast<int>(MapView::MapRasterMode::Difference));
    m_grpMapRaster->addButton(m_rbMapRelief, static_cast<int>(MapView::MapRasterMode::Relief));
    m_grpMapRaster->addButton(m_rbMapNone, static_cast<int>(MapView::MapRasterMode::None));

    rasterLayout->addWidget(m_rbMapModeled);
    rasterLayout->addWidget(m_rbMapObserved);
    rasterLayout->addWidget(m_rbMapDiff);
    rasterLayout->addWidget(m_rbMapRelief);
    rasterLayout->addWidget(m_rbMapNone);
    mvLayout->addWidget(boxRaster);

    connect(m_grpMapRaster, &QButtonGroup::idClicked, this, [this](int id) {
        if (m_blockSync) return;
        if (auto *mv = qobject_cast<MapView*>(m_activeView.data())) {
            mv->setRasterMode(static_cast<MapView::MapRasterMode>(id));
        }
    });

    // Map Overlays & Show Checks
    auto *boxMvOverlays = new QGroupBox(tr("Map Overlays & Show Checks"), m_paneMapFields);
    auto *mvOverlaysLayout = new QVBoxLayout(boxMvOverlays);
    mvOverlaysLayout->setContentsMargins(8, 8, 8, 8);
    mvOverlaysLayout->setSpacing(4);

    m_chkMapStationPoints = new QCheckBox(tr("Observation Station Points"), boxMvOverlays);
    m_chkMapStationPoints->setChecked(true);
    m_chkMapProfiles = new QCheckBox(tr("Profile Navigation Trackers"), boxMvOverlays);
    m_chkMapProfiles->setChecked(true);
    m_chkMapBodyFootprints = new QCheckBox(tr("Body Footprints / Contours"), boxMvOverlays);
    m_chkMapBodyFootprints->setChecked(true);
    m_chkMapReliefContours = new QCheckBox(tr("Topography / Relief Contours"), boxMvOverlays);
    m_chkMapReliefContours->setChecked(true);

    mvOverlaysLayout->addWidget(m_chkMapStationPoints);
    mvOverlaysLayout->addWidget(m_chkMapProfiles);
    mvOverlaysLayout->addWidget(m_chkMapBodyFootprints);
    mvOverlaysLayout->addWidget(m_chkMapReliefContours);
    mvLayout->addWidget(boxMvOverlays);

    connect(m_chkMapStationPoints, &QCheckBox::toggled, this, [this](bool c) {
        if (auto *mv = qobject_cast<MapView*>(m_activeView.data())) mv->setShowStationPoints(c);
    });
    connect(m_chkMapProfiles, &QCheckBox::toggled, this, [this](bool c) {
        if (auto *mv = qobject_cast<MapView*>(m_activeView.data())) mv->setShowProfiles(c);
    });
    connect(m_chkMapBodyFootprints, &QCheckBox::toggled, this, [this](bool c) {
        if (auto *mv = qobject_cast<MapView*>(m_activeView.data())) mv->setShowBodyFootprints(c);
    });
    connect(m_chkMapReliefContours, &QCheckBox::toggled, this, [this](bool c) {
        if (auto *mv = qobject_cast<MapView*>(m_activeView.data())) mv->setShowReliefContours(c);
    });

    layout->addWidget(m_paneMapFields);

    // --- Sub-panel 3: 3D View Controls ---
    m_pane3DView = new QWidget(inner);
    auto *v3dLayout = new QVBoxLayout(m_pane3DView);
    v3dLayout->setContentsMargins(0, 0, 0, 0);
    v3dLayout->setSpacing(6);

    auto *box3DPresets = new QGroupBox(tr("Standard Views"), m_pane3DView);
    auto *grid3D = new QGridLayout(box3DPresets);
    grid3D->setContentsMargins(6, 6, 6, 6);
    grid3D->setSpacing(4);

    m_btn3DTop = new QPushButton(tr("Top (M)"), box3DPresets);
    m_btn3DReset = new QPushButton(tr("Reset"), box3DPresets);
    m_btn3DEast = new QPushButton(tr("East (E)"), box3DPresets);
    m_btn3DWest = new QPushButton(tr("West (W)"), box3DPresets);
    m_btn3DNorth = new QPushButton(tr("North (N)"), box3DPresets);
    m_btn3DSouth = new QPushButton(tr("South (S)"), box3DPresets);

    grid3D->addWidget(m_btn3DTop, 0, 0);
    grid3D->addWidget(m_btn3DReset, 0, 1);
    grid3D->addWidget(m_btn3DEast, 1, 0);
    grid3D->addWidget(m_btn3DWest, 1, 1);
    grid3D->addWidget(m_btn3DNorth, 2, 0);
    grid3D->addWidget(m_btn3DSouth, 2, 1);
    v3dLayout->addWidget(box3DPresets);

    connect(m_btn3DTop, &QPushButton::clicked, this, [this]() {
        if (auto *glv = qobject_cast<GL3DView*>(m_activeView.data())) glv->setViewTopMap();
    });
    connect(m_btn3DReset, &QPushButton::clicked, this, [this]() {
        if (auto *glv = qobject_cast<GL3DView*>(m_activeView.data())) glv->resetCamera();
    });
    connect(m_btn3DEast, &QPushButton::clicked, this, [this]() {
        if (auto *glv = qobject_cast<GL3DView*>(m_activeView.data())) glv->setViewEast();
    });
    connect(m_btn3DWest, &QPushButton::clicked, this, [this]() {
        if (auto *glv = qobject_cast<GL3DView*>(m_activeView.data())) glv->setViewWest();
    });
    connect(m_btn3DNorth, &QPushButton::clicked, this, [this]() {
        if (auto *glv = qobject_cast<GL3DView*>(m_activeView.data())) glv->setViewNorth();
    });
    connect(m_btn3DSouth, &QPushButton::clicked, this, [this]() {
        if (auto *glv = qobject_cast<GL3DView*>(m_activeView.data())) glv->setViewSouth();
    });

    auto *box3DToggles = new QGroupBox(tr("3D Overlays"), m_pane3DView);
    auto *toggles3DLayout = new QVBoxLayout(box3DToggles);
    toggles3DLayout->setContentsMargins(8, 8, 8, 8);
    toggles3DLayout->setSpacing(4);

    m_chk3DSlices = new QCheckBox(tr("Profile Slices (Open Views)"), box3DToggles);
    m_chk3DSlices->setChecked(true);
    m_chk3DCompass = new QCheckBox(tr("Orientation Compass Triad"), box3DToggles);
    m_chk3DCompass->setChecked(true);
    m_chk3DBoundingBox = new QCheckBox(tr("Bounding Box"), box3DToggles);
    m_chk3DBoundingBox->setChecked(true);
    m_chk3DFeatureEdges = new QCheckBox(tr("Feature Edges"), box3DToggles);
    m_chk3DFeatureEdges->setChecked(true);
    m_chk3DRelief = new QCheckBox(tr("Relief Wireframe"), box3DToggles);
    m_chk3DRelief->setChecked(true);
    m_chk3DWireframe = new QCheckBox(tr("Wireframe Mode"), box3DToggles);
    m_chk3DOrtho = new QCheckBox(tr("Orthographic Projection"), box3DToggles);
    m_chk3DDepthFog = new QCheckBox(tr("Depth Cueing / Atmospheric Fog"), box3DToggles);
    m_chk3DDepthFog->setChecked(false);
    m_chk3DTransparentBodies = new QCheckBox(tr("Transparent Bodies"), box3DToggles);
    m_chk3DTransparentBodies->setChecked(false);
    m_chk3DTransparentSlices = new QCheckBox(tr("Transparent Profile / Section Slices"), box3DToggles);
    m_chk3DTransparentSlices->setChecked(true);

    toggles3DLayout->addWidget(m_chk3DSlices);
    toggles3DLayout->addWidget(m_chk3DCompass);
    toggles3DLayout->addWidget(m_chk3DBoundingBox);
    toggles3DLayout->addWidget(m_chk3DFeatureEdges);
    toggles3DLayout->addWidget(m_chk3DRelief);
    toggles3DLayout->addWidget(m_chk3DWireframe);
    toggles3DLayout->addWidget(m_chk3DOrtho);
    toggles3DLayout->addWidget(m_chk3DDepthFog);
    toggles3DLayout->addWidget(m_chk3DTransparentBodies);
    toggles3DLayout->addWidget(m_chk3DTransparentSlices);
    v3dLayout->addWidget(box3DToggles);

    auto *box3DDepth = new QGroupBox(tr("Depth & Elevation Scaling"), m_pane3DView);
    auto *depthLayout = new QHBoxLayout(box3DDepth);
    depthLayout->setContentsMargins(8, 8, 8, 8);
    auto *lblZScale = new QLabel(tr("Z-Exaggeration:"), box3DDepth);
    m_spin3DZScale = new QDoubleSpinBox(box3DDepth);
    m_spin3DZScale->setRange(0.1, 30.0);
    m_spin3DZScale->setSingleStep(0.5);
    m_spin3DZScale->setValue(1.0);
    m_spin3DZScale->setSuffix("x");
    depthLayout->addWidget(lblZScale);
    depthLayout->addWidget(m_spin3DZScale);
    v3dLayout->addWidget(box3DDepth);

    connect(m_chk3DSlices, &QCheckBox::toggled, this, [this](bool c) {
        if (auto *glv = qobject_cast<GL3DView*>(m_activeView.data())) glv->setShowProfilePlane(c);
    });
    connect(m_chk3DCompass, &QCheckBox::toggled, this, [this](bool c) {
        if (auto *glv = qobject_cast<GL3DView*>(m_activeView.data())) glv->setShowOrientationIndicator(c);
    });
    connect(m_chk3DBoundingBox, &QCheckBox::toggled, this, [this](bool c) {
        if (auto *glv = qobject_cast<GL3DView*>(m_activeView.data())) glv->setShowBoundingBox(c);
    });
    connect(m_chk3DFeatureEdges, &QCheckBox::toggled, this, [this](bool c) {
        if (auto *glv = qobject_cast<GL3DView*>(m_activeView.data())) glv->setShowFeatureEdges(c);
    });
    connect(m_chk3DRelief, &QCheckBox::toggled, this, [this](bool c) {
        if (auto *glv = qobject_cast<GL3DView*>(m_activeView.data())) glv->setShowRelief(c);
    });
    connect(m_chk3DWireframe, &QCheckBox::toggled, this, [this](bool c) {
        if (auto *glv = qobject_cast<GL3DView*>(m_activeView.data())) glv->setWireframeMode(c);
    });
    connect(m_chk3DOrtho, &QCheckBox::toggled, this, [this](bool c) {
        if (auto *glv = qobject_cast<GL3DView*>(m_activeView.data())) glv->setOrthographic(c);
    });
    connect(m_chk3DDepthFog, &QCheckBox::toggled, this, [this](bool c) {
        if (auto *glv = qobject_cast<GL3DView*>(m_activeView.data())) glv->setShowDepthFog(c);
    });
    connect(m_chk3DTransparentBodies, &QCheckBox::toggled, this, [this](bool c) {
        if (auto *glv = qobject_cast<GL3DView*>(m_activeView.data())) glv->setShowTransparentBodies(c);
    });
    connect(m_chk3DTransparentSlices, &QCheckBox::toggled, this, [this](bool c) {
        if (auto *glv = qobject_cast<GL3DView*>(m_activeView.data())) glv->setShowTransparentProfilePlane(c);
    });
    connect(m_spin3DZScale, &QDoubleSpinBox::valueChanged, this, [this](double val) {
        if (auto *glv = qobject_cast<GL3DView*>(m_activeView.data())) glv->setZScale(static_cast<float>(val));
    });

    layout->addWidget(m_pane3DView);
    layout->addStretch(1);

    scroll->setWidget(inner);
    auto *tabLayout = new QVBoxLayout(container);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->addWidget(scroll);
}

// ============================================================================
// TAB 2: Profiles Tab
// ============================================================================
void SettingsPane::setupProfilesTab(QWidget *container) {
    auto *scroll = new QScrollArea(container);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *inner = new QWidget(scroll);
    auto *layout = new QVBoxLayout(inner);
    layout->setContentsMargins(6, 8, 6, 8);
    layout->setSpacing(6);
    layout->setAlignment(Qt::AlignTop);

    // 1. Profile Orientation
    auto *boxOrient = new QGroupBox(tr("Profile Orientation"), inner);
    auto *orientLayout = new QHBoxLayout(boxOrient);
    orientLayout->setContentsMargins(8, 8, 8, 8);
    orientLayout->setSpacing(4);

    m_btnProfTabOrientEW = new QPushButton(tr("E-W (H)"), boxOrient);
    m_btnProfTabOrientEW->setCheckable(true);
    m_btnProfTabOrientSN = new QPushButton(tr("S-N (V)"), boxOrient);
    m_btnProfTabOrientSN->setCheckable(true);
    m_btnProfTabOrientEW->setChecked(true);

    auto *grpOrient = new QButtonGroup(boxOrient);
    grpOrient->addButton(m_btnProfTabOrientEW);
    grpOrient->addButton(m_btnProfTabOrientSN);

    orientLayout->addWidget(m_btnProfTabOrientEW);
    orientLayout->addWidget(m_btnProfTabOrientSN);
    layout->addWidget(boxOrient);

    connect(m_btnProfTabOrientEW, &QPushButton::clicked, this, [this]() {
        if (auto *pv = qobject_cast<ProfileView*>(m_activeView.data())) {
            pv->setProfileOrientation(ProfileOrientation::EastWest);
        } else {
            m_doc->setProfileOrientation(ProfileOrientation::EastWest);
        }
    });
    connect(m_btnProfTabOrientSN, &QPushButton::clicked, this, [this]() {
        if (auto *pv = qobject_cast<ProfileView*>(m_activeView.data())) {
            pv->setProfileOrientation(ProfileOrientation::SouthNorth);
        } else {
            m_doc->setProfileOrientation(ProfileOrientation::SouthNorth);
        }
    });

    // 2. Profile Navigation & Position
    auto *boxNav = new QGroupBox(tr("Profile Navigation"), inner);
    auto *navLayout = new QVBoxLayout(boxNav);
    navLayout->setContentsMargins(8, 8, 8, 8);
    navLayout->setSpacing(5);

    auto *idxRow = new QHBoxLayout();
    m_btnProfTabPrev = new QPushButton(tr("◀ Prev (P)"), boxNav);
    m_spinProfTabIdx = new QSpinBox(boxNav);
    m_spinProfTabIdx->setPrefix(tr("Prof: "));
    m_spinProfTabIdx->setRange(0, 100);
    m_btnProfTabNext = new QPushButton(tr("Next (N) ▶"), boxNav);
    idxRow->addWidget(m_btnProfTabPrev);
    idxRow->addWidget(m_spinProfTabIdx);
    idxRow->addWidget(m_btnProfTabNext);
    navLayout->addLayout(idxRow);

    connect(m_btnProfTabPrev, &QPushButton::clicked, this, [this]() {
        if (auto *pv = qobject_cast<ProfileView*>(m_activeView.data())) pv->moveProfile(-1);
        else m_doc->moveProfile(-1);
    });
    connect(m_btnProfTabNext, &QPushButton::clicked, this, [this]() {
        if (auto *pv = qobject_cast<ProfileView*>(m_activeView.data())) pv->moveProfile(1);
        else m_doc->moveProfile(1);
    });
    connect(m_spinProfTabIdx, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        if (m_blockSync) return;
        if (auto *pv = qobject_cast<ProfileView*>(m_activeView.data())) pv->setProfileIndex(val);
        else m_doc->setProfileIndex(val);
    });

    m_lblProfTabCoord = new QLabel(tr("Y = 0.0 m"), boxNav);
    m_lblProfTabCoord->setAlignment(Qt::AlignCenter);
    m_lblProfTabCoord->setStyleSheet("font-weight: bold; font-family: Menlo, Consolas, 'Courier New'; color: #1e293b; background: #f1f5f9; padding: 4px; border-radius: 4px;");
    navLayout->addWidget(m_lblProfTabCoord);

    // Jump Buttons
    auto *jumpRow = new QHBoxLayout();
    m_btnJumpMin = new QPushButton(tr("Min (I)"), boxNav);
    m_btnJumpMax = new QPushButton(tr("Max (A)"), boxNav);
    m_btnJumpExtremum = new QPushButton(tr("Extremum (X)"), boxNav);
    jumpRow->addWidget(m_btnJumpMin);
    jumpRow->addWidget(m_btnJumpMax);
    jumpRow->addWidget(m_btnJumpExtremum);
    navLayout->addLayout(jumpRow);

    connect(m_btnJumpMin, &QPushButton::clicked, this, [this]() {
        if (auto *pv = qobject_cast<ProfileView*>(m_activeView.data())) pv->jumpToMin();
        else m_doc->jumpToMin();
    });
    connect(m_btnJumpMax, &QPushButton::clicked, this, [this]() {
        if (auto *pv = qobject_cast<ProfileView*>(m_activeView.data())) pv->jumpToMax();
        else m_doc->jumpToMax();
    });
    connect(m_btnJumpExtremum, &QPushButton::clicked, this, [this]() {
        if (auto *pv = qobject_cast<ProfileView*>(m_activeView.data())) pv->jumpToExtreme();
        else m_doc->jumpToExtreme();
    });

    m_lblProfGeometryInfo = new QLabel(tr("Spacing: 0 m | Length: 0 m | Profiles: 0"), boxNav);
    m_lblProfGeometryInfo->setStyleSheet("color: #64748b; font-size: 11px;");
    m_lblProfGeometryInfo->setAlignment(Qt::AlignCenter);
    navLayout->addWidget(m_lblProfGeometryInfo);

    layout->addWidget(boxNav);

    // 3. Profile Show Checks
    auto *boxChecks = new QGroupBox(tr("Profile Show Checks"), inner);
    auto *chkLayout = new QVBoxLayout(boxChecks);
    chkLayout->setContentsMargins(8, 8, 8, 8);
    chkLayout->setSpacing(4);

    m_chkProfTabGrid = new QCheckBox(tr("Grid Lines"), boxChecks);
    m_chkProfTabGrid->setChecked(true);
    m_chkProfTabStationPts = new QCheckBox(tr("Observation Points"), boxChecks);
    m_chkProfTabStationPts->setChecked(true);
    m_chkProfTabRelief = new QCheckBox(tr("Relief Profile / DEM Surface"), boxChecks);
    m_chkProfTabRelief->setChecked(true);
    m_chkProfTabIntersects = new QCheckBox(tr("Profile Intersections"), boxChecks);
    m_chkProfTabIntersects->setChecked(true);
    m_chkProfTabGhostPrev = new QCheckBox(tr("Ghost Previous Profile (-1)"), boxChecks);
    m_chkProfTabGhostNext = new QCheckBox(tr("Ghost Next Profile (+1)"), boxChecks);

    chkLayout->addWidget(m_chkProfTabGrid);
    chkLayout->addWidget(m_chkProfTabStationPts);
    chkLayout->addWidget(m_chkProfTabRelief);
    chkLayout->addWidget(m_chkProfTabIntersects);
    chkLayout->addWidget(m_chkProfTabGhostPrev);
    chkLayout->addWidget(m_chkProfTabGhostNext);
    layout->addWidget(boxChecks);

    auto updateFromTabChecks = [this]() {
        if (m_blockSync) return;
        if (auto *pv = qobject_cast<ProfileView*>(m_activeView.data())) {
            pv->setShowGridLines(m_chkProfTabGrid->isChecked());
            pv->setShowObservationPoints(m_chkProfTabStationPts->isChecked());
            pv->setShowRelief(m_chkProfTabRelief->isChecked());
            pv->setShowProfileIntersections(m_chkProfTabIntersects->isChecked());
            pv->setShowPreviousProfile(m_chkProfTabGhostPrev->isChecked());
            pv->setShowNextProfile(m_chkProfTabGhostNext->isChecked());
        }
        m_blockSync = true;
        m_chkProfGrid->setChecked(m_chkProfTabGrid->isChecked());
        m_chkProfShowStationPts->setChecked(m_chkProfTabStationPts->isChecked());
        m_chkProfShowRelief->setChecked(m_chkProfTabRelief->isChecked());
        m_chkProfShowIntersects->setChecked(m_chkProfTabIntersects->isChecked());
        m_chkProfGhostPrev->setChecked(m_chkProfTabGhostPrev->isChecked());
        m_chkProfGhostNext->setChecked(m_chkProfTabGhostNext->isChecked());
        m_blockSync = false;
    };
    connect(m_chkProfTabGrid, &QCheckBox::toggled, this, updateFromTabChecks);
    connect(m_chkProfTabStationPts, &QCheckBox::toggled, this, updateFromTabChecks);
    connect(m_chkProfTabRelief, &QCheckBox::toggled, this, updateFromTabChecks);
    connect(m_chkProfTabIntersects, &QCheckBox::toggled, this, updateFromTabChecks);
    connect(m_chkProfTabGhostPrev, &QCheckBox::toggled, this, updateFromTabChecks);
    connect(m_chkProfTabGhostNext, &QCheckBox::toggled, this, updateFromTabChecks);

    // 4. Profile Operations & Tools
    auto *boxTools = new QGroupBox(tr("Profile Operations"), inner);
    auto *toolsLayout = new QVBoxLayout(boxTools);
    toolsLayout->setContentsMargins(8, 8, 8, 8);
    toolsLayout->setSpacing(4);

    m_btnCopyBodyPrev = new QPushButton(tr("📋 Copy Body to Prev Profile"), boxTools);
    m_btnCopyBodyNext = new QPushButton(tr("📋 Copy Body to Next Profile"), boxTools);
    m_btnOpenPerpendicular = new QPushButton(tr("📐 Open Perpendicular Profile"), boxTools);

    toolsLayout->addWidget(m_btnCopyBodyPrev);
    toolsLayout->addWidget(m_btnCopyBodyNext);
    toolsLayout->addWidget(m_btnOpenPerpendicular);
    layout->addWidget(boxTools);

    connect(m_btnCopyBodyPrev, &QPushButton::clicked, this, [this]() {
        int bId = m_comboBodies->currentData().toInt();
        if (bId < 0 && !m_doc->model().bodies().empty()) {
            bId = m_doc->model().bodies().front()->id();
        }
        if (bId >= 0) {
            if (auto *pv = qobject_cast<ProfileView*>(m_activeView.data())) {
                m_doc->copyBodyBetweenProfiles(bId, pv->profileOrientation(), pv->profileIndex(), pv->profileIndex() - 1);
            } else {
                m_doc->copyBodyToPreviousProfile(bId);
            }
        }
    });

    connect(m_btnCopyBodyNext, &QPushButton::clicked, this, [this]() {
        int bId = m_comboBodies->currentData().toInt();
        if (bId < 0 && !m_doc->model().bodies().empty()) {
            bId = m_doc->model().bodies().front()->id();
        }
        if (bId >= 0) {
            if (auto *pv = qobject_cast<ProfileView*>(m_activeView.data())) {
                m_doc->copyBodyBetweenProfiles(bId, pv->profileOrientation(), pv->profileIndex(), pv->profileIndex() + 1);
            } else {
                m_doc->copyBodyToNextProfile(bId);
            }
        }
    });

    connect(m_btnOpenPerpendicular, &QPushButton::clicked, this, [this]() {
        if (auto *pv = qobject_cast<ProfileView*>(m_activeView.data())) {
            auto perpOrient = (pv->profileOrientation() == ProfileOrientation::EastWest)
                ? ProfileOrientation::SouthNorth : ProfileOrientation::EastWest;
            m_mainWindow->openProfileView(perpOrient, -1);
        } else {
            auto perpOrient = (m_doc->profileOrientation() == ProfileOrientation::EastWest)
                ? ProfileOrientation::SouthNorth : ProfileOrientation::EastWest;
            m_mainWindow->openProfileView(perpOrient, -1);
        }
    });

    layout->addStretch(1);

    scroll->setWidget(inner);
    auto *tabLayout = new QVBoxLayout(container);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->addWidget(scroll);
}

// ============================================================================
// TAB 3: Geological Body Editor Tab
// ============================================================================
void SettingsPane::setupBodyEditorTab(QWidget *container) {
    auto *scroll = new QScrollArea(container);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *inner = new QWidget(scroll);
    auto *layout = new QVBoxLayout(inner);
    layout->setContentsMargins(6, 8, 6, 8);
    layout->setSpacing(6);
    layout->setAlignment(Qt::AlignTop);

    m_comboBodies = new QComboBox(inner);
    layout->addWidget(m_comboBodies);

    auto *form = new QFormLayout();
    form->setContentsMargins(0, 0, 0, 0);

    m_editBodyName = new QLineEdit(inner);
    form->addRow(tr("Name:"), m_editBodyName);

    m_spinDensity = new QDoubleSpinBox(inner);
    m_spinDensity->setRange(-10000.0, 10000.0);
    m_spinDensity->setSingleStep(50.0);
    m_spinDensity->setDecimals(1);
    m_spinDensity->setSuffix(tr(" kg/m³"));
    form->addRow(tr("Density:"), m_spinDensity);

    m_spinSusceptibility = new QDoubleSpinBox(inner);
    m_spinSusceptibility->setRange(-10.0, 10.0);
    m_spinSusceptibility->setSingleStep(0.001);
    m_spinSusceptibility->setDecimals(5);
    m_spinSusceptibility->setSuffix(tr(" SI"));
    form->addRow(tr("Susceptibility:"), m_spinSusceptibility);

    m_btnBodyColor = new QPushButton(tr("Select Color..."), inner);
    form->addRow(tr("Color:"), m_btnBodyColor);

    auto *chkRow = new QHBoxLayout();
    m_chkBodyActive = new QCheckBox(tr("Active in Compute"), inner);
    m_chkBodyLocked = new QCheckBox(tr("Locked"), inner);
    m_chkBodyTransparent = new QCheckBox(tr("Transparent in 3D"), inner);
    chkRow->addWidget(m_chkBodyActive);
    chkRow->addWidget(m_chkBodyLocked);
    chkRow->addWidget(m_chkBodyTransparent);
    form->addRow(QString(), chkRow);

    layout->addLayout(form);

    m_btnContinuousDrawing = new QPushButton(tr("✏ Move Upper Boundary (Drawing Mode)"), inner);
    m_btnContinuousDrawing->setCheckable(true);
    m_btnContinuousDrawing->setStyleSheet("QPushButton:checked { background-color: #fef08a; font-weight: bold; }");
    layout->addWidget(m_btnContinuousDrawing);

    connect(m_btnContinuousDrawing, &QPushButton::toggled, this, [this](bool checked) {
        if (auto *pv = qobject_cast<ProfileView*>(m_activeView.data())) {
            pv->setContinuousDrawingMode(checked);
        }
    });

    // Advanced Density Gradient Groupbox
    auto *boxGrad = new QGroupBox(tr("Linear Density Gradient (∇ρ)"), inner);
    auto *formGrad = new QFormLayout(boxGrad);
    formGrad->setContentsMargins(6, 6, 6, 6);

    m_spinGradX = new QDoubleSpinBox(boxGrad);
    m_spinGradX->setRange(-100.0, 100.0);
    m_spinGradX->setDecimals(4);
    formGrad->addRow(tr("Grad X (kg/m⁴):"), m_spinGradX);

    m_spinGradY = new QDoubleSpinBox(boxGrad);
    m_spinGradY->setRange(-100.0, 100.0);
    m_spinGradY->setDecimals(4);
    formGrad->addRow(tr("Grad Y (kg/m⁴):"), m_spinGradY);

    m_spinGradZ = new QDoubleSpinBox(boxGrad);
    m_spinGradZ->setRange(-100.0, 100.0);
    m_spinGradZ->setDecimals(4);
    formGrad->addRow(tr("Grad Z (kg/m⁴):"), m_spinGradZ);

    layout->addWidget(boxGrad);

    // Advanced Remanent Magnetization Groupbox
    auto *boxRem = new QGroupBox(tr("Remanent Magnetization (Jrem)"), inner);
    auto *formRem = new QFormLayout(boxRem);
    formRem->setContentsMargins(6, 6, 6, 6);

    m_spinRemJ = new QDoubleSpinBox(boxRem);
    m_spinRemJ->setRange(0.0, 1e6);
    m_spinRemJ->setSuffix(tr(" nT"));
    formRem->addRow(tr("Intensity J:"), m_spinRemJ);

    m_spinRemInc = new QDoubleSpinBox(boxRem);
    m_spinRemInc->setRange(-90.0, 90.0);
    m_spinRemInc->setSuffix(tr(" °"));
    formRem->addRow(tr("Inclination:"), m_spinRemInc);

    m_spinRemDec = new QDoubleSpinBox(boxRem);
    m_spinRemDec->setRange(0.0, 360.0);
    m_spinRemDec->setSuffix(tr(" °"));
    formRem->addRow(tr("Declination:"), m_spinRemDec);

    layout->addWidget(boxRem);

    // Body Action Buttons
    auto *btnRow1 = new QHBoxLayout();
    m_btnAddBody = new QPushButton(tr("+ New Body"), inner);
    m_btnDeleteBody = new QPushButton(tr("🗑 Delete"), inner);
    btnRow1->addWidget(m_btnAddBody);
    btnRow1->addWidget(m_btnDeleteBody);
    layout->addLayout(btnRow1);

    auto *btnRow2 = new QHBoxLayout();
    m_btnBodyProperties = new QPushButton(tr("⚙ Advanced Properties..."), inner);
    m_btnInvertDensity = new QPushButton(tr("⚡ Invert Density"), inner);
    btnRow2->addWidget(m_btnBodyProperties);
    btnRow2->addWidget(m_btnInvertDensity);
    layout->addLayout(btnRow2);

    layout->addStretch(1);

    scroll->setWidget(inner);
    auto *tabLayout = new QVBoxLayout(container);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->addWidget(scroll);

    connect(m_comboBodies, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (!m_blockSync) loadBodyData();
    });

    connect(m_editBodyName, &QLineEdit::editingFinished, this, &SettingsPane::applyBodyData);
    connect(m_spinDensity, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) {
        applyBodyData();
    });
    connect(m_spinSusceptibility, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) {
        applyBodyData();
    });
    connect(m_chkBodyActive, &QCheckBox::toggled, this, &SettingsPane::applyBodyData);
    connect(m_chkBodyLocked, &QCheckBox::toggled, this, &SettingsPane::applyBodyData);
    connect(m_chkBodyTransparent, &QCheckBox::toggled, this, &SettingsPane::applyBodyData);
    connect(m_spinGradX, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) {
        applyBodyData();
    });
    connect(m_spinGradY, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) {
        applyBodyData();
    });
    connect(m_spinGradZ, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) {
        applyBodyData();
    });
    connect(m_spinRemJ, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) {
        applyBodyData();
    });
    connect(m_spinRemInc, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) {
        applyBodyData();
    });
    connect(m_spinRemDec, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) {
        applyBodyData();
    });

    connect(m_btnBodyColor, &QPushButton::clicked, this, [this]() {
        int bId = m_comboBodies->currentData().toInt();
        auto *b = m_doc->model().body(bId);
        if (!b) return;
        QColor curCol(b->color().r, b->color().g, b->color().b);
        QColor chosen = QColorDialog::getColor(curCol, this, tr("Select Body Color"));
        if (chosen.isValid()) {
            b->set_color(mod3d::BodyColor(chosen.red(), chosen.green(), chosen.blue()));
            m_doc->setModified(true);
            m_doc->computeDelta({});
            updateBodyList();
            loadBodyData();
        }
    });

    connect(m_btnAddBody, &QPushButton::clicked, this, [this]() {
        auto *newB = m_doc->createNewBody();
        if (newB) {
            updateBodyList();
            int idx = m_comboBodies->findData(newB->id());
            if (idx >= 0) m_comboBodies->setCurrentIndex(idx);
            loadBodyData();
        }
    });

    connect(m_btnDeleteBody, &QPushButton::clicked, this, [this]() {
        int bId = m_comboBodies->currentData().toInt();
        if (bId < 0) return;
        auto res = QMessageBox::question(this, tr("Delete Body"),
            tr("Are you sure you want to delete Body %1?").arg(bId),
            QMessageBox::Yes | QMessageBox::No);
        if (res == QMessageBox::Yes) {
            m_doc->model().delete_body(bId);
            m_doc->setModified(true);
            m_doc->computeAll();
            updateBodyList();
            loadBodyData();
        }
    });

    connect(m_btnBodyProperties, &QPushButton::clicked, this, [this]() {
        int bId = m_comboBodies->currentData().toInt();
        if (bId >= 0) {
            emit requestBodyPropertiesDialog(bId);
        }
    });

    connect(m_btnInvertDensity, &QPushButton::clicked, this, [this]() {
        int bId = m_comboBodies->currentData().toInt();
        auto *b = m_doc->model().body(bId);
        if (!b) return;
        std::vector<mod3d::Facet3Pt> facets;
        m_doc->model().facets_computation(facets);
        m_doc->observation().fit_density(b, facets, m_doc->activeField());
        m_doc->computeAll();
        loadBodyData();
    });
}

// ============================================================================
// TAB 4: Physics & Model Parameters Tab
// ============================================================================
void SettingsPane::setupPhysicsTab(QWidget *container) {
    auto *scroll = new QScrollArea(container);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *inner = new QWidget(scroll);
    auto *layout = new QVBoxLayout(inner);
    layout->setContentsMargins(6, 8, 6, 8);
    layout->setSpacing(6);
    layout->setAlignment(Qt::AlignTop);

    // 1. Geomagnetic Inducing Field
    auto *boxInd = new QGroupBox(tr("Geomagnetic Inducing Field (T0, I0, D0)"), inner);
    auto *formInd = new QFormLayout(boxInd);
    formInd->setContentsMargins(8, 8, 8, 8);

    m_spinIndT0 = new QDoubleSpinBox(boxInd);
    m_spinIndT0->setRange(0.0, 100000.0);
    m_spinIndT0->setSuffix(tr(" nT"));
    formInd->addRow(tr("Intensity (T0):"), m_spinIndT0);

    m_spinIndInc = new QDoubleSpinBox(boxInd);
    m_spinIndInc->setRange(-90.0, 90.0);
    m_spinIndInc->setSuffix(tr(" °"));
    formInd->addRow(tr("Inclination (I0):"), m_spinIndInc);

    m_spinIndDec = new QDoubleSpinBox(boxInd);
    m_spinIndDec->setRange(0.0, 360.0);
    m_spinIndDec->setSuffix(tr(" °"));
    formInd->addRow(tr("Declination (D0):"), m_spinIndDec);

    layout->addWidget(boxInd);

    // 2. Sensor Heights & Density Reduction
    auto *boxSens = new QGroupBox(tr("Sensors && Reduction"), inner);
    auto *formSens = new QFormLayout(boxSens);
    formSens->setContentsMargins(8, 8, 8, 8);

    m_spinSensorGrvHeight = new QDoubleSpinBox(boxSens);
    m_spinSensorGrvHeight->setRange(0.0, 50000.0);
    m_spinSensorGrvHeight->setSuffix(tr(" m"));
    formSens->addRow(tr("Gravity Sensor Height:"), m_spinSensorGrvHeight);

    m_spinSensorMagHeight = new QDoubleSpinBox(boxSens);
    m_spinSensorMagHeight->setRange(0.0, 50000.0);
    m_spinSensorMagHeight->setSuffix(tr(" m"));
    formSens->addRow(tr("Magnetics Sensor Height:"), m_spinSensorMagHeight);

    m_spinRefDensity = new QDoubleSpinBox(boxSens);
    m_spinRefDensity->setRange(0.0, 10000.0);
    m_spinRefDensity->setSuffix(tr(" kg/m³"));
    formSens->addRow(tr("Reduction Density:"), m_spinRefDensity);

    layout->addWidget(boxSens);

    // 3. Model Vertical Range & Dimensions
    auto *boxModel = new QGroupBox(tr("Model Vertical Limits (Hell && Heaven)"), inner);
    auto *formModel = new QFormLayout(boxModel);
    formModel->setContentsMargins(8, 8, 8, 8);

    m_spinModelZMax = new QDoubleSpinBox(boxModel);
    m_spinModelZMax->setRange(-1e6, 1e6);
    m_spinModelZMax->setSuffix(tr(" m"));
    formModel->addRow(tr("Top (Heaven / Z max):"), m_spinModelZMax);

    m_spinModelZMin = new QDoubleSpinBox(boxModel);
    m_spinModelZMin->setRange(-1e6, 1e6);
    m_spinModelZMin->setSuffix(tr(" m"));
    formModel->addRow(tr("Bottom (Hell / Z min):"), m_spinModelZMin);

    m_lblModelDimensions = new QLabel(boxModel);
    m_lblModelDimensions->setStyleSheet("color: #475569; font-size: 11px; background: #f8fafc; padding: 4px; border: 1px solid #e2e8f0; border-radius: 4px;");
    formModel->addRow(tr("Grid Info:"), m_lblModelDimensions);

    layout->addWidget(boxModel);

    // 4. Automated 1D Inversion (Fit1D)
    auto *boxFit = new QGroupBox(tr("Automated 1D Inversion (Fit1D)"), inner);
    auto *fitLayout = new QVBoxLayout(boxFit);
    fitLayout->setContentsMargins(8, 8, 8, 8);
    fitLayout->setSpacing(5);

    auto *algoRow = new QHBoxLayout();
    m_rbOptBrent = new QRadioButton(tr("Brent's Method"), boxFit);
    m_rbOptGolden = new QRadioButton(tr("Golden Section"), boxFit);
    m_rbOptBrent->setChecked(true);
    algoRow->addWidget(m_rbOptBrent);
    algoRow->addWidget(m_rbOptGolden);
    fitLayout->addLayout(algoRow);

    auto *targetRow = new QHBoxLayout();
    m_rbTargetRms = new QRadioButton(tr("Minimize RMS"), boxFit);
    m_rbTargetRoughness = new QRadioButton(tr("Minimize Roughness"), boxFit);
    m_rbTargetRms->setChecked(true);
    targetRow->addWidget(m_rbTargetRms);
    targetRow->addWidget(m_rbTargetRoughness);
    fitLayout->addLayout(targetRow);

    auto *formFit = new QFormLayout();
    m_spinFitTol = new QDoubleSpinBox(boxFit);
    m_spinFitTol->setRange(1e-8, 1e-1);
    m_spinFitTol->setDecimals(6);
    m_spinFitTol->setValue(1e-4);
    formFit->addRow(tr("Tolerance:"), m_spinFitTol);

    m_spinFitMaxIter = new QSpinBox(boxFit);
    m_spinFitMaxIter->setRange(5, 500);
    m_spinFitMaxIter->setValue(50);
    formFit->addRow(tr("Max Iterations:"), m_spinFitMaxIter);
    fitLayout->addLayout(formFit);

    auto *btnFitRow = new QHBoxLayout();
    m_btnFitActiveDensity = new QPushButton(tr("⚡ Fit Density"), boxFit);
    m_btnFit1DVertices = new QPushButton(tr("⚡ Fit Vertices"), boxFit);
    btnFitRow->addWidget(m_btnFitActiveDensity);
    btnFitRow->addWidget(m_btnFit1DVertices);
    fitLayout->addLayout(btnFitRow);

    layout->addWidget(boxFit);

    connect(m_btnFitActiveDensity, &QPushButton::clicked, this, [this]() {
        int bId = m_comboBodies->currentData().toInt();
        auto *b = m_doc->model().body(bId);
        if (!b) return;
        std::vector<mod3d::Facet3Pt> facets;
        m_doc->model().facets_computation(facets);
        auto method = m_rbOptBrent->isChecked() ? mod3d::FitMethod::Brent : mod3d::FitMethod::Golden;
        m_doc->observation().fit_density(b, facets, m_doc->activeField(), b->density().value(), m_spinFitTol->value(), method);
        m_doc->computeAll();
        loadBodyData();
    });

    connect(m_btnFit1DVertices, &QPushButton::clicked, this, [this]() {
        if (auto *pv = qobject_cast<ProfileView*>(m_activeView.data())) {
            int profIdx = pv->profileIndex();
            auto orient = pv->profileOrientation();
            int bId = m_comboBodies->currentData().toInt();
            auto method = m_rbOptBrent->isChecked() ? mod3d::FitMethod::Brent : mod3d::FitMethod::Golden;
            double tol = m_spinFitTol->value();

            int nLines = (orient == ProfileOrientation::EastWest) ? m_doc->model().cols() : m_doc->model().rows();
            for (int colOrRow = 1; colOrRow <= nLines; ++colOrRow) {
                int r = (orient == ProfileOrientation::EastWest) ? (profIdx + 1) : colOrRow;
                int c = (orient == ProfileOrientation::EastWest) ? colOrRow : (profIdx + 1);
                int count = m_doc->model().count(r, c);
                for (int vi = 0; vi < count; ++vi) {
                    auto *cp = m_doc->model().at(r, c, vi);
                    if (cp && cp->body_id() == bId) {
                        m_doc->observation().fit_vertex(m_doc->model(), r, c, vi, m_doc->activeField(), tol, method);
                    }
                }
            }
            m_doc->setModified(true);
            m_doc->computeAll();
        }
    });

    layout->addStretch(1);

    scroll->setWidget(inner);
    auto *tabLayout = new QVBoxLayout(container);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->addWidget(scroll);

    // Sync input events
    auto onPhysicsChanged = [this]() {
        applyPhysicsData();
    };
    connect(m_spinIndT0, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, onPhysicsChanged);
    connect(m_spinIndInc, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, onPhysicsChanged);
    connect(m_spinIndDec, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, onPhysicsChanged);
    connect(m_spinSensorGrvHeight, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, onPhysicsChanged);
    connect(m_spinSensorMagHeight, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, onPhysicsChanged);
    connect(m_spinRefDensity, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, onPhysicsChanged);
    connect(m_spinModelZMax, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, onPhysicsChanged);
    connect(m_spinModelZMin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, onPhysicsChanged);
}

void SettingsPane::setupComputeCard(QWidget *container) {
    m_grpCompute = new QGroupBox(tr("Computation && Status"), container);
    m_grpCompute->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto *layout = new QVBoxLayout(m_grpCompute);
    layout->setContentsMargins(8, 10, 8, 8);
    layout->setSpacing(5);
    layout->setAlignment(Qt::AlignTop);

    m_btnComputeAll = new QPushButton(tr("⚡ Compute All (F5)"), m_grpCompute);
    m_btnComputeAll->setStyleSheet("background-color: #2563eb; color: white; font-weight: bold; padding: 6px; border-radius: 4px;");
    layout->addWidget(m_btnComputeAll);

    connect(m_btnComputeAll, &QPushButton::clicked, this, [this]() {
        m_doc->computeAll();
    });

    auto *row = new QHBoxLayout();
    row->setContentsMargins(0, 0, 0, 0);
    m_chkRealTimeCompute = new QCheckBox(tr("Real-Time"), m_grpCompute);
    m_chkRealTimeCompute->setChecked(m_doc->isRealTimeCompute());
    row->addWidget(m_chkRealTimeCompute);

    connect(m_chkRealTimeCompute, &QCheckBox::toggled, this, [this](bool c) {
        m_doc->setRealTimeCompute(c);
    });

    m_lblRmsStatus = new QLabel(tr("RMS: 0.00 mGal | DRV: 0.00"), m_grpCompute);
    m_lblRmsStatus->setStyleSheet("font-size: 11px; color: #475569; font-family: Menlo, Consolas, 'Courier New'; font-weight: bold;");
    row->addWidget(m_lblRmsStatus, 0, Qt::AlignRight);
    layout->addLayout(row);
}

void SettingsPane::setActiveSubWindow(QMdiSubWindow *sub) {
    if (auto *oldPv = qobject_cast<ProfileView*>(m_activeView.data())) {
        disconnect(oldPv, &ProfileView::displaySettingsChanged, this, &SettingsPane::updateViewContext);
    }
    m_activeView = sub ? sub->widget() : nullptr;
    if (auto *newPv = qobject_cast<ProfileView*>(m_activeView.data())) {
        connect(newPv, &ProfileView::displaySettingsChanged, this, &SettingsPane::updateViewContext, Qt::UniqueConnection);
    }
    updateViewContext();
}

void SettingsPane::updateViewContext() {
    m_blockSync = true;

    if (auto *pv = qobject_cast<ProfileView*>(m_activeView.data())) {
        QString dirStr = (pv->profileOrientation() == ProfileOrientation::EastWest) ? "E-W" : "S-N";
        m_lblViewContext->setText(QString("Active View: Profile %1 [%2]").arg(pv->profileIndex() + 1).arg(dirStr));
        m_paneProfileFields->setVisible(true);
        m_paneMapFields->setVisible(false);
        m_pane3DView->setVisible(false);

        // Sync Profile View UI in Display Tab
        int idx = m_comboProfileMainField->findData(static_cast<int>(pv->mainFieldComponent()));
        if (idx >= 0) m_comboProfileMainField->setCurrentIndex(idx);
        m_lblProfileFieldInfo->setText(QString("Y-Axis: %1 (%2)")
            .arg(ModelDocument::componentName(pv->mainFieldComponent()))
            .arg(ModelDocument::componentUnit(pv->mainFieldComponent())));

        bool hasMeasured = pv->hasMeasuredField();

        m_chkProfModeled->setChecked(pv->showModeledCurve());

        m_chkProfMeasured->setEnabled(hasMeasured);
        m_chkProfMeasured->setChecked(hasMeasured && pv->showMeasuredCurve());
        m_chkProfMeasured->setToolTip(hasMeasured ? QString() : tr("No measured field available for %1").arg(ModelDocument::componentName(pv->mainFieldComponent())));

        m_chkProfDiff->setEnabled(hasMeasured);
        m_chkProfDiff->setChecked(hasMeasured && pv->showDifferenceCurve());
        m_chkProfDiff->setToolTip(hasMeasured ? QString() : tr("No measured field available for difference calculation"));

        m_chkProfSecondary->setChecked(pv->showSecondaryCurve());

        m_chkProfShowStationPts->setChecked(pv->showObservationPoints());
        m_chkProfGrid->setChecked(pv->showGridLines());
        m_chkProfShowRelief->setChecked(pv->showRelief());
        m_chkProfShowIntersects->setChecked(pv->showProfileIntersections());
        m_chkProfGhostPrev->setChecked(pv->showPreviousProfile());
        m_chkProfGhostNext->setChecked(pv->showNextProfile());

        // Sync Profiles Tab
        m_btnProfTabOrientEW->setChecked(pv->profileOrientation() == ProfileOrientation::EastWest);
        m_btnProfTabOrientSN->setChecked(pv->profileOrientation() == ProfileOrientation::SouthNorth);
        m_spinProfTabIdx->setMaximum(pv->maxProfileIndex());
        m_spinProfTabIdx->setValue(pv->profileIndex());

        double coord = (pv->profileOrientation() == ProfileOrientation::EastWest)
            ? m_doc->model().yd_val(pv->profileIndex() + 1)
            : m_doc->model().xd_val(pv->profileIndex() + 1);
        QString coordName = (pv->profileOrientation() == ProfileOrientation::EastWest) ? "Y" : "X";
        m_lblProfTabCoord->setText(QString("%1 = %2 m").arg(coordName).arg(coord, 0, 'f', 1));

        double spacing = (pv->profileOrientation() == ProfileOrientation::EastWest)
            ? m_doc->model().dy_val() : m_doc->model().dx_val();
        double len = (pv->profileOrientation() == ProfileOrientation::EastWest)
            ? (m_doc->model().x_max_val() - m_doc->model().x_min_val())
            : (m_doc->model().y_max_val() - m_doc->model().y_min_val());
        m_lblProfGeometryInfo->setText(QString("Spacing: %1 m | Length: %2 m | Total: %3")
            .arg(spacing, 0, 'f', 1).arg(len, 0, 'f', 1).arg(pv->maxProfileIndex() + 1));

        m_chkProfTabGrid->setChecked(pv->showGridLines());
        m_chkProfTabStationPts->setChecked(pv->showObservationPoints());
        m_chkProfTabRelief->setChecked(pv->showRelief());
        m_chkProfTabIntersects->setChecked(pv->showProfileIntersections());
        m_chkProfTabGhostPrev->setChecked(pv->showPreviousProfile());
        m_chkProfTabGhostNext->setChecked(pv->showNextProfile());

        m_btnContinuousDrawing->setChecked(pv->continuousDrawingMode());

    } else if (auto *mv = qobject_cast<MapView*>(m_activeView.data())) {
        m_lblViewContext->setText(tr("Active View: Map View"));
        m_paneProfileFields->setVisible(false);
        m_paneMapFields->setVisible(true);
        m_pane3DView->setVisible(false);

        int idx = m_comboMapField->findData(static_cast<int>(mv->fieldComponent()));
        if (idx >= 0) m_comboMapField->setCurrentIndex(idx);

        bool hasMapMeasured = mv->hasMeasuredField();
        m_rbMapObserved->setEnabled(hasMapMeasured);
        m_rbMapDiff->setEnabled(hasMapMeasured);
        m_rbMapObserved->setToolTip(hasMapMeasured ? QString() : tr("No measured field available for %1").arg(ModelDocument::componentName(mv->fieldComponent())));
        m_rbMapDiff->setToolTip(hasMapMeasured ? QString() : tr("No measured field available for difference calculation"));

        if (!hasMapMeasured && (mv->rasterMode() == MapView::MapRasterMode::Observed || mv->rasterMode() == MapView::MapRasterMode::Difference)) {
            mv->setRasterMode(MapView::MapRasterMode::Modeled);
        }
        if (auto *btn = m_grpMapRaster->button(static_cast<int>(mv->rasterMode()))) {
            btn->setChecked(true);
        }
        m_chkMapStationPoints->setChecked(mv->showStationPoints());
        m_chkMapBodyFootprints->setChecked(mv->showBodyFootprints());
        m_chkMapProfiles->setChecked(mv->showProfiles());
        m_chkMapReliefContours->setChecked(mv->showReliefContours());

    } else if (auto *glv = qobject_cast<GL3DView*>(m_activeView.data())) {
        m_lblViewContext->setText(tr("Active View: 3D View"));
        m_paneProfileFields->setVisible(false);
        m_paneMapFields->setVisible(false);
        m_pane3DView->setVisible(true);

        m_chk3DSlices->setChecked(glv->showProfilePlane());
        m_chk3DCompass->setChecked(glv->showOrientationIndicator());
        m_chk3DBoundingBox->setChecked(glv->showBoundingBox());
        m_chk3DFeatureEdges->setChecked(glv->showFeatureEdges());
        m_chk3DRelief->setChecked(glv->showRelief());
        m_chk3DWireframe->setChecked(glv->wireframeMode());
        m_chk3DOrtho->setChecked(glv->isOrthographic());
        m_chk3DDepthFog->setChecked(glv->showDepthFog());
        m_chk3DTransparentBodies->setChecked(glv->showTransparentBodies());
        m_chk3DTransparentSlices->setChecked(glv->showTransparentProfilePlane());
        m_spin3DZScale->setValue(glv->zScale());

    } else {
        m_lblViewContext->setText(tr("Active View: Workspace"));
        m_paneProfileFields->setVisible(true);
        m_paneMapFields->setVisible(false);
        m_pane3DView->setVisible(false);
    }

    m_blockSync = false;
}

void SettingsPane::updateBodyList() {
    m_blockSync = true;
    int prevBodyId = m_comboBodies->currentData().toInt();
    m_comboBodies->clear();

    const auto &bodies = m_doc->model().bodies();
    for (const auto &b : bodies) {
        QColor col(b->color().r, b->color().g, b->color().b);
        QString itemText = QString("Body %1: %2 (ρ=%3 kg/m³)").arg(b->id()).arg(QString::fromStdString(b->name())).arg(b->raw_density().value(), 0, 'f', 1);
        m_comboBodies->addItem(createColorIcon(col), itemText, b->id());
    }

    int idx = m_comboBodies->findData(prevBodyId);
    if (idx >= 0) {
        m_comboBodies->setCurrentIndex(idx);
    } else if (m_comboBodies->count() > 0) {
        m_comboBodies->setCurrentIndex(0);
    }
    m_blockSync = false;
}

void SettingsPane::loadBodyData() {
    int bId = m_comboBodies->currentData().toInt();
    if (m_mainWindow && m_mainWindow->mdiArea()) {
        for (auto *sub : m_mainWindow->mdiArea()->subWindowList()) {
            if (auto *glv = qobject_cast<GL3DView*>(sub->widget())) {
                glv->setSelectedBodyId(bId);
            }
        }
    }
    auto *b = m_doc->model().body(bId);
    if (!b) {
        m_editBodyName->clear();
        m_spinDensity->setValue(0.0);
        m_spinSusceptibility->setValue(0.0);
        m_btnBodyColor->setIcon(QIcon());
        m_spinGradX->setValue(0.0);
        m_spinGradY->setValue(0.0);
        m_spinGradZ->setValue(0.0);
        m_spinRemJ->setValue(0.0);
        m_spinRemInc->setValue(0.0);
        m_spinRemDec->setValue(0.0);
        return;
    }

    m_blockSync = true;
    m_editBodyName->setText(QString::fromStdString(b->name()));
    m_spinDensity->setValue(b->raw_density().value());
    m_spinSusceptibility->setValue(b->susceptibility());
    QColor col(b->color().r, b->color().g, b->color().b);
    m_btnBodyColor->setIcon(createColorIcon(col, 20));
    m_btnBodyColor->setText(QString("#%1%2%3")
        .arg(col.red(), 2, 16, QChar('0'))
        .arg(col.green(), 2, 16, QChar('0'))
        .arg(col.blue(), 2, 16, QChar('0')).toUpper());
    m_chkBodyActive->setChecked(b->is_active());
    m_chkBodyLocked->setChecked(b->is_locked());
    m_chkBodyTransparent->setChecked(b->is_transparent());

    m_spinGradX->setValue(b->density_gradient().x_val());
    m_spinGradY->setValue(b->density_gradient().y_val());
    m_spinGradZ->setValue(b->density_gradient().z_val());

    const auto &rem = b->remanent_magnetization();
    double jRem = rem.total_val();
    m_spinRemJ->setValue(jRem);
    double inc = (jRem > 1e-9) ? (std::asin(-rem.z_val() / jRem) * 180.0 / 3.141592653589793) : 0.0;
    double dec = (std::abs(rem.x_val()) > 1e-9 || std::abs(rem.y_val()) > 1e-9) ? (std::atan2(rem.x_val(), rem.y_val()) * 180.0 / 3.141592653589793) : 0.0;
    if (dec < 0.0) dec += 360.0;
    m_spinRemInc->setValue(inc);
    m_spinRemDec->setValue(dec);

    m_blockSync = false;
}

void SettingsPane::applyBodyData() {
    if (m_blockSync) return;
    int bId = m_comboBodies->currentData().toInt();
    auto *b = m_doc->model().body(bId);
    if (!b) return;

    b->set_name(m_editBodyName->text().toStdString());
    b->set_density(m_spinDensity->value());
    b->set_susceptibility(m_spinSusceptibility->value());
    b->set_active(m_chkBodyActive->isChecked());
    b->set_locked(m_chkBodyLocked->isChecked());
    b->set_transparent(m_chkBodyTransparent->isChecked());

    b->set_density_gradient(mod3d::Point3D(m_spinGradX->value(), m_spinGradY->value(), m_spinGradZ->value()));

    double j = m_spinRemJ->value();
    double incRad = m_spinRemInc->value() * 3.141592653589793 / 180.0;
    double decRad = m_spinRemDec->value() * 3.141592653589793 / 180.0;
    double mx = j * std::cos(incRad) * std::sin(decRad);
    double my = j * std::cos(incRad) * std::cos(decRad);
    double mz = -j * std::sin(incRad);
    b->set_remanent_magnetization(mod3d::Point3D(mx, my, mz));

    m_doc->setModified(true);

    if (m_doc->isRealTimeCompute()) {
        m_doc->computeAll();
    }
    m_doc->emitModelGeometryChanged();
}

void SettingsPane::loadPhysicsData() {
    m_blockSync = true;

    // Inducing Field
    m_spinIndT0->setValue(m_doc->observation().magnetic_intensity());
    m_spinIndInc->setValue(m_doc->observation().inclination_deg());
    m_spinIndDec->setValue(m_doc->observation().declination_deg());

    // Sensor Heights
    m_spinSensorGrvHeight->setValue(m_doc->observation().gravity_height());
    m_spinSensorMagHeight->setValue(m_doc->observation().magnetic_height());
    m_spinRefDensity->setValue(2670.0);

    // Model Boundaries
    m_spinModelZMax->setValue(m_doc->model().z_max_val());
    m_spinModelZMin->setValue(m_doc->model().z_min_val());

    const auto &m = m_doc->model();
    m_lblModelDimensions->setText(QString("%1 × %2 cells (%3 × %4 m)\nX: [%5, %6] m\nY: [%7, %8] m")
        .arg(m.rows()).arg(m.cols())
        .arg(m.dx_val(), 0, 'f', 1).arg(m.dy_val(), 0, 'f', 1)
        .arg(m.x_min_val(), 0, 'f', 1).arg(m.x_max_val(), 0, 'f', 1)
        .arg(m.y_min_val(), 0, 'f', 1).arg(m.y_max_val(), 0, 'f', 1));

    m_blockSync = false;
}

void SettingsPane::applyPhysicsData() {
    if (m_blockSync) return;

    m_doc->observation().set_ambient_field_params(m_spinIndInc->value(), m_spinIndDec->value(), m_spinIndT0->value());
    m_doc->observation().set_gravity_observation(mod3d::ObservationMode::SensorHeight, m_spinSensorGrvHeight->value());
    m_doc->observation().set_magnetic_observation(mod3d::ObservationMode::SensorHeight, m_spinSensorMagHeight->value());

    m_doc->model().set_z_max(m_spinModelZMax->value());
    m_doc->model().set_z_min(m_spinModelZMin->value());

    m_doc->setModified(true);
    if (m_doc->isRealTimeCompute()) {
        m_doc->computeAll();
    }
}

void SettingsPane::refreshAll() {
    updateViewContext();
    updateBodyList();
    loadBodyData();
    loadPhysicsData();
}

} // namespace mod3d_gui
