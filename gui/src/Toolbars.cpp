#include "mod3d_gui/Toolbars.h"
#include "mod3d_gui/ProfileView.h"
#include <QIcon>
#include <QStyle>
#include <QPainter>
#include <QPixmap>
#include <QRadialGradient>
#include <cmath>

namespace mod3d_gui {

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

ToolbarsManager::ToolbarsManager(ModelDocument *doc, QWidget *parent)
    : QObject(parent)
    , m_doc(doc)
{
    setupMainToolBar(parent);
    setupProfileToolBar(parent);
    setupMapToolBar(parent);

    connect(m_doc, &ModelDocument::profileChanged, this, [this](int, ProfileOrientation) {
        updateProfileWidgets();
    });
    connect(m_doc, &ModelDocument::documentReset, this, [this]() {
        updateProfileWidgets();
    });

    updateProfileWidgets();
}

ToolbarsManager::~ToolbarsManager() {
    if (m_activeProfileView) {
        disconnect(m_activeProfileView.data(), nullptr, this, nullptr);
        m_activeProfileView = nullptr;
    }
}

void ToolbarsManager::setActiveProfileView(ProfileView *pv) {
    if (m_activeProfileView == pv) return;

    if (m_activeProfileView) {
        disconnect(m_activeProfileView.data(), nullptr, this, nullptr);
    }

    m_activeProfileView = pv;

    if (m_activeProfileView) {
        connect(m_activeProfileView.data(), &ProfileView::profileSettingsChanged, this, [this](int, ProfileOrientation) {
            updateProfileWidgets();
        });
        connect(m_activeProfileView.data(), &ProfileView::displaySettingsChanged, this, [this]() {
            updateProfileWidgets();
        });
        connect(m_activeProfileView.data(), &QObject::destroyed, this, [this]() {
            m_activeProfileView = nullptr;
            updateProfileWidgets();
        });
    }

    updateProfileWidgets();
}

void ToolbarsManager::setupMainToolBar(QWidget *parent) {
    m_mainToolBar = new QToolBar(tr("Main Toolbar"), parent);
    m_mainToolBar->setObjectName("MainToolBar");

    auto *actNew = m_mainToolBar->addAction(parent->style()->standardIcon(QStyle::SP_FileIcon), tr("New (Ctrl+N)"), [this]() {
        m_doc->createDefaultProject();
    });
    actNew->setShortcut(QKeySequence::New);

    auto *actOpen = m_mainToolBar->addAction(parent->style()->standardIcon(QStyle::SP_DialogOpenButton), tr("Open (Ctrl+O)"), [this]() {
        emit actionTriggered("FileOpen");
    });
    actOpen->setShortcut(QKeySequence::Open);

    auto *actSave = m_mainToolBar->addAction(parent->style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Save (Ctrl+S)"), [this]() {
        emit actionTriggered("FileSave");
    });
    actSave->setShortcut(QKeySequence::Save);

    m_mainToolBar->addSeparator();

    auto *actZoomIn = m_mainToolBar->addAction(createZoomIcon(true), tr("Zoom In"), [this]() {
        emit actionTriggered("ZoomIn");
    });
    actZoomIn->setShortcut(QKeySequence::ZoomIn);
    actZoomIn->setToolTip(tr("Zoom In (Ctrl++)"));

    auto *actZoomOut = m_mainToolBar->addAction(createZoomIcon(false), tr("Zoom Out"), [this]() {
        emit actionTriggered("ZoomOut");
    });
    actZoomOut->setShortcut(QKeySequence::ZoomOut);
    actZoomOut->setToolTip(tr("Zoom Out (Ctrl+-)"));
    m_mainToolBar->addAction(tr("Fit Page"), [this]() {
        emit actionTriggered("FitPage");
    });
    m_mainToolBar->addAction(tr("Equal Scale"), [this]() {
        emit actionTriggered("EqualScale");
    });

    m_mainToolBar->addSeparator();

    m_mainToolBar->addAction(tr("Tile V"), [this]() {
        emit actionTriggered("TileVertically");
    });
    m_mainToolBar->addAction(tr("Tile H"), [this]() {
        emit actionTriggered("TileHorizontally");
    });
    m_mainToolBar->addAction(tr("Cascade"), [this]() {
        emit actionTriggered("Cascade");
    });

    m_mainToolBar->addSeparator();

    auto *actCompute = m_mainToolBar->addAction(parent->style()->standardIcon(QStyle::SP_BrowserReload), tr("Compute"), [this]() {
        m_doc->computeAll();
    });
    actCompute->setToolTip(tr("Compute forward fields (Compute \\ Compute)"));

    m_mainToolBar->addAction(tr("Redraw"), [this]() {
        emit actionTriggered("Redraw");
    });
}

void ToolbarsManager::setupProfileToolBar(QWidget *parent) {
    m_profileToolBar = new QToolBar(tr("Profile Toolbar"), parent);
    m_profileToolBar->setObjectName("ProfileToolBar");

    m_actProfEW = m_profileToolBar->addAction(tr("E-W (H)"), [this]() {
        if (m_activeProfileView) {
            m_activeProfileView->setProfileOrientation(ProfileOrientation::EastWest);
        } else {
            m_doc->setProfileOrientation(ProfileOrientation::EastWest);
        }
    });
    m_actProfEW->setCheckable(true);
    m_actProfEW->setShortcut(QKeySequence(Qt::Key_H));
    m_actProfEW->setToolTip(tr("Orient profile East-West (H)"));

    m_actProfSN = m_profileToolBar->addAction(tr("S-N (V)"), [this]() {
        if (m_activeProfileView) {
            m_activeProfileView->setProfileOrientation(ProfileOrientation::SouthNorth);
        } else {
            m_doc->setProfileOrientation(ProfileOrientation::SouthNorth);
        }
    });
    m_actProfSN->setCheckable(true);
    m_actProfSN->setShortcut(QKeySequence(Qt::Key_V));
    m_actProfSN->setToolTip(tr("Orient profile South-North (V)"));

    auto *grpOrient = new QActionGroup(this);
    grpOrient->addAction(m_actProfEW);
    grpOrient->addAction(m_actProfSN);
    m_actProfEW->setChecked(true);

    m_profileToolBar->addSeparator();

    m_actProfPrev = m_profileToolBar->addAction(tr("◀ Prev (P)"), [this]() {
        if (m_activeProfileView) {
            m_activeProfileView->moveProfile(-1);
        } else {
            m_doc->moveProfile(-1);
        }
    });
    m_actProfPrev->setShortcut(QKeySequence(Qt::Key_P));
    m_actProfPrev->setToolTip(tr("Navigate to previous profile (P)"));

    m_spinProfileIndex = new QSpinBox(m_profileToolBar);
    m_spinProfileIndex->setPrefix(tr("Prof: "));
    m_spinProfileIndex->setRange(0, 100);
    connect(m_spinProfileIndex, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        if (m_activeProfileView) {
            if (val != m_activeProfileView->profileIndex()) {
                m_activeProfileView->setProfileIndex(val);
            }
        } else {
            if (val != m_doc->profileIndex()) {
                m_doc->setProfileIndex(val);
            }
        }
    });
    m_profileToolBar->addWidget(m_spinProfileIndex);

    m_lblProfileCoord = new QLabel(m_profileToolBar);
    m_lblProfileCoord->setMinimumWidth(80);
    m_profileToolBar->addWidget(m_lblProfileCoord);

    m_actProfNext = m_profileToolBar->addAction(tr("Next (N) ▶"), [this]() {
        if (m_activeProfileView) {
            m_activeProfileView->moveProfile(1);
        } else {
            m_doc->moveProfile(1);
        }
    });
    m_actProfNext->setShortcut(QKeySequence(Qt::Key_N));
    m_actProfNext->setToolTip(tr("Navigate to next profile (N)"));
}

void ToolbarsManager::setupMapToolBar(QWidget *parent) {
    m_mapToolBar = new QToolBar(tr("Map Toolbar"), parent);
    m_mapToolBar->setObjectName("MapToolBar");

    auto *actObs = m_mapToolBar->addAction(tr("Obs Points"), [this]() {
        emit actionTriggered("ToggleObsPoints");
    });
    actObs->setCheckable(true);
    actObs->setChecked(true);

    auto *actProfLine = m_mapToolBar->addAction(tr("Profiles"), [this]() {
        emit actionTriggered("ToggleProfiles");
    });
    actProfLine->setCheckable(true);
    actProfLine->setChecked(true);

    auto *actReliefCnt = m_mapToolBar->addAction(tr("Relief Contours"), [this]() {
        emit actionTriggered("ToggleReliefContours");
    });
    actReliefCnt->setCheckable(true);
    actReliefCnt->setChecked(true);

    auto *actReliefBmp = m_mapToolBar->addAction(tr("Relief Image"), [this]() {
        emit actionTriggered("ToggleReliefImage");
    });
    actReliefBmp->setCheckable(true);
    actReliefBmp->setChecked(true);

    auto *actBodyBound = m_mapToolBar->addAction(tr("Body Footprints"), [this]() {
        emit actionTriggered("ToggleBodyFootprints");
    });
    actBodyBound->setCheckable(true);
    actBodyBound->setChecked(true);
}

void ToolbarsManager::updateProfileWidgets() {
    if (!m_doc || !m_spinProfileIndex || !m_lblProfileCoord || !m_actProfEW || !m_actProfSN) return;
    ProfileOrientation orient = m_activeProfileView ? m_activeProfileView->profileOrientation() : m_doc->profileOrientation();
    int prfIdx = m_activeProfileView ? m_activeProfileView->profileIndex() : m_doc->profileIndex();
    int maxIdx = m_activeProfileView ? m_activeProfileView->maxProfileIndex() : m_doc->maxProfileIndex();
    double coord = m_activeProfileView ? m_activeProfileView->currentProfileCoordinate() : m_doc->currentProfileCoordinate();

    bool isEW = (orient == ProfileOrientation::EastWest);
    m_actProfEW->setChecked(isEW);
    m_actProfSN->setChecked(!isEW);

    m_spinProfileIndex->blockSignals(true);
    m_spinProfileIndex->setMaximum(maxIdx);
    m_spinProfileIndex->setValue(prfIdx);
    m_spinProfileIndex->blockSignals(false);

    m_lblProfileCoord->setText(QString("(%1 = %2 m)").arg(isEW ? "Y" : "X").arg(coord, 0, 'f', 0));
}

} // namespace mod3d_gui
