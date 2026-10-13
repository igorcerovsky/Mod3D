#pragma once

#include "mod3d_gui/ModelDocument.h"
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QPoint>

class QMdiArea;

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

    [[nodiscard]] bool wireframeMode() const noexcept { return m_wireframe; }
    void setWireframeMode(bool wireframe);

    [[nodiscard]] bool showRelief() const noexcept { return m_showRelief; }
    void setShowRelief(bool show);

    [[nodiscard]] bool showProfilePlane() const noexcept { return m_showProfilePlane; }
    void setShowProfilePlane(bool show);

    [[nodiscard]] bool showFeatureEdges() const noexcept { return m_showFeatureEdges; }
    void setShowFeatureEdges(bool show);

    [[nodiscard]] bool showBottom() const noexcept { return m_showBottom; }
    void setShowBottom(bool show);

    [[nodiscard]] bool showOuterFacets() const noexcept { return m_showOuterFacets; }
    void setShowOuterFacets(bool show);

    [[nodiscard]] bool showBoundingBox() const noexcept { return m_showBoundingBox; }
    void setShowBoundingBox(bool show);

    [[nodiscard]] bool showOrientationIndicator() const noexcept { return m_showOrientationIndicator; }
    void setShowOrientationIndicator(bool show);
    [[nodiscard]] float pitch() const noexcept { return m_pitch; }
    [[nodiscard]] float azimuth() const noexcept { return m_azimuth; }
    void setCameraAngles(float pitch, float azimuth) { m_pitch = pitch; m_azimuth = azimuth; update(); }

    void invalidateMeshes() noexcept { m_meshDirty = true; update(); }

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void drawBoundingBox();
    void drawBodies();
    void drawProfilePlane();
    void drawRelief();
    void drawOrientationHud();
    void showContextMenu(const QPoint &globalPos);
    QMdiArea *findMdiArea() const;

    ModelDocument *m_doc;

    // Cached 3D Polyhedral Meshes
    std::vector<mod3d::BodyMesh3D> m_bodyMeshes;
    bool m_meshDirty{true};

    // Turntable / Orbit camera state (standard across Blender, CAD, ParaView, Google Earth)
    // m_pitch: Elevation angle from horizontal plane, in degrees [-89.0f, +89.0f]
    // m_azimuth: Azimuth angle around vertical Z axis, in degrees (continuous 360)
    float m_pitch{30.0f};
    float m_azimuth{-45.0f};
    float m_panX{0.0f};
    float m_panY{0.0f};
    float m_zoom{1.0f};

    QPoint m_lastMousePos;
    QPoint m_dragStartPos;
    bool m_mouseDragging{false};
    Qt::MouseButton m_dragButton{Qt::NoButton};

    // Render toggles
    bool m_wireframe{false};
    bool m_showFeatureEdges{true};
    bool m_showBottom{false};
    bool m_showOuterFacets{false};
    bool m_showRelief{true};
    bool m_showProfilePlane{true};
    bool m_showBoundingBox{true};
    bool m_showOrientationIndicator{true};
};

} // namespace mod3d_gui
