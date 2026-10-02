#include <gtest/gtest.h>
#include "mod3d/Project.h"
#include <filesystem>
#include <fstream>

using namespace mod3d;

TEST(ProjectIOTest, MetadataAndSettingsRoundtrip) {
    Project proj;
    proj.metadata().title = "Geophysical Survey Block A";
    proj.metadata().author = "Chief Geophysicist";
    proj.metadata().description = "3D gravity and magnetic inversion over porphyry complex";
    proj.metadata().createdAt = "2026-10-02T10:00:00Z";
    proj.metadata().modifiedAt = "2026-10-02T11:00:00Z";
    proj.metadata().version = "2.1.0";

    proj.settings().computeRealTime = true;
    proj.settings().gravityFormula = 1;
    proj.settings().magneticFormula = 2;
    proj.settings().activeComponents = {"gz", "delta_t", "gzz"};

    std::string jsonStr = proj.toJsonString();
    EXPECT_FALSE(jsonStr.empty());

    Project loaded;
    EXPECT_TRUE(loaded.fromJsonString(jsonStr));

    EXPECT_EQ(loaded.metadata().title, "Geophysical Survey Block A");
    EXPECT_EQ(loaded.metadata().author, "Chief Geophysicist");
    EXPECT_EQ(loaded.metadata().description, "3D gravity and magnetic inversion over porphyry complex");
    EXPECT_EQ(loaded.metadata().createdAt, "2026-10-02T10:00:00Z");
    EXPECT_EQ(loaded.metadata().modifiedAt, "2026-10-02T11:00:00Z");
    EXPECT_EQ(loaded.metadata().version, "2.1.0");

    EXPECT_TRUE(loaded.settings().computeRealTime);
    EXPECT_EQ(loaded.settings().gravityFormula, 1);
    EXPECT_EQ(loaded.settings().magneticFormula, 2);
    ASSERT_EQ(loaded.settings().activeComponents.size(), 3u);
    EXPECT_EQ(loaded.settings().activeComponents[0], "gz");
    EXPECT_EQ(loaded.settings().activeComponents[1], "delta_t");
    EXPECT_EQ(loaded.settings().activeComponents[2], "gzz");
}

TEST(ProjectIOTest, FullModelAndObservationJsonRoundtrip) {
    namespace fs = std::filesystem;
    const std::string tmpPath = "test_roundtrip.mod3d";

    // Build complete project
    Project proj;
    proj.metadata().title = "Porphyry Intrusion Model";
    proj.settings().computeRealTime = false;

    // 1. Setup Model (3x3 active columns -> 5x5 with extensions)
    proj.model().init(3, 3, 0.0, 0.0, 500.0, 500.0, -4000.0, 500.0);
    proj.model().setExtensions(2000.0, 2000.0, 2000.0, 2000.0, true);

    // Insert 2 geological bodies
    Body *b1 = proj.model().newBody();
    b1->SetName("Host Sediment");
    b1->SetDensity(2550.0);
    b1->SetSusceptibility(0.001);
    BodyColor c1{120, 180, 80, 255};
    b1->SetColor(c1);

    Body *b2 = proj.model().newBody();
    b2->SetName("Granite Stock");
    b2->SetDensity(2670.0);
    b2->SetSusceptibility(0.045);
    b2->SetRemanentMagnetization(Point3D(0.5, 0.2, -0.8));
    b2->SetDensityGradient(Point3D(0.001, 0.0, -0.002));
    BodyColor c2{220, 50, 50, 255};
    b2->SetColor(c2);

    // Add stratigraphic layer across 2x2 column cells
    for (int r = 1; r <= 2; ++r) {
        for (int c = 1; c <= 2; ++c) {
            proj.model().insertBody(r, c, -1500.0, 400.0, false, b2->GetID());
        }
    }
    proj.model().initFacetList();

    std::vector<Facet3Pt> origFacets;
    proj.model().getFacetsComputation(origFacets);
    ASSERT_GT(origFacets.size(), 0u);

    // 2. Setup Observation Space
    proj.observation().initGeometry(3, 3, 0.0, 0.0, 500.0, 500.0, 15.0);
    Grid relief(3, 3, 0.0, 0.0, 500.0, 500.0);
    relief(0, 0) = 100.0; relief(1, 1) = 250.0; relief(2, 2) = 150.0;
    proj.observation().setSurfaceRelief(relief);

    proj.observation().setGravityObservation(ObservationMode::SensorHeight, 50.0);
    proj.observation().setMagneticObservation(ObservationMode::FlightElevation, 400.0);
    proj.observation().setTensorObservation(ObservationMode::SensorHeight, 30.0);
    proj.observation().setAmbientFieldParams(62.5, 3.2, 49800.0);
    proj.observation().setReferenceDensity(2670.0);

    // Set observed grid
    Grid obsGz(3, 3, 0.0, 0.0, 500.0, 500.0);
    obsGz.fill(12.5);
    obsGz(1, 1) = 18.2;
    proj.observation().setObservedGrid(FieldComponent::GZ, obsGz);

    // Save project
    EXPECT_TRUE(proj.saveJson(tmpPath));
    EXPECT_TRUE(fs::exists(tmpPath));

    // Reload project
    Project loaded;
    EXPECT_TRUE(loaded.loadJson(tmpPath));

    // Cleanup temp file
    fs::remove(tmpPath);

    // Verify Model
    EXPECT_EQ(loaded.model().getRows(), proj.model().getRows());
    EXPECT_EQ(loaded.model().getCols(), proj.model().getCols());
    EXPECT_DOUBLE_EQ(loaded.model().getX0(), 0.0);
    EXPECT_DOUBLE_EQ(loaded.model().getY0(), 0.0);
    EXPECT_DOUBLE_EQ(loaded.model().getXSize(), 500.0);
    EXPECT_DOUBLE_EQ(loaded.model().getYSize(), 500.0);
    EXPECT_DOUBLE_EQ(loaded.model().getZMin(), -4000.0);
    EXPECT_DOUBLE_EQ(loaded.model().getZMax(), 500.0);

    EXPECT_TRUE(loaded.model().isExtend());
    EXPECT_DOUBLE_EQ(loaded.model().getExN(), 2000.0);
    EXPECT_DOUBLE_EQ(loaded.model().getExS(), 2000.0);
    EXPECT_DOUBLE_EQ(loaded.model().getExE(), 2000.0);
    EXPECT_DOUBLE_EQ(loaded.model().getExW(), 2000.0);

    // Verify Bodies
    ASSERT_EQ(loaded.model().getBodies().size(), 2u);
    const Body *lb1 = loaded.model().getBody(b1->GetID());
    ASSERT_NE(lb1, nullptr);
    EXPECT_EQ(lb1->GetName(), "Host Sediment");
    EXPECT_DOUBLE_EQ(lb1->GetRawDensity(), 2550.0);
    EXPECT_DOUBLE_EQ(lb1->GetSusceptibility(), 0.001);
    EXPECT_EQ(lb1->GetColor().r, 120);

    const Body *lb2 = loaded.model().getBody(b2->GetID());
    ASSERT_NE(lb2, nullptr);
    EXPECT_EQ(lb2->GetName(), "Granite Stock");
    EXPECT_DOUBLE_EQ(lb2->GetRawDensity(), 2670.0);
    EXPECT_DOUBLE_EQ(lb2->GetSusceptibility(), 0.045);
    EXPECT_DOUBLE_EQ(lb2->GetRemanentMagnetization().x, 0.5);
    EXPECT_DOUBLE_EQ(lb2->GetDensityGradient().x, 0.001);
    EXPECT_EQ(lb2->GetColor().r, 220);

    // Verify Facet Regeneration (Geometric Parity)
    std::vector<Facet3Pt> loadedFacets;
    loaded.model().getFacetsComputation(loadedFacets);
    ASSERT_EQ(loadedFacets.size(), origFacets.size());
    for (size_t i = 0; i < origFacets.size(); ++i) {
        EXPECT_DOUBLE_EQ(loadedFacets[i].pts[0].x, origFacets[i].pts[0].x);
        EXPECT_DOUBLE_EQ(loadedFacets[i].pts[0].y, origFacets[i].pts[0].y);
        EXPECT_DOUBLE_EQ(loadedFacets[i].pts[0].z, origFacets[i].pts[0].z);
        EXPECT_DOUBLE_EQ(loadedFacets[i].Normal().z, origFacets[i].Normal().z);
    }

    // Verify Observation Space
    EXPECT_EQ(loaded.observation().getRows(), 3u);
    EXPECT_EQ(loaded.observation().getCols(), 3u);
    EXPECT_DOUBLE_EQ(loaded.observation().getRotDeg(), 15.0);
    EXPECT_EQ(loaded.observation().getGravityMode(), ObservationMode::SensorHeight);
    EXPECT_DOUBLE_EQ(loaded.observation().getGravityHeight(), 50.0);
    EXPECT_EQ(loaded.observation().getMagneticMode(), ObservationMode::FlightElevation);
    EXPECT_DOUBLE_EQ(loaded.observation().getMagneticHeight(), 400.0);
    EXPECT_DOUBLE_EQ(loaded.observation().getInclinationDeg(), 62.5);
    EXPECT_DOUBLE_EQ(loaded.observation().getDeclinationDeg(), 3.2);
    EXPECT_DOUBLE_EQ(loaded.observation().getMagneticIntensity(), 49800.0);
    EXPECT_DOUBLE_EQ(loaded.observation().getReferenceDensity(), 2670.0);

    // Verify Relief and Observed Grids
    EXPECT_DOUBLE_EQ(loaded.observation().getSurfaceRelief()(1, 1), 250.0);
    const Grid *loadedObsGz = loaded.observation().getObservedGrid(FieldComponent::GZ);
    ASSERT_NE(loadedObsGz, nullptr);
    EXPECT_DOUBLE_EQ((*loadedObsGz)(1, 1), 18.2);
    EXPECT_DOUBLE_EQ((*loadedObsGz)(0, 0), 12.5);
}
