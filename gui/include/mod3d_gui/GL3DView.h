#pragma once

#include "mod3d_gui/ModelDocument.h"
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QPoint>
#include <QVector3D>
#include <QMatrix4x4>
#include <QRect>

#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLShaderProgram>

class QMdiArea;
#include <QVariantAnimation>
#include <QPointer>
#include "mod3d_gui/ProfileView.h"

namespace mod3d_gui {

class GL3DView : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    struct BodyRange {
        int bodyId{0};
        int first{0};
        int count{0};
        bool isTransparent{false};
    };

    struct Vertex3D {
        float x, y, z;
        float nx, ny, nz;
        float r, g, b, a;
    };

    struct FlatVertex3D {
        float x, y, z;
        float r, g, b, a;
    };

    explicit GL3DView(ModelDocument *doc, QWidget *parent = nullptr);
    ~GL3DView() override;

    void setViewEast();
    void setViewWest();
    void setViewNorth();
    void setViewSouth();
    void setViewTopMap();
    void resetCamera();
    void fitToView();

    enum class ReliefMode {
        Surface,
        Wireframe
    };

    [[nodiscard]] bool wireframeMode() const noexcept { return m_wireframe; }
    void setWireframeMode(bool wireframe);

    [[nodiscard]] bool showRelief() const noexcept { return m_showRelief; }
    void setShowRelief(bool show);

    [[nodiscard]] ReliefMode reliefMode() const noexcept { return m_reliefMode; }
    void setReliefMode(ReliefMode mode);

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

    [[nodiscard]] bool showTransparentBodies() const noexcept { return m_transparentBodies; }
    void setShowTransparentBodies(bool enable);

    [[nodiscard]] bool showTransparentProfilePlane() const noexcept { return m_transparentProfiles; }
    void setShowTransparentProfilePlane(bool enable);

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

    [[nodiscard]] int selectedBodyId() const noexcept { return m_selectedBodyId; }
    void setSelectedBodyId(int bodyId);
    int hitTestBody(const QPoint &pos);
    [[nodiscard]] bool shadersInitialized() const noexcept { return m_shadersInitialized; }

    void animateToCamera(float targetPitch, float targetAzimuth, float targetDist = -1.0f, const QVector3D *targetLookAt = nullptr, int durationMs = 250);

    void invalidateMeshes() noexcept { m_meshDirty = true; update(); }
    [[nodiscard]] const std::vector<mod3d::BodyMesh3D> &bodyMeshes() const noexcept { return m_bodyMeshes; }

signals:
    void cameraChanged();
    void zScaleChanged(float zScale);
    void projectionChanged(bool isOrtho);
    void bodySelected(int bodyId);
    void reliefModeChanged(ReliefMode mode);

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
    void leaveEvent(QEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    void setupMatrices(int w, int h);
    void initShaders();
    void initBuffers();
    void updateBodyBuffers();
    void renderLines(const std::vector<FlatVertex3D> &lines, float lineWidth = 1.0f);
    void renderFlatTriangles(const std::vector<FlatVertex3D> &tris, bool blend = false);

    void drawBoundingBox();
    void drawBodies();
    void drawProfilePlane();
    void drawRelief();
    void drawOrientationHud(QPainter &painter);
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
    bool m_showDepthFog{false};

    // Interaction state
    QPoint m_lastMousePos;
    QPoint m_dragStartPos;
    bool m_mouseDragging{false};
    Qt::MouseButton m_dragButton{Qt::NoButton};
    bool m_draggingGizmo{false};
    int m_gizmoHoverAxis{-1};
    QRect m_gizmoRect;
    int m_selectedBodyId{-1};

    // Matrices
    QMatrix4x4 m_projMatrix;
    QMatrix4x4 m_mvMatrix;

    // Smooth animation
    QPointer<QVariantAnimation> m_camAnim{nullptr};

    // Modern Shaders
    QOpenGLShaderProgram *m_shaderProg{nullptr};
    QOpenGLShaderProgram *m_flatShaderProg{nullptr};
    bool m_shadersInitialized{false};

    // GPU Buffers & VAOs
    std::vector<BodyRange> m_bodyRanges;
    QOpenGLVertexArrayObject m_bodyVao;
    QOpenGLBuffer m_bodyVbo{QOpenGLBuffer::VertexBuffer};

    QOpenGLVertexArrayObject m_transVao;
    QOpenGLBuffer m_transVbo{QOpenGLBuffer::VertexBuffer};

    QOpenGLVertexArrayObject m_linesVao;
    QOpenGLBuffer m_linesVbo{QOpenGLBuffer::VertexBuffer};

    QOpenGLVertexArrayObject m_flatTrisVao;
    QOpenGLBuffer m_flatTrisVbo{QOpenGLBuffer::VertexBuffer};

    bool m_buffersInitialized{false};

    // Render toggles
    bool m_wireframe{false};
    bool m_showFeatureEdges{true};
    bool m_showBottom{false};
    bool m_showOuterFacets{false};
    bool m_showRelief{true};
    ReliefMode m_reliefMode{ReliefMode::Wireframe};
    bool m_showProfilePlane{true};
    bool m_showBoundingBox{true};
    bool m_showOrientationIndicator{true};
    bool m_transparentBodies{false};
    bool m_transparentProfiles{true};
    QPointer<ProfileView> m_lastActiveProfileView{nullptr};
};

} // namespace mod3d_gui

