#include "mod3d_gui/Toolbars.h"
#include "mod3d_gui/ProfileView.h"
#include <QIcon>
#include <QStyle>

namespace mod3d_gui {

ToolbarsManager::ToolbarsManager(ModelDocument *doc, QWidget *parent)
    : QObject(parent)
    , m_doc(doc)
{
    setupMainToolBar(parent);
    setupProfileToolBar(parent);
    setupMapToolBar(parent);
    setupFieldToolBar(parent);

    connect(m_doc, &ModelDocument::profileChanged, this, [this](int, ProfileOrientation) {
        updateProfileWidgets();
    });
    connect(m_doc, &ModelDocument::documentReset, this, [this]() {
        updateProfileWidgets();
        updateFieldWidgets();
    });
    connect(m_doc, &ModelDocument::activeFieldChanged, this, [this](mod3d::FieldComponent) {
        updateFieldWidgets();
    });
    connect(m_doc, &ModelDocument::fieldDisplayToggled, this, [this]() {
        updateFieldWidgets();
    });

    updateProfileWidgets();
    updateFieldWidgets();
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

    m_mainToolBar->addAction(parent->style()->standardIcon(QStyle::SP_ArrowUp), tr("Zoom In"), [this]() {
        emit actionTriggered("ZoomIn");
    });
    m_mainToolBar->addAction(parent->style()->standardIcon(QStyle::SP_ArrowDown), tr("Zoom Out"), [this]() {
        emit actionTriggered("ZoomOut");
    });
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

    m_profileToolBar->addSeparator();

    m_actShowPrev = m_profileToolBar->addAction(tr("Ghost Prev"), [this](bool checked) {
        if (m_activeProfileView) {
            m_activeProfileView->setShowPreviousProfile(checked);
        } else {
            m_doc->setShowPreviousProfile(checked);
        }
    });
    m_actShowPrev->setCheckable(true);
    m_actShowPrev->setToolTip(tr("Show ghost outlines from previous profile"));

    m_actShowNext = m_profileToolBar->addAction(tr("Ghost Next"), [this](bool checked) {
        if (m_activeProfileView) {
            m_activeProfileView->setShowNextProfile(checked);
        } else {
            m_doc->setShowNextProfile(checked);
        }
    });
    m_actShowNext->setCheckable(true);
    m_actShowNext->setToolTip(tr("Show ghost outlines from next profile"));

    m_actShowGridLines = m_profileToolBar->addAction(tr("Grid Lines"), [this](bool checked) {
        if (m_activeProfileView) {
            m_activeProfileView->setShowGridLines(checked);
        } else {
            m_doc->setShowGridLines(checked);
        }
    });
    m_actShowGridLines->setCheckable(true);
    m_actShowGridLines->setChecked(true);
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

void ToolbarsManager::setupFieldToolBar(QWidget *parent) {
    m_fieldToolBar = new QToolBar(tr("Field Toolbar"), parent);
    m_fieldToolBar->setObjectName("FieldToolBar");

    m_actShowModeled = m_fieldToolBar->addAction(tr("Modeled"), [this](bool checked) {
        m_doc->setShowModeledField(checked);
    });
    m_actShowModeled->setCheckable(true);
    m_actShowModeled->setChecked(true);

    m_actShowMeasured = m_fieldToolBar->addAction(tr("Measured"), [this](bool checked) {
        m_doc->setShowMeasuredField(checked);
    });
    m_actShowMeasured->setCheckable(true);
    m_actShowMeasured->setChecked(true);

    m_actShowDiff = m_fieldToolBar->addAction(tr("Diff"), [this](bool checked) {
        m_doc->setShowDifferenceField(checked);
    });
    m_actShowDiff->setCheckable(true);
    m_actShowDiff->setChecked(true);

    m_fieldToolBar->addSeparator();

    m_fieldCompGroup = new QActionGroup(this);

    auto addCompAction = [this](const QString &text, mod3d::FieldComponent comp, bool checked = false) {
        auto *act = m_fieldToolBar->addAction(text, [this, comp]() {
            m_doc->setActiveField(comp);
        });
        act->setCheckable(true);
        act->setChecked(checked);
        act->setData(static_cast<int>(comp));
        m_fieldCompGroup->addAction(act);
        return act;
    };

    // Gravity components
    addCompAction("Gx", mod3d::FieldComponent::GX);
    addCompAction("Gy", mod3d::FieldComponent::GY);
    addCompAction("Gz", mod3d::FieldComponent::GZ, true);
    addCompAction("G tot", mod3d::FieldComponent::G_TOT);

    m_fieldToolBar->addSeparator();

    // Magnetic components
    addCompAction("Mx", mod3d::FieldComponent::MX);
    addCompAction("My", mod3d::FieldComponent::MY);
    addCompAction("Mz", mod3d::FieldComponent::MZ);
    addCompAction("ΔT", mod3d::FieldComponent::DELTA_T);

    m_fieldToolBar->addSeparator();

    // Tensor components
    addCompAction("Txx", mod3d::FieldComponent::GXX);
    addCompAction("Tyy", mod3d::FieldComponent::GYY);
    addCompAction("Tzz", mod3d::FieldComponent::GZZ);
    addCompAction("Txy", mod3d::FieldComponent::GXY);
    addCompAction("Txz", mod3d::FieldComponent::GXZ);
    addCompAction("Tyz", mod3d::FieldComponent::GYZ);
}

void ToolbarsManager::updateProfileWidgets() {
    ProfileOrientation orient = m_activeProfileView ? m_activeProfileView->profileOrientation() : m_doc->profileOrientation();
    int prfIdx = m_activeProfileView ? m_activeProfileView->profileIndex() : m_doc->profileIndex();
    int maxIdx = m_activeProfileView ? m_activeProfileView->maxProfileIndex() : m_doc->maxProfileIndex();
    double coord = m_activeProfileView ? m_activeProfileView->currentProfileCoordinate() : m_doc->currentProfileCoordinate();
    bool showPrev = m_activeProfileView ? m_activeProfileView->showPreviousProfile() : m_doc->showPreviousProfile();
    bool showNext = m_activeProfileView ? m_activeProfileView->showNextProfile() : m_doc->showNextProfile();
    bool showGrid = m_activeProfileView ? m_activeProfileView->showGridLines() : m_doc->showGridLines();

    bool isEW = (orient == ProfileOrientation::EastWest);
    m_actProfEW->setChecked(isEW);
    m_actProfSN->setChecked(!isEW);

    m_spinProfileIndex->blockSignals(true);
    m_spinProfileIndex->setMaximum(maxIdx);
    m_spinProfileIndex->setValue(prfIdx);
    m_spinProfileIndex->blockSignals(false);

    m_lblProfileCoord->setText(QString("(%1 = %2 m)").arg(isEW ? "Y" : "X").arg(coord, 0, 'f', 0));

    m_actShowPrev->setChecked(showPrev);
    m_actShowNext->setChecked(showNext);
    m_actShowGridLines->setChecked(showGrid);
}

void ToolbarsManager::updateFieldWidgets() {
    m_actShowModeled->setChecked(m_doc->showModeledField());
    m_actShowMeasured->setChecked(m_doc->showMeasuredField());
    m_actShowDiff->setChecked(m_doc->showDifferenceField());

    int targetComp = static_cast<int>(m_doc->activeField());
    for (auto *act : m_fieldCompGroup->actions()) {
        if (act->data().toInt() == targetComp) {
            act->setChecked(true);
            break;
        }
    }
}

} // namespace mod3d_gui
