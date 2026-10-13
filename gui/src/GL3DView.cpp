#include "mod3d_gui/GL3DView.h"
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <cmath>
#include <algorithm>

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

    // Apply rotation (Z points Up)
    glRotatef(m_rotX - 90.0f, 1.0f, 0.0f, 0.0f);
    glRotatef(m_rotZ, 0.0f, 0.0f, 1.0f);
    glRotatef(m_rotY, 0.0f, 1.0f, 0.0f);

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

    // Coordinate orientation axis tripod at origin corner (x0, y0, z0)
    float axisLen = static_cast<float>((x1 - x0) * 0.12f);
    // X (Red - East)
    glColor3f(0.85f, 0.20f, 0.20f);
    glVertex3f(x0, y0, z0); glVertex3f(x0 + axisLen, y0, z0);
    // Y (Green - North)
    glColor3f(0.20f, 0.75f, 0.20f);
    glVertex3f(x0, y0, z0); glVertex3f(x0, y0 + axisLen, z0);
    // Z (Blue - Up)
    glColor3f(0.20f, 0.40f, 0.90f);
    glVertex3f(x0, y0, z0); glVertex3f(x0, y0, z0 + axisLen);
    glEnd();

    glEnable(GL_LIGHTING);
}

void GL3DView::drawRelief() {
    glDisable(GL_LIGHTING);
    glColor4f(0.35f, 0.65f, 0.40f, 0.35f);
    glLineWidth(1.0f);

    const auto &m = m_doc->model();
    int rows = m.getRows();
    int cols = m.getCols();

    // Wireframe terrain mesh conforming to the actual model surface elevations at index 0
    glBegin(GL_LINES);
    for (int r = 1; r <= rows; ++r) {
        float y = static_cast<float>(m.getYd(r));
        for (int c = 1; c < cols; ++c) {
            float xA = static_cast<float>(m.getXd(c));
            float xB = static_cast<float>(m.getXd(c + 1));
            float zA = static_cast<float>(m.getZ(r, c, 0));
            float zB = static_cast<float>(m.getZ(r, c + 1, 0));
            glVertex3f(xA, y, zA);
            glVertex3f(xB, y, zB);
        }
    }
    for (int c = 1; c <= cols; ++c) {
        float x = static_cast<float>(m.getXd(c));
        for (int r = 1; r < rows; ++r) {
            float yA = static_cast<float>(m.getYd(r));
            float yB = static_cast<float>(m.getYd(r + 1));
            float zA = static_cast<float>(m.getZ(r, c, 0));
            float zB = static_cast<float>(m.getZ(r + 1, c, 0));
            glVertex3f(x, yA, zA);
            glVertex3f(x, yB, zB);
        }
    }
    glEnd();

    glEnable(GL_LIGHTING);
}

void GL3DView::drawBodies() {
    if (m_meshDirty) {
        m_bodyMeshes = m_doc->model().generateBodyMeshes(-1, m_showOuterFacets);
        m_meshDirty = false;
    }

    if (m_bodyMeshes.empty()) return;

    double dBot = m_doc->model().getHell();

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
                fct.pts[0].z <= dBot + 1e-3 &&
                fct.pts[1].z <= dBot + 1e-3 &&
                fct.pts[2].z <= dBot + 1e-3) {
                continue;
            }
            glNormal3d(fct.v_n.x, fct.v_n.y, fct.v_n.z);
            glVertex3d(fct.pts[0].x, fct.pts[0].y, fct.pts[0].z);
            glVertex3d(fct.pts[1].x, fct.pts[1].y, fct.pts[1].z);
            glVertex3d(fct.pts[2].x, fct.pts[2].y, fct.pts[2].z);
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
                    fct.pts[0].z <= dBot + 1e-3 &&
                    fct.pts[1].z <= dBot + 1e-3 &&
                    fct.pts[2].z <= dBot + 1e-3) {
                    continue;
                }
                glNormal3d(fct.v_n.x, fct.v_n.y, fct.v_n.z);
                glVertex3d(fct.pts[0].x, fct.pts[0].y, fct.pts[0].z);
                glVertex3d(fct.pts[1].x, fct.pts[1].y, fct.pts[1].z);
                glVertex3d(fct.pts[2].x, fct.pts[2].y, fct.pts[2].z);
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

void GL3DView::drawProfilePlane() {
    glDisable(GL_LIGHTING);
    const auto &m = m_doc->model();
    double coord = m_doc->currentProfileCoordinate();
    bool isEW = (m_doc->profileOrientation() == ProfileOrientation::EastWest);

    float z0 = static_cast<float>(m.getZMin());
    float z1 = static_cast<float>(m.getZMax());

    // Semi-transparent slice plane
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.95f, 0.25f, 0.25f, 0.20f);

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

    // Sharp outline around the profile slice plane
    glColor4f(0.95f, 0.25f, 0.25f, 0.85f);
    glLineWidth(1.5f);
    glBegin(GL_LINE_LOOP);
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

    glDisable(GL_BLEND);
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
            // Pan
            m_panX += dx * 20.0f / m_zoom;
            m_panY -= dy * 20.0f / m_zoom;
        } else if (m_dragButton == Qt::LeftButton) {
            // Trackball Rotate
            m_rotZ += dx * 0.5f;
            m_rotX += dy * 0.5f;
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

    auto *actProfile = menu.addAction(tr("Show Active Profile Plane"), this, [this](bool checked) {
        setShowProfilePlane(checked);
    });
    actProfile->setCheckable(true);
    actProfile->setChecked(m_showProfilePlane);

    auto *actBBox = menu.addAction(tr("Show Bounding Box"), this, [this](bool checked) {
        setShowBoundingBox(checked);
    });
    actBBox->setCheckable(true);
    actBBox->setChecked(m_showBoundingBox);

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

} // namespace mod3d_gui
