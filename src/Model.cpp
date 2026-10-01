#include "mod3d/Model.h"
#include <algorithm>
#include <cmath>
#include <cassert>

namespace mod3d {

Model::Model() = default;

int Model::getFlatIndex(int row, int col) const
{
    if (row < 0 || col < 0 || row >= m_nRows || col >= m_nCols) {
        return -1;
    }
    return row * m_nCols + col;
}

double Model::getXe(int col) const
{
    if (col == 0) return m_x0 - m_dExW;
    if (col == m_nCols - 1) return m_x0 + (col - 2) * m_xSize + m_dExE;
    return m_x0 + (col - 1) * m_xSize;
}

double Model::getYe(int row) const
{
    if (row == 0) return m_y0 - m_dExS;
    if (row == m_nRows - 1) return m_y0 + (row - 2) * m_ySize + m_dExN;
    return m_y0 + (row - 1) * m_ySize;
}

double Model::getX2e(int col) const
{
    if (col == 0) return m_x0 - m_dExW / 2.0;
    if (col == m_nCols - 2) return m_x0 + (col - 1) * m_xSize + m_dExE / 2.0;
    return m_x0 + (col - 1) * m_xSize + m_xSize / 2.0;
}

double Model::getY2e(int row) const
{
    if (row == 0) return m_y0 - m_dExS / 2.0;
    if (row == m_nRows - 2) return m_y0 + (row - 1) * m_ySize + m_dExN / 2.0;
    return m_y0 + (row - 1) * m_ySize + m_ySize / 2.0;
}

void Model::setColumnPointCoords(ColumnPoint &pt, int row, int col, double z)
{
    pt.point().x = getXe(col);
    pt.point().y = getYe(row);
    pt.setZ(z);
}

bool Model::init(int nRows, int nCols, double x0, double y0, double xSize, double ySize, double zMin, double zMax)
{
    m_nRows = nRows + 2;
    m_nCols = nCols + 2;
    m_x0 = x0;
    m_y0 = y0;
    m_xSize = xSize;
    m_ySize = ySize;
    m_xMin = m_x0;
    m_xMax = m_x0 + (m_nCols - 3) * m_xSize;
    m_yMin = m_y0;
    m_yMax = m_y0 + (m_nRows - 3) * m_ySize;
    m_zMin = zMin;
    m_zMax = zMax;

    const size_t totalCells = static_cast<size_t>(m_nRows * m_nCols);
    m_data.resize(totalCells);
    m_facets.resize(totalCells);

    for (int r = 0; r < m_nRows; ++r) {
        for (int c = 0; c < m_nCols; ++c) {
            auto &colVec = m_data[r * m_nCols + c];
            colVec.clear();
            colVec.reserve(16);

            ColumnPoint pTop(m_zMax);
            setColumnPointCoords(pTop, r, c, m_zMax);
            colVec.push_back(pTop);

            ColumnPoint pBot(m_zMin);
            setColumnPointCoords(pBot, r, c, m_zMin);
            colVec.push_back(pBot);
        }
    }

    m_initialized = true;
    return true;
}

bool Model::init(const Grid &reliefGrid, double zMin, double zMax)
{
    const int nRows = static_cast<int>(reliefGrid.rows());
    const int nCols = static_cast<int>(reliefGrid.cols());
    m_nRows = nRows + 2;
    m_nCols = nCols + 2;
    m_x0 = reliefGrid.x0();
    m_y0 = reliefGrid.y0();
    m_xSize = reliefGrid.xSize();
    m_ySize = reliefGrid.ySize();
    m_xMin = m_x0;
    m_xMax = m_x0 + (m_nCols - 3) * m_xSize;
    m_yMin = m_y0;
    m_yMax = m_y0 + (m_nRows - 3) * m_ySize;
    m_zMin = zMin;
    m_zMax = zMax;

    const size_t totalCells = static_cast<size_t>(m_nRows * m_nCols);
    m_data.resize(totalCells);
    m_facets.resize(totalCells);

    for (int r = 0; r < m_nRows; ++r) {
        for (int c = 0; c < m_nCols; ++c) {
            int ii = std::clamp(r - 1, 0, nRows - 1);
            int jj = std::clamp(c - 1, 0, nCols - 1);
            double v = reliefGrid(ii, jj);

            auto &colVec = m_data[r * m_nCols + c];
            colVec.clear();
            colVec.reserve(16);

            ColumnPoint pTop(v);
            setColumnPointCoords(pTop, r, c, v);
            colVec.push_back(pTop);

            ColumnPoint pBot(m_zMin);
            setColumnPointCoords(pBot, r, c, m_zMin);
            colVec.push_back(pBot);
        }
    }

    m_initialized = true;
    return true;
}

size_t Model::getCount(int row, int col) const
{
    int idx = getFlatIndex(row, col);
    if (idx < 0) return 0;
    return m_data[idx].size();
}

int Model::getUpperBound(int row, int col) const
{
    int count = static_cast<int>(getCount(row, col));
    return count - 1;
}

const ColumnPoint *Model::getAt(int row, int col, int index) const
{
    int idx = getFlatIndex(row, col);
    if (idx < 0 || index < 0 || static_cast<size_t>(index) >= m_data[idx].size()) {
        return nullptr;
    }
    return &m_data[idx][index];
}

ColumnPoint *Model::getAt(int row, int col, int index)
{
    int idx = getFlatIndex(row, col);
    if (idx < 0 || index < 0 || static_cast<size_t>(index) >= m_data[idx].size()) {
        return nullptr;
    }
    return &m_data[idx][index];
}

void Model::add(int row, int col, const ColumnPoint &pt)
{
    int idx = getFlatIndex(row, col);
    if (idx < 0) return;
    ColumnPoint pCopy = pt;
    setColumnPointCoords(pCopy, row, col, pCopy.z());
    m_data[idx].push_back(pCopy);
}

void Model::insertAt(int row, int col, int index, const ColumnPoint &pt)
{
    int idx = getFlatIndex(row, col);
    if (idx < 0) return;
    ColumnPoint pCopy = pt;
    setColumnPointCoords(pCopy, row, col, pCopy.z());
    if (index >= 0 && static_cast<size_t>(index) <= m_data[idx].size()) {
        m_data[idx].insert(m_data[idx].begin() + index, pCopy);
    }
}

void Model::removeAt(int row, int col, int index, int count)
{
    int idx = getFlatIndex(row, col);
    if (idx < 0) return;
    if (index >= 0 && static_cast<size_t>(index + count) <= m_data[idx].size()) {
        m_data[idx].erase(m_data[idx].begin() + index, m_data[idx].begin() + index + count);
    }
}

double Model::getZ(int row, int col, int index) const
{
    if (index < 0) return getHeaven();
    if (index > getUpperBound(row, col)) return getHell();
    const ColumnPoint *pt = getAt(row, col, index);
    return pt ? pt->z() : 0.0;
}

void Model::setZ(int row, int col, int index, double z)
{
    ColumnPoint *pt = getAt(row, col, index);
    if (pt) {
        pt->setZ(z);
    }
}

double Model::getThickness(int row, int col, int index) const
{
    if (index <= 0 || (index % 2) == 0) return 0.0;
    const ColumnPoint *top = getAt(row, col, index);
    const ColumnPoint *bot = getAt(row, col, index + 1);
    if (!top || !bot) return 0.0;
    return std::abs(top->z() - bot->z());
}

Body *Model::newBody()
{
    auto b = std::make_unique<Body>();
    b->SetID(m_nextBodyId++);
    Body *raw = b.get();
    m_bodies.push_back(std::move(b));
    updateBodyIndex();
    return raw;
}

Body *Model::getBody(int bodyId)
{
    for (auto &b : m_bodies) {
        if (b->GetID() == bodyId) return b.get();
    }
    return nullptr;
}

const Body *Model::getBody(int bodyId) const
{
    for (const auto &b : m_bodies) {
        if (b->GetID() == bodyId) return b.get();
    }
    return nullptr;
}

void Model::updateBodyIndex()
{
    for (size_t i = 0; i < m_bodies.size(); ++i) {
        m_bodies[i]->SetIndex(static_cast<int>(i));
    }
}

int Model::getBodyIndex(int bodyId, int row, int col) const
{
    const size_t cnt = getCount(row, col);
    for (size_t i = 1; i < cnt; i += 2) {
        const ColumnPoint *pt = getAt(row, col, static_cast<int>(i));
        if (pt && pt->bodyId() == bodyId) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int Model::getID(int index, int row, int col) const
{
    const int count = static_cast<int>(getCount(row, col));
    if (index > 0 && index < count - 1) {
        const ColumnPoint *pt = getAt(row, col, index);
        if (pt) return pt->bodyId();
    }
    return -1;
}

bool Model::isInsPosOK(int index, int bodyId, int row, int col) const
{
    const int cnt = static_cast<int>(getCount(row, col));
    for (int i = 0; i < cnt - 1; i += 2) {
        if (bodyId == getID(i, row, col)) {
            return false;
        }
    }

    for (int i = 1; i < cnt - 1; i += 2) {
        int tag = (i < index) ? 1 : 0;
        int idCheck = getID(i, row, col);
        if (!isInsPosOK_1(tag, bodyId, idCheck, row - 1, col)) return false;
        if (!isInsPosOK_1(tag, bodyId, idCheck, row + 1, col)) return false;
        if (!isInsPosOK_1(tag, bodyId, idCheck, row, col - 1)) return false;
        if (!isInsPosOK_1(tag, bodyId, idCheck, row, col + 1)) return false;
        if (!isInsPosOK_1(tag, bodyId, idCheck, row + 1, col + 1)) return false;
        if (!isInsPosOK_1(tag, bodyId, idCheck, row - 1, col - 1)) return false;
        if (!isInsPosOK_1(tag, bodyId, idCheck, row + 1, col - 1)) return false;
        if (!isInsPosOK_1(tag, bodyId, idCheck, row - 1, col + 1)) return false;
    }
    return true;
}

bool Model::isInsPosOK_1(int tag, int bodyId, int idCheck, int row, int col) const
{
    if (row < 0 || col < 0 || row >= m_nRows || col >= m_nCols) return true;
    const int cnt = static_cast<int>(getCount(row, col));
    for (int i = 0; i < cnt - 1; i += 2) {
        if (bodyId == getID(i, row, col)) {
            return isInsPosOK_2(tag, bodyId, idCheck, row, col);
        }
    }
    return true;
}

bool Model::isInsPosOK_2(int tag, int bodyId, int idCheck, int row, int col) const
{
    int tagHere = 0;
    const int cnt = static_cast<int>(getCount(row, col));
    for (int i = 0; i < cnt - 1; i += 2) {
        if (bodyId == getID(i, row, col)) {
            tagHere = 1;
        }
        if (tag == tagHere && idCheck == getID(i, row, col)) {
            return false;
        }
    }
    return true;
}

int Model::insertBody(int row, int col, double z, double thickness, bool isNew, int bodyId, double zT, double zB, bool bZ)
{
    Body *pBody = nullptr;
    if (isNew) {
        pBody = newBody();
    } else {
        pBody = getBody(bodyId);
    }
    if (!pBody) return -1;
    bodyId = pBody->GetID();

    double t = (thickness == -1.0) ? getDefaultThickness() : thickness;
    if (bZ) {
        zT = z + t;
        zB = z - t;
    } else {
        z = (zT + zB) / 2.0;
    }
    if (m_constTop) zT = m_dConstTop;
    if (m_constBot) zB = m_dConstBot;

    double zD = m_zMin;
    double zU = getZ(row, col, 0);
    if (!(zD < z && z < zU)) return -1;

    int nIndex = 1;
    const int iMax = static_cast<int>(getCount(row, col));
    while (nIndex < iMax) {
        zU = getZ(row, col, nIndex - 1);
        zD = getZ(row, col, nIndex);
        if (zD < z && z < zU) break;
        nIndex += 2;
    }
    if (nIndex > iMax) return -1;

    if (!isInsPosOK(nIndex, bodyId, row, col)) return -1;

    switch (m_creationTag) {
        case BodyCreateTag::None: break;
        case BodyCreateTag::JoinTop: zT = zU; break;
        case BodyCreateTag::JoinBot: zB = zD; break;
        case BodyCreateTag::JoinTopBot: zT = zU; zB = zD; break;
    }

    zT = std::min(zU, zT);
    zB = std::max(zD, zB);

    ColumnPoint ptBot(bodyId, zB, pBody);
    ColumnPoint ptTop(bodyId, zT, pBody);
    insertAt(row, col, nIndex, ptBot);
    insertAt(row, col, nIndex, ptTop);

    m_currentRow = row;
    m_currentCol = col;
    updateFacetList(row, col);

    return nIndex;
}

int Model::removeBody(int index, int row, int col)
{
    m_currentRow = row;
    m_currentCol = col;
    removeAt(row, col, index, 2);
    updateFacetList(row, col);
    return 1;
}

int Model::deleteBody(int bodyId)
{
    for (int r = 0; r < m_nRows; ++r) {
        for (int c = 0; c < m_nCols; ++c) {
            int idx = getBodyIndex(bodyId, r, c);
            if (idx != -1) {
                removeBody(idx, r, c);
            }
        }
    }

    auto it = std::remove_if(m_bodies.begin(), m_bodies.end(), [bodyId](const auto &b) {
        return b->GetID() == bodyId;
    });
    if (it != m_bodies.end()) {
        m_bodies.erase(it, m_bodies.end());
        updateBodyIndex();
        bool oldRealTime = m_computeRealTime;
        m_computeRealTime = false;
        initFacetList();
        m_computeRealTime = oldRealTime;
        return 1;
    }
    return 0;
}

int Model::moveVertex(int &index, int row, int col, double z, BodyMoveType moveType)
{
    if (index <= 0) return -1;

    const int indexMax = static_cast<int>(getCount(row, col)) - 1;
    const double zR = getZ(row, col, 0);
    const double zC = getZ(row, col, index);

    int i = index;
    int j = index + 1;
    double zU = getZ(row, col, index - 1);
    double zD = getZ(row, col, index + 1);

    while (j < indexMax + 1) {
        zD = getZ(row, col, j);
        if (zC > zD) break;
        if (j == indexMax) {
            zD = getZ(row, col, j);
            break;
        }
        j++;
    }

    m_currentRow = row;
    m_currentCol = col;

    if (moveType == BodyMoveType::Normal) {
        if (z >= zU) {
            z = zU;
            index--;
        }
        if (z <= zD) {
            z = zD;
        }
        while (i < j) {
            setZ(row, col, i, z);
            i++;
        }
        if (z == zR) {
            index = 1;
            return 0;
        }
        if (z == m_zMin) {
            return 0;
        }
    } else if (moveType == BodyMoveType::Split) {
        if (z >= zC && z < zU) {
            setZ(row, col, index, z);
        }
        if (z <= zC && z > zD) {
            index = j - 1;
            setZ(row, col, index, z);
        }
    } else if (moveType == BodyMoveType::Constrained) {
        if (z > zD && z < zU) {
            setZ(row, col, index, z);
        } else {
            return 2;
        }
    }

    updateFacetList(row, col);
    return 0;
}

// ---------------------------------------------------------------------------
// Facet Generation Engine
// ---------------------------------------------------------------------------

std::vector<Facet3Pt> *Model::getFacetList(int row, int col)
{
    if (row < 0 || col < 0 || row > m_nRows - 2 || col > m_nCols - 2) return nullptr;
    int idx = row * m_nCols + col;
    if (idx < static_cast<int>(m_facets.size())) return &m_facets[idx];
    return nullptr;
}

const std::vector<Facet3Pt> *Model::getFacetList(int row, int col) const
{
    if (row < 0 || col < 0 || row > m_nRows - 2 || col > m_nCols - 2) return nullptr;
    int idx = row * m_nCols + col;
    if (idx < static_cast<int>(m_facets.size())) return &m_facets[idx];
    return nullptr;
}

int Model::addFctGen(std::vector<FctGen> &fga, int row, int col, int tag)
{
    const int count = static_cast<int>(getCount(row, col));
    for (int i = 1; i < count - 1; i += 2) {
        int id = getID(i, row, col);
        bool added = false;
        for (auto &fg : fga) {
            if (fg.id == id) {
                fg.idPrev[tag] = getID(i - 1, row, col);
                fg.idNext[tag] = getID(i + 1, row, col);
                fg.top[tag] = getAt(row, col, i)->point();
                fg.bot[tag] = getAt(row, col, i + 1)->point();
                fg.b[tag] = true;
                fg.l++;
                added = true;
                break;
            }
        }
        if (!added) {
            FctGen fg;
            fg.id = id;
            fg.idPrev[tag] = getID(i - 1, row, col);
            fg.idNext[tag] = getID(i + 1, row, col);
            fg.top[tag] = getAt(row, col, i)->point();
            fg.bot[tag] = getAt(row, col, i + 1)->point();
            fg.b[tag] = true;
            fg.l = 1;
            fga.push_back(fg);
        }
    }
    return static_cast<int>(fga.size());
}

void Model::initSideFacetsBound(const FctGen &fg, std::vector<Facet3Pt> &facetList, int i0, int i1)
{
    if (fg.top[i0] == fg.bot[i0] && fg.top[i1] == fg.bot[i1]) return;

    Body *pBd = getBody(fg.id);
    if (fg.top[i0] != fg.bot[i0]) {
        Facet3Pt fct(fg.top[i0], fg.bot[i0], fg.top[i1]);
        fct.pBody = pBd;
        facetList.push_back(fct);
    }
    if (fg.top[i1] != fg.bot[i1]) {
        Facet3Pt fct(fg.bot[i0], fg.bot[i1], fg.top[i1]);
        fct.pBody = pBd;
        facetList.push_back(fct);
    }
}

void Model::fgToFctBound(FctGen &fg, std::vector<Facet3Pt> &facetList)
{
    if (fg.l == 2) return;

    if (fg.b[0] && fg.b[1] && fg.top[0].y == m_yMin - m_dExS) initSideFacetsBound(fg, facetList, 0, 1);
    if (fg.b[2] && fg.b[3] && fg.top[2].y == m_yMax + m_dExN) initSideFacetsBound(fg, facetList, 2, 3);
    if (fg.b[3] && fg.b[0] && fg.top[3].x == m_xMin - m_dExW) initSideFacetsBound(fg, facetList, 3, 0);
    if (fg.b[1] && fg.b[2] && fg.top[1].x == m_xMax + m_dExE) initSideFacetsBound(fg, facetList, 1, 2);
}

void Model::initSideFacets_2(const FctGen &fg, std::vector<Facet3Pt> &facetList, int i0, int i1)
{
    if (fg.b[0] && fg.b[1] && fg.top[0].y == m_yMin - m_dExS) return;
    if (fg.b[2] && fg.b[3] && fg.top[2].y == m_yMax + m_dExN) return;
    if (fg.b[3] && fg.b[0] && fg.top[3].x == m_xMin - m_dExW) return;
    if (fg.b[1] && fg.b[2] && fg.top[1].x == m_xMax + m_dExE) return;

    if (fg.b[i0] && fg.b[i1] && ((fg.top[i0] != fg.bot[i0]) || (fg.top[i1] != fg.bot[i1]))) {
        Body *pBd = getBody(fg.id);
        if (fg.top[i0] != fg.bot[i0]) {
            Facet3Pt fct(fg.top[i0], fg.bot[i0], fg.top[i1]);
            fct.nType = FacetType::FCT_SIDE;
            fct.pBody = pBd;
            facetList.push_back(fct);
        }
        if (fg.top[i1] != fg.bot[i1]) {
            Facet3Pt fct(fg.bot[i0], fg.bot[i1], fg.top[i1]);
            fct.nType = FacetType::FCT_SIDE;
            fct.pBody = pBd;
            facetList.push_back(fct);
        }
    }
}

void Model::initSideFacets_3(const FctGen &fg, std::vector<Facet3Pt> &facetList, int i0, int i1)
{
    if ((fg.top[i0] != fg.bot[i0]) || (fg.top[i1] != fg.bot[i1])) {
        Body *pBd = getBody(fg.id);
        if (fg.top[i0] != fg.bot[i0]) {
            Facet3Pt fct(fg.top[i0], fg.bot[i0], fg.top[i1]);
            fct.nType = FacetType::FCT_SIDE;
            fct.pBody = pBd;
            facetList.push_back(fct);
        }
        if (fg.top[i1] != fg.bot[i1]) {
            Facet3Pt fct(fg.bot[i0], fg.bot[i1], fg.top[i1]);
            fct.nType = FacetType::FCT_SIDE;
            fct.pBody = pBd;
            facetList.push_back(fct);
        }
    }
}

void Model::initSideFacets(const FctGen &fg, std::vector<Facet3Pt> &facetList)
{
    if (fg.l == 2) {
        initSideFacets_2(fg, facetList, 1, 0);
        initSideFacets_2(fg, facetList, 2, 1);
        initSideFacets_2(fg, facetList, 3, 2);
        initSideFacets_2(fg, facetList, 0, 3);
    }
    if (fg.l == 3) {
        if (fg.b[0] && fg.b[1] && fg.b[2]) {
            initSideFacets_3(fg, facetList, 2, 4);
            initSideFacets_3(fg, facetList, 4, 0);
        }
        if (fg.b[1] && fg.b[2] && fg.b[3]) {
            initSideFacets_3(fg, facetList, 3, 4);
            initSideFacets_3(fg, facetList, 4, 1);
        }
        if (fg.b[2] && fg.b[3] && fg.b[0]) {
            initSideFacets_3(fg, facetList, 0, 4);
            initSideFacets_3(fg, facetList, 4, 2);
        }
        if (fg.b[3] && fg.b[0] && fg.b[1]) {
            initSideFacets_3(fg, facetList, 1, 4);
            initSideFacets_3(fg, facetList, 4, 3);
        }
    }
}

void Model::initFacet(const FctGen &pU, const FctGen &pL, std::vector<Facet3Pt> &facetList, int i0, int i1)
{
    const int i2 = 4;
    Facet3Pt fct;
    fct.pBody = getBody(pU.id);

    if (pU.bot[i0] == pL.top[i0] && pU.bot[i1] == pL.top[i1] && pU.bot[i2] == pL.top[i2]) {
        fct.pBodyOpos = getBody(pU.id);
        fct.pBody = getBody(pL.id);
        fct.Init(pL.top[i0], pL.top[i1], pL.top[i2]);
        facetList.push_back(fct);
    } else {
        if (pU.b[i0] && pU.b[i1]) {
            fct.pBody = getBody(pU.id);
            fct.Init(pU.bot[i0], pU.bot[i2], pU.bot[i1]);
            facetList.push_back(fct);
        }
        if (pL.b[i0] && pL.b[i1]) {
            fct.pBody = getBody(pL.id);
            fct.Init(pL.top[i0], pL.top[i1], pL.top[i2]);
            facetList.push_back(fct);
        }
    }
}

void Model::initFacetTBM(const FctGen &fg, std::vector<Facet3Pt> &facetList, int i0, int i1, FgType tag)
{
    const int i2 = 4;
    Facet3Pt fct;
    fct.pBody = getBody(fg.id);

    if (tag == FgType::TopMost) {
        fct.Init(fg.top[i0], fg.top[i1], fg.top[i2]);
    } else if (tag == FgType::BotMost) {
        fct.Init(fg.bot[i0], fg.bot[i2], fg.bot[i1]);
    }
    facetList.push_back(fct);
}

void Model::fgToFct(std::vector<FctGen> &fga, std::vector<Facet3Pt> &facetList, int i0, int i1)
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
                initFacetTBM(*pC, facetList, i0, i1, FgType::TopMost);
                bFirst = false;
            }
            pB = pC;
            while (i < n) {
                FctGen *pN = &fga[i];
                if (pN->b[i0] && pN->b[i1] &&
                    (pN->top[i0] != pN->bot[i0] || pN->top[i1] != pN->bot[i1] || pN->top[i2] != pN->bot[i2]))
                {
                    initFacet(*pC, *pN, facetList, i0, i1);
                    pB = pN;
                    break;
                } else {
                    i++;
                }
            }
        }
    }
    if (pB) {
        initFacetTBM(*pB, facetList, i0, i1, FgType::BotMost);
    }
}

int Model::generateFacets(std::vector<Facet3Pt> &facetList, int row, int col)
{
    if (row < 0 || col < 0 || row > m_nRows - 2 || col > m_nCols - 2) return 0;

    std::vector<FctGen> fga;
    fga.reserve(16);

    addFctGen(fga, row, col, 0);
    addFctGen(fga, row, col + 1, 1);
    addFctGen(fga, row + 1, col + 1, 2);
    addFctGen(fga, row + 1, col, 3);

    if (fga.empty()) return 0;

    for (size_t i = 0; i < fga.size(); ) {
        FctGen &fg = fga[i];
        fgToFctBound(fg, facetList);
        if (fg.l <= 2) {
            if (fg.l == 2) initSideFacets(fg, facetList);
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
                fg.top[4].x = fg.bot[4].x = getX2e(col);
                fg.top[4].y = fg.bot[4].y = getY2e(row);
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
        initSideFacets(pC, facetList);
    }

    fgToFct(fga, facetList, 0, 1);
    fgToFct(fga, facetList, 1, 2);
    fgToFct(fga, facetList, 2, 3);
    fgToFct(fga, facetList, 3, 0);

    return static_cast<int>(facetList.size());
}

void Model::updateFacetColumn(int row, int col)
{
    std::vector<Facet3Pt> *pFctLst = getFacetList(row, col);
    if (!pFctLst) return;

    if (m_computeRealTime) {
        for (auto &fct : *pFctLst) {
            Facet3Pt fCopy = fct;
            fCopy.SetSign(-1.0);
            if (fCopy.pBody) {
                fCopy.density = fCopy.pBody->GetDensity();
                fCopy.v_densGrad = fCopy.pBody->GetDensityGradient();
            }
            if (fCopy.pBodyOpos) {
                fCopy.densityOpos = fCopy.pBodyOpos->GetDensity();
                fCopy.v_densGradOpos = fCopy.pBodyOpos->GetDensityGradient();
            }
            m_fctLstUpdate.push_back(fCopy);
        }
    }

    pFctLst->clear();
    generateFacets(*pFctLst, row, col);

    if (m_computeRealTime) {
        for (const auto &fct : *pFctLst) {
            Facet3Pt fCopy = fct;
            fCopy.SetSign(1.0);
            if (fCopy.pBody) {
                fCopy.density = fCopy.pBody->GetDensity();
                fCopy.v_densGrad = fCopy.pBody->GetDensityGradient();
            }
            if (fCopy.pBodyOpos) {
                fCopy.densityOpos = fCopy.pBodyOpos->GetDensity();
                fCopy.v_densGradOpos = fCopy.pBodyOpos->GetDensityGradient();
            }
            m_fctLstUpdate.push_back(fCopy);
        }
    }
}

void Model::updateFacetList(int row, int col)
{
    updateFacetColumn(row - 1, col - 1);
    updateFacetColumn(row - 1, col);
    updateFacetColumn(row, col - 1);
    updateFacetColumn(row, col);
}

void Model::initFacetList()
{
    for (int r = 0; r < m_nRows - 1; ++r) {
        for (int c = 0; c < m_nCols - 1; ++c) {
            updateFacetColumn(r, c);
            std::vector<Facet3Pt> *lst = getFacetList(r, c);
            if (lst) {
                for (auto &fct : *lst) {
                    fct.SetSign(-1.0);
                }
            }
        }
    }
}

int Model::getFacetsComputation(std::vector<Facet3Pt> &outList) const
{
    outList.clear();
    for (int r = 0; r < m_nRows - 1; ++r) {
        for (int c = 0; c < m_nCols - 1; ++c) {
            const std::vector<Facet3Pt> *lst = getFacetList(r, c);
            if (lst) {
                for (const auto &fct : *lst) {
                    Facet3Pt copy = fct;
                    copy.SetSign(1.0);
                    if (copy.pBody) {
                        copy.density = copy.pBody->GetDensity();
                        copy.v_densGrad = copy.pBody->GetDensityGradient();
                    }
                    if (copy.pBodyOpos) {
                        copy.densityOpos = copy.pBodyOpos->GetDensity();
                        copy.v_densGradOpos = copy.pBodyOpos->GetDensityGradient();
                    }
                    outList.push_back(copy);
                }
            }
        }
    }
    return static_cast<int>(outList.size());
}

} // namespace mod3d
