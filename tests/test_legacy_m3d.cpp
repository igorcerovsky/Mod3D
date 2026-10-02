#include <gtest/gtest.h>
#include "mod3d/Project.h"
#include <filesystem>
#include <fstream>
#include <cstring>
#include <cstdint>

using namespace mod3d;

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
