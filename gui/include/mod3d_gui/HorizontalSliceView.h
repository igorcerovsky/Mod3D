#pragma once

#include "mod3d_gui/ModelDocument.h"
#include <QWidget>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QRect>
#include <QPoint>
#include <vector>
#include <limits>

namespace mod3d_gui {

class HorizontalSliceView : public QWidget {
    Q_OBJECT

public:
    enum class CardinalDirection {
        East,
        West,
        North,
        South
    };

    struct BoundaryEdge {
        int bodyId{-1};
        int srcRow{0};
        int srcCol{0};
        int tgtRow{0};
        int tgtCol{0};
        CardinalDirection dir{CardinalDirection::East};
        // World coordinates of edge line segment
        double x1{0.0}, y1{0.0};
        double x2{0.0}, y2{0.0};
    };

    enum class HitType {
        None,
        BoundaryEdge,
        BodyCell,
        EmptyCell
    };

    enum class DragOperation {
        None,
        Pan,
        ExtendOrCollapseEdge
    };

    explicit HorizontalSliceView(ModelDocument *doc, double initialZ = std::numeric_limits<double>::quiet_NaN(), QWidget *parent = nullptr);
    ~HorizontalSliceView() override = default;

    [[nodiscard]] double sliceElevation() const noexcept { return m_sliceZ; }
    void setSliceElevation(double z);
    void stepElevation(int numSteps);

    // Zoom and Pan
    void zoomIn();
    void zoomOut();
    void fitToWindow();

    QRect canvasRect() const;
    double scalePixelsPerMeter() const;

    // Coordinate conversions
    [[nodiscard]] double screenToWorldX(int sx) const;
    [[nodiscard]] double screenToWorldY(int sy) const;
    [[nodiscard]] int worldToScreenX(double wx) const;
    [[nodiscard]] int worldToScreenY(double wy) const;

    // View Toggles
    [[nodiscard]] bool showStationPoints() const noexcept { return m_showStationPoints; }
    void setShowStationPoints(bool show);

    [[nodiscard]] bool showProfiles() const noexcept { return m_showProfiles; }
    void setShowProfiles(bool show);

    [[nodiscard]] bool showCellBorders() const noexcept { return m_showCellBorders; }
    void setShowCellBorders(bool show);

    [[nodiscard]] std::vector<BoundaryEdge> activeBoundaryEdges() const;
    void updateTitle();

signals:
    void sliceElevationChanged(double z);
    void cursorStationChanged(int row, int col, double x, double y, double z, double fldVal);
    void requestBodyProperties(int bodyId);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void setupUi();
    void updateHeaderUi();

    HitType performHitTest(const QPoint &pos, BoundaryEdge &outEdge, int &outRow, int &outCol, int &outBodyId) const;

    // Drawing helpers
    void drawGridAndCells(QPainter &painter, const QRect &canvas);
    void drawBoundaryEdges(QPainter &painter, const QRect &canvas);
    void drawProfilesAndMarkers(QPainter &painter, const QRect &canvas);
    void drawAxes(QPainter &painter, const QRect &canvas);

    ModelDocument *m_doc{nullptr};
    double m_sliceZ{0.0};

    // Margins for coordinate axes
    int m_marginLeft{65};
    int m_marginRight{35};
    int m_marginTop{48}; // Space for top header toolbar
    int m_marginBottom{45};

    double m_worldXMin{0.0};
    double m_worldXMax{10000.0};
    double m_worldYMin{0.0};
    double m_worldYMax{10000.0};

    bool m_showStationPoints{true};
    bool m_showProfiles{true};
    bool m_showCellBorders{true};

    // UI Header controls
    QWidget *m_headerWidget{nullptr};
    QDoubleSpinBox *m_spinElevation{nullptr};
    QSlider *m_sliderElevation{nullptr};
    QLabel *m_labelDepth{nullptr};
    QPushButton *m_btnStepUp{nullptr};
    QPushButton *m_btnStepDown{nullptr};
    QPushButton *m_btnFit{nullptr};

    // Dragging / Interaction State
    DragOperation m_dragOp{DragOperation::None};
    QPoint m_dragStartPos;
    BoundaryEdge m_activeDragEdge;
    int m_lastDragRow{-1};
    int m_lastDragCol{-1};

    // Hover state
    bool m_isHoveringEdge{false};
    BoundaryEdge m_hoverEdge;
    int m_hoverRow{-1};
    int m_hoverCol{-1};

    // Pan state
    QPoint m_lastPanPos;
    int m_wheelDeltaAccumulator{0};
};

} // namespace mod3d_gui
