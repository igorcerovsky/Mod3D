#include "mod3d/Model.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <map>
#include <queue>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

namespace mod3d {

Model::Model() = default;

int Model::flat_index(int row, int col) const {
  if (row < 0 || col < 0 || row >= rows_ || col >= cols_) {
    return -1;
  }
  return row * cols_ + col;
}

units::Meters Model::xe(int col) const {
  if (col == 0)
    return x0_ - ex_w_;
  if (col == cols_ - 1)
    return x0_ + (col - 2) * x_size_ + ex_e_;
  return x0_ + (col - 1) * x_size_;
}

units::Meters Model::ye(int row) const {
  if (row == 0)
    return y0_ - ex_s_;
  if (row == rows_ - 1)
    return y0_ + (row - 2) * y_size_ + ex_n_;
  return y0_ + (row - 1) * y_size_;
}

units::Meters Model::x2e(int col) const {
  if (col == 0)
    return x0_ - ex_w_ / 2.0;
  if (col == cols_ - 2)
    return x0_ + (col - 1) * x_size_ + ex_e_ / 2.0;
  return x0_ + (col - 1) * x_size_ + x_size_ / 2.0;
}

units::Meters Model::y2e(int row) const {
  if (row == 0)
    return y0_ - ex_s_ / 2.0;
  if (row == rows_ - 2)
    return y0_ + (row - 1) * y_size_ + ex_n_ / 2.0;
  return y0_ + (row - 1) * y_size_ + y_size_ / 2.0;
}

double Model::xe_val(int col) const { return xe(col).value(); }

double Model::ye_val(int row) const { return ye(row).value(); }

double Model::x2e_val(int col) const { return x2e(col).value(); }

double Model::y2e_val(int row) const { return y2e(row).value(); }

void Model::set_column_point_coords(ColumnPoint &pt, int row, int col,
                                    double z) {
  pt.set_coords(xe(col), ye(row), units::Meters(z));
}

void Model::set_column_point_coords(ColumnPoint &pt, int row, int col,
                                    units::Meters z) {
  pt.set_coords(xe(col), ye(row), z);
}

bool Model::init(int nRows, int nCols, units::Meters x0, units::Meters y0,
                 units::Meters xSize, units::Meters ySize, units::Meters zMin,
                 units::Meters zMax) {
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

  facets_update_.clear();
  initialized_ = true;
  return true;
}

bool Model::init(const Grid &reliefGrid, units::Meters zMin,
                 units::Meters zMax) {
  const int nRows = static_cast<int>(reliefGrid.rows());
  const int nCols = static_cast<int>(reliefGrid.cols());
  rows_ = nRows + 2;
  cols_ = nCols + 2;
  x0_ = units::Meters(reliefGrid.x0());
  y0_ = units::Meters(reliefGrid.y0());
  x_size_ = units::Meters(reliefGrid.dx());
  y_size_ = units::Meters(reliefGrid.dy());
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

  facets_update_.clear();
  initialized_ = true;
  return true;
}

bool Model::init_empty(int totalRows, int totalCols, units::Meters x0,
                       units::Meters y0, units::Meters xSize,
                       units::Meters ySize, units::Meters zMin,
                       units::Meters zMax) {
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

void Model::clear() {
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

size_t Model::count(int row, int col) const {
  int idx = flat_index(row, col);
  if (idx < 0)
    return 0;
  return data_[idx].size();
}

int Model::upper_bound(int row, int col) const {
  int cnt = static_cast<int>(count(row, col));
  return cnt - 1;
}

const ColumnPoint *Model::at(int row, int col, int index) const {
  int idx = flat_index(row, col);
  if (idx < 0 || index < 0 || static_cast<size_t>(index) >= data_[idx].size()) {
    return nullptr;
  }
  return &data_[idx][index];
}

ColumnPoint *Model::at(int row, int col, int index) {
  int idx = flat_index(row, col);
  if (idx < 0 || index < 0 || static_cast<size_t>(index) >= data_[idx].size()) {
    return nullptr;
  }
  return &data_[idx][index];
}

void Model::add(int row, int col, const ColumnPoint &pt) {
  int idx = flat_index(row, col);
  if (idx < 0)
    return;
  ColumnPoint pCopy = pt;
  set_column_point_coords(pCopy, row, col, pCopy.z().value());
  data_[idx].push_back(pCopy);
}

void Model::insert_at(int row, int col, int index, const ColumnPoint &pt) {
  int idx = flat_index(row, col);
  if (idx < 0)
    return;
  ColumnPoint pCopy = pt;
  set_column_point_coords(pCopy, row, col, pCopy.z().value());
  if (index >= 0 && static_cast<size_t>(index) <= data_[idx].size()) {
    data_[idx].insert(data_[idx].begin() + index, pCopy);
  }
}

void Model::remove_at(int row, int col, int index, int count) {
  int idx = flat_index(row, col);
  if (idx < 0)
    return;
  if (index >= 0 && static_cast<size_t>(index + count) <= data_[idx].size()) {
    data_[idx].erase(data_[idx].begin() + index,
                     data_[idx].begin() + index + count);
  }
}

double Model::z(int row, int col, int index) const {
  if (index < 0)
    return z_max_val();
  if (index > upper_bound(row, col))
    return z_min_val();
  const ColumnPoint *pt = at(row, col, index);
  return pt ? pt->z().value() : 0.0;
}

void Model::set_z(int row, int col, int index, double z_val) {
  ColumnPoint *pt = at(row, col, index);
  if (pt) {
    pt->set_z(z_val);
  }
}

bool Model::would_vertical_range_adjust_bodies(
    units::Meters zMin, units::Meters zMax) const noexcept {
  const double zMinVal = zMin.value();
  const double zMaxVal = zMax.value();
  for (int r = 0; r < rows_; ++r) {
    for (int c = 0; c < cols_; ++c) {
      const auto &colVec = data_[r * cols_ + c];
      for (size_t k = 1; k + 1 < colVec.size(); ++k) {
        const double zk = colVec[k].z().value();
        if (zk > zMaxVal + 1e-9 || zk < zMinVal - 1e-9) {
          return true;
        }
      }
    }
  }
  return false;
}

void Model::set_vertical_range(units::Meters zMin, units::Meters zMax) {
  if (zMin >= zMax) {
    return;
  }
  const double oldZMax = z_max_.value();
  bool flatTopAtOldZMax = (rows_ > 0 && cols_ > 0);
  for (int r = 0; r < rows_ && flatTopAtOldZMax; ++r) {
    for (int c = 0; c < cols_; ++c) {
      const ColumnPoint *ptTop = at(r, c, 0);
      if (!ptTop || std::abs(ptTop->z().value() - oldZMax) > 1e-5) {
        flatTopAtOldZMax = false;
        break;
      }
    }
  }

  z_min_ = zMin;
  z_max_ = zMax;
  const double zMinVal = z_min_.value();
  const double zMaxVal = z_max_.value();

  for (int r = 0; r < rows_; ++r) {
    for (int c = 0; c < cols_; ++c) {
      auto &colVec = data_[r * cols_ + c];
      if (colVec.empty())
        continue;

      double topBound = colVec.front().z().value();
      if (flatTopAtOldZMax || topBound > zMaxVal || topBound < zMinVal) {
        topBound =
            flatTopAtOldZMax ? zMaxVal : std::clamp(topBound, zMinVal, zMaxVal);
        set_column_point_coords(colVec.front(), r, c, topBound);
        colVec.front().set_z(topBound);
      }

      if (colVec.size() >= 2) {
        set_column_point_coords(colVec.back(), r, c, zMinVal);
        colVec.back().set_z(zMinVal);
      }

      double prevZ = topBound;
      for (size_t k = 1; k + 1 < colVec.size(); ++k) {
        double zk = std::clamp(colVec[k].z().value(), zMinVal, prevZ);
        set_column_point_coords(colVec[k], r, c, zk);
        colVec[k].set_z(zk);
        prevZ = zk;
      }
    }
  }

  if (initialized_) {
    init_facet_list();
  }
}

bool Model::set_surface_relief(const Grid &reliefGrid) {
  if (reliefGrid.empty()) {
    return false;
  }

  const int coreRows = rows_ - 2;
  const int coreCols = cols_ - 2;
  if (coreRows < 1 || coreCols < 1) {
    return false;
  }

  const int nRows = static_cast<int>(reliefGrid.rows());
  const int nCols = static_cast<int>(reliefGrid.cols());

  double maxReliefZ = -std::numeric_limits<double>::max();
  double minReliefZ = std::numeric_limits<double>::max();

  bool exactMatch = (nRows == coreRows && nCols == coreCols);

  for (int r = 0; r < rows_; ++r) {
    for (int c = 0; c < cols_; ++c) {
      double v = 0.0;
      if (exactMatch) {
        int ii = std::clamp(r - 1, 0, nRows - 1);
        int jj = std::clamp(c - 1, 0, nCols - 1);
        v = reliefGrid(ii, jj);
      } else {
        double x = xd_val(c);
        double y = yd_val(r);
        v = reliefGrid.interpolate(x, y);
        if (v == GRID_DUMMY) {
          double cx = std::clamp(x, reliefGrid.x_min().value(),
                                 reliefGrid.x_max().value());
          double cy = std::clamp(y, reliefGrid.y_min().value(),
                                 reliefGrid.y_max().value());
          v = reliefGrid.interpolate(cx, cy);
          if (v == GRID_DUMMY) {
            v = 0.5 * (reliefGrid.z_min().value() + reliefGrid.z_max().value());
          }
        }
      }

      if (r >= 1 && r <= coreRows && c >= 1 && c <= coreCols) {
        if (v > maxReliefZ)
          maxReliefZ = v;
        if (v < minReliefZ)
          minReliefZ = v;
      }

      auto &colVec = data_[r * cols_ + c];
      if (!colVec.empty()) {
        ColumnPoint &pTop = colVec.front();
        set_column_point_coords(pTop, r, c, v);
        pTop.set_z(v);
      }
    }
  }

  if (maxReliefZ > z_max_.value()) {
    z_max_ = units::Meters(std::ceil(maxReliefZ));
  }
  if (minReliefZ < z_min_.value()) {
    z_min_ = units::Meters(std::floor(minReliefZ) - 100.0);
  }

  const double zMinVal = z_min_.value();
  for (int r = 0; r < rows_; ++r) {
    for (int c = 0; c < cols_; ++c) {
      auto &colVec = data_[r * cols_ + c];
      if (colVec.empty())
        continue;
      if (colVec.size() >= 2) {
        ColumnPoint &pBot = colVec.back();
        pBot.set_z(z_min_);
        set_column_point_coords(pBot, r, c, z_min_);
      }
      double prevZ = colVec.front().z().value();
      for (size_t k = 1; k + 1 < colVec.size(); ++k) {
        double zk = std::clamp(colVec[k].z().value(), zMinVal, prevZ);
        colVec[k].set_z(zk);
        set_column_point_coords(colVec[k], r, c, zk);
        prevZ = zk;
      }
    }
  }

  init_facet_list();
  return true;
}

void Model::set_extensions(units::Meters exN, units::Meters exS,
                           units::Meters exE, units::Meters exW, bool enable) {
  ex_n_ = exN;
  ex_s_ = exS;
  ex_e_ = exE;
  ex_w_ = exW;
  extend_ = enable;

  if (!initialized_ || rows_ < 2 || cols_ < 2) {
    return;
  }

  // Resynchronize (x, y) coordinates on the outer extension ring
  for (int r = 0; r < rows_; ++r) {
    for (int c = 0; c < cols_; ++c) {
      if (r != 0 && r != rows_ - 1 && c != 0 && c != cols_ - 1)
        continue;
      auto &colVec = data_[r * cols_ + c];
      for (auto &pt : colVec) {
        set_column_point_coords(pt, r, c, pt.z());
      }
    }
  }

  // Update border facet cells if facets have already been generated
  bool anyFacets = false;
  for (const auto &fl : facets_) {
    if (!fl.empty()) {
      anyFacets = true;
      break;
    }
  }
  if (anyFacets) {
    for (int r = 0; r < rows_ - 1; ++r) {
      for (int c = 0; c < cols_ - 1; ++c) {
        if (r == 0 || r == rows_ - 2 || c == 0 || c == cols_ - 2) {
          update_facet_column(r, c);
        }
      }
    }
  }
}

double Model::thickness(int row, int col, int index) const {
  if (index <= 0 || (index % 2) == 0)
    return 0.0;
  const ColumnPoint *top = at(row, col, index);
  const ColumnPoint *bot = at(row, col, index + 1);
  if (!top || !bot)
    return 0.0;
  return std::abs((top->z() - bot->z()).value());
}

Body *Model::create_body() {
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
  if (pIdx < 0)
    pIdx = 0;
  b->set_color(defaultPalette[pIdx]);
  Body *raw = b.get();
  bodies_.push_back(std::move(b));
  return raw;
}

void Model::add_body(std::unique_ptr<Body> body) {
  if (body) {
    if (body->id() >= next_body_id_) {
      next_body_id_ = body->id() + 1;
    }
    bodies_.push_back(std::move(body));
  }
}

Body *Model::body(int bodyId) {
  for (auto &b : bodies_) {
    if (b->id() == bodyId)
      return b.get();
  }
  return nullptr;
}

const Body *Model::body(int bodyId) const {
  for (const auto &b : bodies_) {
    if (b->id() == bodyId)
      return b.get();
  }
  return nullptr;
}

void Model::compute_body_magnetization_vector(const Point3D &indFld) {
  for (auto &b : bodies_) {
    if (b) {
      b->compute_magnetization_vector(indFld);
    }
  }
}

void Model::compute_body_magnetization_vector(double incDeg, double decDeg,
                                              double intensityNT) {
  compute_body_magnetization_vector(Point3D(incDeg, decDeg, intensityNT));
}

int Model::body_index(int bodyId, int row, int col) const {
  const size_t cnt = count(row, col);
  for (size_t i = 1; i < cnt; i += 2) {
    const ColumnPoint *pt = at(row, col, static_cast<int>(i));
    if (pt && pt->body_id() == bodyId) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

int Model::update_move_index(int indexOld, int row, int col, int bodyId) const {
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

int Model::id(int index, int row, int col) const {
  const int cnt = static_cast<int>(count(row, col));
  if (index > 0 && index < cnt - 1) {
    const ColumnPoint *pt = at(row, col, index);
    if (pt)
      return pt->body_id();
  }
  return -1;
}

bool Model::is_ins_pos_ok(int index, int bodyId, int row, int col) const {
  const int cnt = static_cast<int>(count(row, col));
  for (int i = 0; i < cnt - 1; i += 2) {
    if (bodyId == id(i, row, col)) {
      return false;
    }
  }

  for (int i = 1; i < cnt - 1; i += 2) {
    int tag = (i < index) ? 1 : 0;
    int idCheck = id(i, row, col);
    if (!is_ins_pos_ok_1(tag, bodyId, idCheck, row - 1, col))
      return false;
    if (!is_ins_pos_ok_1(tag, bodyId, idCheck, row + 1, col))
      return false;
    if (!is_ins_pos_ok_1(tag, bodyId, idCheck, row, col - 1))
      return false;
    if (!is_ins_pos_ok_1(tag, bodyId, idCheck, row, col + 1))
      return false;
    if (!is_ins_pos_ok_1(tag, bodyId, idCheck, row + 1, col + 1))
      return false;
    if (!is_ins_pos_ok_1(tag, bodyId, idCheck, row - 1, col - 1))
      return false;
    if (!is_ins_pos_ok_1(tag, bodyId, idCheck, row + 1, col - 1))
      return false;
    if (!is_ins_pos_ok_1(tag, bodyId, idCheck, row - 1, col + 1))
      return false;
  }

  if (would_create_stratigraphic_cycle(index, bodyId, row, col))
    return false;

  return true;
}

bool Model::would_create_stratigraphic_cycle(int index, int bodyId, int row,
                                             int col) const {
  // Build global directed stratigraphic precedence graph G = (V, E)
  // Edge u -> v means body u lies above body v in at least one column.
  std::unordered_map<int, std::unordered_set<int>> adj;

  for (int r = 0; r < rows_; ++r) {
    for (int c = 0; c < cols_; ++c) {
      const int cnt = static_cast<int>(count(r, c));
      std::vector<int> colBodies;
      colBodies.reserve(static_cast<size_t>(cnt / 2 + 1));
      bool inserted = false;
      for (int i = 1; i < cnt - 1; i += 2) {
        if (r == row && c == col && i == index && !inserted) {
          colBodies.push_back(bodyId);
          inserted = true;
        }
        int bId = id(i, r, c);
        if (bId >= 0) {
          colBodies.push_back(bId);
        }
      }
      if (r == row && c == col && !inserted) {
        colBodies.push_back(bodyId);
      }

      for (size_t u = 0; u < colBodies.size(); ++u) {
        for (size_t v = u + 1; v < colBodies.size(); ++v) {
          if (colBodies[u] != colBodies[v]) {
            adj[colBodies[u]].insert(colBodies[v]);
          }
        }
      }
    }
  }

  // Check if adding `bodyId` at `index` in `(row, col)` creates a directed
  // cycle via Kahn's topological sort / cycle detection
  std::unordered_map<int, int> inDegree;
  for (const auto &[u, nbrs] : adj) {
    inDegree.try_emplace(u, 0);
    for (int v : nbrs) {
      inDegree[v]++;
    }
  }

  std::queue<int> q;
  for (const auto &[node, deg] : inDegree) {
    if (deg == 0) {
      q.push(node);
    }
  }

  size_t visited = 0;
  while (!q.empty()) {
    int u = q.front();
    q.pop();
    visited++;
    if (auto it = adj.find(u); it != adj.end()) {
      for (int v : it->second) {
        if (--inDegree[v] == 0) {
          q.push(v);
        }
      }
    }
  }

  return visited != inDegree.size();
}

bool Model::is_ins_pos_ok_1(int tag, int bodyId, int idCheck, int row,
                            int col) const {
  if (row < 0 || col < 0 || row >= rows_ || col >= cols_)
    return true;
  const int cnt = static_cast<int>(count(row, col));
  for (int i = 0; i < cnt - 1; i += 2) {
    if (bodyId == id(i, row, col)) {
      return is_ins_pos_ok_2(tag, bodyId, idCheck, row, col);
    }
  }
  return true;
}

bool Model::is_ins_pos_ok_2(int tag, int bodyId, int idCheck, int row,
                            int col) const {
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

int Model::insert_body(int row, int col, double z, double thickness, bool isNew,
                       int bodyId, double zT, double zB, bool bZ) {
  Body *pBody = nullptr;
  if (isNew) {
    pBody = create_body();
  } else {
    pBody = body(bodyId);
  }
  if (!pBody)
    return -1;
  bodyId = pBody->id();

  double t = (thickness == -1.0) ? default_thickness_val() : thickness;
  if (bZ) {
    zT = z + t;
    zB = z - t;
  } else {
    z = (zT + zB) / 2.0;
  }
  if (const_top_)
    zT = const_top_z_.value();
  if (const_bot_)
    zB = const_bot_z_.value();

  double zD = z_min_.value();
  double zU = this->z(row, col, 0);
  if (!(zD < z && z < zU))
    return -1;

  int nIndex = 1;
  const int iMax = static_cast<int>(count(row, col));
  while (nIndex < iMax) {
    zU = this->z(row, col, nIndex - 1);
    zD = this->z(row, col, nIndex);
    if (zD < z && z < zU)
      break;
    nIndex += 2;
  }
  if (nIndex > iMax)
    return -1;

  if (!is_ins_pos_ok(nIndex, bodyId, row, col))
    return -1;

  switch (creation_tag_) {
  case BodyCreateTag::None:
    break;
  case BodyCreateTag::JoinTop:
    zT = zU;
    break;
  case BodyCreateTag::JoinBot:
    zB = zD;
    break;
  case BodyCreateTag::JoinTopBot:
    zT = zU;
    zB = zD;
    break;
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

int Model::remove_body(int index, int row, int col) {
  current_row_ = row;
  current_col_ = col;
  remove_at(row, col, index, 2);
  update_facet_list(row, col);
  return 1;
}

int Model::delete_body(int bodyId) {
  bool oldRealTime = compute_real_time_;
  compute_real_time_ = false;

  for (int r = 0; r < rows_; ++r) {
    for (int c = 0; c < cols_; ++c) {
      int idx;
      while ((idx = body_index(bodyId, r, c)) != -1) {
        remove_at(r, c, idx, 2);
      }
    }
  }

  auto it =
      std::remove_if(bodies_.begin(), bodies_.end(),
                     [bodyId](const auto &b) { return b->id() == bodyId; });
  bool erased = false;
  if (it != bodies_.end()) {
    bodies_.erase(it, bodies_.end());
    erased = true;
  }

  init_facet_list();
  facets_update_.clear();
  compute_real_time_ = oldRealTime;

  return erased ? 1 : 0;
}

int Model::move_vertex(int &index, int row, int col, double zVal,
                       BodyMoveType moveType) {
  if (index <= 0)
    return -1;

  const int indexMax = static_cast<int>(count(row, col)) - 1;
  if (index >= indexMax)
    return -1;

  const double zR = z(row, col, 0);
  const double zC = z(row, col, index);

  // Identify contiguous cluster of vertices [i, j) that share the same Z as
  // index
  int i = index;
  while (i > 1 && z(row, col, i - 1) == zC) {
    --i;
  }
  double zU = z(row, col, i - 1);

  int j = index + 1;
  while (j < indexMax + 1) {
    double nextZ = z(row, col, j);
    if (zC > nextZ)
      break;
    if (j == indexMax)
      break;
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

std::vector<Facet3Pt> *Model::facet_list(int row, int col) {
  if (row < 0 || col < 0 || row > rows_ - 2 || col > cols_ - 2)
    return nullptr;
  int idx = row * cols_ + col;
  if (idx < static_cast<int>(facets_.size()))
    return &facets_[idx];
  return nullptr;
}

const std::vector<Facet3Pt> *Model::facet_list(int row, int col) const {
  if (row < 0 || col < 0 || row > rows_ - 2 || col > cols_ - 2)
    return nullptr;
  int idx = row * cols_ + col;
  if (idx < static_cast<int>(facets_.size()))
    return &facets_[idx];
  return nullptr;
}

int Model::add_fct_gen(std::vector<FctGen> &fga, int row, int col, int tag) {
  const int count = static_cast<int>(this->count(row, col));
  for (int i = 1; i < count - 1; i += 2) {
    int bodyId = id(i, row, col);
    bool added = false;
    for (auto &fg : fga) {
      if (fg.id == bodyId) {
        fg.idPrev[tag] = id(i - 1, row, col);
        fg.idNext[tag] = id(i + 1, row, col);
        fg.stratRank[tag] = i;
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
      fg.stratRank[tag] = i;
      fg.top[tag] = at(row, col, i)->point();
      fg.bot[tag] = at(row, col, i + 1)->point();
      fg.b[tag] = true;
      fg.l = 1;
      fga.push_back(fg);
    }
  }
  return static_cast<int>(fga.size());
}

void Model::init_side_facets_bound(const FctGen &fg,
                                   std::vector<Facet3Pt> &facetList, int i0,
                                   int i1) {
  if (current_bound_col_) {
    current_bound_col_->push_back({fg.id, fg.bot[i0], fg.bot[i1]});
  }

  if (fg.top[i0] == fg.bot[i0] && fg.top[i1] == fg.bot[i1])
    return;

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

void Model::fg_to_fct_bound(FctGen &fg, std::vector<Facet3Pt> &facetList) {
  if (fg.l == 2)
    return;

  // Use exact topological cell indices so zero-extension outer rings (ex_* ==
  // 0) never falsely trigger boundary closure on interior cells.
  if (fg.b[0] && fg.b[1] && current_row_ == 0)
    init_side_facets_bound(fg, facetList, 0, 1);
  if (fg.b[2] && fg.b[3] && current_row_ == rows_ - 2)
    init_side_facets_bound(fg, facetList, 2, 3);
  if (fg.b[3] && fg.b[0] && current_col_ == 0)
    init_side_facets_bound(fg, facetList, 3, 0);
  if (fg.b[1] && fg.b[2] && current_col_ == cols_ - 2)
    init_side_facets_bound(fg, facetList, 1, 2);
}

void Model::init_side_facets_2(const FctGen &fg,
                               std::vector<Facet3Pt> &facetList, int i0,
                               int i1) {
  // Only skip if the 2-corner edge lies on the outer model boundary
  if (fg.b[0] && fg.b[1] && current_row_ == 0)
    return;
  if (fg.b[2] && fg.b[3] && current_row_ == rows_ - 2)
    return;
  if (fg.b[3] && fg.b[0] && current_col_ == 0)
    return;
  if (fg.b[1] && fg.b[2] && current_col_ == cols_ - 2)
    return;

  if (fg.b[i0] && fg.b[i1]) {
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

void Model::init_side_facets_3(const FctGen &fg,
                               std::vector<Facet3Pt> &facetList, int i0,
                               int i1) {
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

void Model::init_side_facets(const FctGen &fg,
                             std::vector<Facet3Pt> &facetList) {
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

void Model::init_facet(const FctGen &pU, const FctGen &pL,
                       std::vector<Facet3Pt> &facetList, int i0, int i1) {
  const int i2 = 4;

  if (pU.bot[i0] == pL.top[i0] && pU.bot[i1] == pL.top[i1] &&
      pU.bot[i2] == pL.top[i2]) {
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

void Model::init_facet_tbm(const FctGen &fg, std::vector<Facet3Pt> &facetList,
                           int i0, int i1, FgType tag) {
  const int i2 = 4;
  if (tag == FgType::TopMost) {
    auto &fct = facetList.emplace_back(fg.top[i0], fg.top[i1], fg.top[i2]);
    fct.primary = body(fg.id);
  } else if (tag == FgType::BotMost) {
    auto &fct = facetList.emplace_back(fg.bot[i0], fg.bot[i2], fg.bot[i1]);
    fct.primary = body(fg.id);
  }
}

void Model::fg_to_fct(std::vector<FctGen> &fga,
                      std::vector<Facet3Pt> &facetList, int i0, int i1) {
  const int i2 = 4;
  const size_t n = fga.size();
  size_t i = 0;
  bool bFirst = true;
  FctGen *pB = nullptr;

  while (i < n) {
    FctGen *pC = &fga[i];
    i++;
    if (pC->b[i0] && pC->b[i1] &&
        (pC->top[i0] != pC->bot[i0] || pC->top[i1] != pC->bot[i1] ||
         pC->top[i2] != pC->bot[i2])) {
      if (bFirst) {
        init_facet_tbm(*pC, facetList, i0, i1, FgType::TopMost);
        bFirst = false;
      }
      pB = pC;
      while (i < n) {
        FctGen *pN = &fga[i];
        if (pN->b[i0] && pN->b[i1] &&
            (pN->top[i0] != pN->bot[i0] || pN->top[i1] != pN->bot[i1] ||
             pN->top[i2] != pN->bot[i2])) {
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

int Model::generate_facets(std::vector<Facet3Pt> &facetList, int row, int col) {
  if (row < 0 || col < 0 || row > rows_ - 2 || col > cols_ - 2)
    return 0;

  current_row_ = row;
  current_col_ = col;

  std::vector<FctGen> fga;
  fga.reserve(16);

  add_fct_gen(fga, row, col, 0);
  add_fct_gen(fga, row, col + 1, 1);
  add_fct_gen(fga, row + 1, col + 1, 2);
  add_fct_gen(fga, row + 1, col, 3);

  if (fga.empty())
    return 0;

  for (size_t i = 0; i < fga.size();) {
    FctGen &fg = fga[i];
    fg_to_fct_bound(fg, facetList);
    if (fg.l <= 2) {
      if (fg.l == 2)
        init_side_facets(fg, facetList);
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
        fg.top[4].x = fg.bot[4].x = x2e_val(col);
        fg.top[4].y = fg.bot[4].y = y2e_val(row);
        fg.top[4].z = t / l;
        fg.bot[4].z = b / l;
        if (fg.l == 3 && ((fg.bot[0].z == fg.top[0].z &&
                           fg.bot[2].z == fg.top[2].z && fg.b[0] && fg.b[2]) ||
                          (fg.bot[1].z == fg.top[1].z &&
                           fg.bot[3].z == fg.top[3].z && fg.b[1] && fg.b[3]))) {
          fg.top[4].z = fg.bot[4].z = (t + b) / (2.0 * l);
        }
        i++;
      }
    }
  }

  if (!fga.empty()) {
    // Sort in stratigraphic order (shallower depth first) using exact column
    // rank when both bodies exist in a shared corner, or mean elevation
    // fallback.
    std::sort(fga.begin(), fga.end(), [](const FctGen &f1, const FctGen &f2) {
      int f1Above = 0;
      int f2Above = 0;
      for (int i = 0; i < 4; ++i) {
        if (f1.b[i] && f2.b[i]) {
          if (f1.stratRank[i] >= 0 && f2.stratRank[i] >= 0 &&
              f1.stratRank[i] != f2.stratRank[i]) {
            if (f1.stratRank[i] < f2.stratRank[i])
              f1Above++;
            else
              f2Above++;
          } else if (f1.top[i].z > f2.bot[i].z && f1.bot[i].z >= f2.top[i].z) {
            f1Above++;
          } else if (f1.bot[i].z < f2.top[i].z && f1.top[i].z <= f2.bot[i].z) {
            f2Above++;
          }
        }
      }
      if (f1Above != f2Above)
        return f1Above > f2Above;

      double z1 = 0.5 * (f1.top[4].z + f1.bot[4].z);
      double z2 = 0.5 * (f2.top[4].z + f2.bot[4].z);
      if (std::abs(z1 - z2) > 1e-9)
        return z1 > z2;
      return f1.id < f2.id;
    });

    const size_t n = fga.size();
    for (size_t i = 0; i < n; ++i) {
      FctGen &pC = fga[i];
      // Search all subsequent bodies in the cell for a shared contact interface
      for (size_t j = i + 1; j < n; ++j) {
        FctGen &pN = fga[j];
        bool b01 = (pC.bot[0].z == pN.top[0].z) && (pC.bot[1].z == pN.top[1].z);
        bool b12 = (pC.bot[1].z == pN.top[1].z) && (pC.bot[2].z == pN.top[2].z);
        bool b23 = (pC.bot[2].z == pN.top[2].z) && (pC.bot[3].z == pN.top[3].z);
        bool b30 = (pC.bot[3].z == pN.top[3].z) && (pC.bot[0].z == pN.top[0].z);
        bool sharedEdge = (pC.b[0] && pN.b[0] && pC.b[1] && pN.b[1] && b01) ||
                          (pC.b[1] && pN.b[1] && pC.b[2] && pN.b[2] && b12) ||
                          (pC.b[2] && pN.b[2] && pC.b[3] && pN.b[3] && b23) ||
                          (pC.b[3] && pN.b[3] && pC.b[0] && pN.b[0] && b30);
        bool fullContact = (pC.l == 4 && pN.l == 4 && b01 && b12 && b23 && b30);
        if (fullContact || ((pC.l == 3 || pN.l == 3) && sharedEdge)) {
          if (pN.top[4].z == pN.bot[4].z) {
            pN.top[4].z = pN.bot[4].z = pC.bot[4].z;
          }
          pN.top[4].z = pC.bot[4].z;
          if (pN.top[4].z < pN.bot[4].z)
            pN.bot[4].z = pN.top[4].z;
        }
      }
      init_side_facets(pC, facetList);
    }

    fg_to_fct(fga, facetList, 0, 1);
    fg_to_fct(fga, facetList, 1, 2);
    fg_to_fct(fga, facetList, 2, 3);
    fg_to_fct(fga, facetList, 3, 0);
  }

  // Remove degenerate (zero-area) facets and fuse coincident opposite facets
  // within the cell into single shared-interface facets (primary + opposite).
  for (size_t i = 0; i < facetList.size();) {
    if (!facetList[i].pfld_facet().is_initialized() ||
        facetList[i].area() <= 1e-12) {
      facetList.erase(facetList.begin() + static_cast<std::ptrdiff_t>(i));
    } else {
      ++i;
    }
  }

  for (size_t i = 0; i < facetList.size(); ++i) {
    if (facetList[i].opposite != nullptr)
      continue;
    for (size_t j = i + 1; j < facetList.size(); ++j) {
      if (facetList[j].opposite != nullptr)
        continue;
      if (facetList[i].is_opposite(facetList[j])) {
        if (facetList[i].primary == facetList[j].primary) {
          // Internal seam between two parts of the same body cancels out
          facetList.erase(facetList.begin() + static_cast<std::ptrdiff_t>(j));
          facetList.erase(facetList.begin() + static_cast<std::ptrdiff_t>(i));
          --i;
          break;
        } else {
          // Fuse into a single shared facet with both primary and opposite
          // bodies
          facetList[i].opposite = facetList[j].primary;
          facetList.erase(facetList.begin() + static_cast<std::ptrdiff_t>(j));
          break;
        }
      }
    }
  }

  return static_cast<int>(facetList.size());
}

void Model::update_facet_column(int row, int col) {
  std::vector<Facet3Pt> *pFctLst = facet_list(row, col);
  if (!pFctLst)
    return;

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

void Model::update_facet_list(int row, int col) {
  update_facet_column(row - 1, col - 1);
  update_facet_column(row - 1, col);
  update_facet_column(row, col - 1);
  update_facet_column(row, col);
}

void Model::init_facet_list() {
  bool oldRealTime = compute_real_time_;
  compute_real_time_ = false;

  for (int r = 0; r < rows_ - 1; ++r) {
    for (int c = 0; c < cols_ - 1; ++c) {
      update_facet_column(r, c);
      std::vector<Facet3Pt> *lst = facet_list(r, c);
      if (lst) {
        for (auto &fct : *lst) {
          fct.set_sign(1.0);
        }
      }
    }
  }

  facets_update_.clear();
  compute_real_time_ = oldRealTime;
}

int Model::facets_computation(std::vector<Facet3Pt> &outList) const {
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

int Model::body_facets(int bodyId, std::vector<Facet3Pt> &outList,
                       bool includeOuter) const {
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

  auto hasBodyInCol = [this](int r, int c, int bId, Point3D &topPt,
                             Point3D &botPt) noexcept -> bool {
    const int idx = body_index(bId, r, c);
    if (idx < 1)
      return false;
    const ColumnPoint *pT = at(r, c, idx);
    const ColumnPoint *pB = at(r, c, idx + 1);
    if (!pT || !pB)
      return false;
    topPt = pT->point();
    botPt = pB->point();
    return true;
  };

  auto emitBoundaryQuad = [this, &outList](int bId, const Point3D &t0,
                                           const Point3D &b0, const Point3D &t1,
                                           const Point3D &b1) {
    if (t0 == b0 && t1 == b1)
      return;
    Body *pBd = const_cast<Body *>(body(bId));
    if (t0 != b0) {
      auto &fct = outList.emplace_back(t0, b0, t1);
      if (fct.pfld_facet().is_initialized() && fct.area() > 1e-12) {
        fct.primary = pBd;
      } else {
        outList.pop_back();
      }
    }
    if (t1 != b1) {
      auto &fct = outList.emplace_back(b0, b1, t1);
      if (fct.pfld_facet().is_initialized() && fct.area() > 1e-12) {
        fct.primary = pBd;
      } else {
        outList.pop_back();
      }
    }
  };

  const bool clipOuter = (!includeOuter && rows_ > 3 && cols_ > 3);

  for (int r = rMin; r <= rMax; ++r) {
    for (int c = cMin; c <= cMax; ++c) {
      Point3D t0, b0, t1, b1, t2, b2, t3, b3;
      bool has0 = false, has1 = false, has2 = false, has3 = false;
      int l = 0;
      if (clipOuter && bodyId != -1) {
        has0 = hasBodyInCol(r, c, bodyId, t0, b0);
        has1 = hasBodyInCol(r, c + 1, bodyId, t1, b1);
        has2 = hasBodyInCol(r + 1, c + 1, bodyId, t2, b2);
        has3 = hasBodyInCol(r + 1, c, bodyId, t3, b3);
        l = (has0 ? 1 : 0) + (has1 ? 1 : 0) + (has2 ? 1 : 0) + (has3 ? 1 : 0);

        // If l == 2 and the 2 active corners lie on the clipped outer border,
        // those FCT_SIDE facets belong to the excluded outer-ring cell.
        if (l == 2) {
          if ((has0 && has1 && r == rMin) || (has2 && has3 && r == rMax) ||
              (has3 && has0 && c == cMin) || (has1 && has2 && c == cMax)) {
            continue;
          }
        }
      }

      const auto *lst = facet_list(r, c);
      if (lst) {
        for (const auto &fct : *lst) {
          if (bodyId == -1) {
            outList.push_back(fct);
          } else {
            if (fct.primary && fct.primary->id() == bodyId) {
              Facet3Pt copy = fct;
              copy.opposite = nullptr;
              outList.push_back(copy);
            } else if (fct.opposite && fct.opposite->id() == bodyId) {
              // Contact interface: invert winding and normal so it faces
              // outward from this body
              Facet3Pt copy = fct;
              copy.reverse();
              copy.opposite = nullptr;
              outList.push_back(copy);
            }
          }
        }
      }

      // If excluding the outer extension ring and this interior cell contains
      // volume of `bodyId` (l >= 3), emit the boundary wall only on edges where
      // the body terminates at the interior border (i.e. does not extend into
      // the adjacent outer-ring cell, where its FCT_SIDE was stored).
      if (clipOuter && bodyId != -1 && l >= 3) {
        if (r == rMin && has0 && has1 && body_index(bodyId, r - 1, c) < 1 &&
            body_index(bodyId, r - 1, c + 1) < 1) {
          emitBoundaryQuad(bodyId, t0, b0, t1, b1);
        }
        if (r == rMax && has2 && has3 && body_index(bodyId, r + 2, c) < 1 &&
            body_index(bodyId, r + 2, c + 1) < 1) {
          emitBoundaryQuad(bodyId, t2, b2, t3, b3);
        }
        if (c == cMin && has3 && has0 && body_index(bodyId, r, c - 1) < 1 &&
            body_index(bodyId, r + 1, c - 1) < 1) {
          emitBoundaryQuad(bodyId, t3, b3, t0, b0);
        }
        if (c == cMax && has1 && has2 && body_index(bodyId, r, c + 2) < 1 &&
            body_index(bodyId, r + 1, c + 2) < 1) {
          emitBoundaryQuad(bodyId, t1, b1, t2, b2);
        }
      }
    }
  }
  return static_cast<int>(outList.size());
}

int Model::facets_for_3d_view(std::vector<Facet3Pt> &outList,
                              bool includeOuter) const {
  outList.clear();
  return body_facets(-1, outList, includeOuter);
}

std::vector<BodyMesh3D> Model::generate_body_meshes(int bodyId,
                                                    bool includeOuter) const {
  std::vector<BodyMesh3D> meshes;
  for (const auto &bPtr : bodies_) {
    if (!bPtr)
      continue;
    int bId = bPtr->id();
    if (bodyId != -1 && bId != bodyId)
      continue;

    BodyMesh3D mesh;
    mesh.bodyId = bId;
    mesh.bodyName = bPtr->name();
    mesh.color = bPtr->color();
    mesh.alpha = bPtr->is_transparent() ? bPtr->transparency() : 1.0f;
    mesh.isVisible = bPtr->is_visible();
    mesh.isTransparent = bPtr->is_transparent();

    // Get watertight facets for this body
    body_facets(bId, mesh.facets, includeOuter);
    if (mesh.facets.empty())
      continue;

    // Extract feature crease & boundary edges using cache-friendly flat sort
    struct EdgeRecord {
      long long x0, y0, z0;
      long long x1, y1, z1;
      Point3D pt0, pt1;
      Point3D normal;

      bool same_key(const EdgeRecord &o) const noexcept {
        return x0 == o.x0 && y0 == o.y0 && z0 == o.z0 && x1 == o.x1 &&
               y1 == o.y1 && z1 == o.z1;
      }

      bool operator<(const EdgeRecord &o) const noexcept {
        return std::tie(x0, y0, z0, x1, y1, z1) <
               std::tie(o.x0, o.y0, o.z0, o.x1, o.y1, o.z1);
      }
    };

    std::vector<EdgeRecord> edgeRecords;
    edgeRecords.reserve(mesh.facets.size() * 3);

    auto quant = [](double v) noexcept -> long long {
      return static_cast<long long>(std::llround(v * 1000.0));
    };

    for (const auto &f : mesh.facets) {
      const Point3D n = f.normal();
      for (int k = 0; k < 3; ++k) {
        const Point3D &pA = f[k];
        const Point3D &pB = f[(k + 1) % 3];

        long long qAx = quant(pA.x), qAy = quant(pA.y), qAz = quant(pA.z);
        long long qBx = quant(pB.x), qBy = quant(pB.y), qBz = quant(pB.z);

        if (std::tie(qAx, qAy, qAz) <= std::tie(qBx, qBy, qBz)) {
          edgeRecords.push_back({qAx, qAy, qAz, qBx, qBy, qBz, pA, pB, n});
        } else {
          edgeRecords.push_back({qBx, qBy, qBz, qAx, qAy, qAz, pB, pA, n});
        }
      }
    }

    std::sort(edgeRecords.begin(), edgeRecords.end());
    mesh.featureEdges.reserve(edgeRecords.size() / 4);

    for (size_t i = 0; i < edgeRecords.size();) {
      size_t j = i + 1;
      while (j < edgeRecords.size() &&
             edgeRecords[i].same_key(edgeRecords[j])) {
        ++j;
      }
      const size_t count = j - i;
      if (count == 1) {
        mesh.featureEdges.push_back({edgeRecords[i].pt0, edgeRecords[i].pt1});
      } else if (count >= 2) {
        double dot = edgeRecords[i].normal.dot(edgeRecords[i + 1].normal);
        if (dot < 0.94) {
          mesh.featureEdges.push_back({edgeRecords[i].pt0, edgeRecords[i].pt1});
        }
      }
      i = j;
    }

    meshes.push_back(std::move(mesh));
  }
  return meshes;
}

// ---------------------------------------------------------------------------
// Graph-Theoretic Topological Validation, Stratigraphic DAG & Sanitizer
// ---------------------------------------------------------------------------

std::vector<int> Model::stratigraphic_topological_order() const {
  std::unordered_map<int, std::unordered_set<int>> adj;
  std::unordered_map<int, int> inDegree;

  for (const auto &bPtr : bodies_) {
    if (bPtr) {
      inDegree.try_emplace(bPtr->id(), 0);
    }
  }

  for (int r = 0; r < rows_; ++r) {
    for (int c = 0; c < cols_; ++c) {
      const int cnt = static_cast<int>(count(r, c));
      std::vector<int> colBodies;
      for (int i = 1; i < cnt - 1; i += 2) {
        int bId = id(i, r, c);
        if (bId >= 0) {
          colBodies.push_back(bId);
          inDegree.try_emplace(bId, 0);
        }
      }
      for (size_t u = 0; u < colBodies.size(); ++u) {
        for (size_t v = u + 1; v < colBodies.size(); ++v) {
          if (colBodies[u] != colBodies[v]) {
            if (adj[colBodies[u]].insert(colBodies[v]).second) {
              inDegree[colBodies[v]]++;
            }
          }
        }
      }
    }
  }

  std::priority_queue<int, std::vector<int>, std::greater<int>> q;
  for (const auto &[bId, deg] : inDegree) {
    if (deg == 0) {
      q.push(bId);
    }
  }

  std::vector<int> order;
  order.reserve(inDegree.size());
  while (!q.empty()) {
    int u = q.top();
    q.pop();
    order.push_back(u);
    if (auto it = adj.find(u); it != adj.end()) {
      for (int v : it->second) {
        if (--inDegree[v] == 0) {
          q.push(v);
        }
      }
    }
  }

  if (order.size() != inDegree.size()) {
    return {};
  }
  return order;
}

bool Model::is_stratigraphic_dag_valid() const {
  if (bodies_.empty())
    return true;
  std::unordered_set<int> activeBodies;
  for (const auto &b : bodies_) {
    if (b)
      activeBodies.insert(b->id());
  }
  for (int r = 0; r < rows_; ++r) {
    for (int c = 0; c < cols_; ++c) {
      const int cnt = static_cast<int>(count(r, c));
      std::unordered_set<int> seenInCol;
      for (int i = 1; i < cnt - 1; i += 2) {
        int bId = id(i, r, c);
        if (bId >= 0) {
          if (!seenInCol.insert(bId).second)
            return false; // Duplicate body in same column
          activeBodies.insert(bId);
        }
      }
    }
  }
  auto order = stratigraphic_topological_order();
  return order.size() == activeBodies.size();
}

BodyTopologyStatus Model::analyze_body_topology(int bodyId,
                                                bool includeOuter) const {
  BodyTopologyStatus status;
  status.bodyId = bodyId;

  std::vector<Facet3Pt> facets;
  body_facets(bodyId, facets, includeOuter);
  status.facetCount = facets.size();
  if (facets.empty()) {
    status.isWatertight = true;
    status.isOutwardOriented = true;
    return status;
  }

  struct QuantPt {
    long long x, y, z;
    bool operator==(const QuantPt &o) const noexcept {
      return x == o.x && y == o.y && z == o.z;
    }
  };
  struct QuantPtHash {
    size_t operator()(const QuantPt &p) const noexcept {
      size_t h1 = std::hash<long long>{}(p.x);
      size_t h2 = std::hash<long long>{}(p.y);
      size_t h3 = std::hash<long long>{}(p.z);
      return h1 ^ (h2 * 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2)) ^
             (h3 * 0x517cc1b727220a95ULL + (h2 << 6) + (h2 >> 2));
    }
  };

  auto quantPt = [](const Point3D &p) noexcept -> QuantPt {
    return {static_cast<long long>(std::llround(p.x * 1000.0)),
            static_cast<long long>(std::llround(p.y * 1000.0)),
            static_cast<long long>(std::llround(p.z * 1000.0))};
  };

  std::unordered_map<QuantPt, int, QuantPtHash> vertexToId;
  int nextVid = 0;

  struct DirectedEdge {
    int u, v;
    size_t facetIdx;
  };

  struct UndirectedEdgeKey {
    int a, b;
    bool operator==(const UndirectedEdgeKey &o) const noexcept {
      return a == o.a && b == o.b;
    }
  };
  struct UndirectedEdgeHash {
    size_t operator()(const UndirectedEdgeKey &e) const noexcept {
      return (static_cast<size_t>(e.a) << 32) ^ static_cast<size_t>(e.b);
    }
  };

  struct EdgeIncidence {
    int forwardCount{0};  // u < v
    int backwardCount{0}; // u > v
    std::vector<size_t> incidentFacets;
  };

  std::unordered_map<UndirectedEdgeKey, EdgeIncidence, UndirectedEdgeHash>
      edgeGraph;

  for (size_t fIdx = 0; fIdx < facets.size(); ++fIdx) {
    const auto &f = facets[fIdx];
    if (!f.pfld_facet().is_initialized() || f.area() <= 1e-12 ||
        f.normal().is_zero()) {
      status.degenerateFacetCount++;
      continue;
    }

    int vids[3];
    for (int k = 0; k < 3; ++k) {
      QuantPt qp = quantPt(f[k]);
      auto [it, inserted] = vertexToId.try_emplace(qp, nextVid);
      if (inserted)
        nextVid++;
      vids[k] = it->second;
    }

    if (vids[0] == vids[1] || vids[1] == vids[2] || vids[2] == vids[0]) {
      status.degenerateFacetCount++;
      continue;
    }

    for (int k = 0; k < 3; ++k) {
      int u = vids[k];
      int v = vids[(k + 1) % 3];
      status.halfEdgeCount++;
      UndirectedEdgeKey key{std::min(u, v), std::max(u, v)};
      auto &inc = edgeGraph[key];
      if (u < v)
        inc.forwardCount++;
      else
        inc.backwardCount++;
      inc.incidentFacets.push_back(fIdx);
    }
  }

  status.vertexCount = vertexToId.size();

  // Build Dual Facet Graph adjacency list to find connected polyhedral shells
  std::vector<std::vector<size_t>> dualAdj(facets.size());
  for (const auto &[key, inc] : edgeGraph) {
    if (inc.forwardCount != inc.backwardCount) {
      // Unbalanced half-edges on this edge
      status.boundaryEdgeCount +=
          static_cast<size_t>(std::abs(inc.forwardCount - inc.backwardCount));
    }
    if ((inc.forwardCount > 1 && inc.backwardCount == 0) ||
        (inc.backwardCount > 1 && inc.forwardCount == 0)) {
      // Conflicting winding (Möbius strip defect)
      status.nonManifoldEdgeCount++;
    }
    for (size_t i = 0; i < inc.incidentFacets.size(); ++i) {
      for (size_t j = i + 1; j < inc.incidentFacets.size(); ++j) {
        size_t f1 = inc.incidentFacets[i];
        size_t f2 = inc.incidentFacets[j];
        dualAdj[f1].push_back(f2);
        dualAdj[f2].push_back(f1);
      }
    }
  }

  // BFS on Dual Facet Graph: compute signed volume per connected component via
  // Divergence Theorem: V = (1/6) * sum(p0 . (p1 x p2))
  // Because Facet3Pt stores outward normal n = normalize((p0 - p1) x (p1 -
  // p2)), the outward-oriented triple product is p0 . ((p0 - p1) x (p1 - p2)) =
  // (p0 x p1) . p2. Wait: let's check the exact sign of (p0 - p1) x (p1 - p2):
  // (p0 - p1) x (p1 - p2) = p0 x p1 - p0 x p2 - p1 x p1 + p1 x p2
  // = (p1 - p0) x (p2 - p0) if and only if:
  // (p1 - p0) x (p2 - p0) = p1 x p2 - p1 x p0 - p0 x p2 = p0 x p1 + p1 x p2 +
  // p2 x p0! Yes! (p0 - p1) x (p1 - p2) == (p1 - p0) x (p2 - p0) identically!
  // Therefore, for any closed surface with outward normal _n = normalize((p0 -
  // p1) x (p1 - p2)), the signed volume is V = (1/6) * sum( p0 . ((p1 - p0) x
  // (p2 - p0)) ) = (1/6) * sum( p0 . (p1 x p2) ) > 0!
  std::vector<bool> visited(facets.size(), false);
  bool allShellsPositiveVolume = true;
  double totalVolume = 0.0;

  for (size_t i = 0; i < facets.size(); ++i) {
    if (visited[i])
      continue;
    if (!facets[i].pfld_facet().is_initialized() || facets[i].area() <= 1e-12)
      continue;

    status.connectedComponents++;
    double shellVolume6 = 0.0;
    std::queue<size_t> q;
    q.push(i);
    visited[i] = true;

    while (!q.empty()) {
      size_t curr = q.front();
      q.pop();
      const auto &f = facets[curr];
      shellVolume6 += f[0].dot(f[1].cross(f[2]));

      for (size_t nb : dualAdj[curr]) {
        if (!visited[nb]) {
          visited[nb] = true;
          q.push(nb);
        }
      }
    }

    const double shellVol = shellVolume6 / 6.0;
    totalVolume += shellVol;
    if (shellVol <= 1e-9) {
      allShellsPositiveVolume = false;
    }
  }

  status.signedVolume = totalVolume;
  status.isWatertight =
      (status.boundaryEdgeCount == 0 && status.nonManifoldEdgeCount == 0 &&
       status.degenerateFacetCount == 0);
  status.isOutwardOriented =
      status.isWatertight && allShellsPositiveVolume && (totalVolume > 1e-9);
  return status;
}

ModelTopologyReport Model::analyze_topology(bool includeOuter) const {
  ModelTopologyReport report;
  report.isStratigraphicDagValid = is_stratigraphic_dag_valid();

  std::vector<Facet3Pt> compFacets;
  facets_computation(compFacets);
  report.totalComputationFacets = compFacets.size();
  for (const auto &fct : compFacets) {
    if (fct.primary != nullptr && fct.opposite != nullptr) {
      report.sharedInterfaceFacets++;
    }
  }

  for (const auto &bPtr : bodies_) {
    if (!bPtr)
      continue;
    report.bodyStatuses.push_back(
        analyze_body_topology(bPtr->id(), includeOuter));
  }
  return report;
}

size_t Model::sanitize() {
  if (!initialized_ || rows_ <= 0 || cols_ <= 0) {
    return 0;
  }

  size_t repairs = 0;
  const double zMinVal = z_min_.value();
  const double zMaxVal = z_max_.value();

  for (int r = 0; r < rows_; ++r) {
    for (int c = 0; c < cols_; ++c) {
      auto &colVec = data_[r * cols_ + c];

      // 1. Ensure column has at least [top, bottom] boundary points
      if (colVec.size() < 2) {
        colVec.clear();
        ColumnPoint pTop(z_max_);
        set_column_point_coords(pTop, r, c, z_max_);
        colVec.push_back(pTop);

        ColumnPoint pBot(z_min_);
        set_column_point_coords(pBot, r, c, z_min_);
        colVec.push_back(pBot);
        repairs++;
        continue;
      }

      // 2. Clamp surface and bottom points to [zMin, zMax]
      double topZ = colVec.front().z().value();
      if (topZ > zMaxVal || topZ < zMinVal) {
        topZ = std::clamp(topZ, zMinVal, zMaxVal);
        colVec.front().set_z(topZ);
        repairs++;
      }
      set_column_point_coords(colVec.front(), r, c, topZ);

      if (std::abs(colVec.back().z().value() - zMinVal) > 1e-9) {
        colVec.back().set_z(zMinVal);
        repairs++;
      }
      set_column_point_coords(colVec.back(), r, c, zMinVal);

      // 3. Validate interior body vertex pairs [1 .. size-2]
      std::vector<ColumnPoint> validPairs;
      validPairs.reserve(colVec.size());
      std::unordered_set<int> seenBodies;
      double prevZ = topZ;

      for (size_t k = 1; k + 1 < colVec.size();) {
        if (k + 2 >= colVec.size()) {
          // Dangling odd point at end of column before bottom boundary
          repairs++;
          break;
        }
        ColumnPoint ptT = colVec[k];
        ColumnPoint ptB = colVec[k + 1];
        const int bIdT = ptT.body_id();
        const int bIdB = ptB.body_id();
        Body *bPtr = (bIdT >= 0) ? body(bIdT) : nullptr;

        if (bIdT < 0 || bIdT != bIdB || bPtr == nullptr ||
            seenBodies.count(bIdT) > 0) {
          // Invalid body ID, mismatched pair, or duplicate body in same column
          repairs++;
          k += (bIdT == bIdB) ? 2 : 1;
          continue;
        }

        seenBodies.insert(bIdT);
        double zT = std::clamp(ptT.z().value(), zMinVal, prevZ);
        double zB = std::clamp(ptB.z().value(), zMinVal, zT);
        if (std::abs(zT - ptT.z().value()) > 1e-9 ||
            std::abs(zB - ptB.z().value()) > 1e-9) {
          repairs++;
        }

        ptT.set_body(bPtr);
        ptB.set_body(bPtr);
        ptT.set_z(zT);
        ptB.set_z(zB);
        set_column_point_coords(ptT, r, c, zT);
        set_column_point_coords(ptB, r, c, zB);
        validPairs.push_back(ptT);
        validPairs.push_back(ptB);
        prevZ = zB;
        k += 2;
      }

      if (validPairs.size() + 2 != colVec.size()) {
        ColumnPoint frontPt = colVec.front();
        ColumnPoint backPt = colVec.back();
        colVec.clear();
        colVec.push_back(frontPt);
        colVec.insert(colVec.end(), validPairs.begin(), validPairs.end());
        colVec.push_back(backPt);
      } else {
        for (size_t j = 0; j < validPairs.size(); ++j) {
          colVec[j + 1] = validPairs[j];
        }
      }
    }
  }

  init_facet_list();
  return repairs;
}

const std::vector<BodyBoundarySegment> *Model::body_bound_at(int row,
                                                             int col) const {
  if (row < 0 || col < 0 || row > rows_ - 2 || col > cols_ - 2)
    return nullptr;
  int idx = row * cols_ + col;
  if (idx >= 0 && idx < static_cast<int>(body_bounds_.size())) {
    return &body_bounds_[idx];
  }
  return nullptr;
}

std::vector<BodyBoundarySegment> Model::all_body_boundaries() const {
  std::vector<BodyBoundarySegment> all;
  for (const auto &colBounds : body_bounds_) {
    all.insert(all.end(), colBounds.begin(), colBounds.end());
  }
  return all;
}

} // namespace mod3d
