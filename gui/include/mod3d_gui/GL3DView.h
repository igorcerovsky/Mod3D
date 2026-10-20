#pragma once

#include "mod3d_gui/ModelDocument.h"
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QPoint>
#include <QVector3D>
#include <QMatrix4x4>
#include <QRect>

class QMdiArea;
class QVariantAnimation;
class QOpenGLShaderProgram;

namespace mod3d_gui {

class GL3DView : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit GL3DView(ModelDocument *doc, QWidget *parent = nullptr);
    ~GL3DView() override;

    void setViewEast();
    void setViewWest();
    void setViewNorth();
    void setViewSouth();
    void setViewTopMap();
    void resetCamera();
    void fitToView();

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

    [[nodiscard]] bool showDepthFog() const noexcept { return m_showDepthFog; }
    void setShowDepthFog(bool fog);

    [[nodiscard]] float zScale() const noexcept { return m_zScale; }
    void setZScale(float zScale);

    [[nodiscard]] bool isOrthographic() const noexcept { return m_orthographic; }
    void setOrthographic(bool ortho);

    [[nodiscard]] float pitch() const noexcept { return m_pitch; }
    [[nodiscard]] float azimuth() const noexcept { return m_azimuth; }
    void setCameraAngles(float pitch, float azimuth);

    [[nodiscard]] const QVector3D &target() const noexcept { return m_target; }
    void setTarget(const QVector3D &target);

    [[nodiscard]] float cameraDistance() const noexcept { return m_distance; }
    void setCameraDistance(float dist);

    [[nodiscard]] float zoom() const noexcept { return m_zoom; }
    void setZoom(float zoom);

    void animateToCamera(float targetPitch, float targetAzimuth, float targetDist = -1.0f, const QVector3D *targetLookAt = nullptr, int durationMs = 250);

    void invalidateMeshes() noexcept { m_meshDirty = true; update(); }

signals:
    void cameraChanged();
    void zScaleChanged(float zScale);
    void projectionChanged(bool isOrtho);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void paintEvent(QPaintEvent *event) override;

public:
    void paintSoftware(QPainter &painter);

    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void setupMatrices(int w, int h);
    void drawBoundingBox();
    void drawBodies();
    void drawProfilePlane();
    void drawRelief();
    void drawOrientationHud();
    void drawPivotIndicator();
    void showContextMenu(const QPoint &globalPos);
    QMdiArea *findMdiArea() const;

    void ensureDefaultTargetAndDistance();
    QVector3D screenToWorldPlane(const QPoint &pos, float planeZ = 0.0f);
    int hitTestGizmo(const QPoint &pos);

    ModelDocument *m_doc;

    // Cached 3D Polyhedral Meshes
    std::vector<mod3d::BodyMesh3D> m_bodyMeshes;
    bool m_meshDirty{true};

    // Camera LookAt / Turntable state
    QVector3D m_target{0.0f, 0.0f, 0.0f};
    bool m_targetInitialized{false};
    float m_pitch{30.0f};
    float m_azimuth{-45.0f};
    float m_distance{10000.0f};
    float m_zoom{1.0f};
    float m_panX{0.0f}; // Legacy compatibility offset
    float m_panY{0.0f};

    // Controls & Visual Options
    float m_zScale{1.0f};
    bool m_orthographic{false};
    bool m_showDepthFog{true};

    // Interaction state
    QPoint m_lastMousePos;
    QPoint m_dragStartPos;
    bool m_mouseDragging{false};
    Qt::MouseButton m_dragButton{Qt::NoButton};
    bool m_draggingGizmo{false};
    int m_gizmoHoverAxis{-1};
    QRect m_gizmoRect;

    // Smooth animation
    QVariantAnimation *m_camAnim{nullptr};

    // Shaders
    QOpenGLShaderProgram *m_shaderProg{nullptr};
    bool m_shadersInitialized{false};

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

