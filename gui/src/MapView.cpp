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
    connect(m_doc, &ModelDocument::fieldDisplayToggled, this, QOverload<>::of(&QWidget::update));
    connect(m_doc, &ModelDocument::profileChanged, this, [this](int, ProfileOrientation) { update(); });
    connect(m_doc, &ModelDocument::activeProfileCursorChanged, this, [this](const ModelDocument::ProfileCursorPoint &) { update(); });
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

namespace {

enum class MapPaletteType {
    Gravity = 0,
    Magnetics = 1,
    Relief = 2,
    Difference = 3,
    Grayscale = 4,
    Field = 5
};

QColor getPaletteColor(double t, MapPaletteType palette) {
    t = std::clamp(t, 0.0, 1.0);

    struct ColorPeg {
        double pos;
        int r, g, b;
    };

    // Legacy CTBL_FLD: Dark Blue -> Cyan -> Green -> Yellow -> Red -> Magenta -> Light Pink
    static const std::vector<ColorPeg> gravPegs = {
        {0.0 / 6.0, 0, 0, 153},
        {1.0 / 6.0, 0, 204, 255},
        {2.0 / 6.0, 0, 255, 0},
        {3.0 / 6.0, 255, 255, 0},
        {4.0 / 6.0, 255, 0, 0},
        {5.0 / 6.0, 255, 0, 255},
        {6.0 / 6.0, 255, 200, 255}
    };

    // Multi-color field palette (fldPeg): Sky Blue -> Mint -> Green -> Yellow -> Orange -> Red -> Magenta -> Pink
    [[maybe_unused]] static const std::vector<ColorPeg> fldPeg = {
        {0.0 / 7.0, 0, 200, 255},
        {1.0 / 7.0, 50, 235, 170},
        {2.0 / 7.0, 110, 245, 60},
        {3.0 / 7.0, 255, 245, 0},
        {4.0 / 7.0, 255, 140, 0},
        {5.0 / 7.0, 240, 20, 60},
        {6.0 / 7.0, 245, 110, 245},
        {7.0 / 7.0, 255, 235, 255}
    };

    // Magnetics / Difference: Diverging Blue -> White -> Red
    static const std::vector<ColorPeg> diffPegs = {
        {0.0, 0, 0, 160},
        {0.5, 255, 255, 255},
        {1.0, 160, 0, 0}
    };

    // Legacy CTBL_REL: Topographic relief
    static const std::vector<ColorPeg> relPegs = {
        {0.0 / 10.0, 221, 250, 218},
        {1.0 / 10.0, 168, 251, 185},
        {2.0 / 10.0, 27, 206, 96},
        {3.0 / 10.0, 58, 160, 81},
        {4.0 / 10.0, 238, 247, 189},
        {5.0 / 10.0, 236, 230, 168},
        {6.0 / 10.0, 231, 219, 167},
        {7.0 / 10.0, 161, 127, 111},
        {8.0 / 10.0, 108, 74, 76},
        {9.0 / 10.0, 147, 137, 133},
        {10.0 / 10.0, 249, 245, 231}
    };

    const auto &pegs = (palette == MapPaletteType::Magnetics) ? diffPegs
                     : (palette == MapPaletteType::Difference) ? diffPegs
                     : (palette == MapPaletteType::Relief) ? relPegs
                     : (palette == MapPaletteType::Field) ? fldPeg
                     : gravPegs;

    if (t <= pegs.front().pos) return QColor(pegs.front().r, pegs.front().g, pegs.front().b);
    if (t >= pegs.back().pos) return QColor(pegs.back().r, pegs.back().g, pegs.back().b);

    for (size_t i = 0; i < pegs.size() - 1; ++i) {
        if (t >= pegs[i].pos && t <= pegs[i + 1].pos) {
            double span = pegs[i + 1].pos - pegs[i].pos;
            double f = (span > 1e-9) ? (t - pegs[i].pos) / span : 0.0;
            int r = static_cast<int>(std::round(pegs[i].r + f * (pegs[i + 1].r - pegs[i].r)));
            int g = static_cast<int>(std::round(pegs[i].g + f * (pegs[i + 1].g - pegs[i].g)));
            int b = static_cast<int>(std::round(pegs[i].b + f * (pegs[i + 1].b - pegs[i].b)));
            return QColor(std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255));
        }
    }
    return QColor(pegs.back().r, pegs.back().g, pegs.back().b);
}

} // namespace

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

    // 5. Draw Active Profile Point Marker (synced with ProfileView)
    drawActiveProfilePointMarker(painter);

    painter.restore();

    // 5. Draw Canvas Border & Axes
    painter.setPen(QColor(160, 160, 170));
    painter.drawRect(canvas);
    drawAxes(painter);

    // 6. Draw Color Scale Legend with Ticks, Numeric Annotations & Component Title
    drawColorScaleLegend(painter);
}

void MapView::drawBackgroundRaster(QPainter &painter) {
    const mod3d::Grid *grid = nullptr;
    bool isMag = mod3d::is_magnetic_component(m_doc->activeField());
    MapPaletteType palType = isMag ? MapPaletteType::Magnetics : MapPaletteType::Gravity;

    if (m_doc->showDifferenceField()) {
        grid = m_doc->activeDifferenceGrid();
        if (!isMag) {
            palType = MapPaletteType::Difference;
        }
    } else if (m_doc->showMeasuredField() && !m_doc->showModeledField()) {
        grid = m_doc->activeObservedGrid();
    } else {
        grid = m_doc->activeModeledGrid();
        if (!grid || grid->empty()) {
            grid = m_doc->activeObservedGrid();
        }
    }
    if (!grid || grid->empty()) {
        grid = m_doc->activeModeledGrid();
    }
    if (!grid || grid->empty()) {
        grid = m_doc->activeObservedGrid();
    }
    if (!grid || grid->empty()) return;
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
            double t = (maxV > minV) ? std::clamp((v - minV) / (maxV - minV), 0.0, 1.0) : 0.5;
            QColor col = getPaletteColor(t, palType);

            // Invert row for screen Y (Northing is up)
            img.setPixel(static_cast<int>(c), static_cast<int>(rows - 1 - r), col.rgb());
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
    if (!m_showBodyFootprints) return;
    const auto &model = m_doc->model();
    auto boundaries = model.getAllBodyBoundaries();
    if (boundaries.empty()) return;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    for (const auto &seg : boundaries) {
        const auto *body = model.getBody(seg.bodyId);
        if (body && !body->is_visible()) continue;
        QColor col = body ? QColor(body->color().r, body->color().g, body->color().b) : QColor(0, 0, 180);
        painter.setPen(QPen(col, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        int sx0 = worldToScreenX(seg.pt0.x);
        int sy0 = worldToScreenY(seg.pt0.y);
        int sx1 = worldToScreenX(seg.pt1.x);
        int sy1 = worldToScreenY(seg.pt1.y);
        painter.drawLine(sx0, sy0, sx1, sy1);
    }
    painter.restore();
}

void MapView::drawColorScaleLegend(QPainter &painter) {
    const mod3d::Grid *grid = nullptr;
    bool isMag = mod3d::is_magnetic_component(m_doc->activeField());
    MapPaletteType palType = isMag ? MapPaletteType::Magnetics : MapPaletteType::Gravity;

    if (m_doc->showDifferenceField()) {
        grid = m_doc->activeDifferenceGrid();
        if (!isMag) {
            palType = MapPaletteType::Difference;
        }
    } else if (m_doc->showMeasuredField() && !m_doc->showModeledField()) {
        grid = m_doc->activeObservedGrid();
    } else {
        grid = m_doc->activeModeledGrid();
        if (!grid || grid->empty()) {
            grid = m_doc->activeObservedGrid();
        }
    }
    if (!grid || grid->empty()) {
        grid = m_doc->activeModeledGrid();
    }
    if (!grid || grid->empty()) {
        grid = m_doc->activeObservedGrid();
    }
    if (!grid || grid->empty()) return;

    double minV = grid->min();
    double maxV = grid->max();
    if (minV >= maxV) return;

    QRect canvas = mapCanvasRect();
    int barW = 12;
    int barH = std::clamp(canvas.height() - 40, 60, 240);
    int barLeft = canvas.right() + 10;
    int barTop = canvas.top() + (canvas.height() - barH) / 2 - 10;
    int barBottom = barTop + barH;

    painter.save();

    // 1. Draw gradient vertical bar
    for (int y = barTop; y <= barBottom; ++y) {
        double t = (barH > 0) ? static_cast<double>(barBottom - y) / barH : 0.5;
        QColor col = getPaletteColor(t, palType);
        painter.setPen(col);
        painter.drawLine(barLeft, y, barLeft + barW - 1, y);
    }

    // 2. Bar Border
    painter.setPen(QColor(60, 60, 70));
    painter.drawRect(barLeft, barTop, barW, barH);

    // 3. Ticks and Numerical Labels
    painter.setFont(QFont("Arial", 8));
    QFontMetrics fm = painter.fontMetrics();
    int numTicks = 5;
    double span = maxV - minV;
    int decimals = (span < 2.0) ? 2 : ((span < 20.0) ? 1 : 0);

    for (int i = 0; i <= numTicks; ++i) {
        double frac = static_cast<double>(i) / numTicks;
        double val = minV + frac * span;
        int ty = barBottom - static_cast<int>(std::round(frac * barH));

        painter.setPen(QColor(80, 80, 90));
        painter.drawLine(barLeft + barW, ty, barLeft + barW + 4, ty);

        QString valStr = QString::number(val, 'f', decimals);
        int textY = ty + fm.ascent() / 2 - 1;
        painter.drawText(barLeft + barW + 7, textY, valStr);
    }

    // 4. Component Label & Unit below bar
    QString compStr = m_doc->activeFieldComponentName();
    if (palType == MapPaletteType::Difference) {
        compStr = "d" + compStr;
    }
    QString unitStr = m_doc->activeFieldUnit();
    QString titleStr = unitStr.isEmpty() ? compStr : QString("%1\n(%2)").arg(compStr, unitStr);

    painter.setPen(QColor(40, 40, 50));
    QFont boldFont("Arial", 8, QFont::Bold);
    painter.setFont(boldFont);
    QRect titleRect(barLeft - 6, barBottom + 8, 65, 32);
    painter.drawText(titleRect, Qt::AlignLeft | Qt::AlignTop, titleStr);

    painter.restore();
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

void MapView::drawActiveProfilePointMarker(QPainter &painter) {
    if (!m_doc) return;
    const auto &pt = m_doc->activeProfileCursor();
    if (!pt.active) return;

    QRect canvas = mapCanvasRect();
    int sx = worldToScreenX(pt.x);
    int sy = worldToScreenY(pt.y);

    if (sx < canvas.left() - 40 || sx > canvas.right() + 40 ||
        sy < canvas.top() - 40 || sy > canvas.bottom() + 40) {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    QColor bodyCol(235, 40, 40);
    if (pt.bodyId >= 0) {
        if (const auto *b = m_doc->model().getBody(pt.bodyId)) {
            bodyCol = QColor(b->color().r, b->color().g, b->color().b);
        }
    }

    int haloRadius = pt.isDragging ? 13 : 9;
    int coreRadius = pt.isDragging ? 6 : 5;

    // 1. Semi-transparent pulse/glow outer halo
    QColor haloCol = bodyCol;
    haloCol.setAlpha(pt.isDragging ? 110 : 70);
    painter.setPen(QPen(QColor(255, 255, 255, 140), 1.0));
    painter.setBrush(haloCol);
    painter.drawEllipse(QPoint(sx, sy), haloRadius, haloRadius);

    // 2. High-contrast precision crosshairs
    painter.setPen(QPen(QColor(255, 255, 255, 220), 2.0));
    painter.drawLine(sx - haloRadius - 4, sy, sx - coreRadius - 1, sy);
    painter.drawLine(sx + coreRadius + 1, sy, sx + haloRadius + 4, sy);
    painter.drawLine(sx, sy - haloRadius - 4, sx, sy - coreRadius - 1);
    painter.drawLine(sx, sy + coreRadius + 1, sx, sy + haloRadius + 4);

    painter.setPen(QPen(QColor(30, 30, 30, 200), 1.0));
    painter.drawLine(sx - haloRadius - 4, sy, sx - coreRadius - 1, sy);
    painter.drawLine(sx + coreRadius + 1, sy, sx + haloRadius + 4, sy);
    painter.drawLine(sx, sy - haloRadius - 4, sx, sy - coreRadius - 1);
    painter.drawLine(sx, sy + coreRadius + 1, sx, sy + haloRadius + 4);

    // 3. Central disc
    painter.setPen(QPen(Qt::white, 2.0));
    painter.setBrush(bodyCol);
    painter.drawEllipse(QPoint(sx, sy), coreRadius, coreRadius);

    // 4. Center pinpoint
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white);
    painter.drawEllipse(QPoint(sx, sy), 2, 2);

    // 5. Floating information badge
    QString labelText;
    if (pt.bodyId >= 0 && !pt.bodyName.empty()) {
        labelText = QString("%1: Z = %2 m").arg(QString::fromStdString(pt.bodyName)).arg(pt.z, 0, 'f', 1);
    } else if (pt.row >= 0 && pt.col >= 0) {
        labelText = QString("Station [%1, %2] (Z = %3 m)").arg(pt.row + 1).arg(pt.col + 1).arg(pt.z, 0, 'f', 1);
    } else {
        labelText = QString("(X=%1, Y=%2)").arg(pt.x, 0, 'f', 0).arg(pt.y, 0, 'f', 0);
    }
    if (pt.isDragging) {
        labelText = tr("✎ ") + labelText;
    }

    QFont font("Arial", 8, QFont::Bold);
    painter.setFont(font);
    QFontMetrics fm(font);
    int textW = fm.horizontalAdvance(labelText);
    int badgeW = textW + 18;
    int badgeH = fm.height() + 6;

    int badgeX = sx + 12;
    int badgeY = sy - badgeH - 6;
    if (badgeX + badgeW > canvas.right() - 5) {
        badgeX = sx - badgeW - 12;
    }
    if (badgeY < canvas.top() + 5) {
        badgeY = sy + 12;
    }

    QRect badgeRect(badgeX, badgeY, badgeW, badgeH);
    painter.setPen(QPen(QColor(255, 255, 255, 180), 1.0));
    painter.setBrush(QColor(20, 22, 28, 225));
    painter.drawRoundedRect(badgeRect, 4.0, 4.0);

    // Color dot inside badge
    painter.setPen(Qt::NoPen);
    painter.setBrush(bodyCol);
    painter.drawEllipse(QPoint(badgeX + 8, badgeY + badgeH / 2), 3, 3);

    // Badge text
    painter.setPen(Qt::white);
    painter.drawText(badgeX + 15, badgeY + badgeH - 5, labelText);

    painter.restore();
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
            int cols = m_doc->model().getCols();
            int bestCol = 1;
            double bestColDist = 1e30;
            for (int c = 1; c < cols - 1; ++c) {
                double xd = m_doc->model().getXd(c);
                double d = std::abs(xd - wx);
                if (d < bestColDist) {
                    bestColDist = d;
                    bestCol = c;
                }
            }
            auto cur = m_doc->activeProfileCursor();
            cur.col = bestCol;
            m_doc->setActiveProfileCursor(cur);
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
            int rows = m_doc->model().getRows();
            int bestRow = 1;
            double bestRowDist = 1e30;
            for (int r = 1; r < rows - 1; ++r) {
                double yd = m_doc->model().getYd(r);
                double d = std::abs(yd - wy);
                if (d < bestRowDist) {
                    bestRowDist = d;
                    bestRow = r;
                }
            }
            auto cur = m_doc->activeProfileCursor();
            cur.row = bestRow;
            m_doc->setActiveProfileCursor(cur);
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
