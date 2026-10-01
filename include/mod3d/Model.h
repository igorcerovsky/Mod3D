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

    // Initialization
    bool init(int nRows, int nCols, double x0, double y0, double xSize, double ySize, double zMin, double zMax);
    bool init(const Grid &reliefGrid, double zMin, double zMax);
    bool isInitialized() const { return m_initialized; }

    // Dimensions and Coordinates
    int getRows() const { return m_nRows; }
    int getCols() const { return m_nCols; }
    double getX0() const { return m_x0; }
    double getY0() const { return m_y0; }
    double getXSize() const { return m_xSize; }
    double getYSize() const { return m_ySize; }
    double getZMin() const { return m_zMin; }
    double getZMax() const { return m_zMax; }
    double getHell() const { return m_zMin; }
    double getHeaven() const { return m_zMax; }

    double getXd(int col) const { return m_x0 + (col - 1) * m_xSize; }
    double getYd(int row) const { return m_y0 + (row - 1) * m_ySize; }
    double getXe(int col) const;
    double getYe(int row) const;
    double getX2e(int col) const;
    double getY2e(int row) const;

    // Column Point Access
    size_t getCount(int row, int col) const;
    int getUpperBound(int row, int col) const;
    const ColumnPoint *getAt(int row, int col, int index) const;
    ColumnPoint *getAt(int row, int col, int index);
    void add(int row, int col, const ColumnPoint &pt);
    void insertAt(int row, int col, int index, const ColumnPoint &pt);
    void removeAt(int row, int col, int index, int count = 1);

    double getZ(int row, int col, int index) const;
    void setZ(int row, int col, int index, double z);
    double getThickness(int row, int col, int index) const;
    double getDefaultThickness() const { return (m_zMax - m_zMin) / 10.0; }

    // Vertex Manipulation
    int moveVertex(int &index, int row, int col, double z, BodyMoveType moveType = BodyMoveType::Normal);

    // Body Management
    Body *newBody();
    int insertBody(int row, int col, double z, double thickness = -1.0, bool isNew = true, int bodyId = -1, double zT = 0.0, double zB = 0.0, bool bZ = true);
    int removeBody(int index, int row, int col);
    int deleteBody(int bodyId);
    Body *getBody(int bodyId);
    const Body *getBody(int bodyId) const;
    int getBodyIndex(int bodyId, int row, int col) const;
    int getID(int index, int row, int col) const;
    const std::vector<std::unique_ptr<Body>> &getBodies() const { return m_bodies; }
    void updateBodyIndex();

    // Facet Generation
    void initFacetList();
    int generateFacets(std::vector<Facet3Pt> &facetList, int row, int col);
    void updateFacetColumn(int row, int col);
    void updateFacetList(int row, int col);
    int getFacetsComputation(std::vector<Facet3Pt> &outList) const;
    std::vector<Facet3Pt> *getFacetList(int row, int col);
    const std::vector<Facet3Pt> *getFacetList(int row, int col) const;

    // Real-Time Delta Updates
    void setComputeRealTime(bool realTime = true) { m_computeRealTime = realTime; }
    bool isComputeRealTime() const { return m_computeRealTime; }
    std::vector<Facet3Pt> &getFacetsUpdate() { return m_fctLstUpdate; }
    const std::vector<Facet3Pt> &getFacetsUpdate() const { return m_fctLstUpdate; }
    void clearFacetsUpdate() { m_fctLstUpdate.clear(); }

    // Boundary Extensions
    void setExtensions(double exN, double exS, double exE, double exW, bool enable = true) {
        m_dExN = exN; m_dExS = exS; m_dExE = exE; m_dExW = exW; m_extend = enable;
    }

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

    bool m_computeRealTime{false};
    std::vector<Facet3Pt> m_fctLstUpdate;
};

} // namespace mod3d
