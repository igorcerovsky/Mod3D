#include "mod3d_gui/MainWindow.h"
#include "mod3d_gui/dialogs/BodyPropertiesDialog.h"
#include "mod3d_gui/dialogs/ModelPropertiesDialog.h"
#include "mod3d_gui/dialogs/DefineObservationDialog.h"
#include "mod3d_gui/dialogs/VerticalRangeDialog.h"
#include "mod3d_gui/dialogs/InducingFieldDialog.h"
#include "mod3d_gui/dialogs/ComputePropertiesDialog.h"
#include "mod3d_gui/dialogs/ActiveGridsDialog.h"
#include "mod3d_gui/dialogs/Fit1DDialog.h"

#include <QMenuBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QCloseEvent>
#include <QApplication>
#include <QTimer>

namespace mod3d_gui {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_doc(new ModelDocument(this))
    , m_mdiArea(new QMdiArea(this))
{
    setCentralWidget(m_mdiArea);
    m_mdiArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_mdiArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    m_statusBarMgr = new StatusBarManager(statusBar(), this);
    m_toolbarsMgr = new ToolbarsManager(m_doc, this);

    addToolBar(Qt::TopToolBarArea, m_toolbarsMgr->mainToolBar());
    addToolBar(Qt::TopToolBarArea, m_toolbarsMgr->profileToolBar());
    addToolBar(Qt::TopToolBarArea, m_toolbarsMgr->mapToolBar());

    m_settingsPane = new SettingsPane(m_doc, this, this);
    addDockWidget(Qt::RightDockWidgetArea, m_settingsPane);

    setupMenus();
    setupConnections();
    updateWindowTitle();

    connect(m_settingsPane, &SettingsPane::requestBodyPropertiesDialog, this, &MainWindow::onBodyProperties);

    connect(m_mdiArea, &QMdiArea::subWindowActivated, this, [this](QMdiSubWindow *sub) {
        if (m_settingsPane) {
            m_settingsPane->setActiveSubWindow(sub);
        }
        if (sub) {
            if (auto *pv = qobject_cast<ProfileView*>(sub->widget())) {
                m_toolbarsMgr->setActiveProfileView(pv);
                m_doc->setProfileOrientation(pv->profileOrientation());
                m_doc->setProfileIndex(pv->profileIndex());
            }
        }
        for (auto *s : m_mdiArea->subWindowList()) {
            if (auto *glv = qobject_cast<GL3DView*>(s->widget())) {
                glv->update();
            }
        }
    });

    // Default layout on opening:
    //  - top left: E-W profile
    //  - top right: map view
    //  - bottom left: 3D view
    //  - bottom right: N-S (perpendicular) profile
    auto *pvEW = openProfileView(ProfileOrientation::EastWest);
    openMapView();
    openGL3DView();
    openProfileView(ProfileOrientation::SouthNorth);

    if (pvEW) {
        m_settingsPane->setActiveSubWindow(qobject_cast<QMdiSubWindow*>(pvEW->parentWidget()));
    }

    resize(1260, 820);
    arrangeDefaultTiles();
    QTimer::singleShot(0, this, [this]() { arrangeDefaultTiles(); });
}

void MainWindow::updateWindowTitle() {
    setWindowTitle(QString("Mod3D 2.1 - [%1]").arg(m_doc->documentTitle()));
}

void MainWindow::setupConnections() {
    connect(m_doc, &ModelDocument::documentModifiedChanged, this, [this](bool) {
        updateWindowTitle();
    });
    connect(m_doc, &ModelDocument::statusMessage, this, [this](const QString &msg) {
        m_statusBarMgr->setMessage(msg);
    });

    connect(m_toolbarsMgr, &ToolbarsManager::actionTriggered, this, [this](const QString &act) {
        if (act == "FileOpen") onFileOpen();
        else if (act == "FileSave") onFileSave();
        else if (act == "TileVertically") m_mdiArea->tileSubWindows();
        else if (act == "TileHorizontally") m_mdiArea->tileSubWindows();
        else if (act == "Cascade") m_mdiArea->cascadeSubWindows();
        else if (act == "ZoomIn") {
            auto *active = m_mdiArea->activeSubWindow();
            if (active) {
                if (auto *pv = qobject_cast<ProfileView*>(active->widget())) pv->zoomIn();
                else if (auto *mv = qobject_cast<MapView*>(active->widget())) mv->zoomIn();
            }
        } else if (act == "ZoomOut") {
            auto *active = m_mdiArea->activeSubWindow();
            if (active) {
                if (auto *pv = qobject_cast<ProfileView*>(active->widget())) pv->zoomOut();
                else if (auto *mv = qobject_cast<MapView*>(active->widget())) mv->zoomOut();
            }
        } else if (act == "FitPage") {
            auto *active = m_mdiArea->activeSubWindow();
            if (active) {
                if (auto *pv = qobject_cast<ProfileView*>(active->widget())) pv->fitToWindow();
                else if (auto *mv = qobject_cast<MapView*>(active->widget())) mv->fitToWindow();
            }
        }
    });
}

void MainWindow::setupMenus() {
    auto *mb = menuBar();

    // 1. File Menu
    auto *menuFile = mb->addMenu(tr("&File"));
    auto *actNew = menuFile->addAction(tr("&New"), this, &MainWindow::onFileNew);
    actNew->setShortcut(QKeySequence::New);
    auto *actOpen = menuFile->addAction(tr("&Open..."), this, &MainWindow::onFileOpen);
    actOpen->setShortcut(QKeySequence::Open);
    auto *actSave = menuFile->addAction(tr("&Save"), this, &MainWindow::onFileSave);
    actSave->setShortcut(QKeySequence::Save);
    menuFile->addAction(tr("Save &As..."), this, &MainWindow::onFileSaveAs);
    menuFile->addSeparator();

    auto *menuImport = menuFile->addMenu(tr("Import"));
    menuImport->addAction(tr("Measured Field Grid..."), this, &MainWindow::onImportMeasuredField);
    menuImport->addAction(tr("Topography / Relief DEM..."), this, &MainWindow::onImportDem);

    auto *menuExport = menuFile->addMenu(tr("Export"));
    menuExport->addAction(tr("Modeled Field..."), [this]() {
        QString fn = QFileDialog::getSaveFileName(this, tr("Export Modeled Field"), QString(), tr("Surfer Binary Grid (*.grd);;All Files (*)"));
        if (!fn.isEmpty()) {
            const auto *grd = m_doc->activeModeledGrid();
            if (grd) grd->saveSrf6Binary(fn.toStdString());
        }
    });
    menuExport->addAction(tr("Body Mesh (OBJ)..."), [this]() {
        QString fn = QFileDialog::getSaveFileName(this, tr("Export Body Mesh"), QString(), tr("Wavefront OBJ (*.obj);;All Files (*)"));
        if (!fn.isEmpty()) {
            m_doc->project().exportObj(fn.toStdString());
        }
    });

    menuFile->addSeparator();
    auto *actExit = menuFile->addAction(tr("E&xit"), this, &QWidget::close);
    actExit->setShortcut(QKeySequence::Quit);

    // 2. View Menu
    auto *menuView = mb->addMenu(tr("&View"));
    menuView->addAction(m_settingsPane->toggleViewAction());
    menuView->addSeparator();
    menuView->addAction(tr("Redraw"), this, [this]() {
        for (auto *sub : m_mdiArea->subWindowList()) {
            sub->widget()->update();
        }
    });
    menuView->addSeparator();
    menuView->addAction(tr("Fit to Window"), this, [this]() {
        if (auto *sub = m_mdiArea->activeSubWindow()) {
            if (auto *pv = qobject_cast<ProfileView*>(sub->widget())) pv->fitToWindow();
            else if (auto *mv = qobject_cast<MapView*>(sub->widget())) mv->fitToWindow();
        }
    });
    menuView->addAction(tr("Zoom In"), this, [this]() {
        if (auto *sub = m_mdiArea->activeSubWindow()) {
            if (auto *pv = qobject_cast<ProfileView*>(sub->widget())) pv->zoomIn();
            else if (auto *mv = qobject_cast<MapView*>(sub->widget())) mv->zoomIn();
        }
    });
    menuView->addAction(tr("Zoom Out"), this, [this]() {
        if (auto *sub = m_mdiArea->activeSubWindow()) {
            if (auto *pv = qobject_cast<ProfileView*>(sub->widget())) pv->zoomOut();
            else if (auto *mv = qobject_cast<MapView*>(sub->widget())) mv->zoomOut();
        }
    });

    // 3. Edit Menu
    auto *menuEdit = mb->addMenu(tr("&Edit"));
    auto *actUndo = m_doc->undoStack()->createUndoAction(this, tr("&Undo"));
    actUndo->setShortcut(QKeySequence::Undo);
    menuEdit->addAction(actUndo);

    auto *actRedo = m_doc->undoStack()->createRedoAction(this, tr("&Redo"));
    actRedo->setShortcut(QKeySequence::Redo);
    menuEdit->addAction(actRedo);

    // 4. Profile Menu
    auto *menuProf = mb->addMenu(tr("&Profile"));
    menuProf->addAction(tr("E-W Profile\tH"), [this]() {
        if (auto *pv = activeProfileView()) pv->setProfileOrientation(ProfileOrientation::EastWest);
        else m_doc->setProfileOrientation(ProfileOrientation::EastWest);
    });
    menuProf->addAction(tr("S-N Profile\tV"), [this]() {
        if (auto *pv = activeProfileView()) pv->setProfileOrientation(ProfileOrientation::SouthNorth);
        else m_doc->setProfileOrientation(ProfileOrientation::SouthNorth);
    });
    menuProf->addSeparator();
    menuProf->addAction(tr("Move to Previous\tP"), [this]() {
        if (auto *pv = activeProfileView()) pv->moveProfile(-1);
        else m_doc->moveProfile(-1);
    });
    menuProf->addAction(tr("Move to Next\tN"), [this]() {
        if (auto *pv = activeProfileView()) pv->moveProfile(1);
        else m_doc->moveProfile(1);
    });
    menuProf->addSeparator();
    menuProf->addAction(tr("Show Previous"), [this]() {
        if (auto *pv = activeProfileView()) pv->setShowPreviousProfile(!pv->showPreviousProfile());
        else m_doc->setShowPreviousProfile(!m_doc->showPreviousProfile());
    });
    menuProf->addAction(tr("Show Next"), [this]() {
        if (auto *pv = activeProfileView()) pv->setShowNextProfile(!pv->showNextProfile());
        else m_doc->setShowNextProfile(!m_doc->showNextProfile());
    });
    menuProf->addSeparator();
    menuProf->addAction(tr("Jump to Extrem\tE"), [this]() {
        if (auto *pv = activeProfileView()) pv->jumpToExtreme();
        else m_doc->jumpToExtreme();
    });
    menuProf->addAction(tr("Jump to Min\tI"), [this]() {
        if (auto *pv = activeProfileView()) pv->jumpToMin();
        else m_doc->jumpToMin();
    });
    menuProf->addAction(tr("Jump to Max\tA"), [this]() {
        if (auto *pv = activeProfileView()) pv->jumpToMax();
        else m_doc->jumpToMax();
    });

    // 5. Model Menu
    auto *menuModel = mb->addMenu(tr("&Model"));
    menuModel->addAction(tr("Properties..."), this, &MainWindow::onModelProperties);
    menuModel->addSeparator();
    menuModel->addAction(tr("Define Observation..."), this, &MainWindow::onDefineObservation);
    menuModel->addAction(tr("Change Vertical Range..."), this, &MainWindow::onChangeVerticalRange);
    menuModel->addSeparator();
    menuModel->addAction(tr("Inducing Field..."), this, &MainWindow::onInducingField);

    // 6. Body Menu
    auto *menuBody = mb->addMenu(tr("&Body"));
    menuBody->addAction(tr("New Body"), [this]() { m_doc->createNewBody(); });
    menuBody->addAction(tr("Properties..."), [this]() { onBodyProperties(); });
    menuBody->addSeparator();
    menuBody->addAction(tr("Copy to Previous Profile\tCtrl+Shift+P"), [this]() {
        if (!m_doc->model().getBodies().empty()) {
            int bId = m_doc->model().getBodies().front()->id();
            if (auto *pv = activeProfileView()) {
                m_doc->copyBodyBetweenProfiles(bId, pv->profileOrientation(), pv->profileIndex(), pv->profileIndex() - 1);
            } else {
                m_doc->copyBodyToPreviousProfile(bId);
            }
        }
    });
    menuBody->addAction(tr("Copy to Next Profile\tCtrl+Shift+N"), [this]() {
        if (!m_doc->model().getBodies().empty()) {
            int bId = m_doc->model().getBodies().front()->id();
            if (auto *pv = activeProfileView()) {
                m_doc->copyBodyBetweenProfiles(bId, pv->profileOrientation(), pv->profileIndex(), pv->profileIndex() + 1);
            } else {
                m_doc->copyBodyToNextProfile(bId);
            }
        }
    });
    menuBody->addSeparator();
    menuBody->addAction(tr("Invert Density"), [this]() {
        if (!m_doc->model().getBodies().empty()) {
            auto *b = m_doc->model().getBodies().front().get();
            std::vector<mod3d::Facet3Pt> facets;
            m_doc->model().getFacetsComputation(facets);
            m_doc->observation().fit_density(b, facets, m_doc->activeField());
            m_doc->computeAll();
        }
    });

    // 7. Compute Menu
    auto *menuCompute = mb->addMenu(tr("&Compute"));
    menuCompute->addAction(tr("Compute\tF5"), [this]() { m_doc->computeAll(); });
    auto *actRealTime = menuCompute->addAction(tr("Compute Real-Time"), [this](bool checked) {
        m_doc->setRealTimeCompute(checked);
    });
    actRealTime->setCheckable(true);
    actRealTime->setChecked(m_doc->isRealTimeCompute());
    menuCompute->addSeparator();
    menuCompute->addAction(tr("Properties..."), this, &MainWindow::onComputeProperties);
    menuCompute->addAction(tr("Active Field..."), this, &MainWindow::onActiveGrids);
    menuCompute->addAction(tr("Vertex Fit Properties..."), this, &MainWindow::onFit1DProperties);

    // 8. Window Menu
    auto *menuWindow = mb->addMenu(tr("&Window"));
    menuWindow->addAction(tr("New &Profile Window (E-W)"), this, [this]() { openProfileView(ProfileOrientation::EastWest); });
    menuWindow->addAction(tr("New &Profile Window (S-N)"), this, [this]() { openProfileView(ProfileOrientation::SouthNorth); });
    menuWindow->addAction(tr("New &Map Window"), this, &MainWindow::openMapView);
    menuWindow->addAction(tr("New &3D Window"), this, &MainWindow::openGL3DView);
    menuWindow->addAction(tr("New &Spreadsheet Window"), this, &MainWindow::openSpreadsheetView);
    menuWindow->addSeparator();
    menuWindow->addAction(tr("&2x2 Quad Tile Layout"), this, &MainWindow::arrangeDefaultTiles);
    menuWindow->addAction(tr("&Cascade"), m_mdiArea, &QMdiArea::cascadeSubWindows);
    menuWindow->addAction(tr("&Tile"), m_mdiArea, &QMdiArea::tileSubWindows);

    // 9. Help Menu
    auto *menuHelp = mb->addMenu(tr("&Help"));
    menuHelp->addAction(tr("&About Mod3D..."), this, &MainWindow::onAbout);
}

ProfileView *MainWindow::activeProfileView() const {
    auto *sub = m_mdiArea->activeSubWindow();
    return sub ? qobject_cast<ProfileView*>(sub->widget()) : nullptr;
}

ProfileView *MainWindow::openProfileView(ProfileOrientation orient, int profileIndex) {
    auto *pv = new ProfileView(m_doc, orient, profileIndex, this);
    auto *sub = m_mdiArea->addSubWindow(pv);
    pv->updateTitle();
    sub->resize(600, 420);
    sub->show();

    connect(sub, &QObject::destroyed, this, [this]() {
        for (auto *s : m_mdiArea->subWindowList()) {
            if (auto *glv = qobject_cast<GL3DView*>(s->widget())) {
                glv->update();
            }
        }
    });

    connect(pv, &ProfileView::cursorStationChanged, this, [this](int r, int c, double x, double y, double z, double) {
        m_statusBarMgr->setRow(r);
        m_statusBarMgr->setCol(c);
        m_statusBarMgr->setX(x);
        m_statusBarMgr->setY(y);
        m_statusBarMgr->setZ(z);
    });
    connect(pv, &ProfileView::requestBodyProperties, this, &MainWindow::onBodyProperties);
    connect(pv, &ProfileView::requestFit1DDialog, this, &MainWindow::onFit1DProperties);
    connect(pv, &ProfileView::requestOpenPerpendicularProfile, this, [this](ProfileOrientation orient, int idx) {
        openProfileView(orient, idx);
    });

    connect(pv, &ProfileView::profileSettingsChanged, this, [this, pv](int idx, ProfileOrientation o) {
        if (activeProfileView() == pv) {
            m_toolbarsMgr->updateProfileWidgets();
            m_doc->setProfileOrientation(o);
            m_doc->setProfileIndex(idx);
        }
        for (auto *s : m_mdiArea->subWindowList()) {
            if (auto *otherPv = qobject_cast<ProfileView*>(s->widget())) {
                if (otherPv != pv) otherPv->update();
            } else if (auto *mv = qobject_cast<MapView*>(s->widget())) {
                mv->update();
            } else if (auto *glv = qobject_cast<GL3DView*>(s->widget())) {
                glv->update();
            }
        }
    });

    m_toolbarsMgr->setActiveProfileView(pv);

    for (auto *s : m_mdiArea->subWindowList()) {
        if (auto *glv = qobject_cast<GL3DView*>(s->widget())) {
            glv->update();
        }
    }

    return pv;
}

MapView *MainWindow::openMapView() {
    auto *mv = new MapView(m_doc, this);
    auto *sub = m_mdiArea->addSubWindow(mv);
    sub->setWindowTitle(tr("Map View"));
    sub->resize(500, 420);
    sub->show();

    connect(mv, &MapView::cursorStationChanged, this, [this](int, int, double x, double y, double, double) {
        m_statusBarMgr->setX(x);
        m_statusBarMgr->setY(y);
    });

    return mv;
}

GL3DView *MainWindow::openGL3DView() {
    auto *glv = new GL3DView(m_doc, this);
    auto *sub = m_mdiArea->addSubWindow(glv);
    sub->setWindowTitle(tr("3D View"));
    sub->resize(540, 420);
    sub->show();
    return glv;
}

SpreadsheetView *MainWindow::openSpreadsheetView() {
    auto *sv = new SpreadsheetView(m_doc, this);
    auto *sub = m_mdiArea->addSubWindow(sv);
    sub->setWindowTitle(tr("Spreadsheet View"));
    sub->resize(540, 360);
    sub->show();
    return sv;
}

void MainWindow::arrangeDefaultTiles() {
    QMdiSubWindow *subPvEW = nullptr;
    QMdiSubWindow *subMap = nullptr;
    QMdiSubWindow *sub3D = nullptr;
    QMdiSubWindow *subPvNS = nullptr;

    for (auto *sub : m_mdiArea->subWindowList()) {
        if (!sub || !sub->widget()) continue;
        if (auto *pv = qobject_cast<ProfileView*>(sub->widget())) {
            if (pv->profileOrientation() == ProfileOrientation::EastWest && !subPvEW) {
                subPvEW = sub;
            } else if (pv->profileOrientation() == ProfileOrientation::SouthNorth && !subPvNS) {
                subPvNS = sub;
            } else if (!subPvEW) {
                subPvEW = sub;
            } else if (!subPvNS) {
                subPvNS = sub;
            }
        } else if (qobject_cast<MapView*>(sub->widget())) {
            if (!subMap) subMap = sub;
        } else if (qobject_cast<GL3DView*>(sub->widget())) {
            if (!sub3D) sub3D = sub;
        }
    }

    QRect r = m_mdiArea->viewport()->contentsRect();
    int w = r.width();
    int h = r.height();
    if (w < 200 || h < 200) {
        w = 1180;
        h = 720;
    }

    int halfW = w / 2;
    int halfH = h / 2;

    // Default layout per user specifications:
    //  - top left: E-W profile
    if (subPvEW) {
        subPvEW->showNormal();
        subPvEW->setGeometry(0, 0, halfW, halfH);
    }
    //  - top right: map view
    if (subMap) {
        subMap->showNormal();
        subMap->setGeometry(halfW, 0, w - halfW, halfH);
    }
    //  - bottom left: 3D view
    if (sub3D) {
        sub3D->showNormal();
        sub3D->setGeometry(0, halfH, halfW, h - halfH);
    }
    //  - bottom right: N-S / perpendicular profile
    if (subPvNS) {
        subPvNS->showNormal();
        subPvNS->setGeometry(halfW, halfH, w - halfW, h - halfH);
    }

    // Set Top-Left E-W profile active by default
    if (subPvEW) {
        m_mdiArea->setActiveSubWindow(subPvEW);
        if (auto *pv = qobject_cast<ProfileView*>(subPvEW->widget())) {
            m_toolbarsMgr->setActiveProfileView(pv);
            m_doc->setProfileOrientation(pv->profileOrientation());
            m_doc->setProfileIndex(pv->profileIndex());
        }
    }
}

void MainWindow::onFileNew() {
    if (m_doc->isModified()) {
        auto res = QMessageBox::question(this, tr("Save Changes"), tr("Save changes before creating a new project?"),
                                         QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (res == QMessageBox::Yes) onFileSave();
        else if (res == QMessageBox::Cancel) return;
    }
    m_doc->createDefaultProject();
}

void MainWindow::onFileOpen() {
    QString fn = QFileDialog::getOpenFileName(this, tr("Open Mod3D File"), QString(),
                                              tr("Mod3D Files (*.mod3d *.m3d *.json);;All Files (*)"));
    if (!fn.isEmpty()) {
        m_doc->openFile(fn);
    }
}

bool MainWindow::onFileSave() {
    if (m_doc->filePath().isEmpty()) {
        return onFileSaveAs();
    }
    return m_doc->saveFile();
}

bool MainWindow::onFileSaveAs() {
    QString fn = QFileDialog::getSaveFileName(this, tr("Save Mod3D Project"), QString(),
                                              tr("Mod3D Modern Project (*.mod3d);;All Files (*)"));
    if (!fn.isEmpty()) {
        return m_doc->saveFile(fn);
    }
    return false;
}

void MainWindow::onImportMeasuredField() {
    QString fn = QFileDialog::getOpenFileName(
        this,
        tr("Import Measured Potential Field Grid"),
        QString(),
        tr("Surfer Grid Files (*.grd);;All Files (*)")
    );
    if (!fn.isEmpty()) {
        bool ok = m_doc->loadMeasuredGrid(fn);
        if (ok) {
            statusBar()->showMessage(tr("Imported measured field from %1").arg(QFileInfo(fn).fileName()), 3000);
        } else {
            QMessageBox::warning(this, tr("Import Failed"), tr("Failed to load grid file:\n%1").arg(fn));
        }
    }
}

void MainWindow::onImportDem() {
    QString fn = QFileDialog::getOpenFileName(
        this,
        tr("Import Relief / Topography DEM"),
        QString(),
        tr("Surfer Grid Files (*.grd);;All Files (*)")
    );
    if (!fn.isEmpty()) {
        bool ok = m_doc->loadReliefGrid(fn);
        if (ok) {
            statusBar()->showMessage(tr("Imported topography DEM from %1").arg(QFileInfo(fn).fileName()), 3000);
        } else {
            QMessageBox::warning(this, tr("Import Failed"), tr("Failed to load grid file:\n%1").arg(fn));
        }
    }
}

void MainWindow::onModelProperties() {
    ModelPropertiesDialog dlg(m_doc, this);
    dlg.exec();
}

void MainWindow::onDefineObservation() {
    DefineObservationDialog dlg(m_doc, this);
    dlg.exec();
}

void MainWindow::onChangeVerticalRange() {
    VerticalRangeDialog dlg(m_doc, this);
    dlg.exec();
}

void MainWindow::onInducingField() {
    InducingFieldDialog dlg(m_doc, this);
    dlg.exec();
}

void MainWindow::onComputeProperties() {
    ComputePropertiesDialog dlg(m_doc, this);
    dlg.exec();
}

void MainWindow::onActiveGrids() {
    ActiveGridsDialog dlg(m_doc, this);
    dlg.exec();
}

void MainWindow::onBodyProperties(int bodyId) {
    if (m_doc->model().getBodies().empty()) {
        QMessageBox::information(this, tr("Body Properties"), tr("No bodies exist in the current model."));
        return;
    }
    mod3d::Body *b = (bodyId >= 0) ? m_doc->model().getBody(bodyId) : m_doc->model().getBodies().front().get();
    if (b) {
        BodyPropertiesDialog dlg(b, this);
        if (dlg.exec() == QDialog::Accepted) {
            m_doc->setModified(true);
            emit m_doc->modelGeometryChanged();
            m_doc->computeAll();
        }
    }
}

void MainWindow::onFit1DProperties() {
    Fit1DDialog dlg(m_doc, this);
    dlg.exec();
}

void MainWindow::onAbout() {
    QMessageBox::about(this, tr("About Mod3D"),
                       tr("<h3>Mod3D 2.1</h3>"
                          "<p>Modern Interactive 2.5D & 3D Potential Field Modeling & Inversion System.</p>"
                          "<p>Standard C++20 Core & Qt 6 Modern GUI Architecture.</p>"));
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (m_doc->isModified()) {
        auto res = QMessageBox::question(this, tr("Save Changes"), tr("Save changes before closing?"),
                                         QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (res == QMessageBox::Yes) {
            if (!onFileSave()) {
                event->ignore();
                return;
            }
        } else if (res == QMessageBox::Cancel) {
            event->ignore();
            return;
        }
    }
    event->accept();
}

} // namespace mod3d_gui
