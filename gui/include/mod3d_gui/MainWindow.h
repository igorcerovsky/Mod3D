#pragma once

#include "mod3d_gui/ModelDocument.h"
#include "mod3d_gui/StatusBarManager.h"
#include "mod3d_gui/Toolbars.h"
#include "mod3d_gui/ProfileView.h"
#include "mod3d_gui/MapView.h"
#include "mod3d_gui/GL3DView.h"
#include "mod3d_gui/SpreadsheetView.h"
#include "mod3d_gui/SettingsPane.h"

#include <QMainWindow>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QResizeEvent>
#include <QShowEvent>
#include <QPointer>

namespace mod3d_gui {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

    ModelDocument *document() const { return m_doc; }
    QMdiArea *mdiArea() const { return m_mdiArea; }
    SettingsPane *settingsPane() const { return m_settingsPane; }

    ProfileView *openProfileView(ProfileOrientation orient = ProfileOrientation::EastWest, int profileIndex = -1);
    ProfileView *activeProfileView() const;
    ProfileView *lastActiveProfileView() const { return m_lastActiveProfileView.data(); }
    MapView *openMapView();
    GL3DView *openGL3DView();
    SpreadsheetView *openSpreadsheetView();
    void arrangeDefaultTiles();
    void resizeSubWindowsToFit();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    bool m_inResizeSubWindows{false};
    void setupMenus();
    void setupConnections();
    void updateWindowTitle();

    // File Actions
    void onFileNew();
    void onFileOpen();
    bool onFileSave();
    bool onFileSaveAs();
    void onImportMeasuredField();
    void onImportDem();

    // Dialogs
    void onModelProperties();
    void onDefineObservation();
    void onChangeVerticalRange();
    void onInducingField();
    void onComputeProperties();
    void onActiveGrids();
    void onBodyProperties(int bodyId = -1);
    void onFit1DProperties();
    void onAbout();

    ModelDocument *m_doc;
    QMdiArea *m_mdiArea;
    StatusBarManager *m_statusBarMgr;
    ToolbarsManager *m_toolbarsMgr;
    SettingsPane *m_settingsPane{nullptr};
    QPointer<ProfileView> m_lastActiveProfileView{nullptr};
};

} // namespace mod3d_gui
