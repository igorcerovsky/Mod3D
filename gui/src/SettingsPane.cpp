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
    setMinimumWidth(320);
    toggleViewAction()->setText(tr("Settings && Inspector Pane"));

    setupUi();

    connect(m_doc, &ModelDocument::modelGeometryChanged, this, [this]() {
        updateBodyList();
        loadBodyData();
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
    connect(m_doc, &ModelDocument::profileChanged, this, [this](int idx, ProfileOrientation orient) {
        if (!m_blockSync) {
            m_blockSync = true;
            m_spinProfileIdx->setValue(idx);
            m_btnOrientEW->setChecked(orient == ProfileOrientation::EastWest);
            m_btnOrientSN->setChecked(orient == ProfileOrientation::SouthNorth);
            m_blockSync = false;
        }
    });

    refreshAll();
}

void SettingsPane::selectFieldsTab() {
    if (m_tabs) m_tabs->setCurrentIndex(0);
}

void SettingsPane::selectBodyEditorTab() {
    if (m_tabs) m_tabs->setCurrentIndex(1);
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
            padding: 6px 12px;
            border: 1px solid #cbd5e1;
            border-bottom: none;
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
            margin-right: 2px;
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

    m_tabFields = new QWidget();
    m_tabBodies = new QWidget();

    setupFieldsTab(m_tabFields);
    setupBodyEditorTab(m_tabBodies);

    m_tabs->addTab(m_tabFields, tr("📊 Fields Display"));
    m_tabs->addTab(m_tabBodies, tr("🪨 Body Editor"));

    mainLayout->addWidget(m_tabs, 1);

    setupComputeCard(centralWidget);
    mainLayout->addWidget(m_grpCompute);

    setWidget(centralWidget);
}

void SettingsPane::setupFieldsTab(QWidget *container) {
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

    // --- Sub-panel 1: Profile View Fields ---
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
    m_chkProfMeasured->setChecked(true);
    m_chkProfDiff = new QCheckBox(tr("Difference Obs-Mod (Magenta --)"), boxCurves);
    m_chkProfDiff->setChecked(true);

    curvesLayout->addWidget(m_chkProfModeled);
    curvesLayout->addWidget(m_chkProfMeasured);
    curvesLayout->addWidget(m_chkProfDiff);

    // Secondary overlay curve
    auto *secRow = new QHBoxLayout();
    secRow->setContentsMargins(0, 0, 0, 0);
    m_chkProfSecondary = new QCheckBox(tr("Secondary Overlay:"), boxCurves);
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
            pv->setShowModeledCurve(m_chkProfModeled->isChecked());
            pv->setShowMeasuredCurve(m_chkProfMeasured->isChecked());
            pv->setShowDifferenceCurve(m_chkProfDiff->isChecked());
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

    // Quick Profile Navigation controls
    auto *boxNav = new QGroupBox(tr("Profile Navigation"), m_paneProfileFields);
    auto *navLayout = new QVBoxLayout(boxNav);
    navLayout->setContentsMargins(8, 8, 8, 8);
    navLayout->setSpacing(4);

    auto *orientRow = new QHBoxLayout();
    m_btnOrientEW = new QPushButton(tr("E-W (H)"), boxNav);
    m_btnOrientEW->setCheckable(true);
    m_btnOrientSN = new QPushButton(tr("S-N (V)"), boxNav);
    m_btnOrientSN->setCheckable(true);
    orientRow->addWidget(m_btnOrientEW);
    orientRow->addWidget(m_btnOrientSN);
    navLayout->addLayout(orientRow);

    connect(m_btnOrientEW, &QPushButton::clicked, this, [this]() {
        if (auto *pv = qobject_cast<ProfileView*>(m_activeView.data())) {
            pv->setProfileOrientation(ProfileOrientation::EastWest);
        }
    });
    connect(m_btnOrientSN, &QPushButton::clicked, this, [this]() {
        if (auto *pv = qobject_cast<ProfileView*>(m_activeView.data())) {
            pv->setProfileOrientation(ProfileOrientation::SouthNorth);
        }
    });

    auto *idxRow = new QHBoxLayout();
    m_btnProfPrev = new QPushButton(tr("◀ Prev"), boxNav);
    m_spinProfileIdx = new QSpinBox(boxNav);
    m_spinProfileIdx->setPrefix(tr("Prof: "));
    m_btnProfNext = new QPushButton(tr("Next ▶"), boxNav);
    idxRow->addWidget(m_btnProfPrev);
    idxRow->addWidget(m_spinProfileIdx);
    idxRow->addWidget(m_btnProfNext);
    navLayout->addLayout(idxRow);

    connect(m_btnProfPrev, &QPushButton::clicked, this, [this]() {
        if (auto *pv = qobject_cast<ProfileView*>(m_activeView.data())) pv->moveProfile(-1);
    });
    connect(m_btnProfNext, &QPushButton::clicked, this, [this]() {
        if (auto *pv = qobject_cast<ProfileView*>(m_activeView.data())) pv->moveProfile(1);
    });
    connect(m_spinProfileIdx, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        if (m_blockSync) return;
        if (auto *pv = qobject_cast<ProfileView*>(m_activeView.data())) pv->setProfileIndex(val);
    });

    auto *togglesRow = new QHBoxLayout();
    m_chkProfGrid = new QCheckBox(tr("Grid"), boxNav);
    m_chkProfGhostPrev = new QCheckBox(tr("Ghost -1"), boxNav);
    m_chkProfGhostNext = new QCheckBox(tr("Ghost +1"), boxNav);
    togglesRow->addWidget(m_chkProfGrid);
    togglesRow->addWidget(m_chkProfGhostPrev);
    togglesRow->addWidget(m_chkProfGhostNext);
    navLayout->addLayout(togglesRow);

    connect(m_chkProfGrid, &QCheckBox::toggled, this, [this](bool c) {
        if (auto *pv = qobject_cast<ProfileView*>(m_activeView.data())) pv->setShowGridLines(c);
    });
    connect(m_chkProfGhostPrev, &QCheckBox::toggled, this, [this](bool c) {
        if (auto *pv = qobject_cast<ProfileView*>(m_activeView.data())) pv->setShowPreviousProfile(c);
    });
    connect(m_chkProfGhostNext, &QCheckBox::toggled, this, [this](bool c) {
        if (auto *pv = qobject_cast<ProfileView*>(m_activeView.data())) pv->setShowNextProfile(c);
    });

    pvLayout->addWidget(boxNav);
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

    // Map Overlays
    auto *boxMvOverlays = new QGroupBox(tr("Map Overlays"), m_paneMapFields);
    auto *mvOverlaysLayout = new QVBoxLayout(boxMvOverlays);
    mvOverlaysLayout->setContentsMargins(8, 8, 8, 8);
    mvOverlaysLayout->setSpacing(4);

    m_chkMapStationPoints = new QCheckBox(tr("Observation Station Points"), boxMvOverlays);
    m_chkMapStationPoints->setChecked(true);
    m_chkMapBodyFootprints = new QCheckBox(tr("Body Footprints / Contours"), boxMvOverlays);
    m_chkMapBodyFootprints->setChecked(true);
    m_chkMapProfiles = new QCheckBox(tr("Profile Navigation Trackers"), boxMvOverlays);
    m_chkMapProfiles->setChecked(true);

    mvOverlaysLayout->addWidget(m_chkMapStationPoints);
    mvOverlaysLayout->addWidget(m_chkMapBodyFootprints);
    mvOverlaysLayout->addWidget(m_chkMapProfiles);
    mvLayout->addWidget(boxMvOverlays);

    connect(m_chkMapStationPoints, &QCheckBox::toggled, this, [this](bool c) {
        if (auto *mv = qobject_cast<MapView*>(m_activeView.data())) mv->setShowStationPoints(c);
    });
    connect(m_chkMapBodyFootprints, &QCheckBox::toggled, this, [this](bool c) {
        if (auto *mv = qobject_cast<MapView*>(m_activeView.data())) mv->setShowBodyFootprints(c);
    });
    connect(m_chkMapProfiles, &QCheckBox::toggled, this, [this](bool c) {
        if (auto *mv = qobject_cast<MapView*>(m_activeView.data())) mv->setShowProfiles(c);
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

    toggles3DLayout->addWidget(m_chk3DSlices);
    toggles3DLayout->addWidget(m_chk3DCompass);
    toggles3DLayout->addWidget(m_chk3DBoundingBox);
    toggles3DLayout->addWidget(m_chk3DFeatureEdges);
    toggles3DLayout->addWidget(m_chk3DRelief);
    toggles3DLayout->addWidget(m_chk3DWireframe);
    v3dLayout->addWidget(box3DToggles);

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

    layout->addWidget(m_pane3DView);
    layout->addStretch(1);

    scroll->setWidget(inner);
    auto *tabLayout = new QVBoxLayout(container);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->addWidget(scroll);
}

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
    m_spinDensity->setRange(-15.0, 15.0);
    m_spinDensity->setSingleStep(0.05);
    m_spinDensity->setDecimals(3);
    m_spinDensity->setSuffix(tr(" g/cm³"));
    form->addRow(tr("Density:"), m_spinDensity);

    m_spinSusceptibility = new QDoubleSpinBox(inner);
    m_spinSusceptibility->setRange(0.0, 2000.0);
    m_spinSusceptibility->setSingleStep(0.1);
    m_spinSusceptibility->setDecimals(2);
    m_spinSusceptibility->setSuffix(tr(" ×10⁻³ SI"));
    form->addRow(tr("Susceptibility:"), m_spinSusceptibility);

    m_btnBodyColor = new QPushButton(tr("Select Color..."), inner);
    form->addRow(tr("Color:"), m_btnBodyColor);

    auto *chkRow = new QHBoxLayout();
    m_chkBodyActive = new QCheckBox(tr("Active in Compute"), inner);
    m_chkBodyLocked = new QCheckBox(tr("Locked"), inner);
    chkRow->addWidget(m_chkBodyActive);
    chkRow->addWidget(m_chkBodyLocked);
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

    // Body Action Buttons
    auto *btnRow1 = new QHBoxLayout();
    m_btnAddBody = new QPushButton(tr("+ New Body"), inner);
    m_btnDeleteBody = new QPushButton(tr("🗑 Delete"), inner);
    btnRow1->addWidget(m_btnAddBody);
    btnRow1->addWidget(m_btnDeleteBody);
    layout->addLayout(btnRow1);

    auto *btnRow2 = new QHBoxLayout();
    m_btnBodyProperties = new QPushButton(tr("⚙ Properties..."), inner);
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

    connect(m_btnBodyColor, &QPushButton::clicked, this, [this]() {
        int bId = m_comboBodies->currentData().toInt();
        auto *b = m_doc->model().getBody(bId);
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
            m_doc->model().deleteBody(bId);
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
        auto *b = m_doc->model().getBody(bId);
        if (!b) return;
        std::vector<mod3d::Facet3Pt> facets;
        m_doc->model().getFacetsComputation(facets);
        m_doc->observation().fit_density(b, facets, m_doc->activeField());
        m_doc->computeAll();
        loadBodyData();
    });
}

void SettingsPane::setupComputeCard(QWidget *container) {
    m_grpCompute = new QGroupBox(tr("Computation & Status"), container);
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
    m_lblRmsStatus->setStyleSheet("font-size: 11px; color: #475569; font-family: monospace; font-weight: bold;");
    row->addWidget(m_lblRmsStatus, 0, Qt::AlignRight);
    layout->addLayout(row);
}

void SettingsPane::setActiveSubWindow(QMdiSubWindow *sub) {
    m_activeView = sub ? sub->widget() : nullptr;
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

        // Sync Profile View UI
        int idx = m_comboProfileMainField->findData(static_cast<int>(pv->mainFieldComponent()));
        if (idx >= 0) m_comboProfileMainField->setCurrentIndex(idx);
        m_lblProfileFieldInfo->setText(QString("Y-Axis: %1 (%2)")
            .arg(ModelDocument::componentName(pv->mainFieldComponent()))
            .arg(ModelDocument::componentUnit(pv->mainFieldComponent())));

        m_chkProfModeled->setChecked(pv->showModeledCurve());
        m_chkProfMeasured->setChecked(pv->showMeasuredCurve());
        m_chkProfDiff->setChecked(pv->showDifferenceCurve());
        m_chkProfSecondary->setChecked(pv->showSecondaryCurve());

        m_btnOrientEW->setChecked(pv->profileOrientation() == ProfileOrientation::EastWest);
        m_btnOrientSN->setChecked(pv->profileOrientation() == ProfileOrientation::SouthNorth);
        m_spinProfileIdx->setMaximum(pv->maxProfileIndex());
        m_spinProfileIdx->setValue(pv->profileIndex());
        m_chkProfGrid->setChecked(pv->showGridLines());
        m_chkProfGhostPrev->setChecked(pv->showPreviousProfile());
        m_chkProfGhostNext->setChecked(pv->showNextProfile());
        m_btnContinuousDrawing->setChecked(pv->continuousDrawingMode());

    } else if (auto *mv = qobject_cast<MapView*>(m_activeView.data())) {
        m_lblViewContext->setText(tr("Active View: Map View"));
        m_paneProfileFields->setVisible(false);
        m_paneMapFields->setVisible(true);
        m_pane3DView->setVisible(false);

        int idx = m_comboMapField->findData(static_cast<int>(mv->fieldComponent()));
        if (idx >= 0) m_comboMapField->setCurrentIndex(idx);

        if (auto *btn = m_grpMapRaster->button(static_cast<int>(mv->rasterMode()))) {
            btn->setChecked(true);
        }
        m_chkMapStationPoints->setChecked(mv->showStationPoints());
        m_chkMapBodyFootprints->setChecked(mv->showBodyFootprints());
        m_chkMapProfiles->setChecked(mv->showProfiles());

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

    const auto &bodies = m_doc->model().getBodies();
    for (const auto &b : bodies) {
        QColor col(b->color().r, b->color().g, b->color().b);
        QString itemText = QString("Body %1: %2 (ρ=%3)").arg(b->id()).arg(QString::fromStdString(b->name())).arg(b->density(), 0, 'f', 2);
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
    auto *b = m_doc->model().getBody(bId);
    if (!b) {
        m_editBodyName->clear();
        m_spinDensity->setValue(0.0);
        m_spinSusceptibility->setValue(0.0);
        m_btnBodyColor->setIcon(QIcon());
        return;
    }

    m_blockSync = true;
    m_editBodyName->setText(QString::fromStdString(b->name()));
    m_spinDensity->setValue(b->density());
    m_spinSusceptibility->setValue(b->susceptibility() * 1000.0);
    QColor col(b->color().r, b->color().g, b->color().b);
    m_btnBodyColor->setIcon(createColorIcon(col, 20));
    m_btnBodyColor->setText(QString("#%1%2%3")
        .arg(col.red(), 2, 16, QChar('0'))
        .arg(col.green(), 2, 16, QChar('0'))
        .arg(col.blue(), 2, 16, QChar('0')).toUpper());
    m_chkBodyActive->setChecked(b->is_active());
    m_chkBodyLocked->setChecked(b->is_locked());
    m_blockSync = false;
}

void SettingsPane::applyBodyData() {
    if (m_blockSync) return;
    int bId = m_comboBodies->currentData().toInt();
    auto *b = m_doc->model().getBody(bId);
    if (!b) return;

    b->set_name(m_editBodyName->text().toStdString());
    b->set_density(m_spinDensity->value());
    b->set_susceptibility(m_spinSusceptibility->value() / 1000.0);
    b->set_active(m_chkBodyActive->isChecked());
    b->set_locked(m_chkBodyLocked->isChecked());
    m_doc->setModified(true);

    if (m_doc->isRealTimeCompute()) {
        m_doc->computeAll();
    }
    m_doc->emitModelGeometryChanged();
}

void SettingsPane::refreshAll() {
    updateViewContext();
    updateBodyList();
    loadBodyData();
}

} // namespace mod3d_gui
