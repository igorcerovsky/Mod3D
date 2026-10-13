#include "mod3d_gui/MapView.h"
#include "mod3d_gui/ProfileView.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <algorithm>
#include <cmath>

namespace mod3d_gui {

MapView::MapView(ModelDocument *doc, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
{
    setMouseTracking(true);

    connect(m_doc, &ModelDocument::modelGeometryChanged, this, QOverload<>::of(&QWidget::update));
    connect(m_doc, &ModelDocument::computationFinished, this, QOverload<>::of(&QWidget::update));
    connect(m_doc, &ModelDocument::activeFieldChanged, this, [this](mod3d::FieldComponent) { update(); });
    connect(m_doc, &ModelDocument::profileChanged, this, [this](int, ProfileOrientation) { update(); });
    connect(m_doc, &ModelDocument::documentReset, this, [this]() {
        fitToWindow();
        update();
    });

    fitToWindow();
}

void MapView::fitToWindow() {
    if (!m_doc) return;
    const auto &m = m_doc->model();
    m_worldXMin = m.getXMin();
    m_worldXMax = m.getXMax();
    m_worldYMin = m.getYMin();
    m_worldYMax = m.getYMax();
    update();
}

void MapView::zoomIn() {
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

void MapView::zoomOut() {
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

QRect MapView::mapCanvasRect() const {
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

    // Equal scale: identical pixels per meter on both horizontal and vertical axes
    double scaleX = static_cast<double>(availW) / spanX;
    double scaleY = static_cast<double>(availH) / spanY;
    double scale = std::min(scaleX, scaleY);

    int actualW = std::max(1, static_cast<int>(std::round(scale * spanX)));
    int actualH = std::max(1, static_cast<int>(std::round(scale * spanY)));

    // Center map canvas in the available area between margins
    int offX = m_marginLeft + (availW - actualW) / 2;
    int offY = m_marginTop + (availH - actualH) / 2;

    return QRect(offX, offY, actualW, actualH);
}

double MapView::scalePixelsPerMeter() const {
    QRect canvas = mapCanvasRect();
    double spanX = m_worldXMax - m_worldXMin;
    if (spanX <= 0.0) return 1.0;
    return static_cast<double>(canvas.width()) / spanX;
}

double MapView::screenToWorldX(int sx) const {
    QRect canvas = mapCanvasRect();
    if (canvas.width() <= 0) return m_worldXMin;
    double t = static_cast<double>(sx - canvas.left()) / canvas.width();
    return m_worldXMin + t * (m_worldXMax - m_worldXMin);
}

int MapView::worldToScreenX(double wx) const {
    QRect canvas = mapCanvasRect();
    double spanX = m_worldXMax - m_worldXMin;
    if (spanX <= 0.0) return canvas.left();
    double t = (wx - m_worldXMin) / spanX;
    return canvas.left() + static_cast<int>(std::round(t * canvas.width()));
}

double MapView::screenToWorldY(int sy) const {
    QRect canvas = mapCanvasRect();
    if (canvas.height() <= 0) return m_worldYMin;
    double t = static_cast<double>(sy - canvas.top()) / canvas.height();
    return m_worldYMax - t * (m_worldYMax - m_worldYMin);
}

int MapView::worldToScreenY(double wy) const {
    QRect canvas = mapCanvasRect();
    double spanY = m_worldYMax - m_worldYMin;
    if (spanY <= 0.0) return canvas.top();
    double t = (m_worldYMax - wy) / spanY;
    return canvas.top() + static_cast<int>(std::round(t * canvas.height()));
}

void MapView::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Background outside map canvas
    painter.fillRect(rect(), QColor(245, 245, 248));

    QRect canvas = mapCanvasRect();
    painter.fillRect(canvas, Qt::white);

    // Clip rendering strictly to the map canvas
    painter.save();
    painter.setClipRect(canvas);

    // 1. Draw Field / DEM Raster
    drawBackgroundRaster(painter);

    // 2. Draw Station Points
    drawStationPoints(painter);

    // 3. Draw Body Outlines
    drawBodyOutlines(painter);

    // 4. Draw Active Profile Tracker Line
    drawActiveProfileTracker(painter);

    painter.restore();

    // 5. Draw Canvas Border & Axes
    painter.setPen(QColor(160, 160, 170));
    painter.drawRect(canvas);
    drawAxes(painter);
}

void MapView::drawBackgroundRaster(QPainter &painter) {
    const auto *grid = m_doc->activeModeledGrid();
    if (!grid) return;
    size_t rows = grid->rows();
    size_t cols = grid->cols();
    if (rows < 2 || cols < 2) return;

    double minV = grid->min();
    double maxV = grid->max();
    if (minV >= maxV) return;

    QImage img(static_cast<int>(cols), static_cast<int>(rows), QImage::Format_RGB32);

    for (size_t r = 0; r < rows; ++r) {
        for (size_t c = 0; c < cols; ++c) {
            double v = (*grid)(r, c);
            double t = std::clamp((v - minV) / (maxV - minV), 0.0, 1.0);

            // Jet-like rainbow palette
            int red = static_cast<int>(255 * std::clamp(1.5 - std::abs(4.0 * t - 3.0), 0.0, 1.0));
            int green = static_cast<int>(255 * std::clamp(1.5 - std::abs(4.0 * t - 2.0), 0.0, 1.0));
            int blue = static_cast<int>(255 * std::clamp(1.5 - std::abs(4.0 * t - 1.0), 0.0, 1.0));

            // Invert row for screen Y (Northing is up)
            img.setPixel(static_cast<int>(c), static_cast<int>(rows - 1 - r), qRgb(red, green, blue));
        }
    }

    double gX0 = grid->x0();
    double gX1 = grid->x0() + (cols - 1) * grid->x_size();
    double gY0 = grid->y0();
    double gY1 = grid->y0() + (rows - 1) * grid->y_size();

    int left = worldToScreenX(gX0);
    int right = worldToScreenX(gX1);
    int top = worldToScreenY(gY1);
    int bottom = worldToScreenY(gY0);

    QRect targetRect(left, top, right - left, bottom - top);
    painter.setOpacity(0.65);
    painter.drawImage(targetRect, img);
    painter.setOpacity(1.0);
}

void MapView::drawStationPoints(QPainter &painter) {
    int rows = m_doc->model().getRows();
    int cols = m_doc->model().getCols();

    painter.setPen(QColor(140, 140, 150));
    for (int r = 1; r <= rows; ++r) {
        double y = m_doc->model().getYd(r);
        int sy = worldToScreenY(y);
        for (int c = 1; c <= cols; ++c) {
            double x = m_doc->model().getXd(c);
            int sx = worldToScreenX(x);
            painter.drawPoint(sx, sy);
        }
    }
}

void MapView::drawBodyOutlines(QPainter &painter) {
    const auto &model = m_doc->model();
    int rows = model.getRows();
    int cols = model.getCols();

    painter.setPen(QPen(QColor(80, 50, 40), 2.0));
    for (const auto &body : model.getBodies()) {
        int bId = body->id();
        // Collect points where body exists
        for (int r = 1; r <= rows; ++r) {
            for (int c = 1; c <= cols; ++c) {
                if (model.getBodyIndex(bId, r, c) >= 0) {
                    double x = model.getXd(c);
                    double y = model.getYd(r);
                    int sx = worldToScreenX(x);
                    int sy = worldToScreenY(y);
                    painter.drawRect(sx - 2, sy - 2, 4, 4);
                }
            }
        }
    }
}

void MapView::drawActiveProfileTracker(QPainter &painter) {
    QMdiArea *mdi = nullptr;
    QWidget *p = parentWidget();
    while (p) {
        if (auto *m = qobject_cast<QMdiArea*>(p)) {
            mdi = m;
            break;
        }
        p = p->parentWidget();
    }

    ProfileView *activePv = nullptr;
    std::vector<ProfileView*> otherViews;
    if (mdi) {
        if (auto *activeSub = mdi->activeSubWindow()) {
            activePv = qobject_cast<ProfileView*>(activeSub->widget());
        }
        for (auto *sub : mdi->subWindowList()) {
            if (auto *pv = qobject_cast<ProfileView*>(sub->widget())) {
                if (pv != activePv) {
                    otherViews.push_back(pv);
                }
            }
        }
    }

    // 1. Draw inactive open profile views (e.g. perpendicular profiles) in secondary styling
    for (auto *pv : otherViews) {
        double c = pv->currentProfileCoordinate();
        bool ew = (pv->profileOrientation() == ProfileOrientation::EastWest);
        painter.setPen(QPen(QColor(220, 110, 20, 180), 1.5, Qt::DashLine));
        if (ew) {
            int sy = worldToScreenY(c);
            int leftX = worldToScreenX(m_worldXMin);
            int rightX = worldToScreenX(m_worldXMax);
            painter.drawLine(leftX, sy, rightX, sy);
            painter.setPen(QColor(190, 90, 10));
            painter.drawText(leftX + 10, sy - 4, QString("Prf %1 [E-W]").arg(pv->profileIndex() + 1));
        } else {
            int sx = worldToScreenX(c);
            int topY = worldToScreenY(m_worldYMax);
            int botY = worldToScreenY(m_worldYMin);
            painter.drawLine(sx, botY, sx, topY);
            painter.setPen(QColor(190, 90, 10));
            painter.drawText(sx + 4, botY - 8, QString("Prf %1 [S-N]").arg(pv->profileIndex() + 1));
        }
    }

    // 2. Draw active profile tracker in primary bold style
    double coord = activePv ? activePv->currentProfileCoordinate() : m_doc->currentProfileCoordinate();
    bool isEW = activePv ? (activePv->profileOrientation() == ProfileOrientation::EastWest)
                         : (m_doc->profileOrientation() == ProfileOrientation::EastWest);
    int activeIdx = activePv ? activePv->profileIndex() : m_doc->profileIndex();

    painter.setPen(QPen(QColor(220, 20, 20), 2.5, Qt::SolidLine));

    if (isEW) {
        int sy = worldToScreenY(coord);
        int leftX = worldToScreenX(m_worldXMin);
        int rightX = worldToScreenX(m_worldXMax);
        painter.drawLine(leftX, sy, rightX, sy);

        // Direction arrow pointing East
        painter.drawLine(rightX - 10, sy - 5, rightX, sy);
        painter.drawLine(rightX - 10, sy + 5, rightX, sy);

        // Label
        painter.setPen(QColor(180, 0, 0));
        painter.drawText(leftX + 10, sy - 5, QString("Profile %1 (Y=%2 m)").arg(activeIdx + 1).arg(coord, 0, 'f', 0));
    } else {
        int sx = worldToScreenX(coord);
        int topY = worldToScreenY(m_worldYMax);
        int botY = worldToScreenY(m_worldYMin);
        painter.drawLine(sx, botY, sx, topY);

        // Direction arrow pointing North
        painter.drawLine(sx - 5, topY + 10, sx, topY);
        painter.drawLine(sx + 5, topY + 10, sx, topY);

        // Label
        painter.setPen(QColor(180, 0, 0));
        painter.drawText(sx + 5, botY - 10, QString("Profile %1 (X=%2 m)").arg(activeIdx + 1).arg(coord, 0, 'f', 0));
    }
}

void MapView::drawAxes(QPainter &painter) {
    QRect canvas = mapCanvasRect();

    painter.setPen(QColor(60, 60, 60));
    QFont font = painter.font();
    font.setPointSize(9);
    painter.setFont(font);

    // Northing Y-axis (vertical along left edge of map canvas)
    int nYTicks = 5;
    for (int i = 0; i <= nYTicks; ++i) {
        double y = m_worldYMin + i * (m_worldYMax - m_worldYMin) / nYTicks;
        int sy = worldToScreenY(y);
        painter.drawLine(canvas.left() - 5, sy, canvas.left(), sy);
        QString label = QString::number(y, 'f', 0);
        painter.drawText(canvas.left() - m_marginLeft, sy - 8, m_marginLeft - 8, 16, Qt::AlignRight | Qt::AlignVCenter, label);
    }

    // Easting X-axis (horizontal along bottom edge of map canvas)
    int nXTicks = 5;
    int axisY = canvas.bottom();
    for (int i = 0; i <= nXTicks; ++i) {
        double x = m_worldXMin + i * (m_worldXMax - m_worldXMin) / nXTicks;
        int sx = worldToScreenX(x);
        painter.drawLine(sx, axisY, sx, axisY + 5);
        QString label = QString::number(x, 'f', 0);
        painter.drawText(sx - 35, axisY + 8, 70, 16, Qt::AlignCenter, label);
    }

    // Axis titles
    painter.drawText(canvas.center().x() - 50, axisY + 26, 100, 16, Qt::AlignCenter, "X (m) [East]");
    painter.save();
    painter.translate(canvas.left() - m_marginLeft + 15, canvas.center().y());
    painter.rotate(-90);
    painter.drawText(-50, -8, 100, 16, Qt::AlignCenter, "Y (m) [North]");
    painter.restore();
}

void MapView::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_draggingProfileLine = true;
        mouseMoveEvent(event);
    } else if (event->button() == Qt::RightButton || event->button() == Qt::MiddleButton) {
        m_panning = true;
        m_lastPanPos = event->pos();
    }
    QWidget::mousePressEvent(event);
}

void MapView::mouseMoveEvent(QMouseEvent *event) {
    double wx = screenToWorldX(event->pos().x());
    double wy = screenToWorldY(event->pos().y());

    if (m_panning) {
        QPoint delta = event->pos() - m_lastPanPos;
        m_lastPanPos = event->pos();
        double scale = scalePixelsPerMeter();
        if (scale > 0.0) {
            double dx = delta.x() / scale;
            double dy = delta.y() / scale;
            m_worldXMin -= dx;
            m_worldXMax -= dx;
            m_worldYMin += dy;
            m_worldYMax += dy;
            update();
        }
        return;
    }

    if (m_draggingProfileLine) {
        if (m_doc->profileOrientation() == ProfileOrientation::EastWest) {
            // Find closest row
            int rows = m_doc->model().getRows();
            int bestRow = 0;
            double bestDist = 1e30;
            for (int r = 1; r <= rows; ++r) {
                double yd = m_doc->model().getYd(r);
                double d = std::abs(yd - wy);
                if (d < bestDist) {
                    bestDist = d;
                    bestRow = r - 1;
                }
            }
            m_doc->setProfileIndex(bestRow);
        } else {
            // Find closest col
            int cols = m_doc->model().getCols();
            int bestCol = 0;
            double bestDist = 1e30;
            for (int c = 1; c <= cols; ++c) {
                double xd = m_doc->model().getXd(c);
                double d = std::abs(xd - wx);
                if (d < bestDist) {
                    bestDist = d;
                    bestCol = c - 1;
                }
            }
            m_doc->setProfileIndex(bestCol);
        }
    }

    emit cursorStationChanged(0, 0, wx, wy, 0.0, 0.0);
    QWidget::mouseMoveEvent(event);
}

void MapView::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_draggingProfileLine = false;
    } else if (event->button() == Qt::RightButton || event->button() == Qt::MiddleButton) {
        m_panning = false;
    }
    QWidget::mouseReleaseEvent(event);
}

void MapView::wheelEvent(QWheelEvent *event) {
    if (event->modifiers() & Qt::ControlModifier) {
        double factor = (event->angleDelta().y() > 0) ? 0.8 : 1.25;
        QPoint mousePos = event->position().toPoint();
        double cursorX = screenToWorldX(mousePos.x());
        double cursorY = screenToWorldY(mousePos.y());

        m_worldXMin = cursorX + (m_worldXMin - cursorX) * factor;
        m_worldXMax = cursorX + (m_worldXMax - cursorX) * factor;
        m_worldYMin = cursorY + (m_worldYMin - cursorY) * factor;
        m_worldYMax = cursorY + (m_worldYMax - cursorY) * factor;
        update();
    } else {
        int delta = event->angleDelta().y();
        if (delta == 0) {
            delta = event->angleDelta().x();
        }
        if ((m_wheelDeltaAccumulator > 0 && delta < 0) || (m_wheelDeltaAccumulator < 0 && delta > 0)) {
            m_wheelDeltaAccumulator = 0;
        }
        m_wheelDeltaAccumulator += delta;
        while (m_wheelDeltaAccumulator >= 120) {
            m_doc->moveProfile(1);
            m_wheelDeltaAccumulator -= 120;
        }
        while (m_wheelDeltaAccumulator <= -120) {
            m_doc->moveProfile(-1);
            m_wheelDeltaAccumulator += 120;
        }
    }
    event->accept();
}

} // namespace mod3d_gui
