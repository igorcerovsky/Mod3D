#pragma once

#include "mod3d_gui/ModelDocument.h"
#include <QObject>
#include <QToolBar>
#include <QAction>
#include <QActionGroup>
#include <QSpinBox>
#include <QLabel>

#include <QPointer>

namespace mod3d_gui {

class ProfileView;

QIcon createZoomIcon(bool zoomIn);

class ToolbarsManager : public QObject {
    Q_OBJECT

public:
    explicit ToolbarsManager(ModelDocument *doc, QWidget *parent);

    QToolBar *mainToolBar() const { return m_mainToolBar; }
    QToolBar *profileToolBar() const { return m_profileToolBar; }
    QToolBar *mapToolBar() const { return m_mapToolBar; }
    QToolBar *fieldToolBar() const { return nullptr; }

    ProfileView *activeProfileView() const { return m_activeProfileView.data(); }
    void setActiveProfileView(ProfileView *pv);

    // Synchronize toolbar widgets with document state
    void updateProfileWidgets();
    void updateFieldWidgets() {}

signals:
    void actionTriggered(const QString &actionName);

private:
    void setupMainToolBar(QWidget *parent);
    void setupProfileToolBar(QWidget *parent);
    void setupMapToolBar(QWidget *parent);

    ModelDocument *m_doc;

    QToolBar *m_mainToolBar{nullptr};
    QToolBar *m_profileToolBar{nullptr};
    QToolBar *m_mapToolBar{nullptr};

    // Profile widgets
    QPointer<ProfileView> m_activeProfileView{nullptr};
    QAction *m_actProfEW{nullptr};
    QAction *m_actProfSN{nullptr};
    QAction *m_actProfPrev{nullptr};
    QAction *m_actProfNext{nullptr};
    QSpinBox *m_spinProfileIndex{nullptr};
    QLabel *m_lblProfileCoord{nullptr};
};

} // namespace mod3d_gui
