#include "mod3d/Model.h"
#include <algorithm>
#include <cmath>
#include <cassert>
#include <map>
#include <tuple>

namespace mod3d {

Model::Model() = default;

int Model::flat_index(int row, int col) const
{
    if (row < 0 || col < 0 || row >= rows_ || col >= cols_) {
        return -1;
    }
    return row * cols_ + col;
}

double Model::xe(int col) const
{
    if (col == 0) return x0_ - ex_w_;
    if (col == cols_ - 1) return x0_ + (col - 2) * x_size_ + ex_e_;
    return x0_ + (col - 1) * x_size_;
}

double Model::ye(int row) const
{
    if (row == 0) return y0_ - ex_s_;
    if (row == rows_ - 1) return y0_ + (row - 2) * y_size_ + ex_n_;
    return y0_ + (row - 1) * y_size_;
}

double Model::x2e(int col) const
{
    if (col == 0) return x0_ - ex_w_ / 2.0;
    if (col == cols_ - 2) return x0_ + (col - 1) * x_size_ + ex_e_ / 2.0;
    return x0_ + (col - 1) * x_size_ + x_size_ / 2.0;
}

double Model::y2e(int row) const
{
    if (row == 0) return y0_ - ex_s_ / 2.0;
    if (row == rows_ - 2) return y0_ + (row - 1) * y_size_ + ex_n_ / 2.0;
    return y0_ + (row - 1) * y_size_ + y_size_ / 2.0;
}

void Model::set_column_point_coords(ColumnPoint &pt, int row, int col, double z)
{
    pt.set_coords(xe(col), ye(row), z);
}

bool Model::init(int nRows, int nCols, double x0, double y0, double xSize, double ySize, double zMin, double zMax)
{
    rows_ = nRows + 2;
    cols_ = nCols + 2;
    x0_ = x0;
    y0_ = y0;
    x_size_ = xSize;
    y_size_ = ySize;
    x_min_ = x0_;
    x_max_ = x0_ + (cols_ - 3) * x_size_;
    y_min_ = y0_;
    y_max_ = y0_ + (rows_ - 3) * y_size_;
    z_min_ = zMin;
    z_max_ = zMax;

    const size_t totalCells = static_cast<size_t>(rows_ * cols_);
    data_.resize(totalCells);
    facets_.resize(totalCells);
    body_bounds_.resize(totalCells);

    for (int r = 0; r < rows_; ++r) {
        for (int c = 0; c < cols_; ++c) {
            auto &colVec = data_[r * cols_ + c];
            colVec.clear();
            colVec.reserve(16);

            ColumnPoint pTop(z_max_);
            set_column_point_coords(pTop, r, c, z_max_);
            colVec.push_back(pTop);

            ColumnPoint pBot(z_min_);
            set_column_point_coords(pBot, r, c, z_min_);
            colVec.push_back(pBot);
        }
    }

    initialized_ = true;
    return true;
}

bool Model::init(const Grid &reliefGrid, double zMin, double zMax)
{
    const int nRows = static_cast<int>(reliefGrid.rows());
    const int nCols = static_cast<int>(reliefGrid.cols());
    rows_ = nRows + 2;
    cols_ = nCols + 2;
    x0_ = reliefGrid.x0();
    y0_ = reliefGrid.y0();
    x_size_ = reliefGrid.dx();
    y_size_ = reliefGrid.dy();
    x_min_ = x0_;
    x_max_ = x0_ + (cols_ - 3) * x_size_;
    y_min_ = y0_;
    y_max_ = y0_ + (rows_ - 3) * y_size_;
    z_min_ = zMin;
    z_max_ = zMax;

    const size_t totalCells = static_cast<size_t>(rows_ * cols_);
    data_.resize(totalCells);
    facets_.resize(totalCells);
    body_bounds_.resize(totalCells);

    for (int r = 0; r < rows_; ++r) {
        for (int c = 0; c < cols_; ++c) {
            int ii = std::clamp(r - 1, 0, nRows - 1);
            int jj = std::clamp(c - 1, 0, nCols - 1);
            double v = reliefGrid(ii, jj);

            auto &colVec = data_[r * cols_ + c];
            colVec.clear();
            colVec.reserve(16);

            ColumnPoint pTop(v);
            set_column_point_coords(pTop, r, c, v);
            colVec.push_back(pTop);

            ColumnPoint pBot(z_min_);
            set_column_point_coords(pBot, r, c, z_min_);
            colVec.push_back(pBot);
        }
    }

    initialized_ = true;
    return true;
}

bool Model::init_empty(int totalRows, int totalCols, double x0, double y0, double xSize, double ySize, double zMin, double zMax)
{
    rows_ = totalRows;
    cols_ = totalCols;
    x0_ = x0;
    y0_ = y0;
    x_size_ = xSize;
    y_size_ = ySize;
    x_min_ = x0_;
    x_max_ = x0_ + (cols_ - 3) * x_size_;
    y_min_ = y0_;
    y_max_ = y0_ + (rows_ - 3) * y_size_;
    z_min_ = zMin;
    z_max_ = zMax;

    const size_t totalCells = static_cast<size_t>(rows_ * cols_);
    data_.clear();
    data_.resize(totalCells);
    facets_.clear();
    facets_.resize(totalCells);
    body_bounds_.clear();
    body_bounds_.resize(totalCells);
    current_bound_col_ = nullptr;

    initialized_ = true;
    return true;
}

void Model::clear()
{
    initialized_ = false;
    rows_ = 0;
    cols_ = 0;
    bodies_.clear();
    data_.clear();
    facets_.clear();
    body_bounds_.clear();
    current_bound_col_ = nullptr;
    facets_update_.clear();
    next_body_id_ = 1;
}

size_t Model::count(int row, int col) const
{
    int idx = flat_index(row, col);
    if (idx < 0) return 0;
    return data_[idx].size();
}

int Model::upper_bound(int row, int col) const
{
    int cnt = static_cast<int>(count(row, col));
    return cnt - 1;
}

const ColumnPoint *Model::at(int row, int col, int index) const
{
    int idx = flat_index(row, col);
    if (idx < 0 || index < 0 || static_cast<size_t>(index) >= data_[idx].size()) {
        return nullptr;
    }
    return &data_[idx][index];
}

ColumnPoint *Model::at(int row, int col, int index)
{
    int idx = flat_index(row, col);
    if (idx < 0 || index < 0 || static_cast<size_t>(index) >= data_[idx].size()) {
        return nullptr;
    }
    return &data_[idx][index];
}

void Model::add(int row, int col, const ColumnPoint &pt)
{
    int idx = flat_index(row, col);
    if (idx < 0) return;
    ColumnPoint pCopy = pt;
    set_column_point_coords(pCopy, row, col, pCopy.z().value());
    data_[idx].push_back(pCopy);
}

void Model::insert_at(int row, int col, int index, const ColumnPoint &pt)
{
    int idx = flat_index(row, col);
    if (idx < 0) return;
    ColumnPoint pCopy = pt;
    set_column_point_coords(pCopy, row, col, pCopy.z().value());
    if (index >= 0 && static_cast<size_t>(index) <= data_[idx].size()) {
        data_[idx].insert(data_[idx].begin() + index, pCopy);
    }
}

void Model::remove_at(int row, int col, int index, int count)
{
    int idx = flat_index(row, col);
    if (idx < 0) return;
    if (index >= 0 && static_cast<size_t>(index + count) <= data_[idx].size()) {
        data_[idx].erase(data_[idx].begin() + index, data_[idx].begin() + index + count);
    }
}

double Model::z(int row, int col, int index) const
{
    if (index < 0) return z_max();
    if (index > upper_bound(row, col)) return z_min();
    const ColumnPoint *pt = at(row, col, index);
    return pt ? pt->z().value() : 0.0;
}

void Model::set_z(int row, int col, int index, double z_val)
{
    ColumnPoint *pt = at(row, col, index);
    if (pt) {
        pt->set_z(z_val);
    }
}

double Model::thickness(int row, int col, int index) const
{
    if (index <= 0 || (index % 2) == 0) return 0.0;
    const ColumnPoint *top = at(row, col, index);
    const ColumnPoint *bot = at(row, col, index + 1);
    if (!top || !bot) return 0.0;
    return std::abs((top->z() - bot->z()).value());
}

Body *Model::create_body()
{
    auto b = std::make_unique<Body>();
    int newId = next_body_id_++;
    b->set_id(newId);
    static const BodyColor defaultPalette[] = {
        {180, 120, 80, 255},  // Sandstone
        {220, 80, 60, 255},   // Granite
        {60, 140, 180, 255},  // Limestone
        {100, 180, 100, 255}, // Shale
        {180, 100, 180, 255}, // Basalt
        {220, 180, 60, 255},  // Alluvium
        {80, 80, 90, 255}     // Gabbro
    };
    int pIdx = (newId - 1) % 7;
    if (pIdx < 0) pIdx = 0;
    b->set_color(defaultPalette[pIdx]);
    Body *raw = b.get();
    bodies_.push_back(std::move(b));
    return raw;
}

void Model::add_body(std::unique_ptr<Body> body)
{
    if (body) {
        if (body->id() >= next_body_id_) {
            next_body_id_ = body->id() + 1;
        }
        bodies_.push_back(std::move(body));
    }
}

Body *Model::body(int bodyId)
{
    for (auto &b : bodies_) {
        if (b->id() == bodyId) return b.get();
    }
    return nullptr;
}

const Body *Model::body(int bodyId) const
{
    for (const auto &b : bodies_) {
        if (b->id() == bodyId) return b.get();
    }
    return nullptr;
}

void Model::compute_body_magnetization_vector(const Point3D &indFld)
{
    for (auto &b : bodies_) {
        if (b) {
            b->compute_magnetization_vector(indFld);
        }
    }
}

void Model::compute_body_magnetization_vector(double incDeg, double decDeg, double intensityNT)
{
    compute_body_magnetization_vector(Point3D(incDeg, decDeg, intensityNT));
}

int Model::body_index(int bodyId, int row, int col) const
{
    const size_t cnt = count(row, col);
    for (size_t i = 1; i < cnt; i += 2) {
        const ColumnPoint *pt = at(row, col, static_cast<int>(i));
        if (pt && pt->body_id() == bodyId) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int Model::update_move_index(int indexOld, int row, int col, int bodyId) const
{
    int iPlus = (indexOld + 1) % 2;
    int indexMax = static_cast<int>(count(row, col));
    int i = 1;
    while (i < indexMax) {
        if (bodyId == id(i, row, col)) {
            return i + iPlus;
        }
        i += 2;
    }
    return -1;
}

int Model::id(int index, int row, int col) const
{
    const int cnt = static_cast<int>(count(row, col));
    if (index > 0 && index < cnt - 1) {
        const ColumnPoint *pt = at(row, col, index);
        if (pt) return pt->body_id();
    }
    return -1;
}

bool Model::is_ins_pos_ok(int index, int bodyId, int row, int col) const
{
    const int cnt = static_cast<int>(count(row, col));
    for (int i = 0; i < cnt - 1; i += 2) {
        if (bodyId == id(i, row, col)) {
            return false;
        }
    }

    for (int i = 1; i < cnt - 1; i += 2) {
        int tag = (i < index) ? 1 : 0;
        int idCheck = id(i, row, col);
        if (!is_ins_pos_ok_1(tag, bodyId, idCheck, row - 1, col)) return false;
        if (!is_ins_pos_ok_1(tag, bodyId, idCheck, row + 1, col)) return false;
        if (!is_ins_pos_ok_1(tag, bodyId, idCheck, row, col - 1)) return false;
        if (!is_ins_pos_ok_1(tag, bodyId, idCheck, row, col + 1)) return false;
        if (!is_ins_pos_ok_1(tag, bodyId, idCheck, row + 1, col + 1)) return false;
        if (!is_ins_pos_ok_1(tag, bodyId, idCheck, row - 1, col - 1)) return false;
        if (!is_ins_pos_ok_1(tag, bodyId, idCheck, row + 1, col - 1)) return false;
        if (!is_ins_pos_ok_1(tag, bodyId, idCheck, row - 1, col + 1)) return false;
    }
    return true;
}

bool Model::is_ins_pos_ok_1(int tag, int bodyId, int idCheck, int row, int col) const
{
    if (row < 0 || col < 0 || row >= rows_ || col >= cols_) return true;
    const int cnt = static_cast<int>(count(row, col));
    for (int i = 0; i < cnt - 1; i += 2) {
        if (bodyId == id(i, row, col)) {
            return is_ins_pos_ok_2(tag, bodyId, idCheck, row, col);
        }
    }
    return true;
}

bool Model::is_ins_pos_ok_2(int tag, int bodyId, int idCheck, int row, int col) const
{
    int tagHere = 0;
    const int cnt = static_cast<int>(count(row, col));
    for (int i = 0; i < cnt - 1; i += 2) {
        if (bodyId == id(i, row, col)) {
            tagHere = 1;
        }
        if (tag == tagHere && idCheck == id(i, row, col)) {
            return false;
        }
    }
    return true;
}

int Model::insert_body(int row, int col, double z, double thickness, bool isNew, int bodyId, double zT, double zB, bool bZ)
{
    Body *pBody = nullptr;
    if (isNew) {
        pBody = create_body();
    } else {
        pBody = body(bodyId);
    }
    if (!pBody) return -1;
    bodyId = pBody->id();

    double t = (thickness == -1.0) ? default_thickness() : thickness;
    if (bZ) {
        zT = z + t;
        zB = z - t;
    } else {
        z = (zT + zB) / 2.0;
    }
    if (const_top_) zT = const_top_z_;
    if (const_bot_) zB = const_bot_z_;

    double zD = z_min_;
    double zU = this->z(row, col, 0);
    if (!(zD < z && z < zU)) return -1;

    int nIndex = 1;
    const int iMax = static_cast<int>(count(row, col));
    while (nIndex < iMax) {
        zU = this->z(row, col, nIndex - 1);
        zD = this->z(row, col, nIndex);
        if (zD < z && z < zU) break;
        nIndex += 2;
    }
    if (nIndex > iMax) return -1;

    if (!is_ins_pos_ok(nIndex, bodyId, row, col)) return -1;

    switch (creation_tag_) {
        case BodyCreateTag::None: break;
        case BodyCreateTag::JoinTop: zT = zU; break;
        case BodyCreateTag::JoinBot: zB = zD; break;
        case BodyCreateTag::JoinTopBot: zT = zU; zB = zD; break;
    }

    zT = std::min(zU, zT);
    zB = std::max(zD, zB);

    ColumnPoint ptBot(bodyId, zB, pBody);
    ColumnPoint ptTop(bodyId, zT, pBody);
    insert_at(row, col, nIndex, ptBot);
    insert_at(row, col, nIndex, ptTop);

    current_row_ = row;
    current_col_ = col;
    update_facet_list(row, col);

    return nIndex;
}

int Model::remove_body(int index, int row, int col)
{
    current_row_ = row;
    current_col_ = col;
    remove_at(row, col, index, 2);
    update_facet_list(row, col);
    return 1;
}

int Model::delete_body(int bodyId)
{
    for (int r = 0; r < rows_; ++r) {
        for (int c = 0; c < cols_; ++c) {
            int idx = body_index(bodyId, r, c);
            if (idx != -1) {
                remove_body(idx, r, c);
            }
        }
    }

    auto it = std::remove_if(bodies_.begin(), bodies_.end(), [bodyId](const auto &b) {
        return b->id() == bodyId;
    });
    if (it != bodies_.end()) {
        bodies_.erase(it, bodies_.end());
        bool oldRealTime = compute_real_time_;
        compute_real_time_ = false;
        init_facet_list();
        compute_real_time_ = oldRealTime;
        return 1;
    }
    return 0;
}

int Model::move_vertex(int &index, int row, int col, double zVal, BodyMoveType moveType)
{
    if (index <= 0) return -1;

    const int indexMax = static_cast<int>(count(row, col)) - 1;
    if (index >= indexMax) return -1;

    const double zR = z(row, col, 0);
    const double zC = z(row, col, index);

    // Identify contiguous cluster of vertices [i, j) that share the same Z as index
    int i = index;
    while (i > 1 && z(row, col, i - 1) == zC) {
        --i;
    }
    double zU = z(row, col, i - 1);

    int j = index + 1;
    while (j < indexMax + 1) {
        double nextZ = z(row, col, j);
        if (zC > nextZ) break;
        if (j == indexMax) break;
        j++;
    }
    double zD = z(row, col, j);

    current_row_ = row;
    current_col_ = col;

    if (moveType == BodyMoveType::Normal) {
        if (zVal >= zU) {
            zVal = zU;
            index--;
        }
        if (zVal <= zD) {
            zVal = zD;
        }
        while (i < j) {
            set_z(row, col, i, zVal);
            i++;
        }
        if (zVal == zR) {
            index = 1;
            return 0;
        }
        if (zVal == z_min_) {
            return 0;
        }
    } else if (moveType == BodyMoveType::Split) {
        if (zVal > zC) {
            double targetZ = std::min(zVal, zU);
            index = i;
            set_z(row, col, index, targetZ);
        } else if (zVal < zC) {
            double targetZ = std::max(zVal, zD);
            index = j - 1;
            set_z(row, col, index, targetZ);
        }
    } else if (moveType == BodyMoveType::Constrained) {
        if (zVal > zD && zVal < zU) {
            set_z(row, col, index, zVal);
        } else {
            return 2;
        }
    }

    update_facet_list(row, col);
    return 0;
}

// ---------------------------------------------------------------------------
// Facet Generation Engine
// ---------------------------------------------------------------------------

std::vector<Facet3Pt> *Model::facet_list(int row, int col)
{
    if (row < 0 || col < 0 || row > rows_ - 2 || col > cols_ - 2) return nullptr;
    int idx = row * cols_ + col;
    if (idx < static_cast<int>(facets_.size())) return &facets_[idx];
    return nullptr;
}

const std::vector<Facet3Pt> *Model::facet_list(int row, int col) const
{
    if (row < 0 || col < 0 || row > rows_ - 2 || col > cols_ - 2) return nullptr;
    int idx = row * cols_ + col;
    if (idx < static_cast<int>(facets_.size())) return &facets_[idx];
    return nullptr;
}

int Model::add_fct_gen(std::vector<FctGen> &fga, int row, int col, int tag)
{
    const int count = static_cast<int>(this->count(row, col));
    for (int i = 1; i < count - 1; i += 2) {
        int bodyId = id(i, row, col);
        bool added = false;
        for (auto &fg : fga) {
            if (fg.id == bodyId) {
                fg.idPrev[tag] = id(i - 1, row, col);
                fg.idNext[tag] = id(i + 1, row, col);
                fg.top[tag] = at(row, col, i)->point();
                fg.bot[tag] = at(row, col, i + 1)->point();
                fg.b[tag] = true;
                fg.l++;
                added = true;
                break;
            }
        }
        if (!added) {
            FctGen fg;
            fg.id = bodyId;
            fg.idPrev[tag] = id(i - 1, row, col);
            fg.idNext[tag] = id(i + 1, row, col);
            fg.top[tag] = at(row, col, i)->point();
            fg.bot[tag] = at(row, col, i + 1)->point();
            fg.b[tag] = true;
            fg.l = 1;
            fga.push_back(fg);
        }
    }
    return static_cast<int>(fga.size());
}

void Model::init_side_facets_bound(const FctGen &fg, std::vector<Facet3Pt> &facetList, int i0, int i1)
{
    if (fg.top[i0] == fg.bot[i0] && fg.top[i1] == fg.bot[i1]) return;

    if (current_bound_col_) {
        current_bound_col_->push_back({fg.id, fg.bot[i0], fg.bot[i1]});
    }

    Body *pBd = body(fg.id);
    if (fg.top[i0] != fg.bot[i0]) {
        auto &fct = facetList.emplace_back(fg.top[i0], fg.bot[i0], fg.top[i1]);
        fct.primary = pBd;
    }
    if (fg.top[i1] != fg.bot[i1]) {
        auto &fct = facetList.emplace_back(fg.bot[i0], fg.bot[i1], fg.top[i1]);
        fct.primary = pBd;
    }
}

void Model::fg_to_fct_bound(FctGen &fg, std::vector<Facet3Pt> &facetList)
{
    if (fg.l == 2) return;

    if (fg.b[0] && fg.b[1] && fg.top[0].y == y_min_ - ex_s_) init_side_facets_bound(fg, facetList, 0, 1);
    if (fg.b[2] && fg.b[3] && fg.top[2].y == y_max_ + ex_n_) init_side_facets_bound(fg, facetList, 2, 3);
    if (fg.b[3] && fg.b[0] && fg.top[3].x == x_min_ - ex_w_) init_side_facets_bound(fg, facetList, 3, 0);
    if (fg.b[1] && fg.b[2] && fg.top[1].x == x_max_ + ex_e_) init_side_facets_bound(fg, facetList, 1, 2);
}

void Model::init_side_facets_2(const FctGen &fg, std::vector<Facet3Pt> &facetList, int i0, int i1)
{
    if (fg.b[0] && fg.b[1] && fg.top[0].y == y_min_ - ex_s_) return;
    if (fg.b[2] && fg.b[3] && fg.top[2].y == y_max_ + ex_n_) return;
    if (fg.b[3] && fg.b[0] && fg.top[3].x == x_min_ - ex_w_) return;
    if (fg.b[1] && fg.b[2] && fg.top[1].x == x_max_ + ex_e_) return;

    if (fg.b[i0] && fg.b[i1] && ((fg.top[i0] != fg.bot[i0]) || (fg.top[i1] != fg.bot[i1]))) {
        if (current_bound_col_) {
            current_bound_col_->push_back({fg.id, fg.bot[i0], fg.bot[i1]});
        }
        Body *pBd = body(fg.id);
        if (fg.top[i0] != fg.bot[i0]) {
            auto &fct = facetList.emplace_back(fg.top[i0], fg.bot[i0], fg.top[i1]);
            fct.nType = FacetType::FCT_SIDE;
            fct.primary = pBd;
        }
        if (fg.top[i1] != fg.bot[i1]) {
            auto &fct = facetList.emplace_back(fg.bot[i0], fg.bot[i1], fg.top[i1]);
            fct.nType = FacetType::FCT_SIDE;
            fct.primary = pBd;
        }
    }
}

void Model::init_side_facets_3(const FctGen &fg, std::vector<Facet3Pt> &facetList, int i0, int i1)
{
    if (current_bound_col_) {
        current_bound_col_->push_back({fg.id, fg.bot[i0], fg.bot[i1]});
    }

    if ((fg.top[i0] != fg.bot[i0]) || (fg.top[i1] != fg.bot[i1])) {
        Body *pBd = body(fg.id);
        if (fg.top[i0] != fg.bot[i0]) {
            auto &fct = facetList.emplace_back(fg.top[i0], fg.bot[i0], fg.top[i1]);
            fct.nType = FacetType::FCT_SIDE;
            fct.primary = pBd;
        }
        if (fg.top[i1] != fg.bot[i1]) {
            auto &fct = facetList.emplace_back(fg.bot[i0], fg.bot[i1], fg.top[i1]);
            fct.nType = FacetType::FCT_SIDE;
            fct.primary = pBd;
        }
    }
}

void Model::init_side_facets(const FctGen &fg, std::vector<Facet3Pt> &facetList)
{
    if (fg.l == 2) {
        init_side_facets_2(fg, facetList, 1, 0);
        init_side_facets_2(fg, facetList, 2, 1);
        init_side_facets_2(fg, facetList, 3, 2);
        init_side_facets_2(fg, facetList, 0, 3);
    }
    if (fg.l == 3) {
        if (fg.b[0] && fg.b[1] && fg.b[2]) {
            init_side_facets_3(fg, facetList, 2, 4);
            init_side_facets_3(fg, facetList, 4, 0);
        }
        if (fg.b[1] && fg.b[2] && fg.b[3]) {
            init_side_facets_3(fg, facetList, 3, 4);
            init_side_facets_3(fg, facetList, 4, 1);
        }
        if (fg.b[2] && fg.b[3] && fg.b[0]) {
            init_side_facets_3(fg, facetList, 0, 4);
            init_side_facets_3(fg, facetList, 4, 2);
        }
        if (fg.b[3] && fg.b[0] && fg.b[1]) {
            init_side_facets_3(fg, facetList, 1, 4);
            init_side_facets_3(fg, facetList, 4, 3);
        }
    }
}

void Model::init_facet(const FctGen &pU, const FctGen &pL, std::vector<Facet3Pt> &facetList, int i0, int i1)
{
    const int i2 = 4;

    if (pU.bot[i0] == pL.top[i0] && pU.bot[i1] == pL.top[i1] && pU.bot[i2] == pL.top[i2]) {
        auto &fct = facetList.emplace_back(pL.top[i0], pL.top[i1], pL.top[i2]);
        fct.primary = body(pL.id);
        fct.opposite = body(pU.id);
    } else {
        if (pU.b[i0] && pU.b[i1]) {
            auto &fct = facetList.emplace_back(pU.bot[i0], pU.bot[i2], pU.bot[i1]);
            fct.primary = body(pU.id);
        }
        if (pL.b[i0] && pL.b[i1]) {
            auto &fct = facetList.emplace_back(pL.top[i0], pL.top[i1], pL.top[i2]);
            fct.primary = body(pL.id);
        }
    }
}

void Model::init_facet_tbm(const FctGen &fg, std::vector<Facet3Pt> &facetList, int i0, int i1, FgType tag)
{
    const int i2 = 4;
    if (tag == FgType::TopMost) {
        auto &fct = facetList.emplace_back(fg.top[i0], fg.top[i1], fg.top[i2]);
        fct.primary = body(fg.id);
    } else if (tag == FgType::BotMost) {
        auto &fct = facetList.emplace_back(fg.bot[i0], fg.bot[i2], fg.bot[i1]);
        fct.primary = body(fg.id);
    }
}

void Model::fg_to_fct(std::vector<FctGen> &fga, std::vector<Facet3Pt> &facetList, int i0, int i1)
{
    const int i2 = 4;
    const size_t n = fga.size();
    size_t i = 0;
    bool bFirst = true;
    FctGen *pB = nullptr;

    while (i < n) {
        FctGen *pC = &fga[i];
        i++;
        if (pC->b[i0] && pC->b[i1] &&
            (pC->top[i0] != pC->bot[i0] || pC->top[i1] != pC->bot[i1] || pC->top[i2] != pC->bot[i2]))
        {
            if (bFirst) {
                init_facet_tbm(*pC, facetList, i0, i1, FgType::TopMost);
                bFirst = false;
            }
            pB = pC;
            while (i < n) {
                FctGen *pN = &fga[i];
                if (pN->b[i0] && pN->b[i1] &&
                    (pN->top[i0] != pN->bot[i0] || pN->top[i1] != pN->bot[i1] || pN->top[i2] != pN->bot[i2]))
                {
                    init_facet(*pC, *pN, facetList, i0, i1);
                    pB = pN;
                    break;
                } else {
                    i++;
                }
            }
        }
    }
    if (pB) {
        init_facet_tbm(*pB, facetList, i0, i1, FgType::BotMost);
    }
}

int Model::generate_facets(std::vector<Facet3Pt> &facetList, int row, int col)
{
    if (row < 0 || col < 0 || row > rows_ - 2 || col > cols_ - 2) return 0;

    std::vector<FctGen> fga;
    fga.reserve(16);

    add_fct_gen(fga, row, col, 0);
    add_fct_gen(fga, row, col + 1, 1);
    add_fct_gen(fga, row + 1, col + 1, 2);
    add_fct_gen(fga, row + 1, col, 3);

    if (fga.empty()) return 0;

    for (size_t i = 0; i < fga.size(); ) {
        FctGen &fg = fga[i];
        fg_to_fct_bound(fg, facetList);
        if (fg.l <= 2) {
            if (fg.l == 2) init_side_facets(fg, facetList);
            fga.erase(fga.begin() + i);
        } else {
            double t = 0.0, b = 0.0;
            int l = 0;
            for (int k = 0; k < 4; ++k) {
                if (fg.b[k]) {
                    t += fg.top[k].z;
                    b += fg.bot[k].z;
                    l++;
                }
            }
            if ((t - b) == 0.0) {
                fga.erase(fga.begin() + i);
            } else {
                fg.top[4].x = fg.bot[4].x = x2e(col);
                fg.top[4].y = fg.bot[4].y = y2e(row);
                fg.top[4].z = t / l;
                fg.bot[4].z = b / l;
                if (fg.l == 3 &&
                    ((fg.bot[0].z == fg.top[0].z && fg.bot[2].z == fg.top[2].z && fg.b[0] && fg.b[2]) ||
                     (fg.bot[1].z == fg.top[1].z && fg.bot[3].z == fg.top[3].z && fg.b[1] && fg.b[3])))
                {
                    fg.top[4].z = fg.bot[4].z = (t + b) / (2.0 * l);
                }
                i++;
            }
        }
    }

    if (fga.empty()) return 0;

    // Sort in ascending Z (shallower depth first)
    std::sort(fga.begin(), fga.end(), [](const FctGen &f1, const FctGen &f2) {
        for (int i = 0; i < 4; ++i) {
            if (f1.top[i].z > f2.bot[i].z && f1.b[i] && f2.b[i]) return true;
            if (f1.bot[i].z < f2.top[i].z && f1.b[i] && f2.b[i]) return false;
        }
        return f1.id < f2.id;
    });

    const size_t n = fga.size();
    for (size_t i = 0; i < n; ++i) {
        FctGen &pC = fga[i];
        if (i < n - 1) {
            FctGen &pN = fga[i + 1];
            bool b01 = (pC.bot[0].z == pN.top[0].z) && (pC.bot[1].z == pN.top[1].z);
            bool b12 = (pC.bot[1].z == pN.top[1].z) && (pC.bot[2].z == pN.top[2].z);
            bool b23 = (pC.bot[2].z == pN.top[2].z) && (pC.bot[3].z == pN.top[3].z);
            bool b30 = (pC.bot[3].z == pN.top[3].z) && (pC.bot[0].z == pN.top[0].z);
            if ((pC.l == 3 || pN.l == 3) &&
                ((pC.b[0] && pN.b[0] && pC.b[1] && pN.b[1] && b01) ||
                 (pC.b[1] && pN.b[1] && pC.b[2] && pN.b[2] && b12) ||
                 (pC.b[2] && pN.b[2] && pC.b[3] && pN.b[3] && b23) ||
                 (pC.b[3] && pN.b[3] && pC.b[0] && pN.b[0] && b30)))
            {
                if (pN.top[4].z == pN.bot[4].z) {
                    pN.top[4].z = pN.bot[4].z = pC.bot[4].z;
                }
                pN.top[4].z = pC.bot[4].z;
                if (pN.top[4].z < pN.bot[4].z) pN.bot[4].z = pN.top[4].z;
            }
        }
        init_side_facets(pC, facetList);
    }

    fg_to_fct(fga, facetList, 0, 1);
    fg_to_fct(fga, facetList, 1, 2);
    fg_to_fct(fga, facetList, 2, 3);
    fg_to_fct(fga, facetList, 3, 0);

    return static_cast<int>(facetList.size());
}

void Model::update_facet_column(int row, int col)
{
    std::vector<Facet3Pt> *pFctLst = facet_list(row, col);
    if (!pFctLst) return;

    int bIdx = row * cols_ + col;
    if (bIdx >= 0 && bIdx < static_cast<int>(body_bounds_.size())) {
        body_bounds_[bIdx].clear();
        current_bound_col_ = &body_bounds_[bIdx];
    } else {
        current_bound_col_ = nullptr;
    }

    if (compute_real_time_) {
        for (auto &fct : *pFctLst) {
            Facet3Pt fCopy = fct;
            fCopy.set_sign(-1.0);
            facets_update_.push_back(fCopy);
        }
    }

    pFctLst->clear();
    generate_facets(*pFctLst, row, col);

    if (compute_real_time_) {
        for (const auto &fct : *pFctLst) {
            Facet3Pt fCopy = fct;
            fCopy.set_sign(1.0);
            facets_update_.push_back(fCopy);
        }
    }
}

void Model::update_facet_list(int row, int col)
{
    update_facet_column(row - 1, col - 1);
    update_facet_column(row - 1, col);
    update_facet_column(row, col - 1);
    update_facet_column(row, col);
}

void Model::init_facet_list()
{
    for (int r = 0; r < rows_ - 1; ++r) {
        for (int c = 0; c < cols_ - 1; ++c) {
            update_facet_column(r, c);
            std::vector<Facet3Pt> *lst = facet_list(r, c);
            if (lst) {
                for (auto &fct : *lst) {
                    fct.set_sign(-1.0);
                }
            }
        }
    }
}

int Model::facets_computation(std::vector<Facet3Pt> &outList) const
{
    outList.clear();
    for (int r = 0; r < rows_ - 1; ++r) {
        for (int c = 0; c < cols_ - 1; ++c) {
            const std::vector<Facet3Pt> *lst = facet_list(r, c);
            if (lst) {
                for (const auto &fct : *lst) {
                    Facet3Pt copy = fct;
                    copy.set_sign(1.0);
                    outList.push_back(copy);
                }
            }
        }
    }
    return static_cast<int>(outList.size());
}

int Model::body_facets(int bodyId, std::vector<Facet3Pt> &outList, bool includeOuter) const
{
    outList.clear();
    int rMin = 0;
    int rMax = rows_ - 2;
    int cMin = 0;
    int cMax = cols_ - 2;

    if (!includeOuter && rows_ > 3 && cols_ > 3) {
        rMin = 1;
        rMax = rows_ - 3;
        cMin = 1;
        cMax = cols_ - 3;
    }

    for (int r = rMin; r <= rMax; ++r) {
        for (int c = cMin; c <= cMax; ++c) {
            const auto *lst = facet_list(r, c);
            if (!lst) continue;
            for (const auto &fct : *lst) {
                if (bodyId == -1) {
                    outList.push_back(fct);
                } else {
                    if (fct.primary && fct.primary->id() == bodyId) {
                        outList.push_back(fct);
                    } else if (fct.opposite && fct.opposite->id() == bodyId) {
                        // Contact interface: invert winding and normal so it faces outward from this body
                        Facet3Pt copy = fct;
                        copy.reverse();
                        outList.push_back(copy);
                    }
                }
            }
        }
    }
    return static_cast<int>(outList.size());
}

int Model::facets_for_3d_view(std::vector<Facet3Pt> &outList, bool includeOuter) const
{
    return body_facets(-1, outList, includeOuter);
}

std::vector<BodyMesh3D> Model::generate_body_meshes(int bodyId, bool includeOuter) const
{
    std::vector<BodyMesh3D> meshes;
    for (const auto &bPtr : bodies_) {
        if (!bPtr) continue;
        int bId = bPtr->id();
        if (bodyId != -1 && bId != bodyId) continue;

        BodyMesh3D mesh;
        mesh.bodyId = bId;
        mesh.bodyName = bPtr->name();
        mesh.color = bPtr->color();
        mesh.alpha = bPtr->is_transparent() ? bPtr->transparency() : 1.0f;
        mesh.isVisible = bPtr->is_visible();
        mesh.isTransparent = bPtr->is_transparent();

        // Get watertight facets for this body
        body_facets(bId, mesh.facets, includeOuter);

        // Extract feature crease & boundary edges for crisp CAD rendering
        struct CanonicalEdge {
            long long x0, y0, z0;
            long long x1, y1, z1;
            Point3D pt0, pt1;

            bool operator<(const CanonicalEdge &o) const {
                if (x0 != o.x0) return x0 < o.x0;
                if (y0 != o.y0) return y0 < o.y0;
                if (z0 != o.z0) return z0 < o.z0;
                if (x1 != o.x1) return x1 < o.x1;
                if (y1 != o.y1) return y1 < o.y1;
                return z1 < o.z1;
            }
        };

        struct EdgeInfo {
            Point3D pt0, pt1;
            std::vector<Point3D> facetNormals;
        };

        std::map<CanonicalEdge, EdgeInfo> edgeMap;

        auto quant = [](double v) -> long long {
            return static_cast<long long>(std::round(v * 100.0));
        };

        for (const auto &f : mesh.facets) {
            for (int k = 0; k < 3; ++k) {
                const Point3D &pA = f[k];
                const Point3D &pB = f[(k + 1) % 3];

                long long qAx = quant(pA.x), qAy = quant(pA.y), qAz = quant(pA.z);
                long long qBx = quant(pB.x), qBy = quant(pB.y), qBz = quant(pB.z);

                CanonicalEdge key;
                Point3D startPt, endPt;
                if (std::tie(qAx, qAy, qAz) <= std::tie(qBx, qBy, qBz)) {
                    key = { qAx, qAy, qAz, qBx, qBy, qBz, pA, pB };
                    startPt = pA; endPt = pB;
                } else {
                    key = { qBx, qBy, qBz, qAx, qAy, qAz, pB, pA };
                    startPt = pB; endPt = pA;
                }

                auto &info = edgeMap[key];
                info.pt0 = startPt;
                info.pt1 = endPt;
                info.facetNormals.push_back(f.normal());
            }
        }

        mesh.featureEdges.reserve(edgeMap.size() / 2);
        for (const auto &[key, info] : edgeMap) {
            if (info.facetNormals.size() == 1) {
                // Boundary / silhouette edge
                mesh.featureEdges.push_back({info.pt0, info.pt1});
            } else if (info.facetNormals.size() >= 2) {
                // Dihedral angle check between adjacent facets: crease edge if angle > ~20 deg
                double dot = info.facetNormals[0].dot(info.facetNormals[1]);
                if (dot < 0.94) {
                    mesh.featureEdges.push_back({info.pt0, info.pt1});
                }
            }
        }

        meshes.push_back(std::move(mesh));
    }
    return meshes;
}

const std::vector<BodyBoundarySegment> *Model::body_bound_at(int row, int col) const
{
    if (row < 0 || col < 0 || row > rows_ - 2 || col > cols_ - 2) return nullptr;
    int idx = row * cols_ + col;
    if (idx >= 0 && idx < static_cast<int>(body_bounds_.size())) {
        return &body_bounds_[idx];
    }
    return nullptr;
}

std::vector<BodyBoundarySegment> Model::all_body_boundaries() const
{
    std::vector<BodyBoundarySegment> all;
    for (const auto &colBounds : body_bounds_) {
        all.insert(all.end(), colBounds.begin(), colBounds.end());
    }
    return all;
}

} // namespace mod3d
