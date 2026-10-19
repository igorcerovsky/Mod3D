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
#include <cmath>
#include <algorithm>
#include <string>
#include <QPainter>

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
    // Invert the camera rotation so the label is strictly parallel to screen / camera
    glRotatef(-azimuth, 0.0f, 0.0f, 1.0f);
    glRotatef(-(pitch - 90.0f), 1.0f, 0.0f, 0.0f);

    // 1. Dark outline / drop shadow
    glColor4f(0.06f, 0.07f, 0.10f, 0.90f);
    glLineWidth(3.8f);
    drawStrokeString(text, size);

    // 2. Vivid foreground core
    glColor4f(r, g, b, 1.0f);
    glLineWidth(2.2f);
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

    connect(m_doc, &ModelDocument::modelGeometryChanged, this, [this]() {
        m_meshDirty = true;
        update();
    });
    connect(m_doc, &ModelDocument::profileChanged, this, [this](int, ProfileOrientation) {
        update();
    });
    connect(m_doc, &ModelDocument::documentReset, this, [this]() {
        m_meshDirty = true;
        resetCamera();
        update();
    });
}

void GL3DView::initializeGL() {
    if (!isValid() || !context()) return;
    initializeOpenGLFunctions();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

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
}

void GL3DView::resizeGL(int w, int h) {
    if (!isValid() || !context()) return;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    float aspect = (h > 0) ? (static_cast<float>(w) / h) : 1.0f;
    float fov = 45.0f * 3.14159265f / 180.0f;
    float zNear = 100.0f;
    float zFar = 100000.0f;
    float top = zNear * std::tan(fov * 0.5f);
    float bottom = -top;
    float left = bottom * aspect;
    float right = top * aspect;
    glFrustum(left, right, bottom, top, zNear, zFar);

    glMatrixMode(GL_MODELVIEW);
}

void GL3DView::paintGL() {
    if (!isValid() || !context()) return;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    if (!m_doc) return;
    const auto &model = m_doc->model();

    // Directional Key Light & Soft Fill Light in Eye Coordinates (headlight / sun)
    GLfloat light0Pos[] = { 0.5f, 0.7f, 1.0f, 0.0f }; // Directional key light from upper-right-front
    GLfloat light0Amb[] = { 0.15f, 0.15f, 0.15f, 1.0f };
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

    // Center model at origin
    double midX = (model.x_min_val() + model.x_max_val()) * 0.5;
    double midY = (model.y_min_val() + model.y_max_val()) * 0.5;
    double midZ = (model.z_min_val() + model.z_max_val()) * 0.5;
    double maxDim = std::max({ model.x_max_val() - model.x_min_val(),
                               model.y_max_val() - model.y_min_val(),
                               model.z_max_val() - model.z_min_val() });
    if (maxDim <= 0.0) maxDim = 10000.0;

    // Camera eye position back along Z
    float dist = static_cast<float>(maxDim * 1.8 / m_zoom);
    glTranslatef(m_panX, m_panY, -dist);

    // Apply Turntable Orbit (Pitch tilts from horizontal, Azimuth orbits around global Z)
    glRotatef(m_pitch - 90.0f, 1.0f, 0.0f, 0.0f);
    glRotatef(m_azimuth, 0.0f, 0.0f, 1.0f);

    // Shift to model center
    glTranslatef(static_cast<float>(-midX), static_cast<float>(-midY), static_cast<float>(-midZ));

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

    if (m_showOrientationIndicator) {
        drawOrientationHud();
    }
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
    float z0 = static_cast<float>(m.z_min_val());
    float z1 = static_cast<float>(m.z_max_val());

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

    // Coordinate orientation axis tripod at origin corner (x0, y0, z0)
    float axisLen = static_cast<float>((x1 - x0) * 0.14f);
    float coneLen = axisLen * 0.22f;
    float coneRadius = axisLen * 0.065f;
    float labelSize = axisLen * 0.18f;

    // East (+X) Axis - Red
    glColor3f(0.92f, 0.22f, 0.22f);
    glLineWidth(2.5f);
    glBegin(GL_LINES);
    glVertex3f(x0, y0, z0); glVertex3f(x0 + axisLen, y0, z0);
    glEnd();
    drawArrowCone(x0 + axisLen, y0, z0, 1.0f, 0.0f, 0.0f, coneLen, coneRadius);
    drawBillboardLabel("E", x0 + axisLen + coneLen * 0.9f, y0, z0,
                       0.95f, 0.25f, 0.25f, labelSize, m_pitch, m_azimuth);

    // North (+Y) Axis - Green
    glColor3f(0.18f, 0.80f, 0.32f);
    glLineWidth(2.5f);
    glBegin(GL_LINES);
    glVertex3f(x0, y0, z0); glVertex3f(x0, y0 + axisLen, z0);
    glEnd();
    drawArrowCone(x0, y0 + axisLen, z0, 0.0f, 1.0f, 0.0f, coneLen, coneRadius);
    drawBillboardLabel("N", x0, y0 + axisLen + coneLen * 0.9f, z0,
                       0.20f, 0.85f, 0.35f, labelSize, m_pitch, m_azimuth);

    // Up (+Z) Axis - Blue
    glColor3f(0.20f, 0.55f, 0.95f);
    glLineWidth(2.5f);
    glBegin(GL_LINES);
    glVertex3f(x0, y0, z0); glVertex3f(x0, y0, z0 + axisLen);
    glEnd();
    drawArrowCone(x0, y0, z0 + axisLen, 0.0f, 0.0f, 1.0f, coneLen, coneRadius);
    drawBillboardLabel("UP", x0, y0, z0 + axisLen + coneLen * 0.9f,
                       0.25f, 0.65f, 1.0f, labelSize * 0.85f, m_pitch, m_azimuth);

    // Compass cardinal direction labels on outer bounding box top edges
    float edgeLabelSize = (x1 - x0) * 0.035f;
    float midX = (x0 + x1) * 0.5f;
    float midY = (y0 + y1) * 0.5f;
    drawBillboardLabel("NORTH", midX, y1, z1, 0.18f, 0.80f, 0.32f, edgeLabelSize, m_pitch, m_azimuth);
    drawBillboardLabel("SOUTH", midX, y0, z1, 0.55f, 0.60f, 0.68f, edgeLabelSize, m_pitch, m_azimuth);
    drawBillboardLabel("EAST",  x1, midY, z1, 0.92f, 0.25f, 0.25f, edgeLabelSize, m_pitch, m_azimuth);
    drawBillboardLabel("WEST",  x0, midY, z1, 0.55f, 0.60f, 0.68f, edgeLabelSize, m_pitch, m_azimuth);

    glEnable(GL_LIGHTING);
}

void GL3DView::drawRelief() {
    glDisable(GL_LIGHTING);
    glColor4f(0.35f, 0.65f, 0.40f, 0.35f);
    glLineWidth(1.0f);

    const auto &m = m_doc->model();
    int rows = m.rows();
    int cols = m.cols();
    int rMax = rows - 2;
    int cMax = cols - 2;
    if (rMax < 1 || cMax < 1) return;

    // Wireframe terrain mesh conforming to the actual model surface elevations at index 0
    glBegin(GL_LINES);
    // Grid lines along X (running West-East at each core row)
    for (int r = 1; r <= rMax; ++r) {
        float y = static_cast<float>(m.yd_val(r));
        for (int c = 1; c < cMax; ++c) {
            float xA = static_cast<float>(m.xd_val(c));
            float xB = static_cast<float>(m.xd_val(c + 1));
            float zA = static_cast<float>(m.z(r, c, 0));
            float zB = static_cast<float>(m.z(r, c + 1, 0));
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
            float zA = static_cast<float>(m.z(r, c, 0));
            float zB = static_cast<float>(m.z(r + 1, c, 0));
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

    // 1. Opaque bodies pass
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

    for (const auto &mesh : m_bodyMeshes) {
        if (!mesh.isVisible || mesh.isTransparent) continue;

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
            glNormal3d(fct.normal().x, fct.normal().y, fct.normal().z);
            glVertex3d(fct[0].x, fct[0].y, fct[0].z);
            glVertex3d(fct[1].x, fct[1].y, fct[1].z);
            glVertex3d(fct[2].x, fct[2].y, fct[2].z);
        }
        glEnd();
    }

    if (!m_wireframe) {
        glDisable(GL_POLYGON_OFFSET_FILL);

        // Feature edge outlines for opaque bodies
        if (m_showFeatureEdges) {
            glDisable(GL_LIGHTING);
            glLineWidth(1.2f);
            for (const auto &mesh : m_bodyMeshes) {
                if (!mesh.isVisible || mesh.isTransparent) continue;

                auto bc = mesh.color;
                glColor4f(bc.r * 0.45f / 255.0f, bc.g * 0.45f / 255.0f, bc.b * 0.45f / 255.0f, 0.85f);

                glBegin(GL_LINES);
                for (const auto &edge : mesh.featureEdges) {
                    if (!m_showBottom && edge.first.z <= dBot + 1e-3 && edge.second.z <= dBot + 1e-3) {
                        continue;
                    }
                    glVertex3d(edge.first.x, edge.first.y, edge.first.z);
                    glVertex3d(edge.second.x, edge.second.y, edge.second.z);
                }
                glEnd();
            }
        }
    }

    // 2. Transparent bodies pass
    bool hasTransparent = false;
    for (const auto &mesh : m_bodyMeshes) {
        if (mesh.isVisible && mesh.isTransparent) {
            hasTransparent = true;
            break;
        }
    }

    if (hasTransparent) {
        glEnable(GL_LIGHTING);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        if (m_wireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        } else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(1.0f, 1.0f);
        }

        for (const auto &mesh : m_bodyMeshes) {
            if (!mesh.isVisible || !mesh.isTransparent) continue;

            auto bc = mesh.color;
            float alpha = std::clamp(mesh.alpha, 0.15f, 0.85f);
            glColor4f(bc.r / 255.0f, bc.g / 255.0f, bc.b / 255.0f, alpha);

            glBegin(GL_TRIANGLES);
            for (const auto &fct : mesh.facets) {
                if (!m_showBottom &&
                    fct[0].z <= dBot + 1e-3 &&
                    fct[1].z <= dBot + 1e-3 &&
                    fct[2].z <= dBot + 1e-3) {
                    continue;
                }
                glNormal3d(fct.normal().x, fct.normal().y, fct.normal().z);
                glVertex3d(fct[0].x, fct[0].y, fct[0].z);
                glVertex3d(fct[1].x, fct[1].y, fct[1].z);
                glVertex3d(fct[2].x, fct[2].y, fct[2].z);
            }
            glEnd();
        }

        if (!m_wireframe) {
            glDisable(GL_POLYGON_OFFSET_FILL);

            if (m_showFeatureEdges) {
                glDisable(GL_LIGHTING);
                glLineWidth(1.2f);
                for (const auto &mesh : m_bodyMeshes) {
                    if (!mesh.isVisible || !mesh.isTransparent) continue;

                    auto bc = mesh.color;
                    glColor4f(bc.r * 0.45f / 255.0f, bc.g * 0.45f / 255.0f, bc.b * 0.45f / 255.0f, 0.65f);

                    glBegin(GL_LINES);
                    for (const auto &edge : mesh.featureEdges) {
                        if (!m_showBottom && edge.first.z <= dBot + 1e-3 && edge.second.z <= dBot + 1e-3) {
                            continue;
                        }
                        glVertex3d(edge.first.x, edge.first.y, edge.first.z);
                        glVertex3d(edge.second.x, edge.second.y, edge.second.z);
                    }
                    glEnd();
                }
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

    float z0 = static_cast<float>(m.z_min_val());
    float z1 = static_cast<float>(m.z_max_val());
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
        // Fallback for standalone usage / testing
        slices.push_back({ m_doc->profileOrientation(), m_doc->profileIndex(), true });
    }

    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // 1. Semi-transparent slice planes pass
    glDepthMask(GL_FALSE);
    glBegin(GL_QUADS);

    // Inactive profile planes (open in inactive profile views)
    for (const auto &sl : slices) {
        if (sl.isActive) continue;
        glColor4f(0.35f, 0.55f, 0.85f, 0.08f);
        if (sl.orient == ProfileOrientation::EastWest) {
            float y = static_cast<float>(m.yd_val(sl.index + 1));
            glVertex3f(x0, y, z0);
            glVertex3f(x1, y, z0);
            glVertex3f(x1, y, z1);
            glVertex3f(x0, y, z1);
        } else {
            float x = static_cast<float>(m.xd_val(sl.index + 1));
            glVertex3f(x, y0, z0);
            glVertex3f(x, y1, z0);
            glVertex3f(x, y1, z1);
            glVertex3f(x, y0, z1);
        }
    }

    // Active profile plane (open in active profile view)
    for (const auto &sl : slices) {
        if (!sl.isActive) continue;
        glColor4f(0.96f, 0.28f, 0.22f, 0.26f);
        if (sl.orient == ProfileOrientation::EastWest) {
            float y = static_cast<float>(m.yd_val(sl.index + 1));
            glVertex3f(x0, y, z0);
            glVertex3f(x1, y, z0);
            glVertex3f(x1, y, z1);
            glVertex3f(x0, y, z1);
        } else {
            float x = static_cast<float>(m.xd_val(sl.index + 1));
            glVertex3f(x, y0, z0);
            glVertex3f(x, y1, z0);
            glVertex3f(x, y1, z1);
            glVertex3f(x, y0, z1);
        }
    }
    glEnd();
    glDepthMask(GL_TRUE);

    // 2. Profile outlines
    // Inactive profile outlines
    glLineWidth(1.2f);
    glColor4f(0.35f, 0.60f, 0.85f, 0.50f);
    for (const auto &sl : slices) {
        if (sl.isActive) continue;
        glBegin(GL_LINE_LOOP);
        if (sl.orient == ProfileOrientation::EastWest) {
            float y = static_cast<float>(m.yd_val(sl.index + 1));
            glVertex3f(x0, y, z0);
            glVertex3f(x1, y, z0);
            glVertex3f(x1, y, z1);
            glVertex3f(x0, y, z1);
        } else {
            float x = static_cast<float>(m.xd_val(sl.index + 1));
            glVertex3f(x, y0, z0);
            glVertex3f(x, y1, z0);
            glVertex3f(x, y1, z1);
            glVertex3f(x, y0, z1);
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
            glVertex3f(x0, y, z0);
            glVertex3f(x1, y, z0);
            glVertex3f(x1, y, z1);
            glVertex3f(x0, y, z1);
        } else {
            float x = static_cast<float>(m.xd_val(sl.index + 1));
            glVertex3f(x, y0, z0);
            glVertex3f(x, y1, z0);
            glVertex3f(x, y1, z1);
            glVertex3f(x, y0, z1);
        }
        glEnd();

        // Golden accent line on top ridge of active profile
        glLineWidth(3.0f);
        glColor4f(1.0f, 0.85f, 0.20f, 0.95f);
        glBegin(GL_LINES);
        if (sl.orient == ProfileOrientation::EastWest) {
            float y = static_cast<float>(m.yd_val(sl.index + 1));
            glVertex3f(x0, y, z1);
            glVertex3f(x1, y, z1);
        } else {
            float x = static_cast<float>(m.xd_val(sl.index + 1));
            glVertex3f(x, y0, z1);
            glVertex3f(x, y1, z1);
        }
        glEnd();
    }

    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

void GL3DView::drawOrientationHud() {
    int hudSize = 130;
    qreal dpr = devicePixelRatioF();
    int physSize = static_cast<int>(hudSize * dpr);
    int margin = static_cast<int>(16 * dpr);

    GLint prevViewport[4];
    glGetIntegerv(GL_VIEWPORT, prevViewport);

    glViewport(margin, margin, physSize, physSize);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(-1.8, 1.8, -1.8, 1.8, -10.0, 10.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // Clear depth in HUD viewport so it renders cleanly above scene
    glClear(GL_DEPTH_BUFFER_BIT);
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

    // 2. 3D Rotating Compass Coordinate Tripod
    glPushMatrix();
    // Apply camera rotation matching 3D turntable orbit
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

    // Cardinal tick marks on horizontal ring
    glBegin(GL_LINES);
    for (int i = 0; i < 8; ++i) {
        float angle = static_cast<float>(i) * (3.14159265f / 4.0f);
        float ca = std::cos(angle);
        float sa = std::sin(angle);
        float r1 = (i % 2 == 0) ? 0.82f : 0.87f;
        glVertex3f(ca * r1, sa * r1, 0.0f);
        glVertex3f(ca * 0.92f, sa * 0.92f, 0.0f);
    }
    glEnd();

    // North (+Y) Axis & Cone - Vibrant Green
    glColor3f(0.18f, 0.85f, 0.35f);
    glLineWidth(2.8f);
    glBegin(GL_LINES);
    glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(0.0f, 0.88f, 0.0f);
    glEnd();
    drawArrowCone(0.0f, 0.88f, 0.0f, 0.0f, 1.0f, 0.0f, 0.22f, 0.075f);

    // South (-Y) Axis & Cone - Subtle Slate
    glColor3f(0.55f, 0.62f, 0.72f);
    glLineWidth(1.8f);
    glBegin(GL_LINES);
    glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(0.0f, -0.68f, 0.0f);
    glEnd();
    drawArrowCone(0.0f, -0.68f, 0.0f, 0.0f, -1.0f, 0.0f, 0.16f, 0.055f);

    // East (+X) Axis & Cone - Vibrant Red
    glColor3f(0.92f, 0.22f, 0.22f);
    glLineWidth(2.8f);
    glBegin(GL_LINES);
    glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(0.88f, 0.0f, 0.0f);
    glEnd();
    drawArrowCone(0.88f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.22f, 0.075f);

    // West (-X) Axis & Cone - Subtle Slate
    glColor3f(0.55f, 0.62f, 0.72f);
    glLineWidth(1.8f);
    glBegin(GL_LINES);
    glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(-0.68f, 0.0f, 0.0f);
    glEnd();
    drawArrowCone(-0.68f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.16f, 0.055f);

    // Up (+Z) Axis & Cone - Vibrant Royal Blue
    glColor3f(0.20f, 0.60f, 1.0f);
    glLineWidth(2.8f);
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

    // Labels - Billboarded camera-facing text
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

void GL3DView::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton || event->button() == Qt::MiddleButton) {
        m_mouseDragging = true;
        m_dragButton = event->button();
        m_dragStartPos = event->pos();
        m_lastMousePos = event->pos();
    }
    QOpenGLWidget::mousePressEvent(event);
}

void GL3DView::mouseMoveEvent(QMouseEvent *event) {
    if (m_mouseDragging) {
        int dx = event->pos().x() - m_lastMousePos.x();
        int dy = event->pos().y() - m_lastMousePos.y();

        if (m_dragButton == Qt::RightButton || m_dragButton == Qt::MiddleButton || (event->modifiers() & Qt::ShiftModifier)) {
            // Tactile 1:1 Pan proportional to model scale, camera distance, and viewport height
            double maxDim = 10000.0;
            if (m_doc) {
                const auto &model = m_doc->model();
                maxDim = std::max({ model.x_max_val() - model.x_min_val(),
                                    model.y_max_val() - model.y_min_val(),
                                    model.z_max_val() - model.z_min_val() });
            }
            if (maxDim <= 0.0) maxDim = 10000.0;
            float dist = static_cast<float>(maxDim * 1.8 / m_zoom);
            float fov = 45.0f * 3.14159265f / 180.0f;
            float viewH = 2.0f * dist * std::tan(fov * 0.5f);
            float pixelScale = viewH / static_cast<float>(std::max(1, height()));

            m_panX += dx * pixelScale;
            m_panY -= dy * pixelScale;
        } else if (m_dragButton == Qt::LeftButton) {
            if (event->modifiers() & Qt::ControlModifier) {
                // Smooth interactive zoom with Ctrl+Left drag
                float zoomDelta = 1.0f - static_cast<float>(dy) * 0.008f;
                m_zoom *= zoomDelta;
                if (m_zoom < 0.01f) m_zoom = 0.01f;
                if (m_zoom > 100.0f) m_zoom = 100.0f;
            } else {
                // Standard CAD / DCC Turntable Orbit (Blender, ParaView, SketchUp):
                // - Horizontal mouse movement orbits azimuth smoothly around global Z axis
                // - Vertical mouse movement adjusts elevation / pitch clamped to [-89.0°, +89.0°]
                // Moving mouse UP tilts view up towards top-down; moving mouse DOWN lowers elevation
                float rotSpeed = 0.40f;
                m_azimuth += dx * rotSpeed;
                m_pitch -= dy * rotSpeed;

                if (m_pitch > 89.0f) m_pitch = 89.0f;
                if (m_pitch < -89.0f) m_pitch = -89.0f;

                while (m_azimuth > 180.0f) m_azimuth -= 360.0f;
                while (m_azimuth < -180.0f) m_azimuth += 360.0f;
            }
        }

        m_lastMousePos = event->pos();
        update();
    }
    QOpenGLWidget::mouseMoveEvent(event);
}

void GL3DView::mouseReleaseEvent(QMouseEvent *event) {
    if (m_mouseDragging) {
        m_mouseDragging = false;
        if (event->button() == Qt::RightButton) {
            int dist = (event->pos() - m_dragStartPos).manhattanLength();
            if (dist <= 3) {
                showContextMenu(event->globalPosition().toPoint());
            }
        }
    }
    QOpenGLWidget::mouseReleaseEvent(event);
}

void GL3DView::wheelEvent(QWheelEvent *event) {
    float numDegrees = event->angleDelta().y() / 8.0f;
    float numSteps = numDegrees / 15.0f;
    float factor = std::pow(1.15f, numSteps);
    m_zoom *= factor;
    if (m_zoom < 0.01f) m_zoom = 0.01f;
    if (m_zoom > 100.0f) m_zoom = 100.0f;
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
    default:
        QOpenGLWidget::keyPressEvent(event);
        break;
    }
}

void GL3DView::contextMenuEvent(QContextMenuEvent *event) {
    showContextMenu(event->globalPos());
}

void GL3DView::showContextMenu(const QPoint &globalPos) {
    QMenu menu(this);

    menu.addAction(tr("Reset Camera"), this, &GL3DView::resetCamera);

    auto *viewsMenu = menu.addMenu(tr("Standard Views"));
    viewsMenu->addAction(tr("Top Map View (M)"), this, &GL3DView::setViewTopMap);
    viewsMenu->addAction(tr("East View (E)"), this, &GL3DView::setViewEast);
    viewsMenu->addAction(tr("West View (W)"), this, &GL3DView::setViewWest);
    viewsMenu->addAction(tr("North View (N)"), this, &GL3DView::setViewNorth);
    viewsMenu->addAction(tr("South View (S)"), this, &GL3DView::setViewSouth);

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
}

void GL3DView::setViewEast() {
    m_pitch = 0.0f;
    m_azimuth = 90.0f;
    m_panX = 0.0f;
    m_panY = 0.0f;
    update();
}

void GL3DView::setViewWest() {
    m_pitch = 0.0f;
    m_azimuth = -90.0f;
    m_panX = 0.0f;
    m_panY = 0.0f;
    update();
}

void GL3DView::setViewNorth() {
    m_pitch = 0.0f;
    m_azimuth = 180.0f;
    m_panX = 0.0f;
    m_panY = 0.0f;
    update();
}

void GL3DView::setViewSouth() {
    m_pitch = 0.0f;
    m_azimuth = 0.0f;
    m_panX = 0.0f;
    m_panY = 0.0f;
    update();
}

void GL3DView::setViewTopMap() {
    m_pitch = 89.9f;
    m_azimuth = 0.0f;
    m_panX = 0.0f;
    m_panY = 0.0f;
    update();
}

void GL3DView::resetCamera() {
    m_pitch = 30.0f;
    m_azimuth = -45.0f;
    m_panX = 0.0f;
    m_panY = 0.0f;
    m_zoom = 1.0f;
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

} // namespace mod3d_gui
