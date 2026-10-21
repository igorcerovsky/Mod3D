#pragma once

#include "mod3d_gui/ModelDocument.h"
#include "mod3d/Units.h"
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

    // Coordinate Mapping (supports strongly-typed units and raw doubles)
    template <mod3d::units::UnitType U, typename Rep = double>
        requires mod3d::units::SameDimension<U, mod3d::units::tag::Meters>
    [[nodiscard]] int worldToScreenX(const mod3d::units::Quantity<U, Rep>& wx) const {
        return worldToScreenX(wx.template in<mod3d::units::tag::Meters>());
    }
    [[nodiscard]] int worldToScreenX(double wx) const;

    template <mod3d::units::UnitType U, typename Rep = double>
        requires mod3d::units::SameDimension<U, mod3d::units::tag::Meters>
    [[nodiscard]] int worldToScreenY(const mod3d::units::Quantity<U, Rep>& wy) const {
        return worldToScreenY(wy.template in<mod3d::units::tag::Meters>());
    }
    [[nodiscard]] int worldToScreenY(double wy) const;

    template <mod3d::units::UnitType Ux, mod3d::units::UnitType Uy, typename Rep = double>
        requires (mod3d::units::SameDimension<Ux, mod3d::units::tag::Meters> &&
                  mod3d::units::SameDimension<Uy, mod3d::units::tag::Meters>)
    [[nodiscard]] QPoint worldToScreen(const mod3d::units::Quantity<Ux, Rep>& wx,
                                       const mod3d::units::Quantity<Uy, Rep>& wy) const {
        return QPoint(worldToScreenX(wx), worldToScreenY(wy));
    }
    [[nodiscard]] QPoint worldToScreen(double wx, double wy) const {
        return QPoint(worldToScreenX(wx), worldToScreenY(wy));
    }
    [[nodiscard]] QPoint worldToScreen(const mod3d::Point3D &pt) const {
        return QPoint(worldToScreenX(pt.x), worldToScreenY(pt.y));
    }
    [[nodiscard]] QPoint worldToScreen(const mod3d::Position3D &pos) const {
        return QPoint(worldToScreenX(pos.x), worldToScreenY(pos.y));
    }

    [[nodiscard]] double screenToWorldX(int sx) const;
    [[nodiscard]] double screenToWorldY(int sy) const;
    [[nodiscard]] mod3d::units::Meters screenToWorldX_m(int sx) const {
        return mod3d::units::Meters(screenToWorldX(sx));
    }
    [[nodiscard]] mod3d::units::Meters screenToWorldY_m(int sy) const {
        return mod3d::units::Meters(screenToWorldY(sy));
    }

    enum class MapRasterMode {
        Modeled = 0,
        Observed = 1,
        Difference = 2,
        Relief = 3,
        None = 4
    };

    MapRasterMode rasterMode() const noexcept { return m_rasterMode; }
    void setRasterMode(MapRasterMode mode);

    mod3d::FieldComponent fieldComponent() const noexcept { return m_fieldComponent; }
    void setFieldComponent(mod3d::FieldComponent comp);

    bool hasMeasuredField() const;

    bool showStationPoints() const noexcept { return m_showStationPoints; }
    void setShowStationPoints(bool show);

    bool showProfiles() const noexcept { return m_showProfiles; }
    void setShowProfiles(bool show);

    bool showBodyFootprints() const noexcept { return m_showBodyFootprints; }
    void setShowBodyFootprints(bool show);

    bool showReliefContours() const noexcept { return m_showReliefContours; }
    void setShowReliefContours(bool show);

signals:
    void cursorStationChanged(int row, int col, double x, double y, double z, double fldVal);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:

    void drawBackgroundRaster(QPainter &painter);
    void drawReliefContours(QPainter &painter);
    void drawStationPoints(QPainter &painter);
    void drawBodyOutlines(QPainter &painter);
    void drawActiveProfileTracker(QPainter &painter);
    void drawActiveProfilePointMarker(QPainter &painter);
    void drawAxes(QPainter &painter);
    void drawColorScaleLegend(QPainter &painter);

    ModelDocument *m_doc;

    int m_marginLeft{65};
    int m_marginRight{75};
    int m_marginTop{25};
    int m_marginBottom{45};

    double m_worldXMin{0.0};
    double m_worldXMax{10000.0};
    double m_worldYMin{0.0};
    double m_worldYMax{10000.0};

    MapRasterMode m_rasterMode{MapRasterMode::Modeled};
    mod3d::FieldComponent m_fieldComponent{mod3d::FieldComponent::GZ};
    bool m_showStationPoints{true};
    bool m_showProfiles{true};
    bool m_showBodyFootprints{true};
    bool m_showReliefContours{true};
    bool m_draggingProfileLine{false};
    bool m_panning{false};
    QPoint m_lastPanPos;
    int m_wheelDeltaAccumulator{0};
};

} // namespace mod3d_gui
