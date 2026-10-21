#pragma once

#include "mod3d_gui/ModelDocument.h"
#include "mod3d/Units.h"
#include <QWidget>
#include <QMenu>
#include <QPoint>
#include <QRect>
#include <vector>

namespace mod3d_gui {

class ProfileView : public QWidget {
    Q_OBJECT

public:
    enum class CursorMode {
        Normal = 0,
        OverGridLine,
        OverVertex,
        OverVertexLocked,
        OverBodyLine,
        OverBody,
        OverDivider
    };

    enum class DragOperation {
        None = 0,
        MoveVertex,
        MoveBodyVertical,
        ExtendBodyLateral,
        ResizeDivider
    };

    explicit ProfileView(ModelDocument *doc, QWidget *parent = nullptr);
    explicit ProfileView(ModelDocument *doc, ProfileOrientation orient, int profileIndex = -1, QWidget *parent = nullptr);
    ~ProfileView() override = default;

    // View Navigation
    void zoomIn();
    void zoomOut();
    void fitToWindow();
    void setEqualScale(bool equal);

    // Profile State & Navigation (Independent per View)
    ProfileOrientation profileOrientation() const noexcept { return m_profileOrient; }
    void setProfileOrientation(ProfileOrientation orient);

    int profileIndex() const noexcept { return m_profileIndex; }
    void setProfileIndex(int index);
    void moveProfile(int delta);
    int maxProfileIndex() const noexcept;
    double currentProfileCoordinate() const noexcept;
    int profileColumnLineCount() const noexcept;
    double columnCoordinate(int colIdx) const noexcept;
    void getRowCol(int lineIdx, int &row, int &col) const;

    void jumpToExtreme();
    void jumpToMin();
    void jumpToMax();

    bool showPreviousProfile() const noexcept { return m_showPreviousProfile; }
    void setShowPreviousProfile(bool show);

    bool showNextProfile() const noexcept { return m_showNextProfile; }
    void setShowNextProfile(bool show);

    bool showGridLines() const noexcept { return m_showGridLines; }
    void setShowGridLines(bool show);

    bool showObservationPoints() const noexcept { return m_showObservationPoints; }
    void setShowObservationPoints(bool show);

    bool showRelief() const noexcept { return m_showRelief; }
    void setShowRelief(bool show);

    bool showProfileIntersections() const noexcept { return m_showProfileIntersections; }
    void setShowProfileIntersections(bool show);

    // View-specific Field Curves & Primary Y-Axis Field
    mod3d::FieldComponent mainFieldComponent() const noexcept { return m_mainField; }
    void setMainFieldComponent(mod3d::FieldComponent comp);

    bool showModeledCurve() const noexcept { return m_showModeledCurve; }
    void setShowModeledCurve(bool show);

    bool showMeasuredCurve() const noexcept { return m_showMeasuredCurve && hasMeasuredField(); }
    void setShowMeasuredCurve(bool show);

    bool showDifferenceCurve() const noexcept { return m_showDifferenceCurve && hasMeasuredField(); }
    void setShowDifferenceCurve(bool show);

    bool hasMeasuredField() const;

    bool showSecondaryCurve() const noexcept { return m_showSecondaryCurve; }
    void setShowSecondaryCurve(bool show);

    std::optional<mod3d::FieldComponent> secondaryFieldComponent() const noexcept { return m_secondaryField; }
    void setSecondaryFieldComponent(std::optional<mod3d::FieldComponent> comp);

    bool continuousDrawingMode() const noexcept { return m_continuousDrawingMode; }
    void setContinuousDrawingMode(bool enabled);

    void updateTitle();
    void updateStationPointOnProfileChange();

signals:
    void cursorStationChanged(int row, int col, double x, double y, double z, double fldVal);
    void requestBodyProperties(int bodyId);
    void requestFit1DDialog(int lineIdx, int vertexIdx);
    void profileSettingsChanged(int index, ProfileOrientation orient);
    void requestOpenPerpendicularProfile(ProfileOrientation orient, int profileIndex);
    void displaySettingsChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void leaveEvent(QEvent *event) override;

    // Coordinate Conversion
    double screenToWorldX(int screenX) const;

    template <mod3d::units::UnitType U, typename Rep = double>
        requires mod3d::units::SameDimension<U, mod3d::units::tag::Meters>
    [[nodiscard]] int worldToScreenX(const mod3d::units::Quantity<U, Rep>& worldX) const {
        return worldToScreenX(worldX.template in<mod3d::units::tag::Meters>());
    }
    int worldToScreenX(double worldX) const;

    double screenToWorldZ(int screenZ) const;

    template <mod3d::units::UnitType U, typename Rep = double>
        requires mod3d::units::SameDimension<U, mod3d::units::tag::Meters>
    [[nodiscard]] int worldToScreenZ(const mod3d::units::Quantity<U, Rep>& worldZ) const {
        return worldToScreenZ(worldZ.template in<mod3d::units::tag::Meters>());
    }
    int worldToScreenZ(double worldZ) const;

    [[nodiscard]] mod3d::units::Meters screenToWorldX_m(int screenX) const {
        return mod3d::units::Meters(screenToWorldX(screenX));
    }
    [[nodiscard]] mod3d::units::Meters screenToWorldZ_m(int screenZ) const {
        return mod3d::units::Meters(screenToWorldZ(screenZ));
    }

private:
    // Layout & Geometry
    void updatePaneRects();
    int worldToScreenField(double fldVal, double minFld, double maxFld) const;
    int findClosestLineIndex(int screenX) const;

    // Rendering Sub-Passes
    void drawFieldCurves(QPainter &painter);
    void drawFieldAxes(QPainter &painter, double minFld, double maxFld);
    void drawRmsDrvBadges(QPainter &painter);
    void drawDivider(QPainter &painter);
    void drawGeologicalSection(QPainter &painter);
    void drawGridLines(QPainter &painter);
    void drawProfileIntersections(QPainter &painter);
    void drawReliefAndBottom(QPainter &painter);
    void drawBodies(QPainter &painter);
    void drawGhostBodies(QPainter &painter, int ghostProfileIdx, const QPen &pen);
    void drawSectionAxes(QPainter &painter);

    // Hit-Testing
    CursorMode performHitTest(const QPoint &pos, int &hitLineIdx, int &hitVertexIdx, int &hitBodyId);
    void updateCursorShape(CursorMode mode);

    // Context Menus
    void showBackgroundContextMenu(const QPoint &globalPos);
    void showGridLineContextMenu(const QPoint &globalPos, int lineIdx, double targetZ);
    void showBodyContextMenu(const QPoint &globalPos, int bodyId, int lineIdx, int vertexIdx);
    void showVertexContextMenu(const QPoint &globalPos, int lineIdx, int vertexIdx);

    ModelDocument *m_doc;

    // Profile View State (Independent per view)
    ProfileOrientation m_profileOrient{ProfileOrientation::EastWest};
    int m_profileIndex{0};
    bool m_showPreviousProfile{false};
    bool m_showNextProfile{false};
    bool m_showGridLines{true};

    // Dual-Pane Layout
    float m_dividerRatio{0.32f}; // ratio of upper field pane height to total height
    QRect m_rectFieldPane;
    QRect m_rectDivider;
    QRect m_rectSectionPane;

    // Axis Margins (in device pixels)
    int m_marginLeft{65};
    int m_marginRight{25};
    int m_marginTop{20};
    int m_marginBottom{40};

    // View Transformation / Zoom
    double m_worldXMin{0.0};
    double m_worldXMax{10000.0};
    double m_worldZMin{-4000.0};
    double m_worldZMax{1000.0};
    bool m_equalScale{false};

    // Hit-Testing State
    CursorMode m_currentCursorMode{CursorMode::Normal};
    int m_hoverLineIdx{-1};
    int m_hoverVertexIdx{-1};
    int m_hoverBodyId{-1};

    // Interactive Drag State
    DragOperation m_dragOp{DragOperation::None};
    QPoint m_dragStartPos;
    int m_dragLineIdx{-1};
    int m_dragVertexIdx{-1};
    int m_dragBodyId{-1};
    double m_dragInitialVertexZ{0.0};
    double m_dragLastMouseZ{0.0};

    int m_wheelDeltaAccumulator{0};

    // View-specific Field Curves & Primary Y-Axis Field
    mod3d::FieldComponent m_mainField{mod3d::FieldComponent::GZ};
    bool m_showModeledCurve{true};
    bool m_showMeasuredCurve{false};
    bool m_showDifferenceCurve{false};
    bool m_showSecondaryCurve{false};
    std::optional<mod3d::FieldComponent> m_secondaryField{std::nullopt};
    bool m_continuousDrawingMode{false};
    bool m_showObservationPoints{true};
    bool m_showRelief{true};
    bool m_showProfileIntersections{true};
};

} // namespace mod3d_gui
