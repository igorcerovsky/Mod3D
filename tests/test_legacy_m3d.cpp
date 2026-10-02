#include <gtest/gtest.h>
#include "mod3d/Project.h"
#include <filesystem>
#include <fstream>
#include <cstring>
#include <cstdint>

using namespace mod3d;
namespace fs = std::filesystem;

namespace {

void writeCString(std::ostream &os, const std::string &str) {
    if (str.length() < 0xFF) {
        uint8_t len = static_cast<uint8_t>(str.length());
        os.write(reinterpret_cast<const char*>(&len), 1);
    } else {
        uint8_t marker = 0xFF;
        os.write(reinterpret_cast<const char*>(&marker), 1);
        uint16_t len = static_cast<uint16_t>(str.length());
        os.write(reinterpret_cast<const char*>(&len), 2);
    }
    os.write(str.data(), str.length());
}

void writeArrayCount(std::ostream &os, uint32_t count) {
    if (count < 0xFFFF) {
        uint16_t c16 = static_cast<uint16_t>(count);
        os.write(reinterpret_cast<const char*>(&c16), 2);
    } else {
        uint16_t marker = 0xFFFF;
        os.write(reinterpret_cast<const char*>(&marker), 2);
        os.write(reinterpret_cast<const char*>(&count), 4);
    }
}

template <typename T>
void writeVal(std::ostream &os, T val) {
    os.write(reinterpret_cast<const char*>(&val), sizeof(T));
}

void createSyntheticLegacyM3D(const std::string &filename) {
    std::ofstream os(filename, std::ios::binary);

    // 1. 256-byte header
    char hdr[256] = {0};
    std::snprintf(hdr, sizeof(hdr), "Mod3D 20040419");
    os.write(hdr, 256);

    // Version & docId
    writeVal<int32_t>(os, 20040419);
    writeVal<int32_t>(os, 1);

    // Observation header
    writeVal<int32_t>(os, 3); // nRows
    writeVal<int32_t>(os, 3); // nCols
    writeVal<double>(os, 0.0); // x0
    writeVal<double>(os, 0.0); // y0
    writeVal<double>(os, 100.0); // xSize
    writeVal<double>(os, 100.0); // ySize
    writeVal<double>(os, 0.0); // dMinX
    writeVal<double>(os, 200.0); // dMaxX
    writeVal<double>(os, 0.0); // dMinY
    writeVal<double>(os, 200.0); // dMaxY
    writeVal<double>(os, -2000.0); // dMinZ
    writeVal<double>(os, 200.0); // dMaxZ

    // Computation
    writeVal<int32_t>(os, 1); // m_nComputationType (real-time true)
    writeVal<int32_t>(os, 0); // m_bSherComp

    // Gravity
    writeVal<int32_t>(os, 0); // formula
    writeVal<double>(os, 2670.0); // grvDensRef
    writeVal<double>(os, 1.0); // grvUnits
    writeVal<double>(os, 0.0); writeVal<double>(os, 0.0); writeVal<double>(os, 0.0); // grad
    writeVal<double>(os, 0.0); writeVal<double>(os, 0.0); writeVal<double>(os, 0.0); // origo
    writeVal<int32_t>(os, 0); // tensTag
    writeVal<int32_t>(os, 1); // tensCompute
    writeVal<double>(os, 250.0); // tensFlightElev
    writeVal<double>(os, 40.0); // tensHeight
    writeVal<double>(os, 1.0); // tensUnits
    writeVal<double>(os, 0.0); // grvSens
    writeVal<double>(os, 40.0); // grvElev
    writeVal<int32_t>(os, 0); // grvObsTag
    writeVal<int32_t>(os, 0); // remMeanGrv
    writeVal<int32_t>(os, 0); // remMeanTns

    // Magnetic
    writeVal<int32_t>(os, 0); // magFormula
    writeVal<double>(os, 0.0); // magSens
    writeVal<double>(os, 250.0); // magElev
    writeVal<double>(os, 12000.0); writeVal<double>(os, 2500.0); writeVal<double>(os, 46000.0); // indFld
    writeVal<int32_t>(os, 0); // magObsTag
    writeVal<int32_t>(os, 0); // remMeanMag

    // Fitting parameters (16 ints/doubles)
    writeVal<int32_t>(os, 0); // fitFld
    writeVal<int32_t>(os, 0); writeVal<int32_t>(os, 10); writeVal<int32_t>(os, 0);
    writeVal<double>(os, 1e-4); writeVal<double>(os, 1e-4); writeVal<double>(os, 1e-4);
    writeVal<int32_t>(os, 0); writeVal<int32_t>(os, 0);
    writeVal<int32_t>(os, 0); writeVal<int32_t>(os, 10); writeVal<int32_t>(os, 0);
    writeVal<double>(os, 1e-4); writeVal<double>(os, 1e-4);
    writeVal<int32_t>(os, 0);

    // CModel::Serialize
    writeVal<int32_t>(os, 1); // model ID
    writeCString(os, "Synthetic Legacy Model");
    writeVal<int32_t>(os, 1); // body ID

    writeVal<int32_t>(os, 3); // rows
    writeVal<int32_t>(os, 3); // cols
    writeVal<double>(os, 0.0); // x0
    writeVal<double>(os, 100.0); // xSize
    writeVal<double>(os, 0.0); // y0
    writeVal<double>(os, 100.0); // ySize
    writeVal<double>(os, 0.0); writeVal<double>(os, 200.0); // xMin, xMax
    writeVal<double>(os, 0.0); writeVal<double>(os, 200.0); // yMin, yMax
    writeVal<double>(os, -2000.0); writeVal<double>(os, 200.0); // zMin, zMax

    writeVal<int32_t>(os, 0); // bExtend
    writeVal<double>(os, 0.0); writeVal<double>(os, 0.0); writeVal<double>(os, 0.0); writeVal<double>(os, 0.0); // dEx

    // 9 column cells
    for (int i = 0; i < 9; ++i) {
        writeArrayCount(os, 2); // 2 points per column
        // pt 0
        writeVal<int32_t>(os, 0); // bodyId
        writeVal<double>(os, 200.0); // m_z
        writeVal<double>(os, (i % 3) * 100.0); // x
        writeVal<double>(os, (i / 3) * 100.0); // y
        writeVal<double>(os, 200.0); // z
        // pt 1
        writeVal<int32_t>(os, 0); // bodyId
        writeVal<double>(os, -2000.0); // m_z
        writeVal<double>(os, (i % 3) * 100.0); // x
        writeVal<double>(os, (i / 3) * 100.0); // y
        writeVal<double>(os, -2000.0); // z
    }

    // Bodies array
    writeArrayCount(os, 1);
    // Body 0
    writeVal<int32_t>(os, 1); // bId
    writeCString(os, "Basement Granodiorite");
    writeCString(os, "Crystalline basement");
    writeVal<int32_t>(os, 1); // bActive
    writeVal<int32_t>(os, 0); // bLocked
    writeVal<int32_t>(os, 1); // bShow
    writeVal<double>(os, 2820.0); // density
    writeVal<double>(os, 0.0); writeVal<double>(os, 0.0); writeVal<double>(os, 0.0); // densGrad
    writeVal<double>(os, 0.0); writeVal<double>(os, 0.0); writeVal<double>(os, 0.0); // densOrg
    writeVal<double>(os, 0.035); // susc
    writeVal<double>(os, 0.1); writeVal<double>(os, 0.0); writeVal<double>(os, -0.2); // rem

    // Pen & brush attributes (13 DWORDs)
    uint32_t colorRef = 0x004080; // RGB: red=0x80, green=0x40, blue=0x00
    writeVal<uint32_t>(os, colorRef); writeVal<int32_t>(os, 0); writeVal<int32_t>(os, 1);
    writeVal<uint32_t>(os, colorRef); writeVal<int32_t>(os, 0); writeVal<int32_t>(os, 0);
    writeVal<uint32_t>(os, colorRef); writeVal<int32_t>(os, 0); writeVal<int32_t>(os, 1);
    writeVal<uint32_t>(os, colorRef); writeVal<int32_t>(os, 0); writeVal<int32_t>(os, 0);
    writeVal<int32_t>(os, 1); // bFill
}

} // namespace

TEST(LegacyM3DTest, ParseSyntheticBinaryArchive) {
    namespace fs = std::filesystem;
    const std::string m3dPath = "test_synthetic.m3d";

    createSyntheticLegacyM3D(m3dPath);
    ASSERT_TRUE(fs::exists(m3dPath));

    Project proj;
    EXPECT_TRUE(proj.loadLegacyM3D(m3dPath));

    fs::remove(m3dPath);

    // Verify Model
    EXPECT_EQ(proj.model().getRows(), 3);
    EXPECT_EQ(proj.model().getCols(), 3);
    EXPECT_DOUBLE_EQ(proj.model().getX0(), 0.0);
    EXPECT_DOUBLE_EQ(proj.model().getY0(), 0.0);
    EXPECT_DOUBLE_EQ(proj.model().getXSize(), 100.0);
    EXPECT_DOUBLE_EQ(proj.model().getYSize(), 100.0);
    EXPECT_DOUBLE_EQ(proj.model().getZMin(), -2000.0);
    EXPECT_DOUBLE_EQ(proj.model().getZMax(), 200.0);

    // Verify Body
    ASSERT_EQ(proj.model().getBodies().size(), 1u);
    const Body *b = proj.model().getBody(1);
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->GetName(), "Basement Granodiorite");
    EXPECT_EQ(b->GetDescription(), "Crystalline basement");
    EXPECT_DOUBLE_EQ(b->GetRawDensity(), 2820.0);
    EXPECT_DOUBLE_EQ(b->GetSusceptibility(), 0.035);
    EXPECT_TRUE(b->IsActive());
    EXPECT_FALSE(b->IsLocked());
    EXPECT_TRUE(b->IsVisible());

    // Verify Observation Space
    EXPECT_DOUBLE_EQ(proj.observation().getReferenceDensity(), 2670.0);
    EXPECT_EQ(proj.observation().getGravityMode(), ObservationMode::SensorHeight);
    EXPECT_DOUBLE_EQ(proj.observation().getGravityHeight(), 40.0);
    EXPECT_EQ(proj.observation().getMagneticMode(), ObservationMode::FlightElevation);
    EXPECT_DOUBLE_EQ(proj.observation().getMagneticHeight(), 250.0);
}

TEST(LegacyM3DTest, LoadAuthenticExample_Test_m3d) {
    std::string path = std::string(MOD3D_EXAMPLES_DIR) + "/Test.m3d";
    ASSERT_TRUE(fs::exists(path)) << "Path does not exist: " << path;

    Project proj;
    ASSERT_TRUE(proj.loadLegacyM3D(path));

    // Verify Model Geometry
    EXPECT_EQ(proj.model().getRows(), 23);
    EXPECT_EQ(proj.model().getCols(), 23);
    EXPECT_DOUBLE_EQ(proj.model().getX0(), -100.0);
    EXPECT_DOUBLE_EQ(proj.model().getY0(), -100.0);
    EXPECT_DOUBLE_EQ(proj.model().getXSize(), 10.0);
    EXPECT_DOUBLE_EQ(proj.model().getYSize(), 10.0);
    EXPECT_DOUBLE_EQ(proj.model().getZMin(), -100.0);
    EXPECT_DOUBLE_EQ(proj.model().getZMax(), 0.0);
    EXPECT_TRUE(proj.model().isExtend());

    // Verify Bodies
    ASSERT_EQ(proj.model().getBodies().size(), 1u);
    const Body *b0 = proj.model().getBody(0);
    ASSERT_NE(b0, nullptr);
    EXPECT_EQ(b0->GetName(), "Name the body...");
    EXPECT_DOUBLE_EQ(b0->GetRawDensity(), 2700.0);
    EXPECT_NEAR(b0->GetSusceptibility(), 0.01, 1e-6);

    // Verify Facets Generation
    std::vector<Facet3Pt> facets;
    proj.model().getFacetsComputation(facets);
    EXPECT_GT(facets.size(), 10u);
    for (const auto &f : facets) {
        Point3D u = f.pts[1] - f.pts[0];
        Point3D v = f.pts[2] - f.pts[0];
        EXPECT_GT(u.cross(v).length(), 0.0);
    }

    // Verify Observation Space
    EXPECT_EQ(proj.observation().getRows(), 21u);
    EXPECT_EQ(proj.observation().getCols(), 21u);
    EXPECT_DOUBLE_EQ(proj.observation().getX0(), -100.0);
    EXPECT_DOUBLE_EQ(proj.observation().getY0(), -100.0);
    EXPECT_DOUBLE_EQ(proj.observation().getDx(), 10.0);
    EXPECT_DOUBLE_EQ(proj.observation().getDy(), 10.0);

    // Verify Observation Relief Grid
    const Grid &relief = proj.observation().getSurfaceRelief();
    EXPECT_FALSE(relief.empty());
    EXPECT_EQ(relief.rows(), 21u);
    EXPECT_EQ(relief.cols(), 21u);

    // Verify Forward Modeling Simulation on Loaded Project
    proj.observation().computeForwardField(facets, true);
    const Grid *gzModeled = proj.observation().getModeledGrid(FieldComponent::GZ);
    ASSERT_NE(gzModeled, nullptr);
    EXPECT_FALSE(gzModeled->empty());
    EXPECT_EQ(gzModeled->rows(), 21u);
    EXPECT_EQ(gzModeled->cols(), 21u);
}

TEST(LegacyM3DTest, LoadAuthenticExample_Sample_m3d) {
    std::string path = std::string(MOD3D_EXAMPLES_DIR) + "/Sample.m3d";
    ASSERT_TRUE(fs::exists(path)) << "Path does not exist: " << path;

    Project proj;
    ASSERT_TRUE(proj.loadLegacyM3D(path));

    // Verify Model Geometry
    EXPECT_EQ(proj.model().getRows(), 23);
    EXPECT_EQ(proj.model().getCols(), 23);
    EXPECT_DOUBLE_EQ(proj.model().getX0(), 0.0);
    EXPECT_DOUBLE_EQ(proj.model().getY0(), 0.0);
    EXPECT_DOUBLE_EQ(proj.model().getXSize(), 400.0);
    EXPECT_DOUBLE_EQ(proj.model().getYSize(), 400.0);
    EXPECT_DOUBLE_EQ(proj.model().getZMin(), -2000.0);
    EXPECT_DOUBLE_EQ(proj.model().getZMax(), 500.0);

    // Verify 3 Physical Bodies
    ASSERT_EQ(proj.model().getBodies().size(), 3u);
    const Body *b0 = proj.model().getBody(0);
    ASSERT_NE(b0, nullptr);
    EXPECT_EQ(b0->GetName(), "Body");
    EXPECT_DOUBLE_EQ(b0->GetRawDensity(), 2800.0);

    const Body *b1 = proj.model().getBody(1);
    ASSERT_NE(b1, nullptr);
    EXPECT_EQ(b1->GetName(), "Wings");
    EXPECT_DOUBLE_EQ(b1->GetRawDensity(), 2640.0);

    const Body *b2 = proj.model().getBody(2);
    ASSERT_NE(b2, nullptr);
    EXPECT_EQ(b2->GetName(), "... and I feel fine");
    EXPECT_DOUBLE_EQ(b2->GetRawDensity(), 3400.0);

    // Verify Multi-Body Facets Generation
    std::vector<Facet3Pt> facets;
    proj.model().getFacetsComputation(facets);
    EXPECT_GT(facets.size(), 300u);

    // Verify Polyhedral Mesh Exporters with Authentic Sample Model
    std::string objOut = (fs::temp_directory_path() / "test_sample_export.obj").string();
    std::string stlOut = (fs::temp_directory_path() / "test_sample_export.stl").string();
    std::string vtkOut = (fs::temp_directory_path() / "test_sample_export.vtk").string();

    EXPECT_TRUE(proj.exportObj(objOut));
    EXPECT_TRUE(proj.exportStl(stlOut, true));
    EXPECT_TRUE(proj.exportVtk(vtkOut));

    EXPECT_GT(fs::file_size(objOut), 1000u);
    EXPECT_GT(fs::file_size(stlOut), 1000u);
    EXPECT_GT(fs::file_size(vtkOut), 1000u);

    fs::remove(objOut);
    fs::remove(stlOut);
    fs::remove(vtkOut);
}

TEST(LegacyM3DTest, LoadAuthenticExample_TestMag_m3d) {
    std::string path = std::string(MOD3D_EXAMPLES_DIR) + "/TestMag.m3d";
    ASSERT_TRUE(fs::exists(path));

    Project proj;
    ASSERT_TRUE(proj.loadLegacyM3D(path));

    EXPECT_EQ(proj.model().getRows(), 23);
    EXPECT_EQ(proj.model().getCols(), 23);
    ASSERT_EQ(proj.model().getBodies().size(), 1u);

    std::vector<Facet3Pt> facets;
    proj.model().getFacetsComputation(facets);
    EXPECT_GT(facets.size(), 50u);

    // Compute magnetic forward field
    proj.observation().computeForwardField(facets, true);
    const Grid *dTModeled = proj.observation().getModeledGrid(FieldComponent::DELTA_T);
    ASSERT_NE(dTModeled, nullptr);
    EXPECT_FALSE(dTModeled->empty());
}

TEST(LegacyM3DTest, LoadAuthenticExample_NestedAndInclinedBodies) {
    std::string nestedPath = std::string(MOD3D_EXAMPLES_DIR) + "/NestedInside.m3d";
    ASSERT_TRUE(fs::exists(nestedPath));

    Project projNested;
    ASSERT_TRUE(projNested.loadLegacyM3D(nestedPath));
    EXPECT_EQ(projNested.model().getBodies().size(), 2u);
    std::vector<Facet3Pt> nestedFacets;
    projNested.model().getFacetsComputation(nestedFacets);
    EXPECT_GT(nestedFacets.size(), 100u);

    std::string inclinedPath = std::string(MOD3D_EXAMPLES_DIR) + "/InclinedBody.m3d";
    ASSERT_TRUE(fs::exists(inclinedPath));

    Project projInclined;
    ASSERT_TRUE(projInclined.loadLegacyM3D(inclinedPath));
    EXPECT_EQ(projInclined.model().getBodies().size(), 1u);
    std::vector<Facet3Pt> inclinedFacets;
    projInclined.model().getFacetsComputation(inclinedFacets);
    EXPECT_GT(inclinedFacets.size(), 100u);
}

TEST(LegacyM3DTest, LoadAuthenticGrids_Surfer6Binary) {
    std::string reliefPath = std::string(MOD3D_EXAMPLES_DIR) + "/SampleRelief.grd";
    ASSERT_TRUE(fs::exists(reliefPath));

    Grid relief;
    EXPECT_TRUE(relief.loadSrf6Binary(reliefPath));
    EXPECT_EQ(relief.rows(), 21u);
    EXPECT_EQ(relief.cols(), 21u);
    EXPECT_DOUBLE_EQ(relief.x0(), 0.0);
    EXPECT_DOUBLE_EQ(relief.y0(), 0.0);
    EXPECT_DOUBLE_EQ(relief.xSize(), 400.0);
    EXPECT_DOUBLE_EQ(relief.ySize(), 400.0);
    EXPECT_NEAR(relief.getMin(), 131.45, 0.1);
    EXPECT_NEAR(relief.getMax(), 484.95, 0.1);

    // Test grid with dummy (nodata) values
    std::string dummyPath = std::string(MOD3D_EXAMPLES_DIR) + "/gz_dummy.grd";
    ASSERT_TRUE(fs::exists(dummyPath));

    Grid gzDummy;
    EXPECT_TRUE(gzDummy.loadSrf6Binary(dummyPath));
    EXPECT_EQ(gzDummy.rows(), 21u);
    EXPECT_EQ(gzDummy.cols(), 21u);

    // Count dummy cells
    size_t dummyCount = 0;
    for (size_t r = 0; r < gzDummy.rows(); ++r) {
        for (size_t c = 0; c < gzDummy.cols(); ++c) {
            if (gzDummy.isDummy(r, c)) {
                ++dummyCount;
            }
        }
    }
    EXPECT_EQ(dummyCount, 42u);

    // Min and max must properly exclude dummy values
    EXPECT_NEAR(gzDummy.getMin(), -12.06, 0.1);
    EXPECT_NEAR(gzDummy.getMax(), 7.82, 0.1);
}

