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
#include <string>

#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

namespace mod3d_gui {

namespace {

void drawStrokeChar(char c, float cx, float cy, float size) {
    float h = size * 0.5f;
    float w = size * 0.35f;
    float x0 = cx - w;
    float x1 = cx + w;
    float y0 = cy - h;
    float y1 = cy + h;
    float ym = cy;
    float xm = cx;

    switch (c) {
    case 'N':
        glVertex2f(x0, y0); glVertex2f(x0, y1);
        glVertex2f(x0, y1); glVertex2f(x1, y0);
        glVertex2f(x1, y0); glVertex2f(x1, y1);
        break;
    case 'S':
        glVertex2f(x1, y1); glVertex2f(x0, y1);
        glVertex2f(x0, y1); glVertex2f(x0, ym);
        glVertex2f(x0, ym); glVertex2f(x1, ym);
        glVertex2f(x1, ym); glVertex2f(x1, y0);
        glVertex2f(x1, y0); glVertex2f(x0, y0);
        break;
    case 'E':
        glVertex2f(x1, y1); glVertex2f(x0, y1);
        glVertex2f(x0, y1); glVertex2f(x0, y0);
        glVertex2f(x0, y0); glVertex2f(x1, y0);
        glVertex2f(x0, ym); glVertex2f(xm + w * 0.2f, ym);
        break;
    case 'W':
        glVertex2f(x0, y1); glVertex2f(x0 + w * 0.4f, y0);
        glVertex2f(x0 + w * 0.4f, y0); glVertex2f(xm, ym);
        glVertex2f(xm, ym); glVertex2f(x1 - w * 0.4f, y0);
        glVertex2f(x1 - w * 0.4f, y0); glVertex2f(x1, y1);
        break;
    case 'U':
        glVertex2f(x0, y1); glVertex2f(x0, y0);
        glVertex2f(x0, y0); glVertex2f(x1, y0);
        glVertex2f(x1, y0); glVertex2f(x1, y1);
        break;
    case 'P':
        glVertex2f(x0, y0); glVertex2f(x0, y1);
        glVertex2f(x0, y1); glVertex2f(x1, y1);
        glVertex2f(x1, y1); glVertex2f(x1, ym);
        glVertex2f(x1, ym); glVertex2f(x0, ym);
        break;
    case 'O':
        glVertex2f(x0, y0); glVertex2f(x0, y1);
        glVertex2f(x0, y1); glVertex2f(x1, y1);
        glVertex2f(x1, y1); glVertex2f(x1, y0);
        glVertex2f(x1, y0); glVertex2f(x0, y0);
        break;
    case 'R':
        glVertex2f(x0, y0); glVertex2f(x0, y1);
        glVertex2f(x0, y1); glVertex2f(x1, y1);
        glVertex2f(x1, y1); glVertex2f(x1, ym);
        glVertex2f(x1, ym); glVertex2f(x0, ym);
        glVertex2f(xm, ym); glVertex2f(x1, y0);
        break;
    case 'T':
        glVertex2f(x0, y1); glVertex2f(x1, y1);
        glVertex2f(xm, y1); glVertex2f(xm, y0);
        break;
    case 'H':
        glVertex2f(x0, y0); glVertex2f(x0, y1);
        glVertex2f(x1, y0); glVertex2f(x1, y1);
        glVertex2f(x0, ym); glVertex2f(x1, ym);
        break;
    case 'A':
        glVertex2f(x0, y0); glVertex2f(xm, y1);
        glVertex2f(xm, y1); glVertex2f(x1, y0);
        glVertex2f(x0 + w * 0.35f, ym - h * 0.15f); glVertex2f(x1 - w * 0.35f, ym - h * 0.15f);
        break;
    case 'Z':
        glVertex2f(x0, y1); glVertex2f(x1, y1);
        glVertex2f(x1, y1); glVertex2f(x0, y0);
        glVertex2f(x0, y0); glVertex2f(x1, y0);
        break;
    case 'B':
        glVertex2f(x0, y0); glVertex2f(x0, y1);
        glVertex2f(x0, y1); glVertex2f(x1 - w * 0.2f, y1);
        glVertex2f(x1 - w * 0.2f, y1); glVertex2f(x1, ym + h * 0.5f);
        glVertex2f(x1, ym + h * 0.5f); glVertex2f(x0, ym);
        glVertex2f(x0, ym); glVertex2f(x1 - w * 0.2f, ym);
        glVertex2f(x1 - w * 0.2f, ym); glVertex2f(x1, ym - h * 0.5f);
        glVertex2f(x1, ym - h * 0.5f); glVertex2f(x0, y0);
        break;
    case '0':
        glVertex2f(x0, y0); glVertex2f(x1, y0);
        glVertex2f(x1, y0); glVertex2f(x1, y1);
        glVertex2f(x1, y1); glVertex2f(x0, y1);
        glVertex2f(x0, y1); glVertex2f(x0, y0);
        break;
    case '1':
        glVertex2f(xm, y0); glVertex2f(xm, y1);
        glVertex2f(x0, y1 - h * 0.25f); glVertex2f(xm, y1);
        glVertex2f(x0 + w * 0.2f, y0); glVertex2f(x1 - w * 0.2f, y0);
        break;
    case '2':
        glVertex2f(x0, y1); glVertex2f(x1, y1);
        glVertex2f(x1, y1); glVertex2f(x1, ym);
        glVertex2f(x1, ym); glVertex2f(x0, y0);
        glVertex2f(x0, y0); glVertex2f(x1, y0);
        break;
    case '3':
        glVertex2f(x0, y1); glVertex2f(x1, y1);
        glVertex2f(x1, y1); glVertex2f(x1, ym);
        glVertex2f(x0 + w * 0.25f, ym); glVertex2f(x1, ym);
        glVertex2f(x1, ym); glVertex2f(x1, y0);
        glVertex2f(x1, y0); glVertex2f(x0, y0);
        break;
    case '4':
        glVertex2f(x0, y1); glVertex2f(x0, ym);
        glVertex2f(x0, ym); glVertex2f(x1, ym);
        glVertex2f(x1 - w * 0.2f, y1); glVertex2f(x1 - w * 0.2f, y0);
        break;
    case '5':
        glVertex2f(x1, y1); glVertex2f(x0, y1);
        glVertex2f(x0, y1); glVertex2f(x0, ym);
        glVertex2f(x0, ym); glVertex2f(x1, ym);
        glVertex2f(x1, ym); glVertex2f(x1, y0);
        glVertex2f(x1, y0); glVertex2f(x0, y0);
        break;
    case '6':
        glVertex2f(x1, y1); glVertex2f(x0, y1);
        glVertex2f(x0, y1); glVertex2f(x0, y0);
        glVertex2f(x0, y0); glVertex2f(x1, y0);
        glVertex2f(x1, y0); glVertex2f(x1, ym);
        glVertex2f(x1, ym); glVertex2f(x0, ym);
        break;
    case '7':
        glVertex2f(x0, y1); glVertex2f(x1, y1);
        glVertex2f(x1, y1); glVertex2f(x0 + w * 0.25f, y0);
        break;
    case '8':
        glVertex2f(x0, y0); glVertex2f(x1, y0);
        glVertex2f(x1, y0); glVertex2f(x1, y1);
        glVertex2f(x1, y1); glVertex2f(x0, y1);
        glVertex2f(x0, y1); glVertex2f(x0, y0);
        glVertex2f(x0, ym); glVertex2f(x1, ym);
        break;
    case '9':
        glVertex2f(x0, ym); glVertex2f(x1, ym);
        glVertex2f(x0, ym); glVertex2f(x0, y1);
        glVertex2f(x0, y1); glVertex2f(x1, y1);
        glVertex2f(x1, y1); glVertex2f(x1, y0);
        glVertex2f(x1, y0); glVertex2f(x0, y0);
        break;
    case '-':
        glVertex2f(x0, ym); glVertex2f(x1, ym);
        break;
    case '+':
        glVertex2f(x0, ym); glVertex2f(x1, ym);
        glVertex2f(xm, y0 + h * 0.15f); glVertex2f(xm, y1 - h * 0.15f);
        break;
    case '.':
        glVertex2f(xm - w * 0.2f, y0); glVertex2f(xm + w * 0.2f, y0);
        break;
    case '(':
        glVertex2f(x1, y1); glVertex2f(x0, ym);
        glVertex2f(x0, ym); glVertex2f(x1, y0);
        break;
    case ')':
        glVertex2f(x0, y1); glVertex2f(x1, ym);
        glVertex2f(x1, ym); glVertex2f(x0, y0);
        break;
    case 'm':
        glVertex2f(x0, y0); glVertex2f(x0, ym + h * 0.1f);
        glVertex2f(x0, ym + h * 0.1f); glVertex2f(xm, ym + h * 0.1f);
        glVertex2f(xm, ym + h * 0.1f); glVertex2f(xm, y0);
        glVertex2f(xm, ym + h * 0.1f); glVertex2f(x1, ym + h * 0.1f);
        glVertex2f(x1, ym + h * 0.1f); glVertex2f(x1, y0);
        break;
    case 'k':
        glVertex2f(x0, y0); glVertex2f(x0, y1);
        glVertex2f(x0, ym); glVertex2f(x1, y1);
        glVertex2f(x0, ym); glVertex2f(x1, y0);
        break;
    case ' ':
        break;
    default:
        break;
    }
}

void drawStrokeString(const std::string &str, float size, float spacing = 0.85f) {
    if (str.empty()) return;
    float step = size * spacing;
    float totalW = static_cast<float>(str.size() - 1) * step;
    float startX = -totalW * 0.5f;

    glBegin(GL_LINES);
    for (size_t i = 0; i < str.size(); ++i) {
        float cx = startX + static_cast<float>(i) * step;
        drawStrokeChar(str[i], cx, 0.0f, size);
    }
    glEnd();
}

void drawBillboardLabel(const std::string &text, float x, float y, float z,
                        float r, float g, float b, float size,
                        float pitch, float azimuth) {
    glPushMatrix();
    glTranslatef(x, y, z);
    // Invert the camera rotation so label remains strictly parallel to screen
    glRotatef(-azimuth, 0.0f, 0.0f, 1.0f);
    glRotatef(-(pitch - 90.0f), 1.0f, 0.0f, 0.0f);

    // 1. Dark outline / drop shadow for legibility
    glColor4f(0.06f, 0.07f, 0.10f, 0.85f);
    glLineWidth(2.8f);
    drawStrokeString(text, size);

    // 2. Foreground core
    glColor4f(r, g, b, 1.0f);
    glLineWidth(1.6f);
    drawStrokeString(text, size);

    glPopMatrix();
}

void drawArrowCone(float tipX, float tipY, float tipZ,
                   float dirX, float dirY, float dirZ,
                   float length, float radius, int slices = 12) {
    float len = std::sqrt(dirX * dirX + dirY * dirY + dirZ * dirZ);
    if (len < 1e-6f) return;
    dirX /= len; dirY /= len; dirZ /= len;

    float baseX = tipX - dirX * length;
    float baseY = tipY - dirY * length;
    float baseZ = tipZ - dirZ * length;

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

    // Cone sides
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(tipX, tipY, tipZ);
    for (int i = 0; i <= slices; ++i) {
        float angle = 2.0f * 3.14159265f * static_cast<float>(i) / static_cast<float>(slices);
        float ca = std::cos(angle) * radius;
        float sa = std::sin(angle) * radius;
        glVertex3f(baseX + ca * ux + sa * vx,
                   baseY + ca * uy + sa * vy,
                   baseZ + ca * uz + sa * vz);
    }
    glEnd();

    // Base cap
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(baseX, baseY, baseZ);
    for (int i = slices; i >= 0; --i) {
        float angle = 2.0f * 3.14159265f * static_cast<float>(i) / static_cast<float>(slices);
        float ca = std::cos(angle) * radius;
        float sa = std::sin(angle) * radius;
        glVertex3f(baseX + ca * ux + sa * vx,
                   baseY + ca * uy + sa * vy,
                   baseZ + ca * uz + sa * vz);
    }
    glEnd();
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
        delete m_camAnim;
    }
    delete m_shaderProg;
    m_shaderProg = nullptr;
}

void GL3DView::setSelectedBodyId(int bodyId) {
    if (m_selectedBodyId == bodyId) return;
    m_selectedBodyId = bodyId;
    update();
}

int GL3DView::hitTestBody(const QPoint &pos) {
    if (!m_doc) return -1;
    if (m_meshDirty || m_bodyMeshes.empty()) {
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

    if (isValid() && context()) {
        glMatrixMode(GL_PROJECTION);
        glLoadMatrixf(m_projMatrix.constData());

        glMatrixMode(GL_MODELVIEW);
        glLoadMatrixf(m_mvMatrix.constData());
    }
}

void GL3DView::initShaders() {
    if (m_shadersInitialized) return;
    if (!isValid() || !context()) return;

    delete m_shaderProg;
    m_shaderProg = new QOpenGLShaderProgram(this);

    const char *vsrc =
        "#version 120\n"
        "uniform mat4 u_mvpMatrix;\n"
        "uniform mat4 u_mvMatrix;\n"
        "uniform mat3 u_normalMatrix;\n"
        "varying vec3 v_normal;\n"
        "varying vec3 v_eyePos;\n"
        "varying vec4 v_color;\n"
        "void main() {\n"
        "    vec4 eyePos4 = u_mvMatrix * gl_Vertex;\n"
        "    v_eyePos = eyePos4.xyz;\n"
        "    v_normal = normalize(u_normalMatrix * gl_Normal);\n"
        "    v_color = gl_Color;\n"
        "    gl_Position = u_mvpMatrix * gl_Vertex;\n"
        "}\n";

    const char *fsrc =
        "#version 120\n"
        "varying vec3 v_normal;\n"
        "varying vec3 v_eyePos;\n"
        "varying vec4 v_color;\n"
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
        "    gl_FragColor = vec4(lit, alpha);\n"
        "}\n";

    if (!m_shaderProg->addShaderFromSourceCode(QOpenGLShader::Vertex, vsrc)) {
        delete m_shaderProg;
        m_shaderProg = nullptr;
        m_shadersInitialized = false;
        return;
    }
    if (!m_shaderProg->addShaderFromSourceCode(QOpenGLShader::Fragment, fsrc)) {
        delete m_shaderProg;
        m_shaderProg = nullptr;
        m_shadersInitialized = false;
        return;
    }
    if (!m_shaderProg->link()) {
        delete m_shaderProg;
        m_shaderProg = nullptr;
        m_shadersInitialized = false;
        return;
    }

    m_shadersInitialized = true;
}

void GL3DView::initializeGL() {
    if (!isValid() || !context()) return;
    initializeOpenGLFunctions();
    initShaders();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_NORMALIZE);

    // Alpha blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Two-sided lighting & materials
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);

    GLfloat lmodel_ambient[] = { 0.28f, 0.28f, 0.30f, 1.0f };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lmodel_ambient);

    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    // Subtle specular material sheen
    GLfloat matSpec[] = { 0.35f, 0.35f, 0.35f, 1.0f };
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, matSpec);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 32.0f);

    // Clean modern studio neutral background
    glClearColor(0.92f, 0.93f, 0.95f, 1.0f);

    // Optional modern depth fog for visual depth comprehension
    if (m_showDepthFog) {
        glFogi(GL_FOG_MODE, GL_LINEAR);
        GLfloat fogColor[] = { 0.92f, 0.93f, 0.95f, 1.0f };
        glFogfv(GL_FOG_COLOR, fogColor);
        glHint(GL_FOG_HINT, GL_NICEST);
    }
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

    // Directional Key Light & Soft Fill Light in Eye Coordinates (headlight / sun)
    GLfloat light0Pos[] = { 0.5f, 0.7f, 1.0f, 0.0f }; // Directional key light from upper-right-front
    GLfloat light0Amb[] = { 0.18f, 0.18f, 0.20f, 1.0f };
    GLfloat light0Diff[] = { 0.85f, 0.85f, 0.85f, 1.0f };
    GLfloat light0Spec[] = { 0.40f, 0.40f, 0.40f, 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, light0Pos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, light0Amb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light0Diff);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light0Spec);

    GLfloat light1Pos[] = { -0.5f, -0.6f, 0.4f, 0.0f }; // Soft fill light from lower-left-front
    GLfloat light1Amb[] = { 0.05f, 0.05f, 0.05f, 1.0f };
    GLfloat light1Diff[] = { 0.30f, 0.30f, 0.35f, 1.0f };
    glLightfv(GL_LIGHT1, GL_POSITION, light1Pos);
    glLightfv(GL_LIGHT1, GL_AMBIENT, light1Amb);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, light1Diff);

    // Setup Depth Fog parameters
    if (m_showDepthFog) {
        glEnable(GL_FOG);
        float effDist = m_distance / std::max(0.01f, m_zoom);
        glFogf(GL_FOG_START, effDist * 0.75f);
        glFogf(GL_FOG_END, effDist * 2.2f);
    } else {
        glDisable(GL_FOG);
    }

    // Draw elements
    if (m_showBoundingBox) {
        drawBoundingBox();
    }

    if (m_showRelief) {
        drawRelief();
    }

    drawBodies();

    if (m_showProfilePlane) {
        drawProfilePlane();
    }

    if (m_mouseDragging && (m_dragButton == Qt::LeftButton) && !m_draggingGizmo) {
        drawPivotIndicator();
    }

    glDisable(GL_FOG);

    if (m_showOrientationIndicator) {
        drawOrientationHud();
    }
}

void GL3DView::paintEvent(QPaintEvent *event) {
    if (isValid() && context()) {
        QOpenGLWidget::paintEvent(event);
        return;
    }
    QPainter painter(this);
    paintSoftware(painter);
}

void GL3DView::paintSoftware(QPainter &painter) {
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

    // 3. Active Profile Plane
    if (m_showProfilePlane) {
        int pIdx = m_doc->profileIndex();
        double z0 = model.z_min_val();
        double z1 = model.z_max_val();
        QPolygonF poly;
        if (m_doc->profileOrientation() == ProfileOrientation::EastWest) {
            double y = model.yd_val(pIdx + 1);
            poly << project(model.x_min_val(), y, z0).first
                 << project(model.x_max_val(), y, z0).first
                 << project(model.x_max_val(), y, z1).first
                 << project(model.x_min_val(), y, z1).first;
        } else {
            double x = model.xd_val(pIdx + 1);
            poly << project(x, model.y_min_val(), z0).first
                 << project(x, model.y_max_val(), z0).first
                 << project(x, model.y_max_val(), z1).first
                 << project(x, model.y_min_val(), z1).first;
        }
        if (m_transparentProfiles) {
            painter.setBrush(QColor(245, 158, 11, 45));
        } else {
            painter.setBrush(Qt::NoBrush);
        }
        painter.setPen(QPen(QColor(217, 119, 6, 200), 1.5, Qt::DashLine));
        painter.drawPolygon(poly);
    }

    // 4. 3D Polyhedral Bodies (Depth-Sorted Back-to-Front)
    if (m_meshDirty || m_bodyMeshes.empty()) {
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

    // 5. Orientation Indicator HUD (Compass Tripod in bottom-left corner)
    if (m_showOrientationIndicator) {
        double cx = 58.0;
        double cy = h - 58.0;
        double rHud = 44.0;

        painter.setPen(QPen(QColor(203, 213, 225), 1.2));
        painter.setBrush(QColor(255, 255, 255, 225));
        painter.drawRoundedRect(QRectF(cx - rHud, cy - rHud, rHud * 2.0, rHud * 2.0), 12.0, 12.0);

        auto projectAxis = [&](double ax, double ay, double az) {
            double azRad = m_azimuth * 3.14159265358979323846 / 180.0;
            double rx = ax * std::cos(azRad) - ay * std::sin(azRad);
            double ry = ax * std::sin(azRad) + ay * std::cos(azRad);
            double rz = az;

            double pitchRad = (m_pitch - 90.0) * 3.14159265358979323846 / 180.0;
            double px = rx;
            double py = ry * std::cos(pitchRad) - rz * std::sin(pitchRad);
            return QPointF(cx + px * 26.0, cy - py * 26.0);
        };

        QPointF pCenter(cx, cy);
        QPointF pX = projectAxis(1.0, 0.0, 0.0);
        QPointF pY = projectAxis(0.0, 1.0, 0.0);
        QPointF pZ = projectAxis(0.0, 0.0, 1.0);

        // X axis - East (Red)
        painter.setPen(QPen(QColor(220, 38, 38), 2.2));
        painter.drawLine(pCenter, pX);
        painter.drawText(pX + QPointF(2, 4), "E");

        // Y axis - North (Green)
        painter.setPen(QPen(QColor(22, 163, 74), 2.2));
        painter.drawLine(pCenter, pY);
        painter.drawText(pY + QPointF(2, 4), "N");

        // Z axis - Up (Blue)
        painter.setPen(QPen(QColor(37, 99, 235), 2.2));
        painter.drawLine(pCenter, pZ);
        painter.drawText(pZ + QPointF(2, 4), "Z");
    }

    painter.restore();
}

void GL3DView::drawPivotIndicator() {
    // Draws a subtle visual reticle at m_target during mouse orbit
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glPushMatrix();
    glTranslatef(m_target.x(), m_target.y(), m_target.z());
    glRotatef(-m_azimuth, 0.0f, 0.0f, 1.0f);
    glRotatef(-(m_pitch - 90.0f), 1.0f, 0.0f, 0.0f);

    float effDist = m_distance / std::max(0.01f, m_zoom);
    float size = effDist * 0.018f;

    glColor4f(0.96f, 0.55f, 0.12f, 0.85f);
    glLineWidth(2.0f);

    // Reticle circle
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 24; ++i) {
        float angle = 2.0f * 3.14159265f * static_cast<float>(i) / 24.0f;
        glVertex2f(std::cos(angle) * size, std::sin(angle) * size);
    }
    glEnd();

    // Crosshairs
    glBegin(GL_LINES);
    glVertex2f(-size * 1.5f, 0.0f); glVertex2f(-size * 0.5f, 0.0f);
    glVertex2f(size * 0.5f, 0.0f);  glVertex2f(size * 1.5f, 0.0f);
    glVertex2f(0.0f, -size * 1.5f); glVertex2f(0.0f, -size * 0.5f);
    glVertex2f(0.0f, size * 0.5f);  glVertex2f(0.0f, size * 1.5f);
    glEnd();

    glPopMatrix();
    glEnable(GL_LIGHTING);
}

void GL3DView::drawBoundingBox() {
    glDisable(GL_LIGHTING);
    glColor3f(0.55f, 0.55f, 0.60f);
    glLineWidth(1.0f);

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

    glBegin(GL_LINES);
    // Bottom
    glVertex3f(x0, y0, z0); glVertex3f(x1, y0, z0);
    glVertex3f(x1, y0, z0); glVertex3f(x1, y1, z0);
    glVertex3f(x1, y1, z0); glVertex3f(x0, y1, z0);
    glVertex3f(x0, y1, z0); glVertex3f(x0, y0, z0);
    // Top
    glVertex3f(x0, y0, z1); glVertex3f(x1, y0, z1);
    glVertex3f(x1, y0, z1); glVertex3f(x1, y1, z1);
    glVertex3f(x1, y1, z1); glVertex3f(x0, y1, z1);
    glVertex3f(x0, y1, z1); glVertex3f(x0, y0, z1);
    // Pillars
    glVertex3f(x0, y0, z0); glVertex3f(x0, y0, z1);
    glVertex3f(x1, y0, z0); glVertex3f(x1, y0, z1);
    glVertex3f(x1, y1, z0); glVertex3f(x1, y1, z1);
    glVertex3f(x0, y1, z0); glVertex3f(x0, y1, z1);
    glEnd();

    float dx = x1 - x0;
    float dy = y1 - y0;
    float diagXY = std::sqrt(dx * dx + dy * dy);
    if (diagXY < 1.0f) diagXY = 1000.0f;

    // Coordinate orientation axis tripod at origin corner (x0, y0, z0)
    float axisLen = diagXY * 0.055f;
    float coneLen = axisLen * 0.22f;
    float coneRadius = axisLen * 0.065f;
    float labelSize = axisLen * 0.22f;

    // East (+X) Axis - Red
    glColor3f(0.92f, 0.22f, 0.22f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glVertex3f(x0, y0, z0); glVertex3f(x0 + axisLen, y0, z0);
    glEnd();
    drawArrowCone(x0 + axisLen, y0, z0, 1.0f, 0.0f, 0.0f, coneLen, coneRadius);
    drawBillboardLabel("E", x0 + axisLen + coneLen * 0.9f, y0, z0,
                       0.95f, 0.25f, 0.25f, labelSize, m_pitch, m_azimuth);

    // North (+Y) Axis - Green
    glColor3f(0.18f, 0.80f, 0.32f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glVertex3f(x0, y0, z0); glVertex3f(x0, y0 + axisLen, z0);
    glEnd();
    drawArrowCone(x0, y0 + axisLen, z0, 0.0f, 1.0f, 0.0f, coneLen, coneRadius);
    drawBillboardLabel("N", x0, y0 + axisLen + coneLen * 0.9f, z0,
                       0.20f, 0.85f, 0.35f, labelSize, m_pitch, m_azimuth);

    // Compass cardinal direction labels on outer bounding box top edges
    float edgeLabelSize = diagXY * 0.016f;
    float midX = (x0 + x1) * 0.5f;
    float midY = (y0 + y1) * 0.5f;
    drawBillboardLabel("NORTH", midX, y1, z1, 0.18f, 0.80f, 0.32f, edgeLabelSize, m_pitch, m_azimuth);
    drawBillboardLabel("SOUTH", midX, y0, z1, 0.55f, 0.60f, 0.68f, edgeLabelSize, m_pitch, m_azimuth);
    drawBillboardLabel("EAST",  x1, midY, z1, 0.92f, 0.25f, 0.25f, edgeLabelSize, m_pitch, m_azimuth);
    drawBillboardLabel("WEST",  x0, midY, z1, 0.55f, 0.60f, 0.68f, edgeLabelSize, m_pitch, m_azimuth);

    // Elevation / Depth Ticks on Vertical Corner Post (Southwest pillar)
    float zSpan = z1Raw - z0Raw;
    if (zSpan > 0.0f) {
        // Calculate standard "nice step" (multiples of 1, 2, or 5 * 10^k)
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

        // Size the tick marks and labels appropriately (subtle, non-intrusive)
        float tickLen = diagXY * 0.012f;
        float scaledSpacing = static_cast<float>(step) * m_zScale;
        float tickFontSize = std::min(diagXY * 0.0075f, scaledSpacing * 0.25f);
        tickFontSize = std::max(tickFontSize, diagXY * 0.0035f);

        // First tick at or above z0Raw
        double startZ = std::ceil(static_cast<double>(z0Raw) / step) * step;

        glLineWidth(1.2f);
        for (double zVal = startZ; zVal <= static_cast<double>(z1Raw) + 1e-4; zVal += step) {
            float zPos = zMid + (static_cast<float>(zVal) - zMid) * m_zScale;

            // Draw clean tick notch pointing outward diagonally (-X, -Y) from corner
            glColor3f(0.65f, 0.70f, 0.80f);
            glBegin(GL_LINES);
            glVertex3f(x0, y0, zPos);
            glVertex3f(x0 - tickLen * 0.7071f, y0 - tickLen * 0.7071f, zPos);
            glEnd();

            int zInt = static_cast<int>(std::round(zVal));
            std::string tickStr = std::to_string(zInt) + "m";

            float labelDist = tickLen * 0.7071f + tickFontSize * static_cast<float>(tickStr.size()) * 0.42f + diagXY * 0.003f;
            drawBillboardLabel(tickStr,
                               x0 - labelDist * 0.7071f,
                               y0 - labelDist * 0.7071f,
                               zPos,
                               0.65f, 0.72f, 0.82f,
                               tickFontSize,
                               m_pitch, m_azimuth);
        }

        // Draw axis title "Z (m)" near the top of the pillar
        float topZPos = z1 + tickFontSize * 1.5f;
        drawBillboardLabel("Z (m)",
                           x0 - tickLen * 0.7071f,
                           y0 - tickLen * 0.7071f,
                           topZPos,
                           0.30f, 0.65f, 1.0f,
                           tickFontSize * 1.15f,
                           m_pitch, m_azimuth);
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

        glLineWidth(1.2f);
        for (double xVal = startX; xVal <= static_cast<double>(x1) + 1e-4; xVal += step) {
            float xPos = static_cast<float>(xVal);
            glColor3f(0.65f, 0.70f, 0.80f);
            glBegin(GL_LINES);
            glVertex3f(xPos, y0, z0);
            glVertex3f(xPos, y0 - tickLen, z0);
            glEnd();

            int xInt = static_cast<int>(std::round(xVal));
            std::string tickStr = std::to_string(xInt) + "m";
            drawBillboardLabel(tickStr, xPos, y0 - tickLen * 1.8f, z0,
                               0.65f, 0.72f, 0.82f, tickFontSize, m_pitch, m_azimuth);
        }
        drawBillboardLabel("X (m)", x1 + tickLen * 2.0f, y0 - tickLen * 1.8f, z0,
                           0.92f, 0.25f, 0.25f, tickFontSize * 1.15f, m_pitch, m_azimuth);
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

        glLineWidth(1.2f);
        for (double yVal = startY; yVal <= static_cast<double>(y1) + 1e-4; yVal += step) {
            float yPos = static_cast<float>(yVal);
            glColor3f(0.65f, 0.70f, 0.80f);
            glBegin(GL_LINES);
            glVertex3f(x0, yPos, z0);
            glVertex3f(x0 - tickLen, yPos, z0);
            glEnd();

            int yInt = static_cast<int>(std::round(yVal));
            std::string tickStr = std::to_string(yInt) + "m";
            drawBillboardLabel(tickStr, x0 - tickLen * 1.8f, yPos, z0,
                               0.65f, 0.72f, 0.82f, tickFontSize, m_pitch, m_azimuth);
        }
        drawBillboardLabel("Y (m)", x0 - tickLen * 1.8f, y1 + tickLen * 2.0f, z0,
                           0.20f, 0.85f, 0.35f, tickFontSize * 1.15f, m_pitch, m_azimuth);
    }

    glEnable(GL_LIGHTING);
}

void GL3DView::drawRelief() {
    glDisable(GL_LIGHTING);
    glColor4f(0.35f, 0.65f, 0.40f, 0.40f);
    glLineWidth(1.0f);

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

    // Wireframe terrain mesh conforming to the actual model surface elevations at index 0
    glBegin(GL_LINES);
    // Grid lines along X (running West-East at each core row)
    for (int r = 1; r <= rMax; ++r) {
        float y = static_cast<float>(m.yd_val(r));
        for (int c = 1; c < cMax; ++c) {
            float xA = static_cast<float>(m.xd_val(c));
            float xB = static_cast<float>(m.xd_val(c + 1));
            float zA = scaleZ(m.z(r, c, 0));
            float zB = scaleZ(m.z(r, c + 1, 0));
            glVertex3f(xA, y, zA);
            glVertex3f(xB, y, zB);
        }
    }
    // Grid lines along Y (running South-North at each core col)
    for (int c = 1; c <= cMax; ++c) {
        float x = static_cast<float>(m.xd_val(c));
        for (int r = 1; r < rMax; ++r) {
            float yA = static_cast<float>(m.yd_val(r));
            float yB = static_cast<float>(m.yd_val(r + 1));
            float zA = scaleZ(m.z(r, c, 0));
            float zB = scaleZ(m.z(r + 1, c, 0));
            glVertex3f(x, yA, zA);
            glVertex3f(x, yB, zB);
        }
    }
    glEnd();

    glEnable(GL_LIGHTING);
}

void GL3DView::drawBodies() {
    if (m_meshDirty) {
        m_bodyMeshes = m_doc->model().generate_body_meshes(-1, m_showOuterFacets);
        m_meshDirty = false;
    }

    if (m_bodyMeshes.empty()) return;

    double dBot = m_doc->model().z_min_val();
    float zMid = static_cast<float>((m_doc->model().z_min_val() + m_doc->model().z_max_val()) * 0.5);

    auto scaleZ = [this, zMid](double zRaw) -> float {
        return zMid + static_cast<float>(zRaw - zMid) * m_zScale;
    };

    // 1. OPAQUE BODIES PASS
    glEnable(GL_LIGHTING);
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

    bool useShader = (m_shadersInitialized && m_shaderProg != nullptr && !m_wireframe);
    if (useShader) {
        m_shaderProg->bind();
        m_shaderProg->setUniformValue("u_mvpMatrix", m_projMatrix * m_mvMatrix);
        m_shaderProg->setUniformValue("u_mvMatrix", m_mvMatrix);
        m_shaderProg->setUniformValue("u_normalMatrix", m_mvMatrix.normalMatrix());
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
    }

    for (const auto &mesh : m_bodyMeshes) {
        if (!mesh.isVisible || (m_transparentBodies && mesh.isTransparent)) continue;

        if (useShader) {
            m_shaderProg->setUniformValue("u_highlight", (mesh.bodyId == m_selectedBodyId));
        }

        auto bc = mesh.color;
        glColor4f(bc.r / 255.0f, bc.g / 255.0f, bc.b / 255.0f, 1.0f);

        glBegin(GL_TRIANGLES);
        for (const auto &fct : mesh.facets) {
            if (!m_showBottom &&
                fct[0].z <= dBot + 1e-3 &&
                fct[1].z <= dBot + 1e-3 &&
                fct[2].z <= dBot + 1e-3) {
                continue;
            }
            auto norm = fct.normal();
            norm.z /= std::max(0.1f, m_zScale);
            double nLen = std::sqrt(norm.x*norm.x + norm.y*norm.y + norm.z*norm.z);
            if (nLen > 1e-6) { norm.x /= nLen; norm.y /= nLen; norm.z /= nLen; }

            glNormal3d(norm.x, norm.y, norm.z);
            glVertex3f(static_cast<float>(fct[0].x), static_cast<float>(fct[0].y), scaleZ(fct[0].z));
            glVertex3f(static_cast<float>(fct[1].x), static_cast<float>(fct[1].y), scaleZ(fct[1].z));
            glVertex3f(static_cast<float>(fct[2].x), static_cast<float>(fct[2].y), scaleZ(fct[2].z));
        }
        glEnd();
    }

    if (useShader) {
        m_shaderProg->release();
    }

    if (!m_wireframe) {
        glDisable(GL_POLYGON_OFFSET_FILL);

        // Feature edge outlines for opaque bodies
        if (m_showFeatureEdges) {
            glDisable(GL_LIGHTING);
            for (const auto &mesh : m_bodyMeshes) {
                if (!mesh.isVisible || (m_transparentBodies && mesh.isTransparent)) continue;

                bool isSelected = (mesh.bodyId == m_selectedBodyId);
                if (isSelected) {
                    glLineWidth(2.5f);
                    glColor4f(1.0f, 0.85f, 0.15f, 1.0f);
                } else {
                    glLineWidth(1.2f);
                    auto bc = mesh.color;
                    glColor4f(bc.r * 0.45f / 255.0f, bc.g * 0.45f / 255.0f, bc.b * 0.45f / 255.0f, 0.85f);
                }

                glBegin(GL_LINES);
                for (const auto &edge : mesh.featureEdges) {
                    if (!m_showBottom && edge.first.z <= dBot + 1e-3 && edge.second.z <= dBot + 1e-3) {
                        continue;
                    }
                    glVertex3f(static_cast<float>(edge.first.x), static_cast<float>(edge.first.y), scaleZ(edge.first.z));
                    glVertex3f(static_cast<float>(edge.second.x), static_cast<float>(edge.second.y), scaleZ(edge.second.z));
                }
                glEnd();
            }
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
            QColor color;
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
            int alpha = static_cast<int>(std::clamp(mesh.alpha, 0.15f, 0.85f) * 255.0f);
            QColor color(bc.r, bc.g, bc.b, alpha);

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

                auto norm = fct.normal();
                norm.z /= std::max(0.1f, m_zScale);
                double nLen = std::sqrt(norm.x*norm.x + norm.y*norm.y + norm.z*norm.z);
                if (nLen > 1e-6) { norm.x /= nLen; norm.y /= nLen; norm.z /= nLen; }

                float d = eyeDepth((v0 + v1 + v2) * (1.0f / 3.0f));
                transFacets.push_back({ v0, v1, v2, QVector3D(norm.x, norm.y, norm.z), color, d, mesh.bodyId });
            }
        }

        // Sort back-to-front (smaller eye-space depth is further from camera)
        std::sort(transFacets.begin(), transFacets.end(), [](const TransFacet &a, const TransFacet &b) {
            return a.depth < b.depth;
        });

        glEnable(GL_LIGHTING);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        if (m_wireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        } else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }

        if (useShader) {
            m_shaderProg->bind();
            m_shaderProg->setUniformValue("u_mvpMatrix", m_projMatrix * m_mvMatrix);
            m_shaderProg->setUniformValue("u_mvMatrix", m_mvMatrix);
            m_shaderProg->setUniformValue("u_normalMatrix", m_mvMatrix.normalMatrix());
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
        }

        glBegin(GL_TRIANGLES);
        for (const auto &tf : transFacets) {
            if (useShader) {
                m_shaderProg->setUniformValue("u_highlight", (tf.bodyId == m_selectedBodyId));
            }
            glColor4f(tf.color.redF(), tf.color.greenF(), tf.color.blueF(), tf.color.alphaF());
            glNormal3f(tf.normal.x(), tf.normal.y(), tf.normal.z());
            glVertex3f(tf.p0.x(), tf.p0.y(), tf.p0.z());
            glVertex3f(tf.p1.x(), tf.p1.y(), tf.p1.z());
            glVertex3f(tf.p2.x(), tf.p2.y(), tf.p2.z());
        }
        glEnd();

        if (useShader) {
            m_shaderProg->release();
        }

        if (!m_wireframe && m_showFeatureEdges) {
            glDisable(GL_LIGHTING);
            for (const auto &mesh : m_bodyMeshes) {
                if (!mesh.isVisible || !mesh.isTransparent) continue;

                bool isSelected = (mesh.bodyId == m_selectedBodyId);
                if (isSelected) {
                    glLineWidth(2.5f);
                    glColor4f(1.0f, 0.85f, 0.15f, 1.0f);
                } else {
                    glLineWidth(1.2f);
                    auto bc = mesh.color;
                    glColor4f(bc.r * 0.45f / 255.0f, bc.g * 0.45f / 255.0f, bc.b * 0.45f / 255.0f, 0.65f);
                }

                glBegin(GL_LINES);
                for (const auto &edge : mesh.featureEdges) {
                    if (!m_showBottom && edge.first.z <= dBot + 1e-3 && edge.second.z <= dBot + 1e-3) {
                        continue;
                    }
                    glVertex3f(static_cast<float>(edge.first.x), static_cast<float>(edge.first.y), scaleZ(edge.first.z));
                    glVertex3f(static_cast<float>(edge.second.x), static_cast<float>(edge.second.y), scaleZ(edge.second.z));
                }
                glEnd();
            }
        }

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_LIGHTING);
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
        if (!activePv) {
            if (auto *mw = qobject_cast<MainWindow*>(mdi->window())) {
                activePv = mw->activeProfileView();
            }
        }
        if (!activePv && !openPvs.empty()) {
            activePv = openPvs.front();
        }

        for (auto *pv : openPvs) {
            slices.push_back({ pv->profileOrientation(), pv->profileIndex(), (pv == activePv) });
        }
    }

    if (slices.empty()) {
        slices.push_back({ m_doc->profileOrientation(), m_doc->profileIndex(), true });
    }

    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // 1. Semi-transparent slice planes pass
    if (m_transparentProfiles) {
        glDepthMask(GL_FALSE);
        glBegin(GL_QUADS);

        // Inactive profile planes
        for (const auto &sl : slices) {
            if (sl.isActive) continue;
            glColor4f(0.35f, 0.55f, 0.85f, 0.08f);
            if (sl.orient == ProfileOrientation::EastWest) {
                float y = static_cast<float>(m.yd_val(sl.index + 1));
                glVertex3f(x0, y, z0); glVertex3f(x1, y, z0);
                glVertex3f(x1, y, z1); glVertex3f(x0, y, z1);
            } else {
                float x = static_cast<float>(m.xd_val(sl.index + 1));
                glVertex3f(x, y0, z0); glVertex3f(x, y1, z0);
                glVertex3f(x, y1, z1); glVertex3f(x, y0, z1);
            }
        }

        // Active profile plane
        for (const auto &sl : slices) {
            if (!sl.isActive) continue;
            glColor4f(0.96f, 0.28f, 0.22f, 0.26f);
            if (sl.orient == ProfileOrientation::EastWest) {
                float y = static_cast<float>(m.yd_val(sl.index + 1));
                glVertex3f(x0, y, z0); glVertex3f(x1, y, z0);
                glVertex3f(x1, y, z1); glVertex3f(x0, y, z1);
            } else {
                float x = static_cast<float>(m.xd_val(sl.index + 1));
                glVertex3f(x, y0, z0); glVertex3f(x, y1, z0);
                glVertex3f(x, y1, z1); glVertex3f(x, y0, z1);
            }
        }
        glEnd();
        glDepthMask(GL_TRUE);
    }

    // 2. Profile outlines
    glLineWidth(1.2f);
    glColor4f(0.35f, 0.60f, 0.85f, 0.50f);
    for (const auto &sl : slices) {
        if (sl.isActive) continue;
        glBegin(GL_LINE_LOOP);
        if (sl.orient == ProfileOrientation::EastWest) {
            float y = static_cast<float>(m.yd_val(sl.index + 1));
            glVertex3f(x0, y, z0); glVertex3f(x1, y, z0);
            glVertex3f(x1, y, z1); glVertex3f(x0, y, z1);
        } else {
            float x = static_cast<float>(m.xd_val(sl.index + 1));
            glVertex3f(x, y0, z0); glVertex3f(x, y1, z0);
            glVertex3f(x, y1, z1); glVertex3f(x, y0, z1);
        }
        glEnd();
    }

    // Active profile highlighted outline
    for (const auto &sl : slices) {
        if (!sl.isActive) continue;
        glLineWidth(2.8f);
        glColor4f(0.96f, 0.20f, 0.15f, 0.95f);
        glBegin(GL_LINE_LOOP);
        if (sl.orient == ProfileOrientation::EastWest) {
            float y = static_cast<float>(m.yd_val(sl.index + 1));
            glVertex3f(x0, y, z0); glVertex3f(x1, y, z0);
            glVertex3f(x1, y, z1); glVertex3f(x0, y, z1);
        } else {
            float x = static_cast<float>(m.xd_val(sl.index + 1));
            glVertex3f(x, y0, z0); glVertex3f(x, y1, z0);
            glVertex3f(x, y1, z1); glVertex3f(x, y0, z1);
        }
        glEnd();

        // Golden accent line on top ridge of active profile
        glLineWidth(3.0f);
        glColor4f(1.0f, 0.85f, 0.20f, 0.95f);
        glBegin(GL_LINES);
        if (sl.orient == ProfileOrientation::EastWest) {
            float y = static_cast<float>(m.yd_val(sl.index + 1));
            glVertex3f(x0, y, z1); glVertex3f(x1, y, z1);
        } else {
            float x = static_cast<float>(m.xd_val(sl.index + 1));
            glVertex3f(x, y0, z1); glVertex3f(x, y1, z1);
        }
        glEnd();
    }

    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
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

void GL3DView::drawOrientationHud() {
    int hudSize = 130;
    qreal dpr = devicePixelRatioF();
    int physSize = static_cast<int>(hudSize * dpr);
    int margin = static_cast<int>(16 * dpr);

    GLint prevViewport[4];
    glGetIntegerv(GL_VIEWPORT, prevViewport);

    // Use glScissor so clearing depth inside HUD does NOT wipe the main 3D depth buffer
    glEnable(GL_SCISSOR_TEST);
    glScissor(margin, margin, physSize, physSize);
    glViewport(margin, margin, physSize, physSize);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(-1.8, 1.8, -1.8, 1.8, -10.0, 10.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glClear(GL_DEPTH_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);

    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // 1. Frosted circular background plate in screen space
    glColor4f(0.12f, 0.14f, 0.18f, 0.72f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(0.0f, 0.0f);
    for (int i = 0; i <= 36; ++i) {
        float angle = 2.0f * 3.14159265f * static_cast<float>(i) / 36.0f;
        glVertex2f(std::cos(angle) * 1.68f, std::sin(angle) * 1.68f);
    }
    glEnd();

    // Outer border ring
    glColor4f(0.35f, 0.42f, 0.52f, 0.85f);
    glLineWidth(1.8f);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 36; ++i) {
        float angle = 2.0f * 3.14159265f * static_cast<float>(i) / 36.0f;
        glVertex2f(std::cos(angle) * 1.68f, std::sin(angle) * 1.68f);
    }
    glEnd();

    // 2. 3D Rotating Coordinate Tripod
    glPushMatrix();
    glRotatef(m_pitch - 90.0f, 1.0f, 0.0f, 0.0f);
    glRotatef(m_azimuth, 0.0f, 0.0f, 1.0f);

    // Compass ring on horizontal (XY) plane
    glColor4f(0.40f, 0.50f, 0.65f, 0.45f);
    glLineWidth(1.2f);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 36; ++i) {
        float angle = 2.0f * 3.14159265f * static_cast<float>(i) / 36.0f;
        glVertex3f(std::cos(angle) * 0.92f, std::sin(angle) * 0.92f, 0.0f);
    }
    glEnd();

    // Cardinal tick marks
    glBegin(GL_LINES);
    for (int i = 0; i < 8; ++i) {
        float angle = static_cast<float>(i) * (3.14159265f / 4.0f);
        float ca = std::cos(angle), sa = std::sin(angle);
        float r1 = (i % 2 == 0) ? 0.82f : 0.87f;
        glVertex3f(ca * r1, sa * r1, 0.0f);
        glVertex3f(ca * 0.92f, sa * 0.92f, 0.0f);
    }
    glEnd();

    // North (+Y) Axis & Cone - Vibrant Green (Highlight if hovered)
    if (m_gizmoHoverAxis == 2) glColor3f(0.40f, 1.0f, 0.60f); else glColor3f(0.18f, 0.85f, 0.35f);
    glLineWidth(m_gizmoHoverAxis == 2 ? 3.8f : 2.8f);
    glBegin(GL_LINES);
    glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(0.0f, 0.88f, 0.0f);
    glEnd();
    drawArrowCone(0.0f, 0.88f, 0.0f, 0.0f, 1.0f, 0.0f, 0.22f, 0.075f);

    // South (-Y) Axis & Cone
    if (m_gizmoHoverAxis == 3) glColor3f(0.85f, 0.90f, 1.0f); else glColor3f(0.55f, 0.62f, 0.72f);
    glLineWidth(m_gizmoHoverAxis == 3 ? 3.0f : 1.8f);
    glBegin(GL_LINES);
    glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(0.0f, -0.68f, 0.0f);
    glEnd();
    drawArrowCone(0.0f, -0.68f, 0.0f, 0.0f, -1.0f, 0.0f, 0.16f, 0.055f);

    // East (+X) Axis & Cone - Vibrant Red
    if (m_gizmoHoverAxis == 0) glColor3f(1.0f, 0.45f, 0.45f); else glColor3f(0.92f, 0.22f, 0.22f);
    glLineWidth(m_gizmoHoverAxis == 0 ? 3.8f : 2.8f);
    glBegin(GL_LINES);
    glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(0.88f, 0.0f, 0.0f);
    glEnd();
    drawArrowCone(0.88f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.22f, 0.075f);

    // West (-X) Axis & Cone
    if (m_gizmoHoverAxis == 1) glColor3f(0.85f, 0.90f, 1.0f); else glColor3f(0.55f, 0.62f, 0.72f);
    glLineWidth(m_gizmoHoverAxis == 1 ? 3.0f : 1.8f);
    glBegin(GL_LINES);
    glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(-0.68f, 0.0f, 0.0f);
    glEnd();
    drawArrowCone(-0.68f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.16f, 0.055f);

    // Up (+Z) Axis & Cone - Vibrant Royal Blue
    if (m_gizmoHoverAxis == 4) glColor3f(0.45f, 0.80f, 1.0f); else glColor3f(0.20f, 0.60f, 1.0f);
    glLineWidth(m_gizmoHoverAxis == 4 ? 3.8f : 2.8f);
    glBegin(GL_LINES);
    glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(0.0f, 0.0f, 0.88f);
    glEnd();
    drawArrowCone(0.0f, 0.0f, 0.88f, 0.0f, 0.0f, 1.0f, 0.22f, 0.075f);

    // Center pivot node
    glColor3f(0.92f, 0.92f, 0.95f);
    glPointSize(5.0f);
    glBegin(GL_POINTS);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glEnd();

    // Labels
    float hudLabelSize = 0.24f;
    drawBillboardLabel("N",  0.0f,  1.18f, 0.0f, 0.20f, 0.95f, 0.40f, hudLabelSize, m_pitch, m_azimuth);
    drawBillboardLabel("S",  0.0f, -0.96f, 0.0f, 0.75f, 0.82f, 0.90f, hudLabelSize * 0.85f, m_pitch, m_azimuth);
    drawBillboardLabel("E",  1.18f, 0.0f,  0.0f, 1.0f,  0.30f, 0.30f, hudLabelSize, m_pitch, m_azimuth);
    drawBillboardLabel("W", -0.96f, 0.0f,  0.0f, 0.75f, 0.82f, 0.90f, hudLabelSize * 0.85f, m_pitch, m_azimuth);
    drawBillboardLabel("UP", 0.0f,  0.0f,  1.18f, 0.35f, 0.75f, 1.0f,  hudLabelSize * 0.90f, m_pitch, m_azimuth);

    glPopMatrix(); // End 3D rotating triad

    // Restore ModelView & Projection
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    // Restore Viewport
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
    glEnable(GL_LIGHTING);
}

void GL3DView::animateToCamera(float targetPitch, float targetAzimuth, float targetDist, const QVector3D *targetLookAt, int durationMs) {
    if (m_camAnim) {
        m_camAnim->stop();
        delete m_camAnim;
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

    connect(m_camAnim, &QVariantAnimation::valueChanged, this, [=, this](const QVariant &v) {
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

    connect(m_camAnim, &QVariantAnimation::finished, this, [this]() {
        m_camAnim->deleteLater();
        m_camAnim = nullptr;
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

void GL3DView::setShowProfilePlane(bool show) {
    m_showProfilePlane = show;
    update();
}

void GL3DView::setShowFeatureEdges(bool show) {
    m_showFeatureEdges = show;
    update();
}

void GL3DView::setShowBottom(bool show) {
    m_showBottom = show;
    update();
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

