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
    EXPECT_TRUE(proj.load_legacy_m3d(m3dPath));

    fs::remove(m3dPath);

    // Verify Model
    EXPECT_EQ(proj.model().rows(), 3);
    EXPECT_EQ(proj.model().cols(), 3);
    EXPECT_DOUBLE_EQ(proj.model().x0(), 0.0);
    EXPECT_DOUBLE_EQ(proj.model().y0(), 0.0);
    EXPECT_DOUBLE_EQ(proj.model().dx(), 100.0);
    EXPECT_DOUBLE_EQ(proj.model().dy(), 100.0);
    EXPECT_DOUBLE_EQ(proj.model().z_min(), -2000.0);
    EXPECT_DOUBLE_EQ(proj.model().z_max(), 200.0);

    // Verify Body
    ASSERT_EQ(proj.model().bodies().size(), 1u);
    const Body *b = proj.model().body(1);
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->name(), "Basement Granodiorite");
    EXPECT_EQ(b->description(), "Crystalline basement");
    EXPECT_DOUBLE_EQ(b->raw_density(), 2820.0);
    EXPECT_DOUBLE_EQ(b->susceptibility(), 0.035);
    EXPECT_TRUE(b->is_active());
    EXPECT_FALSE(b->is_locked());
    EXPECT_TRUE(b->is_visible());

    // Verify Observation Space
    EXPECT_DOUBLE_EQ(proj.observation().reference_density(), 2670.0);
    EXPECT_EQ(proj.observation().gravity_mode(), ObservationMode::SensorHeight);
    EXPECT_DOUBLE_EQ(proj.observation().gravity_height(), 40.0);
    EXPECT_EQ(proj.observation().magnetic_mode(), ObservationMode::FlightElevation);
    EXPECT_DOUBLE_EQ(proj.observation().magnetic_height(), 250.0);
}

TEST(LegacyM3DTest, LoadAuthenticExample_Test_m3d) {
    std::string path = std::string(MOD3D_TEST_DATA_DIR) + "/models/Test.m3d";
    ASSERT_TRUE(fs::exists(path)) << "Path does not exist: " << path;

    Project proj;
    ASSERT_TRUE(proj.load_legacy_m3d(path));

    // Verify Model Geometry
    EXPECT_EQ(proj.model().rows(), 23);
    EXPECT_EQ(proj.model().cols(), 23);
    EXPECT_DOUBLE_EQ(proj.model().x0(), -100.0);
    EXPECT_DOUBLE_EQ(proj.model().y0(), -100.0);
    EXPECT_DOUBLE_EQ(proj.model().dx(), 10.0);
    EXPECT_DOUBLE_EQ(proj.model().dy(), 10.0);
    EXPECT_DOUBLE_EQ(proj.model().z_min(), -100.0);
    EXPECT_DOUBLE_EQ(proj.model().z_max(), 0.0);
    EXPECT_TRUE(proj.model().is_extend());

    // Verify Bodies
    ASSERT_EQ(proj.model().bodies().size(), 1u);
    const Body *b0 = proj.model().body(0);
    ASSERT_NE(b0, nullptr);
    EXPECT_EQ(b0->name(), "Name the body...");
    EXPECT_DOUBLE_EQ(b0->raw_density(), 2700.0);
    EXPECT_NEAR(b0->susceptibility(), 0.01, 1e-6);

    // Verify Facets Generation
    std::vector<Facet3Pt> facets;
    proj.model().facets_computation(facets);
    EXPECT_GT(facets.size(), 10u);
    for (const auto &f : facets) {
        Point3D u = f.pts[1] - f.pts[0];
        Point3D v = f.pts[2] - f.pts[0];
        EXPECT_GT(u.cross(v).length(), 0.0);
    }

    // Verify Observation Space
    EXPECT_EQ(proj.observation().rows(), 21u);
    EXPECT_EQ(proj.observation().cols(), 21u);
    EXPECT_DOUBLE_EQ(proj.observation().x0(), -100.0);
    EXPECT_DOUBLE_EQ(proj.observation().y0(), -100.0);
    EXPECT_DOUBLE_EQ(proj.observation().dx(), 10.0);
    EXPECT_DOUBLE_EQ(proj.observation().dy(), 10.0);

    // Verify Observation Relief Grid
    const Grid &relief = proj.observation().surface_relief();
    EXPECT_FALSE(relief.empty());
    EXPECT_EQ(relief.rows(), 21u);
    EXPECT_EQ(relief.cols(), 21u);

    // Verify Forward Modeling Simulation on Loaded Project
    proj.observation().compute_forward_field(facets, true);
    const Grid *gzModeled = proj.observation().modeled_grid(FieldComponent::GZ);
    ASSERT_NE(gzModeled, nullptr);
    EXPECT_FALSE(gzModeled->empty());
    EXPECT_EQ(gzModeled->rows(), 21u);
    EXPECT_EQ(gzModeled->cols(), 21u);
}

TEST(LegacyM3DTest, LoadAuthenticExample_Sample_m3d) {
    std::string path = std::string(MOD3D_TEST_DATA_DIR) + "/models/Sample.m3d";
    ASSERT_TRUE(fs::exists(path)) << "Path does not exist: " << path;

    Project proj;
    ASSERT_TRUE(proj.load_legacy_m3d(path));

    // Verify Model Geometry
    EXPECT_EQ(proj.model().rows(), 23);
    EXPECT_EQ(proj.model().cols(), 23);
    EXPECT_DOUBLE_EQ(proj.model().x0(), 0.0);
    EXPECT_DOUBLE_EQ(proj.model().y0(), 0.0);
    EXPECT_DOUBLE_EQ(proj.model().dx(), 400.0);
    EXPECT_DOUBLE_EQ(proj.model().dy(), 400.0);
    EXPECT_DOUBLE_EQ(proj.model().z_min(), -2000.0);
    EXPECT_DOUBLE_EQ(proj.model().z_max(), 500.0);

    // Verify 3 Physical Bodies
    ASSERT_EQ(proj.model().bodies().size(), 3u);
    const Body *b0 = proj.model().body(0);
    ASSERT_NE(b0, nullptr);
    EXPECT_EQ(b0->name(), "Body");
    EXPECT_DOUBLE_EQ(b0->raw_density(), 2800.0);

    const Body *b1 = proj.model().body(1);
    ASSERT_NE(b1, nullptr);
    EXPECT_EQ(b1->name(), "Wings");
    EXPECT_DOUBLE_EQ(b1->raw_density(), 2640.0);

    const Body *b2 = proj.model().body(2);
    ASSERT_NE(b2, nullptr);
    EXPECT_EQ(b2->name(), "... and I feel fine");
    EXPECT_DOUBLE_EQ(b2->raw_density(), 3400.0);

    // Verify Multi-Body Facets Generation
    std::vector<Facet3Pt> facets;
    proj.model().facets_computation(facets);
    EXPECT_GT(facets.size(), 300u);

    // Verify Body Attributes & Map Boundary Segments
    EXPECT_FALSE(b0->is_filled());
    EXPECT_NE(b0->color().a, 0);
    EXPECT_NE(b1->color().a, 0);
    EXPECT_NE(b2->color().a, 0);
    EXPECT_GT(proj.model().all_body_boundaries().size(), 0u);
}

TEST(LegacyM3DTest, LoadAuthenticExample_TestMag_m3d) {
    std::string path = std::string(MOD3D_TEST_DATA_DIR) + "/models/TestMag.m3d";
    ASSERT_TRUE(fs::exists(path));

    Project proj;
    ASSERT_TRUE(proj.load_legacy_m3d(path));

    EXPECT_EQ(proj.model().rows(), 23);
    EXPECT_EQ(proj.model().cols(), 23);
    ASSERT_EQ(proj.model().bodies().size(), 1u);

    std::vector<Facet3Pt> facets;
    proj.model().facets_computation(facets);
    EXPECT_GT(facets.size(), 50u);

    // Verify loaded authentic modeled magnetic field from file
    const Grid *dTLoaded = proj.observation().modeled_grid(FieldComponent::DELTA_T);
    ASSERT_NE(dTLoaded, nullptr);
    EXPECT_FALSE(dTLoaded->empty());
    Grid loadedDT = *dTLoaded;
    EXPECT_NEAR(loadedDT(11, 11), -0.496, 0.05);

    // Verify body magnetization was computed on load
    const Body *body = proj.model().bodies().front().get();
    ASSERT_NE(body, nullptr);
    EXPECT_GT(body->susceptibility(), 0.0);
    EXPECT_FALSE(body->magnetization_vector().is_zero());

    // Recompute magnetic forward field and compare with loaded authentic grid
    proj.observation().compute_forward_field(facets, true);
    const Grid *dTRecomputed = proj.observation().modeled_grid(FieldComponent::DELTA_T);
    ASSERT_NE(dTRecomputed, nullptr);
    EXPECT_FALSE(dTRecomputed->empty());

    EXPECT_NEAR((*dTRecomputed)(11, 11), loadedDT(11, 11), 0.05);
    EXPECT_NEAR(dTRecomputed->min(), loadedDT.min(), 0.1);
    EXPECT_NEAR(dTRecomputed->max(), loadedDT.max(), 0.1);
}

TEST(LegacyM3DTest, LoadAuthenticExample_NestedAndInclinedBodies) {
    std::string nestedPath = std::string(MOD3D_TEST_DATA_DIR) + "/models/NestedInside.m3d";
    ASSERT_TRUE(fs::exists(nestedPath));

    Project projNested;
    ASSERT_TRUE(projNested.load_legacy_m3d(nestedPath));
    EXPECT_EQ(projNested.model().bodies().size(), 2u);
    std::vector<Facet3Pt> nestedFacets;
    projNested.model().facets_computation(nestedFacets);
    EXPECT_GT(nestedFacets.size(), 100u);

    std::string inclinedPath = std::string(MOD3D_TEST_DATA_DIR) + "/models/InclinedBody.m3d";
    ASSERT_TRUE(fs::exists(inclinedPath));

    Project projInclined;
    ASSERT_TRUE(projInclined.load_legacy_m3d(inclinedPath));
    EXPECT_EQ(projInclined.model().bodies().size(), 1u);
    std::vector<Facet3Pt> inclinedFacets;
    projInclined.model().facets_computation(inclinedFacets);
    EXPECT_GT(inclinedFacets.size(), 100u);
}

TEST(LegacyM3DTest, LoadAuthenticGrids_Surfer6Binary) {
    std::string reliefPath = std::string(MOD3D_TEST_DATA_DIR) + "/grids/SampleRelief.grd";
    ASSERT_TRUE(fs::exists(reliefPath));

    Grid relief;
    EXPECT_TRUE(relief.load_srf6_binary(reliefPath));
    EXPECT_EQ(relief.rows(), 21u);
    EXPECT_EQ(relief.cols(), 21u);
    EXPECT_DOUBLE_EQ(relief.x0(), 0.0);
    EXPECT_DOUBLE_EQ(relief.y0(), 0.0);
    EXPECT_DOUBLE_EQ(relief.dx(), 400.0);
    EXPECT_DOUBLE_EQ(relief.dy(), 400.0);
    EXPECT_NEAR(relief.min(), 131.45, 0.1);
    EXPECT_NEAR(relief.max(), 484.95, 0.1);

    // Test grid with dummy (nodata) values
    std::string dummyPath = std::string(MOD3D_TEST_DATA_DIR) + "/grids/gz_dummy.grd";
    ASSERT_TRUE(fs::exists(dummyPath));

    Grid gzDummy;
    EXPECT_TRUE(gzDummy.load_srf6_binary(dummyPath));
    EXPECT_EQ(gzDummy.rows(), 21u);
    EXPECT_EQ(gzDummy.cols(), 21u);

    // Count dummy cells
    size_t dummyCount = 0;
    for (size_t r = 0; r < gzDummy.rows(); ++r) {
        for (size_t c = 0; c < gzDummy.cols(); ++c) {
            if (gzDummy.is_dummy(r, c)) {
                ++dummyCount;
            }
        }
    }
    EXPECT_EQ(dummyCount, 42u);

    // Min and max must properly exclude dummy values
    EXPECT_NEAR(gzDummy.min(), -12.06, 0.1);
    EXPECT_NEAR(gzDummy.max(), 7.82, 0.1);
}

TEST(LegacyM3DTest, ConvertAllAuthenticModelsToModernJsonAndRoundtrip) {
    const std::vector<std::string> modelFiles = {
        "Test.m3d",
        "Sample.m3d",
        "TestMag.m3d",
        "NestedInside.m3d",
        "InclinedBody.m3d",
        "TestZeroDensity.m3d"
    };

    for (const auto &fn : modelFiles) {
        std::string m3dPath = std::string(MOD3D_TEST_DATA_DIR) + "/models/" + fn;
        ASSERT_TRUE(fs::exists(m3dPath)) << "Missing test model: " << m3dPath;

        Project origProj;
        ASSERT_TRUE(origProj.load_legacy_m3d(m3dPath)) << "Failed to load legacy: " << fn;

        // Export to modern .mod3d (JSON)
        fs::path jsonPath = fs::temp_directory_path() / (fn + ".mod3d");
        ASSERT_TRUE(origProj.save_json(jsonPath)) << "Failed to save modern JSON for: " << fn;
        ASSERT_TRUE(fs::exists(jsonPath));
        ASSERT_GT(fs::file_size(jsonPath), 500u);

        // Reload the modern project format
        Project reloadedProj;
        ASSERT_TRUE(reloadedProj.load_json(jsonPath)) << "Failed to reload modern JSON for: " << fn;

        // Verify full fidelity between original and reloaded
        EXPECT_EQ(origProj.model().rows(), reloadedProj.model().rows());
        EXPECT_EQ(origProj.model().cols(), reloadedProj.model().cols());
        EXPECT_DOUBLE_EQ(origProj.model().x0(), reloadedProj.model().x0());
        EXPECT_DOUBLE_EQ(origProj.model().y0(), reloadedProj.model().y0());
        EXPECT_DOUBLE_EQ(origProj.model().dx(), reloadedProj.model().dx());
        EXPECT_DOUBLE_EQ(origProj.model().dy(), reloadedProj.model().dy());
        EXPECT_DOUBLE_EQ(origProj.model().z_min(), reloadedProj.model().z_min());
        EXPECT_DOUBLE_EQ(origProj.model().z_max(), reloadedProj.model().z_max());
        EXPECT_EQ(origProj.model().is_extend(), reloadedProj.model().is_extend());

        // Verify Bodies
        ASSERT_EQ(origProj.model().bodies().size(), reloadedProj.model().bodies().size());
        for (const auto &origBody : origProj.model().bodies()) {
            const Body *relBody = reloadedProj.model().body(origBody->id());
            ASSERT_NE(relBody, nullptr) << "Body ID " << origBody->id() << " not found in reloaded " << fn;
            EXPECT_EQ(origBody->name(), relBody->name());
            EXPECT_DOUBLE_EQ(origBody->raw_density(), relBody->raw_density());
            EXPECT_NEAR(origBody->susceptibility(), relBody->susceptibility(), 1e-6);
            EXPECT_EQ(origBody->is_active(), relBody->is_active());
        }

        // Verify Column Points
        for (int r = 0; r < origProj.model().rows(); ++r) {
            for (int c = 0; c < origProj.model().cols(); ++c) {
                size_t cnt1 = origProj.model().count(r, c);
                size_t cnt2 = reloadedProj.model().count(r, c);
                ASSERT_EQ(cnt1, cnt2);
                for (size_t k = 0; k < cnt1; ++k) {
                    const auto *p1 = origProj.model().at(r, c, static_cast<int>(k));
                    const auto *p2 = reloadedProj.model().at(r, c, static_cast<int>(k));
                    ASSERT_NE(p1, nullptr);
                    ASSERT_NE(p2, nullptr);
                    EXPECT_EQ(p1->body_id(), p2->body_id());
                    EXPECT_NEAR(p1->point().x, p2->point().x, 1e-4);
                    EXPECT_NEAR(p1->point().y, p2->point().y, 1e-4);
                    EXPECT_NEAR(p1->point().z, p2->point().z, 1e-4);
                }
            }
        }

        // Verify Facets List
        std::vector<Facet3Pt> fOrig, fReload;
        origProj.model().facets_computation(fOrig);
        reloadedProj.model().facets_computation(fReload);
        EXPECT_EQ(fOrig.size(), fReload.size()) << "Facet mismatch for " << fn;

        // Cleanup
        fs::remove(jsonPath);
    }
}

TEST(LegacyM3DTest, ExportAllAuthenticModelsToObjStlVtk) {
    const std::vector<std::string> modelFiles = {
        "Test.m3d",
        "Sample.m3d",
        "TestMag.m3d",
        "NestedInside.m3d",
        "InclinedBody.m3d"
    };

    for (const auto &fn : modelFiles) {
        std::string m3dPath = std::string(MOD3D_TEST_DATA_DIR) + "/models/" + fn;
        Project proj;
        ASSERT_TRUE(proj.load_legacy_m3d(m3dPath));

        std::vector<Facet3Pt> facets;
        proj.model().facets_computation(facets);
        ASSERT_GT(facets.size(), 0u);

        fs::path baseOut = fs::temp_directory_path() / fn;
        fs::path objPath = baseOut; objPath += ".obj";
        fs::path stlAsciiPath = baseOut; stlAsciiPath += "_ascii.stl";
        fs::path stlBinPath = baseOut; stlBinPath += "_bin.stl";
        fs::path vtkPath = baseOut; vtkPath += ".vtk";

        // 1. Export OBJ
        EXPECT_TRUE(proj.export_obj(objPath));
        ASSERT_TRUE(fs::exists(objPath));
        {
            std::ifstream ifs(objPath);
            std::string line;
            size_t vertexCount = 0, faceCount = 0;
            while (std::getline(ifs, line)) {
                if (line.rfind("v ", 0) == 0) ++vertexCount;
                if (line.rfind("f ", 0) == 0) ++faceCount;
            }
            EXPECT_EQ(faceCount, facets.size());
            EXPECT_EQ(vertexCount, facets.size() * 3);
        }

        // 2. Export STL ASCII
        EXPECT_TRUE(proj.export_stl(stlAsciiPath, -1, false));
        ASSERT_TRUE(fs::exists(stlAsciiPath));
        {
            std::ifstream ifs(stlAsciiPath);
            std::string firstLine;
            std::getline(ifs, firstLine);
            EXPECT_NE(firstLine.find("solid"), std::string::npos);
        }

        // 3. Export STL Binary
        EXPECT_TRUE(proj.export_stl(stlBinPath, -1, true));
        ASSERT_TRUE(fs::exists(stlBinPath));
        {
            size_t binSize = fs::file_size(stlBinPath);
            ASSERT_GE(binSize, 84u);
            std::ifstream ifs(stlBinPath, std::ios::binary);
            ifs.seekg(80);
            uint32_t triCount = 0;
            ifs.read(reinterpret_cast<char *>(&triCount), sizeof(uint32_t));
            EXPECT_EQ(binSize, 84u + triCount * 50u);
            EXPECT_GT(triCount, 0u);
        }

        // 4. Export VTK PolyData
        EXPECT_TRUE(proj.export_vtk(vtkPath));
        ASSERT_TRUE(fs::exists(vtkPath));
        {
            std::ifstream ifs(vtkPath);
            std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
            EXPECT_NE(content.find("# vtk DataFile Version 3.0"), std::string::npos);
            EXPECT_NE(content.find("DATASET POLYDATA"), std::string::npos);
            EXPECT_NE(content.find("CELL_DATA"), std::string::npos);
            EXPECT_NE(content.find("SCALARS Density"), std::string::npos);
        }

        // Clean up
        fs::remove(objPath);
        fs::remove(stlAsciiPath);
        fs::remove(stlBinPath);
        fs::remove(vtkPath);
    }
}

