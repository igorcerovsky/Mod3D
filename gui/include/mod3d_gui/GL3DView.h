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

    [[nodiscard]] bool showPlanarPlane() const noexcept { return m_showPlanarPlane; }
    void setShowPlanarPlane(bool enable);

    [[nodiscard]] bool octantCut() const noexcept { return m_octantCut; }
    void setOctantCut(bool enable);

    // Backward-compatible quadrantCut aliases
    [[nodiscard]] bool quadrantCut() const noexcept { return octantCut(); }
    void setQuadrantCut(bool enable) { setOctantCut(enable); }

    struct OctantCutState {
        bool active{false};

        // East-West profile plane (Y = ewY)
        bool hasEW{false};
        int ewRow{1};
        double ewY{0.0};
        bool cutPosY{false};

        // South-North profile plane (X = snX)
        bool hasSN{false};
        int snCol{1};
        double snX{0.0};
        bool cutPosX{true};

        // Horizontal Planar plane (Z = planarZ)
        bool hasPlanar{false};
        double planarZ{0.0};
        bool cutPosZ{true}; // Cut Z > planarZ when viewing from above

        bool operator==(const OctantCutState &o) const noexcept {
            return active == o.active &&
                   hasEW == o.hasEW && ewRow == o.ewRow && cutPosY == o.cutPosY &&
                   hasSN == o.hasSN && snCol == o.snCol && cutPosX == o.cutPosX &&
                   hasPlanar == o.hasPlanar && std::abs(planarZ - o.planarZ) < 1e-4 && cutPosZ == o.cutPosZ;
        }
        bool operator!=(const OctantCutState &o) const noexcept {
            return !(*this == o);
        }

        [[nodiscard]] bool inCutX(double x, double eps = 1e-3) const noexcept {
            if (!hasSN) return true;
            return cutPosX ? (x > snX + eps) : (x < snX - eps);
        }

        [[nodiscard]] bool inCutY(double y, double eps = 1e-3) const noexcept {
            if (!hasEW) return true;
            return cutPosY ? (y > ewY + eps) : (y < ewY - eps);
        }

        [[nodiscard]] bool inCutZ(double z, double eps = 1e-3) const noexcept {
            if (!hasPlanar) return true;
            return cutPosZ ? (z > planarZ + eps) : (z < planarZ - eps);
        }

        [[nodiscard]] bool isPointCut(double x, double y, double z, double eps = 1e-3) const noexcept {
            if (!active) return false;
            return inCutX(x, eps) && inCutY(y, eps) && inCutZ(z, eps);
        }

        // Backward-compatible 2D overload
        [[nodiscard]] bool isPointCut(double x, double y) const noexcept {
            if (!active) return false;
            return inCutX(x) && inCutY(y);
        }

        [[nodiscard]] bool isFacetOnCutWall(const mod3d::Facet3Pt &fct, double eps = 1e-3) const noexcept {
            if (!active) return false;
            double xm = (fct[0].x + fct[1].x + fct[2].x) / 3.0;
            double ym = (fct[0].y + fct[1].y + fct[2].y) / 3.0;
            double zm = (fct[0].z + fct[1].z + fct[2].z) / 3.0;

            if (hasEW &&
                std::abs(fct[0].y - ewY) <= eps &&
                std::abs(fct[1].y - ewY) <= eps &&
                std::abs(fct[2].y - ewY) <= eps &&
                inCutX(xm, eps) && inCutZ(zm, eps)) {
                return true;
            }
            if (hasSN &&
                std::abs(fct[0].x - snX) <= eps &&
                std::abs(fct[1].x - snX) <= eps &&
                std::abs(fct[2].x - snX) <= eps &&
                inCutY(ym, eps) && inCutZ(zm, eps)) {
                return true;
            }
            if (hasPlanar &&
                std::abs(fct[0].z - planarZ) <= eps &&
                std::abs(fct[1].z - planarZ) <= eps &&
                std::abs(fct[2].z - planarZ) <= eps &&
                inCutX(xm, eps) && inCutY(ym, eps)) {
                return true;
            }
            return false;
        }

        [[nodiscard]] bool isSegmentOnCutWall(const mod3d::Point3D &p0, const mod3d::Point3D &p1, double eps = 1e-3) const noexcept {
            if (!active) return false;
            double xm = 0.5 * (p0.x + p1.x);
            double ym = 0.5 * (p0.y + p1.y);
            double zm = 0.5 * (p0.z + p1.z);

            if (hasEW && std::abs(p0.y - ewY) <= eps && std::abs(p1.y - ewY) <= eps && inCutX(xm, -eps) && inCutZ(zm, -eps)) {
                return true;
            }
            if (hasSN && std::abs(p0.x - snX) <= eps && std::abs(p1.x - snX) <= eps && inCutY(ym, -eps) && inCutZ(zm, -eps)) {
                return true;
            }
            if (hasPlanar && std::abs(p0.z - planarZ) <= eps && std::abs(p1.z - planarZ) <= eps && inCutX(xm, -eps) && inCutY(ym, -eps)) {
                return true;
            }
            return false;
        }
    };

    using QuadrantCutState = OctantCutState;

    [[nodiscard]] OctantCutState computeOctantCutState() const;
    [[nodiscard]] OctantCutState computeQuadrantCutState() const { return computeOctantCutState(); }

    void generateBodyCutCaps(int bodyId, const OctantCutState &cut,
                             std::vector<mod3d::Facet3Pt> &outCapFacets,
                             std::vector<std::pair<mod3d::Point3D, mod3d::Point3D>> &outCapEdges) const;

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
    void octantCutChanged(bool enabled);
    void quadrantCutChanged(bool enabled);

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
    void drawPlanarPlane();
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
    bool m_showOuterFacets{false};
    bool m_showRelief{true};
    ReliefMode m_reliefMode{ReliefMode::Wireframe};
    bool m_showProfilePlane{true};
    bool m_showPlanarPlane{true};
    bool m_showBoundingBox{true};
    bool m_showOrientationIndicator{true};
    bool m_transparentBodies{false};
    bool m_transparentProfiles{true};
    bool m_octantCut{false};
    OctantCutState m_cachedCutState{};
    QPointer<ProfileView> m_lastActiveProfileView{nullptr};
};

} // namespace mod3d_gui

