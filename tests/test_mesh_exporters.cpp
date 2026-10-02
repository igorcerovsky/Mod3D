#include <gtest/gtest.h>
#include "mod3d/Project.h"
#include <filesystem>
#include <fstream>
#include <string>

using namespace mod3d;

namespace {

Project createSampleProjectWithBody() {
    Project proj;
    proj.metadata().title = "Exporters Test Project";
    proj.model().init(3, 3, 0.0, 0.0, 1000.0, 1000.0, -2000.0, 0.0);

    Body *b = proj.model().newBody();
    b->SetName("OreBody");
    b->SetDensity(3200.0);
    b->SetSusceptibility(0.08);

    for (int r = 1; r <= 2; ++r) {
        for (int c = 1; c <= 2; ++c) {
            proj.model().insertBody(r, c, -800.0, 200.0, false, b->GetID());
        }
    }
    proj.model().initFacetList();
    return proj;
}

} // namespace

TEST(MeshExportersTest, ExportWavefrontOBJ) {
    namespace fs = std::filesystem;
    const std::string objPath = "test_export.obj";

    Project proj = createSampleProjectWithBody();

    EXPECT_TRUE(proj.exportObj(objPath));
    ASSERT_TRUE(fs::exists(objPath));
    EXPECT_GT(fs::file_size(objPath), 100u);

    // Read and verify OBJ syntax
    std::ifstream ifs(objPath);
    std::string line;
    bool hasVertex = false;
    bool hasNormal = false;
    bool hasFace = false;
    bool hasObject = false;

    while (std::getline(ifs, line)) {
        if (line.rfind("v ", 0) == 0) hasVertex = true;
        if (line.rfind("vn ", 0) == 0) hasNormal = true;
        if (line.rfind("f ", 0) == 0) hasFace = true;
        if (line.rfind("o Body_", 0) == 0) hasObject = true;
    }

    EXPECT_TRUE(hasVertex);
    EXPECT_TRUE(hasNormal);
    EXPECT_TRUE(hasFace);
    EXPECT_TRUE(hasObject);

    fs::remove(objPath);
}

TEST(MeshExportersTest, ExportStlAsciiAndBinary) {
    namespace fs = std::filesystem;
    const std::string stlAsciiPath = "test_export_ascii.stl";
    const std::string stlBinaryPath = "test_export_binary.stl";

    Project proj = createSampleProjectWithBody();

    // 1. ASCII STL
    EXPECT_TRUE(proj.exportStl(stlAsciiPath, -1, false));
    ASSERT_TRUE(fs::exists(stlAsciiPath));

    std::ifstream ifsAscii(stlAsciiPath);
    std::string line;
    bool hasSolid = false;
    bool hasFacet = false;
    bool hasEndSolid = false;
    while (std::getline(ifsAscii, line)) {
        if (line.rfind("solid", 0) == 0) hasSolid = true;
        if (line.find("facet normal") != std::string::npos) hasFacet = true;
        if (line.rfind("endsolid", 0) == 0) hasEndSolid = true;
    }
    EXPECT_TRUE(hasSolid);
    EXPECT_TRUE(hasFacet);
    EXPECT_TRUE(hasEndSolid);
    fs::remove(stlAsciiPath);

    // 2. Binary STL
    EXPECT_TRUE(proj.exportStl(stlBinaryPath, -1, true));
    ASSERT_TRUE(fs::exists(stlBinaryPath));

    size_t binSize = fs::file_size(stlBinaryPath);
    ASSERT_GE(binSize, 84u);

    std::ifstream ifsBin(stlBinaryPath, std::ios::binary);
    char hdr[80];
    ifsBin.read(hdr, 80);
    uint32_t numTriangles = 0;
    ifsBin.read(reinterpret_cast<char*>(&numTriangles), 4);
    EXPECT_GT(numTriangles, 0u);

    // Standard binary STL size = 84 + 50 * N
    EXPECT_EQ(binSize, 84u + 50u * numTriangles);

    fs::remove(stlBinaryPath);
}

TEST(MeshExportersTest, ExportParaViewVTK) {
    namespace fs = std::filesystem;
    const std::string vtkPath = "test_export.vtk";

    Project proj = createSampleProjectWithBody();

    EXPECT_TRUE(proj.exportVtk(vtkPath));
    ASSERT_TRUE(fs::exists(vtkPath));

    std::ifstream ifs(vtkPath);
    std::string line;
    bool hasPolydata = false;
    bool hasPoints = false;
    bool hasPolygons = false;
    bool hasCellData = false;
    bool hasDensity = false;
    bool hasSusceptibility = false;
    bool hasBodyID = false;

    while (std::getline(ifs, line)) {
        if (line.find("DATASET POLYDATA") != std::string::npos) hasPolydata = true;
        if (line.rfind("POINTS ", 0) == 0) hasPoints = true;
        if (line.rfind("POLYGONS ", 0) == 0) hasPolygons = true;
        if (line.rfind("CELL_DATA ", 0) == 0) hasCellData = true;
        if (line.find("SCALARS Density") != std::string::npos) hasDensity = true;
        if (line.find("SCALARS Susceptibility") != std::string::npos) hasSusceptibility = true;
        if (line.find("SCALARS BodyID") != std::string::npos) hasBodyID = true;
    }

    EXPECT_TRUE(hasPolydata);
    EXPECT_TRUE(hasPoints);
    EXPECT_TRUE(hasPolygons);
    EXPECT_TRUE(hasCellData);
    EXPECT_TRUE(hasDensity);
    EXPECT_TRUE(hasSusceptibility);
    EXPECT_TRUE(hasBodyID);

    fs::remove(vtkPath);
}
