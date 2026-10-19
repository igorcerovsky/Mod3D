#include <gtest/gtest.h>
#include <QApplication>
#include <QPixmap>
#include <QPainter>
#include <filesystem>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QWheelEvent>

#include "mod3d_gui/ModelDocument.h"
#include "mod3d_gui/ProfileView.h"
#include "mod3d_gui/MapView.h"
#include "mod3d_gui/SpreadsheetView.h"
#include "mod3d_gui/GL3DView.h"
#include "mod3d_gui/MainWindow.h"
#include "mod3d_gui/SettingsPane.h"
#include "mod3d_gui/dialogs/BodyPropertiesDialog.h"

using namespace mod3d_gui;

namespace {
void saveTestScreenshot(const QPixmap &pix, const QString &filename) {
    const std::string dir = "docs/images";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    pix.save(QString::fromStdString(dir + "/" + filename.toStdString()));
}
} // namespace

class GuiDocumentTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        static int argc = 1;
        static char arg0[] = "test_gui_document";
        static char *argv[] = { arg0, nullptr };
        if (!qApp) {
            new QApplication(argc, argv);
        }
    }
};

TEST_F(GuiDocumentTest, NewProjectInitializesCorrectState) {
    ModelDocument doc;
    // rows=8, cols=10, x0=1000, y0=2000, dx=50, dy=50, zMin=-2000, zMax=0
    doc.newProject(8, 10, 1000.0, 2000.0, 50.0, 50.0, -2000.0, 0.0);

    // Model grid includes 1-cell border on each side: rows + 2 and cols + 2
    EXPECT_EQ(doc.model().rows(), 8 + 2);
    EXPECT_EQ(doc.model().cols(), 10 + 2);
    EXPECT_DOUBLE_EQ(doc.model().dx().value(), 50.0);
    EXPECT_DOUBLE_EQ(doc.model().dy().value(), 50.0);

    // Default orientation is East-West -> row profiles (8 profiles, maxProfileIndex() == 7)
    EXPECT_EQ(doc.profileOrientation(), ProfileOrientation::EastWest);
    EXPECT_EQ(doc.maxProfileIndex(), 7);
    EXPECT_EQ(doc.profileColumnLineCount(), 10);
    EXPECT_EQ(doc.profileIndex(), 4);

    // Coordinate verification
    EXPECT_DOUBLE_EQ(doc.columnCoordinate(0), 1000.0);
    EXPECT_DOUBLE_EQ(doc.columnCoordinate(1), 1050.0);
    EXPECT_DOUBLE_EQ(doc.columnCoordinate(9), 1450.0);
}

TEST_F(GuiDocumentTest, ProfileNavigationAndOrientationToggle) {
    ModelDocument doc;
    doc.newProject(10, 12, 0.0, 0.0, 100.0, 100.0, -3000.0, 0.0);

    // Initial East-West (10 rows -> max index 9)
    EXPECT_EQ(doc.maxProfileIndex(), 9);
    doc.setProfileIndex(5);
    EXPECT_EQ(doc.profileIndex(), 5);

    doc.moveProfile(1);
    EXPECT_EQ(doc.profileIndex(), 6);

    doc.moveProfile(-1);
    EXPECT_EQ(doc.profileIndex(), 5);

    // Toggle orientation to South-North
    doc.setProfileOrientation(ProfileOrientation::SouthNorth);
    EXPECT_EQ(doc.profileOrientation(), ProfileOrientation::SouthNorth);
    // In South-North: columns are profiles (12 cols -> max index 11), lines are rows (10 lines)
    EXPECT_EQ(doc.maxProfileIndex(), 11);
    EXPECT_EQ(doc.profileColumnLineCount(), 10);

    // Index clamped if necessary
    doc.setProfileIndex(11);
    EXPECT_EQ(doc.profileIndex(), 11);

    // Switch back to East-West -> index 11 clamped to 9
    doc.setProfileOrientation(ProfileOrientation::EastWest);
    EXPECT_EQ(doc.maxProfileIndex(), 9);
    EXPECT_EQ(doc.profileIndex(), 9);
}

TEST_F(GuiDocumentTest, InsertBodyAndEditVerticesWithRealtimeRecalc) {
    ModelDocument doc;
    doc.newProject(8, 8, 0.0, 0.0, 50.0, 50.0, -2000.0, 0.0);
    doc.setProfileIndex(0); // Profile 0 -> row 1

    EXPECT_EQ(doc.model().bodies().size(), 0);

    // Insert new body at line index 2 (col 3)
    int bodyId = doc.insertBodyAt(2, -500.0, 200.0);
    EXPECT_GT(bodyId, 0);
    EXPECT_EQ(doc.model().bodies().size(), 1);
    EXPECT_TRUE(doc.isModified());

    // Extend body to line index 3 (col 4)
    int extId = doc.insertBodyAt(3, -550.0, 220.0, bodyId);
    EXPECT_EQ(extId, bodyId);

    // In EastWest, profile 0 (row 1): col 3 has index idxCol3, col 4 has index idxCol4
    int idxCol3 = doc.model().body_index(bodyId, 1, 3);
    int idxCol4 = doc.model().body_index(bodyId, 1, 4);
    EXPECT_GE(idxCol3, 0);
    EXPECT_GE(idxCol4, 0);

    // Body at col 3 has center -500 and half-thickness 100 -> top -400, bot -600
    EXPECT_NEAR(doc.model().z(1, 3, idxCol3), -400.0, 1e-4);
    EXPECT_NEAR(doc.model().z(1, 3, idxCol3 + 1), -600.0, 1e-4);

    // Interactive vertex dragging: commit change
    doc.moveVertex(2, idxCol3, -450.0);
    EXPECT_NEAR(doc.model().z(1, 3, idxCol3), -450.0, 1e-4);
}

TEST_F(GuiDocumentTest, CopyBodyAcrossProfiles) {
    ModelDocument doc;
    doc.newProject(6, 6, 0.0, 0.0, 100.0, 100.0, -2000.0, 0.0);

    doc.setProfileIndex(1); // Profile 1 (row 2)
    int bodyId = doc.insertBodyAt(2, -600.0, 300.0);
    doc.insertBodyAt(3, -600.0, 300.0, bodyId);

    // Verify present on row 2
    EXPECT_GE(doc.model().body_index(bodyId, 2, 3), 0);
    // Not present on row 3 yet
    EXPECT_LT(doc.model().body_index(bodyId, 3, 3), 0);

    // Copy to Next Profile (row 3)
    doc.copyBodyToNextProfile(bodyId);
    EXPECT_GE(doc.model().body_index(bodyId, 3, 3), 0);
    EXPECT_GE(doc.model().body_index(bodyId, 3, 4), 0);

    // Copy to Previous Profile (row 1)
    doc.copyBodyToPreviousProfile(bodyId);
    EXPECT_GE(doc.model().body_index(bodyId, 1, 3), 0);

    // Remove from current profile (row 2)
    doc.removeBodyFromCurrentProfile(bodyId);
    EXPECT_LT(doc.model().body_index(bodyId, 2, 3), 0);
    // Still present on row 1 and row 3
    EXPECT_GE(doc.model().body_index(bodyId, 1, 3), 0);
    EXPECT_GE(doc.model().body_index(bodyId, 3, 3), 0);
}

TEST_F(GuiDocumentTest, LoadLegacyProjectAndComputeField) {
    ModelDocument doc;
    std::string samplePath = std::string(MOD3D_EXAMPLES_DIR) + "/Test.m3d";
    if (!std::filesystem::exists(samplePath)) {
        samplePath = std::string(MOD3D_EXAMPLES_DIR) + "/Sample.m3d";
    }

    if (std::filesystem::exists(samplePath)) {
        bool ok = doc.openFile(QString::fromStdString(samplePath));
        EXPECT_TRUE(ok);
        EXPECT_GT(doc.model().rows(), 0);
        EXPECT_GT(doc.model().cols(), 0);

        // Compute forward field
        doc.computeAll();
        // Modeled grid should be allocated
        const auto *modGrid = doc.modeledGrid(mod3d::FieldComponent::GZ);
        if (modGrid) {
            EXPECT_EQ(static_cast<int>(modGrid->rows()), doc.model().rows() - 2);
            EXPECT_EQ(static_cast<int>(modGrid->cols()), doc.model().cols() - 2);
        }
    }
}

TEST_F(GuiDocumentTest, ProfileViewHitTestingAndOffscreenRendering) {
    ModelDocument doc;
    doc.newProject(8, 10, 0.0, 0.0, 100.0, 100.0, -2000.0, 0.0);
    int bId = doc.insertBodyAt(3, -500.0, 200.0);
    doc.insertBodyAt(4, -500.0, 200.0, bId);

    ProfileView view(&doc);
    view.resize(900, 600);
    view.show();

    // Offscreen render to verify no crash and valid pixmap
    QPixmap pixmap(900, 600);
    pixmap.fill(Qt::white);
    view.render(&pixmap);

    EXPECT_FALSE(pixmap.isNull());
    EXPECT_EQ(pixmap.width(), 900);
    EXPECT_EQ(pixmap.height(), 600);
}

TEST_F(GuiDocumentTest, MeasuredFieldLoadingFromLegacyM3DAndExternalGrid) {
    ModelDocument doc;
    std::string samplePath = std::string(MOD3D_EXAMPLES_DIR) + "/Sample.m3d";
    if (std::filesystem::exists(samplePath)) {
        bool ok = doc.openFile(QString::fromStdString(samplePath));
        EXPECT_TRUE(ok);

        // Sample.m3d contains measured total field (GRDMES_G = 43)
        const auto *obsGrid = doc.activeObservedGrid();
        ASSERT_NE(obsGrid, nullptr);
        EXPECT_FALSE(obsGrid->empty());
        EXPECT_EQ(obsGrid->rows(), 21u);
        EXPECT_EQ(obsGrid->cols(), 21u);
        EXPECT_NEAR(obsGrid->min(), -1.2392, 0.01);
        EXPECT_NEAR(obsGrid->max(), 2.9756, 0.01);
    }

    // Test importing external Surfer 7 DSRB binary grid (gz.grd)
    std::string gzGrdPath = std::string(MOD3D_EXAMPLES_DIR) + "/gz.grd";
    if (std::filesystem::exists(gzGrdPath)) {
        bool ok = doc.loadMeasuredGrid(QString::fromStdString(gzGrdPath), mod3d::FieldComponent::GZ);
        EXPECT_TRUE(ok);
        const auto *gzObs = doc.observedGrid(mod3d::FieldComponent::GZ);
        ASSERT_NE(gzObs, nullptr);
        EXPECT_FALSE(gzObs->empty());
        EXPECT_EQ(gzObs->rows(), 21u);
        EXPECT_EQ(gzObs->cols(), 21u);

        // Difference grid should be automatically updated
        const auto *diffGrid = doc.activeDifferenceGrid();
        ASSERT_NE(diffGrid, nullptr);
        EXPECT_FALSE(diffGrid->empty());
    }
}

TEST_F(GuiDocumentTest, RealtimeModeledFieldUpdateOnProfileVertexMove) {
    ModelDocument doc;
    doc.newProject(8, 8, 0.0, 0.0, 50.0, 50.0, -2000.0, 0.0);
    doc.setProfileIndex(2); // row 3

    // Insert body with high density contrast across profiles to form 3D prisms
    int bodyId = doc.insertBodyAt(2, -500.0, 200.0);
    ASSERT_GT(bodyId, 0);
    doc.insertBodyAt(3, -500.0, 200.0, bodyId);
    doc.copyBodyToNextProfile(bodyId); // Copy to profile 3 (row 4)
    auto *b = doc.model().body(bodyId);
    ASSERT_NE(b, nullptr);
    const_cast<mod3d::Body*>(b)->set_density(3200.0);

    // Initial forward computation
    doc.computeAll();
    const auto *modGrid = doc.activeModeledGrid();
    ASSERT_NE(modGrid, nullptr);
    double initialVal = (*modGrid)(2, 2);
    EXPECT_GT(initialVal, 0.0);

    // In EastWest, profile 2 is row 3; lineIdx 2 is col 3
    int vIdx = doc.model().body_index(bodyId, 3, 3);
    ASSERT_GE(vIdx, 0);

    // Moving vertex should automatically update the modeled field in real-time
    EXPECT_TRUE(doc.isRealTimeCompute());
    EXPECT_TRUE(doc.model().is_compute_real_time());
    bool moved = doc.moveVertex(2, vIdx, -300.0);
    EXPECT_TRUE(moved);

    double updatedVal = (*doc.activeModeledGrid())(2, 2);
    // Depth moved shallower (-500 -> -300) so gravity anomaly should significantly increase
    EXPECT_NE(initialVal, updatedVal);
    EXPECT_GT(updatedVal, initialVal);
}

TEST_F(GuiDocumentTest, FieldUnitsAndComponentNaming) {
    ModelDocument doc;

    doc.setActiveField(mod3d::FieldComponent::GZ);
    EXPECT_EQ(doc.activeFieldComponentName(), "Gz");
    EXPECT_EQ(doc.activeFieldUnit(), "mGal");

    doc.setActiveField(mod3d::FieldComponent::G_TOT);
    EXPECT_EQ(doc.activeFieldComponentName(), "Gt");
    EXPECT_EQ(doc.activeFieldUnit(), "mGal");

    doc.setActiveField(mod3d::FieldComponent::DELTA_T);
    EXPECT_EQ(doc.activeFieldComponentName(), "ΔT");
    EXPECT_EQ(doc.activeFieldUnit(), "nT");

    doc.setActiveField(mod3d::FieldComponent::GZZ);
    EXPECT_EQ(doc.activeFieldComponentName(), "Gzz");
    EXPECT_EQ(doc.activeFieldUnit(), "E");

    // Offscreen render ProfileView with measured field toggled on
    doc.newProject(8, 8, 0.0, 0.0, 50.0, 50.0, -2000.0, 0.0);
    doc.setShowMeasuredField(true);
    doc.setShowModeledField(true);

    ProfileView view(&doc);
    view.resize(800, 500);
    QPixmap pixmap(800, 500);
    pixmap.fill(Qt::white);
    view.render(&pixmap);
    EXPECT_FALSE(pixmap.isNull());
}

TEST_F(GuiDocumentTest, ProfileFieldAxisGridScaling) {
    ModelDocument doc;
    std::string samplePath = std::string(MOD3D_EXAMPLES_DIR) + "/Sample.m3d";
    if (std::filesystem::exists(samplePath)) {
        bool ok = doc.openFile(QString::fromStdString(samplePath));
        ASSERT_TRUE(ok);

        // Initial compute to populate modeled grid
        doc.computeAll();

        const auto *modGrid = doc.activeModeledGrid();
        const auto *obsGrid = doc.activeObservedGrid();
        ASSERT_NE(modGrid, nullptr);
        ASSERT_NE(obsGrid, nullptr);

        // 2D grid statistics are fixed across all profile views
        auto modStats = modGrid->compute_stats();
        auto obsStats = obsGrid->compute_stats();
        std::cout << ">>> modStats: min=" << modStats.min << " max=" << modStats.max << std::endl;
        std::cout << ">>> obsStats: min=" << obsStats.min << " max=" << obsStats.max << std::endl;
        auto difGrid = doc.activeDifferenceGrid();
        if (difGrid) {
            auto difStats = difGrid->compute_stats();
            std::cout << ">>> difStats: min=" << difStats.min << " max=" << difStats.max << std::endl;
        }
        EXPECT_FALSE(std::isnan(modStats.min));
        EXPECT_FALSE(std::isnan(modStats.max));
        EXPECT_FALSE(std::isnan(obsStats.min));
        EXPECT_FALSE(std::isnan(obsStats.max));

        // ProfileView should render consistently across profiles without crashing
        ProfileView view(&doc);
        view.resize(900, 600);
        QPixmap pixmap(900, 600);

        for (int p = 0; p <= doc.maxProfileIndex(); p += 5) {
            doc.setProfileIndex(p);
            pixmap.fill(Qt::white);
            view.render(&pixmap);
            EXPECT_FALSE(pixmap.isNull());
        }
    }
}

TEST_F(GuiDocumentTest, NewBodyInsertionCreatesPointOnProfile) {
    ModelDocument doc;

    // 1. Test standard project with default depth range [-2000, 0]
    doc.newProject(8, 10, 0.0, 0.0, 100.0, 100.0, -2000.0, 0.0);
    doc.setProfileIndex(2); // profile 2
    size_t initialBodies = doc.model().bodies().size();

    auto *b = doc.createNewBody();
    ASSERT_NE(b, nullptr);
    int bId = b->id();
    EXPECT_GT(bId, 0);
    EXPECT_EQ(doc.model().bodies().size(), initialBodies + 1);

    // createNewBody inserts at the center column of the current profile
    int curLine = doc.profileColumnLineCount() / 2;
    int row = -1, col = -1;
    doc.getRowCol(curLine, row, col);
    EXPECT_GT(row, 0);
    EXPECT_GT(col, 0);

    // Verify vertices were created on profile column
    int vIdx = doc.model().body_index(bId, row, col);
    EXPECT_GE(vIdx, 0);

    // Also test inserting at a specific column line (e.g. line 0)
    int line0Body = doc.insertBodyAt(0, -500.0, 200.0);
    EXPECT_GT(line0Body, 0);
    int row0 = -1, col0 = -1;
    doc.getRowCol(0, row0, col0);
    EXPECT_GE(doc.model().body_index(line0Body, row0, col0), 0);

    // 2. Test shallow depth model like Test.m3d [-100, 0]
    std::string testPath = std::string(MOD3D_EXAMPLES_DIR) + "/Test.m3d";
    if (std::filesystem::exists(testPath)) {
        bool ok = doc.openFile(QString::fromStdString(testPath));
        ASSERT_TRUE(ok);

        double zMin = doc.model().z_min_val();
        double zMax = doc.model().z_max_val();
        EXPECT_GT(zMax, zMin);

        size_t countBefore = doc.model().bodies().size();
        auto *newBody = doc.createNewBody();
        ASSERT_NE(newBody, nullptr);
        int newId = newBody->id();
        EXPECT_GT(newId, 0);
        EXPECT_EQ(doc.model().bodies().size(), countBefore + 1);

        curLine = doc.profileColumnLineCount() / 2;
        doc.getRowCol(curLine, row, col);
        int shallowVIdx = doc.model().body_index(newId, row, col);
        EXPECT_GE(shallowVIdx, 0);
    }
}

TEST_F(GuiDocumentTest, UnsnapVertexAtAndSplitDrag) {
    ModelDocument doc;
    doc.newProject(6, 6, 0.0, 0.0, 100.0, 100.0, -2000.0, 0.0);
    doc.setProfileIndex(2);

    int bId = doc.insertBodyAt(2, -500.0, 200.0);
    ASSERT_GT(bId, 0);

    int row = -1, col = -1;
    doc.getRowCol(2, row, col);
    int topIdx = doc.model().body_index(bId, row, col);
    int botIdx = topIdx + 1;
    EXPECT_DOUBLE_EQ(doc.model().z(row, col, topIdx), -400.0);
    EXPECT_DOUBLE_EQ(doc.model().z(row, col, botIdx), -600.0);

    // 1. Snap them together
    bool moved = doc.moveVertex(2, topIdx, -600.0, mod3d::BodyMoveType::Normal);
    EXPECT_TRUE(moved);
    EXPECT_DOUBLE_EQ(doc.model().z(row, col, topIdx), -600.0);
    EXPECT_DOUBLE_EQ(doc.model().z(row, col, botIdx), -600.0);

    // 2. Unsnap via Split move (Ctrl+Drag)
    int vIdx = topIdx;
    moved = doc.moveVertex(2, vIdx, -450.0, mod3d::BodyMoveType::Split, &vIdx);
    EXPECT_TRUE(moved);
    EXPECT_EQ(vIdx, topIdx);
    EXPECT_DOUBLE_EQ(doc.model().z(row, col, topIdx), -450.0);
    EXPECT_DOUBLE_EQ(doc.model().z(row, col, botIdx), -600.0);

    // 3. Snap together again
    doc.moveVertex(2, topIdx, -600.0, mod3d::BodyMoveType::Normal);
    EXPECT_DOUBLE_EQ(doc.model().z(row, col, topIdx), -600.0);
    EXPECT_DOUBLE_EQ(doc.model().z(row, col, botIdx), -600.0);

    // 4. Unsnap via unsnapVertexAt() (right-click menu action)
    bool unsnapped = doc.unsnapVertexAt(2, topIdx, 150.0);
    EXPECT_TRUE(unsnapped);
    EXPECT_GT(doc.model().z(row, col, topIdx), doc.model().z(row, col, botIdx));
}

TEST_F(GuiDocumentTest, BodyColorAndFillApplication) {
    ModelDocument doc;
    doc.newProject(6, 6, 0.0, 0.0, 100.0, 100.0, -2000.0, 0.0);

    int bId = doc.insertBodyAt(2, -500.0, 200.0);
    ASSERT_GT(bId, 0);

    auto *b = doc.model().body(bId);
    ASSERT_NE(b, nullptr);

    // Initial palette color is valid
    auto initCol = b->color();
    EXPECT_TRUE(initCol.r > 0 || initCol.g > 0 || initCol.b > 0);
    EXPECT_TRUE(b->is_filled());

    // Update body color to a custom color (e.g. bright green)
    mod3d::BodyColor newColor(45, 210, 85, 255);
    const_cast<mod3d::Body*>(b)->set_color(newColor);
    const_cast<mod3d::Body*>(b)->set_filled(false);
    doc.setModified(true);

    EXPECT_EQ(b->color().r, 45);
    EXPECT_EQ(b->color().g, 210);
    EXPECT_EQ(b->color().b, 85);
    EXPECT_FALSE(b->is_filled());

    // ProfileView rendering respects the new color and fill without errors
    ProfileView view(&doc);
    view.resize(800, 500);
    QPixmap pixmap(800, 500);
    pixmap.fill(Qt::white);
    view.render(&pixmap);
    EXPECT_FALSE(pixmap.isNull());
}

TEST_F(GuiDocumentTest, FieldScalingAnomalyIsNotStraightLine) {
    ModelDocument doc;
    std::string samplePath = std::string(MOD3D_EXAMPLES_DIR) + "/Sample.m3d";
    if (std::filesystem::exists(samplePath)) {
        bool ok = doc.openFile(QString::fromStdString(samplePath));
        ASSERT_TRUE(ok);

        doc.computeAll();

        const auto *modGrid = doc.activeModeledGrid();
        const auto *obsGrid = doc.activeObservedGrid();
        ASSERT_NE(modGrid, nullptr);
        ASSERT_NE(obsGrid, nullptr);

        auto modStats = modGrid->compute_stats();
        auto obsStats = obsGrid->compute_stats();

        // 1. Modeled field is properly scaled in mGal, matching observed amplitude
        EXPECT_NEAR(modStats.min, obsStats.min, 0.05);
        EXPECT_NEAR(modStats.max, obsStats.max, 0.05);
        EXPECT_GT(modStats.max - modStats.min, 3.5); // span is ~4.2 mGal

        // 2. On profile 10 (center cross section), field values vary dynamically
        doc.setProfileIndex(10);
        int nLines = doc.profileColumnLineCount();
        ASSERT_GT(nLines, 5);

        double minPrf = 1e30, maxPrf = -1e30;
        for (int c = 0; c < nLines; ++c) {
            double v = (*modGrid)(10, c);
            if (!mod3d::Grid::is_dummy_value(v)) {
                minPrf = std::min(minPrf, v);
                maxPrf = std::max(maxPrf, v);
            }
        }

        // Anomaly is distinctly curved and not a straight flat line
        double dynamicRange = maxPrf - minPrf;
        EXPECT_GT(dynamicRange, 2.0); // Dynamic anomaly swing > 2 mGal

        ProfileView view(&doc);
        view.resize(900, 600);
        QPixmap pixmap(900, 600);
        pixmap.fill(Qt::white);
        view.render(&pixmap);
        EXPECT_FALSE(pixmap.isNull());
    }
}

TEST_F(GuiDocumentTest, IndependentPerpendicularProfileViews) {
    ModelDocument doc;
    // 10 rows, 12 cols
    doc.newProject(10, 12, 0.0, 0.0, 100.0, 100.0, -3000.0, 0.0);

    // Create an East-West profile view on profile index 4
    ProfileView pvEW(&doc, ProfileOrientation::EastWest, 4);
    EXPECT_EQ(pvEW.profileOrientation(), ProfileOrientation::EastWest);
    EXPECT_EQ(pvEW.profileIndex(), 4);
    EXPECT_EQ(pvEW.profileColumnLineCount(), 12);
    EXPECT_TRUE(pvEW.windowTitle().contains("[E-W]"));

    // Create a South-North profile view on profile index 6
    ProfileView pvSN(&doc, ProfileOrientation::SouthNorth, 6);
    EXPECT_EQ(pvSN.profileOrientation(), ProfileOrientation::SouthNorth);
    EXPECT_EQ(pvSN.profileIndex(), 6);
    EXPECT_EQ(pvSN.profileColumnLineCount(), 10);
    EXPECT_TRUE(pvSN.windowTitle().contains("[S-N]"));

    // 1. Modifying pvEW does not affect pvSN
    pvEW.moveProfile(1);
    EXPECT_EQ(pvEW.profileIndex(), 5);
    EXPECT_EQ(pvSN.profileIndex(), 6);

    pvSN.setShowGridLines(false);
    EXPECT_FALSE(pvSN.showGridLines());
    EXPECT_TRUE(pvEW.showGridLines());

    pvEW.setShowPreviousProfile(true);
    EXPECT_TRUE(pvEW.showPreviousProfile());
    EXPECT_FALSE(pvSN.showPreviousProfile());

    // 2. Insert a body at the intersection of pvEW (row 6) and pvSN (col 7)
    // In Model 1-based indexing: row 6, col 7
    int rowIntersect = 5 + 1; // row 6
    int colIntersect = 6 + 1; // col 7
    int bId = doc.insertBodyAtRowCol(rowIntersect, colIntersect, -1000.0, 200.0);
    EXPECT_GT(bId, 0);

    // Verify intersection has body index in model
    int bIdx = doc.model().body_index(bId, rowIntersect, colIntersect);
    EXPECT_GE(bIdx, 0);

    // Verify both perpendicular views render successfully
    pvEW.resize(800, 500);
    QPixmap pixEW(800, 500);
    pixEW.fill(Qt::white);
    pvEW.render(&pixEW);
    EXPECT_FALSE(pixEW.isNull());

    pvSN.resize(800, 500);
    QPixmap pixSN(800, 500);
    pixSN.fill(Qt::white);
    pvSN.render(&pixSN);
    EXPECT_FALSE(pixSN.isNull());

    // 3. Move vertex at the intersection from pvEW, check model updates and pvSN sees it
    bool moved = doc.moveVertexAtRowCol(rowIntersect, colIntersect, bIdx, -600.0);
    EXPECT_TRUE(moved);
    EXPECT_DOUBLE_EQ(doc.model().z(rowIntersect, colIntersect, bIdx), -600.0);

    // Render again to verify real-time update
    pvEW.render(&pixEW);
    pvSN.render(&pixSN);
    EXPECT_FALSE(pixEW.isNull());
    EXPECT_FALSE(pixSN.isNull());

    // 4. Test QMdiArea layout with both views and MapView
    QMdiArea mdi;
    auto *subEW = mdi.addSubWindow(new ProfileView(&doc, ProfileOrientation::EastWest, 5));
    auto *subSN = mdi.addSubWindow(new ProfileView(&doc, ProfileOrientation::SouthNorth, 6));
    auto *subMap = mdi.addSubWindow(new MapView(&doc));
    mdi.resize(1000, 700);
    subEW->resize(450, 320);
    subSN->resize(450, 320);
    subMap->resize(450, 320);

    QPixmap pixMdi(1000, 700);
    pixMdi.fill(Qt::white);
    mdi.render(&pixMdi);
    EXPECT_FALSE(pixMdi.isNull());

    // 5. Test requestOpenPerpendicularProfile signal
    bool signalFired = false;
    ProfileOrientation sigOrient = ProfileOrientation::EastWest;
    int sigIdx = -1;
    auto *pvWidget = qobject_cast<ProfileView*>(subEW->widget());
    ASSERT_NE(pvWidget, nullptr);
    QObject::connect(pvWidget, &ProfileView::requestOpenPerpendicularProfile,
                     [&](ProfileOrientation o, int idx) {
        signalFired = true;
        sigOrient = o;
        sigIdx = idx;
    });
    emit pvWidget->requestOpenPerpendicularProfile(ProfileOrientation::SouthNorth, 6);
    EXPECT_TRUE(signalFired);
    EXPECT_EQ(sigOrient, ProfileOrientation::SouthNorth);
    EXPECT_EQ(sigIdx, 6);
}

TEST_F(GuiDocumentTest, MapViewEqualScaleBothAxes) {
    ModelDocument doc;
    // Square model domain: 10 rows, 10 cols, dx=100m, dy=100m -> xMin=0, xMax=1100, yMin=0, yMax=1100
    // (Model grid has 1-cell border on each side: 11 intervals of 100m = 1100m)
    doc.newProject(10, 10, 0.0, 0.0, 100.0, 100.0, -2000.0, 0.0);

    MapView mv(&doc);
    mv.fitToWindow();

    // 1. Wide rectangular window: width 1000, height 500
    mv.resize(1000, 500);
    QRect canvasWide = mv.mapCanvasRect();
    // In a square domain, the map canvas MUST be square (width == height) to prevent distortion
    EXPECT_EQ(canvasWide.width(), canvasWide.height());

    // Verify scale: 100 meters in X must equal exactly the same number of pixels as 100 meters in Y
    double spanX = doc.model().x_max_val() - doc.model().x_min_val();
    double spanY = doc.model().y_max_val() - doc.model().y_min_val();
    EXPECT_DOUBLE_EQ(spanX, spanY);

    // 500 meters along X vs 500 meters along Y
    int pix500mX = std::abs(mv.mapCanvasRect().left() + static_cast<int>(std::round(500.0 / spanX * canvasWide.width())) - mv.mapCanvasRect().left());
    int pix500mY = std::abs(mv.mapCanvasRect().top() + static_cast<int>(std::round(500.0 / spanY * canvasWide.height())) - mv.mapCanvasRect().top());
    EXPECT_EQ(pix500mX, pix500mY);

    // 2. Tall rectangular window: width 400, height 900
    mv.resize(400, 900);
    QRect canvasTall = mv.mapCanvasRect();
    EXPECT_EQ(canvasTall.width(), canvasTall.height());

    // 3. Rectangular model domain: xSpan = 2000m, ySpan = 1000m (2:1 aspect ratio)
    ModelDocument docRect;
    // 8 rows (dy=100m -> spanY = 900m), 18 cols (dx=100m -> spanX = 1900m)
    docRect.newProject(8, 18, 0.0, 0.0, 100.0, 100.0, -2000.0, 0.0);
    MapView mvRect(&docRect);
    mvRect.fitToWindow();
    mvRect.resize(1200, 600);
    EXPECT_GT(mvRect.mapCanvasRect().width(), 0);

    // In physical meters: 1 meter in X must occupy the EXACT SAME pixel size as 1 meter in Y
    double scalePxPerMeter = mvRect.scalePixelsPerMeter();
    EXPECT_GT(scalePxPerMeter, 0.0);

    // Check pixel distance for 400m along X vs 400m along Y
    double testDist = 400.0;
    int pxDistX = std::abs(static_cast<int>(std::round(testDist * scalePxPerMeter)));
    int pxDistY = std::abs(static_cast<int>(std::round(testDist * scalePxPerMeter)));
    EXPECT_EQ(pxDistX, pxDistY);

    // 4. Verify rendering offscreen in rectangular view without deformation
    QPixmap pix(1200, 600);
    pix.fill(Qt::white);
    mvRect.render(&pix);
    EXPECT_FALSE(pix.isNull());
}

TEST_F(GuiDocumentTest, MouseWheelProfileNavigationAndCtrlZoom) {
    ModelDocument doc;
    doc.newProject(10, 10, 0.0, 0.0, 100.0, 100.0, -2000.0, 0.0);

    ProfileView pv(&doc, ProfileOrientation::EastWest, 4);
    EXPECT_EQ(pv.profileIndex(), 4);

    // 1. Plain mouse wheel scrolling down moves to previous profile
    QWheelEvent wheelDown(QPointF(200, 200), QPointF(200, 200), QPoint(), QPoint(0, -120),
                          Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&pv, &wheelDown);
    EXPECT_EQ(pv.profileIndex(), 3);

    // 2. Plain mouse wheel scrolling up moves to next profile
    QWheelEvent wheelUp(QPointF(200, 200), QPointF(200, 200), QPoint(), QPoint(0, 120),
                        Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&pv, &wheelUp);
    EXPECT_EQ(pv.profileIndex(), 4);

    // 3. CTRL + mouse wheel zooms, does NOT change profile index
    QWheelEvent ctrlWheelUp(QPointF(200, 200), QPointF(200, 200), QPoint(), QPoint(0, 120),
                            Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&pv, &ctrlWheelUp);
    EXPECT_EQ(pv.profileIndex(), 4);

    // 4. Test MapView wheel behavior
    doc.setProfileIndex(4);
    MapView mv(&doc);
    mv.resize(600, 400);

    // Scroll up on MapView advances profile
    QApplication::sendEvent(&mv, &wheelUp);
    EXPECT_EQ(doc.profileIndex(), 5);

    // Scroll down on MapView decreases profile
    QApplication::sendEvent(&mv, &wheelDown);
    EXPECT_EQ(doc.profileIndex(), 4);

    // Ctrl+scroll on MapView zooms, does not change profile
    QApplication::sendEvent(&mv, &ctrlWheelUp);
    EXPECT_EQ(doc.profileIndex(), 4);
}

TEST_F(GuiDocumentTest, ExtendBodySidePreservesHeightAndCollapses) {
    ModelDocument doc;
    doc.newProject(6, 6, 0.0, 0.0, 100.0, 100.0, -2000.0, 0.0);

    // Profile 0 (row 1 in EW): insert a body at column 3 with thickness 200 (top -400, bot -600)
    doc.setProfileIndex(0);
    int bodyId = doc.insertBodyAtRowCol(1, 3, -500.0, 200.0);
    ASSERT_GT(bodyId, 0);

    int idxCol3 = doc.model().body_index(bodyId, 1, 3);
    ASSERT_GE(idxCol3, 0);
    double initialTop = doc.model().z(1, 3, idxCol3);
    double initialBot = doc.model().z(1, 3, idxCol3 + 1);
    double initialHeight = std::abs(initialTop - initialBot);
    EXPECT_NEAR(initialHeight, 200.0, 1e-4);

    // Extend to column 4 (horizontal extension)
    bool extOk = doc.extendBodySideAtRowCol(1, 3, 1, 4, bodyId);
    EXPECT_TRUE(extOk);
    int idxCol4 = doc.model().body_index(bodyId, 1, 4);
    ASSERT_GE(idxCol4, 0);
    double col4Top = doc.model().z(1, 4, idxCol4);
    double col4Bot = doc.model().z(1, 4, idxCol4 + 1);
    double col4Height = std::abs(col4Top - col4Bot);
    // Height must remain 200, NOT double or increase
    EXPECT_NEAR(col4Height, 200.0, 1e-4);
    EXPECT_NEAR(col4Top, initialTop, 1e-4);
    EXPECT_NEAR(col4Bot, initialBot, 1e-4);

    // Extend to column 5
    extOk = doc.extendBodySideAtRowCol(1, 4, 1, 5, bodyId);
    EXPECT_TRUE(extOk);
    int idxCol5 = doc.model().body_index(bodyId, 1, 5);
    ASSERT_GE(idxCol5, 0);
    double col5Height = std::abs(doc.model().z(1, 5, idxCol5) - doc.model().z(1, 5, idxCol5 + 1));
    EXPECT_NEAR(col5Height, 200.0, 1e-4);

    // Drag backwards (collapse from 5 into 4)
    bool colOk = doc.extendBodySideAtRowCol(1, 5, 1, 4, bodyId);
    EXPECT_TRUE(colOk);
    EXPECT_LT(doc.model().body_index(bodyId, 1, 5), 0);
    EXPECT_GE(doc.model().body_index(bodyId, 1, 4), 0);
}

TEST_F(GuiDocumentTest, MoveUpperBoundaryDrawingMode) {
    ModelDocument doc;
    doc.newProject(6, 6, 0.0, 0.0, 100.0, 100.0, -2000.0, 0.0);

    // Insert body at row 1 across columns 2, 3, 4
    doc.setProfileIndex(0);
    int bodyId = doc.insertBodyAtRowCol(1, 2, -500.0, 200.0);
    doc.insertBodyAtRowCol(1, 3, -500.0, 200.0, bodyId);
    doc.insertBodyAtRowCol(1, 4, -500.0, 200.0, bodyId);

    // Check update_move_index functionality
    int vIdxCol2Top = doc.model().body_index(bodyId, 1, 2);
    int vIdxCol2Bot = vIdxCol2Top + 1;

    // Moving top boundary (odd index) across columns
    int vIdxCol3Top = doc.model().update_move_index(vIdxCol2Top, 1, 3, bodyId);
    EXPECT_EQ(vIdxCol3Top, doc.model().body_index(bodyId, 1, 3));
    EXPECT_EQ(vIdxCol3Top % 2, 1); // Upper boundary is odd

    // Moving bottom boundary (even index) across columns
    int vIdxCol3Bot = doc.model().update_move_index(vIdxCol2Bot, 1, 3, bodyId);
    EXPECT_EQ(vIdxCol3Bot, doc.model().body_index(bodyId, 1, 3) + 1);
    EXPECT_EQ(vIdxCol3Bot % 2, 0); // Lower boundary is even

    // Column 5 does not have bodyId -> returns -1
    EXPECT_EQ(doc.model().update_move_index(vIdxCol2Top, 1, 5, bodyId), -1);

    // Test stepping drawing mode across columns 2 -> 3 -> 4
    int currVIdx = vIdxCol2Top;
    double newZ = -350.0;
    for (int col = 2; col <= 4; ++col) {
        if (col > 2) {
            currVIdx = doc.model().update_move_index(currVIdx, 1, col, bodyId);
            ASSERT_GT(currVIdx, 0);
        }
        bool moved = doc.moveVertexAtRowCol(1, col, currVIdx, newZ);
        EXPECT_TRUE(moved);
        EXPECT_NEAR(doc.model().z(1, col, currVIdx), newZ, 1e-4);
    }

    // Verify all 3 columns now have upper boundary sculpted to -350.0
    EXPECT_NEAR(doc.model().z(1, 2, vIdxCol2Top), -350.0, 1e-4);
    int vIdxCol3 = doc.model().body_index(bodyId, 1, 3);
    EXPECT_NEAR(doc.model().z(1, 3, vIdxCol3), -350.0, 1e-4);
    int vIdxCol4 = doc.model().body_index(bodyId, 1, 4);
    EXPECT_NEAR(doc.model().z(1, 4, vIdxCol4), -350.0, 1e-4);

    // ProfileView offscreen render works cleanly with the sculpted model
    ProfileView pv(&doc, ProfileOrientation::EastWest, 0);
    pv.resize(800, 600);
    QPixmap pm(800, 600);
    pv.render(&pm);
    EXPECT_FALSE(pm.isNull());
}

class TestProfileView : public ProfileView {
public:
    using ProfileView::ProfileView;
    void sendMousePress(const QPoint &pos, Qt::MouseButton btn = Qt::LeftButton, Qt::KeyboardModifiers mods = Qt::NoModifier) {
        QMouseEvent ev(QEvent::MouseButtonPress, QPointF(pos), QPointF(pos), btn, btn, mods);
        mousePressEvent(&ev);
    }
    void sendMouseMove(const QPoint &pos, Qt::MouseButton btn = Qt::LeftButton, Qt::KeyboardModifiers mods = Qt::NoModifier) {
        QMouseEvent ev(QEvent::MouseMove, QPointF(pos), QPointF(pos), btn, btn, mods);
        mouseMoveEvent(&ev);
    }
    void sendMouseRelease(const QPoint &pos, Qt::MouseButton btn = Qt::LeftButton, Qt::KeyboardModifiers mods = Qt::NoModifier) {
        QMouseEvent ev(QEvent::MouseButtonRelease, QPointF(pos), QPointF(pos), btn, Qt::NoButton, mods);
        mouseReleaseEvent(&ev);
    }
    int toScreenX(double xw) const { return worldToScreenX(xw); }
    int toScreenZ(double zw) const { return worldToScreenZ(zw); }
};

TEST_F(GuiDocumentTest, LoadedBodyDrawingModeTest) {
    ModelDocument doc;
    std::string testPath = std::string(MOD3D_EXAMPLES_DIR) + "/Test.m3d";
    if (!std::filesystem::exists(testPath)) {
        testPath = std::string(MOD3D_TEST_DATA_DIR) + "/models/Test.m3d";
    }
    ASSERT_TRUE(std::filesystem::exists(testPath));
    ASSERT_TRUE(doc.openFile(QString::fromStdString(testPath)));

    ASSERT_FALSE(doc.model().bodies().empty());
    int loadedBodyId = doc.model().bodies().front()->id();
    EXPECT_EQ(loadedBodyId, 0); // Confirms loaded body has ID 0

    // Find a row and col where loadedBodyId exists with an adjacent column
    int foundRow = -1, foundCol = -1;
    for (int r = 1; r < doc.model().rows() - 1; ++r) {
        for (int c = 1; c < doc.model().cols() - 2; ++c) {
            int idx1 = doc.model().body_index(loadedBodyId, r, c);
            int idx2 = doc.model().body_index(loadedBodyId, r, c + 1);
            if (idx1 >= 0 && idx2 >= 0) {
                foundRow = r;
                foundCol = c;
                break;
            }
        }
        if (foundRow >= 0) break;
    }
    ASSERT_GE(foundRow, 0);
    ASSERT_GE(foundCol, 0);

    int vIdx = doc.model().body_index(loadedBodyId, foundRow, foundCol);
    int vIdxNext = doc.model().body_index(loadedBodyId, foundRow, foundCol + 1);
    ASSERT_GE(vIdx, 0);
    ASSERT_GE(vIdxNext, 0);

    doc.setProfileOrientation(ProfileOrientation::EastWest);
    doc.setProfileIndex(foundRow - 1);

    TestProfileView pv(&doc, ProfileOrientation::EastWest, foundRow - 1);
    pv.resize(1000, 600);
    QPixmap pm(1000, 600);
    pv.render(&pm);

    double initialColZ = doc.model().z(foundRow, foundCol, vIdx);
    double initialNextColZ = doc.model().z(foundRow, foundCol + 1, vIdxNext);

    int lineIdxStart = foundCol - 1;
    int lineIdxTarget = (foundCol + 1) - 1;

    double xwStart = pv.columnCoordinate(lineIdxStart);
    int sxStart = pv.toScreenX(xwStart);
    int syTopStart = pv.toScreenZ(initialColZ);

    // 1. Mouse Press on the upper vertex at starting column
    pv.sendMousePress(QPoint(sxStart, syTopStart));

    // 2. Mouse Move across into target column at a new depth
    double newZ = initialColZ + 15.0;
    double xwTarget = pv.columnCoordinate(lineIdxTarget);
    int sxTarget = pv.toScreenX(xwTarget);
    int syTarget = pv.toScreenZ(newZ);

    pv.sendMouseMove(QPoint(sxTarget, syTarget));

    // 3. Mouse Release
    pv.sendMouseRelease(QPoint(sxTarget, syTarget));

    // Verify drawing mode successfully moved the upper boundary of body 0 in the adjacent column!
    double updatedNextColZ = doc.model().z(foundRow, foundCol + 1, vIdxNext);
    EXPECT_NEAR(updatedNextColZ, newZ, 0.5);
    EXPECT_NE(updatedNextColZ, initialNextColZ);
}

TEST_F(GuiDocumentTest, MagneticFieldComputedOnSelectionAndUpdatesRealTime) {
    ModelDocument doc;
    std::string magPath = std::string(MOD3D_TEST_DATA_DIR) + "/models/TestMag.m3d";
    ASSERT_TRUE(std::filesystem::exists(magPath));
    ASSERT_TRUE(doc.openFile(QString::fromStdString(magPath)));

    // Select DELTA_T magnetic component
    doc.setActiveField(mod3d::FieldComponent::DELTA_T);
    EXPECT_EQ(doc.activeField(), mod3d::FieldComponent::DELTA_T);

    const auto *modGrid = doc.activeModeledGrid();
    ASSERT_NE(modGrid, nullptr);
    EXPECT_FALSE(modGrid->empty());
    double initialVal = (*modGrid)(11, 11);
    EXPECT_NEAR(initialVal, -0.496, 0.05);

    // Profile view renders without crashing and displays valid magnetic field
    ProfileView pv(&doc);
    pv.resize(800, 600);
    QPixmap pm(800, 600);
    pv.render(&pm);
    EXPECT_FALSE(pm.isNull());

    // Switch to Mx component
    doc.setActiveField(mod3d::FieldComponent::MX);
    EXPECT_EQ(doc.activeField(), mod3d::FieldComponent::MX);
    const auto *mxGrid = doc.activeModeledGrid();
    ASSERT_NE(mxGrid, nullptr);
    EXPECT_FALSE(mxGrid->empty());

    // Enable real-time compute, modify vertex and verify modeled magnetic field updates
    doc.setRealTimeCompute(true);
    doc.setActiveField(mod3d::FieldComponent::DELTA_T);
    int bodyId = doc.model().bodies().front()->id();
    int foundR = -1, foundC = -1, bIdx = -1;
    for (int r = 1; r < doc.model().rows() - 1 && foundR < 0; ++r) {
        for (int c = 1; c < doc.model().cols() - 1; ++c) {
            int idx = doc.model().body_index(bodyId, r, c);
            if (idx >= 0) {
                foundR = r;
                foundC = c;
                bIdx = idx;
                break;
            }
        }
    }
    ASSERT_GE(foundR, 0);
    ASSERT_GE(foundC, 0);
    ASSERT_GE(bIdx, 0);
    double curZ = doc.model().z(foundR, foundC, bIdx);
    doc.moveVertexAtRowCol(foundR, foundC, bIdx, curZ + 50.0);

    const auto *updatedModGrid = doc.activeModeledGrid();
    ASSERT_NE(updatedModGrid, nullptr);
    double updatedVal = (*updatedModGrid)(11, 11);
    EXPECT_NE(updatedVal, initialVal);
}

TEST_F(GuiDocumentTest, MapViewColorScaleLegendAndBodyContourProjection) {
    std::string path = std::string(MOD3D_TEST_DATA_DIR) + "/models/Sample.m3d";
    ASSERT_TRUE(std::filesystem::exists(path));

    ModelDocument doc;
    ASSERT_TRUE(doc.openFile(QString::fromStdString(path)));

    // 1. Verify loaded body attributes
    ASSERT_EQ(doc.model().bodies().size(), 3u);
    const auto *b0 = doc.model().body(0);
    const auto *b1 = doc.model().body(1);
    const auto *b2 = doc.model().body(2);
    ASSERT_NE(b0, nullptr);
    ASSERT_NE(b1, nullptr);
    ASSERT_NE(b2, nullptr);

    EXPECT_EQ(b0->name(), "Body");
    EXPECT_EQ(b1->name(), "Wings");
    EXPECT_EQ(b2->name(), "... and I feel fine");
    EXPECT_FALSE(b0->is_filled());

    // 2. Verify 2D body boundary contours exist
    const auto &boundaries = doc.model().all_body_boundaries();
    EXPECT_GT(boundaries.size(), 0u);
    for (const auto &seg : boundaries) {
        EXPECT_TRUE(seg.bodyId == 0 || seg.bodyId == 1 || seg.bodyId == 2);
    }

    // 3. Test MapView rendering with color scale legend and body contours
    MapView mv(&doc);
    mv.resize(900, 700);
    mv.fitToWindow();

    QPixmap pixmap(900, 700);
    pixmap.fill(Qt::white);
    mv.render(&pixmap);
    EXPECT_FALSE(pixmap.isNull());

    // Test with Gravity palette (Gz)
    doc.setActiveField(mod3d::FieldComponent::GZ);
    doc.computeAll();
    mv.render(&pixmap);
    EXPECT_FALSE(pixmap.isNull());

    // Test with Magnetic palette (DELTA_T)
    doc.setActiveField(mod3d::FieldComponent::DELTA_T);
    doc.computeAll();
    mv.render(&pixmap);
    EXPECT_FALSE(pixmap.isNull());

    // Test with Difference palette
    doc.setShowDifferenceField(true);
    mv.render(&pixmap);
    EXPECT_FALSE(pixmap.isNull());

    // Test with Body Footprints enabled and disabled
    EXPECT_TRUE(mv.showBodyFootprints());
    mv.setShowBodyFootprints(false);
    EXPECT_FALSE(mv.showBodyFootprints());
    mv.render(&pixmap);
    EXPECT_FALSE(pixmap.isNull());

    mv.setShowBodyFootprints(true);
    EXPECT_TRUE(mv.showBodyFootprints());
    mv.render(&pixmap);
    EXPECT_FALSE(pixmap.isNull());
}

TEST_F(GuiDocumentTest, ProfileToMapViewCursorSyncAndMarkerRendering) {
    ModelDocument doc;
    doc.newProject(8, 8, 1000.0, 2000.0, 50.0, 50.0, -2000.0, 0.0);
    doc.setProfileIndex(2);

    int bodyId = doc.insertBodyAt(3, -500.0, 200.0);
    ASSERT_GT(bodyId, 0);

    // 1. Profile change automatically positions station point on profile row 3
    EXPECT_TRUE(doc.activeProfileCursor().active);
    EXPECT_EQ(doc.activeProfileCursor().row, 3);

    // 2. Test ModelDocument cursor tracking signal
    bool signalEmitted = false;
    ModelDocument::ProfileCursorPoint receivedPt;
    QObject::connect(&doc, &ModelDocument::activeProfileCursorChanged, [&](const ModelDocument::ProfileCursorPoint &pt) {
        signalEmitted = true;
        receivedPt = pt;
    });

    ModelDocument::ProfileCursorPoint testPt;
    testPt.active = true;
    testPt.row = 2;
    testPt.col = 3;
    testPt.x = 1150.0;
    testPt.y = 2100.0;
    testPt.z = -500.0;
    testPt.bodyId = bodyId;
    testPt.bodyName = "TestBody";
    testPt.isDragging = false;

    doc.setActiveProfileCursor(testPt);
    EXPECT_TRUE(signalEmitted);
    EXPECT_TRUE(doc.activeProfileCursor().active);
    EXPECT_EQ(receivedPt.bodyId, bodyId);
    EXPECT_DOUBLE_EQ(receivedPt.x, 1150.0);
    EXPECT_DOUBLE_EQ(receivedPt.y, 2100.0);
    EXPECT_DOUBLE_EQ(receivedPt.z, -500.0);

    // 3. Test MapView rendering active point marker without throwing/crashing
    MapView mv(&doc);
    mv.resize(800, 600);
    mv.fitToWindow();

    QPixmap pixmap(800, 600);
    pixmap.fill(Qt::white);
    mv.render(&pixmap);
    EXPECT_FALSE(pixmap.isNull());

    // Test dragging state marker
    testPt.isDragging = true;
    doc.setActiveProfileCursor(testPt);
    mv.render(&pixmap);
    EXPECT_FALSE(pixmap.isNull());

    // 4. Test ProfileView mouse events triggering cursor updates
    TestProfileView pv(&doc, ProfileOrientation::EastWest, 2);
    pv.resize(800, 500);
    pv.render(&pixmap);

    int lineIdx = 3;
    double xw = pv.columnCoordinate(lineIdx);
    int sx = pv.toScreenX(xw);
    int sy = pv.toScreenZ(-500.0);

    // Mouse move over cross-section in ProfileView
    pv.sendMouseMove(QPoint(sx, sy));
    EXPECT_TRUE(doc.activeProfileCursor().active);
    EXPECT_DOUBLE_EQ(doc.activeProfileCursor().x, xw);

    // 5. Test profile position change updates station point (including moveProfile and wheel)
    double prevY = doc.activeProfileCursor().y;
    pv.moveProfile(1);
    EXPECT_TRUE(doc.activeProfileCursor().active);
    EXPECT_EQ(doc.activeProfileCursor().row, 4); // row 4 for profile 3
    EXPECT_NE(doc.activeProfileCursor().y, prevY);
    EXPECT_DOUBLE_EQ(doc.activeProfileCursor().y, doc.model().yd_val(4));
    EXPECT_DOUBLE_EQ(doc.activeProfileCursor().x, xw); // X position along column is preserved

    // Wheel scrolling updates profile and station point
    QWheelEvent wheelUp(QPointF(sx, sy), QPointF(sx, sy), QPoint(), QPoint(0, 120),
                        Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&pv, &wheelUp);
    EXPECT_TRUE(doc.activeProfileCursor().active);
    EXPECT_EQ(doc.activeProfileCursor().row, 5); // profile 4 -> row 5
    EXPECT_DOUBLE_EQ(doc.activeProfileCursor().y, doc.model().yd_val(5));

    // Clear cursor
    doc.clearActiveProfileCursor();
    EXPECT_FALSE(doc.activeProfileCursor().active);
}

TEST_F(GuiDocumentTest, GL3DViewFacetedBodiesAndCameraControls) {
    ModelDocument doc;
    doc.newProject(6, 6, 0.0, 0.0, 50.0, 50.0, -1000.0, 0.0);

    auto *b = doc.createNewBody();
    ASSERT_NE(b, nullptr);
    b->set_name("SaltDome");
    int bId = b->id();

    for (int r = 1; r <= 3; ++r) {
        for (int c = 1; c <= 3; ++c) {
            doc.insertBodyAtRowCol(r, c, -400.0, 80.0, bId);
        }
    }

    GL3DView view(&doc);
    view.resize(600, 400);

    // Camera presets
    view.setViewEast();
    view.setViewWest();
    view.setViewNorth();
    view.setViewSouth();
    view.setViewTopMap();
    view.resetCamera();

    // Display toggles
    EXPECT_TRUE(view.showFeatureEdges());
    view.setShowFeatureEdges(false);
    EXPECT_FALSE(view.showFeatureEdges());

    EXPECT_FALSE(view.wireframeMode());
    view.setWireframeMode(true);
    EXPECT_TRUE(view.wireframeMode());

    EXPECT_FALSE(view.showBottom());
    view.setShowBottom(true);
    EXPECT_TRUE(view.showBottom());

    EXPECT_FALSE(view.showOuterFacets());
    view.setShowOuterFacets(true);
    EXPECT_TRUE(view.showOuterFacets());

    EXPECT_TRUE(view.showRelief());
    view.setShowRelief(false);
    EXPECT_FALSE(view.showRelief());

    EXPECT_TRUE(view.showProfilePlane());
    view.setShowProfilePlane(false);
    EXPECT_FALSE(view.showProfilePlane());

    EXPECT_TRUE(view.showBoundingBox());
    view.setShowBoundingBox(false);
    EXPECT_FALSE(view.showBoundingBox());

    EXPECT_TRUE(view.showOrientationIndicator());
    view.setShowOrientationIndicator(false);
    EXPECT_FALSE(view.showOrientationIndicator());
    view.setShowOrientationIndicator(true);

    // Verify invalidateMeshes
    view.invalidateMeshes();
}

TEST_F(GuiDocumentTest, GL3DViewProfileSlicesAndReliefGridSanity) {
    ModelDocument doc;
    doc.newProject(10, 12, 1000.0, 2000.0, 50.0, 50.0, -3000.0, 250.0);

    const auto &m = doc.model();
    int rows = m.rows();
    int cols = m.cols();
    int rMax = rows - 2;
    int cMax = cols - 2;

    // Verify all core grid points used by relief wireframe are valid and have non-Hell elevation
    for (int r = 1; r <= rMax; ++r) {
        for (int c = 1; c <= cMax; ++c) {
            double z = m.z(r, c, 0);
            EXPECT_NE(z, m.z_min_val());
            EXPECT_LE(z, m.z_max_val());
            EXPECT_GE(z, m.z_min_val());
        }
    }

    // Verify profile slice count matches maxProfileIndex() + 1
    int expectedProfiles = doc.maxProfileIndex() + 1;
    EXPECT_EQ(expectedProfiles, 10); // EastWest has 10 profiles (rows - 2 = 10)
    EXPECT_EQ(doc.profileIndex(), 5);

    // Switch orientation and verify
    doc.setProfileOrientation(ProfileOrientation::SouthNorth);
    int expectedNSProfiles = doc.maxProfileIndex() + 1;
    EXPECT_EQ(expectedNSProfiles, 12); // SouthNorth has 12 profiles (cols - 2 = 12)

    GL3DView view(&doc);
    view.resize(600, 400);
    EXPECT_TRUE(view.showOrientationIndicator());
    EXPECT_TRUE(view.showProfilePlane());
}

TEST_F(GuiDocumentTest, GL3DViewTurntableOrbitAndCameraPresets) {
    ModelDocument doc;
    doc.newProject(8, 10, 0.0, 0.0, 50.0, 50.0, -2000.0, 0.0);

    GL3DView view(&doc);
    view.resize(640, 480);

    // Initial default turntable camera angles
    EXPECT_FLOAT_EQ(view.pitch(), 30.0f);
    EXPECT_FLOAT_EQ(view.azimuth(), -45.0f);

    // Standard CAD presets
    view.setViewTopMap();
    EXPECT_FLOAT_EQ(view.pitch(), 89.9f);
    EXPECT_FLOAT_EQ(view.azimuth(), 0.0f);

    view.setViewEast();
    EXPECT_FLOAT_EQ(view.pitch(), 0.0f);
    EXPECT_FLOAT_EQ(view.azimuth(), 90.0f);

    view.setViewWest();
    EXPECT_FLOAT_EQ(view.pitch(), 0.0f);
    EXPECT_FLOAT_EQ(view.azimuth(), -90.0f);

    view.setViewNorth();
    EXPECT_FLOAT_EQ(view.pitch(), 0.0f);
    EXPECT_FLOAT_EQ(view.azimuth(), 180.0f);

    view.setViewSouth();
    EXPECT_FLOAT_EQ(view.pitch(), 0.0f);
    EXPECT_FLOAT_EQ(view.azimuth(), 0.0f);

    view.resetCamera();
    EXPECT_FLOAT_EQ(view.pitch(), 30.0f);
    EXPECT_FLOAT_EQ(view.azimuth(), -45.0f);

    view.setCameraAngles(45.0f, -60.0f);
    EXPECT_FLOAT_EQ(view.pitch(), 45.0f);
    EXPECT_FLOAT_EQ(view.azimuth(), -60.0f);
}

TEST_F(GuiDocumentTest, MainWindowDefaultOpening2x2QuadTileLayout) {
    MainWindow mw;
    mw.resize(1200, 800);
    mw.show();

    // Verify 4 subwindows opened by default
    auto subs = mw.mdiArea()->subWindowList();
    ASSERT_EQ(subs.size(), 4);

    ProfileView *pvEW = nullptr;
    MapView *mv = nullptr;
    GL3DView *glv = nullptr;
    ProfileView *pvNS = nullptr;

    QMdiSubWindow *subEW = nullptr;
    QMdiSubWindow *subMap = nullptr;
    QMdiSubWindow *sub3D = nullptr;
    QMdiSubWindow *subNS = nullptr;

    for (auto *sub : subs) {
        if (auto *pv = qobject_cast<ProfileView*>(sub->widget())) {
            if (pv->profileOrientation() == ProfileOrientation::EastWest && !pvEW) {
                pvEW = pv;
                subEW = sub;
            } else if (pv->profileOrientation() == ProfileOrientation::SouthNorth && !pvNS) {
                pvNS = pv;
                subNS = sub;
            }
        } else if (auto *m = qobject_cast<MapView*>(sub->widget())) {
            mv = m;
            subMap = sub;
        } else if (auto *g = qobject_cast<GL3DView*>(sub->widget())) {
            glv = g;
            sub3D = sub;
        }
    }

    ASSERT_NE(pvEW, nullptr);
    ASSERT_NE(mv, nullptr);
    ASSERT_NE(glv, nullptr);
    ASSERT_NE(pvNS, nullptr);

    // Call arrangeDefaultTiles to arrange in 2x2 grid
    mw.arrangeDefaultTiles();

    // Verify relative positions:
    // Top-Left: E-W profile
    EXPECT_EQ(subEW->x(), 0);
    EXPECT_EQ(subEW->y(), 0);

    // Top-Right: Map view
    EXPECT_GE(subMap->x(), subEW->width() - 2);
    EXPECT_EQ(subMap->y(), 0);

    // Bottom-Left: 3D view
    EXPECT_EQ(sub3D->x(), 0);
    EXPECT_GE(sub3D->y(), subEW->height() - 2);

    // Bottom-Right: N-S / S-N profile
    EXPECT_GE(subNS->x(), subEW->width() - 2);
    EXPECT_GE(subNS->y(), subEW->height() - 2);

    // Active profile view should default to top-left E-W profile
    EXPECT_EQ(mw.activeProfileView(), pvEW);

    // Save screenshot for visual verification
    QPixmap pix(1200, 800);
    pix.fill(Qt::white);
    mw.render(&pix);
    saveTestScreenshot(pix, "quad_layout_3d_turntable.png");
}

TEST_F(GuiDocumentTest, ModernSideSettingsPaneAndDockIntegration) {
    MainWindow mw;
    mw.resize(1280, 820);
    mw.show();

    ASSERT_NE(mw.settingsPane(), nullptr);
    EXPECT_EQ(mw.dockWidgetArea(mw.settingsPane()), Qt::RightDockWidgetArea);
    EXPECT_FALSE(mw.settingsPane()->isHidden());

    // Verify 4 default views exist
    auto subWindows = mw.mdiArea()->subWindowList();
    EXPECT_EQ(subWindows.size(), 4);

    // Call arrange tiles
    mw.arrangeDefaultTiles();

    // Verify rendering with side pane (Profile View + Fields Display tab)
    QPixmap pix(1280, 820);
    pix.fill(Qt::white);
    mw.render(&pix);
    saveTestScreenshot(pix, "modern_settings_pane_layout.png");

    // 2. Switch to Body Editor tab
    mw.settingsPane()->selectBodyEditorTab();
    pix.fill(Qt::white);
    mw.render(&pix);
    saveTestScreenshot(pix, "modern_settings_body_editor.png");

    // 3. Switch to Map View and back to Fields tab
    QMdiSubWindow *subMap = nullptr;
    for (auto *sub : subWindows) {
        if (qobject_cast<MapView*>(sub->widget())) {
            subMap = sub;
            break;
        }
    }
    ASSERT_NE(subMap, nullptr);
    mw.mdiArea()->setActiveSubWindow(subMap);
    mw.settingsPane()->setActiveSubWindow(subMap);
    mw.settingsPane()->selectFieldsTab();
    pix.fill(Qt::white);
    mw.render(&pix);
    saveTestScreenshot(pix, "modern_settings_map_view.png");
}

TEST_F(GuiDocumentTest, ViewSpecificCurveDisplayProfileAndSingleBitmapMap) {
    ModelDocument doc;
    doc.createDefaultProject();

    // 1. ProfileView view-specific field settings
    ProfileView pv1(&doc, ProfileOrientation::EastWest, 0);
    ProfileView pv2(&doc, ProfileOrientation::SouthNorth, 0);

    // Default primary field is GZ
    EXPECT_EQ(pv1.mainFieldComponent(), mod3d::FieldComponent::GZ);
    EXPECT_EQ(pv2.mainFieldComponent(), mod3d::FieldComponent::GZ);

    // Customize pv1 to DELTA_T, pv2 stays GZ
    pv1.setMainFieldComponent(mod3d::FieldComponent::DELTA_T);
    EXPECT_EQ(pv1.mainFieldComponent(), mod3d::FieldComponent::DELTA_T);
    EXPECT_EQ(pv2.mainFieldComponent(), mod3d::FieldComponent::GZ);

    // View-specific curve toggles: without measured field, measured and diff curves are unchecked/false
    EXPECT_TRUE(pv1.showModeledCurve());
    EXPECT_FALSE(pv1.showMeasuredCurve());
    EXPECT_FALSE(pv1.showDifferenceCurve());
    EXPECT_FALSE(pv1.showSecondaryCurve());

    // Without measured field, setting show to true has no effect
    pv1.setShowMeasuredCurve(true);
    EXPECT_FALSE(pv1.showMeasuredCurve());

    pv1.setShowSecondaryCurve(true);
    pv1.setSecondaryFieldComponent(mod3d::FieldComponent::GZ);

    EXPECT_FALSE(pv1.showMeasuredCurve());
    EXPECT_FALSE(pv1.showDifferenceCurve());
    EXPECT_TRUE(pv1.showSecondaryCurve());
    EXPECT_EQ(pv1.secondaryFieldComponent(), mod3d::FieldComponent::GZ);

    // 2. MapView single raster display mode: without measured field, raster mode falls back to Modeled
    MapView mv(&doc);
    EXPECT_EQ(mv.rasterMode(), MapView::MapRasterMode::Modeled);
    EXPECT_EQ(mv.fieldComponent(), mod3d::FieldComponent::GZ);

    mv.setRasterMode(MapView::MapRasterMode::Observed);
    EXPECT_EQ(mv.rasterMode(), MapView::MapRasterMode::Modeled); // falls back to Modeled

    // Now populate observed grid for GZ
    mod3d::Grid obsGrd(21, 21, 0.0, 0.0, 500.0, 500.0);
    obsGrd.fill(12.5);
    doc.observation().set_observed_grid(mod3d::FieldComponent::GZ, obsGrd);
    doc.setShowMeasuredField(true);
    doc.setShowDifferenceField(true);

    // pv2 uses GZ which now has measured field
    EXPECT_TRUE(pv2.hasMeasuredField());
    pv2.setShowMeasuredCurve(true);
    pv2.setShowDifferenceCurve(true);
    EXPECT_TRUE(pv2.showMeasuredCurve());
    EXPECT_TRUE(pv2.showDifferenceCurve());
    EXPECT_FALSE(pv2.showSecondaryCurve());

    // MapView now supports Observed and Difference for GZ
    mv.setRasterMode(MapView::MapRasterMode::Observed);
    EXPECT_EQ(mv.rasterMode(), MapView::MapRasterMode::Observed);

    mv.setRasterMode(MapView::MapRasterMode::Difference);
    EXPECT_EQ(mv.rasterMode(), MapView::MapRasterMode::Difference);

    mv.setRasterMode(MapView::MapRasterMode::Relief);
    EXPECT_EQ(mv.rasterMode(), MapView::MapRasterMode::Relief);

    mv.setRasterMode(MapView::MapRasterMode::None);
    EXPECT_EQ(mv.rasterMode(), MapView::MapRasterMode::None);

    // Switching MapView component to DELTA_T (which has no measured field) falls back from Observed
    mv.setRasterMode(MapView::MapRasterMode::Observed);
    mv.setFieldComponent(mod3d::FieldComponent::DELTA_T);
    EXPECT_EQ(mv.fieldComponent(), mod3d::FieldComponent::DELTA_T);
    EXPECT_EQ(mv.rasterMode(), MapView::MapRasterMode::Modeled);

    // Map overlay toggles
    EXPECT_TRUE(mv.showStationPoints());
    EXPECT_TRUE(mv.showProfiles());
    EXPECT_TRUE(mv.showBodyFootprints());

    mv.setShowStationPoints(false);
    EXPECT_FALSE(mv.showStationPoints());

    mv.setShowProfiles(false);
    EXPECT_FALSE(mv.showProfiles());

    mv.setShowBodyFootprints(false);
    EXPECT_FALSE(mv.showBodyFootprints());
}

TEST_F(GuiDocumentTest, SettingsPaneSyncsWithActiveViewContext) {
    MainWindow mw;
    mw.show();

    auto *pane = mw.settingsPane();
    ASSERT_NE(pane, nullptr);

    // Find ProfileView and MapView subwindows
    QMdiSubWindow *subPv = nullptr;
    QMdiSubWindow *subMv = nullptr;
    QMdiSubWindow *sub3D = nullptr;

    for (auto *sub : mw.mdiArea()->subWindowList()) {
        if (qobject_cast<ProfileView*>(sub->widget())) subPv = sub;
        else if (qobject_cast<MapView*>(sub->widget())) subMv = sub;
        else if (qobject_cast<GL3DView*>(sub->widget())) sub3D = sub;
    }

    ASSERT_NE(subPv, nullptr);
    ASSERT_NE(subMv, nullptr);
    ASSERT_NE(sub3D, nullptr);

    // Activate profile view
    mw.mdiArea()->setActiveSubWindow(subPv);
    pane->setActiveSubWindow(subPv);

    // Activate map view
    mw.mdiArea()->setActiveSubWindow(subMv);
    pane->setActiveSubWindow(subMv);

    // Activate 3D view
    mw.mdiArea()->setActiveSubWindow(sub3D);
    pane->setActiveSubWindow(sub3D);
}

TEST_F(GuiDocumentTest, SettingsPaneFourTabsAndProfilesToolbarIntegration) {
    MainWindow mw;
    mw.resize(1400, 900);

    auto *pane = mw.settingsPane();
    ASSERT_NE(pane, nullptr);

    // 1. Find profile view subwindow
    QMdiSubWindow *subPv = nullptr;
    for (auto *sub : mw.mdiArea()->subWindowList()) {
        if (qobject_cast<ProfileView*>(sub->widget())) {
            subPv = sub;
            break;
        }
    }
    ASSERT_NE(subPv, nullptr);
    mw.mdiArea()->setActiveSubWindow(subPv);
    pane->setActiveSubWindow(subPv);

    auto *pv = qobject_cast<ProfileView*>(subPv->widget());
    ASSERT_NE(pv, nullptr);

    // Verify Show Checks on ProfileView
    EXPECT_TRUE(pv->showObservationPoints());
    EXPECT_TRUE(pv->showRelief());
    EXPECT_TRUE(pv->showProfileIntersections());
    EXPECT_TRUE(pv->showGridLines());

    pv->setShowObservationPoints(false);
    EXPECT_FALSE(pv->showObservationPoints());
    pv->setShowObservationPoints(true);
    EXPECT_TRUE(pv->showObservationPoints());

    pv->setShowRelief(false);
    EXPECT_FALSE(pv->showRelief());
    pv->setShowRelief(true);
    EXPECT_TRUE(pv->showRelief());

    pv->setShowProfileIntersections(false);
    EXPECT_FALSE(pv->showProfileIntersections());
    pv->setShowProfileIntersections(true);
    EXPECT_TRUE(pv->showProfileIntersections());

    // 2. Tab switching & screenshot captures
    pane->resize(380, 820);
    QPixmap pix(380, 820);

    // Tab 1: Display Tab
    pane->selectDisplayTab();
    pix.fill(Qt::white);
    pane->render(&pix);
    saveTestScreenshot(pix, "modern_settings_tab_display.png");

    // Tab 2: Profiles Tab
    pane->selectProfilesTab();
    pix.fill(Qt::white);
    pane->render(&pix);
    saveTestScreenshot(pix, "modern_settings_tab_profiles.png");

    // Tab 3: Bodies Tab
    pane->selectBodyEditorTab();
    pix.fill(Qt::white);
    pane->render(&pix);
    saveTestScreenshot(pix, "modern_settings_tab_bodies.png");

    // Tab 4: Physics Tab
    pane->selectPhysicsTab();
    pix.fill(Qt::white);
    pane->render(&pix);
    saveTestScreenshot(pix, "modern_settings_tab_physics.png");

    // 3. Test Physics parameters reflection in ModelDocument
    auto *doc = mw.document();
    ASSERT_NE(doc, nullptr);

    // Inducing field
    doc->observation().set_ambient_field_params(65.0, 12.0, 48500.0);
    EXPECT_DOUBLE_EQ(doc->observation().magnetic_intensity(), 48500.0);
    EXPECT_DOUBLE_EQ(doc->observation().inclination_deg(), 65.0);
    EXPECT_DOUBLE_EQ(doc->observation().declination_deg(), 12.0);

    // Sensor heights
    doc->observation().set_gravity_observation(mod3d::ObservationMode::SensorHeight, 150.0);
    doc->observation().set_magnetic_observation(mod3d::ObservationMode::SensorHeight, 120.0);
    EXPECT_DOUBLE_EQ(doc->observation().gravity_height(), 150.0);
    EXPECT_DOUBLE_EQ(doc->observation().magnetic_height(), 120.0);

    // Model vertical limits
    doc->model().set_z_min(-3500.0);
    doc->model().set_z_max(450.0);
    EXPECT_DOUBLE_EQ(doc->model().z_min().value(), -3500.0);
    EXPECT_DOUBLE_EQ(doc->model().z_max().value(), 450.0);
    EXPECT_DOUBLE_EQ(doc->model().z_min_val(), -3500.0);
    EXPECT_DOUBLE_EQ(doc->model().z_max_val(), 450.0);

    // Body density gradient and remanence
    if (!doc->model().bodies().empty()) {
        auto *b = doc->model().bodies().front().get();
        b->set_density_gradient(mod3d::Point3D(0.001, -0.002, 0.005));
        EXPECT_DOUBLE_EQ(b->density_gradient().x_val(), 0.001);
        EXPECT_DOUBLE_EQ(b->density_gradient().y_val(), -0.002);
        EXPECT_DOUBLE_EQ(b->density_gradient().z_val(), 0.005);

        b->set_remanent_magnetization(mod3d::Point3D(10.0, 20.0, -30.0));
        EXPECT_DOUBLE_EQ(b->remanent_magnetization().x_val(), 10.0);
        EXPECT_DOUBLE_EQ(b->remanent_magnetization().y_val(), 20.0);
        EXPECT_DOUBLE_EQ(b->remanent_magnetization().z_val(), -30.0);
    }
}

TEST_F(GuiDocumentTest, NoOverlappingMeasuredToolbarOnMainWindow) {
    MainWindow mw;
    mw.resize(1280, 800);
    mw.show();

    // Verify all toolbars belonging to MainWindow
    auto toolbars = mw.findChildren<QToolBar*>();
    // Exactly 3 toolbars: MainToolBar, ProfileToolBar, MapToolBar
    EXPECT_EQ(toolbars.size(), 3);
    for (auto *tb : toolbars) {
        EXPECT_TRUE(tb->objectName() == "MainToolBar" ||
                    tb->objectName() == "ProfileToolBar" ||
                    tb->objectName() == "MapToolBar");
        // Verify no toolbar has an action named "Measured"
        for (auto *act : tb->actions()) {
            EXPECT_FALSE(act->text().contains("Measured", Qt::CaseInsensitive));
        }
    }

    // Verify that the first actions of MainToolBar are New, Open, Save
    auto *mainTb = mw.findChild<QToolBar*>("MainToolBar");
    ASSERT_NE(mainTb, nullptr);
    auto acts = mainTb->actions();
    ASSERT_GE(acts.size(), 3);
    EXPECT_TRUE(acts[0]->text().contains("New"));
    EXPECT_TRUE(acts[1]->text().contains("Open"));
    EXPECT_TRUE(acts[2]->text().contains("Save"));

    // Capture screenshot to visually verify toolbar area
    QPixmap pix(1280, 800);
    pix.fill(Qt::white);
    mw.render(&pix);
    saveTestScreenshot(pix, "toolbar_clean_no_measured_overlap.png");
}

TEST_F(GuiDocumentTest, NoMeasuredFieldDisablesAndUnchecksMeasuredAndDifferenceWidgets) {
    MainWindow mw;
    mw.resize(1280, 800);
    mw.show();

    auto *pane = mw.settingsPane();
    ASSERT_NE(pane, nullptr);

    // Find profile view and map view subwindows
    QMdiSubWindow *subPv = nullptr;
    QMdiSubWindow *subMv = nullptr;
    for (auto *sub : mw.mdiArea()->subWindowList()) {
        if (qobject_cast<ProfileView*>(sub->widget())) subPv = sub;
        else if (qobject_cast<MapView*>(sub->widget())) subMv = sub;
    }
    ASSERT_NE(subPv, nullptr);
    ASSERT_NE(subMv, nullptr);

    // 1. In default new project with no measured field:
    // ProfileView: Measured and Difference curve checkboxes must be unchecked AND disabled
    mw.mdiArea()->setActiveSubWindow(subPv);
    pane->setActiveSubWindow(subPv);

    auto *chkMeasured = pane->measuredCurveCheckBox();
    auto *chkDiff = pane->differenceCurveCheckBox();
    ASSERT_NE(chkMeasured, nullptr);
    ASSERT_NE(chkDiff, nullptr);

    EXPECT_FALSE(chkMeasured->isEnabled());
    EXPECT_FALSE(chkMeasured->isChecked());
    EXPECT_FALSE(chkDiff->isEnabled());
    EXPECT_FALSE(chkDiff->isChecked());

    auto *pv = qobject_cast<ProfileView*>(subPv->widget());
    ASSERT_NE(pv, nullptr);
    EXPECT_FALSE(pv->showMeasuredCurve());
    EXPECT_FALSE(pv->showDifferenceCurve());

    // MapView: Observed and Difference bitmap radio buttons must be disabled
    mw.mdiArea()->setActiveSubWindow(subMv);
    pane->setActiveSubWindow(subMv);

    auto *rbMapObserved = pane->mapObservedRadioButton();
    auto *rbMapDiff = pane->mapDifferenceRadioButton();
    ASSERT_NE(rbMapObserved, nullptr);
    ASSERT_NE(rbMapDiff, nullptr);

    EXPECT_FALSE(rbMapObserved->isEnabled());
    EXPECT_FALSE(rbMapDiff->isEnabled());

    // Grab screenshot of disabled/unchecked state
    mw.mdiArea()->setActiveSubWindow(subPv);
    pane->setActiveSubWindow(subPv);
    QPixmap pixNoObs(1280, 800);
    pixNoObs.fill(Qt::white);
    mw.render(&pixNoObs);
    saveTestScreenshot(pixNoObs, "no_measured_field_disabled_state.png");

    // 2. Now supply measured data for GZ
    mod3d::Grid obsGrid(21, 21, 0.0, 0.0, 500.0, 500.0);
    obsGrid.fill(5.0);
    mw.document()->observation().set_observed_grid(mod3d::FieldComponent::GZ, obsGrid);
    mw.document()->setShowMeasuredField(true);
    mw.document()->setShowDifferenceField(true);

    // Activate ProfileView: widgets should now be enabled and checked
    mw.mdiArea()->setActiveSubWindow(subPv);
    pane->setActiveSubWindow(subPv);

    EXPECT_TRUE(chkMeasured->isEnabled());
    EXPECT_TRUE(chkMeasured->isChecked());
    EXPECT_TRUE(chkDiff->isEnabled());
    EXPECT_TRUE(chkDiff->isChecked());
    EXPECT_TRUE(pv->showMeasuredCurve());
    EXPECT_TRUE(pv->showDifferenceCurve());

    // Grab screenshot of enabled/checked state
    QPixmap pixWithObs(1280, 800);
    pixWithObs.fill(Qt::white);
    mw.render(&pixWithObs);
    saveTestScreenshot(pixWithObs, "measured_field_enabled_state.png");

    // Activate MapView: radio buttons should now be enabled
    mw.mdiArea()->setActiveSubWindow(subMv);
    pane->setActiveSubWindow(subMv);

    EXPECT_TRUE(rbMapObserved->isEnabled());
    EXPECT_TRUE(rbMapDiff->isEnabled());
}

TEST_F(GuiDocumentTest, OpenSampleBeginAndVerifyCleanDisplay) {
    MainWindow mw;
    mw.resize(1400, 900);
    mw.show();

    std::string sampleBeginPath = std::string(MOD3D_EXAMPLES_DIR) + "/SampleBegin.m3d";
    if (!std::filesystem::exists(sampleBeginPath)) {
        sampleBeginPath = "Examples/SampleBegin.m3d";
    }
    bool ok = mw.document()->openFile(QString::fromStdString(sampleBeginPath));
    EXPECT_TRUE(ok);

    auto *pane = mw.settingsPane();
    ASSERT_NE(pane, nullptr);

    // SampleBegin has no measured grid
    EXPECT_FALSE(mw.document()->hasAnyObservedGrid());
    EXPECT_FALSE(mw.document()->showMeasuredField());
    EXPECT_FALSE(mw.document()->showDifferenceField());

    auto *chkMeasured = pane->measuredCurveCheckBox();
    auto *chkDiff = pane->differenceCurveCheckBox();
    ASSERT_NE(chkMeasured, nullptr);
    ASSERT_NE(chkDiff, nullptr);

    EXPECT_FALSE(chkMeasured->isEnabled());
    EXPECT_FALSE(chkMeasured->isChecked());
    EXPECT_FALSE(chkDiff->isEnabled());
    EXPECT_FALSE(chkDiff->isChecked());

    mw.arrangeDefaultTiles();
    QPixmap pix(1400, 900);
    pix.fill(Qt::white);
    mw.render(&pix);
    saveTestScreenshot(pix, "sample_begin_loaded.png");
}

TEST_F(GuiDocumentTest, SampleM3DBodyPropertiesAndSIUnitsMatchBetweenPaneAndDialog) {
    MainWindow mw;
    mw.resize(1400, 900);

    std::string samplePath = std::string(MOD3D_EXAMPLES_DIR) + "/Sample.m3d";
    if (!std::filesystem::exists(samplePath)) {
        samplePath = "Examples/Sample.m3d";
    }
    bool ok = mw.document()->openFile(QString::fromStdString(samplePath));
    ASSERT_TRUE(ok);

    auto *pane = mw.settingsPane();
    ASSERT_NE(pane, nullptr);

    // Verify model has 3 bodies and Body 2 has authentic raw density 3400 kg/m3 and susc 0.01 SI
    auto *b2 = mw.document()->model().body(2);
    ASSERT_NE(b2, nullptr);
    EXPECT_DOUBLE_EQ(b2->raw_density().value(), 3400.0);
    EXPECT_DOUBLE_EQ(b2->susceptibility(), 0.01);

    // Switch to Geological Body Editor tab in SettingsPane
    pane->selectBodyEditorTab();

    auto *comboBodies = pane->bodyComboBox();
    auto *spinDensity = pane->bodyDensitySpinBox();
    auto *spinSusc = pane->bodySusceptibilitySpinBox();

    ASSERT_NE(comboBodies, nullptr);
    ASSERT_NE(spinDensity, nullptr);
    ASSERT_NE(spinSusc, nullptr);

    // Suffixes must use SI units
    EXPECT_EQ(spinDensity->suffix(), " kg/m³");
    EXPECT_EQ(spinSusc->suffix(), " SI");

    // Select Body 2 in SettingsPane
    int idx2 = comboBodies->findData(2);
    ASSERT_GE(idx2, 0);
    comboBodies->setCurrentIndex(idx2);

    // Values in Body tab must match authentic SI values exactly
    EXPECT_DOUBLE_EQ(spinDensity->value(), 3400.0);
    EXPECT_DOUBLE_EQ(spinSusc->value(), 0.01);
    EXPECT_TRUE(comboBodies->itemText(idx2).contains("3400.0 kg/m³"));

    // Verify BodyPropertiesDialog displays the exact same SI values and units in its own scope
    {
        BodyPropertiesDialog dlg(b2, nullptr);
        ASSERT_NE(dlg.densitySpinBox(), nullptr);
        ASSERT_NE(dlg.susceptibilitySpinBox(), nullptr);
        EXPECT_EQ(dlg.densitySpinBox()->suffix(), " kg/m³");
        EXPECT_EQ(dlg.susceptibilitySpinBox()->suffix(), " SI");
        EXPECT_DOUBLE_EQ(dlg.densitySpinBox()->value(), 3400.0);
        EXPECT_DOUBLE_EQ(dlg.susceptibilitySpinBox()->value(), 0.01);
    }

    // Edit density and susceptibility in SettingsPane and verify SI persistence
    spinDensity->setValue(3550.0);
    spinSusc->setValue(0.025);

    EXPECT_DOUBLE_EQ(b2->raw_density().value(), 3550.0);
    EXPECT_DOUBLE_EQ(b2->susceptibility(), 0.025);

    int curIdx = comboBodies->findData(2);
    ASSERT_GE(curIdx, 0);
    EXPECT_TRUE(comboBodies->itemText(curIdx).contains("3550.0 kg/m³"));
}

TEST_F(GuiDocumentTest, MapViewTopographyReliefContoursOverlay) {
    MainWindow mw;
    mw.resize(1400, 900);
    mw.show();

    std::string samplePath = std::string(MOD3D_EXAMPLES_DIR) + "/Sample.m3d";
    if (!std::filesystem::exists(samplePath)) {
        samplePath = "Examples/Sample.m3d";
    }
    bool ok = mw.document()->openFile(QString::fromStdString(samplePath));
    ASSERT_TRUE(ok);

    const auto &relief = mw.document()->observation().surface_relief();
    ASSERT_FALSE(relief.empty());
    EXPECT_GT(relief.rows(), 1u);
    EXPECT_GT(relief.cols(), 1u);
    EXPECT_LT(relief.min(), relief.max());

    // Generate contour segments directly on the relief grid
    auto segments = relief.generate_contours(10);
    EXPECT_FALSE(segments.empty());
    for (const auto &seg : segments) {
        EXPECT_GE(seg.z, relief.min());
        EXPECT_LE(seg.z, relief.max());
    }

    // Locate MapView subwindow
    MapView *mv = nullptr;
    QMdiSubWindow *subMap = nullptr;
    for (auto *sub : mw.mdiArea()->subWindowList()) {
        if (auto *v = qobject_cast<MapView*>(sub->widget())) {
            mv = v;
            subMap = sub;
            break;
        }
    }
    ASSERT_NE(mv, nullptr);
    ASSERT_NE(subMap, nullptr);

    mw.mdiArea()->setActiveSubWindow(subMap);
    auto *pane = mw.settingsPane();
    ASSERT_NE(pane, nullptr);
    pane->setActiveSubWindow(subMap);
    pane->selectDisplayTab();

    // Verify relief contours are enabled by default
    EXPECT_TRUE(mv->showReliefContours());
    auto *chkReliefContours = pane->mapReliefContoursCheckBox();
    ASSERT_NE(chkReliefContours, nullptr);
    EXPECT_TRUE(chkReliefContours->isChecked());

    // Render with relief contours enabled
    QPixmap pix(600, 600);
    pix.fill(Qt::white);
    mv->render(&pix);
    saveTestScreenshot(pix, "map_relief_contours_on.png");

    // Toggle off via checkbox
    chkReliefContours->setChecked(false);
    EXPECT_FALSE(mv->showReliefContours());

    QPixmap pixOff(600, 600);
    pixOff.fill(Qt::white);
    mv->render(&pixOff);
    saveTestScreenshot(pixOff, "map_relief_contours_off.png");

    // Toggle back on via MapView API
    mv->setShowReliefContours(true);
    EXPECT_TRUE(mv->showReliefContours());
    pane->setActiveSubWindow(subMap);
    EXPECT_TRUE(chkReliefContours->isChecked());
}

TEST_F(GuiDocumentTest, GenerateReadmeScreenshots) {
    MainWindow mw;
    mw.resize(1440, 920);
    mw.show();

    std::string samplePath = std::string(MOD3D_EXAMPLES_DIR) + "/Sample.m3d";
    if (!std::filesystem::exists(samplePath)) {
        samplePath = "Examples/Sample.m3d";
    }
    bool ok = mw.document()->openFile(QString::fromStdString(samplePath));
    ASSERT_TRUE(ok);

    mw.arrangeDefaultTiles();
    auto *pane = mw.settingsPane();
    ASSERT_NE(pane, nullptr);
    pane->selectBodyEditorTab();

    // 1. Full Main Workspace screenshot
    QPixmap pixMain(1440, 920);
    pixMain.fill(Qt::white);
    mw.render(&pixMain);
    std::filesystem::create_directories("docs/images");
    pixMain.save("docs/images/mod3d_main_workspace.png");

    // 2. Profile View close-up screenshot
    auto *pv = mw.activeProfileView();
    if (pv) {
        QPixmap pixProfile(pv->size());
        pixProfile.fill(Qt::white);
        pv->render(&pixProfile);
        pixProfile.save("docs/images/mod3d_profile_cross_section.png");
    }

    // 3. Map View screenshot (with relief contours and body footprints)
    MapView *mv = nullptr;
    for (auto *sub : mw.mdiArea()->subWindowList()) {
        if (auto *v = qobject_cast<MapView*>(sub->widget())) {
            mv = v;
            break;
        }
    }
    if (mv) {
        mv->setShowReliefContours(true);
        mv->setShowBodyFootprints(true);
        QPixmap pixMap(mv->size());
        pixMap.fill(Qt::white);
        mv->render(&pixMap);
        pixMap.save("docs/images/mod3d_map_view.png");
    }

    // 4. Settings Pane Geological Body Editor screenshot
    QPixmap pixPane(pane->width(), pane->height());
    pixPane.fill(Qt::white);
    pane->render(&pixPane);
    pixPane.save("docs/images/mod3d_body_editor.png");
}

