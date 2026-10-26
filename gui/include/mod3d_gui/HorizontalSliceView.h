#pragma once

#include "mod3d_gui/ModelDocument.h"
#include <QWidget>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QRect>
#include <QComboBox>
#include <QCheckBox>
#include <QImage>
#include <QMdiArea>
#include <QContextMenuEvent>
#include <vector>
#include <set>
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
        CellCenterPoint,
        BodyCell,
        EmptyCell
    };

    enum class DragOperation {
        None,
        Pan,
        ExtendOrCollapseEdge,
        DrawBody
    };

    explicit HorizontalSliceView(ModelDocument *doc, double initialZ = std::numeric_limits<double>::quiet_NaN(), QWidget *parent = nullptr);
    ~HorizontalSliceView() override = default;

    [[nodiscard]] double sliceElevation() const noexcept { return m_sliceZ; }
    void setSliceElevation(double z);
    void stepElevation(int numSteps);

    // Active Body for Drawing Mode
    [[nodiscard]] int activeBodyId() const;
    void setActiveBodyId(int bodyId);
    void refreshBodyList();

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

    enum class SliceRasterMode {
        None = 0,
        Modeled = 1,
        Observed = 2,
        Difference = 3
    };

    [[nodiscard]] SliceRasterMode rasterMode() const noexcept { return m_rasterMode; }
    void setRasterMode(SliceRasterMode mode);

    [[nodiscard]] mod3d::FieldComponent fieldComponent() const noexcept { return m_fieldComponent; }
    void setFieldComponent(mod3d::FieldComponent comp);

    [[nodiscard]] bool hasMeasuredField() const;

    [[nodiscard]] double cachedRasterX0() const noexcept { return m_cachedRasterX0; }
    [[nodiscard]] double cachedRasterY0() const noexcept { return m_cachedRasterY0; }
    [[nodiscard]] double cachedRasterXMax() const noexcept { return m_cachedRasterXMax; }
    [[nodiscard]] double cachedRasterYMax() const noexcept { return m_cachedRasterYMax; }

    // View Toggles
    [[nodiscard]] bool showStationPoints() const noexcept { return m_showStationPoints; }
    void setShowStationPoints(bool show);

    [[nodiscard]] bool showProfiles() const noexcept { return m_showProfiles; }
    void setShowProfiles(bool show);

    [[nodiscard]] bool showCellBorders() const noexcept { return m_showCellBorders; }
    void setShowCellBorders(bool show);

    [[nodiscard]] bool showBodyOutlines() const noexcept { return m_showBodyOutlines; }
    void setShowBodyOutlines(bool show);

    // Profile Views Synchronization
    [[nodiscard]] bool syncProfileViews() const noexcept { return m_syncProfileViews; }
    void setSyncProfileViews(bool sync);
    void synchronizeProfileViewsToPosition(int row, int col, double wx, double wy);
    [[nodiscard]] QCheckBox *syncProfilesCheckBox() const noexcept { return m_chkSyncProfileViews; }

    [[nodiscard]] std::vector<BoundaryEdge> activeBoundaryEdges() const;
    void updateTitle();

    struct OpenProfileInfo {
        ProfileOrientation orientation{ProfileOrientation::EastWest};
        int index{0};
        double coordinate{0.0};
        bool isActive{false};
    };

    struct PlaneIntersection {
        double x{0.0};
        double y{0.0};
        double z{0.0};
        int ewIndex{0};
        int snIndex{0};
        bool isActive{false};
    };

    [[nodiscard]] std::vector<OpenProfileInfo> collectOpenProfiles() const;
    [[nodiscard]] std::vector<PlaneIntersection> planeIntersections() const;
    void setCustomOpenProfiles(const std::vector<OpenProfileInfo> &profiles);
    void clearCustomOpenProfiles();

signals:
    void sliceElevationChanged(double z);
    void displaySettingsChanged();
    void syncProfileViewsChanged(bool sync);
    void profilePositionSyncRequested(int row, int col, double x, double y);
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
    void contextMenuEvent(QContextMenuEvent *event) override;

    HitType performHitTest(const QPoint &pos, BoundaryEdge &outEdge, int &outRow, int &outCol, int &outBodyId) const;

private:
    void setupUi();
    void updateHeaderUi();

    // Drawing helpers
    void drawBackgroundRaster(QPainter &painter, const QRect &canvas);
    void drawColorScaleLegend(QPainter &painter, const QRect &canvas);
    void drawGridAndCells(QPainter &painter, const QRect &canvas);
    void drawBodyOutlines(QPainter &painter, const QRect &canvas);
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
    bool m_showBodyOutlines{true};

    mod3d::FieldComponent m_fieldComponent{mod3d::FieldComponent::GZ};
    SliceRasterMode m_rasterMode{SliceRasterMode::None};
    bool m_rasterDirty{true};
    QImage m_cachedRasterImage;
    double m_cachedRasterX0{0.0};
    double m_cachedRasterY0{0.0};
    double m_cachedRasterXMax{0.0};
    double m_cachedRasterYMax{0.0};

    // UI Header controls
    QWidget *m_headerWidget{nullptr};
    QDoubleSpinBox *m_spinElevation{nullptr};
    QSlider *m_sliderElevation{nullptr};
    QLabel *m_labelDepth{nullptr};
    QPushButton *m_btnStepUp{nullptr};
    QPushButton *m_btnStepDown{nullptr};
    QComboBox *m_comboActiveBody{nullptr};
    QCheckBox *m_chkSyncProfileViews{nullptr};
    QPushButton *m_btnFit{nullptr};

    // Dragging / Interaction State
    DragOperation m_dragOp{DragOperation::None};
    QPoint m_dragStartPos;
    BoundaryEdge m_activeDragEdge;
    int m_lastDragRow{-1};
    int m_lastDragCol{-1};

    // Drawing Mode State (clicking/dragging from middle point of cell)
    int m_drawBodyId{-1};
    int m_drawStartRow{-1};
    int m_drawStartCol{-1};
    int m_drawLastRow{-1};
    int m_drawLastCol{-1};
    bool m_drawPendingStartCell{false};
    std::set<std::pair<int, int>> m_drawVisitedCells;

    // Hover state
    bool m_isHoveringEdge{false};
    BoundaryEdge m_hoverEdge;
    int m_hoverRow{-1};
    int m_hoverCol{-1};

    bool m_isHoveringCenter{false};
    int m_hoverCenterRow{-1};
    int m_hoverCenterCol{-1};

    // Pan state
    QPoint m_lastPanPos;
    int m_wheelDeltaAccumulator{0};

    // Open Profile Views & Plane Intersections
    QMdiArea *findMdiArea() const;
    std::vector<OpenProfileInfo> m_customProfiles;
    int m_hoverIntersectionIdx{-1};
    bool m_syncProfileViews{false};
};

} // namespace mod3d_gui
