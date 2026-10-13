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
    void selectFieldsTab();
    void selectBodyEditorTab();

signals:
    void requestBodyPropertiesDialog(int bodyId);

private:
    void setupUi();
    void setupFieldsTab(QWidget *container);
    void setupBodyEditorTab(QWidget *container);
    void setupComputeCard(QWidget *container);
    void updateViewContext();
    void updateBodyList();
    void loadBodyData();
    void applyBodyData();

    ModelDocument *m_doc{nullptr};
    MainWindow *m_mainWindow{nullptr};
    QPointer<QWidget> m_activeView{nullptr};

    QTabWidget *m_tabs{nullptr};
    QWidget *m_tabFields{nullptr};
    QWidget *m_tabBodies{nullptr};

    // Card 1: Fields Display
    QLabel *m_lblViewContext{nullptr};

    // Profile View controls
    QWidget *m_paneProfileFields{nullptr};
    QComboBox *m_comboProfileMainField{nullptr};
    QLabel *m_lblProfileFieldInfo{nullptr};
    QCheckBox *m_chkProfModeled{nullptr};
    QCheckBox *m_chkProfMeasured{nullptr};
    QCheckBox *m_chkProfDiff{nullptr};
    QCheckBox *m_chkProfSecondary{nullptr};
    QComboBox *m_comboProfileSecField{nullptr};
    QPushButton *m_btnOrientEW{nullptr};
    QPushButton *m_btnOrientSN{nullptr};
    QSpinBox *m_spinProfileIdx{nullptr};
    QPushButton *m_btnProfPrev{nullptr};
    QPushButton *m_btnProfNext{nullptr};
    QCheckBox *m_chkProfGrid{nullptr};
    QCheckBox *m_chkProfGhostPrev{nullptr};
    QCheckBox *m_chkProfGhostNext{nullptr};

    // Map View controls
    QWidget *m_paneMapFields{nullptr};
    QComboBox *m_comboMapField{nullptr};
    QButtonGroup *m_grpMapRaster{nullptr};
    QRadioButton *m_rbMapModeled{nullptr};
    QRadioButton *m_rbMapObserved{nullptr};
    QRadioButton *m_rbMapDiff{nullptr};
    QRadioButton *m_rbMapRelief{nullptr};
    QRadioButton *m_rbMapNone{nullptr};
    QCheckBox *m_chkMapStationPoints{nullptr};
    QCheckBox *m_chkMapBodyFootprints{nullptr};
    QCheckBox *m_chkMapProfiles{nullptr};

    // 3D View controls
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

    // Card 2: Geological Body Editor
    QGroupBox *m_grpBodies{nullptr};
    QComboBox *m_comboBodies{nullptr};
    QLineEdit *m_editBodyName{nullptr};
    QDoubleSpinBox *m_spinDensity{nullptr};
    QDoubleSpinBox *m_spinSusceptibility{nullptr};
    QPushButton *m_btnBodyColor{nullptr};
    QCheckBox *m_chkBodyActive{nullptr};
    QCheckBox *m_chkBodyLocked{nullptr};
    QPushButton *m_btnContinuousDrawing{nullptr};
    QPushButton *m_btnAddBody{nullptr};
    QPushButton *m_btnDeleteBody{nullptr};
    QPushButton *m_btnBodyProperties{nullptr};
    QPushButton *m_btnInvertDensity{nullptr};

    // Card 3: Computation & Status
    QGroupBox *m_grpCompute{nullptr};
    QPushButton *m_btnComputeAll{nullptr};
    QCheckBox *m_chkRealTimeCompute{nullptr};
    QLabel *m_lblRmsStatus{nullptr};

    bool m_blockSync{false};
};

} // namespace mod3d_gui
