#pragma once

#include "mod3d/Point3D.h"
#include "mod3d/Body.h"
#include "mod3d/Facet3Pt.h"
#include "mod3d/ColumnPoint.h"
#include "mod3d/Grid.h"
#include <vector>
#include <memory>
#include <string>

namespace mod3d {

// Enums
enum class BodyMoveType {
    Normal = 0,
    Split = 1,
    Constrained = 2
};

enum class BodyCreateTag {
    None = 0,
    JoinTop = 1,
    JoinBot = 2,
    JoinTopBot = 3
};

enum class ProfileType {
    Row = 0,
    Col = 1
};

enum class FgType {
    Mid = 0,
    Top = 1,
    Bot = 2,
    TopMost = 3,
    BotMost = 4
};

// 4-column prism cell facet discretization structure
struct FctGen {
    int n{0};               // maximum index
    int id{-1};             // body ID
    int idPrev[4]{-1, -1, -1, -1}; // previous body ID at 4 corners
    int idNext[4]{-1, -1, -1, -1}; // next body ID at 4 corners
    int l{0};               // number of valid points in column cell
    bool b[4]{false, false, false, false}; // whether each corner is valid
    Point3D top[5];         // top points: 0..3 corners, 4 center
    Point3D bot[5];         // bottom points: 0..3 corners, 4 center
};

// 2D horizontal projection boundary line segment for a geological body
struct BodyBoundarySegment {
    int bodyId{0};
    Point3D pt0;
    Point3D pt1;
};

// 3D Polyhedral Body Mesh representation for visualization and CAD export (OBJ, STL, VTK)
struct BodyMesh3D {
    int bodyId{-1};
    std::string bodyName;
    BodyColor color;
    float alpha{1.0f};
    bool isVisible{true};
    bool isTransparent{false};

    // Watertight triangulated boundary representation (all normals point outward from this body)
    std::vector<Facet3Pt> facets;

    // Feature / boundary edges (for crisp CAD-style rendering on top of shaded facets)
    std::vector<std::pair<Point3D, Point3D>> featureEdges;
};

/**
 * @brief Stratigraphic Geological Model & 3D Polyhedral Facet Engine.
 * 
 * Manages the regular columnar grid of stratigraphy points, geological bodies,
 * automated 4-column prism facet generation, and real-time delta updating.
 */
class Model {
public:
    Model();
    ~Model() = default;
    Model(const Model &) = delete;
    Model &operator=(const Model &) = delete;
    Model(Model &&) noexcept = default;
    Model &operator=(Model &&) noexcept = default;

    // Initialization
    bool init(int nRows, int nCols, double x0, double y0, double xSize, double ySize, double zMin, double zMax);
    bool init(const Grid &reliefGrid, double zMin, double zMax);
    bool init_empty(int totalRows, int totalCols, double x0, double y0, double xSize, double ySize, double zMin, double zMax);
    void clear();
    [[nodiscard]] bool is_initialized() const noexcept { return m_initialized; }

    // Dimensions and Coordinates
    [[nodiscard]] int rows() const noexcept { return m_nRows; }
    [[nodiscard]] int cols() const noexcept { return m_nCols; }
    [[nodiscard]] double x0() const noexcept { return m_x0; }
    [[nodiscard]] double y0() const noexcept { return m_y0; }
    [[nodiscard]] double dx() const noexcept { return m_xSize; }
    [[nodiscard]] double dy() const noexcept { return m_ySize; }
    [[nodiscard]] double x_size() const noexcept { return m_xSize; }
    [[nodiscard]] double y_size() const noexcept { return m_ySize; }
    [[nodiscard]] double z_min() const noexcept { return m_zMin; }
    [[nodiscard]] double z_max() const noexcept { return m_zMax; }
    void set_z_min(double zMin) noexcept { m_zMin = zMin; }
    void set_z_max(double zMax) noexcept { m_zMax = zMax; }
    [[nodiscard]] double x_min() const noexcept { return m_xMin; }
    [[nodiscard]] double x_max() const noexcept { return m_xMax; }
    [[nodiscard]] double y_min() const noexcept { return m_yMin; }
    [[nodiscard]] double y_max() const noexcept { return m_yMax; }

    [[nodiscard]] double xd(int col) const noexcept { return m_x0 + (col - 1) * m_xSize; }
    [[nodiscard]] double yd(int row) const noexcept { return m_y0 + (row - 1) * m_ySize; }
    [[nodiscard]] double xe(int col) const;
    [[nodiscard]] double ye(int row) const;
    [[nodiscard]] double x2e(int col) const;
    [[nodiscard]] double y2e(int row) const;

    // Column Point Access
    [[nodiscard]] size_t count(int row, int col) const;
    [[nodiscard]] int upper_bound(int row, int col) const;
    [[nodiscard]] const ColumnPoint *at(int row, int col, int index) const;
    [[nodiscard]] ColumnPoint *at(int row, int col, int index);
    void add(int row, int col, const ColumnPoint &pt);
    void insert_at(int row, int col, int index, const ColumnPoint &pt);
    void remove_at(int row, int col, int index, int count = 1);

    [[nodiscard]] double z(int row, int col, int index) const;
    void set_z(int row, int col, int index, double z);
    [[nodiscard]] double thickness(int row, int col, int index) const;
    [[nodiscard]] double default_thickness() const noexcept { return (m_zMax - m_zMin) / 10.0; }

    // Vertex Manipulation
    int move_vertex(int &index, int row, int col, double z, BodyMoveType moveType = BodyMoveType::Normal);

    // Body Management
    Body *create_body();
    void add_body(std::unique_ptr<Body> body);
    int insert_body(int row, int col, double z, double thickness = -1.0, bool isNew = true, int bodyId = -1, double zT = 0.0, double zB = 0.0, bool bZ = true);
    int remove_body(int index, int row, int col);
    int delete_body(int bodyId);
    [[nodiscard]] Body *body(int bodyId);
    [[nodiscard]] const Body *body(int bodyId) const;
    [[nodiscard]] int body_index(int bodyId, int row, int col) const;
    [[nodiscard]] int update_move_index(int indexOld, int row, int col, int bodyId) const;
    [[nodiscard]] int id(int index, int row, int col) const;
    [[nodiscard]] const std::vector<std::unique_ptr<Body>> &bodies() const noexcept { return m_bodies; }
    void update_body_index();
    void compute_body_magnetization_vector(const Point3D &indFld);
    void compute_body_magnetization_vector(double incDeg, double decDeg, double intensityNT);

    // Facet Generation
    void init_facet_list();
    int generate_facets(std::vector<Facet3Pt> &facetList, int row, int col);
    void update_facet_column(int row, int col);
    void update_facet_list(int row, int col);
    int facets_computation(std::vector<Facet3Pt> &outList) const;
    [[nodiscard]] std::vector<Facet3Pt> *facet_list(int row, int col);
    [[nodiscard]] const std::vector<Facet3Pt> *facet_list(int row, int col) const;

    // 3D Polyhedral Body Meshes & Facet Representation for Rendering and Export
    [[nodiscard]] std::vector<BodyMesh3D> generate_body_meshes(int bodyId = -1, bool includeOuter = false) const;
    int body_facets(int bodyId, std::vector<Facet3Pt> &outList, bool includeOuter = false) const;
    int facets_for_3d_view(std::vector<Facet3Pt> &outList, bool includeOuter = false) const;

    // Body Boundaries (2D Horizontal Projection for Map View)
    [[nodiscard]] const std::vector<BodyBoundarySegment> *body_bound_at(int row, int col) const;
    [[nodiscard]] std::vector<BodyBoundarySegment> all_body_boundaries() const;

    // Real-Time Delta Updates
    void set_compute_real_time(bool realTime = true) noexcept { m_computeRealTime = realTime; }
    [[nodiscard]] bool is_compute_real_time() const noexcept { return m_computeRealTime; }
    [[nodiscard]] std::vector<Facet3Pt> &facets_update() noexcept { return m_fctLstUpdate; }
    [[nodiscard]] const std::vector<Facet3Pt> &facets_update() const noexcept { return m_fctLstUpdate; }
    void clear_facets_update() noexcept { m_fctLstUpdate.clear(); }

    // Boundary Extensions
    void set_extensions(double exN, double exS, double exE, double exW, bool enable = true) noexcept {
        m_dExN = exN; m_dExS = exS; m_dExE = exE; m_dExW = exW; m_extend = enable;
    }
    [[nodiscard]] bool is_extend() const noexcept { return m_extend; }
    [[nodiscard]] double ex_n() const noexcept { return m_dExN; }
    [[nodiscard]] double ex_s() const noexcept { return m_dExS; }
    [[nodiscard]] double ex_e() const noexcept { return m_dExE; }
    [[nodiscard]] double ex_w() const noexcept { return m_dExW; }

    // ========================================================================
    // Legacy API Wrappers
    // ========================================================================
    [[nodiscard]] bool initEmpty(int totalRows, int totalCols, double x0, double y0, double xSize, double ySize, double zMin, double zMax) {
        return init_empty(totalRows, totalCols, x0, y0, xSize, ySize, zMin, zMax);
    }
    [[nodiscard]] bool isInitialized() const noexcept { return is_initialized(); }
    [[nodiscard]] int getRows() const noexcept { return rows(); }
    [[nodiscard]] int getCols() const noexcept { return cols(); }
    [[nodiscard]] double getX0() const noexcept { return x0(); }
    [[nodiscard]] double getY0() const noexcept { return y0(); }
    [[nodiscard]] double getXSize() const noexcept { return dx(); }
    [[nodiscard]] double getYSize() const noexcept { return dy(); }
    [[nodiscard]] double getZMin() const noexcept { return z_min(); }
    [[nodiscard]] double getZMax() const noexcept { return z_max(); }
    void setZMin(double zMin) noexcept { set_z_min(zMin); }
    void setZMax(double zMax) noexcept { set_z_max(zMax); }
    [[nodiscard]] double getXMin() const noexcept { return x_min(); }
    [[nodiscard]] double getXMax() const noexcept { return x_max(); }
    [[nodiscard]] double getYMin() const noexcept { return y_min(); }
    [[nodiscard]] double getYMax() const noexcept { return y_max(); }
    [[nodiscard]] double getHell() const noexcept { return z_min(); }
    [[nodiscard]] double getHeaven() const noexcept { return z_max(); }
    void setHell(double hell) noexcept { set_z_min(hell); }
    void setHeaven(double heaven) noexcept { set_z_max(heaven); }
    [[nodiscard]] double getXd(int col) const noexcept { return xd(col); }
    [[nodiscard]] double getYd(int row) const noexcept { return yd(row); }
    [[nodiscard]] double getXe(int col) const { return xe(col); }
    [[nodiscard]] double getYe(int row) const { return ye(row); }
    [[nodiscard]] double getX2e(int col) const { return x2e(col); }
    [[nodiscard]] double getY2e(int row) const { return y2e(row); }
    [[nodiscard]] size_t getCount(int row, int col) const { return count(row, col); }
    [[nodiscard]] int getUpperBound(int row, int col) const { return upper_bound(row, col); }
    [[nodiscard]] const ColumnPoint *getAt(int row, int col, int index) const { return at(row, col, index); }
    [[nodiscard]] ColumnPoint *getAt(int row, int col, int index) { return at(row, col, index); }
    void insertAt(int row, int col, int index, const ColumnPoint &pt) { insert_at(row, col, index, pt); }
    void removeAt(int row, int col, int index, int count = 1) { remove_at(row, col, index, count); }
    [[nodiscard]] double getZ(int row, int col, int index) const { return z(row, col, index); }
    void setZ(int row, int col, int index, double z) { set_z(row, col, index, z); }
    [[nodiscard]] double getThickness(int row, int col, int index) const { return thickness(row, col, index); }
    [[nodiscard]] double getDefaultThickness() const noexcept { return default_thickness(); }
    int moveVertex(int &index, int row, int col, double z, BodyMoveType moveType = BodyMoveType::Normal) {
        return move_vertex(index, row, col, z, moveType);
    }
    Body *newBody() { return create_body(); }
    void addBody(std::unique_ptr<Body> body) { add_body(std::move(body)); }
    int insertBody(int row, int col, double z, double thickness = -1.0, bool isNew = true, int bodyId = -1, double zT = 0.0, double zB = 0.0, bool bZ = true) {
        return insert_body(row, col, z, thickness, isNew, bodyId, zT, zB, bZ);
    }
    int removeBody(int index, int row, int col) { return remove_body(index, row, col); }
    int deleteBody(int bodyId) { return delete_body(bodyId); }
    [[nodiscard]] Body *getBody(int bodyId) { return body(bodyId); }
    [[nodiscard]] const Body *getBody(int bodyId) const { return body(bodyId); }
    [[nodiscard]] int getBodyIndex(int bodyId, int row, int col) const { return body_index(bodyId, row, col); }
    [[nodiscard]] int updateMoveIndex(int indexOld, int row, int col, int bodyId) const { return update_move_index(indexOld, row, col, bodyId); }
    [[nodiscard]] int getID(int index, int row, int col) const { return id(index, row, col); }
    [[nodiscard]] const std::vector<std::unique_ptr<Body>> &getBodies() const noexcept { return bodies(); }
    void updateBodyIndex() { update_body_index(); }
    void computeBodyMagnetizationVector(const Point3D &indFld) { compute_body_magnetization_vector(indFld); }
    void computeBodyMagnetizationVector(double incDeg, double decDeg, double intensityNT) { compute_body_magnetization_vector(incDeg, decDeg, intensityNT); }
    void initFacetList() { init_facet_list(); }
    int generateFacets(std::vector<Facet3Pt> &facetList, int row, int col) { return generate_facets(facetList, row, col); }
    void updateFacetColumn(int row, int col) { update_facet_column(row, col); }
    void updateFacetList(int row, int col) { update_facet_list(row, col); }
    int getFacetsComputation(std::vector<Facet3Pt> &outList) const { return facets_computation(outList); }
    [[nodiscard]] std::vector<Facet3Pt> *getFacetList(int row, int col) { return facet_list(row, col); }
    [[nodiscard]] const std::vector<Facet3Pt> *getFacetList(int row, int col) const { return facet_list(row, col); }
    [[nodiscard]] std::vector<BodyMesh3D> generateBodyMeshes(int bodyId = -1, bool includeOuter = false) const {
        return generate_body_meshes(bodyId, includeOuter);
    }
    int getBodyFacets(int bodyId, std::vector<Facet3Pt> &outList, bool includeOuter = false) const {
        return body_facets(bodyId, outList, includeOuter);
    }
    int getFacetsFor3DView(std::vector<Facet3Pt> &outList, bool includeOuter = false) const {
        return facets_for_3d_view(outList, includeOuter);
    }
    [[nodiscard]] const std::vector<BodyBoundarySegment> *getBodyBoundAt(int row, int col) const { return body_bound_at(row, col); }
    [[nodiscard]] std::vector<BodyBoundarySegment> getAllBodyBoundaries() const { return all_body_boundaries(); }
    void setComputeRealTime(bool realTime = true) noexcept { set_compute_real_time(realTime); }
    [[nodiscard]] bool isComputeRealTime() const noexcept { return is_compute_real_time(); }
    [[nodiscard]] std::vector<Facet3Pt> &getFacetsUpdate() noexcept { return facets_update(); }
    [[nodiscard]] const std::vector<Facet3Pt> &getFacetsUpdate() const noexcept { return facets_update(); }
    void clearFacetsUpdate() noexcept { clear_facets_update(); }
    void setExtensions(double exN, double exS, double exE, double exW, bool enable = true) noexcept {
        set_extensions(exN, exS, exE, exW, enable);
    }
    [[nodiscard]] bool isExtend() const noexcept { return is_extend(); }
    [[nodiscard]] double getExN() const noexcept { return ex_n(); }
    [[nodiscard]] double getExS() const noexcept { return ex_s(); }
    [[nodiscard]] double getExE() const noexcept { return ex_e(); }
    [[nodiscard]] double getExW() const noexcept { return ex_w(); }

private:
    int getFlatIndex(int row, int col) const;
    void setColumnPointCoords(ColumnPoint &pt, int row, int col, double z);
    int addFctGen(std::vector<FctGen> &fga, int row, int col, int tag);
    void fgToFct(std::vector<FctGen> &fga, std::vector<Facet3Pt> &facetList, int i0, int i1);
    void fgToFctBound(FctGen &fg, std::vector<Facet3Pt> &facetList);
    void initFacet(const FctGen &upper, const FctGen &lower, std::vector<Facet3Pt> &facetList, int i0, int i1);
    void initFacetTBM(const FctGen &fg, std::vector<Facet3Pt> &facetList, int i0, int i1, FgType tag);
    void initSideFacets(const FctGen &fg, std::vector<Facet3Pt> &facetList);
    void initSideFacets_2(const FctGen &fg, std::vector<Facet3Pt> &facetList, int i0, int i1);
    void initSideFacets_3(const FctGen &fg, std::vector<Facet3Pt> &facetList, int i0, int i1);
    void initSideFacetsBound(const FctGen &fg, std::vector<Facet3Pt> &facetList, int i0, int i1);

    bool isInsPosOK(int index, int bodyId, int row, int col) const;
    bool isInsPosOK_1(int tag, int bodyId, int idCheck, int row, int col) const;
    bool isInsPosOK_2(int tag, int bodyId, int idCheck, int row, int col) const;

    bool m_initialized{false};
    int m_nRows{0};
    int m_nCols{0};
    int m_currentRow{0};
    int m_currentCol{0};

    double m_x0{0.0};
    double m_y0{0.0};
    double m_xSize{0.0};
    double m_ySize{0.0};
    double m_xMin{0.0};
    double m_xMax{0.0};
    double m_yMin{0.0};
    double m_yMax{0.0};
    double m_zMin{0.0};
    double m_zMax{0.0};

    bool m_extend{false};
    double m_dExN{0.0};
    double m_dExS{0.0};
    double m_dExE{0.0};
    double m_dExW{0.0};

    BodyCreateTag m_creationTag{BodyCreateTag::None};
    bool m_constTop{false};
    double m_dConstTop{0.0};
    bool m_constBot{false};
    double m_dConstBot{0.0};

    int m_nextBodyId{1};
    std::vector<std::unique_ptr<Body>> m_bodies;
    std::vector<std::vector<ColumnPoint>> m_data; // size m_nRows * m_nCols
    std::vector<std::vector<Facet3Pt>> m_facets;  // size m_nRows * m_nCols
    std::vector<std::vector<BodyBoundarySegment>> m_bodyBounds; // size m_nRows * m_nCols
    std::vector<BodyBoundarySegment> *m_currentBoundCol{nullptr};

    bool m_computeRealTime{false};
    std::vector<Facet3Pt> m_fctLstUpdate;
};

} // namespace mod3d
