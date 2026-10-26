#include "mod3d_gui/HorizontalSliceView.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMdiSubWindow>
#include <QApplication>
#include <algorithm>
#include <cmath>

namespace mod3d_gui {

HorizontalSliceView::HorizontalSliceView(ModelDocument *doc, double initialZ, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    if (m_doc) {
        if (!std::isnan(initialZ)) {
            m_sliceZ = initialZ;
        } else {
            m_sliceZ = m_doc->activeSliceElevation();
        }
        double zMin = m_doc->model().z_min_val();
        double zMax = m_doc->model().z_max_val();
        m_sliceZ = std::clamp(m_sliceZ, zMin, zMax);
    }

    setupUi();

    if (m_doc) {
        connect(m_doc, &ModelDocument::modelGeometryChanged, this, QOverload<>::of(&QWidget::update));
        connect(m_doc, &ModelDocument::computationFinished, this, QOverload<>::of(&QWidget::update));
        connect(m_doc, &ModelDocument::profileChanged, this, [this](int, ProfileOrientation) { update(); });
        connect(m_doc, &ModelDocument::activeProfileCursorChanged, this, [this](const ModelDocument::ProfileCursorPoint &) { update(); });
        connect(m_doc, &ModelDocument::documentReset, this, [this]() {
            fitToWindow();
            if (m_doc) {
                m_sliceZ = m_doc->activeSliceElevation();
                updateHeaderUi();
            }
            update();
        });
        connect(m_doc, &ModelDocument::verticalRangeChanged, this, [this](double zMin, double zMax) {
            m_sliceZ = std::clamp(m_sliceZ, zMin, zMax);
            updateHeaderUi();
            update();
        });
        connect(m_doc, &ModelDocument::activeSliceElevationChanged, this, [this](double z) {
            if (std::abs(m_sliceZ - z) > 1e-4) {
                m_sliceZ = z;
                updateHeaderUi();
                updateTitle();
                update();
            }
        });
    }

    fitToWindow();
    updateHeaderUi();
    updateTitle();
}

void HorizontalSliceView::setupUi() {
    m_headerWidget = new QWidget(this);
    m_headerWidget->setObjectName("sliceHeaderWidget");
    m_headerWidget->setStyleSheet(
        "#sliceHeaderWidget {"
        "  background: #f0f1f4;"
        "  border-bottom: 1px solid #d0d2d8;"
        "}"
        "QLabel { font-size: 11px; font-weight: 600; color: #333338; }"
        "QDoubleSpinBox {"
        "  background: #ffffff;"
        "  border: 1px solid #c0c2c8;"
        "  border-radius: 4px;"
        "  padding: 2px 4px;"
        "  font-size: 11px;"
        "}"
        "QPushButton {"
        "  background: #ffffff;"
        "  border: 1px solid #c0c2c8;"
        "  border-radius: 4px;"
        "  padding: 2px 8px;"
        "  font-size: 11px;"
        "  font-weight: 500;"
        "}"
        "QPushButton:hover { background: #e6e8ee; }"
        "QPushButton:pressed { background: #dcdfe6; }"
    );

    auto *headerLayout = new QHBoxLayout(m_headerWidget);
    headerLayout->setContentsMargins(8, 4, 8, 4);
    headerLayout->setSpacing(8);

    auto *lblElev = new QLabel(tr("Elevation Z:"), m_headerWidget);
    headerLayout->addWidget(lblElev);

    m_spinElevation = new QDoubleSpinBox(m_headerWidget);
    m_spinElevation->setDecimals(1);
    m_spinElevation->setSingleStep(50.0);
    m_spinElevation->setSuffix(" m");
    m_spinElevation->setMinimumWidth(90);
    headerLayout->addWidget(m_spinElevation);

    m_btnStepDown = new QPushButton(tr("▼ -50m"), m_headerWidget);
    m_btnStepDown->setToolTip(tr("Step slice down by 50m (Page Down)"));
    headerLayout->addWidget(m_btnStepDown);

    m_btnStepUp = new QPushButton(tr("▲ +50m"), m_headerWidget);
    m_btnStepUp->setToolTip(tr("Step slice up by 50m (Page Up)"));
    headerLayout->addWidget(m_btnStepUp);

    m_sliderElevation = new QSlider(Qt::Horizontal, m_headerWidget);
    m_sliderElevation->setRange(0, 1000);
    m_sliderElevation->setMinimumWidth(120);
    headerLayout->addWidget(m_sliderElevation);

    m_labelDepth = new QLabel(m_headerWidget);
    m_labelDepth->setStyleSheet("color: #60626a; font-weight: normal;");
    headerLayout->addWidget(m_labelDepth);

    headerLayout->addStretch(1);

    m_btnFit = new QPushButton(tr("Fit"), m_headerWidget);
    m_btnFit->setToolTip(tr("Fit model grid to view window"));
    headerLayout->addWidget(m_btnFit);

    connect(m_spinElevation, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        if (std::abs(m_sliceZ - val) > 1e-4) {
            setSliceElevation(val);
        }
    });

    connect(m_sliderElevation, &QSlider::valueChanged, this, [this](int sliderVal) {
        if (!m_doc) return;
        double zMin = m_doc->model().z_min_val();
        double zMax = m_doc->model().z_max_val();
        double fraction = static_cast<double>(sliderVal) / 1000.0;
        double newZ = zMin + fraction * (zMax - zMin);
        if (std::abs(m_sliceZ - newZ) > 1.0) {
            setSliceElevation(newZ);
        }
    });

    connect(m_btnStepUp, &QPushButton::clicked, this, [this]() { stepElevation(1); });
    connect(m_btnStepDown, &QPushButton::clicked, this, [this]() { stepElevation(-1); });
    connect(m_btnFit, &QPushButton::clicked, this, &HorizontalSliceView::fitToWindow);
}

void HorizontalSliceView::updateHeaderUi() {
    if (!m_doc || !m_spinElevation || !m_sliderElevation) return;
    double zMin = m_doc->model().z_min_val();
    double zMax = m_doc->model().z_max_val();

    m_spinElevation->blockSignals(true);
    m_sliderElevation->blockSignals(true);

    m_spinElevation->setRange(zMin, zMax);
    m_spinElevation->setValue(m_sliceZ);

    double span = zMax - zMin;
    if (span > 0.0) {
        int sVal = std::clamp(static_cast<int>(std::round(((m_sliceZ - zMin) / span) * 1000.0)), 0, 1000);
        m_sliderElevation->setValue(sVal);
    }

    m_spinElevation->blockSignals(false);
    m_sliderElevation->blockSignals(false);

    // Compute approximate depth below surface relief
    double avgTopo = 0.0;
    int count = 0;
    const auto &model = m_doc->model();
    for (int r = 1; r < model.rows() - 1; ++r) {
        for (int c = 1; c < model.cols() - 1; ++c) {
            avgTopo += model.z(r, c, 0);
            count++;
        }
    }
    if (count > 0) {
        avgTopo /= count;
        double depth = avgTopo - m_sliceZ;
        m_labelDepth->setText(tr("Avg Depth: ~%1 m").arg(std::round(depth)));
    } else {
        m_labelDepth->clear();
    }
}

void HorizontalSliceView::setSliceElevation(double z) {
    if (!m_doc) return;
    double zMin = m_doc->model().z_min_val();
    double zMax = m_doc->model().z_max_val();
    z = std::clamp(z, zMin, zMax);
    if (std::abs(m_sliceZ - z) > 1e-4) {
        m_sliceZ = z;
        updateHeaderUi();
        updateTitle();
        m_doc->setActiveSliceElevation(m_sliceZ);
        emit sliceElevationChanged(m_sliceZ);
        update();
    }
}

void HorizontalSliceView::stepElevation(int numSteps) {
    if (!m_doc) return;
    double stepSize = 50.0;
    double zMin = m_doc->model().z_min_val();
    double zMax = m_doc->model().z_max_val();
    if ((zMax - zMin) < 200.0) {
        stepSize = 10.0;
    } else if ((zMax - zMin) >= 5000.0) {
        stepSize = 100.0;
    }
    setSliceElevation(m_sliceZ + numSteps * stepSize);
}

void HorizontalSliceView::updateTitle() {
    QString title = QString("Horizontal Planar View — Z = %1 m").arg(std::round(m_sliceZ));
    if (auto *sub = qobject_cast<QMdiSubWindow *>(parentWidget())) {
        sub->setWindowTitle(title);
    } else {
        setWindowTitle(title);
    }
}

void HorizontalSliceView::fitToWindow() {
    if (!m_doc) return;
    const auto &m = m_doc->model();
    double dx = m.dx_val();
    double dy = m.dy_val();
    m_worldXMin = m.x_min_val() - dx * 0.5;
    m_worldXMax = m.x_max_val() + dx * 0.5;
    m_worldYMin = m.y_min_val() - dy * 0.5;
    m_worldYMax = m.y_max_val() + dy * 0.5;
    update();
}

void HorizontalSliceView::zoomIn() {
    double spanX = (m_worldXMax - m_worldXMin) * 0.8;
    double midX = (m_worldXMax + m_worldXMin) * 0.5;
    m_worldXMin = midX - spanX * 0.5;
    m_worldXMax = midX + spanX * 0.5;

    double spanY = (m_worldYMax - m_worldYMin) * 0.8;
    double midY = (m_worldYMax + m_worldYMin) * 0.5;
    m_worldYMin = midY - spanY * 0.5;
    m_worldYMax = midY + spanY * 0.5;
    update();
}

void HorizontalSliceView::zoomOut() {
    double spanX = (m_worldXMax - m_worldXMin) * 1.25;
    double midX = (m_worldXMax + m_worldXMin) * 0.5;
    m_worldXMin = midX - spanX * 0.5;
    m_worldXMax = midX + spanX * 0.5;

    double spanY = (m_worldYMax - m_worldYMin) * 1.25;
    double midY = (m_worldYMax + m_worldYMin) * 0.5;
    m_worldYMin = midY - spanY * 0.5;
    m_worldYMax = midY + spanY * 0.5;
    update();
}

QRect HorizontalSliceView::canvasRect() const {
    int availW = width() - m_marginLeft - m_marginRight;
    int availH = height() - m_marginTop - m_marginBottom;
    if (availW <= 10 || availH <= 10) {
        return QRect(m_marginLeft, m_marginTop, std::max(10, availW), std::max(10, availH));
    }

    double spanX = m_worldXMax - m_worldXMin;
    double spanY = m_worldYMax - m_worldYMin;
    if (spanX <= 0.0 || spanY <= 0.0) {
        return QRect(m_marginLeft, m_marginTop, availW, availH);
    }

    // Equal scale: pixels per meter identical on both axes
    double scaleX = static_cast<double>(availW) / spanX;
    double scaleY = static_cast<double>(availH) / spanY;
    double scale = std::min(scaleX, scaleY);

    int actualW = std::max(1, static_cast<int>(std::round(scale * spanX)));
    int actualH = std::max(1, static_cast<int>(std::round(scale * spanY)));

    int offX = m_marginLeft + (availW - actualW) / 2;
    int offY = m_marginTop + (availH - actualH) / 2;

    return QRect(offX, offY, actualW, actualH);
}

double HorizontalSliceView::scalePixelsPerMeter() const {
    QRect canvas = canvasRect();
    double spanX = m_worldXMax - m_worldXMin;
    if (spanX <= 0.0) return 1.0;
    return static_cast<double>(canvas.width()) / spanX;
}

double HorizontalSliceView::screenToWorldX(int sx) const {
    QRect canvas = canvasRect();
    if (canvas.width() <= 0) return m_worldXMin;
    double t = static_cast<double>(sx - canvas.left()) / canvas.width();
    return m_worldXMin + t * (m_worldXMax - m_worldXMin);
}

double HorizontalSliceView::screenToWorldY(int sy) const {
    QRect canvas = canvasRect();
    if (canvas.height() <= 0) return m_worldYMin;
    double t = static_cast<double>(sy - canvas.top()) / canvas.height();
    return m_worldYMax - t * (m_worldYMax - m_worldYMin);
}

int HorizontalSliceView::worldToScreenX(double wx) const {
    QRect canvas = canvasRect();
    double spanX = m_worldXMax - m_worldXMin;
    if (spanX <= 0.0) return canvas.left();
    double t = (wx - m_worldXMin) / spanX;
    return canvas.left() + static_cast<int>(std::round(t * canvas.width()));
}

int HorizontalSliceView::worldToScreenY(double wy) const {
    QRect canvas = canvasRect();
    double spanY = m_worldYMax - m_worldYMin;
    if (spanY <= 0.0) return canvas.top();
    double t = (m_worldYMax - wy) / spanY;
    return canvas.top() + static_cast<int>(std::round(t * canvas.height()));
}

void HorizontalSliceView::setShowStationPoints(bool show) {
    if (m_showStationPoints != show) {
        m_showStationPoints = show;
        update();
    }
}

void HorizontalSliceView::setShowProfiles(bool show) {
    if (m_showProfiles != show) {
        m_showProfiles = show;
        update();
    }
}

void HorizontalSliceView::setShowCellBorders(bool show) {
    if (m_showCellBorders != show) {
        m_showCellBorders = show;
        update();
    }
}

std::vector<HorizontalSliceView::BoundaryEdge> HorizontalSliceView::activeBoundaryEdges() const {
    std::vector<BoundaryEdge> edges;
    if (!m_doc) return edges;
    const auto &model = m_doc->model();
    int nRows = model.rows();
    int nCols = model.cols();
    double dx = model.dx_val();
    double dy = model.dy_val();

    for (const auto &bPtr : model.bodies()) {
        int bId = bPtr->id();
        for (int r = 1; r < nRows - 1; ++r) {
            for (int c = 1; c < nCols - 1; ++c) {
                int bIdx = model.body_index(bId, r, c);
                if (bIdx < 0) continue;
                double topZ = model.z(r, c, bIdx);
                double botZ = model.z(r, c, bIdx + 1);
                if (topZ <= botZ) continue;
                if (m_sliceZ < botZ || m_sliceZ > topZ) continue;

                double xc = model.xd_val(c);
                double yr = model.yd_val(r);
                double xLeft = xc - dx * 0.5;
                double xRight = xc + dx * 0.5;
                double yBot = yr - dy * 0.5;
                double yTop = yr + dy * 0.5;

                auto intersectsAt = [&](int checkR, int checkC) {
                    if (checkR < 1 || checkR >= nRows - 1 || checkC < 1 || checkC >= nCols - 1) return false;
                    int idx = model.body_index(bId, checkR, checkC);
                    if (idx < 0) return false;
                    double tz = model.z(checkR, checkC, idx);
                    double bz = model.z(checkR, checkC, idx + 1);
                    return (tz > bz && m_sliceZ >= bz && m_sliceZ <= tz);
                };

                // East edge: between c and c+1
                if (!intersectsAt(r, c + 1)) {
                    edges.push_back({bId, r, c, r, c + 1, CardinalDirection::East, xRight, yBot, xRight, yTop});
                }
                // West edge: between c and c-1
                if (!intersectsAt(r, c - 1)) {
                    edges.push_back({bId, r, c, r, c - 1, CardinalDirection::West, xLeft, yBot, xLeft, yTop});
                }
                // North edge: between r and r+1
                if (!intersectsAt(r + 1, c)) {
                    edges.push_back({bId, r, c, r + 1, c, CardinalDirection::North, xLeft, yTop, xRight, yTop});
                }
                // South edge: between r and r-1
                if (!intersectsAt(r - 1, c)) {
                    edges.push_back({bId, r, c, r - 1, c, CardinalDirection::South, xLeft, yBot, xRight, yBot});
                }
            }
        }
    }
    return edges;
}

HorizontalSliceView::HitType HorizontalSliceView::performHitTest(
    const QPoint &pos, BoundaryEdge &outEdge, int &outRow, int &outCol, int &outBodyId) const
{
    outRow = -1;
    outCol = -1;
    outBodyId = -1;

    // 1. Check boundary edges first (distance <= 6 px)
    auto edges = activeBoundaryEdges();
    for (const auto &e : edges) {
        int sx1 = worldToScreenX(e.x1);
        int sy1 = worldToScreenY(e.y1);
        int sx2 = worldToScreenX(e.x2);
        int sy2 = worldToScreenY(e.y2);

        double dxSeg = sx2 - sx1;
        double dySeg = sy2 - sy1;
        double lenSq = dxSeg * dxSeg + dySeg * dySeg;
        if (lenSq < 1e-4) continue;

        double t = std::clamp(((pos.x() - sx1) * dxSeg + (pos.y() - sy1) * dySeg) / lenSq, 0.0, 1.0);
        double px = sx1 + t * dxSeg;
        double py = sy1 + t * dySeg;
        double dist = std::hypot(pos.x() - px, pos.y() - py);

        if (dist <= 6.0) {
            outEdge = e;
            outRow = e.srcRow;
            outCol = e.srcCol;
            outBodyId = e.bodyId;
            return HitType::BoundaryEdge;
        }
    }

    // 2. Check if inside a grid cell
    if (!m_doc) return HitType::None;
    const auto &model = m_doc->model();
    double wx = screenToWorldX(pos.x());
    double wy = screenToWorldY(pos.y());
    double x0 = model.x0_val();
    double y0 = model.y0_val();
    double dx = model.dx_val();
    double dy = model.dy_val();
    if (dx <= 0.0 || dy <= 0.0) return HitType::None;

    int col = static_cast<int>(std::round((wx - x0) / dx)) + 1;
    int row = static_cast<int>(std::round((wy - y0) / dy)) + 1;

    if (row >= 1 && row < model.rows() - 1 && col >= 1 && col < model.cols() - 1) {
        outRow = row;
        outCol = col;
        for (const auto &bPtr : model.bodies()) {
            int bIdx = model.body_index(bPtr->id(), row, col);
            if (bIdx >= 0) {
                double tz = model.z(row, col, bIdx);
                double bz = model.z(row, col, bIdx + 1);
                if (tz > bz && m_sliceZ >= bz && m_sliceZ <= tz) {
                    outBodyId = bPtr->id();
                    return HitType::BodyCell;
                }
            }
        }
        return HitType::EmptyCell;
    }

    return HitType::None;
}

void HorizontalSliceView::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Canvas background
    painter.fillRect(rect(), QColor(248, 248, 250));

    QRect canvas = canvasRect();

    // Draw main components
    drawGridAndCells(painter, canvas);
    drawProfilesAndMarkers(painter, canvas);
    drawBoundaryEdges(painter, canvas);
    drawAxes(painter, canvas);
}

void HorizontalSliceView::drawGridAndCells(QPainter &painter, const QRect &canvas) {
    if (!m_doc) return;
    const auto &model = m_doc->model();
    int nRows = model.rows();
    int nCols = model.cols();
    double dx = model.dx_val();
    double dy = model.dy_val();

    painter.save();
    painter.setClipRect(canvas);
    painter.fillRect(canvas, Qt::white);

    // 1. Draw each cell
    for (int r = 1; r < nRows - 1; ++r) {
        for (int c = 1; c < nCols - 1; ++c) {
            double xc = model.xd_val(c);
            double yr = model.yd_val(r);

            int sx1 = worldToScreenX(xc - dx * 0.5);
            int sx2 = worldToScreenX(xc + dx * 0.5);
            int sy1 = worldToScreenY(yr + dy * 0.5);
            int sy2 = worldToScreenY(yr - dy * 0.5);

            QRect cellRect(sx1, sy1, sx2 - sx1, sy2 - sy1);

            // Check intersecting bodies
            int hitBodyId = -1;
            for (const auto &bPtr : model.bodies()) {
                int bIdx = model.body_index(bPtr->id(), r, c);
                if (bIdx >= 0) {
                    double tz = model.z(r, c, bIdx);
                    double bz = model.z(r, c, bIdx + 1);
                    if (tz > bz && m_sliceZ >= bz && m_sliceZ <= tz) {
                        hitBodyId = bPtr->id();
                        auto bc = bPtr->color();
                        QColor fillCol(bc.r, bc.g, bc.b, 175);
                        painter.fillRect(cellRect, fillCol);
                        break;
                    }
                }
            }

            // Cell border
            if (m_showCellBorders) {
                painter.setPen(QColor(230, 232, 236));
                painter.drawRect(cellRect);
            }

            // Station node marker
            if (m_showStationPoints) {
                int scx = worldToScreenX(xc);
                int scy = worldToScreenY(yr);
                painter.setPen(Qt::NoPen);
                painter.setBrush(hitBodyId >= 0 ? QColor(40, 40, 50, 180) : QColor(160, 165, 175, 160));
                painter.drawEllipse(QPoint(scx, scy), 2, 2);
            }
        }
    }

    painter.restore();
}

void HorizontalSliceView::drawBoundaryEdges(QPainter &painter, const QRect &canvas) {
    if (!m_doc) return;
    painter.save();
    painter.setClipRect(canvas);

    auto edges = activeBoundaryEdges();

    for (const auto &e : edges) {
        int sx1 = worldToScreenX(e.x1);
        int sy1 = worldToScreenY(e.y1);
        int sx2 = worldToScreenX(e.x2);
        int sy2 = worldToScreenY(e.y2);

        bool isHover = (m_isHoveringEdge &&
                        m_hoverEdge.bodyId == e.bodyId &&
                        m_hoverEdge.srcRow == e.srcRow &&
                        m_hoverEdge.srcCol == e.srcCol &&
                        m_hoverEdge.dir == e.dir);

        bool isDrag = (m_dragOp == DragOperation::ExtendOrCollapseEdge &&
                       m_activeDragEdge.bodyId == e.bodyId &&
                       m_activeDragEdge.srcRow == e.srcRow &&
                       m_activeDragEdge.srcCol == e.srcCol &&
                       m_activeDragEdge.dir == e.dir);

        if (isDrag || isHover) {
            // Highlighted active edge
            painter.setPen(QPen(QColor(255, 130, 0), 3.5, Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(sx1, sy1, sx2, sy2);

            // Handle circle at midpoint
            int mx = (sx1 + sx2) / 2;
            int my = (sy1 + sy2) / 2;
            painter.setBrush(QColor(255, 150, 0));
            painter.setPen(QPen(Qt::white, 1.5));
            painter.drawEllipse(QPoint(mx, my), 5, 5);
        } else {
            // Standard boundary stroke
            QColor strokeColor(50, 50, 60, 220);
            if (auto *b = m_doc->model().body(e.bodyId)) {
                auto bc = b->color();
                strokeColor = QColor(std::max(0, bc.r - 40), std::max(0, bc.g - 40), std::max(0, bc.b - 40), 240);
            }
            painter.setPen(QPen(strokeColor, 2.0, Qt::SolidLine, Qt::SquareCap));
            painter.drawLine(sx1, sy1, sx2, sy2);
        }
    }

    painter.restore();
}

void HorizontalSliceView::drawProfilesAndMarkers(QPainter &painter, const QRect &canvas) {
    if (!m_doc) return;
    painter.save();
    painter.setClipRect(canvas);

    // 1. Draw Active Profile Lines
    if (m_showProfiles) {
        int prfIdx = m_doc->profileIndex();
        ProfileOrientation orient = m_doc->profileOrientation();
        const auto &model = m_doc->model();

        if (orient == ProfileOrientation::EastWest && prfIdx >= 0 && prfIdx < model.rows() - 2) {
            double yr = model.yd_val(prfIdx + 1);
            int sy = worldToScreenY(yr);
            int sx1 = worldToScreenX(m_worldXMin);
            int sx2 = worldToScreenX(m_worldXMax);
            painter.setPen(QPen(QColor(0, 140, 230, 200), 1.8, Qt::DashLine));
            painter.drawLine(sx1, sy, sx2, sy);
        } else if (orient == ProfileOrientation::SouthNorth && prfIdx >= 0 && prfIdx < model.cols() - 2) {
            double xc = model.xd_val(prfIdx + 1);
            int sx = worldToScreenX(xc);
            int sy1 = worldToScreenY(m_worldYMin);
            int sy2 = worldToScreenY(m_worldYMax);
            painter.setPen(QPen(QColor(230, 110, 0, 200), 1.8, Qt::DashLine));
            painter.drawLine(sx, sy1, sx, sy2);
        }
    }

    // 2. Draw Active Cursor Point Marker
    const auto &pt = m_doc->activeProfileCursor();
    if (pt.active && pt.row >= 1 && pt.col >= 1) {
        int sx = worldToScreenX(pt.x);
        int sy = worldToScreenY(pt.y);

        painter.setPen(QPen(QColor(220, 30, 30), 2.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(QPoint(sx, sy), 7, 7);

        painter.drawLine(sx - 10, sy, sx + 10, sy);
        painter.drawLine(sx, sy - 10, sx, sy + 10);
    }

    painter.restore();
}

void HorizontalSliceView::drawAxes(QPainter &painter, const QRect &canvas) {
    painter.save();
    painter.setPen(QColor(160, 162, 170));
    painter.drawRect(canvas);

    QFont f = painter.font();
    f.setPointSize(8);
    painter.setFont(f);

    // Bottom X-Axis (East coordinate in meters)
    int nTicksX = 5;
    for (int i = 0; i <= nTicksX; ++i) {
        double t = static_cast<double>(i) / nTicksX;
        double wx = m_worldXMin + t * (m_worldXMax - m_worldXMin);
        int sx = canvas.left() + static_cast<int>(std::round(t * canvas.width()));
        painter.drawLine(sx, canvas.bottom(), sx, canvas.bottom() + 4);
        QString label = QString::number(std::round(wx));
        QRect textRect(sx - 30, canvas.bottom() + 6, 60, 15);
        painter.drawText(textRect, Qt::AlignCenter, label);
    }
    painter.drawText(QRect(canvas.left(), canvas.bottom() + 22, canvas.width(), 16),
                     Qt::AlignCenter, tr("East Coordinate X (m)"));

    // Left Y-Axis (North coordinate in meters)
    int nTicksY = 5;
    for (int i = 0; i <= nTicksY; ++i) {
        double t = static_cast<double>(i) / nTicksY;
        double wy = m_worldYMin + t * (m_worldYMax - m_worldYMin);
        int sy = canvas.bottom() - static_cast<int>(std::round(t * canvas.height()));
        painter.drawLine(canvas.left() - 4, sy, canvas.left(), sy);
        QString label = QString::number(std::round(wy));
        QRect textRect(canvas.left() - 60, sy - 8, 54, 16);
        painter.drawText(textRect, Qt::AlignRight | Qt::AlignVCenter, label);
    }

    painter.restore();
}

void HorizontalSliceView::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        BoundaryEdge hitEdge;
        int hitRow = -1, hitCol = -1, hitBodyId = -1;
        HitType ht = performHitTest(event->pos(), hitEdge, hitRow, hitCol, hitBodyId);

        if (ht == HitType::BoundaryEdge) {
            m_dragOp = DragOperation::ExtendOrCollapseEdge;
            m_dragStartPos = event->pos();
            m_activeDragEdge = hitEdge;
            m_lastDragRow = hitEdge.srcRow;
            m_lastDragCol = hitEdge.srcCol;
            update();
        } else if (ht == HitType::BodyCell || ht == HitType::EmptyCell) {
            if (m_doc && hitRow >= 1 && hitCol >= 1) {
                double wx = m_doc->model().xd_val(hitCol);
                double wy = m_doc->model().yd_val(hitRow);
                std::string bName;
                if (auto *b = m_doc->model().body(hitBodyId)) {
                    bName = b->name();
                }
                m_doc->setActiveProfileCursor({
                    true,
                    hitRow,
                    hitCol,
                    wx,
                    wy,
                    m_sliceZ,
                    hitBodyId,
                    bName,
                    false
                });
                emit cursorStationChanged(hitRow, hitCol, wx, wy, m_sliceZ, 0.0);
                update();
            }
        }
    } else if (event->button() == Qt::RightButton || event->button() == Qt::MiddleButton) {
        m_dragOp = DragOperation::Pan;
        m_lastPanPos = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }

    QWidget::mousePressEvent(event);
}

void HorizontalSliceView::mouseMoveEvent(QMouseEvent *event) {
    if (m_dragOp == DragOperation::Pan) {
        QPoint delta = event->pos() - m_lastPanPos;
        m_lastPanPos = event->pos();

        double scale = scalePixelsPerMeter();
        if (scale > 0.0) {
            double dwx = -delta.x() / scale;
            double dwy = delta.y() / scale;
            m_worldXMin += dwx;
            m_worldXMax += dwx;
            m_worldYMin += dwy;
            m_worldYMax += dwy;
            update();
        }
        return;
    }

    if (m_dragOp == DragOperation::ExtendOrCollapseEdge && m_doc) {
        const auto &model = m_doc->model();
        double wx = screenToWorldX(event->pos().x());
        double wy = screenToWorldY(event->pos().y());
        double x0 = model.x0_val();
        double y0 = model.y0_val();
        double dx = model.dx_val();
        double dy = model.dy_val();

        int currC = std::clamp(static_cast<int>(std::round((wx - x0) / dx)) + 1, 1, model.cols() - 2);
        int currR = std::clamp(static_cast<int>(std::round((wy - y0) / dy)) + 1, 1, model.rows() - 2);

        int srcR = m_activeDragEdge.srcRow;
        int srcC = m_activeDragEdge.srcCol;
        int bId = m_activeDragEdge.bodyId;

        // Process step based on initial drag direction
        switch (m_activeDragEdge.dir) {
        case CardinalDirection::East:
            if (currC > srcC) {
                // Outward extension into column currC
                bool ok = m_doc->extendBodySideAtRowCol(srcR, srcC, srcR, srcC + 1, bId, m_sliceZ);
                if (ok) {
                    m_activeDragEdge.srcCol = srcC + 1;
                    m_activeDragEdge.tgtCol = srcC + 2;
                    update();
                }
            } else if (currC < srcC) {
                // Inward collapse of srcCol
                bool ok = m_doc->collapseBodyEdgeAtRowCol(srcR, srcC, bId);
                if (ok) {
                    m_activeDragEdge.srcCol = srcC - 1;
                    m_activeDragEdge.tgtCol = srcC;
                    update();
                }
            }
            break;

        case CardinalDirection::West:
            if (currC < srcC) {
                // Outward extension into column currC
                bool ok = m_doc->extendBodySideAtRowCol(srcR, srcC, srcR, srcC - 1, bId, m_sliceZ);
                if (ok) {
                    m_activeDragEdge.srcCol = srcC - 1;
                    m_activeDragEdge.tgtCol = srcC - 2;
                    update();
                }
            } else if (currC > srcC) {
                // Inward collapse
                bool ok = m_doc->collapseBodyEdgeAtRowCol(srcR, srcC, bId);
                if (ok) {
                    m_activeDragEdge.srcCol = srcC + 1;
                    m_activeDragEdge.tgtCol = srcC;
                    update();
                }
            }
            break;

        case CardinalDirection::North:
            if (currR > srcR) {
                // Outward extension into row currR
                bool ok = m_doc->extendBodySideAtRowCol(srcR, srcC, srcR + 1, srcC, bId, m_sliceZ);
                if (ok) {
                    m_activeDragEdge.srcRow = srcR + 1;
                    m_activeDragEdge.tgtRow = srcR + 2;
                    update();
                }
            } else if (currR < srcR) {
                // Inward collapse
                bool ok = m_doc->collapseBodyEdgeAtRowCol(srcR, srcC, bId);
                if (ok) {
                    m_activeDragEdge.srcRow = srcR - 1;
                    m_activeDragEdge.tgtRow = srcR;
                    update();
                }
            }
            break;

        case CardinalDirection::South:
            if (currR < srcR) {
                // Outward extension into row currR
                bool ok = m_doc->extendBodySideAtRowCol(srcR, srcC, srcR - 1, srcC, bId, m_sliceZ);
                if (ok) {
                    m_activeDragEdge.srcRow = srcR - 1;
                    m_activeDragEdge.tgtRow = srcR - 2;
                    update();
                }
            } else if (currR > srcR) {
                // Inward collapse
                bool ok = m_doc->collapseBodyEdgeAtRowCol(srcR, srcC, bId);
                if (ok) {
                    m_activeDragEdge.srcRow = srcR + 1;
                    m_activeDragEdge.tgtRow = srcR;
                    update();
                }
            }
            break;
        }

        double wxCoord = model.xd_val(m_activeDragEdge.srcCol);
        double wyCoord = model.yd_val(m_activeDragEdge.srcRow);
        std::string bName;
        if (auto *b = model.body(bId)) bName = b->name();
        m_doc->setActiveProfileCursor({
            true,
            m_activeDragEdge.srcRow,
            m_activeDragEdge.srcCol,
            wxCoord,
            wyCoord,
            m_sliceZ,
            bId,
            bName,
            true
        });
        emit cursorStationChanged(m_activeDragEdge.srcRow, m_activeDragEdge.srcCol, wxCoord, wyCoord, m_sliceZ, 0.0);
        return;
    }

    // Normal Hover detection
    BoundaryEdge hitEdge;
    int hitRow = -1, hitCol = -1, hitBodyId = -1;
    HitType ht = performHitTest(event->pos(), hitEdge, hitRow, hitCol, hitBodyId);

    if (ht == HitType::BoundaryEdge) {
        if (!m_isHoveringEdge || m_hoverEdge.bodyId != hitEdge.bodyId ||
            m_hoverEdge.srcRow != hitEdge.srcRow || m_hoverEdge.srcCol != hitEdge.srcCol ||
            m_hoverEdge.dir != hitEdge.dir) {
            m_isHoveringEdge = true;
            m_hoverEdge = hitEdge;
            m_hoverRow = hitRow;
            m_hoverCol = hitCol;
            update();
        }
        if (hitEdge.dir == CardinalDirection::East || hitEdge.dir == CardinalDirection::West) {
            setCursor(Qt::SizeHorCursor);
        } else {
            setCursor(Qt::SizeVerCursor);
        }
    } else {
        if (m_isHoveringEdge) {
            m_isHoveringEdge = false;
            update();
        }
        if (ht == HitType::BodyCell) {
            setCursor(Qt::ArrowCursor);
        } else if (ht == HitType::EmptyCell) {
            setCursor(Qt::CrossCursor);
        } else {
            setCursor(Qt::ArrowCursor);
        }
    }

    if (m_doc && hitRow >= 1 && hitCol >= 1) {
        double wx = m_doc->model().xd_val(hitCol);
        double wy = m_doc->model().yd_val(hitRow);
        emit cursorStationChanged(hitRow, hitCol, wx, wy, m_sliceZ, 0.0);
    }

    QWidget::mouseMoveEvent(event);
}

void HorizontalSliceView::mouseReleaseEvent(QMouseEvent *event) {
    if (m_dragOp == DragOperation::Pan) {
        m_dragOp = DragOperation::None;
        setCursor(Qt::ArrowCursor);
    } else if (m_dragOp == DragOperation::ExtendOrCollapseEdge) {
        m_dragOp = DragOperation::None;
        if (m_doc) {
            auto pt = m_doc->activeProfileCursor();
            pt.isDragging = false;
            m_doc->setActiveProfileCursor(pt);
        }
        update();
    }
    QWidget::mouseReleaseEvent(event);
}

void HorizontalSliceView::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        BoundaryEdge hitEdge;
        int hitRow = -1, hitCol = -1, hitBodyId = -1;
        HitType ht = performHitTest(event->pos(), hitEdge, hitRow, hitCol, hitBodyId);
        if (ht == HitType::BodyCell && hitBodyId >= 0) {
            emit requestBodyProperties(hitBodyId);
            return;
        } else if (ht == HitType::EmptyCell && m_doc && hitRow >= 1 && hitCol >= 1) {
            // Double click on empty cell inserts a new body at this slice depth
            int newBodyId = m_doc->insertBodyAtRowCol(hitRow, hitCol, m_sliceZ, m_doc->model().default_thickness_val() * 0.5);
            if (newBodyId >= 0) {
                m_doc->setModified(true);
                m_doc->computeAll();
                m_doc->emitModelGeometryChanged();
                emit requestBodyProperties(newBodyId);
                return;
            }
        }
    }
    QWidget::mouseDoubleClickEvent(event);
}

void HorizontalSliceView::wheelEvent(QWheelEvent *event) {
    if (event->modifiers() & Qt::ControlModifier) {
        // Ctrl + Wheel: Zoom in / zoom out
        int numDegrees = event->angleDelta().y() / 8;
        if (numDegrees > 0) {
            zoomIn();
        } else if (numDegrees < 0) {
            zoomOut();
        }
        event->accept();
    } else {
        // Wheel: Step slice elevation Z
        m_wheelDeltaAccumulator += event->angleDelta().y();
        int steps = m_wheelDeltaAccumulator / 120;
        if (steps != 0) {
            stepElevation(steps);
            m_wheelDeltaAccumulator -= steps * 120;
        }
        event->accept();
    }
}

void HorizontalSliceView::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_PageUp) {
        stepElevation(1);
        event->accept();
    } else if (event->key() == Qt::Key_PageDown) {
        stepElevation(-1);
        event->accept();
    } else if (event->key() == Qt::Key_Home) {
        if (m_doc) setSliceElevation(m_doc->model().z_max_val());
        event->accept();
    } else if (event->key() == Qt::Key_End) {
        if (m_doc) setSliceElevation(m_doc->model().z_min_val());
        event->accept();
    } else {
        QWidget::keyPressEvent(event);
    }
}

void HorizontalSliceView::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (m_headerWidget) {
        m_headerWidget->setGeometry(0, 0, width(), 38);
    }
}

void HorizontalSliceView::leaveEvent(QEvent *event) {
    if (m_isHoveringEdge) {
        m_isHoveringEdge = false;
        update();
    }
    QWidget::leaveEvent(event);
}

} // namespace mod3d_gui
