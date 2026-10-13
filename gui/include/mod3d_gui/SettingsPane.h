#pragma once

#include "mod3d_gui/ModelDocument.h"
#include <QDockWidget>
#include <QPointer>
#include <QComboBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QTabWidget>
#include <QMdiSubWindow>

namespace mod3d_gui {

class MainWindow;
class ProfileView;
class MapView;
class GL3DView;

class SettingsPane : public QDockWidget {
    Q_OBJECT

public:
    explicit SettingsPane(ModelDocument *doc, MainWindow *mainWindow, QWidget *parent = nullptr);
    ~SettingsPane() override = default;

    void setActiveSubWindow(QMdiSubWindow *sub);
    void refreshAll();
    void selectDisplayTab();
    void selectFieldsTab() { selectDisplayTab(); }
    void selectProfilesTab();
    void selectBodyEditorTab();
    void selectPhysicsTab();

    QCheckBox *measuredCurveCheckBox() const { return m_chkProfMeasured; }
    QCheckBox *differenceCurveCheckBox() const { return m_chkProfDiff; }
    QRadioButton *mapObservedRadioButton() const { return m_rbMapObserved; }
    QRadioButton *mapDifferenceRadioButton() const { return m_rbMapDiff; }

signals:
    void requestBodyPropertiesDialog(int bodyId);

private:
    void setupUi();
    void setupDisplayTab(QWidget *container);
    void setupProfilesTab(QWidget *container);
    void setupBodyEditorTab(QWidget *container);
    void setupPhysicsTab(QWidget *container);
    void setupComputeCard(QWidget *container);
    void updateViewContext();
    void updateBodyList();
    void loadBodyData();
    void applyBodyData();
    void loadPhysicsData();
    void applyPhysicsData();

    ModelDocument *m_doc{nullptr};
    MainWindow *m_mainWindow{nullptr};
    QPointer<QWidget> m_activeView{nullptr};

    QTabWidget *m_tabs{nullptr};
    QWidget *m_tabDisplay{nullptr};
    QWidget *m_tabProfiles{nullptr};
    QWidget *m_tabBodies{nullptr};
    QWidget *m_tabPhysics{nullptr};

    // Tab 1: Display
    QLabel *m_lblViewContext{nullptr};

    // Profile View Display Controls
    QWidget *m_paneProfileFields{nullptr};
    QComboBox *m_comboProfileMainField{nullptr};
    QLabel *m_lblProfileFieldInfo{nullptr};
    QCheckBox *m_chkProfModeled{nullptr};
    QCheckBox *m_chkProfMeasured{nullptr};
    QCheckBox *m_chkProfDiff{nullptr};
    QCheckBox *m_chkProfSecondary{nullptr};
    QComboBox *m_comboProfileSecField{nullptr};
    QCheckBox *m_chkProfShowStationPts{nullptr};
    QCheckBox *m_chkProfGrid{nullptr};
    QCheckBox *m_chkProfShowRelief{nullptr};
    QCheckBox *m_chkProfShowIntersects{nullptr};
    QCheckBox *m_chkProfGhostPrev{nullptr};
    QCheckBox *m_chkProfGhostNext{nullptr};

    // Map View Display Controls
    QWidget *m_paneMapFields{nullptr};
    QComboBox *m_comboMapField{nullptr};
    QButtonGroup *m_grpMapRaster{nullptr};
    QRadioButton *m_rbMapModeled{nullptr};
    QRadioButton *m_rbMapObserved{nullptr};
    QRadioButton *m_rbMapDiff{nullptr};
    QRadioButton *m_rbMapRelief{nullptr};
    QRadioButton *m_rbMapNone{nullptr};
    QCheckBox *m_chkMapStationPoints{nullptr};
    QCheckBox *m_chkMapProfiles{nullptr};
    QCheckBox *m_chkMapBodyFootprints{nullptr};
    QCheckBox *m_chkMapReliefContours{nullptr};

    // 3D View Display Controls
    QWidget *m_pane3DView{nullptr};
    QPushButton *m_btn3DTop{nullptr};
    QPushButton *m_btn3DReset{nullptr};
    QPushButton *m_btn3DEast{nullptr};
    QPushButton *m_btn3DWest{nullptr};
    QPushButton *m_btn3DNorth{nullptr};
    QPushButton *m_btn3DSouth{nullptr};
    QCheckBox *m_chk3DSlices{nullptr};
    QCheckBox *m_chk3DCompass{nullptr};
    QCheckBox *m_chk3DBoundingBox{nullptr};
    QCheckBox *m_chk3DFeatureEdges{nullptr};
    QCheckBox *m_chk3DWireframe{nullptr};
    QCheckBox *m_chk3DRelief{nullptr};

    // Tab 2: Profiles Tab Controls
    QPushButton *m_btnProfTabOrientEW{nullptr};
    QPushButton *m_btnProfTabOrientSN{nullptr};
    QPushButton *m_btnProfTabPrev{nullptr};
    QSpinBox *m_spinProfTabIdx{nullptr};
    QPushButton *m_btnProfTabNext{nullptr};
    QLabel *m_lblProfTabCoord{nullptr};
    QPushButton *m_btnJumpMin{nullptr};
    QPushButton *m_btnJumpMax{nullptr};
    QPushButton *m_btnJumpExtremum{nullptr};
    QLabel *m_lblProfGeometryInfo{nullptr};
    QCheckBox *m_chkProfTabGrid{nullptr};
    QCheckBox *m_chkProfTabStationPts{nullptr};
    QCheckBox *m_chkProfTabRelief{nullptr};
    QCheckBox *m_chkProfTabIntersects{nullptr};
    QCheckBox *m_chkProfTabGhostPrev{nullptr};
    QCheckBox *m_chkProfTabGhostNext{nullptr};
    QPushButton *m_btnCopyBodyPrev{nullptr};
    QPushButton *m_btnCopyBodyNext{nullptr};
    QPushButton *m_btnOpenPerpendicular{nullptr};

    // Tab 3: Geological Body Editor Controls
    QComboBox *m_comboBodies{nullptr};
    QLineEdit *m_editBodyName{nullptr};
    QDoubleSpinBox *m_spinDensity{nullptr};
    QDoubleSpinBox *m_spinSusceptibility{nullptr};
    QPushButton *m_btnBodyColor{nullptr};
    QCheckBox *m_chkBodyActive{nullptr};
    QCheckBox *m_chkBodyLocked{nullptr};
    QPushButton *m_btnContinuousDrawing{nullptr};
    QDoubleSpinBox *m_spinGradX{nullptr};
    QDoubleSpinBox *m_spinGradY{nullptr};
    QDoubleSpinBox *m_spinGradZ{nullptr};
    QDoubleSpinBox *m_spinRemJ{nullptr};
    QDoubleSpinBox *m_spinRemInc{nullptr};
    QDoubleSpinBox *m_spinRemDec{nullptr};
    QPushButton *m_btnAddBody{nullptr};
    QPushButton *m_btnDeleteBody{nullptr};
    QPushButton *m_btnBodyProperties{nullptr};
    QPushButton *m_btnInvertDensity{nullptr};

    // Tab 4: Physics & Model Controls
    QDoubleSpinBox *m_spinIndT0{nullptr};
    QDoubleSpinBox *m_spinIndInc{nullptr};
    QDoubleSpinBox *m_spinIndDec{nullptr};
    QDoubleSpinBox *m_spinSensorGrvHeight{nullptr};
    QDoubleSpinBox *m_spinSensorMagHeight{nullptr};
    QDoubleSpinBox *m_spinRefDensity{nullptr};
    QDoubleSpinBox *m_spinModelZMax{nullptr};
    QDoubleSpinBox *m_spinModelZMin{nullptr};
    QLabel *m_lblModelDimensions{nullptr};
    QRadioButton *m_rbOptBrent{nullptr};
    QRadioButton *m_rbOptGolden{nullptr};
    QRadioButton *m_rbTargetRms{nullptr};
    QRadioButton *m_rbTargetRoughness{nullptr};
    QDoubleSpinBox *m_spinFitTol{nullptr};
    QSpinBox *m_spinFitMaxIter{nullptr};
    QPushButton *m_btnFitActiveDensity{nullptr};
    QPushButton *m_btnFit1DVertices{nullptr};

    // Pinned Bottom Computation Card
    QGroupBox *m_grpCompute{nullptr};
    QPushButton *m_btnComputeAll{nullptr};
    QCheckBox *m_chkRealTimeCompute{nullptr};
    QLabel *m_lblRmsStatus{nullptr};

    bool m_blockSync{false};
};

} // namespace mod3d_gui
