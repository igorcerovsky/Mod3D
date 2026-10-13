#pragma once

#include "mod3d_gui/ModelDocument.h"
#include <QWidget>
#include <QPoint>
#include <QRect>

namespace mod3d_gui {

class MapView : public QWidget {
    Q_OBJECT

public:
    explicit MapView(ModelDocument *doc, QWidget *parent = nullptr);
    ~MapView() override = default;

    void zoomIn();
    void zoomOut();
    void fitToWindow();

    QRect mapCanvasRect() const;
    double scalePixelsPerMeter() const;

signals:
    void cursorStationChanged(int row, int col, double x, double y, double z, double fldVal);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    double screenToWorldX(int sx) const;
    int worldToScreenX(double wx) const;
    double screenToWorldY(int sy) const;
    int worldToScreenY(double wy) const;

    void drawBackgroundRaster(QPainter &painter);
    void drawStationPoints(QPainter &painter);
    void drawBodyOutlines(QPainter &painter);
    void drawActiveProfileTracker(QPainter &painter);
    void drawAxes(QPainter &painter);

    ModelDocument *m_doc;

    int m_marginLeft{65};
    int m_marginRight{25};
    int m_marginTop{25};
    int m_marginBottom{45};

    double m_worldXMin{0.0};
    double m_worldXMax{10000.0};
    double m_worldYMin{0.0};
    double m_worldYMax{10000.0};

    bool m_draggingProfileLine{false};
    bool m_panning{false};
    QPoint m_lastPanPos;
    int m_wheelDeltaAccumulator{0};
};

} // namespace mod3d_gui
