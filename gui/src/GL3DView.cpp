#include "mod3d_gui/GL3DView.h"
#include "mod3d_gui/ProfileView.h"
#include "mod3d_gui/MainWindow.h"
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QVariantAnimation>
#include <QEasingCurve>
#include <QOpenGLShaderProgram>
#include <QPainter>
#include <QApplication>
#include <cmath>
#include <algorithm>
#include <array>
#include <string>

namespace mod3d_gui {

namespace {

template <typename Emitter>
void emitStrokeChar(char c, float cx, float cy, float size, Emitter &&outSeg) {
    float h = size * 0.5f;
    float w = size * 0.35f;
    float x0 = cx - w;
    float x1 = cx + w;
    float y0 = cy - h;
    float y1 = cy + h;
    float ym = cy;
    float xm = cx;

    auto seg = [&](float ax, float ay, float bx, float by) {
        outSeg(ax, ay, bx, by);
    };

    switch (c) {
    case 'N':
        seg(x0, y0, x0, y1);
        seg(x0, y1, x1, y0);
        seg(x1, y0, x1, y1);
        break;
    case 'S':
        seg(x1, y1, x0, y1);
        seg(x0, y1, x0, ym);
        seg(x0, ym, x1, ym);
        seg(x1, ym, x1, y0);
        seg(x1, y0, x0, y0);
        break;
    case 'E':
        seg(x1, y1, x0, y1);
        seg(x0, y1, x0, y0);
        seg(x0, y0, x1, y0);
        seg(x0, ym, xm + w * 0.2f, ym);
        break;
    case 'W':
        seg(x0, y1, x0 + w * 0.4f, y0);
        seg(x0 + w * 0.4f, y0, xm, ym);
        seg(xm, ym, x1 - w * 0.4f, y0);
        seg(x1 - w * 0.4f, y0, x1, y1);
        break;
    case 'U':
        seg(x0, y1, x0, y0);
        seg(x0, y0, x1, y0);
        seg(x1, y0, x1, y1);
        break;
    case 'P':
        seg(x0, y0, x0, y1);
        seg(x0, y1, x1, y1);
        seg(x1, y1, x1, ym);
        seg(x1, ym, x0, ym);
        break;
    case 'O':
        seg(x0, y0, x0, y1);
        seg(x0, y1, x1, y1);
        seg(x1, y1, x1, y0);
        seg(x1, y0, x0, y0);
        break;
    case 'R':
        seg(x0, y0, x0, y1);
        seg(x0, y1, x1, y1);
        seg(x1, y1, x1, ym);
        seg(x1, ym, x0, ym);
        seg(xm, ym, x1, y0);
        break;
    case 'T':
        seg(x0, y1, x1, y1);
        seg(xm, y1, xm, y0);
        break;
    case 'H':
        seg(x0, y0, x0, y1);
        seg(x1, y0, x1, y1);
        seg(x0, ym, x1, ym);
        break;
    case 'A':
        seg(x0, y0, xm, y1);
        seg(xm, y1, x1, y0);
        seg(x0 + w * 0.35f, ym - h * 0.15f, x1 - w * 0.35f, ym - h * 0.15f);
        break;
    case 'Z':
        seg(x0, y1, x1, y1);
        seg(x1, y1, x0, y0);
        seg(x0, y0, x1, y0);
        break;
    case 'B':
        seg(x0, y0, x0, y1);
        seg(x0, y1, x1 - w * 0.2f, y1);
        seg(x1 - w * 0.2f, y1, x1, ym + h * 0.5f);
        seg(x1, ym + h * 0.5f, x0, ym);
        seg(x0, ym, x1 - w * 0.2f, ym);
        seg(x1 - w * 0.2f, ym, x1, ym - h * 0.5f);
        seg(x1, ym - h * 0.5f, x0, y0);
        break;
    case '0':
        seg(x0, y0, x1, y0);
        seg(x1, y0, x1, y1);
        seg(x1, y1, x0, y1);
        seg(x0, y1, x0, y0);
        break;
    case '1':
        seg(xm, y0, xm, y1);
        seg(x0, y1 - h * 0.25f, xm, y1);
        seg(x0 + w * 0.2f, y0, x1 - w * 0.2f, y0);
        break;
    case '2':
        seg(x0, y1, x1, y1);
        seg(x1, y1, x1, ym);
        seg(x1, ym, x0, y0);
        seg(x0, y0, x1, y0);
        break;
    case '3':
        seg(x0, y1, x1, y1);
        seg(x1, y1, x1, ym);
        seg(x0 + w * 0.25f, ym, x1, ym);
        seg(x1, ym, x1, y0);
        seg(x1, y0, x0, y0);
        break;
    case '4':
        seg(x0, y1, x0, ym);
        seg(x0, ym, x1, ym);
        seg(x1 - w * 0.2f, y1, x1 - w * 0.2f, y0);
        break;
    case '5':
        seg(x1, y1, x0, y1);
        seg(x0, y1, x0, ym);
        seg(x0, ym, x1, ym);
        seg(x1, ym, x1, y0);
        seg(x1, y0, x0, y0);
        break;
    case '6':
        seg(x1, y1, x0, y1);
        seg(x0, y1, x0, y0);
        seg(x0, y0, x1, y0);
        seg(x1, y0, x1, ym);
        seg(x1, ym, x0, ym);
        break;
    case '7':
        seg(x0, y1, x1, y1);
        seg(x1, y1, x0 + w * 0.25f, y0);
        break;
    case '8':
        seg(x0, y0, x1, y0);
        seg(x1, y0, x1, y1);
        seg(x1, y1, x0, y1);
        seg(x0, y1, x0, y0);
        seg(x0, ym, x1, ym);
        break;
    case '9':
        seg(x0, ym, x1, ym);
        seg(x0, ym, x0, y1);
        seg(x0, y1, x1, y1);
        seg(x1, y1, x1, y0);
        seg(x1, y0, x0, y0);
        break;
    case '-':
        seg(x0, ym, x1, ym);
        break;
    case '+':
        seg(x0, ym, x1, ym);
        seg(xm, y0 + h * 0.15f, xm, y1 - h * 0.15f);
        break;
    case '.':
        seg(xm - w * 0.2f, y0, xm + w * 0.2f, y0);
        break;
    case '(':
        seg(x1, y1, x0, ym);
        seg(x0, ym, x1, y0);
        break;
    case ')':
        seg(x0, y1, x1, ym);
        seg(x1, ym, x0, y0);
        break;
    case 'm':
        seg(x0, y0, x0, ym + h * 0.1f);
        seg(x0, ym + h * 0.1f, xm, ym + h * 0.1f);
        seg(xm, ym + h * 0.1f, xm, y0);
        seg(xm, ym + h * 0.1f, x1, ym + h * 0.1f);
        seg(x1, ym + h * 0.1f, x1, y0);
        break;
    case 'k':
        seg(x0, y0, x0, y1);
        seg(x0, ym, x1, y1);
        seg(x0, ym, x1, y0);
        break;
    case 'X':
        seg(x0, y0, x1, y1);
        seg(x0, y1, x1, y0);
        break;
    case 'Y':
        seg(x0, y1, xm, ym);
        seg(x1, y1, xm, ym);
        seg(xm, ym, xm, y0);
        break;
    case ' ':
    default:
        break;
    }
}

void addBillboardLabel(std::vector<GL3DView::FlatVertex3D> &lines,
                       const std::string &text,
                       float x, float y, float z,
                       float r, float g, float b, float a,
                       float size,
                       const QVector3D &camRight,
                       const QVector3D &camUp,
                       float spacing = 0.85f) {
    if (text.empty()) return;

    float step = size * spacing;
    float totalW = static_cast<float>(text.size() - 1) * step;
    float startX = -totalW * 0.5f;

    QVector3D center(x, y, z);

    for (size_t i = 0; i < text.size(); ++i) {
        float cx = startX + static_cast<float>(i) * step;
        emitStrokeChar(text[i], cx, 0.0f, size, [&](float ax, float ay, float bx, float by) {
            QVector3D p0 = center + camRight * ax + camUp * ay;
            QVector3D p1 = center + camRight * bx + camUp * by;
            lines.push_back({ p0.x(), p0.y(), p0.z(), r, g, b, a });
            lines.push_back({ p1.x(), p1.y(), p1.z(), r, g, b, a });
        });
    }
}

void addArrowCone(std::vector<GL3DView::FlatVertex3D> &tris,
                  float tipX, float tipY, float tipZ,
                  float dirX, float dirY, float dirZ,
                  float length, float radius,
                  float r, float g, float b, float a = 1.0f,
                  int slices = 12) {
    float len = std::sqrt(dirX * dirX + dirY * dirY + dirZ * dirZ);
    if (len < 1e-6f) return;
    dirX /= len; dirY /= len; dirZ /= len;

    QVector3D tip(tipX, tipY, tipZ);
    QVector3D baseCenter = tip - QVector3D(dirX, dirY, dirZ) * length;

    // Perpendicular vectors u and v
    float ux, uy, uz;
    if (std::abs(dirZ) < 0.9f) {
        ux = dirY; uy = -dirX; uz = 0.0f;
    } else {
        ux = 0.0f; uy = dirZ; uz = -dirY;
    }
    float uLen = std::sqrt(ux * ux + uy * uy + uz * uz);
    if (uLen > 1e-6f) { ux /= uLen; uy /= uLen; uz /= uLen; }

    float vx = dirY * uz - dirZ * uy;
    float vy = dirZ * ux - dirX * uz;
    float vz = dirX * uy - dirY * ux;
    QVector3D u(ux, uy, uz);
    QVector3D v(vx, vy, vz);

    std::vector<QVector3D> circlePts(slices + 1);
    for (int i = 0; i <= slices; ++i) {
        float angle = 2.0f * 3.14159265f * static_cast<float>(i) / static_cast<float>(slices);
        float ca = std::cos(angle) * radius;
        float sa = std::sin(angle) * radius;
        circlePts[i] = baseCenter + u * ca + v * sa;
    }

    for (int i = 0; i < slices; ++i) {
        // Cone side triangle
        tris.push_back({ tip.x(), tip.y(), tip.z(), r, g, b, a });
        tris.push_back({ circlePts[i].x(), circlePts[i].y(), circlePts[i].z(), r, g, b, a });
        tris.push_back({ circlePts[i + 1].x(), circlePts[i + 1].y(), circlePts[i + 1].z(), r, g, b, a });

        // Base cap triangle
        tris.push_back({ baseCenter.x(), baseCenter.y(), baseCenter.z(), r, g, b, a });
        tris.push_back({ circlePts[i + 1].x(), circlePts[i + 1].y(), circlePts[i + 1].z(), r, g, b, a });
        tris.push_back({ circlePts[i].x(), circlePts[i].y(), circlePts[i].z(), r, g, b, a });
    }
}

struct ReliefColorPeg {
    float pos;
    float r, g, b;
};

// Standard CTBL_REL topographic relief hypsometric colormap (11 pegs from 0.0 to 1.0)
static const std::array<ReliefColorPeg, 11> kReliefPegs = {{
    {0.0f, 221.0f / 255.0f, 250.0f / 255.0f, 218.0f / 255.0f}, // light valley green
    {0.1f, 168.0f / 255.0f, 251.0f / 255.0f, 185.0f / 255.0f}, // pale green
    {0.2f,  27.0f / 255.0f, 206.0f / 255.0f,  96.0f / 255.0f}, // emerald meadow green
    {0.3f,  58.0f / 255.0f, 160.0f / 255.0f,  81.0f / 255.0f}, // forest green
    {0.4f, 238.0f / 255.0f, 247.0f / 255.0f, 189.0f / 255.0f}, // yellow-green plateau
    {0.5f, 236.0f / 255.0f, 230.0f / 255.0f, 168.0f / 255.0f}, // warm ochre tan
    {0.6f, 231.0f / 255.0f, 219.0f / 255.0f, 167.0f / 255.0f}, // sandstone
    {0.7f, 161.0f / 255.0f, 127.0f / 255.0f, 111.0f / 255.0f}, // mountain rock / brown
    {0.8f, 108.0f / 255.0f,  74.0f / 255.0f,  76.0f / 255.0f}, // alpine dark brown
    {0.9f, 147.0f / 255.0f, 137.0f / 255.0f, 133.0f / 255.0f}, // crag / slate
    {1.0f, 249.0f / 255.0f, 245.0f / 255.0f, 231.0f / 255.0f}  // snow-capped white
}};

inline void getReliefColor(float t, float &r, float &g, float &b) {
    t = std::clamp(t, 0.0f, 1.0f);
    if (t <= kReliefPegs.front().pos) {
        r = kReliefPegs.front().r;
        g = kReliefPegs.front().g;
        b = kReliefPegs.front().b;
        return;
    }
    if (t >= kReliefPegs.back().pos) {
        r = kReliefPegs.back().r;
        g = kReliefPegs.back().g;
        b = kReliefPegs.back().b;
        return;
    }
    for (size_t i = 0; i + 1 < kReliefPegs.size(); ++i) {
        if (t >= kReliefPegs[i].pos && t <= kReliefPegs[i + 1].pos) {
            float frac = (t - kReliefPegs[i].pos) / (kReliefPegs[i + 1].pos - kReliefPegs[i].pos);
            r = kReliefPegs[i].r + frac * (kReliefPegs[i + 1].r - kReliefPegs[i].r);
            g = kReliefPegs[i].g + frac * (kReliefPegs[i + 1].g - kReliefPegs[i].g);
            b = kReliefPegs[i].b + frac * (kReliefPegs[i + 1].b - kReliefPegs[i].b);
            return;
        }
    }
    r = kReliefPegs.back().r;
    g = kReliefPegs.back().g;
    b = kReliefPegs.back().b;
}

inline QColor getReliefQColor(float t, int alpha = 255) {
    float r = 0.0f, g = 0.0f, b = 0.0f;
    getReliefColor(t, r, g, b);
    return QColor(std::clamp(static_cast<int>(r * 255.0f + 0.5f), 0, 255),
                  std::clamp(static_cast<int>(g * 255.0f + 0.5f), 0, 255),
                  std::clamp(static_cast<int>(b * 255.0f + 0.5f), 0, 255),
                  alpha);
}

} // namespace

GL3DView::GL3DView(ModelDocument *doc, QWidget *parent)
    : QOpenGLWidget(parent)
    , m_doc(doc)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    connect(m_doc, &ModelDocument::modelGeometryChanged, this, [this]() {
        m_meshDirty = true;
        update();
    });
    connect(m_doc, &ModelDocument::profileChanged, this, [this](int, ProfileOrientation) {
        update();
    });
    connect(m_doc, &ModelDocument::activeProfileCursorChanged, this, [this](const ModelDocument::ProfileCursorPoint &pt) {
        if (pt.bodyId >= 0 && pt.bodyId != m_selectedBodyId) {
            setSelectedBodyId(pt.bodyId);
        }
    });
    connect(m_doc, &ModelDocument::documentReset, this, [this]() {
        m_meshDirty = true;
        m_targetInitialized = false;
        resetCamera();
        update();
    });
}

GL3DView::~GL3DView() {
    if (m_camAnim) {
        m_camAnim->stop();
        delete m_camAnim.data();
        m_camAnim = nullptr;
    }
    if (isValid() && context()) {
        makeCurrent();
        if (m_bodyVao.isCreated()) m_bodyVao.destroy();
        if (m_bodyVbo.isCreated()) m_bodyVbo.destroy();
        if (m_transVao.isCreated()) m_transVao.destroy();
        if (m_transVbo.isCreated()) m_transVbo.destroy();
        if (m_linesVao.isCreated()) m_linesVao.destroy();
        if (m_linesVbo.isCreated()) m_linesVbo.destroy();
        if (m_flatTrisVao.isCreated()) m_flatTrisVao.destroy();
        if (m_flatTrisVbo.isCreated()) m_flatTrisVbo.destroy();
        delete m_shaderProg;
        m_shaderProg = nullptr;
        delete m_flatShaderProg;
        m_flatShaderProg = nullptr;
        doneCurrent();
    } else {
        delete m_shaderProg;
        m_shaderProg = nullptr;
        delete m_flatShaderProg;
        m_flatShaderProg = nullptr;
    }
}

void GL3DView::setSelectedBodyId(int bodyId) {
    if (m_selectedBodyId == bodyId) return;
    m_selectedBodyId = bodyId;
    update();
}

int GL3DView::hitTestBody(const QPoint &pos) {
    if (!m_doc) return -1;
    if (!m_meshDirty) {
        const auto &modelBodies = m_doc->model().bodies();
        if (modelBodies.size() != m_bodyMeshes.size()) {
            m_meshDirty = true;
        } else {
            for (const auto &mesh : m_bodyMeshes) {
                if (!m_doc->model().body(mesh.bodyId)) {
                    m_meshDirty = true;
                    break;
                }
            }
        }
    }
    if (m_meshDirty || (!m_doc->model().bodies().empty() && m_bodyMeshes.empty())) {
        m_bodyMeshes = m_doc->model().generate_body_meshes(-1, m_showOuterFacets);
        m_meshDirty = false;
    }
    if (m_bodyMeshes.empty()) return -1;

    float w = static_cast<float>(width());
    float h = static_cast<float>(height());
    if (w <= 0.0f || h <= 0.0f) return -1;

    float ndcX = (2.0f * pos.x()) / w - 1.0f;
    float ndcY = 1.0f - (2.0f * pos.y()) / h;

    bool invertible = false;
    QMatrix4x4 invMvp = (m_projMatrix * m_mvMatrix).inverted(&invertible);
    if (!invertible) return -1;

    QVector4D nearPt4 = invMvp * QVector4D(ndcX, ndcY, -1.0f, 1.0f);
    QVector4D farPt4  = invMvp * QVector4D(ndcX, ndcY,  1.0f, 1.0f);
    if (std::abs(nearPt4.w()) < 1e-6f || std::abs(farPt4.w()) < 1e-6f) return -1;

    QVector3D rayOrigin = nearPt4.toVector3DAffine();
    QVector3D rayFar    = farPt4.toVector3DAffine();
    QVector3D rayDir    = (rayFar - rayOrigin).normalized();

    double zMid = (m_doc->model().z_min_val() + m_doc->model().z_max_val()) * 0.5;
    auto scaleZ = [this, zMid](double zRaw) -> float {
        return static_cast<float>(zMid + (zRaw - zMid) * m_zScale);
    };

    double dBot = m_doc->model().z_min_val();
    int bestBodyId = -1;
    float minT = std::numeric_limits<float>::max();

    auto intersectTriangle = [](const QVector3D &orig, const QVector3D &dir,
                               const QVector3D &v0, const QVector3D &v1, const QVector3D &v2,
                               float &outT) -> bool {
        const float EPS = 1e-7f;
        QVector3D edge1 = v1 - v0;
        QVector3D edge2 = v2 - v0;
        QVector3D h = QVector3D::crossProduct(dir, edge2);
        float a = QVector3D::dotProduct(edge1, h);
        if (a > -EPS && a < EPS) return false;

        float f = 1.0f / a;
        QVector3D s = orig - v0;
        float u = f * QVector3D::dotProduct(s, h);
        if (u < 0.0f || u > 1.0f) return false;

        QVector3D q = QVector3D::crossProduct(s, edge1);
        float v = f * QVector3D::dotProduct(dir, q);
        if (v < 0.0f || u + v > 1.0f) return false;

        float t = f * QVector3D::dotProduct(edge2, q);
        if (t > EPS) {
            outT = t;
            return true;
        }
        return false;
    };

    for (const auto &mesh : m_bodyMeshes) {
        if (!mesh.isVisible) continue;
        for (const auto &fct : mesh.facets) {
            if (!m_showBottom &&
                fct[0].z <= dBot + 1e-3 &&
                fct[1].z <= dBot + 1e-3 &&
                fct[2].z <= dBot + 1e-3) {
                continue;
            }

            QVector3D v0(static_cast<float>(fct[0].x), static_cast<float>(fct[0].y), scaleZ(fct[0].z));
            QVector3D v1(static_cast<float>(fct[1].x), static_cast<float>(fct[1].y), scaleZ(fct[1].z));
            QVector3D v2(static_cast<float>(fct[2].x), static_cast<float>(fct[2].y), scaleZ(fct[2].z));

            float t = 0.0f;
            if (intersectTriangle(rayOrigin, rayDir, v0, v1, v2, t)) {
                if (t < minT) {
                    minT = t;
                    bestBodyId = mesh.bodyId;
                }
            }
        }
    }

    return bestBodyId;
}

void GL3DView::ensureDefaultTargetAndDistance() {
    if (!m_doc) return;
    const auto &model = m_doc->model();
    double midX = (model.x_min_val() + model.x_max_val()) * 0.5;
    double midY = (model.y_min_val() + model.y_max_val()) * 0.5;
    double midZ = (model.z_min_val() + model.z_max_val()) * 0.5;

    double maxDim = std::max({ model.x_max_val() - model.x_min_val(),
                               model.y_max_val() - model.y_min_val(),
                               (model.z_max_val() - model.z_min_val()) * std::max(1.0f, m_zScale) });
    if (maxDim <= 0.0) maxDim = 10000.0;

    if (!m_targetInitialized) {
        m_target = QVector3D(static_cast<float>(midX), static_cast<float>(midY), static_cast<float>(midZ));
        m_distance = static_cast<float>(maxDim * 1.8 / std::max(0.01f, m_zoom));
        m_targetInitialized = true;
    }
}

void GL3DView::setupMatrices(int w, int h) {
    ensureDefaultTargetAndDistance();

    qreal dpr = devicePixelRatioF();
    int physW = static_cast<int>(std::round(width() * dpr));
    int physH = static_cast<int>(std::round(height() * dpr));
    if (physW <= 0) physW = (w > 0 ? w : 1);
    if (physH <= 0) physH = (h > 0 ? h : 1);

    if (isValid() && context()) {
        glViewport(0, 0, physW, physH);
    }

    float aspect = (physH > 0) ? (static_cast<float>(physW) / static_cast<float>(physH)) : 1.0f;
    float fov = 45.0f;

    // Calculate scene bounding radius for tight, optimal depth range
    double maxDim = 10000.0;
    if (m_doc) {
        const auto &m = m_doc->model();
        maxDim = std::max({ m.x_max_val() - m.x_min_val(),
                            m.y_max_val() - m.y_min_val(),
                            (m.z_max_val() - m.z_min_val()) * std::max(1.0f, m_zScale) });
    }
    float boundingRadius = static_cast<float>(maxDim * 0.75);
    if (boundingRadius < 100.0f) boundingRadius = 100.0f;

    // Effective camera distance based on zoom factor
    float effDist = m_distance / std::max(0.01f, m_zoom);

    // Dynamic Near and Far clipping planes:
    // Ensures high precision 24-bit depth buffer without near-plane slicing or z-fighting
    float zNear = std::max(1.0f, std::max(0.02f * boundingRadius, effDist - boundingRadius * 1.6f));
    float zFar  = effDist + boundingRadius * 2.2f;
    if (zFar <= zNear * 1.2f) zFar = zNear * 10.0f;

    m_projMatrix.setToIdentity();
    if (m_orthographic) {
        float orthoH = 2.0f * effDist * std::tan(fov * 0.5f * 3.14159265f / 180.0f);
        float orthoW = orthoH * aspect;
        m_projMatrix.ortho(-orthoW * 0.5f, orthoW * 0.5f, -orthoH * 0.5f, orthoH * 0.5f, zNear, zFar);
    } else {
        m_projMatrix.perspective(fov, aspect, zNear, zFar);
    }

    m_mvMatrix.setToIdentity();
    m_mvMatrix.translate(m_panX, m_panY, -effDist);
    m_mvMatrix.rotate(m_pitch - 90.0f, 1.0f, 0.0f, 0.0f);
    m_mvMatrix.rotate(m_azimuth, 0.0f, 0.0f, 1.0f);
    m_mvMatrix.translate(-m_target.x(), -m_target.y(), -m_target.z());

}

void GL3DView::initShaders() {
    if (m_shadersInitialized) return;
    if (!isValid() || !context()) return;

    delete m_shaderProg;
    m_shaderProg = new QOpenGLShaderProgram(this);

    auto tryCompile = [](QOpenGLShaderProgram *prog, const char *verStr,
                         const char *vbody, const char *fbody,
                         const std::vector<std::pair<const char*, int>> &attribs) -> bool {
        QString vsrc = QString("%1\n%2").arg(verStr, vbody);
        QString fsrc = QString("%1\n%2").arg(verStr, fbody);
        if (!prog->addShaderFromSourceCode(QOpenGLShader::Vertex, vsrc)) return false;
        if (!prog->addShaderFromSourceCode(QOpenGLShader::Fragment, fsrc)) return false;
        for (const auto &at : attribs) {
            prog->bindAttributeLocation(at.first, at.second);
        }
        return prog->link();
    };

    const char *vbodyShaded =
        "layout(location = 0) in vec3 a_position;\n"
        "layout(location = 1) in vec3 a_normal;\n"
        "layout(location = 2) in vec4 a_color;\n"
        "uniform mat4 u_mvpMatrix;\n"
        "uniform mat4 u_mvMatrix;\n"
        "uniform mat3 u_normalMatrix;\n"
        "uniform float u_zMid;\n"
        "uniform float u_zScale;\n"
        "out vec3 v_normal;\n"
        "out vec3 v_eyePos;\n"
        "out vec4 v_color;\n"
        "void main() {\n"
        "    vec3 pos = a_position;\n"
        "    pos.z = u_zMid + (pos.z - u_zMid) * u_zScale;\n"
        "    vec3 norm = a_normal;\n"
        "    norm.z = norm.z / max(0.05, u_zScale);\n"
        "    norm = normalize(norm);\n"
        "    vec4 eyePos4 = u_mvMatrix * vec4(pos, 1.0);\n"
        "    v_eyePos = eyePos4.xyz;\n"
        "    v_normal = normalize(u_normalMatrix * norm);\n"
        "    v_color = a_color;\n"
        "    gl_Position = u_mvpMatrix * vec4(pos, 1.0);\n"
        "}\n";

    const char *fbodyShaded =
        "in vec3 v_normal;\n"
        "in vec3 v_eyePos;\n"
        "in vec4 v_color;\n"
        "uniform vec3 u_light0Dir;\n"
        "uniform vec3 u_light1Dir;\n"
        "uniform vec4 u_light0Color;\n"
        "uniform vec4 u_light1Color;\n"
        "uniform vec4 u_ambientColor;\n"
        "uniform bool u_enableFog;\n"
        "uniform float u_fogStart;\n"
        "uniform float u_fogEnd;\n"
        "uniform vec4 u_fogColor;\n"
        "uniform bool u_highlight;\n"
        "uniform vec4 u_highlightColor;\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "    vec3 N = normalize(v_normal);\n"
        "    if (!gl_FrontFacing) N = -N;\n"
        "    vec3 V = normalize(-v_eyePos);\n"
        "    vec3 L0 = normalize(u_light0Dir);\n"
        "    vec3 H0 = normalize(L0 + V);\n"
        "    float diff0 = max(dot(N, L0), 0.0);\n"
        "    float spec0 = pow(max(dot(N, H0), 0.0), 32.0);\n"
        "    vec3 L1 = normalize(u_light1Dir);\n"
        "    float diff1 = max(dot(N, L1), 0.0);\n"
        "    float fresnel = pow(1.0 - max(dot(N, V), 0.0), 3.0) * 0.22;\n"
        "    vec3 baseCol = v_color.rgb;\n"
        "    if (u_highlight) {\n"
        "        baseCol = mix(baseCol, u_highlightColor.rgb, 0.40);\n"
        "    }\n"
        "    vec3 lit = baseCol * (u_ambientColor.rgb + diff0 * u_light0Color.rgb + diff1 * u_light1Color.rgb)\n"
        "             + spec0 * vec3(0.35)\n"
        "             + fresnel * vec3(1.0);\n"
        "    if (u_highlight) {\n"
        "        lit += u_highlightColor.rgb * 0.15;\n"
        "    }\n"
        "    float alpha = v_color.a;\n"
        "    if (u_enableFog) {\n"
        "        float dist = length(v_eyePos);\n"
        "        float fogFactor = clamp((dist - u_fogStart) / (u_fogEnd - u_fogStart), 0.0, 1.0);\n"
        "        lit = mix(lit, u_fogColor.rgb, fogFactor);\n"
        "    }\n"
        "    fragColor = vec4(lit, alpha);\n"
        "}\n";

    std::vector<std::pair<const char*, int>> shadedAttribs = {
        { "a_position", 0 },
        { "a_normal", 1 },
        { "a_color", 2 }
    };

    bool okShaded = tryCompile(m_shaderProg, "#version 410 core", vbodyShaded, fbodyShaded, shadedAttribs);
    if (!okShaded) {
        delete m_shaderProg;
        m_shaderProg = new QOpenGLShaderProgram(this);
        okShaded = tryCompile(m_shaderProg, "#version 330 core", vbodyShaded, fbodyShaded, shadedAttribs);
    }

    delete m_flatShaderProg;
    m_flatShaderProg = new QOpenGLShaderProgram(this);

    const char *vbodyFlat =
        "layout(location = 0) in vec3 a_position;\n"
        "layout(location = 1) in vec4 a_color;\n"
        "uniform mat4 u_mvpMatrix;\n"
        "out vec4 v_color;\n"
        "void main() {\n"
        "    v_color = a_color;\n"
        "    gl_Position = u_mvpMatrix * vec4(a_position, 1.0);\n"
        "}\n";

    const char *fbodyFlat =
        "in vec4 v_color;\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "    fragColor = v_color;\n"
        "}\n";

    std::vector<std::pair<const char*, int>> flatAttribs = {
        { "a_position", 0 },
        { "a_color", 1 }
    };

    bool okFlat = tryCompile(m_flatShaderProg, "#version 410 core", vbodyFlat, fbodyFlat, flatAttribs);
    if (!okFlat) {
        delete m_flatShaderProg;
        m_flatShaderProg = new QOpenGLShaderProgram(this);
        okFlat = tryCompile(m_flatShaderProg, "#version 330 core", vbodyFlat, fbodyFlat, flatAttribs);
    }

    m_shadersInitialized = okShaded && okFlat;
}

void GL3DView::initBuffers() {
    if (m_buffersInitialized) return;
    if (!isValid() || !context()) return;

    // Body VAO & VBO
    m_bodyVao.create();
    m_bodyVao.bind();
    m_bodyVbo.create();
    m_bodyVbo.bind();
    m_bodyVbo.setUsagePattern(QOpenGLBuffer::StaticDraw);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<void*>(offsetof(Vertex3D, x)));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<void*>(offsetof(Vertex3D, nx)));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<void*>(offsetof(Vertex3D, r)));

    m_bodyVao.release();
    m_bodyVbo.release();

    // Transparent VAO & VBO
    m_transVao.create();
    m_transVao.bind();
    m_transVbo.create();
    m_transVbo.bind();
    m_transVbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<void*>(offsetof(Vertex3D, x)));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<void*>(offsetof(Vertex3D, nx)));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<void*>(offsetof(Vertex3D, r)));

    m_transVao.release();
    m_transVbo.release();

    // Lines VAO & VBO
    m_linesVao.create();
    m_linesVao.bind();
    m_linesVbo.create();
    m_linesVbo.bind();
    m_linesVbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(FlatVertex3D), reinterpret_cast<void*>(offsetof(FlatVertex3D, x)));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(FlatVertex3D), reinterpret_cast<void*>(offsetof(FlatVertex3D, r)));

    m_linesVao.release();
    m_linesVbo.release();

    // Flat Triangles VAO & VBO
    m_flatTrisVao.create();
    m_flatTrisVao.bind();
    m_flatTrisVbo.create();
    m_flatTrisVbo.bind();
    m_flatTrisVbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(FlatVertex3D), reinterpret_cast<void*>(offsetof(FlatVertex3D, x)));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(FlatVertex3D), reinterpret_cast<void*>(offsetof(FlatVertex3D, r)));

    m_flatTrisVao.release();
    m_flatTrisVbo.release();

    m_buffersInitialized = true;
}

void GL3DView::updateBodyBuffers() {
    if (!m_doc) return;
    initBuffers();

    if (!m_meshDirty) {
        const auto &modelBodies = m_doc->model().bodies();
        if (modelBodies.size() != m_bodyMeshes.size()) {
            m_meshDirty = true;
        } else {
            for (const auto &mesh : m_bodyMeshes) {
                if (!m_doc->model().body(mesh.bodyId)) {
                    m_meshDirty = true;
                    break;
                }
            }
        }
    }

    if (m_meshDirty || (!m_doc->model().bodies().empty() && m_bodyMeshes.empty())) {
        m_bodyMeshes = m_doc->model().generate_body_meshes(-1, m_showOuterFacets);
        m_meshDirty = false;
    }

    // Validate selected body ID
    bool selFound = false;
    for (const auto &mesh : m_bodyMeshes) {
        if (mesh.bodyId == m_selectedBodyId) {
            selFound = true;
            break;
        }
    }
    if (!selFound) {
        m_selectedBodyId = -1;
    }

    std::vector<Vertex3D> opaqueVerts;
    m_bodyRanges.clear();

    double dBot = m_doc->model().z_min_val();

    for (const auto &mesh : m_bodyMeshes) {
        if (!mesh.isVisible) continue;

        BodyRange range;
        range.bodyId = mesh.bodyId;
        range.isTransparent = (m_transparentBodies && mesh.isTransparent);
        range.first = static_cast<int>(opaqueVerts.size());

        if (!range.isTransparent) {
            float r = mesh.color.r / 255.0f;
            float g = mesh.color.g / 255.0f;
            float b = mesh.color.b / 255.0f;
            float a = 1.0f;

            for (const auto &fct : mesh.facets) {
                if (!m_showBottom &&
                    fct[0].z <= dBot + 1e-3 &&
                    fct[1].z <= dBot + 1e-3 &&
                    fct[2].z <= dBot + 1e-3) {
                    continue;
                }

                auto norm = fct.normal();
                double nLen = std::sqrt(norm.x*norm.x + norm.y*norm.y + norm.z*norm.z);
                if (nLen > 1e-6) { norm.x /= nLen; norm.y /= nLen; norm.z /= nLen; }

                for (int i = 0; i < 3; ++i) {
                    Vertex3D v;
                    v.x = static_cast<float>(fct[i].x);
                    v.y = static_cast<float>(fct[i].y);
                    v.z = static_cast<float>(fct[i].z);
                    v.nx = static_cast<float>(norm.x);
                    v.ny = static_cast<float>(norm.y);
                    v.nz = static_cast<float>(norm.z);
                    v.r = r;
                    v.g = g;
                    v.b = b;
                    v.a = a;
                    opaqueVerts.push_back(v);
                }
            }
        }

        range.count = static_cast<int>(opaqueVerts.size()) - range.first;
        m_bodyRanges.push_back(range);
    }

    m_bodyVbo.bind();
    m_bodyVbo.allocate(opaqueVerts.data(), static_cast<int>(opaqueVerts.size() * sizeof(Vertex3D)));
    m_bodyVbo.release();
}

void GL3DView::renderLines(const std::vector<FlatVertex3D> &lines, float lineWidth) {
    if (lines.empty()) return;
    if (!m_flatShaderProg) return;

    initBuffers();

    glLineWidth(lineWidth);
    m_flatShaderProg->bind();
    m_flatShaderProg->setUniformValue("u_mvpMatrix", m_projMatrix * m_mvMatrix);

    m_linesVao.bind();
    m_linesVbo.bind();
    m_linesVbo.allocate(lines.data(), static_cast<int>(lines.size() * sizeof(FlatVertex3D)));

    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(lines.size()));

    m_linesVbo.release();
    m_linesVao.release();
    m_flatShaderProg->release();
}

void GL3DView::renderFlatTriangles(const std::vector<FlatVertex3D> &tris, bool blend) {
    if (tris.empty()) return;
    if (!m_flatShaderProg) return;

    initBuffers();

    if (blend) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
    }

    m_flatShaderProg->bind();
    m_flatShaderProg->setUniformValue("u_mvpMatrix", m_projMatrix * m_mvMatrix);

    m_flatTrisVao.bind();
    m_flatTrisVbo.bind();
    m_flatTrisVbo.allocate(tris.data(), static_cast<int>(tris.size() * sizeof(FlatVertex3D)));

    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(tris.size()));

    m_flatTrisVbo.release();
    m_flatTrisVao.release();
    m_flatShaderProg->release();

    if (blend) {
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
}

void GL3DView::initializeGL() {
    if (!isValid() || !context()) return;
    initializeOpenGLFunctions();
    initShaders();
    initBuffers();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Clean modern studio neutral background
    glClearColor(0.92f, 0.93f, 0.95f, 1.0f);
}

void GL3DView::resizeGL(int w, int h) {
    if (!isValid() || !context()) return;
    setupMatrices(w, h);
}

void GL3DView::paintGL() {
    if (!isValid() || !context()) return;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    qreal dpr = devicePixelRatioF();
    int physW = static_cast<int>(std::round(width() * dpr));
    int physH = static_cast<int>(std::round(height() * dpr));
    setupMatrices(physW, physH);

    if (!m_doc) return;

    if (m_showBoundingBox) {
        drawBoundingBox();
    }

    drawBodies();

    if (m_showRelief) {
        drawRelief();
    }

    if (m_showProfilePlane) {
        drawProfilePlane();
    }

    if (m_mouseDragging && (m_dragButton == Qt::LeftButton) && !m_draggingGizmo) {
        drawPivotIndicator();
    }
}

void GL3DView::paintEvent(QPaintEvent *event) {
    if (isValid() && context()) {
        QOpenGLWidget::paintEvent(event);
        if (m_showOrientationIndicator) {
            QPainter painter(this);
            if (painter.isActive()) {
                drawOrientationHud(painter);
            }
        }
        return;
    }
    // Headless / offscreen environment without OpenGL context:
    // QOpenGLWidget cannot be painted directly by QPainter on uninitialized context.
    Q_UNUSED(event);
}

void GL3DView::paintSoftware(QPainter &painter) {
    if (!painter.isActive()) return;
    int w = width();
    int h = height();
    if (w <= 0 || h <= 0) return;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    // 1. Studio background gradient
    QLinearGradient bgGrad(0, 0, 0, h);
    bgGrad.setColorAt(0.0, QColor(246, 248, 252));
    bgGrad.setColorAt(1.0, QColor(228, 233, 242));
    painter.fillRect(0, 0, w, h, bgGrad);

    if (!m_doc) {
        painter.restore();
        return;
    }

    ensureDefaultTargetAndDistance();
    const auto &model = m_doc->model();

    double zMid = (model.z_min_val() + model.z_max_val()) * 0.5;
    double effDist = m_distance / std::max(0.01f, m_zoom);

    auto project = [&](double x, double y, double z) {
        // Apply vertical exaggeration around mid-elevation
        double zScaled = zMid + (z - zMid) * m_zScale;

        double dx = x - m_target.x();
        double dy = y - m_target.y();
        double dz = zScaled - m_target.z();

        double azRad = m_azimuth * 3.14159265358979323846 / 180.0;
        double rx = dx * std::cos(azRad) - dy * std::sin(azRad);
        double ry = dx * std::sin(azRad) + dy * std::cos(azRad);
        double rz = dz;

        double pitchRad = (m_pitch - 90.0) * 3.14159265358979323846 / 180.0;
        double px = rx;
        double py = ry * std::cos(pitchRad) - rz * std::sin(pitchRad);
        double pz = ry * std::sin(pitchRad) + rz * std::cos(pitchRad);

        double cx = px + m_panX;
        double cy = py + m_panY;
        double cz = effDist - pz;

        double fov = 45.0 * 3.14159265358979323846 / 180.0;
        double top = std::tan(fov * 0.5);

        double scale;
        if (m_orthographic) {
            scale = 0.5 * h / (top * effDist);
        } else {
            scale = (cz > 10.0) ? (0.5 * h / (top * cz)) : (0.5 * h / (top * 10.0));
        }

        double sx = (w * 0.5) + cx * scale;
        double sy = (h * 0.5) - cy * scale;

        return std::make_pair(QPointF(sx, sy), pz);
    };

    // 2. Bounding Box
    if (m_showBoundingBox) {
        double x0 = model.x_min_val(), x1 = model.x_max_val();
        double y0 = model.y_min_val(), y1 = model.y_max_val();
        double z0 = model.z_min_val(), z1 = model.z_max_val();

        QPointF c[8];
        c[0] = project(x0, y0, z0).first;
        c[1] = project(x1, y0, z0).first;
        c[2] = project(x1, y1, z0).first;
        c[3] = project(x0, y1, z0).first;
        c[4] = project(x0, y0, z1).first;
        c[5] = project(x1, y0, z1).first;
        c[6] = project(x1, y1, z1).first;
        c[7] = project(x0, y1, z1).first;

        painter.setPen(QPen(QColor(160, 168, 185, 200), 1.2, Qt::SolidLine));
        painter.drawLine(c[0], c[1]); painter.drawLine(c[1], c[2]);
        painter.drawLine(c[2], c[3]); painter.drawLine(c[3], c[0]);
        painter.drawLine(c[4], c[5]); painter.drawLine(c[5], c[6]);
        painter.drawLine(c[6], c[7]); painter.drawLine(c[7], c[4]);
        painter.drawLine(c[0], c[4]); painter.drawLine(c[1], c[5]);
        painter.drawLine(c[2], c[6]); painter.drawLine(c[3], c[7]);

        // Origin coordinate axes at (x0, y0, z0)
        double axisLen = (x1 - x0) * 0.12;
        QPointF pOrigin = c[0];
        QPointF pAxX = project(x0 + axisLen, y0, z0).first;
        QPointF pAxY = project(x0, y0 + axisLen, z0).first;
        QPointF pAxZ = project(x0, y0, z0 + axisLen).first;

        painter.setPen(QPen(QColor(220, 38, 38), 2.0));
        painter.drawLine(pOrigin, pAxX);
        painter.setPen(QPen(QColor(22, 163, 74), 2.0));
        painter.drawLine(pOrigin, pAxY);
        painter.setPen(QPen(QColor(37, 99, 235), 2.0));
        painter.drawLine(pOrigin, pAxZ);
    }

    // 3. Active and Inactive Profile Plane Slices
    if (m_showProfilePlane) {
        struct SoftwareSlice {
            ProfileOrientation orient;
            int index;
            bool isActive;
        };
        std::vector<SoftwareSlice> swSlices;

        QMdiArea *mdi = findMdiArea();
        if (mdi) {
            std::vector<ProfileView*> openPvs;
            for (auto *sub : mdi->subWindowList()) {
                if (auto *pv = qobject_cast<ProfileView*>(sub->widget())) {
                    openPvs.push_back(pv);
                }
            }

            ProfileView *activePv = nullptr;
            if (auto *actSub = mdi->activeSubWindow()) {
                activePv = qobject_cast<ProfileView*>(actSub->widget());
            }
            if (activePv) {
                m_lastActiveProfileView = activePv;
            } else {
                if (auto *mw = qobject_cast<MainWindow*>(mdi->window())) {
                    activePv = mw->activeProfileView();
                }
                if (!activePv && m_lastActiveProfileView) {
                    if (std::find(openPvs.begin(), openPvs.end(), m_lastActiveProfileView.data()) != openPvs.end()) {
                        activePv = m_lastActiveProfileView.data();
                    }
                }
                if (!activePv) {
                    for (auto *pv : openPvs) {
                        if (pv->profileOrientation() == m_doc->profileOrientation() &&
                            pv->profileIndex() == m_doc->profileIndex()) {
                            activePv = pv;
                            break;
                        }
                    }
                }
                if (!activePv) {
                    for (auto *pv : openPvs) {
                        if (pv->profileOrientation() == m_doc->profileOrientation()) {
                            activePv = pv;
                            break;
                        }
                    }
                }
                if (!activePv && !openPvs.empty()) {
                    activePv = openPvs.front();
                }
            }

            for (auto *pv : openPvs) {
                swSlices.push_back({ pv->profileOrientation(), pv->profileIndex(), (pv == activePv) });
            }
        }

        if (swSlices.empty()) {
            swSlices.push_back({ m_doc->profileOrientation(), m_doc->profileIndex(), true });
        }

        double z0 = model.z_min_val();
        double z1 = model.z_max_val();

        // Inactive slices (subtle blue)
        for (const auto &sl : swSlices) {
            if (sl.isActive) continue;
            QPolygonF poly;
            if (sl.orient == ProfileOrientation::EastWest) {
                double y = model.yd_val(sl.index + 1);
                poly << project(model.x_min_val(), y, z0).first
                     << project(model.x_max_val(), y, z0).first
                     << project(model.x_max_val(), y, z1).first
                     << project(model.x_min_val(), y, z1).first;
            } else {
                double x = model.xd_val(sl.index + 1);
                poly << project(x, model.y_min_val(), z0).first
                     << project(x, model.y_max_val(), z0).first
                     << project(x, model.y_max_val(), z1).first
                     << project(x, model.y_min_val(), z1).first;
            }
            painter.setBrush(m_transparentProfiles ? QColor(59, 130, 246, 20) : Qt::NoBrush);
            painter.setPen(QPen(QColor(59, 130, 246, 120), 1.0, Qt::DashLine));
            painter.drawPolygon(poly);
        }

        // Active slice (prominent amber)
        for (const auto &sl : swSlices) {
            if (!sl.isActive) continue;
            QPolygonF poly;
            if (sl.orient == ProfileOrientation::EastWest) {
                double y = model.yd_val(sl.index + 1);
                poly << project(model.x_min_val(), y, z0).first
                     << project(model.x_max_val(), y, z0).first
                     << project(model.x_max_val(), y, z1).first
                     << project(model.x_min_val(), y, z1).first;
            } else {
                double x = model.xd_val(sl.index + 1);
                poly << project(x, model.y_min_val(), z0).first
                     << project(x, model.y_max_val(), z0).first
                     << project(x, model.y_max_val(), z1).first
                     << project(x, model.y_min_val(), z1).first;
            }
            painter.setBrush(m_transparentProfiles ? QColor(245, 158, 11, 45) : Qt::NoBrush);
            painter.setPen(QPen(QColor(217, 119, 6, 220), 1.8, Qt::DashLine));
            painter.drawPolygon(poly);
        }
    }

    // 4. 3D Polyhedral Bodies (Depth-Sorted Back-to-Front)
    if (!m_meshDirty) {
        const auto &modelBodies = m_doc->model().bodies();
        if (modelBodies.size() != m_bodyMeshes.size()) {
            m_meshDirty = true;
        } else {
            for (const auto &mesh : m_bodyMeshes) {
                if (!m_doc->model().body(mesh.bodyId)) {
                    m_meshDirty = true;
                    break;
                }
            }
        }
    }
    if (m_meshDirty || (!m_doc->model().bodies().empty() && m_bodyMeshes.empty())) {
        m_bodyMeshes = m_doc->model().generate_body_meshes(-1, m_showOuterFacets);
        m_meshDirty = false;
    }

    struct SortFacet {
        QPointF p[3];
        double depth;
        QColor fill;
        QColor edge;
        double edgeWidth;
    };
    std::vector<SortFacet> facetsToDraw;

    double dBot = model.z_min_val();
    double azRad = m_azimuth * 3.14159265358979323846 / 180.0;
    double pitchRad = (m_pitch - 90.0) * 3.14159265358979323846 / 180.0;

    double lx = 0.40, ly = 0.65, lz = 0.65;
    double lLen = std::sqrt(lx*lx + ly*ly + lz*lz);
    lx /= lLen; ly /= lLen; lz /= lLen;

    for (const auto &mesh : m_bodyMeshes) {
        if (!mesh.isVisible) continue;
        auto bc = mesh.color;
        bool isSelected = (mesh.bodyId == m_selectedBodyId);

        for (const auto &fct : mesh.facets) {
            if (!m_showBottom &&
                fct[0].z <= dBot + 1e-3 &&
                fct[1].z <= dBot + 1e-3 &&
                fct[2].z <= dBot + 1e-3) {
                continue;
            }

            auto pt0 = project(fct[0].x, fct[0].y, fct[0].z);
            auto pt1 = project(fct[1].x, fct[1].y, fct[1].z);
            auto pt2 = project(fct[2].x, fct[2].y, fct[2].z);

            double avgDepth = (pt0.second + pt1.second + pt2.second) / 3.0;

            // Rotate normal into camera space
            auto norm = fct.normal();
            norm.z /= std::max(0.1f, m_zScale);
            double nLen = std::sqrt(norm.x*norm.x + norm.y*norm.y + norm.z*norm.z);
            if (nLen > 1e-6) { norm.x /= nLen; norm.y /= nLen; norm.z /= nLen; }

            double nrx = norm.x * std::cos(azRad) - norm.y * std::sin(azRad);
            double nry = norm.x * std::sin(azRad) + norm.y * std::cos(azRad);
            double nrz = norm.z;

            double npx = nrx;
            double npy = nry * std::cos(pitchRad) - nrz * std::sin(pitchRad);
            double npz = nry * std::sin(pitchRad) + nrz * std::cos(pitchRad);

            double dot = std::abs(npx * lx + npy * ly + npz * lz);
            double shade = 0.35 + 0.65 * dot;

            int r = std::clamp(static_cast<int>(bc.r * shade), 0, 255);
            int g = std::clamp(static_cast<int>(bc.g * shade), 0, 255);
            int b = std::clamp(static_cast<int>(bc.b * shade), 0, 255);
            int alpha = (m_transparentBodies && mesh.isTransparent) ? std::clamp(static_cast<int>(mesh.alpha * 255), 30, 240) : 255;

            QColor fillColor(r, g, b, alpha);
            if (isSelected) {
                fillColor = QColor(std::min(255, r + 40), std::min(255, g + 35), std::min(255, b + 10), alpha);
            }
            QColor edgeColor = isSelected ? QColor(234, 179, 8, 240) : QColor(static_cast<int>(r * 0.65), static_cast<int>(g * 0.65), static_cast<int>(b * 0.65), 210);
            double edgeWidth = isSelected ? 1.8 : 0.8;

            facetsToDraw.push_back({ {pt0.first, pt1.first, pt2.first}, avgDepth, fillColor, edgeColor, edgeWidth });
        }
    }

    // 4b. Relief Surface in Software Paint Mode
    if (m_showRelief && m_reliefMode == ReliefMode::Surface) {
        int rMax = model.rows() - 2;
        int cMax = model.cols() - 2;
        if (rMax >= 1 && cMax >= 1) {
            float zMinRelief = std::numeric_limits<float>::max();
            float zMaxRelief = -std::numeric_limits<float>::max();
            for (int rIdx = 1; rIdx <= rMax; ++rIdx) {
                for (int c = 1; c <= cMax; ++c) {
                    float zv = static_cast<float>(model.z(rIdx, c, 0));
                    if (zv < zMinRelief) zMinRelief = zv;
                    if (zv > zMaxRelief) zMaxRelief = zv;
                }
            }
            float zSpan = zMaxRelief - zMinRelief;
            if (zSpan < 1e-4f) zSpan = 1.0f;

            auto getT = [zMinRelief, zSpan](float z) -> float {
                return std::clamp((z - zMinRelief) / zSpan, 0.0f, 1.0f);
            };

            for (int rIdx = 1; rIdx < rMax; ++rIdx) {
                double y0 = model.yd_val(rIdx);
                double y1 = model.yd_val(rIdx + 1);
                for (int c = 1; c < cMax; ++c) {
                    double x0 = model.xd_val(c);
                    double x1 = model.xd_val(c + 1);

                    float z00Raw = static_cast<float>(model.z(rIdx, c, 0));
                    float z10Raw = static_cast<float>(model.z(rIdx, c + 1, 0));
                    float z11Raw = static_cast<float>(model.z(rIdx + 1, c + 1, 0));
                    float z01Raw = static_cast<float>(model.z(rIdx + 1, c, 0));

                    auto pt00 = project(x0, y0, z00Raw);
                    auto pt10 = project(x1, y0, z10Raw);
                    auto pt11 = project(x1, y1, z11Raw);
                    auto pt01 = project(x0, y1, z01Raw);

                    float t00 = getT(z00Raw);
                    float t10 = getT(z10Raw);
                    float t11 = getT(z11Raw);
                    float t01 = getT(z01Raw);

                    float avgT1 = (t00 + t10 + t11) / 3.0f;
                    float avgT2 = (t00 + t11 + t01) / 3.0f;

                    QColor col1 = getReliefQColor(avgT1, 215);
                    QColor edge1 = getReliefQColor(avgT1, 90);
                    double d1 = (pt00.second + pt10.second + pt11.second) / 3.0;
                    facetsToDraw.push_back({ {pt00.first, pt10.first, pt11.first}, d1, col1, edge1, 0.6 });

                    QColor col2 = getReliefQColor(avgT2, 215);
                    QColor edge2 = getReliefQColor(avgT2, 90);
                    double d2 = (pt00.second + pt11.second + pt01.second) / 3.0;
                    facetsToDraw.push_back({ {pt00.first, pt11.first, pt01.first}, d2, col2, edge2, 0.6 });
                }
            }
        }
    }

    // Sort back-to-front (smaller depth is further from eye)
    std::sort(facetsToDraw.begin(), facetsToDraw.end(), [](const SortFacet &a, const SortFacet &b) {
        return a.depth < b.depth;
    });

    for (const auto &f : facetsToDraw) {
        painter.setPen(QPen(f.edge, f.edgeWidth));
        painter.setBrush(f.fill);
        QPointF pts[3] = { f.p[0], f.p[1], f.p[2] };
        painter.drawPolygon(pts, 3);
    }

    // 4c. Relief Wireframe in Software Paint Mode
    if (m_showRelief && m_reliefMode == ReliefMode::Wireframe) {
        int rMax = model.rows() - 2;
        int cMax = model.cols() - 2;
        if (rMax >= 1 && cMax >= 1) {
            float zMinRelief = std::numeric_limits<float>::max();
            float zMaxRelief = -std::numeric_limits<float>::max();
            for (int rIdx = 1; rIdx <= rMax; ++rIdx) {
                for (int c = 1; c <= cMax; ++c) {
                    float zv = static_cast<float>(model.z(rIdx, c, 0));
                    if (zv < zMinRelief) zMinRelief = zv;
                    if (zv > zMaxRelief) zMaxRelief = zv;
                }
            }
            float zSpan = zMaxRelief - zMinRelief;
            if (zSpan < 1e-4f) zSpan = 1.0f;

            auto getT = [zMinRelief, zSpan](float z) -> float {
                return std::clamp((z - zMinRelief) / zSpan, 0.0f, 1.0f);
            };

            for (int rIdx = 1; rIdx <= rMax; ++rIdx) {
                double y = model.yd_val(rIdx);
                for (int c = 1; c < cMax; ++c) {
                    double xA = model.xd_val(c);
                    double xB = model.xd_val(c + 1);
                    float zA = static_cast<float>(model.z(rIdx, c, 0));
                    float zB = static_cast<float>(model.z(rIdx, c + 1, 0));
                    auto ptA = project(xA, y, zA);
                    auto ptB = project(xB, y, zB);
                    float midT = (getT(zA) + getT(zB)) * 0.5f;
                    painter.setPen(QPen(getReliefQColor(midT, 225), 1.2));
                    painter.drawLine(ptA.first, ptB.first);
                }
            }
            for (int c = 1; c <= cMax; ++c) {
                double x = model.xd_val(c);
                for (int rIdx = 1; rIdx < rMax; ++rIdx) {
                    double yA = model.yd_val(rIdx);
                    double yB = model.yd_val(rIdx + 1);
                    float zA = static_cast<float>(model.z(rIdx, c, 0));
                    float zB = static_cast<float>(model.z(rIdx + 1, c, 0));
                    auto ptA = project(x, yA, zA);
                    auto ptB = project(x, yB, zB);
                    float midT = (getT(zA) + getT(zB)) * 0.5f;
                    painter.setPen(QPen(getReliefQColor(midT, 225), 1.2));
                    painter.drawLine(ptA.first, ptB.first);
                }
            }
        }
    }

    // 5. Orientation Indicator HUD (Compass Tripod in bottom-left corner)
    if (m_showOrientationIndicator) {
        drawOrientationHud(painter);
    }

    painter.restore();
}

void GL3DView::drawPivotIndicator() {
    // Draws a subtle visual reticle at m_target during mouse orbit
    float effDist = m_distance / std::max(0.01f, m_zoom);
    float size = effDist * 0.018f;

    QVector3D camRight(m_mvMatrix(0, 0), m_mvMatrix(0, 1), m_mvMatrix(0, 2));
    QVector3D camUp(m_mvMatrix(1, 0), m_mvMatrix(1, 1), m_mvMatrix(1, 2));

    std::vector<FlatVertex3D> lines;
    float r = 0.96f, g = 0.55f, b = 0.12f, a = 0.85f;

    // Reticle circle
    const int numSegs = 24;
    for (int i = 0; i < numSegs; ++i) {
        float a0 = 2.0f * 3.14159265f * static_cast<float>(i) / static_cast<float>(numSegs);
        float a1 = 2.0f * 3.14159265f * static_cast<float>(i + 1) / static_cast<float>(numSegs);
        QVector3D p0 = m_target + camRight * (std::cos(a0) * size) + camUp * (std::sin(a0) * size);
        QVector3D p1 = m_target + camRight * (std::cos(a1) * size) + camUp * (std::sin(a1) * size);
        lines.push_back({ p0.x(), p0.y(), p0.z(), r, g, b, a });
        lines.push_back({ p1.x(), p1.y(), p1.z(), r, g, b, a });
    }

    // Crosshairs
    QVector3D h0 = m_target - camRight * (size * 1.5f);
    QVector3D h1 = m_target - camRight * (size * 0.5f);
    QVector3D h2 = m_target + camRight * (size * 0.5f);
    QVector3D h3 = m_target + camRight * (size * 1.5f);

    QVector3D v0 = m_target - camUp * (size * 1.5f);
    QVector3D v1 = m_target - camUp * (size * 0.5f);
    QVector3D v2 = m_target + camUp * (size * 0.5f);
    QVector3D v3 = m_target + camUp * (size * 1.5f);

    lines.push_back({ h0.x(), h0.y(), h0.z(), r, g, b, a });
    lines.push_back({ h1.x(), h1.y(), h1.z(), r, g, b, a });
    lines.push_back({ h2.x(), h2.y(), h2.z(), r, g, b, a });
    lines.push_back({ h3.x(), h3.y(), h3.z(), r, g, b, a });

    lines.push_back({ v0.x(), v0.y(), v0.z(), r, g, b, a });
    lines.push_back({ v1.x(), v1.y(), v1.z(), r, g, b, a });
    lines.push_back({ v2.x(), v2.y(), v2.z(), r, g, b, a });
    lines.push_back({ v3.x(), v3.y(), v3.z(), r, g, b, a });

    renderLines(lines, 2.0f);
}

void GL3DView::drawBoundingBox() {
    const auto &m = m_doc->model();
    float x0 = static_cast<float>(m.x_min_val());
    float x1 = static_cast<float>(m.x_max_val());
    float y0 = static_cast<float>(m.y_min_val());
    float y1 = static_cast<float>(m.y_max_val());
    float z0Raw = static_cast<float>(m.z_min_val());
    float z1Raw = static_cast<float>(m.z_max_val());

    float zMid = (z0Raw + z1Raw) * 0.5f;
    float z0 = zMid + (z0Raw - zMid) * m_zScale;
    float z1 = zMid + (z1Raw - zMid) * m_zScale;

    std::vector<FlatVertex3D> boxLines;
    std::vector<FlatVertex3D> thickLines;
    std::vector<FlatVertex3D> coneTris;

    float rBox = 0.55f, gBox = 0.55f, bBox = 0.60f, aBox = 1.0f;

    auto addBoxLine = [&](float ax, float ay, float az, float bx, float by, float bz) {
        boxLines.push_back({ ax, ay, az, rBox, gBox, bBox, aBox });
        boxLines.push_back({ bx, by, bz, rBox, gBox, bBox, aBox });
    };

    // Bottom
    addBoxLine(x0, y0, z0, x1, y0, z0);
    addBoxLine(x1, y0, z0, x1, y1, z0);
    addBoxLine(x1, y1, z0, x0, y1, z0);
    addBoxLine(x0, y1, z0, x0, y0, z0);
    // Top
    addBoxLine(x0, y0, z1, x1, y0, z1);
    addBoxLine(x1, y0, z1, x1, y1, z1);
    addBoxLine(x1, y1, z1, x0, y1, z1);
    addBoxLine(x0, y1, z1, x0, y0, z1);
    // Pillars
    addBoxLine(x0, y0, z0, x0, y0, z1);
    addBoxLine(x1, y0, z0, x1, y0, z1);
    addBoxLine(x1, y1, z0, x1, y1, z1);
    addBoxLine(x0, y1, z0, x0, y1, z1);

    float dx = x1 - x0;
    float dy = y1 - y0;
    float diagXY = std::sqrt(dx * dx + dy * dy);
    if (diagXY < 1.0f) diagXY = 1000.0f;

    QVector3D camRight(m_mvMatrix(0, 0), m_mvMatrix(0, 1), m_mvMatrix(0, 2));
    QVector3D camUp(m_mvMatrix(1, 0), m_mvMatrix(1, 1), m_mvMatrix(1, 2));

    // Coordinate orientation axis tripod at origin corner (x0, y0, z0)
    float axisLen = diagXY * 0.055f;
    float coneLen = axisLen * 0.22f;
    float coneRadius = axisLen * 0.065f;
    float labelSize = axisLen * 0.22f;

    // East (+X) Axis - Red
    thickLines.push_back({ x0, y0, z0, 0.92f, 0.22f, 0.22f, 1.0f });
    thickLines.push_back({ x0 + axisLen, y0, z0, 0.92f, 0.22f, 0.22f, 1.0f });
    addArrowCone(coneTris, x0 + axisLen, y0, z0, 1.0f, 0.0f, 0.0f, coneLen, coneRadius, 0.92f, 0.22f, 0.22f);
    addBillboardLabel(boxLines, "E", x0 + axisLen + coneLen * 0.9f, y0, z0,
                      0.95f, 0.25f, 0.25f, 1.0f, labelSize, camRight, camUp);

    // North (+Y) Axis - Green
    thickLines.push_back({ x0, y0, z0, 0.18f, 0.80f, 0.32f, 1.0f });
    thickLines.push_back({ x0, y0 + axisLen, z0, 0.18f, 0.80f, 0.32f, 1.0f });
    addArrowCone(coneTris, x0, y0 + axisLen, z0, 0.0f, 1.0f, 0.0f, coneLen, coneRadius, 0.18f, 0.80f, 0.32f);
    addBillboardLabel(boxLines, "N", x0, y0 + axisLen + coneLen * 0.9f, z0,
                      0.20f, 0.85f, 0.35f, 1.0f, labelSize, camRight, camUp);

    // Compass cardinal direction labels on outer bounding box top edges
    float edgeLabelSize = diagXY * 0.016f;
    float midX = (x0 + x1) * 0.5f;
    float midY = (y0 + y1) * 0.5f;
    addBillboardLabel(boxLines, "NORTH", midX, y1, z1, 0.18f, 0.80f, 0.32f, 1.0f, edgeLabelSize, camRight, camUp);
    addBillboardLabel(boxLines, "SOUTH", midX, y0, z1, 0.55f, 0.60f, 0.68f, 1.0f, edgeLabelSize, camRight, camUp);
    addBillboardLabel(boxLines, "EAST",  x1, midY, z1, 0.92f, 0.25f, 0.25f, 1.0f, edgeLabelSize, camRight, camUp);
    addBillboardLabel(boxLines, "WEST",  x0, midY, z1, 0.55f, 0.60f, 0.68f, 1.0f, edgeLabelSize, camRight, camUp);

    // Elevation / Depth Ticks on Vertical Corner Post (Southwest pillar)
    float zSpan = z1Raw - z0Raw;
    if (zSpan > 0.0f) {
        double roughStep = static_cast<double>(zSpan) / 4.0;
        double exponent = std::floor(std::log10(std::max(1e-4, roughStep)));
        double fraction = roughStep / std::pow(10.0, exponent);
        double niceFraction;
        if (fraction <= 1.5) niceFraction = 1.0;
        else if (fraction <= 3.0) niceFraction = 2.0;
        else if (fraction <= 7.0) niceFraction = 5.0;
        else niceFraction = 10.0;
        double step = niceFraction * std::pow(10.0, exponent);
        if (step < 1.0) step = 1.0;

        float tickLen = diagXY * 0.012f;
        float scaledSpacing = static_cast<float>(step) * m_zScale;
        float tickFontSize = std::min(diagXY * 0.0075f, scaledSpacing * 0.25f);
        tickFontSize = std::max(tickFontSize, diagXY * 0.0035f);

        double startZ = std::ceil(static_cast<double>(z0Raw) / step) * step;

        for (double zVal = startZ; zVal <= static_cast<double>(z1Raw) + 1e-4; zVal += step) {
            float zPos = zMid + (static_cast<float>(zVal) - zMid) * m_zScale;

            thickLines.push_back({ x0, y0, zPos, 0.65f, 0.70f, 0.80f, 1.0f });
            thickLines.push_back({ x0 - tickLen * 0.7071f, y0 - tickLen * 0.7071f, zPos, 0.65f, 0.70f, 0.80f, 1.0f });

            int zInt = static_cast<int>(std::round(zVal));
            std::string tickStr = std::to_string(zInt) + "m";

            float labelDist = tickLen * 0.7071f + tickFontSize * static_cast<float>(tickStr.size()) * 0.42f + diagXY * 0.003f;
            addBillboardLabel(boxLines, tickStr,
                               x0 - labelDist * 0.7071f,
                               y0 - labelDist * 0.7071f,
                               zPos,
                               0.65f, 0.72f, 0.82f, 1.0f,
                               tickFontSize,
                               camRight, camUp);
        }

        float topZPos = z1 + tickFontSize * 1.5f;
        addBillboardLabel(boxLines, "Z (m)",
                           x0 - tickLen * 0.7071f,
                           y0 - tickLen * 0.7071f,
                           topZPos,
                           0.30f, 0.65f, 1.0f, 1.0f,
                           tickFontSize * 1.15f,
                           camRight, camUp);
    }

    // South Base Edge (X-Axis) Coordinate Ticks
    if (dx > 0.0f) {
        double roughStep = static_cast<double>(dx) / 4.0;
        double exponent = std::floor(std::log10(std::max(1e-4, roughStep)));
        double fraction = roughStep / std::pow(10.0, exponent);
        double niceFraction = 1.0;
        if (fraction <= 1.5) niceFraction = 1.0;
        else if (fraction <= 3.0) niceFraction = 2.0;
        else if (fraction <= 7.0) niceFraction = 5.0;
        else niceFraction = 10.0;
        double step = niceFraction * std::pow(10.0, exponent);
        if (step < 1.0) step = 1.0;

        float tickLen = diagXY * 0.012f;
        float tickFontSize = std::max(diagXY * 0.0075f, diagXY * 0.0035f);
        double startX = std::ceil(static_cast<double>(x0) / step) * step;

        for (double xVal = startX; xVal <= static_cast<double>(x1) + 1e-4; xVal += step) {
            float xPos = static_cast<float>(xVal);
            thickLines.push_back({ xPos, y0, z0, 0.65f, 0.70f, 0.80f, 1.0f });
            thickLines.push_back({ xPos, y0 - tickLen, z0, 0.65f, 0.70f, 0.80f, 1.0f });

            int xInt = static_cast<int>(std::round(xVal));
            std::string tickStr = std::to_string(xInt) + "m";
            addBillboardLabel(boxLines, tickStr, xPos, y0 - tickLen * 1.8f, z0,
                               0.65f, 0.72f, 0.82f, 1.0f, tickFontSize, camRight, camUp);
        }
        addBillboardLabel(boxLines, "X (m)", x1 + tickLen * 2.0f, y0 - tickLen * 1.8f, z0,
                           0.92f, 0.25f, 0.25f, 1.0f, tickFontSize * 1.15f, camRight, camUp);
    }

    // West Base Edge (Y-Axis) Coordinate Ticks
    if (dy > 0.0f) {
        double roughStep = static_cast<double>(dy) / 4.0;
        double exponent = std::floor(std::log10(std::max(1e-4, roughStep)));
        double fraction = roughStep / std::pow(10.0, exponent);
        double niceFraction = 1.0;
        if (fraction <= 1.5) niceFraction = 1.0;
        else if (fraction <= 3.0) niceFraction = 2.0;
        else if (fraction <= 7.0) niceFraction = 5.0;
        else niceFraction = 10.0;
        double step = niceFraction * std::pow(10.0, exponent);
        if (step < 1.0) step = 1.0;

        float tickLen = diagXY * 0.012f;
        float tickFontSize = std::max(diagXY * 0.0075f, diagXY * 0.0035f);
        double startY = std::ceil(static_cast<double>(y0) / step) * step;

        for (double yVal = startY; yVal <= static_cast<double>(y1) + 1e-4; yVal += step) {
            float yPos = static_cast<float>(yVal);
            thickLines.push_back({ x0, yPos, z0, 0.65f, 0.70f, 0.80f, 1.0f });
            thickLines.push_back({ x0 - tickLen, yPos, z0, 0.65f, 0.70f, 0.80f, 1.0f });

            int yInt = static_cast<int>(std::round(yVal));
            std::string tickStr = std::to_string(yInt) + "m";
            addBillboardLabel(boxLines, tickStr, x0 - tickLen * 1.8f, yPos, z0,
                               0.65f, 0.72f, 0.82f, 1.0f, tickFontSize, camRight, camUp);
        }
        addBillboardLabel(boxLines, "Y (m)", x0 - tickLen * 1.8f, y1 + tickLen * 2.0f, z0,
                           0.20f, 0.85f, 0.35f, 1.0f, tickFontSize * 1.15f, camRight, camUp);
    }

    renderLines(boxLines, 1.2f);
    renderLines(thickLines, 2.0f);
    renderFlatTriangles(coneTris, false);
}

void GL3DView::drawRelief() {
    const auto &m = m_doc->model();
    int rows = m.rows();
    int cols = m.cols();
    int rMax = rows - 2;
    int cMax = cols - 2;
    if (rMax < 1 || cMax < 1) return;

    float zMid = static_cast<float>((m.z_min_val() + m.z_max_val()) * 0.5);

    auto scaleZ = [this, zMid](double zRaw) -> float {
        return zMid + static_cast<float>(zRaw - zMid) * m_zScale;
    };

    // Calculate elevation range across core grid for colormap normalization
    float zMinRelief = std::numeric_limits<float>::max();
    float zMaxRelief = -std::numeric_limits<float>::max();
    for (int rIdx = 1; rIdx <= rMax; ++rIdx) {
        for (int c = 1; c <= cMax; ++c) {
            float zv = static_cast<float>(m.z(rIdx, c, 0));
            if (zv < zMinRelief) zMinRelief = zv;
            if (zv > zMaxRelief) zMaxRelief = zv;
        }
    }
    float zSpan = zMaxRelief - zMinRelief;
    if (zSpan < 1e-4f) zSpan = 1.0f;

    auto getT = [zMinRelief, zSpan](float z) -> float {
        return std::clamp((z - zMinRelief) / zSpan, 0.0f, 1.0f);
    };

    if (m_reliefMode == ReliefMode::Surface) {
        std::vector<FlatVertex3D> reliefTris;
        std::vector<FlatVertex3D> reliefGridLines;
        reliefTris.reserve(static_cast<size_t>(rMax * cMax * 6));
        reliefGridLines.reserve(static_cast<size_t>(rMax * cMax * 4));

        float alphaSurf = 0.88f;

        for (int rIdx = 1; rIdx < rMax; ++rIdx) {
            float y0 = static_cast<float>(m.yd_val(rIdx));
            float y1 = static_cast<float>(m.yd_val(rIdx + 1));
            for (int c = 1; c < cMax; ++c) {
                float x0 = static_cast<float>(m.xd_val(c));
                float x1 = static_cast<float>(m.xd_val(c + 1));

                float z00Raw = static_cast<float>(m.z(rIdx, c, 0));
                float z10Raw = static_cast<float>(m.z(rIdx, c + 1, 0));
                float z11Raw = static_cast<float>(m.z(rIdx + 1, c + 1, 0));
                float z01Raw = static_cast<float>(m.z(rIdx + 1, c, 0));

                float z00 = scaleZ(z00Raw);
                float z10 = scaleZ(z10Raw);
                float z11 = scaleZ(z11Raw);
                float z01 = scaleZ(z01Raw);

                float r00, g00, b00;
                float r10, g10, b10;
                float r11, g11, b11;
                float r01, g01, b01;

                getReliefColor(getT(z00Raw), r00, g00, b00);
                getReliefColor(getT(z10Raw), r10, g10, b10);
                getReliefColor(getT(z11Raw), r11, g11, b11);
                getReliefColor(getT(z01Raw), r01, g01, b01);

                // Two triangles forming quad: (00 -> 10 -> 11) and (00 -> 11 -> 01)
                reliefTris.push_back({ x0, y0, z00, r00, g00, b00, alphaSurf });
                reliefTris.push_back({ x1, y0, z10, r10, g10, b10, alphaSurf });
                reliefTris.push_back({ x1, y1, z11, r11, g11, b11, alphaSurf });

                reliefTris.push_back({ x0, y0, z00, r00, g00, b00, alphaSurf });
                reliefTris.push_back({ x1, y1, z11, r11, g11, b11, alphaSurf });
                reliefTris.push_back({ x0, y1, z01, r01, g01, b01, alphaSurf });

                // Subtle grid overlay for high-definition topographic features
                float lineAlpha = 0.35f;
                reliefGridLines.push_back({ x0, y0, z00, r00 * 0.7f, g00 * 0.7f, b00 * 0.7f, lineAlpha });
                reliefGridLines.push_back({ x1, y0, z10, r10 * 0.7f, g10 * 0.7f, b10 * 0.7f, lineAlpha });

                reliefGridLines.push_back({ x0, y0, z00, r00 * 0.7f, g00 * 0.7f, b00 * 0.7f, lineAlpha });
                reliefGridLines.push_back({ x0, y1, z01, r01 * 0.7f, g01 * 0.7f, b01 * 0.7f, lineAlpha });
            }
        }

        renderFlatTriangles(reliefTris, true);
        renderLines(reliefGridLines, 1.0f);
    } else {
        // ReliefMode::Wireframe
        std::vector<FlatVertex3D> reliefLines;
        float lineAlpha = 0.90f;

        // Grid lines along X (running West-East at each core row)
        for (int rIdx = 1; rIdx <= rMax; ++rIdx) {
            float y = static_cast<float>(m.yd_val(rIdx));
            for (int c = 1; c < cMax; ++c) {
                float xA = static_cast<float>(m.xd_val(c));
                float xB = static_cast<float>(m.xd_val(c + 1));
                float zARaw = static_cast<float>(m.z(rIdx, c, 0));
                float zBRaw = static_cast<float>(m.z(rIdx, c + 1, 0));
                float zA = scaleZ(zARaw);
                float zB = scaleZ(zBRaw);
                float rA, gA, bA, rB, gB, bB;
                getReliefColor(getT(zARaw), rA, gA, bA);
                getReliefColor(getT(zBRaw), rB, gB, bB);

                reliefLines.push_back({ xA, y, zA, rA, gA, bA, lineAlpha });
                reliefLines.push_back({ xB, y, zB, rB, gB, bB, lineAlpha });
            }
        }
        // Grid lines along Y (running South-North at each core col)
        for (int c = 1; c <= cMax; ++c) {
            float x = static_cast<float>(m.xd_val(c));
            for (int rIdx = 1; rIdx < rMax; ++rIdx) {
                float yA = static_cast<float>(m.yd_val(rIdx));
                float yB = static_cast<float>(m.yd_val(rIdx + 1));
                float zARaw = static_cast<float>(m.z(rIdx, c, 0));
                float zBRaw = static_cast<float>(m.z(rIdx + 1, c, 0));
                float zA = scaleZ(zARaw);
                float zB = scaleZ(zBRaw);
                float rA, gA, bA, rB, gB, bB;
                getReliefColor(getT(zARaw), rA, gA, bA);
                getReliefColor(getT(zBRaw), rB, gB, bB);

                reliefLines.push_back({ x, yA, zA, rA, gA, bA, lineAlpha });
                reliefLines.push_back({ x, yB, zB, rB, gB, bB, lineAlpha });
            }
        }

        renderLines(reliefLines, 1.4f);
    }
}

void GL3DView::drawBodies() {
    initShaders();
    initBuffers();

    if (!m_meshDirty && m_doc) {
        const auto &modelBodies = m_doc->model().bodies();
        if (modelBodies.size() != m_bodyMeshes.size()) {
            m_meshDirty = true;
        } else {
            for (const auto &mesh : m_bodyMeshes) {
                if (!m_doc->model().body(mesh.bodyId)) {
                    m_meshDirty = true;
                    break;
                }
            }
        }
    }

    if (m_meshDirty || (!m_doc->model().bodies().empty() && m_bodyRanges.empty())) {
        updateBodyBuffers();
    }

    if (m_bodyMeshes.empty()) return;

    double dBot = m_doc->model().z_min_val();
    float zMid = static_cast<float>((m_doc->model().z_min_val() + m_doc->model().z_max_val()) * 0.5);

    auto scaleZ = [this, zMid](double zRaw) -> float {
        return zMid + static_cast<float>(zRaw - zMid) * m_zScale;
    };

    // 1. OPAQUE BODIES PASS
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    if (m_wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.0f, 1.0f);
    }

    if (m_shadersInitialized && m_shaderProg) {
        m_shaderProg->bind();
        m_shaderProg->setUniformValue("u_mvpMatrix", m_projMatrix * m_mvMatrix);
        m_shaderProg->setUniformValue("u_mvMatrix", m_mvMatrix);
        m_shaderProg->setUniformValue("u_normalMatrix", m_mvMatrix.normalMatrix());
        m_shaderProg->setUniformValue("u_zMid", zMid);
        m_shaderProg->setUniformValue("u_zScale", m_zScale);
        m_shaderProg->setUniformValue("u_light0Dir", QVector3D(0.5f, 0.7f, 1.0f));
        m_shaderProg->setUniformValue("u_light1Dir", QVector3D(-0.5f, -0.6f, 0.4f));
        m_shaderProg->setUniformValue("u_light0Color", QVector4D(0.85f, 0.85f, 0.85f, 1.0f));
        m_shaderProg->setUniformValue("u_light1Color", QVector4D(0.30f, 0.30f, 0.35f, 1.0f));
        m_shaderProg->setUniformValue("u_ambientColor", QVector4D(0.28f, 0.28f, 0.30f, 1.0f));
        m_shaderProg->setUniformValue("u_enableFog", m_showDepthFog);
        float effDist = m_distance / std::max(0.01f, m_zoom);
        m_shaderProg->setUniformValue("u_fogStart", effDist * 0.75f);
        m_shaderProg->setUniformValue("u_fogEnd", effDist * 2.2f);
        m_shaderProg->setUniformValue("u_fogColor", QVector4D(0.92f, 0.93f, 0.95f, 1.0f));
        m_shaderProg->setUniformValue("u_highlightColor", QVector4D(1.0f, 0.85f, 0.15f, 1.0f));

        m_bodyVao.bind();
        for (const auto &range : m_bodyRanges) {
            if (range.isTransparent || range.count == 0) continue;
            m_shaderProg->setUniformValue("u_highlight", (range.bodyId == m_selectedBodyId));
            glDrawArrays(GL_TRIANGLES, range.first, range.count);
        }
        m_bodyVao.release();
        m_shaderProg->release();
    }

    if (!m_wireframe) {
        glDisable(GL_POLYGON_OFFSET_FILL);

        // Feature edge outlines for opaque bodies
        if (m_showFeatureEdges) {
            std::vector<FlatVertex3D> edgeLines;
            std::vector<FlatVertex3D> selEdgeLines;

            for (const auto &mesh : m_bodyMeshes) {
                if (!mesh.isVisible || (m_transparentBodies && mesh.isTransparent)) continue;

                bool isSelected = (mesh.bodyId == m_selectedBodyId);
                auto &targetList = isSelected ? selEdgeLines : edgeLines;

                float r, g, b, a;
                if (isSelected) {
                    r = 1.0f; g = 0.85f; b = 0.15f; a = 1.0f;
                } else {
                    auto bc = mesh.color;
                    r = bc.r * 0.45f / 255.0f;
                    g = bc.g * 0.45f / 255.0f;
                    b = bc.b * 0.45f / 255.0f;
                    a = 0.85f;
                }

                for (const auto &edge : mesh.featureEdges) {
                    if (!m_showBottom && edge.first.z <= dBot + 1e-3 && edge.second.z <= dBot + 1e-3) {
                        continue;
                    }
                    targetList.push_back({ static_cast<float>(edge.first.x), static_cast<float>(edge.first.y), scaleZ(edge.first.z), r, g, b, a });
                    targetList.push_back({ static_cast<float>(edge.second.x), static_cast<float>(edge.second.y), scaleZ(edge.second.z), r, g, b, a });
                }
            }

            renderLines(edgeLines, 1.2f);
            renderLines(selEdgeLines, 2.5f);
        }
    }

    // 2. TRANSPARENT BODIES PASS (Depth-Sorted Back-to-Front)
    bool hasTransparent = false;
    if (m_transparentBodies) {
        for (const auto &mesh : m_bodyMeshes) {
            if (mesh.isVisible && mesh.isTransparent) {
                hasTransparent = true;
                break;
            }
        }
    }

    if (hasTransparent) {
        struct TransFacet {
            QVector3D p0, p1, p2;
            QVector3D normal;
            float r, g, b, a;
            float depth;
            int bodyId;
        };
        std::vector<TransFacet> transFacets;

        // Camera orientation for depth sorting
        float pRad = (m_pitch - 90.0f) * 3.14159265f / 180.0f;
        float azRad = m_azimuth * 3.14159265f / 180.0f;
        float cosAz = std::cos(azRad), sinAz = std::sin(azRad);
        float cosP = std::cos(pRad), sinP = std::sin(pRad);

        auto eyeDepth = [&](const QVector3D &pt) -> float {
            float dx = pt.x() - m_target.x();
            float dy = pt.y() - m_target.y();
            float dz = pt.z() - m_target.z();
            float ry = dx * sinAz + dy * cosAz;
            float rz = dz;
            return ry * sinP + rz * cosP;
        };

        for (const auto &mesh : m_bodyMeshes) {
            if (!mesh.isVisible || !mesh.isTransparent) continue;

            auto bc = mesh.color;
            float alphaVal = std::clamp(mesh.alpha, 0.15f, 0.85f);
            float rVal = bc.r / 255.0f;
            float gVal = bc.g / 255.0f;
            float bVal = bc.b / 255.0f;

            for (const auto &fct : mesh.facets) {
                if (!m_showBottom &&
                    fct[0].z <= dBot + 1e-3 &&
                    fct[1].z <= dBot + 1e-3 &&
                    fct[2].z <= dBot + 1e-3) {
                    continue;
                }

                QVector3D v0(static_cast<float>(fct[0].x), static_cast<float>(fct[0].y), static_cast<float>(fct[0].z));
                QVector3D v1(static_cast<float>(fct[1].x), static_cast<float>(fct[1].y), static_cast<float>(fct[1].z));
                QVector3D v2(static_cast<float>(fct[2].x), static_cast<float>(fct[2].y), static_cast<float>(fct[2].z));

                auto norm = fct.normal();
                double nLen = std::sqrt(norm.x*norm.x + norm.y*norm.y + norm.z*norm.z);
                if (nLen > 1e-6) { norm.x /= nLen; norm.y /= nLen; norm.z /= nLen; }

                QVector3D v0Scaled(v0.x(), v0.y(), scaleZ(v0.z()));
                QVector3D v1Scaled(v1.x(), v1.y(), scaleZ(v1.z()));
                QVector3D v2Scaled(v2.x(), v2.y(), scaleZ(v2.z()));
                float d = eyeDepth((v0Scaled + v1Scaled + v2Scaled) * (1.0f / 3.0f));

                transFacets.push_back({ v0, v1, v2, QVector3D(norm.x, norm.y, norm.z), rVal, gVal, bVal, alphaVal, d, mesh.bodyId });
            }
        }

        // Sort back-to-front (smaller eye-space depth is further from camera)
        std::sort(transFacets.begin(), transFacets.end(), [](const TransFacet &a, const TransFacet &b) {
            return a.depth < b.depth;
        });

        std::vector<Vertex3D> transVerts;
        transVerts.reserve(transFacets.size() * 3);

        for (const auto &tf : transFacets) {
            float rVal = tf.r;
            float gVal = tf.g;
            float bVal = tf.b;
            if (tf.bodyId == m_selectedBodyId) {
                rVal = std::min(1.0f, rVal * 0.6f + 0.40f);
                gVal = std::min(1.0f, gVal * 0.6f + 0.34f);
                bVal = std::min(1.0f, bVal * 0.6f + 0.06f);
            }

            transVerts.push_back({ tf.p0.x(), tf.p0.y(), tf.p0.z(), tf.normal.x(), tf.normal.y(), tf.normal.z(), rVal, gVal, bVal, tf.a });
            transVerts.push_back({ tf.p1.x(), tf.p1.y(), tf.p1.z(), tf.normal.x(), tf.normal.y(), tf.normal.z(), rVal, gVal, bVal, tf.a });
            transVerts.push_back({ tf.p2.x(), tf.p2.y(), tf.p2.z(), tf.normal.x(), tf.normal.y(), tf.normal.z(), rVal, gVal, bVal, tf.a });
        }

        if (!transVerts.empty() && m_shadersInitialized && m_shaderProg) {
            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            if (m_wireframe) {
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            } else {
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            }

            m_transVbo.bind();
            m_transVbo.allocate(transVerts.data(), static_cast<int>(transVerts.size() * sizeof(Vertex3D)));
            m_transVbo.release();

            m_shaderProg->bind();
            m_shaderProg->setUniformValue("u_mvpMatrix", m_projMatrix * m_mvMatrix);
            m_shaderProg->setUniformValue("u_mvMatrix", m_mvMatrix);
            m_shaderProg->setUniformValue("u_normalMatrix", m_mvMatrix.normalMatrix());
            m_shaderProg->setUniformValue("u_zMid", zMid);
            m_shaderProg->setUniformValue("u_zScale", m_zScale);
            m_shaderProg->setUniformValue("u_highlight", false);

            m_transVao.bind();
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(transVerts.size()));
            m_transVao.release();
            m_shaderProg->release();

            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
        }

        if (!m_wireframe && m_showFeatureEdges) {
            std::vector<FlatVertex3D> transEdgeLines;
            std::vector<FlatVertex3D> transSelEdgeLines;

            for (const auto &mesh : m_bodyMeshes) {
                if (!mesh.isVisible || !mesh.isTransparent) continue;

                bool isSelected = (mesh.bodyId == m_selectedBodyId);
                auto &targetList = isSelected ? transSelEdgeLines : transEdgeLines;

                float r, g, b, a;
                if (isSelected) {
                    r = 1.0f; g = 0.85f; b = 0.15f; a = 1.0f;
                } else {
                    auto bc = mesh.color;
                    r = bc.r * 0.45f / 255.0f;
                    g = bc.g * 0.45f / 255.0f;
                    b = bc.b * 0.45f / 255.0f;
                    a = 0.65f;
                }

                for (const auto &edge : mesh.featureEdges) {
                    if (!m_showBottom && edge.first.z <= dBot + 1e-3 && edge.second.z <= dBot + 1e-3) {
                        continue;
                    }
                    targetList.push_back({ static_cast<float>(edge.first.x), static_cast<float>(edge.first.y), scaleZ(edge.first.z), r, g, b, a });
                    targetList.push_back({ static_cast<float>(edge.second.x), static_cast<float>(edge.second.y), scaleZ(edge.second.z), r, g, b, a });
                }
            }

            renderLines(transEdgeLines, 1.2f);
            renderLines(transSelEdgeLines, 2.5f);
        }
    }

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

QMdiArea *GL3DView::findMdiArea() const {
    QWidget *w = parentWidget();
    while (w) {
        if (auto *sub = qobject_cast<QMdiSubWindow*>(w)) {
            if (sub->mdiArea()) return sub->mdiArea();
        }
        if (auto *mdi = qobject_cast<QMdiArea*>(w)) {
            return mdi;
        }
        w = w->parentWidget();
    }
    return nullptr;
}

void GL3DView::drawProfilePlane() {
    if (!m_doc) return;
    const auto &m = m_doc->model();

    float z0Raw = static_cast<float>(m.z_min_val());
    float z1Raw = static_cast<float>(m.z_max_val());
    float zMid = (z0Raw + z1Raw) * 0.5f;
    float z0 = zMid + (z0Raw - zMid) * m_zScale;
    float z1 = zMid + (z1Raw - zMid) * m_zScale;

    float x0 = static_cast<float>(m.x_min_val());
    float x1 = static_cast<float>(m.x_max_val());
    float y0 = static_cast<float>(m.y_min_val());
    float y1 = static_cast<float>(m.y_max_val());

    struct ProfileSliceToDraw {
        ProfileOrientation orient;
        int index;
        bool isActive;
    };
    std::vector<ProfileSliceToDraw> slices;

    QMdiArea *mdi = findMdiArea();
    if (mdi) {
        std::vector<ProfileView*> openPvs;
        for (auto *sub : mdi->subWindowList()) {
            if (auto *pv = qobject_cast<ProfileView*>(sub->widget())) {
                openPvs.push_back(pv);
            }
        }

        ProfileView *activePv = nullptr;
        if (auto *actSub = mdi->activeSubWindow()) {
            activePv = qobject_cast<ProfileView*>(actSub->widget());
        }
        if (activePv) {
            m_lastActiveProfileView = activePv;
        } else {
            if (auto *mw = qobject_cast<MainWindow*>(mdi->window())) {
                activePv = mw->activeProfileView();
            }
            if (!activePv && m_lastActiveProfileView) {
                if (std::find(openPvs.begin(), openPvs.end(), m_lastActiveProfileView.data()) != openPvs.end()) {
                    activePv = m_lastActiveProfileView.data();
                }
            }
            if (!activePv) {
                for (auto *pv : openPvs) {
                    if (pv->profileOrientation() == m_doc->profileOrientation() &&
                        pv->profileIndex() == m_doc->profileIndex()) {
                        activePv = pv;
                        break;
                    }
                }
            }
            if (!activePv) {
                for (auto *pv : openPvs) {
                    if (pv->profileOrientation() == m_doc->profileOrientation()) {
                        activePv = pv;
                        break;
                    }
                }
            }
            if (!activePv && !openPvs.empty()) {
                activePv = openPvs.front();
            }
        }

        for (auto *pv : openPvs) {
            slices.push_back({ pv->profileOrientation(), pv->profileIndex(), (pv == activePv) });
        }
    }

    if (slices.empty()) {
        slices.push_back({ m_doc->profileOrientation(), m_doc->profileIndex(), true });
    }

    // 1. Semi-transparent slice planes pass
    if (m_transparentProfiles) {
        std::vector<FlatVertex3D> profileTris;

        auto addQuad = [&](const QVector3D &p0, const QVector3D &p1, const QVector3D &p2, const QVector3D &p3,
                           float r, float g, float b, float a) {
            profileTris.push_back({ p0.x(), p0.y(), p0.z(), r, g, b, a });
            profileTris.push_back({ p1.x(), p1.y(), p1.z(), r, g, b, a });
            profileTris.push_back({ p2.x(), p2.y(), p2.z(), r, g, b, a });

            profileTris.push_back({ p0.x(), p0.y(), p0.z(), r, g, b, a });
            profileTris.push_back({ p2.x(), p2.y(), p2.z(), r, g, b, a });
            profileTris.push_back({ p3.x(), p3.y(), p3.z(), r, g, b, a });
        };

        // Inactive profile planes
        for (const auto &sl : slices) {
            if (sl.isActive) continue;
            float r = 0.35f, g = 0.55f, b = 0.85f, a = 0.08f;
            if (sl.orient == ProfileOrientation::EastWest) {
                float y = static_cast<float>(m.yd_val(sl.index + 1));
                addQuad(QVector3D(x0, y, z0), QVector3D(x1, y, z0), QVector3D(x1, y, z1), QVector3D(x0, y, z1), r, g, b, a);
            } else {
                float x = static_cast<float>(m.xd_val(sl.index + 1));
                addQuad(QVector3D(x, y0, z0), QVector3D(x, y1, z0), QVector3D(x, y1, z1), QVector3D(x, y0, z1), r, g, b, a);
            }
        }

        // Active profile plane
        for (const auto &sl : slices) {
            if (!sl.isActive) continue;
            float r = 0.96f, g = 0.28f, b = 0.22f, a = 0.26f;
            if (sl.orient == ProfileOrientation::EastWest) {
                float y = static_cast<float>(m.yd_val(sl.index + 1));
                addQuad(QVector3D(x0, y, z0), QVector3D(x1, y, z0), QVector3D(x1, y, z1), QVector3D(x0, y, z1), r, g, b, a);
            } else {
                float x = static_cast<float>(m.xd_val(sl.index + 1));
                addQuad(QVector3D(x, y0, z0), QVector3D(x, y1, z0), QVector3D(x, y1, z1), QVector3D(x, y0, z1), r, g, b, a);
            }
        }

        renderFlatTriangles(profileTris, true);
    }

    // 2. Profile outlines
    std::vector<FlatVertex3D> inactiveLines;
    std::vector<FlatVertex3D> activeLines;
    std::vector<FlatVertex3D> ridgeLines;

    auto addLoopLines = [&](std::vector<FlatVertex3D> &target,
                            const QVector3D &p0, const QVector3D &p1, const QVector3D &p2, const QVector3D &p3,
                            float r, float g, float b, float a) {
        target.push_back({ p0.x(), p0.y(), p0.z(), r, g, b, a });
        target.push_back({ p1.x(), p1.y(), p1.z(), r, g, b, a });

        target.push_back({ p1.x(), p1.y(), p1.z(), r, g, b, a });
        target.push_back({ p2.x(), p2.y(), p2.z(), r, g, b, a });

        target.push_back({ p2.x(), p2.y(), p2.z(), r, g, b, a });
        target.push_back({ p3.x(), p3.y(), p3.z(), r, g, b, a });

        target.push_back({ p3.x(), p3.y(), p3.z(), r, g, b, a });
        target.push_back({ p0.x(), p0.y(), p0.z(), r, g, b, a });
    };

    for (const auto &sl : slices) {
        if (sl.isActive) continue;
        float r = 0.35f, g = 0.60f, b = 0.85f, a = 0.50f;
        if (sl.orient == ProfileOrientation::EastWest) {
            float y = static_cast<float>(m.yd_val(sl.index + 1));
            addLoopLines(inactiveLines, QVector3D(x0, y, z0), QVector3D(x1, y, z0), QVector3D(x1, y, z1), QVector3D(x0, y, z1), r, g, b, a);
        } else {
            float x = static_cast<float>(m.xd_val(sl.index + 1));
            addLoopLines(inactiveLines, QVector3D(x, y0, z0), QVector3D(x, y1, z0), QVector3D(x, y1, z1), QVector3D(x, y0, z1), r, g, b, a);
        }
    }

    for (const auto &sl : slices) {
        if (!sl.isActive) continue;
        float r = 0.96f, g = 0.20f, b = 0.15f, a = 0.95f;
        if (sl.orient == ProfileOrientation::EastWest) {
            float y = static_cast<float>(m.yd_val(sl.index + 1));
            addLoopLines(activeLines, QVector3D(x0, y, z0), QVector3D(x1, y, z0), QVector3D(x1, y, z1), QVector3D(x0, y, z1), r, g, b, a);
            // Golden accent line on top ridge
            ridgeLines.push_back({ x0, y, z1, 1.0f, 0.85f, 0.20f, 0.95f });
            ridgeLines.push_back({ x1, y, z1, 1.0f, 0.85f, 0.20f, 0.95f });
        } else {
            float x = static_cast<float>(m.xd_val(sl.index + 1));
            addLoopLines(activeLines, QVector3D(x, y0, z0), QVector3D(x, y1, z0), QVector3D(x, y1, z1), QVector3D(x, y0, z1), r, g, b, a);
            // Golden accent line on top ridge
            ridgeLines.push_back({ x, y0, z1, 1.0f, 0.85f, 0.20f, 0.95f });
            ridgeLines.push_back({ x, y1, z1, 1.0f, 0.85f, 0.20f, 0.95f });
        }
    }

    renderLines(inactiveLines, 1.2f);
    renderLines(activeLines, 2.8f);
    renderLines(ridgeLines, 3.0f);
}

int GL3DView::hitTestGizmo(const QPoint &pos) {
    if (!m_showOrientationIndicator) return -1;

    int hudSize = 130;
    int margin = 16;
    int x0 = margin;
    int y0 = height() - margin - hudSize;
    m_gizmoRect = QRect(x0, y0, hudSize, hudSize);

    if (!m_gizmoRect.contains(pos)) return -1;

    float cx = x0 + hudSize * 0.5f;
    float cy = y0 + hudSize * 0.5f;
    float r = hudSize * 0.42f;

    float pRad = (m_pitch - 90.0f) * 3.14159265f / 180.0f;
    float azRad = m_azimuth * 3.14159265f / 180.0f;
    float cosAz = std::cos(azRad), sinAz = std::sin(azRad);
    float cosP = std::cos(pRad), sinP = std::sin(pRad);

    auto projAxis = [&](float ax, float ay, float az) -> QPointF {
        float rx = ax * cosAz - ay * sinAz;
        float ry = ax * sinAz + ay * cosAz;
        float rz = az;
        float px = rx;
        float py = ry * cosP - rz * sinP;
        return QPointF(cx + px * r, cy - py * r);
    };

    struct AxisInfo { int id; float ax, ay, az; };
    AxisInfo axes[] = {
        { 0,  1.0f,  0.0f,  0.0f }, // +X (East)
        { 1, -1.0f,  0.0f,  0.0f }, // -X (West)
        { 2,  0.0f,  1.0f,  0.0f }, // +Y (North)
        { 3,  0.0f, -1.0f,  0.0f }, // -Y (South)
        { 4,  0.0f,  0.0f,  1.0f }, // +Z (Top)
        { 5,  0.0f,  0.0f, -1.0f }, // -Z (Bottom)
    };

    for (const auto &ax : axes) {
        QPointF p = projAxis(ax.ax, ax.ay, ax.az);
        if (QLineF(pos, p).length() <= 16.0) {
            return ax.id;
        }
    }

    return -1;
}

void GL3DView::drawOrientationHud(QPainter &painter) {
    if (!painter.isActive()) return;
    int hudSize = 130;
    int margin = 16;
    int x0 = margin;
    int y0 = height() - margin - hudSize;
    m_gizmoRect = QRect(x0, y0, hudSize, hudSize);

    float cx = x0 + hudSize * 0.5f;
    float cy = y0 + hudSize * 0.5f;
    float r = hudSize * 0.42f;

    float pRad = (m_pitch - 90.0f) * 3.14159265f / 180.0f;
    float azRad = m_azimuth * 3.14159265f / 180.0f;
    float cosAz = std::cos(azRad), sinAz = std::sin(azRad);
    float cosP = std::cos(pRad), sinP = std::sin(pRad);

    auto projAxis = [&](float ax, float ay, float az) -> QPointF {
        float rx = ax * cosAz - ay * sinAz;
        float ry = ax * sinAz + ay * cosAz;
        float rz = az;
        float px = rx;
        float py = ry * cosP - rz * sinP;
        return QPointF(cx + px * r, cy - py * r);
    };

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // 1. Frosted circular background plate
    painter.setPen(QPen(QColor(90, 105, 130, 180), 1.5));
    painter.setBrush(QColor(18, 24, 34, 185));
    painter.drawEllipse(QPointF(cx, cy), r * 1.18f, r * 1.18f);

    // 2. Compass ring on horizontal XY plane
    painter.setPen(QPen(QColor(120, 140, 170, 90), 1.0));
    painter.setBrush(Qt::NoBrush);
    QPolygonF xyRing;
    for (int i = 0; i <= 36; ++i) {
        float ang = 2.0f * 3.14159265f * static_cast<float>(i) / 36.0f;
        xyRing << projAxis(std::cos(ang) * 0.90f, std::sin(ang) * 0.90f, 0.0f);
    }
    painter.drawPolygon(xyRing);

    // 3. Axes
    struct AxisDrawInfo {
        int id;
        float ax, ay, az;
        QString label;
        QColor normalColor;
        QColor hoverColor;
        bool isNegative;
    };

    AxisDrawInfo axes[] = {
        { 0,  1.0f,  0.0f,  0.0f, "E",  QColor(235, 60, 60),   QColor(255, 110, 110), false },
        { 1, -1.0f,  0.0f,  0.0f, "W",  QColor(140, 155, 175), QColor(200, 215, 235), true  },
        { 2,  0.0f,  1.0f,  0.0f, "N",  QColor(40, 200, 85),   QColor(80, 240, 130),  false },
        { 3,  0.0f, -1.0f,  0.0f, "S",  QColor(140, 155, 175), QColor(200, 215, 235), true  },
        { 4,  0.0f,  0.0f,  1.0f, "UP", QColor(50, 140, 255),  QColor(110, 180, 255), false },
        { 5,  0.0f,  0.0f, -1.0f, "DN", QColor(140, 155, 175), QColor(200, 215, 235), true  },
    };

    auto axisDepth = [&](const AxisDrawInfo &a) -> float {
        float ry = a.ax * sinAz + a.ay * cosAz;
        return ry * sinP + a.az * cosP;
    };

    std::vector<AxisDrawInfo> sortedAxes(std::begin(axes), std::end(axes));
    std::sort(sortedAxes.begin(), sortedAxes.end(), [&](const AxisDrawInfo &a, const AxisDrawInfo &b) {
        return axisDepth(a) < axisDepth(b);
    });

    QFont labelFont = painter.font();
    labelFont.setPixelSize(11);
    labelFont.setBold(true);
    painter.setFont(labelFont);

    QPointF pCenter(cx, cy);

    for (const auto &ax : sortedAxes) {
        bool isHovered = (m_gizmoHoverAxis == ax.id);
        QColor col = isHovered ? ax.hoverColor : ax.normalColor;
        float width = isHovered ? (ax.isNegative ? 2.5f : 3.5f) : (ax.isNegative ? 1.5f : 2.5f);

        QPointF pEnd = projAxis(ax.ax * 0.85f, ax.ay * 0.85f, ax.az * 0.85f);
        QPointF pLabel = projAxis(ax.ax * 1.05f, ax.ay * 1.05f, ax.az * 1.05f);

        QPen pen(col, width);
        if (ax.isNegative) {
            pen.setStyle(Qt::DashLine);
        }
        painter.setPen(pen);
        painter.drawLine(pCenter, pEnd);

        painter.setBrush(col);
        painter.setPen(QPen(QColor(20, 24, 30, 200), 1.0));
        float nodeRadius = isHovered ? 5.0f : 3.5f;
        painter.drawEllipse(pEnd, nodeRadius, nodeRadius);

        painter.setPen(col);
        QRectF textRect(pLabel.x() - 12.0, pLabel.y() - 8.0, 24.0, 16.0);
        painter.drawText(textRect, Qt::AlignCenter, ax.label);
    }

    painter.setBrush(QColor(240, 245, 255));
    painter.setPen(QPen(QColor(30, 35, 45), 1.0));
    painter.drawEllipse(pCenter, 3.5, 3.5);

    painter.restore();
}

void GL3DView::animateToCamera(float targetPitch, float targetAzimuth, float targetDist, const QVector3D *targetLookAt, int durationMs) {
    if (m_camAnim) {
        m_camAnim->stop();
        delete m_camAnim.data();
        m_camAnim = nullptr;
    }

    float startPitch = m_pitch;
    float startAzimuth = m_azimuth;
    float startDist = m_distance;
    QVector3D startTarget = m_target;

    float endDist = (targetDist > 0.0f) ? targetDist : m_distance;
    QVector3D endTarget = (targetLookAt != nullptr) ? *targetLookAt : m_target;

    // Shortest rotational path for azimuth
    while (targetAzimuth - startAzimuth > 180.0f) startAzimuth += 360.0f;
    while (targetAzimuth - startAzimuth < -180.0f) startAzimuth -= 360.0f;

    m_camAnim = new QVariantAnimation(this);
    m_camAnim->setDuration(durationMs);
    m_camAnim->setStartValue(0.0f);
    m_camAnim->setEndValue(1.0f);
    m_camAnim->setEasingCurve(QEasingCurve::InOutCubic);

    connect(m_camAnim.data(), &QVariantAnimation::valueChanged, this, [=, this](const QVariant &v) {
        float t = v.toFloat();
        m_pitch = startPitch + (targetPitch - startPitch) * t;
        m_azimuth = startAzimuth + (targetAzimuth - startAzimuth) * t;
        m_distance = startDist + (endDist - startDist) * t;
        m_target = startTarget + (endTarget - startTarget) * t;

        while (m_azimuth > 180.0f) m_azimuth -= 360.0f;
        while (m_azimuth < -180.0f) m_azimuth += 360.0f;

        emit cameraChanged();
        update();
    });

    connect(m_camAnim.data(), &QVariantAnimation::finished, this, [this]() {
        if (m_camAnim) {
            m_camAnim->deleteLater();
            m_camAnim = nullptr;
        }
    });

    m_camAnim->start();
}

void GL3DView::mousePressEvent(QMouseEvent *event) {
    int hitAx = hitTestGizmo(event->pos());
    if (hitAx >= 0) {
        // Clicked interactive Gizmo axis
        switch (hitAx) {
        case 0: animateToCamera(0.0f, 90.0f); break;
        case 1: animateToCamera(0.0f, -90.0f); break;
        case 2: animateToCamera(0.0f, 180.0f); break;
        case 3: animateToCamera(0.0f, 0.0f); break;
        case 4: animateToCamera(89.9f, 0.0f); break;
        case 5: animateToCamera(-89.9f, 0.0f); break; // Bottom View
        }
        event->accept();
        return;
    }

    if (m_gizmoRect.contains(event->pos())) {
        m_draggingGizmo = true;
    } else {
        m_draggingGizmo = false;
    }

    if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton || event->button() == Qt::MiddleButton) {
        m_mouseDragging = true;
        m_dragButton = event->button();
        m_dragStartPos = event->pos();
        m_lastMousePos = event->pos();
    }
    QOpenGLWidget::mousePressEvent(event);
}

void GL3DView::mouseMoveEvent(QMouseEvent *event) {
    int prevHover = m_gizmoHoverAxis;
    m_gizmoHoverAxis = hitTestGizmo(event->pos());
    if (prevHover != m_gizmoHoverAxis) {
        update();
    }

    if (m_mouseDragging) {
        // Safety check: ensure the initiating drag button is actually still held down.
        // If buttons were released during a menu popup, modal dialog, or window focus change,
        // cancel dragging immediately to prevent unwanted hover panning or orbiting.
        Qt::MouseButtons btns = event->buttons();
        if (btns == Qt::NoButton ||
            (m_dragButton == Qt::LeftButton && !(btns & Qt::LeftButton)) ||
            (m_dragButton == Qt::RightButton && !(btns & Qt::RightButton)) ||
            (m_dragButton == Qt::MiddleButton && !(btns & Qt::MiddleButton))) {
            m_mouseDragging = false;
            m_draggingGizmo = false;
            update();
            QOpenGLWidget::mouseMoveEvent(event);
            return;
        }

        int dx = event->pos().x() - m_lastMousePos.x();
        int dy = event->pos().y() - m_lastMousePos.y();

        if (m_draggingGizmo) {
            // Dragging directly within Gizmo orbits the camera
            float rotSpeed = 0.45f;
            m_azimuth += dx * rotSpeed;
            m_pitch -= dy * rotSpeed;
            if (m_pitch > 89.9f) m_pitch = 89.9f;
            if (m_pitch < -89.9f) m_pitch = -89.9f;
            while (m_azimuth > 180.0f) m_azimuth -= 360.0f;
            while (m_azimuth < -180.0f) m_azimuth += 360.0f;
            emit cameraChanged();
        } else if (m_dragButton == Qt::RightButton || m_dragButton == Qt::MiddleButton || (event->modifiers() & Qt::ShiftModifier)) {
            // Tactile 1:1 Pan shifting m_target across the camera view plane
            float effDist = m_distance / std::max(0.01f, m_zoom);
            float fov = 45.0f * 3.14159265f / 180.0f;
            float viewH = 2.0f * effDist * std::tan(fov * 0.5f);
            float pixelScale = viewH / static_cast<float>(std::max(1, height()));

            // Camera right and up vectors in world coordinates
            float pRad = (m_pitch - 90.0f) * 3.14159265f / 180.0f;
            float azRad = m_azimuth * 3.14159265f / 180.0f;
            float cosAz = std::cos(azRad), sinAz = std::sin(azRad);
            float cosP = std::cos(pRad), sinP = std::sin(pRad);

            QVector3D rightDir(cosAz, -sinAz, 0.0f);
            QVector3D upDir(-sinP * sinAz, -sinP * cosAz, cosP);

            m_target += -dx * pixelScale * rightDir + dy * pixelScale * upDir;
            emit cameraChanged();
        } else if (m_dragButton == Qt::LeftButton) {
            if (event->modifiers() & Qt::ControlModifier) {
                // Interactive zoom with Ctrl+Left drag
                float zoomDelta = 1.0f - static_cast<float>(dy) * 0.008f;
                m_zoom *= zoomDelta;
                if (m_zoom < 0.01f) m_zoom = 0.01f;
                if (m_zoom > 100.0f) m_zoom = 100.0f;
            } else {
                // Standard CAD / DCC Turntable Orbit orbiting around m_target
                float rotSpeed = 0.40f;
                m_azimuth += dx * rotSpeed;
                m_pitch -= dy * rotSpeed;

                if (m_pitch > 89.9f) m_pitch = 89.9f;
                if (m_pitch < -89.9f) m_pitch = -89.9f;

                while (m_azimuth > 180.0f) m_azimuth -= 360.0f;
                while (m_azimuth < -180.0f) m_azimuth += 360.0f;
                emit cameraChanged();
            }
        }

        m_lastMousePos = event->pos();
        update();
    }
    QOpenGLWidget::mouseMoveEvent(event);
}

void GL3DView::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && !m_draggingGizmo) {
        if ((event->pos() - m_dragStartPos).manhattanLength() < 5) {
            int hitId = hitTestBody(event->pos());
            setSelectedBodyId(hitId);
            emit bodySelected(hitId);
        }
    }
    m_mouseDragging = false;
    m_draggingGizmo = false;
    update();
    QOpenGLWidget::mouseReleaseEvent(event);
}

void GL3DView::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        // Double-click centers m_target onto the clicked horizontal coordinate
        QVector3D worldPt = screenToWorldPlane(event->pos(), m_target.z());
        animateToCamera(m_pitch, m_azimuth, m_distance, &worldPt, 200);
        event->accept();
        return;
    }
    QOpenGLWidget::mouseDoubleClickEvent(event);
}

QVector3D GL3DView::screenToWorldPlane(const QPoint &pos, float planeZ) {
    float w = static_cast<float>(width());
    float h = static_cast<float>(height());
    if (w <= 0.0f || h <= 0.0f) return m_target;

    float ndcX = (2.0f * pos.x()) / w - 1.0f;
    float ndcY = 1.0f - (2.0f * pos.y()) / h;

    float effDist = m_distance / std::max(0.01f, m_zoom);
    float fov = 45.0f * 3.14159265f / 180.0f;
    float halfH = effDist * std::tan(fov * 0.5f);
    float halfW = halfH * (w / h);

    float pRad = (m_pitch - 90.0f) * 3.14159265f / 180.0f;
    float azRad = m_azimuth * 3.14159265f / 180.0f;
    float cosAz = std::cos(azRad), sinAz = std::sin(azRad);
    float cosP = std::cos(pRad), sinP = std::sin(pRad);

    QVector3D rightDir(cosAz, -sinAz, 0.0f);
    QVector3D upDir(-sinP * sinAz, -sinP * cosAz, cosP);
    QVector3D viewDir(-cosP * sinAz, -cosP * cosAz, -sinP);

    QVector3D eye = m_target - effDist * viewDir;
    QVector3D rayDir = (viewDir * effDist + ndcX * halfW * rightDir + ndcY * halfH * upDir).normalized();

    if (std::abs(rayDir.z()) > 1e-4f) {
        float t = (planeZ - eye.z()) / rayDir.z();
        return eye + t * rayDir;
    }
    return m_target;
}

void GL3DView::wheelEvent(QWheelEvent *event) {
    float numDegrees = event->angleDelta().y() / 8.0f;
    float numSteps = numDegrees / 15.0f;
    float factor = std::pow(1.15f, numSteps);

    // Zoom-to-cursor: smooth transition towards ray cast through mouse position
    QPoint mousePos = event->position().toPoint();
    QVector3D cursorWorld = screenToWorldPlane(mousePos, m_target.z());

    m_zoom *= factor;
    if (m_zoom < 0.01f) m_zoom = 0.01f;
    if (m_zoom > 100.0f) m_zoom = 100.0f;

    // Shift target slightly toward cursor point when zooming in
    if (factor > 1.0f) {
        float blend = std::clamp((factor - 1.0f) * 0.35f, 0.0f, 0.30f);
        m_target = m_target + blend * (cursorWorld - m_target);
    }

    emit cameraChanged();
    update();
    event->accept();
}

void GL3DView::keyPressEvent(QKeyEvent *event) {
    switch (event->key()) {
    case Qt::Key_E: setViewEast(); break;
    case Qt::Key_W: setViewWest(); break;
    case Qt::Key_N: setViewNorth(); break;
    case Qt::Key_S: setViewSouth(); break;
    case Qt::Key_M: setViewTopMap(); break;
    case Qt::Key_O:
    case Qt::Key_5:
        setOrthographic(!m_orthographic);
        break;
    case Qt::Key_Home:
    case Qt::Key_F:
        fitToView();
        break;
    case Qt::Key_Plus:
    case Qt::Key_Equal:
        setZScale(m_zScale * 1.25f);
        break;
    case Qt::Key_Minus:
        setZScale(m_zScale / 1.25f);
        break;
    default:
        QOpenGLWidget::keyPressEvent(event);
        break;
    }
}

void GL3DView::contextMenuEvent(QContextMenuEvent *event) {
    m_mouseDragging = false;
    m_draggingGizmo = false;
    showContextMenu(event->globalPos());
    event->accept();
}

void GL3DView::leaveEvent(QEvent *event) {
    if (QApplication::mouseButtons() == Qt::NoButton) {
        m_mouseDragging = false;
        m_draggingGizmo = false;
    }
    if (m_gizmoHoverAxis != -1) {
        m_gizmoHoverAxis = -1;
        update();
    }
    QOpenGLWidget::leaveEvent(event);
}

void GL3DView::focusOutEvent(QFocusEvent *event) {
    m_mouseDragging = false;
    m_draggingGizmo = false;
    QOpenGLWidget::focusOutEvent(event);
}

void GL3DView::showContextMenu(const QPoint &globalPos) {
    m_mouseDragging = false;
    m_draggingGizmo = false;

    QMenu menu(this);

    menu.addAction(tr("Reset Camera"), this, &GL3DView::resetCamera);
    menu.addAction(tr("Fit to View (F)"), this, &GL3DView::fitToView);

    auto *viewsMenu = menu.addMenu(tr("Standard Views"));
    viewsMenu->addAction(tr("Top Map View (M)"), this, &GL3DView::setViewTopMap);
    viewsMenu->addAction(tr("East View (E)"), this, &GL3DView::setViewEast);
    viewsMenu->addAction(tr("West View (W)"), this, &GL3DView::setViewWest);
    viewsMenu->addAction(tr("North View (N)"), this, &GL3DView::setViewNorth);
    viewsMenu->addAction(tr("South View (S)"), this, &GL3DView::setViewSouth);

    menu.addSeparator();

    auto *actOrtho = menu.addAction(tr("Orthographic Projection (O)"), this, [this](bool checked) {
        setOrthographic(checked);
    });
    actOrtho->setCheckable(true);
    actOrtho->setChecked(m_orthographic);

    auto *zMenu = menu.addMenu(tr("Vertical Exaggeration (Z-Scale)"));
    float scales[] = { 1.0f, 2.0f, 3.0f, 5.0f, 10.0f };
    for (float sc : scales) {
        QString title = QString("%1x").arg(sc, 0, 'f', (sc == static_cast<int>(sc) ? 0 : 1));
        auto *act = zMenu->addAction(title, this, [this, sc]() {
            setZScale(sc);
        });
        act->setCheckable(true);
        act->setChecked(std::abs(m_zScale - sc) < 0.05f);
    }

    auto *actFog = menu.addAction(tr("Depth Cueing / Atmospheric Haze"), this, [this](bool checked) {
        setShowDepthFog(checked);
    });
    actFog->setCheckable(true);
    actFog->setChecked(m_showDepthFog);

    menu.addSeparator();

    auto *actEdges = menu.addAction(tr("Show Feature Edges"), this, [this](bool checked) {
        setShowFeatureEdges(checked);
    });
    actEdges->setCheckable(true);
    actEdges->setChecked(m_showFeatureEdges);

    auto *actWire = menu.addAction(tr("Wireframe Mode"), this, [this](bool checked) {
        setWireframeMode(checked);
    });
    actWire->setCheckable(true);
    actWire->setChecked(m_wireframe);

    auto *actRelief = menu.addAction(tr("Show Relief Topography"), this, [this](bool checked) {
        setShowRelief(checked);
    });
    actRelief->setCheckable(true);
    actRelief->setChecked(m_showRelief);

    auto *reliefModeMenu = menu.addMenu(tr("Relief Display Mode"));
    auto *actSurf = reliefModeMenu->addAction(tr("Surface"), this, [this]() {
        setReliefMode(ReliefMode::Surface);
    });
    actSurf->setCheckable(true);
    actSurf->setChecked(m_reliefMode == ReliefMode::Surface);

    auto *actWireRelief = reliefModeMenu->addAction(tr("Wireframe"), this, [this]() {
        setReliefMode(ReliefMode::Wireframe);
    });
    actWireRelief->setCheckable(true);
    actWireRelief->setChecked(m_reliefMode == ReliefMode::Wireframe);

    auto *actProfile = menu.addAction(tr("Show Profile Slices"), this, [this](bool checked) {
        setShowProfilePlane(checked);
    });
    actProfile->setCheckable(true);
    actProfile->setChecked(m_showProfilePlane);

    auto *actTranspBodies = menu.addAction(tr("Transparent Bodies"), this, [this](bool checked) {
        setShowTransparentBodies(checked);
    });
    actTranspBodies->setCheckable(true);
    actTranspBodies->setChecked(m_transparentBodies);

    auto *actTranspProfiles = menu.addAction(tr("Transparent Profile / Section Slices"), this, [this](bool checked) {
        setShowTransparentProfilePlane(checked);
    });
    actTranspProfiles->setCheckable(true);
    actTranspProfiles->setChecked(m_transparentProfiles);

    auto *actBBox = menu.addAction(tr("Show Bounding Box"), this, [this](bool checked) {
        setShowBoundingBox(checked);
    });
    actBBox->setCheckable(true);
    actBBox->setChecked(m_showBoundingBox);

    auto *actCompass = menu.addAction(tr("Show Orientation Compass"), this, [this](bool checked) {
        setShowOrientationIndicator(checked);
    });
    actCompass->setCheckable(true);
    actCompass->setChecked(m_showOrientationIndicator);

    auto *actBottom = menu.addAction(tr("Show Bottom Base Slab"), this, [this](bool checked) {
        setShowBottom(checked);
    });
    actBottom->setCheckable(true);
    actBottom->setChecked(m_showBottom);

    auto *actOuter = menu.addAction(tr("Show Outer Buffer Facets"), this, [this](bool checked) {
        setShowOuterFacets(checked);
    });
    actOuter->setCheckable(true);
    actOuter->setChecked(m_showOuterFacets);

    menu.exec(globalPos);

    m_mouseDragging = false;
    m_draggingGizmo = false;
}

void GL3DView::setViewEast() {
    m_mouseDragging = false;
    m_draggingGizmo = false;
    if (m_camAnim) { m_camAnim->stop(); }
    setCameraAngles(0.0f, 90.0f);
}

void GL3DView::setViewWest() {
    m_mouseDragging = false;
    m_draggingGizmo = false;
    if (m_camAnim) { m_camAnim->stop(); }
    setCameraAngles(0.0f, -90.0f);
}

void GL3DView::setViewNorth() {
    m_mouseDragging = false;
    m_draggingGizmo = false;
    if (m_camAnim) { m_camAnim->stop(); }
    setCameraAngles(0.0f, 180.0f);
}

void GL3DView::setViewSouth() {
    m_mouseDragging = false;
    m_draggingGizmo = false;
    if (m_camAnim) { m_camAnim->stop(); }
    setCameraAngles(0.0f, 0.0f);
}

void GL3DView::setViewTopMap() {
    m_mouseDragging = false;
    m_draggingGizmo = false;
    if (m_camAnim) { m_camAnim->stop(); }
    setCameraAngles(89.9f, 0.0f);
}

void GL3DView::resetCamera() {
    m_mouseDragging = false;
    m_draggingGizmo = false;
    if (m_camAnim) { m_camAnim->stop(); }
    if (m_doc) {
        const auto &model = m_doc->model();
        double midX = (model.x_min_val() + model.x_max_val()) * 0.5;
        double midY = (model.y_min_val() + model.y_max_val()) * 0.5;
        double midZ = (model.z_min_val() + model.z_max_val()) * 0.5;
        m_target = QVector3D(static_cast<float>(midX), static_cast<float>(midY), static_cast<float>(midZ));
    }
    m_pitch = 30.0f;
    m_azimuth = -45.0f;
    m_panX = 0.0f;
    m_panY = 0.0f;
    m_zoom = 1.0f;
    emit cameraChanged();
    update();
}

void GL3DView::fitToView() {
    m_mouseDragging = false;
    m_draggingGizmo = false;
    if (m_camAnim) { m_camAnim->stop(); }
    if (m_doc) {
        const auto &model = m_doc->model();
        double midX = (model.x_min_val() + model.x_max_val()) * 0.5;
        double midY = (model.y_min_val() + model.y_max_val()) * 0.5;
        double midZ = (model.z_min_val() + model.z_max_val()) * 0.5;
        QVector3D targetPt(static_cast<float>(midX), static_cast<float>(midY), static_cast<float>(midZ));

        double maxDim = std::max({ model.x_max_val() - model.x_min_val(),
                                   model.y_max_val() - model.y_min_val(),
                                   (model.z_max_val() - model.z_min_val()) * std::max(1.0f, m_zScale) });
        float dist = static_cast<float>(maxDim * 1.8);
        animateToCamera(m_pitch, m_azimuth, dist, &targetPt);
    }
}

void GL3DView::setCameraAngles(float pitch, float azimuth) {
    m_mouseDragging = false;
    m_draggingGizmo = false;
    m_pitch = pitch;
    m_azimuth = azimuth;
    emit cameraChanged();
    update();
}

void GL3DView::setTarget(const QVector3D &target) {
    m_target = target;
    m_targetInitialized = true;
    emit cameraChanged();
    update();
}

void GL3DView::setCameraDistance(float dist) {
    m_distance = std::max(10.0f, dist);
    emit cameraChanged();
    update();
}

void GL3DView::setZoom(float zoom) {
    m_zoom = std::clamp(zoom, 0.01f, 100.0f);
    emit cameraChanged();
    update();
}

void GL3DView::setZScale(float zScale) {
    m_zScale = std::clamp(zScale, 0.1f, 30.0f);
    emit zScaleChanged(m_zScale);
    update();
}

void GL3DView::setOrthographic(bool ortho) {
    m_orthographic = ortho;
    emit projectionChanged(m_orthographic);
    update();
}

void GL3DView::setShowDepthFog(bool fog) {
    m_showDepthFog = fog;
    update();
}

void GL3DView::setWireframeMode(bool wireframe) {
    m_wireframe = wireframe;
    update();
}

void GL3DView::setShowRelief(bool show) {
    m_showRelief = show;
    update();
}

void GL3DView::setReliefMode(ReliefMode mode) {
    if (m_reliefMode != mode) {
        m_reliefMode = mode;
        emit reliefModeChanged(mode);
        update();
    }
}

void GL3DView::setShowProfilePlane(bool show) {
    m_showProfilePlane = show;
    update();
}

void GL3DView::setShowFeatureEdges(bool show) {
    m_showFeatureEdges = show;
    update();
}

void GL3DView::setShowBottom(bool show) {
    if (m_showBottom != show) {
        m_showBottom = show;
        m_meshDirty = true;
        update();
    }
}

void GL3DView::setShowOuterFacets(bool show) {
    m_showOuterFacets = show;
    m_meshDirty = true;
    update();
}

void GL3DView::setShowBoundingBox(bool show) {
    m_showBoundingBox = show;
    update();
}

void GL3DView::setShowOrientationIndicator(bool show) {
    m_showOrientationIndicator = show;
    update();
}

void GL3DView::setShowTransparentBodies(bool enable) {
    if (m_transparentBodies != enable) {
        m_transparentBodies = enable;
        m_meshDirty = true;
        update();
    }
}

void GL3DView::setShowTransparentProfilePlane(bool enable) {
    if (m_transparentProfiles != enable) {
        m_transparentProfiles = enable;
        update();
    }
}

} // namespace mod3d_gui

