#pragma once

#include "mod3d/Project.h"
#include "mod3d/Model.h"
#include "mod3d/Observation.h"
#include "mod3d/Grid.h"
#include <QObject>
#include <QString>
#include <QUndoStack>
#include <memory>
#include <vector>
#include <limits>
#include <cmath>

namespace mod3d_gui {

enum class ProfileOrientation {
    EastWest = 0,   // Legacy PRF_HRZ: profile slices along Y (row fixed), X is cross-section coordinate
    SouthNorth = 1  // Legacy PRF_VRT: profile slices along X (col fixed), Y is cross-section coordinate
};

class ModelDocument : public QObject {
    Q_OBJECT

public:
    explicit ModelDocument(QObject *parent = nullptr);
    ~ModelDocument() override = default;

    // Document State & Core Access
    mod3d::Project &project() noexcept { return m_project; }
    const mod3d::Project &project() const noexcept { return m_project; }

    mod3d::Model &model() noexcept { return m_project.model(); }
    const mod3d::Model &model() const noexcept { return m_project.model(); }

    mod3d::ObservationSpace &observation() noexcept { return m_project.observation(); }
    const mod3d::ObservationSpace &observation() const noexcept { return m_project.observation(); }

    const mod3d::Grid *modeledGrid(mod3d::FieldComponent comp) const { return m_project.observation().modeled_grid(comp); }
    const mod3d::Grid *observedGrid(mod3d::FieldComponent comp) const { return m_project.observation().observed_grid(comp); }
    const mod3d::Grid *differenceGrid(mod3d::FieldComponent comp) const { return m_project.observation().difference_grid(comp); }

    const mod3d::Grid *activeModeledGrid() const { return modeledGrid(m_activeField); }
    const mod3d::Grid *activeObservedGrid() const {
        const auto *g = observedGrid(m_activeField);
        if ((!g || g->empty()) && m_activeField == mod3d::FieldComponent::GZ) {
            const auto *gt = observedGrid(mod3d::FieldComponent::G_TOT);
            if (gt && !gt->empty()) return gt;
        } else if ((!g || g->empty()) && m_activeField == mod3d::FieldComponent::G_TOT) {
            const auto *gz = observedGrid(mod3d::FieldComponent::GZ);
            if (gz && !gz->empty()) return gz;
        } else if ((!g || g->empty()) && m_activeField == mod3d::FieldComponent::DELTA_T) {
            const auto *mz = observedGrid(mod3d::FieldComponent::MZ);
            if (mz && !mz->empty()) return mz;
        } else if ((!g || g->empty()) && m_activeField == mod3d::FieldComponent::MZ) {
            const auto *dt = observedGrid(mod3d::FieldComponent::DELTA_T);
            if (dt && !dt->empty()) return dt;
        }
        return g;
    }
    const mod3d::Grid *activeDifferenceGrid() const { return differenceGrid(m_activeField); }

    bool hasObservedGrid(mod3d::FieldComponent comp) const {
        const auto *g = observedGrid(comp);
        if (g && !g->empty()) return true;
        if (comp == mod3d::FieldComponent::GZ) {
            const auto *gt = observedGrid(mod3d::FieldComponent::G_TOT);
            return gt && !gt->empty();
        } else if (comp == mod3d::FieldComponent::G_TOT) {
            const auto *gz = observedGrid(mod3d::FieldComponent::GZ);
            return gz && !gz->empty();
        } else if (comp == mod3d::FieldComponent::DELTA_T) {
            const auto *mz = observedGrid(mod3d::FieldComponent::MZ);
            return mz && !mz->empty();
        } else if (comp == mod3d::FieldComponent::MZ) {
            const auto *dt = observedGrid(mod3d::FieldComponent::DELTA_T);
            return dt && !dt->empty();
        }
        return false;
    }

    bool hasAnyObservedGrid() const {
        for (const auto &[comp, grid] : m_project.observation().observed_grids()) {
            if (!grid.empty()) return true;
        }
        return false;
    }

    static QString componentName(mod3d::FieldComponent comp) {
        switch (comp) {
        case mod3d::FieldComponent::GX: return "Gx";
        case mod3d::FieldComponent::GY: return "Gy";
        case mod3d::FieldComponent::GZ: return "Gz";
        case mod3d::FieldComponent::G_TOT: return "Gt";
        case mod3d::FieldComponent::MX: return "Mx";
        case mod3d::FieldComponent::MY: return "My";
        case mod3d::FieldComponent::MZ: return "Mz";
        case mod3d::FieldComponent::DELTA_T: return "ΔT";
        case mod3d::FieldComponent::GXX: return "Gxx";
        case mod3d::FieldComponent::GYY: return "Gyy";
        case mod3d::FieldComponent::GZZ: return "Gzz";
        case mod3d::FieldComponent::GXY: return "Gxy";
        case mod3d::FieldComponent::GXZ: return "Gxz";
        case mod3d::FieldComponent::GYZ: return "Gyz";
        default: return "Field";
        }
    }

    static QString componentUnit(mod3d::FieldComponent comp) {
        if (mod3d::is_gravity_component(comp)) {
            return "mGal";
        } else if (mod3d::is_magnetic_component(comp)) {
            return "nT";
        } else if (mod3d::is_tensor_component(comp)) {
            return "E";
        }
        return "";
    }

    QString activeFieldComponentName() const { return componentName(m_activeField); }
    QString activeFieldUnit() const { return componentUnit(m_activeField); }

    bool isModified() const noexcept { return m_modified; }
    void setModified(bool modified = true);

    const QString &filePath() const noexcept { return m_filePath; }
    void setFilePath(const QString &path) { m_filePath = path; }

    QString documentTitle() const;

    // Undo / Redo
    QUndoStack *undoStack() noexcept { return &m_undoStack; }

    // Project Lifecycle
    void createDefaultProject();
    bool newProject(int rows, int cols, double x0, double y0, double dx, double dy, double zMin, double zMax);
    bool newProjectFromReliefGrid(const mod3d::Grid &reliefGrid, double zMin, double zMax);
    bool newProjectFromReliefGrid(const QString &filePath, double zMin, double zMax);
    bool newProjectFromImportedGrids(const mod3d::Grid *topoGrid,
                                     const mod3d::Grid *fieldGrid,
                                     mod3d::FieldComponent fieldComp,
                                     double zMin, double zMax);
    bool setVerticalRange(double zMin, double zMax);
    bool openFile(const QString &path);
    bool saveFile(const QString &path = QString());
    bool loadMeasuredGrid(const QString &filePath, std::optional<mod3d::FieldComponent> comp = std::nullopt);
    bool loadReliefGrid(const QString &filePath);

    // Profile State & Navigation
    ProfileOrientation profileOrientation() const noexcept { return m_profileOrient; }
    int profileIndex() const noexcept { return m_profileIndex; }
    int maxProfileIndex() const noexcept;
    double currentProfileCoordinate() const noexcept;
    int profileColumnLineCount() const noexcept;
    double columnCoordinate(int colIdx) const noexcept;

    void setProfileOrientation(ProfileOrientation orient);
    void setProfileIndex(int index);
    void moveProfile(int delta);
    void jumpToExtreme();
    void jumpToMin();
    void jumpToMax();

    // Body Editing Operations (Explicit Row/Col - 1-based coordinates)
    int insertBodyAtRowCol(int row, int col, double z, double thickness = -1.0, int bodyId = -1);
    bool removeBodyAtRowCol(int row, int col, int vertexIdx);
    bool moveVertexAtRowCol(int row, int col, int vertexIdx, double newZ, mod3d::BodyMoveType moveType = mod3d::BodyMoveType::Normal, int *outUpdatedIdx = nullptr);
    bool unsnapVertexAtRowCol(int row, int col, int vertexIdx, double separation = -1.0);
    bool extendBodySideAtRowCol(int srcRow, int srcCol, int tgtRow, int tgtCol, int bodyId, double targetZ = std::numeric_limits<double>::quiet_NaN());
    bool collapseBodyEdgeAtRowCol(int row, int col, int bodyId);
    bool removeBodyFromProfile(int bodyId, ProfileOrientation orient, int prfIdx);
    bool copyBodyBetweenProfiles(int bodyId, ProfileOrientation orient, int srcPrfIdx, int dstPrfIdx);
    bool translateBodyVerticalOnProfile(int bodyId, ProfileOrientation orient, int prfIdx, double deltaZ);

    // Convenience Profile-based Operations (using document default profile)
    mod3d::Body *createNewBody();
    bool deleteBody(int bodyId);
    int insertBodyAt(int lineIdx, double z, double thickness = -1.0, int bodyId = -1);
    bool removeBodyAt(int lineIdx, int vertexIdx);
    bool removeBodyFromCurrentProfile(int bodyId);
    bool copyBodyToProfile(int bodyId, int targetProfileIdx);
    bool copyBodyToNextProfile(int bodyId);
    bool copyBodyToPreviousProfile(int bodyId);
    bool moveVertex(int lineIdx, int vertexIdx, double newZ, mod3d::BodyMoveType moveType = mod3d::BodyMoveType::Normal, int *outUpdatedIdx = nullptr);
    bool unsnapVertexAt(int lineIdx, int vertexIdx, double separation = -1.0);
    bool extendBodySide(int bodyId, int sourceLineIdx, int targetLineIdx, double targetZ = std::numeric_limits<double>::quiet_NaN());
    bool collapseBodyEdge(int bodyId, int lineIdx);
    bool translateBodyVertical(int bodyId, double deltaZ);

    // Computation Pipeline
    void computeAll();
    void computeDelta(const std::vector<mod3d::Facet3Pt> &updateFacets);
    bool isRealTimeCompute() const noexcept { return m_realTimeCompute; }
    void setRealTimeCompute(bool enabled);

    // Active Field Display
    mod3d::FieldComponent activeField() const noexcept { return m_activeField; }
    void setActiveField(mod3d::FieldComponent comp);

    bool showModeledField() const noexcept { return m_showModeled; }
    void setShowModeledField(bool show);

    bool showMeasuredField() const noexcept { return m_showMeasured; }
    void setShowMeasuredField(bool show);

    bool showDifferenceField() const noexcept { return m_showDiff; }
    void setShowDifferenceField(bool show);

    bool showPreviousProfile() const noexcept { return m_showPrev; }
    void setShowPreviousProfile(bool show);

    bool showNextProfile() const noexcept { return m_showNext; }
    void setShowNextProfile(bool show);

    bool showGridLines() const noexcept { return m_showGridLines; }
    void setShowGridLines(bool show);

    // Statistics
    double currentRms() const;
    double currentDerivative() const;

    void getRowCol(int lineIdx, int &row, int &col) const;

    // Profile Cursor Point Tracking (for Map View Synchronization)
    struct ProfileCursorPoint {
        bool active{false};
        int row{-1};
        int col{-1};
        double x{0.0};
        double y{0.0};
        double z{0.0};
        int bodyId{-1};
        std::string bodyName;
        bool isDragging{false};
    };

    const ProfileCursorPoint &activeProfileCursor() const noexcept { return m_activeProfileCursor; }
    void setActiveProfileCursor(const ProfileCursorPoint &pt);
    void clearActiveProfileCursor();
    void updateStationPointForProfileChange();
    void emitModelGeometryChanged() { emit modelGeometryChanged(); }

signals:
    void documentModifiedChanged(bool modified);
    void documentReset();
    void verticalRangeChanged(double zMin, double zMax);
    void modelGeometryChanged();
    void profileChanged(int index, ProfileOrientation orient);
    void activeFieldChanged(mod3d::FieldComponent comp);
    void fieldDisplayToggled();
    void computationFinished();
    void statusMessage(const QString &message);
    void activeProfileCursorChanged(const mod3d_gui::ModelDocument::ProfileCursorPoint &pt);

private:
    void ensureProfileIndexValid();

    mod3d::Project m_project;
    QString m_filePath;
    bool m_modified{false};
    QUndoStack m_undoStack;

    ProfileOrientation m_profileOrient{ProfileOrientation::EastWest};
    int m_profileIndex{0};

    ProfileCursorPoint m_activeProfileCursor;

    mod3d::FieldComponent m_activeField{mod3d::FieldComponent::GZ};
    bool m_realTimeCompute{true};

    bool m_showModeled{true};
    bool m_showMeasured{false};
    bool m_showDiff{false};
    bool m_showPrev{false};
    bool m_showNext{false};
    bool m_showGridLines{true};
};

} // namespace mod3d_gui
