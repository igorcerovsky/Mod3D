#include "mod3d_gui/ModelDocument.h"
#include <QFileInfo>
#include <algorithm>
#include <cmath>

namespace mod3d_gui {

ModelDocument::ModelDocument(QObject *parent)
    : QObject(parent)
{
    createDefaultProject();
}

QString ModelDocument::documentTitle() const {
    if (m_filePath.isEmpty()) {
        return tr("Untitled") + (m_modified ? "*" : "");
    }
    return QFileInfo(m_filePath).fileName() + (m_modified ? "*" : "");
}

void ModelDocument::setModified(bool modified) {
    if (m_modified != modified) {
        m_modified = modified;
        emit documentModifiedChanged(m_modified);
    }
}

void ModelDocument::createDefaultProject() {
    newProject(21, 21, 0.0, 0.0, 500.0, 500.0, -4000.0, 1000.0);
}

bool ModelDocument::newProject(int rows, int cols, double x0, double y0, double dx, double dy, double zMin, double zMax) {
    m_project.clear();
    m_undoStack.clear();

    bool okModel = m_project.model().init(rows, cols, x0, y0, dx, dy, zMin, zMax);
    m_project.model().set_compute_real_time(m_realTimeCompute);
    m_project.observation().init_geometry(rows, cols, x0, y0, dx, dy);
    m_project.observation().set_gravity_observation(mod3d::ObservationMode::SensorHeight, 0.0);
    m_project.observation().set_magnetic_observation(mod3d::ObservationMode::SensorHeight, 0.0);

    m_profileOrient = ProfileOrientation::EastWest;
    m_profileIndex = rows / 2;
    m_activeSliceZ = (zMin + zMax) * 0.5;
    m_filePath.clear();
    m_modified = false;
    m_showMeasured = false;
    m_showDiff = false;

    computeAll();

    emit documentReset();
    emit profileChanged(m_profileIndex, m_profileOrient);
    emit documentModifiedChanged(false);
    emit statusMessage(tr("New model created (%1x%2 nodes).").arg(rows).arg(cols));
    return okModel;
}

bool ModelDocument::newProjectFromReliefGrid(const mod3d::Grid &reliefGrid, double zMin, double zMax) {
    if (reliefGrid.empty() || reliefGrid.rows() < 2 || reliefGrid.cols() < 2) {
        return false;
    }
    m_project.clear();
    m_undoStack.clear();

    bool okModel = m_project.model().init(reliefGrid, zMin, zMax);
    m_project.model().set_compute_real_time(m_realTimeCompute);
    m_project.observation().init_geometry(
        reliefGrid.rows(), reliefGrid.cols(),
        reliefGrid.x0(), reliefGrid.y0(),
        reliefGrid.dx(), reliefGrid.dy(),
        reliefGrid.rotation());
    m_project.observation().set_surface_relief(reliefGrid);
    m_project.observation().set_gravity_observation(mod3d::ObservationMode::SensorHeight, 0.0);
    m_project.observation().set_magnetic_observation(mod3d::ObservationMode::SensorHeight, 0.0);

    m_profileOrient = ProfileOrientation::EastWest;
    m_profileIndex = static_cast<int>(reliefGrid.rows()) / 2;
    m_activeSliceZ = (zMin + zMax) * 0.5;
    m_filePath.clear();
    m_modified = false;
    m_showMeasured = false;
    m_showDiff = false;

    computeAll();

    emit documentReset();
    emit profileChanged(m_profileIndex, m_profileOrient);
    emit documentModifiedChanged(false);
    emit statusMessage(tr("New model created from relief grid (%1x%2 nodes).")
                           .arg(reliefGrid.rows())
                           .arg(reliefGrid.cols()));
    return okModel;
}

bool ModelDocument::newProjectFromReliefGrid(const QString &filePath, double zMin, double zMax) {
    mod3d::Grid g;
    if (!g.load_file(filePath.toStdString())) {
        return false;
    }
    return newProjectFromReliefGrid(g, zMin, zMax);
}

bool ModelDocument::newProjectFromImportedGrids(const mod3d::Grid *topoGrid,
                                                const mod3d::Grid *fieldGrid,
                                                mod3d::FieldComponent fieldComp,
                                                double zMin, double zMax)
{
    if (!topoGrid && !fieldGrid) {
        return false;
    }

    const mod3d::Grid &primaryGrid = topoGrid ? *topoGrid : *fieldGrid;
    int obsRows = static_cast<int>(primaryGrid.rows());
    int obsCols = static_cast<int>(primaryGrid.cols());
    double x0 = primaryGrid.x0().value();
    double y0 = primaryGrid.y0().value();
    double dx = primaryGrid.dx().value();
    double dy = primaryGrid.dy().value();

    if (topoGrid) {
        if (!newProjectFromReliefGrid(*topoGrid, zMin, zMax)) {
            return false;
        }
    } else {
        if (!newProject(obsRows, obsCols, x0, y0, dx, dy, zMin, zMax)) {
            return false;
        }
    }

    if (fieldGrid) {
        m_project.observation().set_observed_grid(fieldComp, *fieldGrid);
        m_project.observation().compute_difference(fieldComp);
        m_activeField = fieldComp;
        m_showMeasured = true;
        m_showDiff = true;
        emit activeFieldChanged(fieldComp);
        emit fieldDisplayToggled();
    }

    computeAll();
    emit modelGeometryChanged();
    setModified(false);
    return true;
}

void ModelDocument::suggestVerticalRange(const mod3d::Grid &topo, double &zMin, double &zMax) {
    const double xLen = static_cast<double>(topo.cols() > 0 ? topo.cols() - 1 : 0) * std::abs(topo.dx().value());
    const double yLen = static_cast<double>(topo.rows() > 0 ? topo.rows() - 1 : 0) * std::abs(topo.dy().value());
    const double len = std::max(xLen, yLen);

    double step = 10.0;
    if (len >= 100000.0) {
        step = 1000.0;
    } else if (len >= 10000.0) {
        step = 100.0;
    }

    zMin = std::floor((topo.min() - len * 0.4) / step) * step;
    zMax = std::ceil((topo.max() + len * 0.1) / step) * step;
}

bool ModelDocument::setVerticalRange(double zMin, double zMax) {
    if (zMin >= zMax) {
        return false;
    }
    m_project.model().set_vertical_range(zMin, zMax);
    m_project.model().init_facet_list();
    setModified(true);
    emit verticalRangeChanged(zMin, zMax);
    emit modelGeometryChanged();
    emit statusMessage(tr("Updated vertical range to [%1, %2] m.").arg(zMin).arg(zMax));
    return true;
}

bool ModelDocument::openFile(const QString &path) {
    bool ok = false;
    if (path.endsWith(".m3d", Qt::CaseInsensitive)) {
        ok = m_project.load_legacy_m3d(path.toStdString());
    } else {
        ok = m_project.load_json(path.toStdString());
    }

    if (ok) {
        m_project.model().set_compute_real_time(m_realTimeCompute);
        m_project.observation().set_remove_observed_mean(m_project.settings().remove_observed_mean);
        m_filePath = path;
        m_modified = false;
        m_undoStack.clear();
        ensureProfileIndexValid();
        m_activeSliceZ = (m_project.model().z_min_val() + m_project.model().z_max_val()) * 0.5;

        // Auto-select field component based on available observed data
        const auto *gzObs = m_project.observation().observed_grid(mod3d::FieldComponent::GZ);
        const auto *gtObs = m_project.observation().observed_grid(mod3d::FieldComponent::G_TOT);
        const auto *magObs = m_project.observation().observed_grid(mod3d::FieldComponent::DELTA_T);
        if ((!gzObs || gzObs->empty()) && (!gtObs || gtObs->empty()) && (magObs && !magObs->empty())) {
            m_activeField = mod3d::FieldComponent::DELTA_T;
        }

        bool hasObs = hasAnyObservedGrid();
        m_showMeasured = hasObs;
        m_showDiff = hasObs;

        computeAll();
        emit documentReset();
        emit profileChanged(m_profileIndex, m_profileOrient);
        emit documentModifiedChanged(false);
        emit statusMessage(tr("Loaded project %1").arg(QFileInfo(path).fileName()));
    }
    return ok;
}

bool ModelDocument::saveFile(const QString &path) {
    QString savePath = path.isEmpty() ? m_filePath : path;
    if (savePath.isEmpty()) {
        return false;
    }

    bool ok = m_project.save_json(savePath.toStdString());
    if (ok) {
        m_filePath = savePath;
        setModified(false);
        emit statusMessage(tr("Saved project to %1").arg(QFileInfo(savePath).fileName()));
    }
    return ok;
}

int ModelDocument::maxProfileIndex() const noexcept {
    if (m_profileOrient == ProfileOrientation::EastWest) {
        return std::max(0, m_project.model().rows() - 3);
    } else {
        return std::max(0, m_project.model().cols() - 3);
    }
}

double ModelDocument::currentProfileCoordinate() const noexcept {
    if (m_profileOrient == ProfileOrientation::EastWest) {
        return m_project.model().yd_val(m_profileIndex + 1);
    } else {
        return m_project.model().xd_val(m_profileIndex + 1);
    }
}

int ModelDocument::profileColumnLineCount() const noexcept {
    if (m_profileOrient == ProfileOrientation::EastWest) {
        return std::max(0, m_project.model().cols() - 2);
    } else {
        return std::max(0, m_project.model().rows() - 2);
    }
}

double ModelDocument::columnCoordinate(int colIdx) const noexcept {
    if (m_profileOrient == ProfileOrientation::EastWest) {
        return m_project.model().xd_val(colIdx + 1);
    } else {
        return m_project.model().yd_val(colIdx + 1);
    }
}

void ModelDocument::ensureProfileIndexValid() {
    int maxIdx = maxProfileIndex();
    if (m_profileIndex < 0) m_profileIndex = 0;
    if (m_profileIndex > maxIdx) m_profileIndex = maxIdx;
}

void ModelDocument::setProfileOrientation(ProfileOrientation orient) {
    if (m_profileOrient != orient) {
        m_profileOrient = orient;
        ensureProfileIndexValid();
        updateStationPointForProfileChange();
        emit profileChanged(m_profileIndex, m_profileOrient);
    }
}

void ModelDocument::setProfileIndex(int index) {
    int clamped = std::clamp(index, 0, maxProfileIndex());
    if (m_profileIndex != clamped) {
        m_profileIndex = clamped;
        updateStationPointForProfileChange();
        emit profileChanged(m_profileIndex, m_profileOrient);
    }
}

void ModelDocument::moveProfile(int delta) {
    setProfileIndex(m_profileIndex + delta);
}

void ModelDocument::getRowCol(int lineIdx, int &row, int &col) const {
    if (m_profileOrient == ProfileOrientation::EastWest) {
        row = m_profileIndex + 1; // 1-based in Model
        col = lineIdx + 1;
    } else {
        row = lineIdx + 1;
        col = m_profileIndex + 1;
    }
}

void ModelDocument::jumpToExtreme() {
    const auto *grid = activeDifferenceGrid();
    if (!grid) grid = activeModeledGrid();
    if (!grid) return;

    int bestRow = m_profileIndex;
    double maxVal = -1.0;
    for (size_t r = 0; r < grid->rows(); ++r) {
        for (size_t c = 0; c < grid->cols(); ++c) {
            double v = std::abs((*grid)(r, c));
            if (v > maxVal) {
                maxVal = v;
                bestRow = static_cast<int>(r);
            }
        }
    }
    if (m_profileOrient == ProfileOrientation::EastWest) {
        setProfileIndex(bestRow);
    }
}

void ModelDocument::jumpToMin() {
    const auto *grid = activeDifferenceGrid();
    if (!grid) grid = activeModeledGrid();
    if (!grid) return;

    int bestIdx = m_profileIndex;
    double minVal = 1e30;
    for (size_t r = 0; r < grid->rows(); ++r) {
        for (size_t c = 0; c < grid->cols(); ++c) {
            double v = (*grid)(r, c);
            if (v < minVal) {
                minVal = v;
                bestIdx = (m_profileOrient == ProfileOrientation::EastWest) ? static_cast<int>(r) : static_cast<int>(c);
            }
        }
    }
    setProfileIndex(bestIdx);
}

void ModelDocument::jumpToMax() {
    const auto *grid = activeDifferenceGrid();
    if (!grid) grid = activeModeledGrid();
    if (!grid) return;

    int bestIdx = m_profileIndex;
    double maxVal = -1e30;
    for (size_t r = 0; r < grid->rows(); ++r) {
        for (size_t c = 0; c < grid->cols(); ++c) {
            double v = (*grid)(r, c);
            if (v > maxVal) {
                maxVal = v;
                bestIdx = (m_profileOrient == ProfileOrientation::EastWest) ? static_cast<int>(r) : static_cast<int>(c);
            }
        }
    }
    setProfileIndex(bestIdx);
}

mod3d::Body *ModelDocument::createNewBody() {
    int curLine = profileColumnLineCount() / 2;
    int row = 0, col = 0;
    getRowCol(curLine, row, col);
    double zMin = m_project.model().z_min_val();
    double zSurface = m_project.model().z(row, col, 0);
    double defThickness = m_project.model().default_thickness_val();
    double halfT = defThickness / 2.0;
    if (halfT <= 0.0 || halfT >= (zSurface - zMin) / 2.0) {
        halfT = (zSurface - zMin) * 0.1;
    }
    double midZ = (zMin + zSurface) / 2.0;
    midZ = std::clamp(midZ, zMin + halfT, zSurface - halfT);

    int bodyId = insertBodyAt(curLine, midZ);
    if (bodyId >= 0) {
        return m_project.model().body(bodyId);
    }
    auto *b = m_project.model().create_body();
    setModified(true);
    emit modelGeometryChanged();
    return b;
}

bool ModelDocument::deleteBody(int bodyId) {
    int res = m_project.model().delete_body(bodyId);
    if (res >= 0) {
        if (m_activeProfileCursor.bodyId == bodyId) {
            clearActiveProfileCursor();
        }
        setModified(true);
        computeAll();
        emit modelGeometryChanged();
        return true;
    }
    return false;
}

int ModelDocument::insertBodyAtRowCol(int row, int col, double z, double thickness, int bodyId) {
    if (thickness <= 0.0) {
        thickness = m_project.model().default_thickness_val();
    }

    bool isNew = (bodyId < 0);
    double halfT = thickness / 2.0;
    int res = m_project.model().insert_body(row, col, z, halfT, isNew, bodyId);
    if (res >= 0) {
        setModified(true);
        if (m_realTimeCompute) {
            computeDelta(m_project.model().facets_update());
            m_project.model().clear_facets_update();
        }
        emit modelGeometryChanged();
        const auto *pt = m_project.model().at(row, col, res);
        return pt ? pt->body_id() : res;
    }
    return -1;
}

int ModelDocument::insertBodyAt(int lineIdx, double z, double thickness, int bodyId) {
    int row = 0, col = 0;
    getRowCol(lineIdx, row, col);
    return insertBodyAtRowCol(row, col, z, thickness, bodyId);
}

bool ModelDocument::removeBodyAtRowCol(int row, int col, int vertexIdx) {
    int res = m_project.model().remove_body(vertexIdx, row, col);
    if (res >= 0) {
        setModified(true);
        if (m_realTimeCompute) {
            computeDelta(m_project.model().facets_update());
            m_project.model().clear_facets_update();
        }
        emit modelGeometryChanged();
        return true;
    }
    return false;
}

bool ModelDocument::removeBodyAt(int lineIdx, int vertexIdx) {
    int row = 0, col = 0;
    getRowCol(lineIdx, row, col);
    return removeBodyAtRowCol(row, col, vertexIdx);
}

bool ModelDocument::removeBodyFromProfile(int bodyId, ProfileOrientation orient, int prfIdx) {
    bool isEW = (orient == ProfileOrientation::EastWest);
    int nLines = isEW ? std::max(0, m_project.model().cols() - 2) : std::max(0, m_project.model().rows() - 2);
    bool anyRemoved = false;
    for (int l = 0; l < nLines; ++l) {
        int row = isEW ? (prfIdx + 1) : (l + 1);
        int col = isEW ? (l + 1) : (prfIdx + 1);
        int bIdx = m_project.model().body_index(bodyId, row, col);
        if (bIdx >= 0) {
            m_project.model().remove_body(bIdx, row, col);
            anyRemoved = true;
        }
    }
    if (anyRemoved) {
        setModified(true);
        if (m_realTimeCompute) {
            computeDelta(m_project.model().facets_update());
            m_project.model().clear_facets_update();
        }
        emit modelGeometryChanged();
    }
    return anyRemoved;
}

bool ModelDocument::removeBodyFromCurrentProfile(int bodyId) {
    return removeBodyFromProfile(bodyId, m_profileOrient, m_profileIndex);
}

bool ModelDocument::copyBodyBetweenProfiles(int bodyId, ProfileOrientation orient, int srcPrfIdx, int dstPrfIdx) {
    bool isEW = (orient == ProfileOrientation::EastWest);
    int maxIdx = isEW ? std::max(0, m_project.model().rows() - 3) : std::max(0, m_project.model().cols() - 3);
    if (dstPrfIdx < 0 || dstPrfIdx > maxIdx) {
        return false;
    }
    int nLines = isEW ? std::max(0, m_project.model().cols() - 2) : std::max(0, m_project.model().rows() - 2);
    bool anyCopied = false;

    for (int l = 0; l < nLines; ++l) {
        int srcRow = isEW ? (srcPrfIdx + 1) : (l + 1);
        int srcCol = isEW ? (l + 1) : (srcPrfIdx + 1);
        int bIdx = m_project.model().body_index(bodyId, srcRow, srcCol);
        if (bIdx >= 0) {
            double zT = m_project.model().z(srcRow, srcCol, bIdx);
            double zB = m_project.model().z(srcRow, srcCol, bIdx + 1);
            int dstRow = isEW ? (dstPrfIdx + 1) : (l + 1);
            int dstCol = isEW ? (l + 1) : (dstPrfIdx + 1);
            m_project.model().insert_body(dstRow, dstCol, 0.0, 0.0, false, bodyId, zT, zB, false);
            anyCopied = true;
        }
    }

    if (anyCopied) {
        setModified(true);
        if (m_realTimeCompute) {
            computeDelta(m_project.model().facets_update());
            m_project.model().clear_facets_update();
        }
        emit modelGeometryChanged();
    }
    return anyCopied;
}

bool ModelDocument::copyBodyToProfile(int bodyId, int targetProfileIdx) {
    return copyBodyBetweenProfiles(bodyId, m_profileOrient, m_profileIndex, targetProfileIdx);
}

bool ModelDocument::copyBodyToNextProfile(int bodyId) {
    return copyBodyToProfile(bodyId, m_profileIndex + 1);
}

bool ModelDocument::copyBodyToPreviousProfile(int bodyId) {
    return copyBodyToProfile(bodyId, m_profileIndex - 1);
}

bool ModelDocument::moveVertexAtRowCol(int row, int col, int vertexIdx, double newZ, mod3d::BodyMoveType moveType, int *outUpdatedIdx) {
    int vIdx = vertexIdx;
    int res = m_project.model().move_vertex(vIdx, row, col, newZ, moveType);
    if (res >= 0) {
        if (outUpdatedIdx) {
            *outUpdatedIdx = vIdx;
        }
        setModified(true);
        if (m_realTimeCompute) {
            computeDelta(m_project.model().facets_update());
            m_project.model().clear_facets_update();
        }
        emit modelGeometryChanged();
        return true;
    }
    return false;
}

bool ModelDocument::moveVertex(int lineIdx, int vertexIdx, double newZ, mod3d::BodyMoveType moveType, int *outUpdatedIdx) {
    int row = 0, col = 0;
    getRowCol(lineIdx, row, col);
    return moveVertexAtRowCol(row, col, vertexIdx, newZ, moveType, outUpdatedIdx);
}

bool ModelDocument::unsnapVertexAtRowCol(int row, int col, int vertexIdx, double separation) {
    if (vertexIdx <= 0 || vertexIdx >= static_cast<int>(m_project.model().count(row, col)) - 1) {
        return false;
    }

    double zC = m_project.model().z(row, col, vertexIdx);
    int i = vertexIdx;
    while (i > 1 && m_project.model().z(row, col, i - 1) == zC) {
        --i;
    }
    int j = vertexIdx + 1;
    int indexMax = static_cast<int>(m_project.model().count(row, col)) - 1;
    while (j < indexMax + 1) {
        if (zC > m_project.model().z(row, col, j)) break;
        if (j == indexMax) break;
        j++;
    }

    if (i == j - 1) {
        return false;
    }

    double zU = m_project.model().z(row, col, i - 1);
    double zD = m_project.model().z(row, col, j);

    if (separation <= 0.0) {
        separation = m_project.model().default_thickness_val();
        if (separation <= 0.0 || separation > (zU - zD) * 0.8) {
            separation = (zU - zD) * 0.2;
        }
    }

    double halfSep = separation / 2.0;
    double newTopZ = std::min(zC + halfSep, zU - (zU - zC) * 0.05);
    double newBotZ = std::max(zC - halfSep, zD + (zC - zD) * 0.05);

    int topIdx = i;
    m_project.model().move_vertex(topIdx, row, col, newTopZ, mod3d::BodyMoveType::Split);
    int botIdx = j - 1;
    m_project.model().move_vertex(botIdx, row, col, newBotZ, mod3d::BodyMoveType::Split);

    setModified(true);
    if (m_realTimeCompute) {
        computeDelta(m_project.model().facets_update());
        m_project.model().clear_facets_update();
    }
    emit modelGeometryChanged();
    return true;
}

bool ModelDocument::unsnapVertexAt(int lineIdx, int vertexIdx, double separation) {
    int row = 0, col = 0;
    getRowCol(lineIdx, row, col);
    return unsnapVertexAtRowCol(row, col, vertexIdx, separation);
}

bool ModelDocument::extendBodySideAtRowCol(int srcRow, int srcCol, int tgtRow, int tgtCol, int bodyId, double targetZ) {
    if (m_project.model().body_index(bodyId, tgtRow, tgtCol) >= 0) {
        return collapseBodyEdgeAtRowCol(srcRow, srcCol, bodyId);
    }

    int bIdx = m_project.model().body_index(bodyId, srcRow, srcCol);
    if (bIdx < 0) return false;

    double topZ = m_project.model().z(srcRow, srcCol, bIdx);
    double botZ = m_project.model().z(srcRow, srcCol, bIdx + 1);
    double th = std::abs(topZ - botZ);
    double halfT = th / 2.0;
    double midZ = std::isnan(targetZ) ? ((topZ + botZ) / 2.0) : targetZ;

    int res = m_project.model().insert_body(tgtRow, tgtCol, midZ, halfT, false, bodyId);
    if (res < 0 && !std::isnan(targetZ)) {
        res = m_project.model().insert_body(tgtRow, tgtCol, (topZ + botZ) / 2.0, halfT, false, bodyId);
    }
    if (res >= 0) {
        setModified(true);
        if (m_realTimeCompute) {
            computeDelta(m_project.model().facets_update());
            m_project.model().clear_facets_update();
        }
        emit modelGeometryChanged();
        return true;
    }
    return false;
}

bool ModelDocument::extendBodySide(int bodyId, int sourceLineIdx, int targetLineIdx, double targetZ) {
    int srcRow = 0, srcCol = 0;
    int tgtRow = 0, tgtCol = 0;
    getRowCol(sourceLineIdx, srcRow, srcCol);
    getRowCol(targetLineIdx, tgtRow, tgtCol);
    return extendBodySideAtRowCol(srcRow, srcCol, tgtRow, tgtCol, bodyId, targetZ);
}

bool ModelDocument::collapseBodyEdgeAtRowCol(int row, int col, int bodyId) {
    int bIdx = m_project.model().body_index(bodyId, row, col);
    if (bIdx >= 0) {
        m_project.model().remove_body(bIdx, row, col);
        setModified(true);
        if (m_realTimeCompute) {
            computeDelta(m_project.model().facets_update());
            m_project.model().clear_facets_update();
        }
        emit modelGeometryChanged();
        return true;
    }
    return false;
}

bool ModelDocument::collapseBodyEdge(int bodyId, int lineIdx) {
    int row = 0, col = 0;
    getRowCol(lineIdx, row, col);
    return collapseBodyEdgeAtRowCol(row, col, bodyId);
}

bool ModelDocument::translateBodyVerticalOnProfile(int bodyId, ProfileOrientation orient, int prfIdx, double deltaZ) {
    bool isEW = (orient == ProfileOrientation::EastWest);
    int nLines = isEW ? std::max(0, m_project.model().cols() - 2) : std::max(0, m_project.model().rows() - 2);
    bool anyMoved = false;
    for (int l = 0; l < nLines; ++l) {
        int row = isEW ? (prfIdx + 1) : (l + 1);
        int col = isEW ? (l + 1) : (prfIdx + 1);
        int bIdx = m_project.model().body_index(bodyId, row, col);
        if (bIdx >= 0) {
            double currentZ = m_project.model().z(row, col, bIdx);
            double newZ = currentZ + deltaZ;
            m_project.model().move_vertex(bIdx, row, col, newZ);
            anyMoved = true;
        }
    }
    if (anyMoved) {
        setModified(true);
        if (m_realTimeCompute) {
            computeDelta(m_project.model().facets_update());
            m_project.model().clear_facets_update();
        }
        emit modelGeometryChanged();
    }
    return anyMoved;
}

bool ModelDocument::translateBodyVertical(int bodyId, double deltaZ) {
    return translateBodyVerticalOnProfile(bodyId, m_profileOrient, m_profileIndex, deltaZ);
}

void ModelDocument::computeAll() {
    m_project.model().clear_facets_update();
    m_project.model().compute_body_magnetization_vector(
        m_project.observation().inclination().value(),
        m_project.observation().declination().value(),
        m_project.observation().magnetic_intensity().value());
    std::vector<mod3d::Facet3Pt> facets;
    m_project.model().facets_computation(facets);
    m_project.observation().compute_forward_field(facets, true);
    m_project.observation().compute_total_fields();
    m_project.observation().compute_difference(m_activeField);
    emit computationFinished();
}

void ModelDocument::computeDelta(const std::vector<mod3d::Facet3Pt> &updateFacets) {
    if (!updateFacets.empty()) {
        m_project.model().compute_body_magnetization_vector(
            m_project.observation().inclination().value(),
            m_project.observation().declination().value(),
            m_project.observation().magnetic_intensity().value());
        m_project.observation().update_delta_field(updateFacets);
        m_project.observation().compute_total_fields();
        m_project.observation().compute_difference(m_activeField);
        emit computationFinished();
    }
}

void ModelDocument::setRealTimeCompute(bool enabled) {
    if (m_realTimeCompute != enabled) {
        m_realTimeCompute = enabled;
        m_project.model().set_compute_real_time(enabled);
        m_project.model().clear_facets_update();
        emit statusMessage(enabled ? tr("Real-time computation enabled.") : tr("Real-time computation disabled."));
    }
}

void ModelDocument::setRemoveObservedMean(bool enabled) {
    if (m_project.observation().remove_observed_mean() == enabled &&
        m_project.settings().remove_observed_mean == enabled) {
        return;
    }
    m_project.settings().remove_observed_mean = enabled;
    m_project.observation().set_remove_observed_mean(enabled);
    m_project.observation().compute_difference(m_activeField);
    setModified(true);
    emit computationFinished();
    emit fieldDisplayToggled();
    emit statusMessage(enabled ? tr("Measured field mean removed (virtual anomaly).")
                               : tr("Measured field mean restored."));
}

void ModelDocument::setActiveField(mod3d::FieldComponent comp) {
    bool changed = (m_activeField != comp);
    m_activeField = comp;
    const auto *grid = modeledGrid(comp);
    if (!grid || grid->empty()) {
        computeAll();
    } else {
        m_project.observation().compute_difference(m_activeField);
    }
    if (changed) {
        emit activeFieldChanged(m_activeField);
    }
}

void ModelDocument::setShowModeledField(bool show) {
    if (m_showModeled != show) {
        m_showModeled = show;
        emit fieldDisplayToggled();
    }
}

void ModelDocument::setShowMeasuredField(bool show) {
    bool eff = show && hasAnyObservedGrid();
    if (m_showMeasured != eff) {
        m_showMeasured = eff;
        emit fieldDisplayToggled();
    }
}

void ModelDocument::setShowDifferenceField(bool show) {
    bool eff = show && hasAnyObservedGrid();
    if (m_showDiff != eff) {
        m_showDiff = eff;
        emit fieldDisplayToggled();
    }
}

void ModelDocument::setShowPreviousProfile(bool show) {
    if (m_showPrev != show) {
        m_showPrev = show;
        emit fieldDisplayToggled();
    }
}

void ModelDocument::setShowNextProfile(bool show) {
    if (m_showNext != show) {
        m_showNext = show;
        emit fieldDisplayToggled();
    }
}

void ModelDocument::setShowGridLines(bool show) {
    if (m_showGridLines != show) {
        m_showGridLines = show;
        emit fieldDisplayToggled();
    }
}

double ModelDocument::currentRms() const {
    const auto *diff = activeDifferenceGrid();
    if (diff) return diff->rms();
    return 0.0;
}

double ModelDocument::currentDerivative() const {
    const auto *diff = activeDifferenceGrid();
    if (!diff || diff->rows() < 2 || diff->cols() < 2) return 0.0;

    double sumSlope = 0.0;
    size_t count = 0;
    for (size_t r = 0; r < diff->rows(); ++r) {
        for (size_t c = 0; c < diff->cols(); ++c) {
            if (c + 1 < diff->cols()) {
                sumSlope += std::abs((*diff)(r, c + 1) - (*diff)(r, c));
                count++;
            }
            if (r + 1 < diff->rows()) {
                sumSlope += std::abs((*diff)(r + 1, c) - (*diff)(r, c));
                count++;
            }
        }
    }
    return count > 0 ? (sumSlope / count) : 0.0;
}

bool ModelDocument::loadMeasuredGrid(const QString &filePath, std::optional<mod3d::FieldComponent> comp) {
    mod3d::Grid g;
    if (!g.load_file(filePath.toStdString())) {
        return false;
    }

    mod3d::FieldComponent targetComp = comp.value_or(m_activeField);
    m_project.observation().set_observed_grid(targetComp, g);
    m_project.observation().compute_difference(targetComp);
    m_showMeasured = true;
    m_showDiff = true;
    setModified(true);
    emit activeFieldChanged(targetComp);
    emit fieldDisplayToggled();
    emit modelGeometryChanged();
    emit statusMessage(tr("Imported measured field (%1x%2 nodes) from %3")
                           .arg(g.rows())
                           .arg(g.cols())
                           .arg(QFileInfo(filePath).fileName()));
    return true;
}

bool ModelDocument::loadReliefGrid(const QString &filePath) {
    mod3d::Grid g;
    if (!g.load_file(filePath.toStdString())) {
        return false;
    }

    m_project.observation().set_surface_relief(g);
    m_project.model().set_surface_relief(g);

    // Set vertical range from topography and footprint
    double zMin = 0.0, zMax = 0.0;
    suggestVerticalRange(g, zMin, zMax);
    if (std::isfinite(zMin) && std::isfinite(zMax) && zMin < zMax) {
        m_project.model().set_vertical_range(zMin, zMax);
        m_project.model().init_facet_list();
    }
    setModified(true);
    computeAll();
    emit verticalRangeChanged(m_project.model().z_min_val(), m_project.model().z_max_val());
    emit modelGeometryChanged();
    emit statusMessage(tr("Imported topography DEM (%1x%2 nodes) from %3")
                           .arg(g.rows())
                           .arg(g.cols())
                           .arg(QFileInfo(filePath).fileName()));
    return true;
}

void ModelDocument::setActiveProfileCursor(const ProfileCursorPoint &pt) {
    if (m_activeProfileCursor.active == pt.active &&
        m_activeProfileCursor.row == pt.row &&
        m_activeProfileCursor.col == pt.col &&
        m_activeProfileCursor.bodyId == pt.bodyId &&
        m_activeProfileCursor.isDragging == pt.isDragging &&
        std::abs(m_activeProfileCursor.x - pt.x) < 0.1 &&
        std::abs(m_activeProfileCursor.y - pt.y) < 0.1 &&
        std::abs(m_activeProfileCursor.z - pt.z) < 0.5) {
        return;
    }
    m_activeProfileCursor = pt;
    emit activeProfileCursorChanged(m_activeProfileCursor);
}

void ModelDocument::clearActiveProfileCursor() {
    if (m_activeProfileCursor.active) {
        m_activeProfileCursor.active = false;
        emit activeProfileCursorChanged(m_activeProfileCursor);
    }
}

void ModelDocument::updateStationPointForProfileChange() {
    int rows = m_project.model().rows();
    int cols = m_project.model().cols();
    if (rows <= 2 || cols <= 2) return;

    int rStation = 1;
    int cStation = 1;

    if (m_profileOrient == ProfileOrientation::EastWest) {
        rStation = m_profileIndex + 1; // 1-based in Model
        if (m_activeProfileCursor.col >= 1 && m_activeProfileCursor.col <= cols - 2) {
            cStation = m_activeProfileCursor.col;
        } else {
            cStation = std::clamp(cols / 2, 1, cols - 2);
        }
    } else {
        cStation = m_profileIndex + 1;
        if (m_activeProfileCursor.row >= 1 && m_activeProfileCursor.row <= rows - 2) {
            rStation = m_activeProfileCursor.row;
        } else {
            rStation = std::clamp(rows / 2, 1, rows - 2);
        }
    }

    double wx = m_project.model().xd_val(cStation);
    double wy = m_project.model().yd_val(rStation);
    double wz = 0.0;

    int bId = m_activeProfileCursor.bodyId;
    std::string bName;
    bool foundBody = false;

    // Check if the previous body exists at this new station on the profile
    if (bId >= 0) {
        int bIdx = m_project.model().body_index(bId, rStation, cStation);
        if (bIdx >= 0) {
            wz = m_project.model().z(rStation, cStation, bIdx);
            if (const auto *b = m_project.model().body(bId)) {
                bName = b->name();
            }
            foundBody = true;
        } else {
            bId = -1;
        }
    }

    // If same body not present, check if any body exists at this station
    if (!foundBody) {
        for (const auto &b : m_project.model().bodies()) {
            int bIdx = m_project.model().body_index(b->id(), rStation, cStation);
            if (bIdx >= 0) {
                bId = b->id();
                bName = b->name();
                wz = m_project.model().z(rStation, cStation, bIdx);
                foundBody = true;
                break;
            }
        }
    }

    if (!foundBody) {
        wz = m_project.model().z(rStation, cStation, 0);
    }

    m_activeProfileCursor = {
        true,
        rStation,
        cStation,
        wx,
        wy,
        wz,
        bId,
        bName,
        false
    };

    emit activeProfileCursorChanged(m_activeProfileCursor);
}

void ModelDocument::setActiveSliceElevation(double z) {
    double zMin = m_project.model().z_min_val();
    double zMax = m_project.model().z_max_val();
    z = std::clamp(z, zMin, zMax);
    if (std::abs(m_activeSliceZ - z) > 1e-4) {
        m_activeSliceZ = z;
        emit activeSliceElevationChanged(m_activeSliceZ);
    }
}

} // namespace mod3d_gui
