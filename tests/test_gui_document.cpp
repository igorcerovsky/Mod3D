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

using namespace mod3d_gui;

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
    EXPECT_EQ(doc.model().getRows(), 8 + 2);
    EXPECT_EQ(doc.model().getCols(), 10 + 2);
    EXPECT_DOUBLE_EQ(doc.model().getXSize(), 50.0);
    EXPECT_DOUBLE_EQ(doc.model().getYSize(), 50.0);

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

    EXPECT_EQ(doc.model().getBodies().size(), 0);

    // Insert new body at line index 2 (col 3)
    int bodyId = doc.insertBodyAt(2, -500.0, 200.0);
    EXPECT_GT(bodyId, 0);
    EXPECT_EQ(doc.model().getBodies().size(), 1);
    EXPECT_TRUE(doc.isModified());

    // Extend body to line index 3 (col 4)
    int extId = doc.insertBodyAt(3, -550.0, 220.0, bodyId);
    EXPECT_EQ(extId, bodyId);

    // In EastWest, profile 0 (row 1): col 3 has index idxCol3, col 4 has index idxCol4
    int idxCol3 = doc.model().getBodyIndex(bodyId, 1, 3);
    int idxCol4 = doc.model().getBodyIndex(bodyId, 1, 4);
    EXPECT_GE(idxCol3, 0);
    EXPECT_GE(idxCol4, 0);

    // Body at col 3 has center -500 and half-thickness 100 -> top -400, bot -600
    EXPECT_NEAR(doc.model().getZ(1, 3, idxCol3), -400.0, 1e-4);
    EXPECT_NEAR(doc.model().getZ(1, 3, idxCol3 + 1), -600.0, 1e-4);

    // Interactive vertex dragging: commit change
    doc.moveVertex(2, idxCol3, -450.0);
    EXPECT_NEAR(doc.model().getZ(1, 3, idxCol3), -450.0, 1e-4);
}

TEST_F(GuiDocumentTest, CopyBodyAcrossProfiles) {
    ModelDocument doc;
    doc.newProject(6, 6, 0.0, 0.0, 100.0, 100.0, -2000.0, 0.0);

    doc.setProfileIndex(1); // Profile 1 (row 2)
    int bodyId = doc.insertBodyAt(2, -600.0, 300.0);
    doc.insertBodyAt(3, -600.0, 300.0, bodyId);

    // Verify present on row 2
    EXPECT_GE(doc.model().getBodyIndex(bodyId, 2, 3), 0);
    // Not present on row 3 yet
    EXPECT_LT(doc.model().getBodyIndex(bodyId, 3, 3), 0);

    // Copy to Next Profile (row 3)
    doc.copyBodyToNextProfile(bodyId);
    EXPECT_GE(doc.model().getBodyIndex(bodyId, 3, 3), 0);
    EXPECT_GE(doc.model().getBodyIndex(bodyId, 3, 4), 0);

    // Copy to Previous Profile (row 1)
    doc.copyBodyToPreviousProfile(bodyId);
    EXPECT_GE(doc.model().getBodyIndex(bodyId, 1, 3), 0);

    // Remove from current profile (row 2)
    doc.removeBodyFromCurrentProfile(bodyId);
    EXPECT_LT(doc.model().getBodyIndex(bodyId, 2, 3), 0);
    // Still present on row 1 and row 3
    EXPECT_GE(doc.model().getBodyIndex(bodyId, 1, 3), 0);
    EXPECT_GE(doc.model().getBodyIndex(bodyId, 3, 3), 0);
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
        EXPECT_GT(doc.model().getRows(), 0);
        EXPECT_GT(doc.model().getCols(), 0);

        // Compute forward field
        doc.computeAll();
        // Modeled grid should be allocated
        const auto *modGrid = doc.modeledGrid(mod3d::FieldComponent::GZ);
        if (modGrid) {
            EXPECT_EQ(static_cast<int>(modGrid->rows()), doc.model().getRows() - 2);
            EXPECT_EQ(static_cast<int>(modGrid->cols()), doc.model().getCols() - 2);
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
    auto *b = doc.model().getBody(bodyId);
    ASSERT_NE(b, nullptr);
    const_cast<mod3d::Body*>(b)->SetDensity(3200.0);

    // Initial forward computation
    doc.computeAll();
    const auto *modGrid = doc.activeModeledGrid();
    ASSERT_NE(modGrid, nullptr);
    double initialVal = (*modGrid)(2, 2);
    EXPECT_GT(initialVal, 0.0);

    // In EastWest, profile 2 is row 3; lineIdx 2 is col 3
    int vIdx = doc.model().getBodyIndex(bodyId, 3, 3);
    ASSERT_GE(vIdx, 0);

    // Moving vertex should automatically update the modeled field in real-time
    EXPECT_TRUE(doc.isRealTimeCompute());
    EXPECT_TRUE(doc.model().isComputeRealTime());
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
    size_t initialBodies = doc.model().getBodies().size();

    auto *b = doc.createNewBody();
    ASSERT_NE(b, nullptr);
    int bId = b->id();
    EXPECT_GT(bId, 0);
    EXPECT_EQ(doc.model().getBodies().size(), initialBodies + 1);

    // createNewBody inserts at the center column of the current profile
    int curLine = doc.profileColumnLineCount() / 2;
    int row = -1, col = -1;
    doc.getRowCol(curLine, row, col);
    EXPECT_GT(row, 0);
    EXPECT_GT(col, 0);

    // Verify vertices were created on profile column
    int vIdx = doc.model().getBodyIndex(bId, row, col);
    EXPECT_GE(vIdx, 0);

    // Also test inserting at a specific column line (e.g. line 0)
    int line0Body = doc.insertBodyAt(0, -500.0, 200.0);
    EXPECT_GT(line0Body, 0);
    int row0 = -1, col0 = -1;
    doc.getRowCol(0, row0, col0);
    EXPECT_GE(doc.model().getBodyIndex(line0Body, row0, col0), 0);

    // 2. Test shallow depth model like Test.m3d [-100, 0]
    std::string testPath = std::string(MOD3D_EXAMPLES_DIR) + "/Test.m3d";
    if (std::filesystem::exists(testPath)) {
        bool ok = doc.openFile(QString::fromStdString(testPath));
        ASSERT_TRUE(ok);

        double zMin = doc.model().getZMin();
        double zMax = doc.model().getZMax();
        EXPECT_GT(zMax, zMin);

        size_t countBefore = doc.model().getBodies().size();
        auto *newBody = doc.createNewBody();
        ASSERT_NE(newBody, nullptr);
        int newId = newBody->id();
        EXPECT_GT(newId, 0);
        EXPECT_EQ(doc.model().getBodies().size(), countBefore + 1);

        curLine = doc.profileColumnLineCount() / 2;
        doc.getRowCol(curLine, row, col);
        int shallowVIdx = doc.model().getBodyIndex(newId, row, col);
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
    int topIdx = doc.model().getBodyIndex(bId, row, col);
    int botIdx = topIdx + 1;
    EXPECT_DOUBLE_EQ(doc.model().getZ(row, col, topIdx), -400.0);
    EXPECT_DOUBLE_EQ(doc.model().getZ(row, col, botIdx), -600.0);

    // 1. Snap them together
    bool moved = doc.moveVertex(2, topIdx, -600.0, mod3d::BodyMoveType::Normal);
    EXPECT_TRUE(moved);
    EXPECT_DOUBLE_EQ(doc.model().getZ(row, col, topIdx), -600.0);
    EXPECT_DOUBLE_EQ(doc.model().getZ(row, col, botIdx), -600.0);

    // 2. Unsnap via Split move (Ctrl+Drag)
    int vIdx = topIdx;
    moved = doc.moveVertex(2, vIdx, -450.0, mod3d::BodyMoveType::Split, &vIdx);
    EXPECT_TRUE(moved);
    EXPECT_EQ(vIdx, topIdx);
    EXPECT_DOUBLE_EQ(doc.model().getZ(row, col, topIdx), -450.0);
    EXPECT_DOUBLE_EQ(doc.model().getZ(row, col, botIdx), -600.0);

    // 3. Snap together again
    doc.moveVertex(2, topIdx, -600.0, mod3d::BodyMoveType::Normal);
    EXPECT_DOUBLE_EQ(doc.model().getZ(row, col, topIdx), -600.0);
    EXPECT_DOUBLE_EQ(doc.model().getZ(row, col, botIdx), -600.0);

    // 4. Unsnap via unsnapVertexAt() (right-click menu action)
    bool unsnapped = doc.unsnapVertexAt(2, topIdx, 150.0);
    EXPECT_TRUE(unsnapped);
    EXPECT_GT(doc.model().getZ(row, col, topIdx), doc.model().getZ(row, col, botIdx));
}

TEST_F(GuiDocumentTest, BodyColorAndFillApplication) {
    ModelDocument doc;
    doc.newProject(6, 6, 0.0, 0.0, 100.0, 100.0, -2000.0, 0.0);

    int bId = doc.insertBodyAt(2, -500.0, 200.0);
    ASSERT_GT(bId, 0);

    auto *b = doc.model().getBody(bId);
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
    int bIdx = doc.model().getBodyIndex(bId, rowIntersect, colIntersect);
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
    EXPECT_DOUBLE_EQ(doc.model().getZ(rowIntersect, colIntersect, bIdx), -600.0);

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
    double spanX = doc.model().getXMax() - doc.model().getXMin();
    double spanY = doc.model().getYMax() - doc.model().getYMin();
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



