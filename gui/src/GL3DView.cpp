#include "mod3d_gui/GL3DView.h"
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <cmath>

#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

namespace mod3d_gui {

GL3DView::GL3DView(ModelDocument *doc, QWidget *parent)
    : QOpenGLWidget(parent)
    , m_doc(doc)
{
    setFocusPolicy(Qt::StrongFocus);

    connect(m_doc, &ModelDocument::modelGeometryChanged, this, QOverload<>::of(&QWidget::update));
    connect(m_doc, &ModelDocument::profileChanged, this, [this](int, ProfileOrientation) { update(); });
    connect(m_doc, &ModelDocument::documentReset, this, [this]() {
        resetCamera();
        update();
    });
}

void GL3DView::initializeGL() {
    initializeOpenGLFunctions();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Light setup
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    GLfloat lightPos[] = { 10000.0f, 10000.0f, 15000.0f, 1.0f };
    GLfloat lightAmb[] = { 0.35f, 0.35f, 0.35f, 1.0f };
    GLfloat lightDiff[] = { 0.85f, 0.85f, 0.85f, 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiff);

    glClearColor(0.92f, 0.93f, 0.95f, 1.0f);
}

void GL3DView::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    float aspect = (h > 0) ? (static_cast<float>(w) / h) : 1.0f;
    // Simple perspective setup: glFrustum
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
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    if (!m_doc) return;
    const auto &model = m_doc->model();

    // Center model at origin
    double midX = (model.getXMin() + model.getXMax()) * 0.5;
    double midY = (model.getYMin() + model.getYMax()) * 0.5;
    double midZ = (model.getZMin() + model.getZMax()) * 0.5;
    double maxDim = std::max({ model.getXMax() - model.getXMin(),
                               model.getYMax() - model.getYMin(),
                               model.getZMax() - model.getZMin() });
    if (maxDim <= 0.0) maxDim = 10000.0;

    // Camera eye position back along Z
    float dist = static_cast<float>(maxDim * 1.8 / m_zoom);
    glTranslatef(m_panX, m_panY, -dist);

    // Apply rotation
    glRotatef(m_rotX - 90.0f, 1.0f, 0.0f, 0.0f); // Make Z point Up
    glRotatef(m_rotZ, 0.0f, 0.0f, 1.0f);
    glRotatef(m_rotY, 0.0f, 1.0f, 0.0f);

    // Shift to model center
    glTranslatef(static_cast<float>(-midX), static_cast<float>(-midY), static_cast<float>(-midZ));

    // Polygon mode
    if (m_wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    // Draw elements
    drawBoundingBox();

    if (m_showRelief) {
        drawRelief();
    }

    drawBodies();

    if (m_showProfilePlane) {
        drawProfilePlane();
    }
}

void GL3DView::drawBoundingBox() {
    glDisable(GL_LIGHTING);
    glColor3f(0.55f, 0.55f, 0.60f);
    glLineWidth(1.0f);

    const auto &m = m_doc->model();
    float x0 = static_cast<float>(m.getXMin());
    float x1 = static_cast<float>(m.getXMax());
    float y0 = static_cast<float>(m.getYMin());
    float y1 = static_cast<float>(m.getYMax());
    float z0 = static_cast<float>(m.getZMin());
    float z1 = static_cast<float>(m.getZMax());

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

    glEnable(GL_LIGHTING);
}

void GL3DView::drawRelief() {
    glDisable(GL_LIGHTING);
    glColor4f(0.35f, 0.70f, 0.35f, 0.40f);

    const auto &m = m_doc->model();
    int rows = m.getRows();
    int cols = m.getCols();

    // Wireframe terrain mesh
    glBegin(GL_LINES);
    for (int r = 1; r <= rows; ++r) {
        float y = static_cast<float>(m.getYd(r));
        for (int c = 1; c < cols; ++c) {
            float xA = static_cast<float>(m.getXd(c));
            float xB = static_cast<float>(m.getXd(c + 1));
            glVertex3f(xA, y, 0.0f);
            glVertex3f(xB, y, 0.0f);
        }
    }
    for (int c = 1; c <= cols; ++c) {
        float x = static_cast<float>(m.getXd(c));
        for (int r = 1; r < rows; ++r) {
            float yA = static_cast<float>(m.getYd(r));
            float yB = static_cast<float>(m.getYd(r + 1));
            glVertex3f(x, yA, 0.0f);
            glVertex3f(x, yB, 0.0f);
        }
    }
    glEnd();

    glEnable(GL_LIGHTING);
}

void GL3DView::drawBodies() {
    const auto &model = m_doc->model();
    int rows = model.getRows();
    int cols = model.getCols();

    for (const auto &body : model.getBodies()) {
        int bId = body->id();
        auto bc = body->color();
        float alpha = body->is_transparent() ? body->transparency() : 0.90f;
        glColor4f(bc.r / 255.0f, bc.g / 255.0f, bc.b / 255.0f, alpha);

        // Discretize prisms across adjacent columns
        for (int r = 1; r < rows; ++r) {
            for (int c = 1; c < cols; ++c) {
                int idx00 = model.getBodyIndex(bId, r, c);
                int idx10 = model.getBodyIndex(bId, r + 1, c);
                int idx11 = model.getBodyIndex(bId, r + 1, c + 1);
                int idx01 = model.getBodyIndex(bId, r, c + 1);

                    float x0 = static_cast<float>(model.getXd(c));
                    float x1 = static_cast<float>(model.getXd(c + 1));
                    float y0 = static_cast<float>(model.getYd(r));
                    float y1 = static_cast<float>(model.getYd(r + 1));

                    // Top quad
                    glBegin(GL_QUADS);
                    glNormal3f(0.0f, 0.0f, 1.0f);
                    glVertex3f(x0, y0, static_cast<float>(model.getZ(r, c, idx00)));
                    glVertex3f(x1, y0, static_cast<float>(model.getZ(r, c + 1, idx01)));
                    glVertex3f(x1, y1, static_cast<float>(model.getZ(r + 1, c + 1, idx11)));
                    glVertex3f(x0, y1, static_cast<float>(model.getZ(r + 1, c, idx10)));
                    glEnd();

                    // Bottom quad
                    glBegin(GL_QUADS);
                    glNormal3f(0.0f, 0.0f, -1.0f);
                    glVertex3f(x0, y0, static_cast<float>(model.getZ(r, c, idx00 + 1)));
                    glVertex3f(x0, y1, static_cast<float>(model.getZ(r + 1, c, idx10 + 1)));
                    glVertex3f(x1, y1, static_cast<float>(model.getZ(r + 1, c + 1, idx11 + 1)));
                    glVertex3f(x1, y0, static_cast<float>(model.getZ(r, c + 1, idx01 + 1)));
                    glEnd();
            }
        }
    }
}

void GL3DView::drawProfilePlane() {
    glDisable(GL_LIGHTING);
    glColor4f(0.95f, 0.20f, 0.20f, 0.25f);

    const auto &m = m_doc->model();
    double coord = m_doc->currentProfileCoordinate();
    bool isEW = (m_doc->profileOrientation() == ProfileOrientation::EastWest);

    float z0 = static_cast<float>(m.getZMin());
    float z1 = static_cast<float>(m.getZMax());

    glBegin(GL_QUADS);
    if (isEW) {
        float y = static_cast<float>(coord);
        float x0 = static_cast<float>(m.getXMin());
        float x1 = static_cast<float>(m.getXMax());
        glVertex3f(x0, y, z0);
        glVertex3f(x1, y, z0);
        glVertex3f(x1, y, z1);
        glVertex3f(x0, y, z1);
    } else {
        float x = static_cast<float>(coord);
        float y0 = static_cast<float>(m.getYMin());
        float y1 = static_cast<float>(m.getYMax());
        glVertex3f(x, y0, z0);
        glVertex3f(x, y1, z0);
        glVertex3f(x, y1, z1);
        glVertex3f(x, y0, z1);
    }
    glEnd();

    glEnable(GL_LIGHTING);
}

void GL3DView::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_mouseDragging = true;
        m_lastMousePos = event->pos();
    }
    QOpenGLWidget::mousePressEvent(event);
}

void GL3DView::mouseMoveEvent(QMouseEvent *event) {
    if (m_mouseDragging) {
        int dx = event->pos().x() - m_lastMousePos.x();
        int dy = event->pos().y() - m_lastMousePos.y();

        if (event->modifiers() & Qt::ShiftModifier) {
            // Pan
            m_panX += dx * 20.0f / m_zoom;
            m_panY -= dy * 20.0f / m_zoom;
        } else {
            // Rotate
            m_rotZ += dx * 0.5f;
            m_rotX += dy * 0.5f;
        }

        m_lastMousePos = event->pos();
        update();
    }
    QOpenGLWidget::mouseMoveEvent(event);
}

void GL3DView::mouseReleaseEvent(QMouseEvent *event) {
    m_mouseDragging = false;
    QOpenGLWidget::mouseReleaseEvent(event);
}

void GL3DView::wheelEvent(QWheelEvent *event) {
    if (event->angleDelta().y() > 0) {
        m_zoom *= 1.15f;
    } else if (event->angleDelta().y() < 0) {
        m_zoom /= 1.15f;
    }
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

void GL3DView::setViewEast() {
    m_rotX = 0.0f;
    m_rotY = 0.0f;
    m_rotZ = 90.0f;
    update();
}

void GL3DView::setViewWest() {
    m_rotX = 0.0f;
    m_rotY = 0.0f;
    m_rotZ = -90.0f;
    update();
}

void GL3DView::setViewNorth() {
    m_rotX = 0.0f;
    m_rotY = 0.0f;
    m_rotZ = 180.0f;
    update();
}

void GL3DView::setViewSouth() {
    m_rotX = 0.0f;
    m_rotY = 0.0f;
    m_rotZ = 0.0f;
    update();
}

void GL3DView::setViewTopMap() {
    m_rotX = -90.0f;
    m_rotY = 0.0f;
    m_rotZ = 0.0f;
    update();
}

void GL3DView::resetCamera() {
    m_rotX = 30.0f;
    m_rotY = -45.0f;
    m_rotZ = 0.0f;
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

} // namespace mod3d_gui
