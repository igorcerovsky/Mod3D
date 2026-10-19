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
    [[nodiscard]] bool is_initialized() const noexcept { return initialized_; }

    // Dimensions and Coordinates
    [[nodiscard]] int rows() const noexcept { return rows_; }
    [[nodiscard]] int cols() const noexcept { return cols_; }
    [[nodiscard]] double x0() const noexcept { return x0_; }
    [[nodiscard]] double y0() const noexcept { return y0_; }
    [[nodiscard]] double dx() const noexcept { return x_size_; }
    [[nodiscard]] double dy() const noexcept { return y_size_; }
    [[nodiscard]] double x_size() const noexcept { return x_size_; }
    [[nodiscard]] double y_size() const noexcept { return y_size_; }
    [[nodiscard]] double z_min() const noexcept { return z_min_; }
    [[nodiscard]] double z_max() const noexcept { return z_max_; }
    void set_z_min(double zMin) noexcept { z_min_ = zMin; }
    void set_z_max(double zMax) noexcept { z_max_ = zMax; }
    [[nodiscard]] double x_min() const noexcept { return x_min_; }
    [[nodiscard]] double x_max() const noexcept { return x_max_; }
    [[nodiscard]] double y_min() const noexcept { return y_min_; }
    [[nodiscard]] double y_max() const noexcept { return y_max_; }

    [[nodiscard]] double xd(int col) const noexcept { return x0_ + (col - 1) * x_size_; }
    [[nodiscard]] double yd(int row) const noexcept { return y0_ + (row - 1) * y_size_; }
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
    [[nodiscard]] double default_thickness() const noexcept { return (z_max_ - z_min_) / 10.0; }

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
    [[nodiscard]] const std::vector<std::unique_ptr<Body>> &bodies() const noexcept { return bodies_; }
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
    void set_compute_real_time(bool realTime = true) noexcept { compute_real_time_ = realTime; }
    [[nodiscard]] bool is_compute_real_time() const noexcept { return compute_real_time_; }
    [[nodiscard]] std::vector<Facet3Pt> &facets_update() noexcept { return facets_update_; }
    [[nodiscard]] const std::vector<Facet3Pt> &facets_update() const noexcept { return facets_update_; }
    void clear_facets_update() noexcept { facets_update_.clear(); }

    // Boundary Extensions
    void set_extensions(double exN, double exS, double exE, double exW, bool enable = true) noexcept {
        ex_n_ = exN; ex_s_ = exS; ex_e_ = exE; ex_w_ = exW; extend_ = enable;
    }
    [[nodiscard]] bool is_extend() const noexcept { return extend_; }
    [[nodiscard]] double ex_n() const noexcept { return ex_n_; }
    [[nodiscard]] double ex_s() const noexcept { return ex_s_; }
    [[nodiscard]] double ex_e() const noexcept { return ex_e_; }
    [[nodiscard]] double ex_w() const noexcept { return ex_w_; }


private:
    int flat_index(int row, int col) const;
    void set_column_point_coords(ColumnPoint &pt, int row, int col, double z);
    int add_fct_gen(std::vector<FctGen> &fga, int row, int col, int tag);
    void fg_to_fct(std::vector<FctGen> &fga, std::vector<Facet3Pt> &facetList, int i0, int i1);
    void fg_to_fct_bound(FctGen &fg, std::vector<Facet3Pt> &facetList);
    void init_facet(const FctGen &upper, const FctGen &lower, std::vector<Facet3Pt> &facetList, int i0, int i1);
    void init_facet_tbm(const FctGen &fg, std::vector<Facet3Pt> &facetList, int i0, int i1, FgType tag);
    void init_side_facets(const FctGen &fg, std::vector<Facet3Pt> &facetList);
    void init_side_facets_2(const FctGen &fg, std::vector<Facet3Pt> &facetList, int i0, int i1);
    void init_side_facets_3(const FctGen &fg, std::vector<Facet3Pt> &facetList, int i0, int i1);
    void init_side_facets_bound(const FctGen &fg, std::vector<Facet3Pt> &facetList, int i0, int i1);

    bool is_ins_pos_ok(int index, int bodyId, int row, int col) const;
    bool is_ins_pos_ok_1(int tag, int bodyId, int idCheck, int row, int col) const;
    bool is_ins_pos_ok_2(int tag, int bodyId, int idCheck, int row, int col) const;

    bool initialized_{false};
    int rows_{0};
    int cols_{0};
    int current_row_{0};
    int current_col_{0};

    double x0_{0.0};
    double y0_{0.0};
    double x_size_{0.0};
    double y_size_{0.0};
    double x_min_{0.0};
    double x_max_{0.0};
    double y_min_{0.0};
    double y_max_{0.0};
    double z_min_{0.0};
    double z_max_{0.0};

    bool extend_{false};
    double ex_n_{0.0};
    double ex_s_{0.0};
    double ex_e_{0.0};
    double ex_w_{0.0};

    BodyCreateTag creation_tag_{BodyCreateTag::None};
    bool const_top_{false};
    double const_top_z_{0.0};
    bool const_bot_{false};
    double const_bot_z_{0.0};

    int next_body_id_{1};
    std::vector<std::unique_ptr<Body>> bodies_;
    std::vector<std::vector<ColumnPoint>> data_; // size rows_ * cols_
    std::vector<std::vector<Facet3Pt>> facets_;  // size rows_ * cols_
    std::vector<std::vector<BodyBoundarySegment>> body_bounds_; // size rows_ * cols_
    std::vector<BodyBoundarySegment> *current_bound_col_{nullptr};

    bool compute_real_time_{false};
    std::vector<Facet3Pt> facets_update_;
};

} // namespace mod3d
