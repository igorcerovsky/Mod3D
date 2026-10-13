#pragma once

#include "mod3d_gui/ModelDocument.h"
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QPoint>

namespace mod3d_gui {

class GL3DView : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit GL3DView(ModelDocument *doc, QWidget *parent = nullptr);
    ~GL3DView() override = default;

    void setViewEast();
    void setViewWest();
    void setViewNorth();
    void setViewSouth();
    void setViewTopMap();
    void resetCamera();

    void setWireframeMode(bool wireframe);
    void setShowRelief(bool show);
    void setShowProfilePlane(bool show);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void drawBoundingBox();
    void drawBodies();
    void drawProfilePlane();
    void drawRelief();

    ModelDocument *m_doc;

    // Camera trackball state
    float m_rotX{30.0f};
    float m_rotY{-45.0f};
    float m_rotZ{0.0f};
    float m_panX{0.0f};
    float m_panY{0.0f};
    float m_zoom{1.0f};

    QPoint m_lastMousePos;
    bool m_mouseDragging{false};

    // Render toggles
    bool m_wireframe{false};
    bool m_showRelief{true};
    bool m_showProfilePlane{true};
};

} // namespace mod3d_gui
