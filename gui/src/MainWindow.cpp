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
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QRadialGradient>
#include <cmath>

namespace mod3d_gui {

namespace {
QIcon createZoomIcon(bool zoomIn) {
    QIcon icon;
    for (int size : {16, 20, 24, 32, 48}) {
        QPixmap pix(size, size);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing, true);

        qreal scale = size / 24.0;
        qreal cx = 9.5 * scale;
        qreal cy = 9.5 * scale;
        qreal r = 5.8 * scale;
        qreal strokeWidth = std::max(1.5, 2.0 * scale);

        // Glass background gradient
        QRadialGradient glassGrad(QPointF(cx - r * 0.3, cy - r * 0.3), r);
        glassGrad.setColorAt(0.0, QColor(240, 249, 255, 230));
        glassGrad.setColorAt(1.0, QColor(186, 230, 253, 170));
        p.setBrush(glassGrad);

        // Lens rim
        QPen rimPen(QColor(30, 41, 59), strokeWidth);
        rimPen.setJoinStyle(Qt::RoundJoin);
        rimPen.setCapStyle(Qt::RoundCap);
        p.setPen(rimPen);
        p.drawEllipse(QPointF(cx, cy), r, r);

        // Handle
        qreal hx1 = cx + r * 0.7071;
        qreal hy1 = cy + r * 0.7071;
        qreal hx2 = size - 2.5 * scale;
        qreal hy2 = size - 2.5 * scale;
        QPen handlePen(QColor(30, 41, 59), strokeWidth * 1.3);
        handlePen.setCapStyle(Qt::RoundCap);
        p.setPen(handlePen);
        p.drawLine(QPointF(hx1, hy1), QPointF(hx2, hy2));

        // Sign (+ or -)
        QPen signPen(QColor(15, 23, 42), std::max(1.5, 1.8 * scale));
        signPen.setCapStyle(Qt::RoundCap);
        p.setPen(signPen);

        qreal signR = 3.2 * scale;
        p.drawLine(QPointF(cx - signR, cy), QPointF(cx + signR, cy));
        if (zoomIn) {
            p.drawLine(QPointF(cx, cy - signR), QPointF(cx, cy + signR));
        }

        p.end();
        icon.addPixmap(pix);
    }

    return QIcon::fromTheme(zoomIn ? "zoom-in" : "zoom-out", icon);
}
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_doc(new ModelDocument(this))
    , m_mdiArea(new QMdiArea(this))
{
    setCentralWidget(m_mdiArea);
    m_mdiArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_mdiArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    m_statusBarMgr = new StatusBarManager(statusBar(), this);

    m_settingsPane = new SettingsPane(m_doc, this, this);
    addDockWidget(Qt::RightDockWidgetArea, m_settingsPane);

    setupMenus();
    setupConnections();
    updateWindowTitle();

    connect(m_settingsPane, &SettingsPane::requestBodyPropertiesDialog, this, &MainWindow::onBodyProperties);

    connect(m_mdiArea, &QMdiArea::subWindowActivated, this, [this](QMdiSubWindow *sub) {
        if (m_isDestroying || !m_mdiArea) return;
        if (sub && sub->widget()) {
            if (auto *pv = qobject_cast<ProfileView*>(sub->widget())) {
                m_lastActiveProfileView = pv;
                if (m_doc) {
                    m_doc->setProfileOrientation(pv->profileOrientation());
                    m_doc->setProfileIndex(pv->profileIndex());
                }
            }
        }
        if (m_settingsPane) {
            m_settingsPane->setActiveSubWindow(sub);
        }
        for (auto *s : m_mdiArea->subWindowList()) {
            if (s && s->widget()) {
                if (auto *glv = qobject_cast<GL3DView*>(s->widget())) {
                    glv->update();
                }
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

    // Auto-fit subwindows whenever the MDI viewport client area is resized
    m_mdiArea->installEventFilter(this);
    m_mdiArea->viewport()->installEventFilter(this);
}

MainWindow::~MainWindow() {
    m_isDestroying = true;
    if (m_mdiArea) {
        m_mdiArea->removeEventFilter(this);
        if (m_mdiArea->viewport()) {
            m_mdiArea->viewport()->removeEventFilter(this);
        }
        m_mdiArea->disconnect(this);
    }
    if (m_settingsPane) {
        m_settingsPane->setActiveSubWindow(nullptr);
    }
}

void MainWindow::updateWindowTitle() {
    setWindowTitle(QString("Mod3D 2.4.5 - [%1]").arg(m_doc->documentTitle()));
}

void MainWindow::setupConnections() {
    connect(m_doc, &ModelDocument::documentModifiedChanged, this, [this](bool) {
        updateWindowTitle();
    });
    connect(m_doc, &ModelDocument::statusMessage, this, [this](const QString &msg) {
        m_statusBarMgr->setMessage(msg);
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
            if (grd) grd->save_srf6_binary(fn.toStdString());
        }
    });
    menuExport->addAction(tr("Body Mesh (OBJ)..."), [this]() {
        QString fn = QFileDialog::getSaveFileName(this, tr("Export Body Mesh"), QString(), tr("Wavefront OBJ (*.obj);;All Files (*)"));
        if (!fn.isEmpty()) {
            m_doc->project().export_obj(fn.toStdString());
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
            else if (auto *hv = qobject_cast<HorizontalSliceView*>(sub->widget())) hv->fitToWindow();
        }
    });
    auto *actZoomIn = menuView->addAction(createZoomIcon(true), tr("Zoom In"), this, [this]() {
        if (auto *sub = m_mdiArea->activeSubWindow()) {
            if (auto *pv = qobject_cast<ProfileView*>(sub->widget())) pv->zoomIn();
            else if (auto *mv = qobject_cast<MapView*>(sub->widget())) mv->zoomIn();
            else if (auto *hv = qobject_cast<HorizontalSliceView*>(sub->widget())) hv->zoomIn();
        }
    });
    actZoomIn->setShortcut(QKeySequence::ZoomIn);

    auto *actZoomOut = menuView->addAction(createZoomIcon(false), tr("Zoom Out"), this, [this]() {
        if (auto *sub = m_mdiArea->activeSubWindow()) {
            if (auto *pv = qobject_cast<ProfileView*>(sub->widget())) pv->zoomOut();
            else if (auto *mv = qobject_cast<MapView*>(sub->widget())) mv->zoomOut();
            else if (auto *hv = qobject_cast<HorizontalSliceView*>(sub->widget())) hv->zoomOut();
        }
    });
    actZoomOut->setShortcut(QKeySequence::ZoomOut);
    menuView->addSeparator();
    menuView->addAction(tr("Horizontal Planar View"), this, [this]() { openHorizontalSliceView(); });

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
    menuBody->addAction(tr("Delete Body..."), [this]() {
        if (m_doc->model().bodies().empty()) {
            QMessageBox::information(this, tr("Delete Body"), tr("No bodies exist in the current model."));
            return;
        }
        int bId = -1;
        if (m_settingsPane && m_settingsPane->bodyComboBox() && m_settingsPane->bodyComboBox()->count() > 0) {
            bId = m_settingsPane->bodyComboBox()->currentData().toInt();
        }
        if (bId < 0) {
            bId = m_doc->model().bodies().front()->id();
        }
        auto res = QMessageBox::question(this, tr("Delete Body"),
            tr("Are you sure you want to delete Body %1?").arg(bId),
            QMessageBox::Yes | QMessageBox::No);
        if (res == QMessageBox::Yes) {
            m_doc->deleteBody(bId);
        }
    });
    menuBody->addAction(tr("Properties..."), [this]() { onBodyProperties(); });
    menuBody->addSeparator();
    menuBody->addAction(tr("Copy to Previous Profile\tCtrl+Shift+P"), [this]() {
        if (!m_doc->model().bodies().empty()) {
            int bId = m_doc->model().bodies().front()->id();
            if (auto *pv = activeProfileView()) {
                m_doc->copyBodyBetweenProfiles(bId, pv->profileOrientation(), pv->profileIndex(), pv->profileIndex() - 1);
            } else {
                m_doc->copyBodyToPreviousProfile(bId);
            }
        }
    });
    menuBody->addAction(tr("Copy to Next Profile\tCtrl+Shift+N"), [this]() {
        if (!m_doc->model().bodies().empty()) {
            int bId = m_doc->model().bodies().front()->id();
            if (auto *pv = activeProfileView()) {
                m_doc->copyBodyBetweenProfiles(bId, pv->profileOrientation(), pv->profileIndex(), pv->profileIndex() + 1);
            } else {
                m_doc->copyBodyToNextProfile(bId);
            }
        }
    });
    menuBody->addSeparator();
    menuBody->addAction(tr("Invert Density"), [this]() {
        if (!m_doc->model().bodies().empty()) {
            auto *b = m_doc->model().bodies().front().get();
            std::vector<mod3d::Facet3Pt> facets;
            m_doc->model().facets_computation(facets);
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
    menuWindow->addAction(tr("New &Horizontal Planar Window"), this, [this]() { openHorizontalSliceView(); });
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
    if (m_isDestroying || !m_mdiArea) return nullptr;
    if (auto *sub = m_mdiArea->activeSubWindow()) {
        if (sub && sub->widget()) {
            if (auto *pv = qobject_cast<ProfileView*>(sub->widget())) {
                return pv;
            }
        }
    }
    return m_lastActiveProfileView.data();
}

ProfileView *MainWindow::openProfileView(ProfileOrientation orient, int profileIndex) {
    auto *pv = new ProfileView(m_doc, orient, profileIndex, this);
    auto *sub = m_mdiArea->addSubWindow(pv);
    pv->updateTitle();
    m_lastActiveProfileView = pv;
    sub->resize(600, 420);
    sub->show();

    connect(sub, &QObject::destroyed, this, [this]() {
        if (m_isDestroying || !m_mdiArea) return;
        for (auto *s : m_mdiArea->subWindowList()) {
            if (s && s->widget()) {
                if (auto *glv = qobject_cast<GL3DView*>(s->widget())) {
                    glv->update();
                }
            }
        }
    });

    connect(pv, &ProfileView::cursorStationChanged, this, [this](int r, int c, double x, double y, double z, double) {
        if (m_isDestroying || !m_statusBarMgr) return;
        m_statusBarMgr->setRow(r);
        m_statusBarMgr->setCol(c);
        m_statusBarMgr->setX(x);
        m_statusBarMgr->setY(y);
        m_statusBarMgr->setZ(z);
    });
    connect(pv, &ProfileView::requestBodyProperties, this, &MainWindow::onBodyProperties);
    connect(pv, &ProfileView::requestFit1DDialog, this, &MainWindow::onFit1DProperties);
    connect(pv, &ProfileView::requestOpenPerpendicularProfile, this, [this](ProfileOrientation orient, int idx) {
        if (m_isDestroying) return;
        openProfileView(orient, idx);
    });

    connect(pv, &ProfileView::profileSettingsChanged, this, [this, pv](int idx, ProfileOrientation o) {
        if (m_isDestroying || !m_mdiArea) return;
        m_lastActiveProfileView = pv;
        if (m_doc) {
            m_doc->setProfileOrientation(o);
            m_doc->setProfileIndex(idx);
        }
        for (auto *s : m_mdiArea->subWindowList()) {
            if (!s || !s->widget()) continue;
            if (auto *otherPv = qobject_cast<ProfileView*>(s->widget())) {
                if (otherPv != pv) otherPv->update();
            } else if (auto *mv = qobject_cast<MapView*>(s->widget())) {
                mv->update();
            } else if (auto *glv = qobject_cast<GL3DView*>(s->widget())) {
                glv->update();
            }
        }
    });

    for (auto *s : m_mdiArea->subWindowList()) {
        if (s && s->widget()) {
            if (auto *glv = qobject_cast<GL3DView*>(s->widget())) {
                glv->update();
            }
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
        if (m_isDestroying || !m_statusBarMgr) return;
        m_statusBarMgr->setX(x);
        m_statusBarMgr->setY(y);
    });

    return mv;
}

HorizontalSliceView *MainWindow::openHorizontalSliceView(double initialZ) {
    auto *hv = new HorizontalSliceView(m_doc, initialZ, this);
    auto *sub = m_mdiArea->addSubWindow(hv);
    hv->updateTitle();
    sub->resize(520, 440);
    sub->show();

    connect(hv, &HorizontalSliceView::cursorStationChanged, this, [this](int r, int c, double x, double y, double z, double) {
        if (m_isDestroying || !m_statusBarMgr) return;
        m_statusBarMgr->setRow(r);
        m_statusBarMgr->setCol(c);
        m_statusBarMgr->setX(x);
        m_statusBarMgr->setY(y);
        m_statusBarMgr->setZ(z);
    });
    connect(hv, &HorizontalSliceView::requestBodyProperties, this, &MainWindow::onBodyProperties);

    return hv;
}

GL3DView *MainWindow::openGL3DView() {
    auto *glv = new GL3DView(m_doc, this);
    auto *sub = m_mdiArea->addSubWindow(glv);
    sub->setWindowTitle(tr("3D View"));
    sub->resize(540, 420);
    sub->show();

    if (m_settingsPane) {
        connect(glv, &GL3DView::bodySelected, this, [this](int bodyId) {
            if (m_isDestroying || !m_settingsPane) return;
            if (bodyId >= 0 && m_settingsPane->bodyComboBox()) {
                int idx = m_settingsPane->bodyComboBox()->findData(bodyId);
                if (idx >= 0) {
                    m_settingsPane->bodyComboBox()->setCurrentIndex(idx);
                }
            }
        });
        if (m_settingsPane->bodyComboBox() && m_settingsPane->bodyComboBox()->count() > 0) {
            glv->setSelectedBodyId(m_settingsPane->bodyComboBox()->currentData().toInt());
        }
    }

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

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    resizeSubWindowsToFit();
}

void MainWindow::showEvent(QShowEvent *event) {
    QMainWindow::showEvent(event);
    resizeSubWindowsToFit();
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    if (m_isDestroying) return false;
    if ((obj == m_mdiArea || (m_mdiArea && obj == m_mdiArea->viewport())) && event->type() == QEvent::Resize) {
        resizeSubWindowsToFit();
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::resizeSubWindowsToFit() {
    if (m_isDestroying || m_inResizeSubWindows || !m_mdiArea) return;
    m_inResizeSubWindows = true;

    auto subWindows = m_mdiArea->subWindowList();
    if (subWindows.empty()) {
        m_inResizeSubWindows = false;
        return;
    }

    // If any window is currently maximized, QMdiArea natively handles fitting it to full client area
    for (auto *sub : subWindows) {
        if (sub && sub->isMaximized()) {
            m_inResizeSubWindows = false;
            return;
        }
    }

    QRect r = m_mdiArea->viewport()->contentsRect();
    int w = r.width();
    int h = r.height();
    if (w < 100 || h < 100) {
        int paneW = (m_settingsPane && !m_settingsPane->isHidden()) ? m_settingsPane->width() : 350;
        if (paneW <= 50) paneW = 350;
        int mainW = (width() > 400) ? width() : 1260;
        int mainH = (height() > 300) ? height() : 820;
        w = std::max(200, mainW - paneW);
        h = std::max(200, mainH - 85);
    }

    // Classify the standard 4 quad subwindows
    QMdiSubWindow *subPvEW = nullptr;
    QMdiSubWindow *subMap = nullptr;
    QMdiSubWindow *sub3D = nullptr;
    QMdiSubWindow *subPvNS = nullptr;

    for (auto *sub : subWindows) {
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

    // 1. Standard 2x2 quad layout auto-fit
    if (subPvEW && subMap && sub3D && subPvNS) {
        int halfW = w / 2;
        int halfH = h / 2;

        if (!subPvEW->isMinimized()) subPvEW->setGeometry(0, 0, halfW, halfH);
        if (!subMap->isMinimized()) subMap->setGeometry(halfW, 0, w - halfW, halfH);
        if (!sub3D->isMinimized()) sub3D->setGeometry(0, halfH, halfW, h - halfH);
        if (!subPvNS->isMinimized()) subPvNS->setGeometry(halfW, halfH, w - halfW, h - halfH);

        m_inResizeSubWindows = false;
        return;
    }

    // 2. Exactly 3 subwindows open
    if (subWindows.size() == 3) {
        auto *w0 = subWindows[0];
        auto *w1 = subWindows[1];
        auto *w2 = subWindows[2];
        int halfW = w / 2;
        int halfH = h / 2;
        if (w >= h) {
            if (!w0->isMinimized()) w0->setGeometry(0, 0, halfW, h);
            if (!w1->isMinimized()) w1->setGeometry(halfW, 0, w - halfW, halfH);
            if (!w2->isMinimized()) w2->setGeometry(halfW, halfH, w - halfW, h - halfH);
        } else {
            if (!w0->isMinimized()) w0->setGeometry(0, 0, w, halfH);
            if (!w1->isMinimized()) w1->setGeometry(0, halfH, halfW, h - halfH);
            if (!w2->isMinimized()) w2->setGeometry(halfW, halfH, w - halfW, h - halfH);
        }
        m_inResizeSubWindows = false;
        return;
    }

    // 3. Exactly 2 subwindows open
    if (subWindows.size() == 2) {
        auto *w0 = subWindows[0];
        auto *w1 = subWindows[1];
        if (w >= h) {
            int halfW = w / 2;
            if (!w0->isMinimized()) w0->setGeometry(0, 0, halfW, h);
            if (!w1->isMinimized()) w1->setGeometry(halfW, 0, w - halfW, h);
        } else {
            int halfH = h / 2;
            if (!w0->isMinimized()) w0->setGeometry(0, 0, w, halfH);
            if (!w1->isMinimized()) w1->setGeometry(0, halfH, w, h - halfH);
        }
        m_inResizeSubWindows = false;
        return;
    }

    // 4. Exactly 1 subwindow open
    if (subWindows.size() == 1) {
        if (!subWindows[0]->isMinimized()) {
            subWindows[0]->setGeometry(0, 0, w, h);
        }
        m_inResizeSubWindows = false;
        return;
    }

    // 5. Generic 4 subwindows
    if (subWindows.size() == 4) {
        int halfW = w / 2;
        int halfH = h / 2;
        if (!subWindows[0]->isMinimized()) subWindows[0]->setGeometry(0, 0, halfW, halfH);
        if (!subWindows[1]->isMinimized()) subWindows[1]->setGeometry(halfW, 0, w - halfW, halfH);
        if (!subWindows[2]->isMinimized()) subWindows[2]->setGeometry(0, halfH, halfW, h - halfH);
        if (!subWindows[3]->isMinimized()) subWindows[3]->setGeometry(halfW, halfH, w - halfW, h - halfH);
        m_inResizeSubWindows = false;
        return;
    }

    // 6. General N windows fallback: tile to fill client area
    m_mdiArea->tileSubWindows();
    m_inResizeSubWindows = false;
}

void MainWindow::arrangeDefaultTiles() {
    for (auto *sub : m_mdiArea->subWindowList()) {
        if (sub && (sub->isMinimized() || sub->isMaximized())) {
            sub->showNormal();
        }
    }

    resizeSubWindowsToFit();

    // Set Top-Left E-W profile active by default when explicitly requested
    for (auto *sub : m_mdiArea->subWindowList()) {
        if (!sub || !sub->widget()) continue;
        if (auto *pv = qobject_cast<ProfileView*>(sub->widget())) {
            if (pv->profileOrientation() == ProfileOrientation::EastWest) {
                m_mdiArea->setActiveSubWindow(sub);
                m_lastActiveProfileView = pv;
                m_doc->setProfileOrientation(pv->profileOrientation());
                m_doc->setProfileIndex(pv->profileIndex());
                break;
            }
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
    DefineObservationDialog dlg(m_doc, this);
    dlg.setWindowTitle(tr("New Model - Define Observation Area"));
    dlg.exec();
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
            for (auto *sub : m_mdiArea->subWindowList()) {
                if (sub && sub->widget()) {
                    sub->widget()->update();
                }
            }
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
    if (m_doc->model().bodies().empty()) {
        QMessageBox::information(this, tr("Body Properties"), tr("No bodies exist in the current model."));
        return;
    }
    mod3d::Body *b = (bodyId >= 0) ? m_doc->model().body(bodyId) : m_doc->model().bodies().front().get();
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
                       tr("<h3>Mod3D 2.4.5</h3>"
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
