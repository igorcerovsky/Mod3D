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
    proj.metadata().created_at = "2026-10-02T10:00:00Z";
    proj.metadata().modified_at = "2026-10-02T11:00:00Z";
    proj.metadata().version = "2.4.2";

    proj.settings().compute_real_time = true;
    proj.settings().gravity_formula = 1;
    proj.settings().magnetic_formula = 2;
    proj.settings().active_components = {"gz", "delta_t", "gzz"};

    std::string jsonStr = proj.to_json_string();
    EXPECT_FALSE(jsonStr.empty());

    Project loaded;
    EXPECT_TRUE(loaded.from_json_string(jsonStr));

    EXPECT_EQ(loaded.metadata().title, "Geophysical Survey Block A");
    EXPECT_EQ(loaded.metadata().author, "Chief Geophysicist");
    EXPECT_EQ(loaded.metadata().description, "3D gravity and magnetic inversion over porphyry complex");
    EXPECT_EQ(loaded.metadata().created_at, "2026-10-02T10:00:00Z");
    EXPECT_EQ(loaded.metadata().modified_at, "2026-10-02T11:00:00Z");
    EXPECT_EQ(loaded.metadata().version, "2.4.2");

    EXPECT_TRUE(loaded.settings().compute_real_time);
    EXPECT_EQ(loaded.settings().gravity_formula, 1);
    EXPECT_EQ(loaded.settings().magnetic_formula, 2);
    ASSERT_EQ(loaded.settings().active_components.size(), 3u);
    EXPECT_EQ(loaded.settings().active_components[0], "gz");
    EXPECT_EQ(loaded.settings().active_components[1], "delta_t");
    EXPECT_EQ(loaded.settings().active_components[2], "gzz");
}

TEST(ProjectIOTest, FullModelAndObservationJsonRoundtrip) {
    namespace fs = std::filesystem;
    const fs::path tmpPath = "test_roundtrip.mod3d";

    // Build complete project
    Project proj;
    proj.metadata().title = "Porphyry Intrusion Model";
    proj.settings().compute_real_time = false;

    // 1. Setup Model (3x3 active columns -> 5x5 with extensions)
    proj.model().init(3, 3, 0.0, 0.0, 500.0, 500.0, -4000.0, 500.0);
    proj.model().set_extensions(2000.0, 2000.0, 2000.0, 2000.0, true);

    // Insert 2 geological bodies
    Body *b1 = proj.model().create_body();
    b1->set_name("Host Sediment");
    b1->set_density(2550.0);
    b1->set_susceptibility(0.001);
    BodyColor c1{120, 180, 80, 255};
    b1->set_color(c1);

    Body *b2 = proj.model().create_body();
    b2->set_name("Granite Stock");
    b2->set_density(2670.0);
    b2->set_susceptibility(0.045);
    b2->set_remanent_magnetization(Point3D(0.5, 0.2, -0.8));
    b2->set_density_gradient(Point3D(0.001, 0.0, -0.002));
    BodyColor c2{220, 50, 50, 255};
    b2->set_color(c2);

    // Add stratigraphic layer across 2x2 column cells
    for (int r = 1; r <= 2; ++r) {
        for (int c = 1; c <= 2; ++c) {
            proj.model().insert_body(r, c, -1500.0, 400.0, false, b2->id());
        }
    }
    proj.model().init_facet_list();

    std::vector<Facet3Pt> origFacets;
    proj.model().facets_computation(origFacets);
    ASSERT_GT(origFacets.size(), 0u);

    // 2. Setup Observation Space
    proj.observation().init_geometry(3, 3, 0.0, 0.0, 500.0, 500.0, 15.0);
    Grid relief(3, 3, 0.0, 0.0, 500.0, 500.0);
    relief(0, 0) = 100.0; relief(1, 1) = 250.0; relief(2, 2) = 150.0;
    proj.observation().set_surface_relief(relief);

    proj.observation().set_gravity_observation(ObservationMode::SensorHeight, 50.0);
    proj.observation().set_magnetic_observation(ObservationMode::FlightElevation, 400.0);
    proj.observation().set_tensor_observation(ObservationMode::SensorHeight, 30.0);
    proj.observation().set_ambient_field_params(62.5, 3.2, 49800.0);
    proj.observation().set_reference_density(2670.0);

    // Set observed grid
    Grid obsGz(3, 3, 0.0, 0.0, 500.0, 500.0);
    obsGz.fill(12.5);
    obsGz(1, 1) = 18.2;
    proj.observation().set_observed_grid(FieldComponent::GZ, obsGz);

    // Save project
    EXPECT_TRUE(proj.save_json(tmpPath));
    EXPECT_TRUE(fs::exists(tmpPath));

    // Reload project
    Project loaded;
    EXPECT_TRUE(loaded.load_json(tmpPath));

    // Cleanup temp file
    fs::remove(tmpPath);

    // Verify Model
    EXPECT_EQ(loaded.model().rows(), proj.model().rows());
    EXPECT_EQ(loaded.model().cols(), proj.model().cols());
    EXPECT_DOUBLE_EQ(loaded.model().x0().value(), 0.0);
    EXPECT_DOUBLE_EQ(loaded.model().y0().value(), 0.0);
    EXPECT_DOUBLE_EQ(loaded.model().dx().value(), 500.0);
    EXPECT_DOUBLE_EQ(loaded.model().dy().value(), 500.0);
    EXPECT_DOUBLE_EQ(loaded.model().z_min().value(), -4000.0);
    EXPECT_DOUBLE_EQ(loaded.model().z_max().value(), 500.0);

    EXPECT_TRUE(loaded.model().is_extend());
    EXPECT_DOUBLE_EQ(loaded.model().ex_n().value(), 2000.0);
    EXPECT_DOUBLE_EQ(loaded.model().ex_s().value(), 2000.0);
    EXPECT_DOUBLE_EQ(loaded.model().ex_e().value(), 2000.0);
    EXPECT_DOUBLE_EQ(loaded.model().ex_w().value(), 2000.0);

    // Verify Bodies
    ASSERT_EQ(loaded.model().bodies().size(), 2u);
    const Body *lb1 = loaded.model().body(b1->id());
    ASSERT_NE(lb1, nullptr);
    EXPECT_EQ(lb1->name(), "Host Sediment");
    EXPECT_DOUBLE_EQ(lb1->raw_density().value(), 2550.0);
    EXPECT_DOUBLE_EQ(lb1->susceptibility(), 0.001);
    EXPECT_EQ(lb1->color().r, 120);

    const Body *lb2 = loaded.model().body(b2->id());
    ASSERT_NE(lb2, nullptr);
    EXPECT_EQ(lb2->name(), "Granite Stock");
    EXPECT_DOUBLE_EQ(lb2->raw_density().value(), 2670.0);
    EXPECT_DOUBLE_EQ(lb2->susceptibility(), 0.045);
    EXPECT_DOUBLE_EQ(lb2->remanent_magnetization().mx.value(), 0.5);
    EXPECT_DOUBLE_EQ(lb2->density_gradient().x_val(), 0.001);
    EXPECT_EQ(lb2->color().r, 220);

    // Verify Facet Regeneration (Geometric Parity)
    std::vector<Facet3Pt> loadedFacets;
    loaded.model().facets_computation(loadedFacets);
    ASSERT_EQ(loadedFacets.size(), origFacets.size());
    for (size_t i = 0; i < origFacets.size(); ++i) {
        EXPECT_DOUBLE_EQ(loadedFacets[i][0].x, origFacets[i][0].x);
        EXPECT_DOUBLE_EQ(loadedFacets[i][0].y, origFacets[i][0].y);
        EXPECT_DOUBLE_EQ(loadedFacets[i][0].z, origFacets[i][0].z);
        EXPECT_DOUBLE_EQ(loadedFacets[i].normal().z, origFacets[i].normal().z);
    }

    // Verify Observation Space
    EXPECT_EQ(loaded.observation().rows(), 3u);
    EXPECT_EQ(loaded.observation().cols(), 3u);
    EXPECT_DOUBLE_EQ(loaded.observation().rotation().value(), 15.0);
    EXPECT_EQ(loaded.observation().gravity_mode(), ObservationMode::SensorHeight);
    EXPECT_DOUBLE_EQ(loaded.observation().gravity_height().value(), 50.0);
    EXPECT_EQ(loaded.observation().magnetic_mode(), ObservationMode::FlightElevation);
    EXPECT_DOUBLE_EQ(loaded.observation().magnetic_height().value(), 400.0);
    EXPECT_DOUBLE_EQ(loaded.observation().inclination().value(), 62.5);
    EXPECT_DOUBLE_EQ(loaded.observation().declination().value(), 3.2);
    EXPECT_DOUBLE_EQ(loaded.observation().magnetic_intensity().value(), 49800.0);
    EXPECT_DOUBLE_EQ(loaded.observation().reference_density().value(), 2670.0);

    // Verify Relief and Observed Grids
    EXPECT_DOUBLE_EQ(loaded.observation().surface_relief()(1, 1), 250.0);
    const Grid *loadedObsGz = loaded.observation().observed_grid(FieldComponent::GZ);
    ASSERT_NE(loadedObsGz, nullptr);
    EXPECT_DOUBLE_EQ((*loadedObsGz)(1, 1), 18.2);
    EXPECT_DOUBLE_EQ((*loadedObsGz)(0, 0), 12.5);
}
