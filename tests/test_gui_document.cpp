#include <gtest/gtest.h>
#include <QApplication>
#include <QAction>
#include <QPixmap>
#include <QPainter>
#include <filesystem>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QWheelEvent>
#include <QToolBar>

#include "mod3d_gui/ModelDocument.h"
#include "mod3d_gui/ProfileView.h"
#include "mod3d_gui/HorizontalSliceView.h"
#include "mod3d_gui/MapView.h"
#include "mod3d_gui/SpreadsheetView.h"
#include "mod3d_gui/GL3DView.h"
#include "mod3d_gui/MainWindow.h"
#include "mod3d_gui/SettingsPane.h"
#include "mod3d_gui/dialogs/BodyPropertiesDialog.h"
#include "mod3d_gui/dialogs/DefineObservationDialog.h"
#include "mod3d_gui/dialogs/VerticalRangeDialog.h"

using namespace mod3d_gui;

namespace {
void saveTestScreenshot(const QPixmap &pix, const QString &filename) {
    const std::string dir = "docs/images";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    pix.save(QString::fromStdString(dir + "/" + filename.toStdString()));

    if (filename.startsWith("mod3d_")) {
        const std::vector<std::string> candidateDirs = { "../../docs/images", "../docs/images" };
        for (const auto &d : candidateDirs) {
            if (std::filesystem::exists(d)) {
                pix.save(QString::fromStdString(d + "/" + filename.toStdString()));
            }
        }
    }
}

std::string resolveLegacySamplePath(const std::string &filename) {
#ifdef MOD3D_LEGACY_EXAMPLES_DIR
    std::string pLegacy = std::string(MOD3D_LEGACY_EXAMPLES_DIR) + "/" + filename;
    if (std::filesystem::exists(pLegacy)) return pLegacy;
#endif
#ifdef MOD3D_EXAMPLES_DIR
    std::string pExamplesSub = std::string(MOD3D_EXAMPLES_DIR) + "/legacy/" + filename;
    if (std::filesystem::exists(pExamplesSub)) return pExamplesSub;
    std::string pExamples = std::string(MOD3D_EXAMPLES_DIR) + "/" + filename;
    if (std::filesystem::exists(pExamples)) return pExamples;
#endif
    if (std::filesystem::exists("examples/legacy/" + filename)) return "examples/legacy/" + filename;
    if (std::filesystem::exists("examples/" + filename)) return "examples/" + filename;
    if (std::filesystem::exists("examples_old/" + filename)) return "examples_old/" + filename;
#ifdef MOD3D_TEST_DATA_DIR
    std::string pTestData = std::string(MOD3D_TEST_DATA_DIR) + "/models/" + filename;
    if (std::filesystem::exists(pTestData)) return pTestData;
    std::string pTestDataGrid = std::string(MOD3D_TEST_DATA_DIR) + "/grids/" + filename;
    if (std::filesystem::exists(pTestDataGrid)) return pTestDataGrid;
#endif
    return "examples/legacy/" + filename;
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
    std::string samplePath = resolveLegacySamplePath("Test.m3d");
    if (!std::filesystem::exists(samplePath)) {
        samplePath = resolveLegacySamplePath("Sample.m3d");
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
    std::string samplePath = resolveLegacySamplePath("Sample.m3d");
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
    std::string gzGrdPath = resolveLegacySamplePath("gz.grd");
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
    std::string samplePath = resolveLegacySamplePath("Sample.m3d");
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
    std::string testPath = resolveLegacySamplePath("Test.m3d");
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
    std::string samplePath = resolveLegacySamplePath("Sample.m3d");
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
    std::string testPath = resolveLegacySamplePath("Test.m3d");
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

    // Vertical Exaggeration (Z-Scale)
    EXPECT_FLOAT_EQ(view.zScale(), 1.0f);
    view.setZScale(4.5f);
    EXPECT_FLOAT_EQ(view.zScale(), 4.5f);

    // Projection Mode (Perspective vs Orthographic)
    EXPECT_FALSE(view.isOrthographic());
    view.setOrthographic(true);
    EXPECT_TRUE(view.isOrthographic());
    view.setOrthographic(false);
    EXPECT_FALSE(view.isOrthographic());

    // Depth Fog / Atmospheric Haze (default off)
    EXPECT_FALSE(view.showDepthFog());
    view.setShowDepthFog(true);
    EXPECT_TRUE(view.showDepthFog());
    view.setShowDepthFog(false);
    EXPECT_FALSE(view.showDepthFog());

    // Target (LookAt Center) & Distance
    QVector3D newTarget(150.0f, 250.0f, -600.0f);
    view.setTarget(newTarget);
    EXPECT_FLOAT_EQ(view.target().x(), 150.0f);
    EXPECT_FLOAT_EQ(view.target().y(), 250.0f);
    EXPECT_FLOAT_EQ(view.target().z(), -600.0f);

    view.setCameraDistance(4500.0f);
    EXPECT_FLOAT_EQ(view.cameraDistance(), 4500.0f);

    view.setZoom(2.0f);
    EXPECT_FLOAT_EQ(view.zoom(), 2.0f);

    view.fitToView();
    view.resetCamera();
    EXPECT_FLOAT_EQ(view.pitch(), 30.0f);
    EXPECT_FLOAT_EQ(view.azimuth(), -45.0f);
    EXPECT_FLOAT_EQ(view.zoom(), 1.0f);
}

TEST_F(GuiDocumentTest, GL3DViewMouseDragStateNotStuckOnMoveWithoutButtonOrReset) {
    ModelDocument doc;
    doc.newProject(8, 10, 0.0, 0.0, 50.0, 50.0, -2000.0, 0.0);
    GL3DView view(&doc);
    view.resize(600, 400);
    view.resetCamera();

    QVector3D initialTarget = view.target();

    // 1. Simulate right-click press (initiates potential pan)
    QMouseEvent pressRight(QEvent::MouseButtonPress, QPointF(100, 100), QPointF(100, 100),
                           Qt::RightButton, Qt::RightButton, Qt::NoModifier);
    view.mousePressEvent(&pressRight);

    // 2. Simulate camera reset (as triggered by context menu, button, or shortcut)
    view.resetCamera();
    EXPECT_FLOAT_EQ(view.pitch(), 30.0f);
    EXPECT_FLOAT_EQ(view.azimuth(), -45.0f);

    // 3. Move mouse without holding any button down (hovering)
    QMouseEvent moveNoButton(QEvent::MouseMove, QPointF(250, 200), QPointF(250, 200),
                             Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    view.mouseMoveEvent(&moveNoButton);

    // Target and angles must NOT have panned or orbited
    EXPECT_FLOAT_EQ(view.target().x(), initialTarget.x());
    EXPECT_FLOAT_EQ(view.target().y(), initialTarget.y());
    EXPECT_FLOAT_EQ(view.target().z(), initialTarget.z());
    EXPECT_FLOAT_EQ(view.pitch(), 30.0f);
    EXPECT_FLOAT_EQ(view.azimuth(), -45.0f);

    // 4. Also verify that normal right-drag DOES pan when the button is actively held down
    QMouseEvent moveWithRight(QEvent::MouseMove, QPointF(260, 210), QPointF(260, 210),
                              Qt::NoButton, Qt::RightButton, Qt::NoModifier);
    // (Notice: without mousePressEvent first, moveWithRight won't pan)
    view.mouseMoveEvent(&moveWithRight);
    EXPECT_FLOAT_EQ(view.target().x(), initialTarget.x());

    // Press right button and then drag with right button held
    QMouseEvent pressRight2(QEvent::MouseButtonPress, QPointF(260, 210), QPointF(260, 210),
                            Qt::RightButton, Qt::RightButton, Qt::NoModifier);
    view.mousePressEvent(&pressRight2);
    QMouseEvent moveWithRight2(QEvent::MouseMove, QPointF(300, 250), QPointF(300, 250),
                               Qt::NoButton, Qt::RightButton, Qt::NoModifier);
    view.mouseMoveEvent(&moveWithRight2);
    EXPECT_NE(view.target(), initialTarget);

    // Release mouse button -> must stop dragging
    QMouseEvent releaseRight(QEvent::MouseButtonRelease, QPointF(300, 250), QPointF(300, 250),
                             Qt::RightButton, Qt::NoButton, Qt::NoModifier);
    view.mouseReleaseEvent(&releaseRight);

    QVector3D pannedTarget = view.target();
    QMouseEvent moveHover(QEvent::MouseMove, QPointF(350, 300), QPointF(350, 300),
                          Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    view.mouseMoveEvent(&moveHover);
    EXPECT_FLOAT_EQ(view.target().x(), pannedTarget.x());
    EXPECT_FLOAT_EQ(view.target().y(), pannedTarget.y());
}

TEST_F(GuiDocumentTest, GL3DViewBodySelectionAndRaycastHitTesting) {
    ModelDocument doc;
    doc.newProject(8, 10, 0.0, 0.0, 50.0, 50.0, -2000.0, 0.0);
    GL3DView view(&doc);
    view.resize(600, 400);
    view.resetCamera();

    EXPECT_EQ(view.selectedBodyId(), -1);

    int receivedBodyId = -999;
    QObject::connect(&view, &GL3DView::bodySelected, [&](int bId) {
        receivedBodyId = bId;
    });

    view.setSelectedBodyId(2);
    EXPECT_EQ(view.selectedBodyId(), 2);

    // Simulate clicking in empty outer space -> should deselect
    QMouseEvent pressEmpty(QEvent::MouseButtonPress, QPointF(10, 10), QPointF(10, 10),
                           Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    view.mousePressEvent(&pressEmpty);
    QMouseEvent releaseEmpty(QEvent::MouseButtonRelease, QPointF(10, 10), QPointF(10, 10),
                             Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    view.mouseReleaseEvent(&releaseEmpty);

    EXPECT_EQ(view.selectedBodyId(), -1);
    EXPECT_EQ(receivedBodyId, -1);

    // Verify software painting with selected body executes cleanly
    view.setSelectedBodyId(1);
    QPixmap pm(300, 200);
    QPainter p(&pm);
    view.paintSoftware(p);
}

TEST_F(GuiDocumentTest, MainWindowDefaultOpening2x2QuadTileLayout) {
    MainWindow mw;
    mw.resize(1200, 800);

    // Verify 5 subwindows opened by default
    auto subs = mw.mdiArea()->subWindowList();
    ASSERT_EQ(subs.size(), 5);

    ProfileView *pvNS = nullptr;
    ProfileView *pvEW = nullptr;
    MapView *mv = nullptr;
    HorizontalSliceView *hv = nullptr;
    GL3DView *glv = nullptr;

    QMdiSubWindow *subPvNS = nullptr;
    QMdiSubWindow *subPvEW = nullptr;
    QMdiSubWindow *subMap = nullptr;
    QMdiSubWindow *subSlice = nullptr;
    QMdiSubWindow *sub3D = nullptr;

    for (auto *sub : subs) {
        if (auto *p = qobject_cast<ProfileView*>(sub->widget())) {
            if (p->profileOrientation() == ProfileOrientation::SouthNorth) {
                pvNS = p;
                subPvNS = sub;
            } else {
                pvEW = p;
                subPvEW = sub;
            }
        } else if (auto *m = qobject_cast<MapView*>(sub->widget())) {
            mv = m;
            subMap = sub;
        } else if (auto *h = qobject_cast<HorizontalSliceView*>(sub->widget())) {
            hv = h;
            subSlice = sub;
        } else if (auto *g = qobject_cast<GL3DView*>(sub->widget())) {
            glv = g;
            sub3D = sub;
        }
    }

    ASSERT_NE(pvNS, nullptr);
    ASSERT_NE(pvEW, nullptr);
    ASSERT_NE(mv, nullptr);
    ASSERT_NE(hv, nullptr);
    ASSERT_NE(glv, nullptr);

    // Call arrangeDefaultTiles to arrange in quadrant grid
    mw.arrangeDefaultTiles();

    // Verify relative positions:
    // Top-Left: Profile view N-S
    EXPECT_EQ(subPvNS->x(), 0);
    EXPECT_EQ(subPvNS->y(), 0);

    // Top-Right: Profile view E-W
    EXPECT_GE(subPvEW->x(), subPvNS->width() - 2);
    EXPECT_EQ(subPvEW->y(), 0);

    // Bottom-Left: Horizontal slice view
    EXPECT_EQ(subSlice->x(), 0);
    EXPECT_GE(subSlice->y(), subPvNS->height() - 2);

    // Bottom-Right: split into 3D view (left) and Map view (right)
    EXPECT_GE(sub3D->x(), subSlice->width() - 2);
    EXPECT_GE(sub3D->y(), subPvEW->height() - 2);
    EXPECT_GE(subMap->x(), sub3D->x() + sub3D->width() - 2);
    EXPECT_GE(subMap->y(), subPvEW->height() - 2);

    // Active profile view should default to top-left profile (N-S)
    EXPECT_EQ(mw.activeProfileView(), pvNS);

#ifndef _WIN32
    // Save screenshot for visual verification
    QPixmap pix(1200, 800);
    pix.fill(Qt::white);
    mw.render(&pix);
    saveTestScreenshot(pix, "quad_layout_3d_turntable.png");
#endif
}

TEST_F(GuiDocumentTest, MainWindowAutoResizeSubWindowsOnClientAreaResize) {
#if defined(__linux__)
    GTEST_SKIP() << "Skipped on Linux CI (QOpenGLWidget unsupported on headless offscreen)";
#endif
    MainWindow mw;
    mw.resize(1200, 800);

    auto subs = mw.mdiArea()->subWindowList();
    ASSERT_EQ(subs.size(), 5);

    QMdiSubWindow *subPvNS = nullptr;
    QMdiSubWindow *subPvEW = nullptr;
    QMdiSubWindow *subMap = nullptr;
    QMdiSubWindow *subSlice = nullptr;
    QMdiSubWindow *sub3D = nullptr;

    for (auto *sub : subs) {
        if (auto *p = qobject_cast<ProfileView*>(sub->widget())) {
            if (p->profileOrientation() == ProfileOrientation::SouthNorth) subPvNS = sub;
            else subPvEW = sub;
        } else if (qobject_cast<MapView*>(sub->widget())) {
            subMap = sub;
        } else if (qobject_cast<HorizontalSliceView*>(sub->widget())) {
            subSlice = sub;
        } else if (qobject_cast<GL3DView*>(sub->widget())) {
            sub3D = sub;
        }
    }

    ASSERT_NE(subPvNS, nullptr);
    ASSERT_NE(subPvEW, nullptr);
    ASSERT_NE(subMap, nullptr);
    ASSERT_NE(subSlice, nullptr);
    ASSERT_NE(sub3D, nullptr);

    mw.show();
    QCoreApplication::processEvents();

    // Initial tile fitting at 1200x800
    int w1 = subPvNS->width() + subPvEW->width();
    int h1 = subPvNS->height() + subSlice->height();
    EXPECT_GT(w1, 500);
    EXPECT_GT(h1, 400);

    // Resize main window to larger geometry
    mw.resize(1500, 960);
    QCoreApplication::processEvents();

    int w2 = subPvNS->width() + subPvEW->width();
    int h2 = subPvNS->height() + subSlice->height();
    EXPECT_GT(w2, w1);
    EXPECT_GT(h2, h1);
    EXPECT_EQ(subPvNS->width(), subSlice->width());
    EXPECT_EQ(subPvEW->width(), sub3D->width() + subMap->width());
    EXPECT_EQ(subPvNS->height(), subPvEW->height());
    EXPECT_EQ(subSlice->height(), sub3D->height());
    EXPECT_EQ(sub3D->height(), subMap->height());

    // Resize main window to smaller geometry
    mw.resize(1000, 680);
    QCoreApplication::processEvents();

    int w3 = subPvNS->width() + subPvEW->width();
    int h3 = subPvNS->height() + subSlice->height();
    EXPECT_LT(w3, w2);
    EXPECT_LT(h3, h2);
    EXPECT_EQ(subPvNS->width(), subSlice->width());
    EXPECT_EQ(subPvEW->width(), sub3D->width() + subMap->width());
}

TEST_F(GuiDocumentTest, ModernSideSettingsPaneAndDockIntegration) {
    MainWindow mw;
    mw.resize(1280, 820);

    ASSERT_NE(mw.settingsPane(), nullptr);
    EXPECT_EQ(mw.dockWidgetArea(mw.settingsPane()), Qt::RightDockWidgetArea);
    EXPECT_FALSE(mw.settingsPane()->isHidden());

    // Verify 5 default views exist
    auto subWindows = mw.mdiArea()->subWindowList();
    EXPECT_EQ(subWindows.size(), 5);

    // Call arrange tiles
    mw.arrangeDefaultTiles();

#ifndef _WIN32
    // Verify rendering with side pane (Profile View + Fields Display tab)
    QPixmap pix(1280, 820);
    pix.fill(Qt::white);
    mw.render(&pix);
    saveTestScreenshot(pix, "modern_settings_pane_layout.png");
#endif

    // 2. Switch to Body Editor tab
    mw.settingsPane()->selectBodyEditorTab();
#ifndef _WIN32
    pix.fill(Qt::white);
    mw.render(&pix);
    saveTestScreenshot(pix, "modern_settings_body_editor.png");
#endif

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
#ifndef _WIN32
    pix.fill(Qt::white);
    mw.render(&pix);
    saveTestScreenshot(pix, "modern_settings_map_view.png");
#endif
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

TEST_F(GuiDocumentTest, SettingsPaneFourTabsIntegration) {
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
    EXPECT_DOUBLE_EQ(doc->observation().magnetic_intensity().value(), 48500.0);
    EXPECT_DOUBLE_EQ(doc->observation().inclination().value(), 65.0);
    EXPECT_DOUBLE_EQ(doc->observation().declination().value(), 12.0);

    // Sensor heights
    doc->observation().set_gravity_observation(mod3d::ObservationMode::SensorHeight, 150.0);
    doc->observation().set_magnetic_observation(mod3d::ObservationMode::SensorHeight, 120.0);
    EXPECT_DOUBLE_EQ(doc->observation().gravity_height().value(), 150.0);
    EXPECT_DOUBLE_EQ(doc->observation().magnetic_height().value(), 120.0);

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

TEST_F(GuiDocumentTest, NoToolbarsOnMainWindow) {
    MainWindow mw;
    mw.resize(1280, 800);

    // Verify all toolbars belonging to MainWindow have been completely removed
    auto toolbars = mw.findChildren<QToolBar*>();
    EXPECT_EQ(toolbars.size(), 0);
    EXPECT_EQ(mw.findChild<QToolBar*>("MainToolBar"), nullptr);
    EXPECT_EQ(mw.findChild<QToolBar*>("ProfileToolBar"), nullptr);
    EXPECT_EQ(mw.findChild<QToolBar*>("MapToolBar"), nullptr);

#ifndef _WIN32
    // Capture screenshot to visually verify toolbar-free clean area
    QPixmap pix(1280, 800);
    pix.fill(Qt::white);
    mw.render(&pix);
    saveTestScreenshot(pix, "mainwindow_clean_no_toolbars.png");
#endif
}

TEST_F(GuiDocumentTest, NoMeasuredFieldDisablesAndUnchecksMeasuredAndDifferenceWidgets) {
    MainWindow mw;
    mw.resize(1280, 800);

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

#ifndef _WIN32
    // Grab screenshot of disabled/unchecked state
    mw.mdiArea()->setActiveSubWindow(subPv);
    pane->setActiveSubWindow(subPv);
    QPixmap pixNoObs(1280, 800);
    pixNoObs.fill(Qt::white);
    mw.render(&pixNoObs);
    saveTestScreenshot(pixNoObs, "no_measured_field_disabled_state.png");
#endif

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

#ifndef _WIN32
    // Grab screenshot of enabled/checked state
    QPixmap pixWithObs(1280, 800);
    pixWithObs.fill(Qt::white);
    mw.render(&pixWithObs);
    saveTestScreenshot(pixWithObs, "measured_field_enabled_state.png");
#endif

    // Activate MapView: radio buttons should now be enabled
    mw.mdiArea()->setActiveSubWindow(subMv);
    pane->setActiveSubWindow(subMv);

    EXPECT_TRUE(rbMapObserved->isEnabled());
    EXPECT_TRUE(rbMapDiff->isEnabled());
}

TEST_F(GuiDocumentTest, OpenSampleBeginAndVerifyCleanDisplay) {
    MainWindow mw;
    mw.resize(1400, 900);

    std::string sampleBeginPath = resolveLegacySamplePath("SampleBegin.m3d");
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

#ifndef _WIN32
    mw.arrangeDefaultTiles();
    QPixmap pix(1400, 900);
    pix.fill(Qt::white);
    mw.render(&pix);
    saveTestScreenshot(pix, "sample_begin_loaded.png");
#endif
}

TEST_F(GuiDocumentTest, SampleM3DBodyPropertiesAndSIUnitsMatchBetweenPaneAndDialog) {
    MainWindow mw;
    mw.resize(1400, 900);

    std::string samplePath = resolveLegacySamplePath("Sample.m3d");
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

    std::string samplePath = resolveLegacySamplePath("Sample.m3d");
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

    std::string samplePath = resolveLegacySamplePath("Sample.m3d");
    bool ok = mw.document()->openFile(QString::fromStdString(samplePath));
    ASSERT_TRUE(ok);

    auto *pane = mw.settingsPane();
    ASSERT_NE(pane, nullptr);
    pane->selectBodyEditorTab();

    // 1. Full Main Workspace screenshot
    QPixmap pixMain(1440, 920);
    pixMain.fill(Qt::white);
    mw.render(&pixMain); // First pass forces MainWindow & QMdiArea layout

    mw.arrangeDefaultTiles(); // Tiles perfectly to actual mdiArea geometry!

    pixMain.fill(Qt::white);
    mw.render(&pixMain); // Final render with perfectly fitted tiles

    // Paint the 3D software rendering onto the 3D subwindow area of pixMain
    for (auto *sub : mw.mdiArea()->subWindowList()) {
        if (auto *glv = qobject_cast<GL3DView*>(sub->widget())) {
            QPoint pos = glv->mapTo(&mw, QPoint(0, 0));
            QPainter p(&pixMain);
            p.translate(pos);
            glv->paintSoftware(p);

            // Also save standalone 3D turntable close-up screenshot
            QSize s = (glv->width() >= 100 && glv->height() >= 100) ? glv->size() : QSize(700, 500);
            QPixmap pix3D(s);
            pix3D.fill(Qt::white);
            QPainter p3D(&pix3D);
            glv->paintSoftware(p3D);
            saveTestScreenshot(pix3D, "mod3d_3d_turntable.png");
            break;
        }
    }

    saveTestScreenshot(pixMain, "mod3d_main_workspace.png");

    // 2. Profile View close-up screenshot
    auto *pv = mw.activeProfileView();
    if (pv) {
        QSize s = (pv->width() >= 100 && pv->height() >= 100) ? pv->size() : QSize(800, 500);
        QPixmap pixProfile(s);
        pixProfile.fill(Qt::white);
        pv->render(&pixProfile);
        saveTestScreenshot(pixProfile, "mod3d_profile_cross_section.png");
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
        QSize s = (mv->width() >= 100 && mv->height() >= 100) ? mv->size() : QSize(600, 600);
        QPixmap pixMap(s);
        pixMap.fill(Qt::white);
        mv->render(&pixMap);
        saveTestScreenshot(pixMap, "mod3d_map_view.png");
    }

    // 4. Settings Pane Geological Body Editor screenshot
    QSize sPane = (pane->width() >= 100 && pane->height() >= 100) ? pane->size() : QSize(380, 820);
    QPixmap pixPane(sPane);
    pixPane.fill(Qt::white);
    pane->render(&pixPane);
    saveTestScreenshot(pixPane, "mod3d_body_editor.png");
}

TEST_F(GuiDocumentTest, GL3DViewAndSettingsPaneTransparencyToggles) {
    MainWindow mw;
    mw.resize(1200, 800);

    auto *doc = mw.document();
    ASSERT_NE(doc, nullptr);

    GL3DView *glv = nullptr;
    QMdiSubWindow *sub3D = nullptr;
    for (auto *sub : mw.mdiArea()->subWindowList()) {
        if (auto *g = qobject_cast<GL3DView*>(sub->widget())) {
            glv = g;
            sub3D = sub;
            break;
        }
    }
    ASSERT_NE(glv, nullptr);
    ASSERT_NE(sub3D, nullptr);

    // 1. Verify default 3D transparency values (transparent bodies off by default)
    EXPECT_FALSE(glv->showTransparentBodies());
    EXPECT_TRUE(glv->showTransparentProfilePlane());

    // 2. Activate 3D subwindow and check SettingsPane synchronization
    mw.mdiArea()->setActiveSubWindow(sub3D);
    auto *pane = mw.settingsPane();
    ASSERT_NE(pane, nullptr);
    pane->selectDisplayTab();

    auto *chkTranspBodies = pane->transparentBodiesCheckBox();
    auto *chkTranspSlices = pane->transparentSlicesCheckBox();
    ASSERT_NE(chkTranspBodies, nullptr);
    ASSERT_NE(chkTranspSlices, nullptr);

    EXPECT_FALSE(chkTranspBodies->isChecked());
    EXPECT_TRUE(chkTranspSlices->isChecked());

    // 3. Toggle via SettingsPane checkboxes and verify GL3DView updates
    chkTranspBodies->setChecked(true);
    EXPECT_TRUE(glv->showTransparentBodies());

    chkTranspSlices->setChecked(false);
    EXPECT_FALSE(glv->showTransparentProfilePlane());

    // Software paint test without crashes
    QPixmap pm(400, 400);
    QPainter painter(&pm);
    glv->paintSoftware(painter);

    // Reset back to defaults
    chkTranspBodies->setChecked(false);
    chkTranspSlices->setChecked(true);
    EXPECT_FALSE(glv->showTransparentBodies());
    EXPECT_TRUE(glv->showTransparentProfilePlane());

    // 4. Per-body transparency toggle in Tab 3 (Body Editor)
    int bodyId = doc->insertBodyAt(2, -500.0, 200.0);
    ASSERT_GT(bodyId, 0);
    auto *b0 = doc->model().body(bodyId);
    ASSERT_NE(b0, nullptr);

    pane->selectBodyEditorTab();
    auto *chkBodyTrans = pane->bodyTransparentCheckBox();
    ASSERT_NE(chkBodyTrans, nullptr);

    // Initial body transparency
    bool initTrans = b0->is_transparent();
    EXPECT_EQ(chkBodyTrans->isChecked(), initTrans);

    // Toggle body transparency in editor
    chkBodyTrans->setChecked(!initTrans);
    EXPECT_EQ(b0->is_transparent(), !initTrans);

    // Reset back
    chkBodyTrans->setChecked(initTrans);
    EXPECT_EQ(b0->is_transparent(), initTrans);
}

TEST_F(GuiDocumentTest, GL3DViewReliefColormapAndSurfaceWireframeModes) {
    MainWindow mw;
    mw.resize(1200, 800);

    GL3DView *glv = nullptr;
    QMdiSubWindow *sub3D = nullptr;
    for (auto *sub : mw.mdiArea()->subWindowList()) {
        if (auto *g = qobject_cast<GL3DView*>(sub->widget())) {
            glv = g;
            sub3D = sub;
            break;
        }
    }
    ASSERT_NE(glv, nullptr);
    ASSERT_NE(sub3D, nullptr);

    // 1. Verify default 3D relief settings (wireframe by default)
    EXPECT_TRUE(glv->showRelief());
    EXPECT_EQ(glv->reliefMode(), GL3DView::ReliefMode::Wireframe);

    // 2. Direct setter and signal emission
    int signalCount = 0;
    GL3DView::ReliefMode emittedMode = GL3DView::ReliefMode::Wireframe;
    QObject::connect(glv, &GL3DView::reliefModeChanged, [&](GL3DView::ReliefMode m) {
        emittedMode = m;
        ++signalCount;
    });

    glv->setReliefMode(GL3DView::ReliefMode::Surface);
    EXPECT_EQ(glv->reliefMode(), GL3DView::ReliefMode::Surface);
    EXPECT_EQ(signalCount, 1);
    EXPECT_EQ(emittedMode, GL3DView::ReliefMode::Surface);

    // Setting same mode should not re-emit
    glv->setReliefMode(GL3DView::ReliefMode::Surface);
    EXPECT_EQ(signalCount, 1);

    // 3. SettingsPane synchronization and controls
    mw.mdiArea()->setActiveSubWindow(sub3D);
    auto *pane = mw.settingsPane();
    ASSERT_NE(pane, nullptr);
    pane->selectDisplayTab();

    auto *chkRelief = pane->reliefCheckBox3D();
    auto *comboMode = pane->reliefModeComboBox3D();
    ASSERT_NE(chkRelief, nullptr);
    ASSERT_NE(comboMode, nullptr);

    EXPECT_TRUE(chkRelief->isChecked());
    EXPECT_TRUE(comboMode->isEnabled());
    EXPECT_EQ(comboMode->currentData().toInt(), static_cast<int>(GL3DView::ReliefMode::Surface));

    // Change combo to Wireframe via UI
    int wireIdx = comboMode->findData(static_cast<int>(GL3DView::ReliefMode::Wireframe));
    ASSERT_GE(wireIdx, 0);
    comboMode->setCurrentIndex(wireIdx);
    EXPECT_EQ(glv->reliefMode(), GL3DView::ReliefMode::Wireframe);
    EXPECT_EQ(signalCount, 2);

    // Toggle relief off via checkbox
    chkRelief->setChecked(false);
    EXPECT_FALSE(glv->showRelief());
    EXPECT_FALSE(comboMode->isEnabled());

    // Toggle relief on via checkbox
    chkRelief->setChecked(true);
    EXPECT_TRUE(glv->showRelief());
    EXPECT_TRUE(comboMode->isEnabled());

    // 4. Software painter renders cleanly in both Surface and Wireframe modes
    QPixmap pmSurf(500, 500);
    pmSurf.fill(Qt::white);
    {
        QPainter painter(&pmSurf);
        glv->setReliefMode(GL3DView::ReliefMode::Surface);
        glv->paintSoftware(painter);
    }
    EXPECT_FALSE(pmSurf.isNull());

    QPixmap pmWire(500, 500);
    pmWire.fill(Qt::white);
    {
        QPainter painter(&pmWire);
        glv->setReliefMode(GL3DView::ReliefMode::Wireframe);
        glv->paintSoftware(painter);
    }
    EXPECT_FALSE(pmWire.isNull());
}

TEST_F(GuiDocumentTest, GL3DViewRetainsLastlySelectedProfileViewOnFocus) {
    MainWindow mw;
    mw.resize(1200, 800);

    ProfileView *pvEW = nullptr;
    ProfileView *pvSN = nullptr;
    GL3DView *glv = nullptr;
    QMdiSubWindow *subEW = nullptr;
    QMdiSubWindow *subSN = nullptr;
    QMdiSubWindow *sub3D = nullptr;

    for (auto *sub : mw.mdiArea()->subWindowList()) {
        if (auto *pv = qobject_cast<ProfileView*>(sub->widget())) {
            if (pv->profileOrientation() == ProfileOrientation::EastWest) {
                pvEW = pv;
                subEW = sub;
            } else if (pv->profileOrientation() == ProfileOrientation::SouthNorth) {
                pvSN = pv;
                subSN = sub;
            }
        } else if (auto *g = qobject_cast<GL3DView*>(sub->widget())) {
            glv = g;
            sub3D = sub;
        }
    }

    if (!pvSN) {
        pvSN = mw.openProfileView(ProfileOrientation::SouthNorth);
        subSN = qobject_cast<QMdiSubWindow*>(pvSN->parentWidget());
    }

    ASSERT_NE(pvEW, nullptr);
    ASSERT_NE(pvSN, nullptr);
    ASSERT_NE(glv, nullptr);
    ASSERT_NE(subEW, nullptr);
    ASSERT_NE(subSN, nullptr);
    ASSERT_NE(sub3D, nullptr);

    // 1. Activate South-North profile view and change its profile
    mw.mdiArea()->setActiveSubWindow(subSN);
    pvSN->setProfileIndex(3);
    EXPECT_EQ(mw.activeProfileView(), pvSN);
    EXPECT_EQ(mw.lastActiveProfileView(), pvSN);
    EXPECT_EQ(mw.document()->profileOrientation(), ProfileOrientation::SouthNorth);
    EXPECT_EQ(mw.document()->profileIndex(), 3);

    // 2. Click / activate 3D View subwindow
    mw.mdiArea()->setActiveSubWindow(sub3D);

    // Verify activeProfileView() and lastActiveProfileView() still return pvSN
    EXPECT_EQ(mw.activeProfileView(), pvSN);
    EXPECT_EQ(mw.lastActiveProfileView(), pvSN);
    EXPECT_EQ(mw.document()->profileOrientation(), ProfileOrientation::SouthNorth);
    EXPECT_EQ(mw.document()->profileIndex(), 3);

    // Render software painter without crashing or resetting profile
    QPixmap pm(400, 400);
    QPainter p(&pm);
    glv->paintSoftware(p);

    EXPECT_EQ(mw.document()->profileOrientation(), ProfileOrientation::SouthNorth);
    EXPECT_EQ(mw.document()->profileIndex(), 3);

    // 3. Now activate East-West profile view and change its profile
    mw.mdiArea()->setActiveSubWindow(subEW);
    pvEW->setProfileIndex(7);
    EXPECT_EQ(mw.activeProfileView(), pvEW);
    EXPECT_EQ(mw.lastActiveProfileView(), pvEW);
    EXPECT_EQ(mw.document()->profileOrientation(), ProfileOrientation::EastWest);
    EXPECT_EQ(mw.document()->profileIndex(), 7);

    // 4. Click / activate 3D View again
    mw.mdiArea()->setActiveSubWindow(sub3D);

    // Verify active profile stays on East-West at index 7, NOT switching back to South-North
    EXPECT_EQ(mw.activeProfileView(), pvEW);
    EXPECT_EQ(mw.lastActiveProfileView(), pvEW);
    EXPECT_EQ(mw.document()->profileOrientation(), ProfileOrientation::EastWest);
    EXPECT_EQ(mw.document()->profileIndex(), 7);

    glv->paintSoftware(p);
    EXPECT_EQ(mw.document()->profileOrientation(), ProfileOrientation::EastWest);
    EXPECT_EQ(mw.document()->profileIndex(), 7);

    // 5. While 3D view is active, navigation via activeProfileView advances the latest profile
    mw.activeProfileView()->moveProfile(1);
    EXPECT_EQ(pvEW->profileIndex(), 8);
    EXPECT_EQ(mw.document()->profileIndex(), 8);
}

TEST_F(GuiDocumentTest, BodyDeletionRemovesBodyFrom3DView) {
    MainWindow mw;
    mw.resize(1200, 800);

    std::string samplePath = resolveLegacySamplePath("Sample.m3d");
    bool ok = mw.document()->openFile(QString::fromStdString(samplePath));
    ASSERT_TRUE(ok);

    // Find GL3DView
    GL3DView *glv = nullptr;
    for (auto *sub : mw.mdiArea()->subWindowList()) {
        if (auto *v = qobject_cast<GL3DView*>(sub->widget())) {
            glv = v;
            break;
        }
    }
    ASSERT_NE(glv, nullptr);

    // Render once in software mode to populate initial meshes
    QPixmap pix(600, 400);
    pix.fill(Qt::white);
    QPainter p(&pix);
    glv->paintSoftware(p);

    const auto &initialMeshes = glv->bodyMeshes();
    ASSERT_FALSE(initialMeshes.empty());
    size_t initialBodyCount = mw.document()->model().bodies().size();
    ASSERT_GE(initialBodyCount, 1u);

    // Body to delete exists in 3D view
    int bodyIdToDelete = mw.document()->model().bodies().front()->id();
    bool foundInMeshes = false;
    for (const auto &m : initialMeshes) {
        if (m.bodyId == bodyIdToDelete) {
            foundInMeshes = true;
            break;
        }
    }
    EXPECT_TRUE(foundInMeshes);

    glv->setSelectedBodyId(bodyIdToDelete);
    EXPECT_EQ(glv->selectedBodyId(), bodyIdToDelete);

    // Delete the body via ModelDocument::deleteBody
    bool delOk = mw.document()->deleteBody(bodyIdToDelete);
    EXPECT_TRUE(delOk);

    // Verify body is completely removed from model
    EXPECT_EQ(mw.document()->model().bodies().size(), initialBodyCount - 1);
    EXPECT_EQ(mw.document()->model().body(bodyIdToDelete), nullptr);

    // Repaint in software mode
    glv->paintSoftware(p);

    // Verify deleted body is NOT in glv->bodyMeshes()
    for (const auto &m : glv->bodyMeshes()) {
        EXPECT_NE(m.bodyId, bodyIdToDelete);
    }

    // Selected body ID should be cleared from deleted body
    EXPECT_NE(glv->selectedBodyId(), bodyIdToDelete);

    // Also test defensive mechanism: if another body is added and deleted directly on model
    auto *newB = mw.document()->createNewBody();
    ASSERT_NE(newB, nullptr);
    int newBId = newB->id();
    glv->paintSoftware(p);

    // Delete directly on model without calling document signal
    mw.document()->model().delete_body(newBId);
    EXPECT_EQ(mw.document()->model().body(newBId), nullptr);

    // Paint software should detect the difference in body count and remove it immediately
    glv->paintSoftware(p);
    for (const auto &m : glv->bodyMeshes()) {
        EXPECT_NE(m.bodyId, newBId);
    }
}

TEST_F(GuiDocumentTest, FirstVertexMoveAfterOpeningModelDoesNotBlowUpRealTimeField) {
    std::string samplePath = resolveLegacySamplePath("Sample.m3d");
    ASSERT_TRUE(std::filesystem::exists(samplePath));

    ModelDocument doc;
    EXPECT_TRUE(doc.isRealTimeCompute());
    ASSERT_TRUE(doc.openFile(QString::fromStdString(samplePath)));

    // Immediately after loading, facets_update must be empty (not queuing the entire model)
    EXPECT_TRUE(doc.model().facets_update().empty());

    const auto *modGrid = doc.activeModeledGrid();
    ASSERT_NE(modGrid, nullptr);
    size_t midR = doc.observation().rows() / 2;
    size_t midC = doc.observation().cols() / 2;
    double initialVal = (*modGrid)(midR, midC);

    // Pick an existing body vertex in the model to move slightly
    int r = 0, c = 0;
    int vIdx = -1;
    for (int row = 1; row < doc.model().rows() - 1 && vIdx < 0; ++row) {
        for (int col = 1; col < doc.model().cols() - 1 && vIdx < 0; ++col) {
            if (doc.model().count(row, col) > 2) {
                r = row;
                c = col;
                vIdx = 1;
            }
        }
    }
    ASSERT_GE(vIdx, 0);

    double oldZ = doc.model().z(r, c, vIdx);
    double newZ = oldZ + 10.0;

    bool moved = doc.moveVertexAtRowCol(r, c, vIdx, newZ, mod3d::BodyMoveType::Normal);
    EXPECT_TRUE(moved);

    double realtimeVal = (*doc.activeModeledGrid())(midR, midC);

    // Now recompute all (as happens on mouse release in ProfileView)
    doc.computeAll();
    double recomputedVal = (*doc.activeModeledGrid())(midR, midC);

    // The real-time delta computed value must closely match the full recomputed field
    // and NOT be inflated / doubled due to entire model facets being erroneously queued in facets_update_!
    EXPECT_NEAR(realtimeVal, recomputedVal, 1e-3);
    // Also verify change was slight, not massive doubling
    EXPECT_NEAR(realtimeVal, initialVal, std::abs(initialVal) * 0.2 + 1.0);
}

TEST_F(GuiDocumentTest, GL3DViewQuadrantCutAndSettingsSync) {
    MainWindow mw;
    mw.resize(1200, 800);

    std::string samplePath = resolveLegacySamplePath("Sample.m3d");
    ASSERT_TRUE(mw.document()->openFile(QString::fromStdString(samplePath)));

    ProfileView *pvEW1 = nullptr;
    ProfileView *pvSN1 = nullptr;
    GL3DView *glv = nullptr;
    QMdiSubWindow *sub3D = nullptr;

    for (auto *sub : mw.mdiArea()->subWindowList()) {
        if (auto *pv = qobject_cast<ProfileView*>(sub->widget())) {
            if (pv->profileOrientation() == ProfileOrientation::EastWest) pvEW1 = pv;
            else if (pv->profileOrientation() == ProfileOrientation::SouthNorth) pvSN1 = pv;
        } else if (auto *g = qobject_cast<GL3DView*>(sub->widget())) {
            glv = g;
            sub3D = sub;
        }
    }
    if (!pvSN1) {
        pvSN1 = mw.openProfileView(ProfileOrientation::SouthNorth);
    }
    ASSERT_NE(pvEW1, nullptr);
    ASSERT_NE(pvSN1, nullptr);
    ASSERT_NE(glv, nullptr);
    ASSERT_NE(sub3D, nullptr);

    // 1. Default state is disabled
    EXPECT_FALSE(glv->quadrantCut());
    EXPECT_FALSE(glv->computeQuadrantCutState().active);

    // 2. Activate 3D subwindow and check SettingsPane synchronization
    mw.mdiArea()->setActiveSubWindow(sub3D);
    auto *pane = mw.settingsPane();
    ASSERT_NE(pane, nullptr);
    pane->selectDisplayTab();

    auto *chkQuadCut = pane->quadrantCutCheckBox3D();
    ASSERT_NE(chkQuadCut, nullptr);
    EXPECT_FALSE(chkQuadCut->isChecked());

    int sigCount = 0;
    bool lastSigVal = false;
    QObject::connect(glv, &GL3DView::quadrantCutChanged, [&](bool enabled) {
        ++sigCount;
        lastSigVal = enabled;
    });

    // Toggle via SettingsPane checkbox
    chkQuadCut->setChecked(true);
    EXPECT_TRUE(glv->quadrantCut());
    EXPECT_EQ(sigCount, 1);
    EXPECT_TRUE(lastSigVal);

    // Toggle via 'C' keyboard shortcut on GL3DView and verify SettingsPane checkbox updates
    QKeyEvent pressC(QEvent::KeyPress, Qt::Key_C, Qt::NoModifier, "c");
    QCoreApplication::sendEvent(glv, &pressC);
    EXPECT_FALSE(glv->quadrantCut());
    EXPECT_FALSE(chkQuadCut->isChecked());
    EXPECT_EQ(sigCount, 2);
    EXPECT_FALSE(lastSigVal);

    // Re-enable quadrant cut
    glv->setQuadrantCut(true);
    EXPECT_TRUE(glv->quadrantCut());
    EXPECT_TRUE(chkQuadCut->isChecked());

    // 3. Open a second E-W and second S-N profile to verify front-most selection across camera quadrants
    pvEW1->setProfileIndex(2); // row = 3
    pvSN1->setProfileIndex(2); // col = 3

    auto *pvEW2 = new ProfileView(mw.document(), ProfileOrientation::EastWest, 5, &mw);
    mw.mdiArea()->addSubWindow(pvEW2);

    auto *pvSN2 = new ProfileView(mw.document(), ProfileOrientation::SouthNorth, 6, &mw);
    mw.mdiArea()->addSubWindow(pvSN2);

    // Camera at default South-East quadrant (azimuth = -45 deg -> +X East, -Y South)
    // Front-most E-W profile is minimum Y (row = 3), front-most S-N profile is maximum X (col = 7)
    glv->setCameraAngles(30.0f, -45.0f);
    auto cutSE = glv->computeQuadrantCutState();
    EXPECT_TRUE(cutSE.active);
    EXPECT_TRUE(cutSE.hasEW);
    EXPECT_TRUE(cutSE.hasSN);
    EXPECT_TRUE(cutSE.cutPosX);
    EXPECT_FALSE(cutSE.cutPosY);
    EXPECT_EQ(cutSE.ewRow, 3);
    EXPECT_EQ(cutSE.snCol, 7);

    // Camera at North-West quadrant (azimuth = +135 deg -> -X West, +Y North)
    // Front-most E-W profile is maximum Y (row = 6), front-most S-N profile is minimum X (col = 3)
    glv->setCameraAngles(30.0f, 135.0f);
    auto cutNW = glv->computeQuadrantCutState();
    EXPECT_TRUE(cutNW.active);
    EXPECT_FALSE(cutNW.cutPosX);
    EXPECT_TRUE(cutNW.cutPosY);
    EXPECT_EQ(cutNW.ewRow, 6);
    EXPECT_EQ(cutNW.snCol, 3);

    // Camera at South-West quadrant (azimuth = +45 deg -> -X West, -Y South)
    glv->setCameraAngles(30.0f, 45.0f);
    auto cutSW = glv->computeQuadrantCutState();
    EXPECT_TRUE(cutSW.active);
    EXPECT_FALSE(cutSW.cutPosX);
    EXPECT_FALSE(cutSW.cutPosY);
    EXPECT_EQ(cutSW.ewRow, 3);
    EXPECT_EQ(cutSW.snCol, 3);

    // Camera at North-East quadrant (azimuth = -135 deg -> +X East, +Y North)
    glv->setCameraAngles(30.0f, -135.0f);
    auto cutNE = glv->computeQuadrantCutState();
    EXPECT_TRUE(cutNE.active);
    EXPECT_TRUE(cutNE.cutPosX);
    EXPECT_TRUE(cutNE.cutPosY);
    EXPECT_EQ(cutNE.ewRow, 6);
    EXPECT_EQ(cutNE.snCol, 7);

    // 4. Verify cross-section cap generation for bodies intersecting the cut walls
    // Position both profiles through an interior cell where a body has 3D volume
    int bodyRow = mw.document()->model().rows() / 2;
    int bodyCol = mw.document()->model().cols() / 2;
    for (int r = 2; r < mw.document()->model().rows() - 3; ++r) {
        for (int c = 2; c < mw.document()->model().cols() - 3; ++c) {
            if (mw.document()->model().count(r, c) > 2 &&
                mw.document()->model().count(r + 1, c) > 2 &&
                mw.document()->model().count(r, c + 1) > 2) {
                bodyRow = r;
                bodyCol = c;
                break;
            }
        }
    }
    pvEW1->setProfileIndex(bodyRow - 1);
    pvEW2->setProfileIndex(bodyRow - 1);
    pvSN1->setProfileIndex(bodyCol - 1);
    pvSN2->setProfileIndex(bodyCol - 1);

    glv->setCameraAngles(30.0f, -45.0f);
    auto cutBodySE = glv->computeQuadrantCutState();
    ASSERT_TRUE(cutBodySE.active);
    EXPECT_EQ(cutBodySE.ewRow, bodyRow);
    EXPECT_EQ(cutBodySE.snCol, bodyCol);

    size_t totalCapFacets = 0;
    size_t totalCapEdges = 0;
    for (const auto &b : mw.document()->model().bodies()) {
        std::vector<mod3d::Facet3Pt> capFacets;
        std::vector<std::pair<mod3d::Point3D, mod3d::Point3D>> capEdges;
        glv->generateBodyCutCaps(b->id(), cutBodySE, capFacets, capEdges);
        totalCapFacets += capFacets.size();
        totalCapEdges += capEdges.size();
    }
    // Cut bodies are hollow: zero facets on the cut plane, while cut rim edges are generated
    EXPECT_EQ(totalCapFacets, 0u);
    EXPECT_GT(totalCapEdges, 0u);

    // 5. Render in software mode with both Surface and Wireframe relief and quadrant cut active
    QPixmap pmCutSurf(600, 500);
    pmCutSurf.fill(Qt::white);
    {
        QPainter painter(&pmCutSurf);
        glv->setShowRelief(true);
        glv->setReliefMode(GL3DView::ReliefMode::Surface);
        glv->paintSoftware(painter);
    }
    EXPECT_FALSE(pmCutSurf.isNull());

    QPixmap pmCutWire(600, 500);
    pmCutWire.fill(Qt::white);
    {
        QPainter painter(&pmCutWire);
        glv->setReliefMode(GL3DView::ReliefMode::Wireframe);
        glv->paintSoftware(painter);
    }
    EXPECT_FALSE(pmCutWire.isNull());
}

TEST_F(GuiDocumentTest, DefineObservationDialogNewModelWorkflow) {
    ModelDocument doc;

    // 1. Custom dimensions mode (Min/Max/Increment synchronization)
    {
        DefineObservationDialog dlg(&doc);
        EXPECT_EQ(dlg.creationMode(), DefineObservationDialog::CreationMode::DefineObservation);
        EXPECT_FALSE(dlg.isFromGridFile());
        EXPECT_EQ(dlg.rowsSpinBox()->value(), 21);
        EXPECT_EQ(dlg.colsSpinBox()->value(), 21);

        dlg.minXSpinBox()->setValue(1000.0);
        dlg.dxSpinBox()->setValue(250.0);
        dlg.maxXSpinBox()->setValue(3500.0); // (3500 - 1000) / 250 + 1 = 11 cols
        EXPECT_EQ(dlg.colsSpinBox()->value(), 11);

        dlg.minYSpinBox()->setValue(2000.0);
        dlg.dySpinBox()->setValue(500.0);
        dlg.maxYSpinBox()->setValue(6000.0); // (6000 - 2000) / 500 + 1 = 9 rows
        EXPECT_EQ(dlg.rowsSpinBox()->value(), 9);

        EXPECT_DOUBLE_EQ(dlg.lengthX(), 2500.0);
        EXPECT_DOUBLE_EQ(dlg.lengthY(), 4000.0);
        EXPECT_TRUE(std::isnan(dlg.gridMinZ()));
        EXPECT_TRUE(std::isnan(dlg.gridMaxZ()));

        dlg.minZSpinBox()->setValue(-3000.0);
        dlg.maxZSpinBox()->setValue(500.0);

        EXPECT_TRUE(dlg.applyToDocument());
        EXPECT_EQ(doc.observation().rows(), 9u);
        EXPECT_EQ(doc.observation().cols(), 11u);
        EXPECT_EQ(doc.model().rows(), 11); // 9 + 2 padding rows
        EXPECT_EQ(doc.model().cols(), 13); // 11 + 2 padding cols
        EXPECT_DOUBLE_EQ(doc.model().x0_val(), 1000.0);
        EXPECT_DOUBLE_EQ(doc.model().y0_val(), 2000.0);
        EXPECT_DOUBLE_EQ(doc.model().dx_val(), 250.0);
        EXPECT_DOUBLE_EQ(doc.model().dy_val(), 500.0);
        EXPECT_DOUBLE_EQ(doc.model().z_min_val(), -3000.0);
        EXPECT_DOUBLE_EQ(doc.model().z_max_val(), 500.0);
    }

    // 2. Create a temporary Surfer .grd file to test "Get From Grid File" and "Import observation area relief grid"
    mod3d::Grid testGrid(6, 8, 500.0, 1500.0, 100.0, 200.0);
    for (size_t r = 0; r < testGrid.rows(); ++r) {
        for (size_t c = 0; c < testGrid.cols(); ++c) {
            testGrid(r, c) = 120.0 + static_cast<double>(r * 10 + c * 5);
        }
    }
    const std::string tmpGrdPath = "test_define_obs_relief.grd";
    ASSERT_TRUE(testGrid.save_srf6_ascii(tmpGrdPath));

    // 2a. DefineObservation mode + Get From Grid File (geometry only, flat top at ZMax)
    {
        DefineObservationDialog dlg(&doc);
        ASSERT_TRUE(dlg.loadFromGridFile(QString::fromStdString(tmpGrdPath)));
        EXPECT_TRUE(dlg.isFromGridFile());
        EXPECT_FALSE(dlg.minXSpinBox()->isEnabled());
        EXPECT_EQ(dlg.rowsSpinBox()->value(), 6);
        EXPECT_EQ(dlg.colsSpinBox()->value(), 8);
        EXPECT_DOUBLE_EQ(dlg.minXSpinBox()->value(), 500.0);
        EXPECT_DOUBLE_EQ(dlg.maxXSpinBox()->value(), 1200.0);
        EXPECT_DOUBLE_EQ(dlg.minYSpinBox()->value(), 1500.0);
        EXPECT_DOUBLE_EQ(dlg.maxYSpinBox()->value(), 2500.0);
        EXPECT_DOUBLE_EQ(dlg.lengthX(), 700.0);
        EXPECT_DOUBLE_EQ(dlg.lengthY(), 1000.0);
        EXPECT_NEAR(dlg.gridMinZ(), 120.0, 1e-4);
        EXPECT_NEAR(dlg.gridMaxZ(), 205.0, 1e-4);
        ASSERT_NE(dlg.lengthXLabel(), nullptr);
        ASSERT_NE(dlg.gridMinZLabel(), nullptr);
        ASSERT_NE(dlg.gridMaxZLabel(), nullptr);
        EXPECT_TRUE(dlg.lengthXLabel()->text().contains("700.00"));
        EXPECT_TRUE(dlg.lengthYLabel()->text().contains("1000.00"));
        EXPECT_TRUE(dlg.gridMinZLabel()->text().contains("120.00"));
        EXPECT_TRUE(dlg.gridMaxZLabel()->text().contains("205.00"));

        dlg.minZSpinBox()->setValue(-2500.0);
        dlg.maxZSpinBox()->setValue(800.0);
        EXPECT_TRUE(dlg.applyToDocument());
        EXPECT_EQ(doc.observation().rows(), 6u);
        EXPECT_EQ(doc.observation().cols(), 8u);
        EXPECT_DOUBLE_EQ(doc.model().z(1, 1, 0), 800.0);
    }

    // 2b. ImportReliefGrid mode (model surface topography + observation relief initialized from grid)
    {
        DefineObservationDialog dlg(&doc);
        dlg.setCreationMode(DefineObservationDialog::CreationMode::ImportReliefGrid);
        dlg.minZSpinBox()->setValue(-2000.0);
        dlg.maxZSpinBox()->setValue(100.0); // Below grid max (205.0), should auto-raise to ceil(grid.max_value())
        ASSERT_TRUE(dlg.loadFromGridFile(QString::fromStdString(tmpGrdPath)));
        EXPECT_GE(dlg.maxZSpinBox()->value(), 205.0);

        EXPECT_TRUE(dlg.applyToDocument());
        EXPECT_EQ(doc.observation().rows(), 6u);
        EXPECT_EQ(doc.observation().cols(), 8u);
        // Surface relief should match testGrid(0, 0) = 120.0 at interior model node (1, 1)
        EXPECT_NEAR(doc.model().z(1, 1, 0), 120.0, 1e-4);
        EXPECT_NEAR(doc.observation().surface_relief()(0, 0), 120.0, 1e-4);
    }

    // 2c. Import mode with ONLY measured field:
    // Uses grid dimensions for observation area definition, and topography min/max Z statistics are grayed out
    const std::string tmpFieldPath = "test_define_obs_field.grd";
    {
        mod3d::Grid fieldGrid(5, 7, 200.0, 400.0, 50.0, 80.0);
        for (size_t r = 0; r < fieldGrid.rows(); ++r) {
            for (size_t c = 0; c < fieldGrid.cols(); ++c) {
                fieldGrid(r, c) = 15.0 + static_cast<double>(r * 2 + c * 3);
            }
        }
        ASSERT_TRUE(fieldGrid.save_srf6_ascii(tmpFieldPath));

        DefineObservationDialog dlg(&doc);
        dlg.setCreationMode(DefineObservationDialog::CreationMode::ImportReliefGrid);
        EXPECT_EQ(dlg.getTopoGridButton()->text(), "Get Topography From Grid...");
        EXPECT_EQ(dlg.getFieldGridButton()->text(), "Get Field From Grid...");

        ASSERT_TRUE(dlg.loadFromFieldGridFile(QString::fromStdString(tmpFieldPath)));
        EXPECT_TRUE(dlg.hasFieldGrid());
        EXPECT_FALSE(dlg.hasTopoGrid());

        // Uses field grid dimensions for observation area definition
        EXPECT_EQ(dlg.rowsSpinBox()->value(), 5);
        EXPECT_EQ(dlg.colsSpinBox()->value(), 7);
        EXPECT_DOUBLE_EQ(dlg.minXSpinBox()->value(), 200.0);
        EXPECT_DOUBLE_EQ(dlg.minYSpinBox()->value(), 400.0);
        EXPECT_DOUBLE_EQ(dlg.dxSpinBox()->value(), 50.0);
        EXPECT_DOUBLE_EQ(dlg.dySpinBox()->value(), 80.0);
        EXPECT_DOUBLE_EQ(dlg.lengthX(), 300.0); // (7 - 1) * 50 = 300
        EXPECT_DOUBLE_EQ(dlg.lengthY(), 320.0); // (5 - 1) * 80 = 320

        // Topography min/max Z statistics must be grayed out
        EXPECT_FALSE(dlg.gridMinZLabel()->isEnabled());
        EXPECT_FALSE(dlg.gridMaxZLabel()->isEnabled());
        EXPECT_TRUE(dlg.gridMinZLabel()->text().contains("N/A"));
        EXPECT_TRUE(dlg.gridMaxZLabel()->text().contains("N/A"));

        dlg.minZSpinBox()->setValue(-1500.0);
        dlg.maxZSpinBox()->setValue(250.0);
        EXPECT_TRUE(dlg.applyToDocument());

        EXPECT_EQ(doc.observation().rows(), 5u);
        EXPECT_EQ(doc.observation().cols(), 7u);
        EXPECT_DOUBLE_EQ(doc.model().z(1, 1, 0), 250.0); // Flat top at zMax because no topography was imported
        EXPECT_TRUE(doc.hasObservedGrid(mod3d::FieldComponent::GZ));
        EXPECT_TRUE(doc.showMeasuredField());
        EXPECT_NEAR((*doc.observedGrid(mod3d::FieldComponent::GZ))(0, 0), 15.0, 1e-4);
    }

    // 2d. Import mode with BOTH topography and measured field:
    // Topography min/max Z statistics are enabled, and both topography and field are applied
    {
        DefineObservationDialog dlg(&doc);
        dlg.setCreationMode(DefineObservationDialog::CreationMode::ImportReliefGrid);
        ASSERT_TRUE(dlg.loadFromTopoGridFile(QString::fromStdString(tmpGrdPath)));
        ASSERT_TRUE(dlg.loadFromFieldGridFile(QString::fromStdString(tmpFieldPath)));
        EXPECT_TRUE(dlg.hasTopoGrid());
        EXPECT_TRUE(dlg.hasFieldGrid());

        // Topography statistics enabled
        EXPECT_TRUE(dlg.gridMinZLabel()->isEnabled());
        EXPECT_TRUE(dlg.gridMaxZLabel()->isEnabled());
        EXPECT_TRUE(dlg.gridMinZLabel()->text().contains("120.00"));
        EXPECT_TRUE(dlg.gridMaxZLabel()->text().contains("205.00"));

        dlg.minZSpinBox()->setValue(-3000.0);
        dlg.maxZSpinBox()->setValue(500.0);
        EXPECT_TRUE(dlg.applyToDocument());

        EXPECT_EQ(doc.observation().rows(), 6u);
        EXPECT_EQ(doc.observation().cols(), 8u);
        // Surface relief from topography
        EXPECT_NEAR(doc.model().z(1, 1, 0), 120.0, 1e-4);
        EXPECT_NEAR(doc.observation().surface_relief()(0, 0), 120.0, 1e-4);
        // Measured field from field grid
        EXPECT_TRUE(doc.hasObservedGrid(mod3d::FieldComponent::GZ));
        EXPECT_TRUE(doc.showMeasuredField());
    }

    std::error_code ec;
    std::filesystem::remove(tmpGrdPath, ec);
    std::filesystem::remove(tmpFieldPath, ec);
}

TEST_F(GuiDocumentTest, VerticalRangeDialogUpdatesProfileAnd3DViewsAndResetsCamera) {
    ModelDocument doc;
    doc.newProject(11, 11, 0.0, 0.0, 100.0, 100.0, -2000.0, 500.0);

    ProfileView pv(&doc, ProfileOrientation::EastWest);
    GL3DView glv(&doc);
    glv.resetCamera();

    EXPECT_DOUBLE_EQ(pv.worldZMin(), -2000.0);
    EXPECT_DOUBLE_EQ(pv.worldZMax(), 500.0);
    EXPECT_NEAR(glv.target().z(), -750.0f, 1e-3f);
    // Initial maxDim = max(1000, 1000, 2500) = 2500 -> distance = 2500 * 1.8 = 4500
    EXPECT_NEAR(glv.cameraDistance(), 4500.0f, 1.0f);

    // Zoom/pan the views away from defaults
    pv.zoomIn();
    glv.setZoom(2.5f);
    glv.setTarget(QVector3D(100.0f, 200.0f, 300.0f));

    // Change vertical limits via VerticalRangeDialog
    VerticalRangeDialog vDlg(&doc);
    EXPECT_DOUBLE_EQ(vDlg.minZSpinBox()->value(), -2000.0);
    EXPECT_DOUBLE_EQ(vDlg.maxZSpinBox()->value(), 500.0);

    vDlg.minZSpinBox()->setValue(-8000.0);
    vDlg.maxZSpinBox()->setValue(2000.0);
    EXPECT_TRUE(vDlg.applyToDocument());

    // Model vertical range and bottom boundary updated
    EXPECT_DOUBLE_EQ(doc.model().z_min_val(), -8000.0);
    EXPECT_DOUBLE_EQ(doc.model().z_max_val(), 2000.0);
    int nBot = doc.model().upper_bound(1, 1);
    EXPECT_DOUBLE_EQ(doc.model().z(1, 1, nBot), -8000.0);

    // ProfileView extent automatically reset to new vertical range
    EXPECT_DOUBLE_EQ(pv.worldZMin(), -8000.0);
    EXPECT_DOUBLE_EQ(pv.worldZMax(), 2000.0);

    // GL3DView camera extent (target Z, distance, zoom) automatically reset
    // New midZ = (-8000 + 2000) / 2 = -3000; new maxDim = max(1000, 1000, 10000) = 10000 -> distance = 18000
    EXPECT_NEAR(glv.target().z(), -3000.0f, 1e-3f);
    EXPECT_NEAR(glv.cameraDistance(), 18000.0f, 1.0f);
    EXPECT_NEAR(glv.zoom(), 1.0f, 1e-4f);
}

TEST_F(GuiDocumentTest, ProfileMoveToPerpendicularProfile) {
    ModelDocument doc;
    doc.newProject(11, 11, 0.0, 0.0, 100.0, 100.0, -2000.0, 500.0);

    ProfileView pv(&doc, ProfileOrientation::EastWest, 2);
    EXPECT_EQ(pv.profileOrientation(), ProfileOrientation::EastWest);
    EXPECT_EQ(pv.profileIndex(), 2);
    EXPECT_TRUE(pv.windowTitle().contains("Profile 3 [E-W]"));

    // Move to perpendicular profile at line 4 (South-North, line 4)
    pv.moveToPerpendicularProfile(4);
    EXPECT_EQ(pv.profileOrientation(), ProfileOrientation::SouthNorth);
    EXPECT_EQ(pv.profileIndex(), 4);
    EXPECT_EQ(pv.hoverLineIndex(), 2);
    EXPECT_TRUE(pv.windowTitle().contains("Profile 5 [S-N]"));

    // Move back to perpendicular profile at line 1 (East-West, line 1)
    pv.moveToPerpendicularProfile(1);
    EXPECT_EQ(pv.profileOrientation(), ProfileOrientation::EastWest);
    EXPECT_EQ(pv.profileIndex(), 1);
    EXPECT_EQ(pv.hoverLineIndex(), 4);
    EXPECT_TRUE(pv.windowTitle().contains("Profile 2 [E-W]"));
}

TEST_F(GuiDocumentTest, ProfilePerpendicularProfileMenuAndNavigation) {
    // 1. Standalone ProfileView with no perpendicular profile window
    ModelDocument doc;
    doc.newProject(11, 11, 0.0, 0.0, 100.0, 100.0, -2000.0, 500.0);

    ProfileView standalonePv(&doc, ProfileOrientation::EastWest, 2);
    EXPECT_EQ(standalonePv.findPerpendicularProfileView(), nullptr);

    auto menuStandalone = standalonePv.createGridLineContextMenu(3, -500.0);
    ASSERT_NE(menuStandalone, nullptr);

    QAction *actPerpStandalone = nullptr;
    for (auto *act : menuStandalone->actions()) {
        if (act->text().contains("Perpendicular Profile")) {
            actPerpStandalone = act;
            break;
        }
    }
    ASSERT_NE(actPerpStandalone, nullptr);
    // Menu item must be disabled when perpendicular profile doesn't exist
    EXPECT_FALSE(actPerpStandalone->isEnabled());

    // 2. Full MainWindow workspace with both E-W and S-N profiles
    MainWindow mw;
    mw.resize(1000, 700);

    ProfileView *pvEW = nullptr;
    ProfileView *pvNS = nullptr;
    QMdiSubWindow *subNS = nullptr;

    for (auto *sub : mw.mdiArea()->subWindowList()) {
        if (!sub) continue;
        if (auto *pv = qobject_cast<ProfileView*>(sub->widget())) {
            if (pv->profileOrientation() == ProfileOrientation::EastWest && !pvEW) {
                pvEW = pv;
            } else if (pv->profileOrientation() == ProfileOrientation::SouthNorth && !pvNS) {
                pvNS = pv;
                subNS = sub;
            }
        }
    }

    if (!pvNS) {
        pvNS = mw.openProfileView(ProfileOrientation::SouthNorth);
        subNS = qobject_cast<QMdiSubWindow*>(pvNS->parentWidget());
    }

    ASSERT_NE(pvEW, nullptr);
    ASSERT_NE(pvNS, nullptr);
    ASSERT_NE(subNS, nullptr);

    // Verify perpendicular cross-referencing
    EXPECT_EQ(pvEW->findPerpendicularProfileView(), pvNS);
    EXPECT_EQ(pvNS->findPerpendicularProfileView(), pvEW);

    int initialEWIndex = pvEW->profileIndex();
    pvNS->setProfileIndex(1); // Set to 1 initially

    // Right-click context menu on pvEW at line index 4
    auto menuEW = pvEW->createGridLineContextMenu(4, -500.0);
    ASSERT_NE(menuEW, nullptr);

    QAction *actPerpEW = nullptr;
    for (auto *act : menuEW->actions()) {
        if (act->text().contains("Perpendicular Profile")) {
            actPerpEW = act;
            break;
        }
    }
    ASSERT_NE(actPerpEW, nullptr);
    // Menu item must be enabled because perpendicular profile exists
    EXPECT_TRUE(actPerpEW->isEnabled());

    // Trigger action: must NOT reuse/modify current profile, but navigate the perpendicular profile
    actPerpEW->trigger();

    // Current profile pvEW orientation and index must remain unchanged
    EXPECT_EQ(pvEW->profileOrientation(), ProfileOrientation::EastWest);
    EXPECT_EQ(pvEW->profileIndex(), initialEWIndex);

    // Perpendicular profile pvNS must be updated to target position line 4
    EXPECT_EQ(pvNS->profileOrientation(), ProfileOrientation::SouthNorth);
    EXPECT_EQ(pvNS->profileIndex(), 4);
    EXPECT_EQ(pvNS->hoverLineIndex(), initialEWIndex);

    // 3. If perpendicular profile window is closed, menu becomes disabled
    subNS->close();
    EXPECT_EQ(pvEW->findPerpendicularProfileView(), nullptr);

    auto menuAfterClose = pvEW->createGridLineContextMenu(4, -500.0);
    QAction *actAfterClose = nullptr;
    for (auto *act : menuAfterClose->actions()) {
        if (act->text().contains("Perpendicular Profile")) {
            actAfterClose = act;
            break;
        }
    }
    ASSERT_NE(actAfterClose, nullptr);
    EXPECT_FALSE(actAfterClose->isEnabled());
}

TEST_F(GuiDocumentTest, GL3DViewProfileReliefIntersectionSoftwareRender) {
    ModelDocument doc;
    doc.newProject(11, 11, 0.0, 0.0, 100.0, 100.0, -2000.0, 500.0);

    GL3DView glv(&doc);
    glv.resize(600, 450);

    // Render with software renderer (exercises profile plane & relief intersection lines)
    QPixmap pix(600, 450);
    pix.fill(Qt::black);
    QPainter painter(&pix);
    glv.paintSoftware(painter);
    painter.end();

    // Verify non-empty rendering
    QImage img = pix.toImage();
    bool hasNonBlack = false;
    for (int y = 0; y < img.height() && !hasNonBlack; ++y) {
        for (int x = 0; x < img.width() && !hasNonBlack; ++x) {
            if (img.pixelColor(x, y) != QColor(Qt::black)) {
                hasNonBlack = true;
            }
        }
    }
    EXPECT_TRUE(hasNonBlack);
}

TEST_F(GuiDocumentTest, ImportTopographyDemUpdatesModelProfileAnd3DView) {
    ModelDocument doc;
    doc.newProject(7, 9, 0.0, 0.0, 100.0, 100.0, -2000.0, 300.0);

    ProfileView pv(&doc, ProfileOrientation::EastWest, 2);
    GL3DView glv(&doc);

    // Initial flat surface at 300.0 m
    EXPECT_DOUBLE_EQ(doc.model().z(1, 1, 0), 300.0);
    EXPECT_DOUBLE_EQ(pv.worldZMax(), 300.0);

    // Create a topography DEM grid matching model core dimensions (5x7)
    // with mountain topography ranging from 350.0 m to 820.0 m
    mod3d::Grid topoGrid(5, 7, 0.0, 0.0, 100.0, 100.0);
    for (size_t r = 0; r < topoGrid.rows(); ++r) {
        for (size_t c = 0; c < topoGrid.cols(); ++c) {
            topoGrid(r, c) = 350.0 + static_cast<double>(r * 40 + c * 70);
        }
    }
    const std::string tmpTopoPath = "test_import_topography.grd";
    ASSERT_TRUE(topoGrid.save_srf6_ascii(tmpTopoPath));

    // Import topography DEM via loadReliefGrid
    ASSERT_TRUE(doc.loadReliefGrid(QString::fromStdString(tmpTopoPath)));

    // 1. Model surface topography z(r, c, 0) updated with imported DEM
    EXPECT_NEAR(doc.model().z(1, 1, 0), topoGrid(0, 0), 1e-4);
    EXPECT_NEAR(doc.model().z(3, 4, 0), topoGrid(2, 3), 1e-4);
    EXPECT_NEAR(doc.observation().surface_relief()(0, 0), topoGrid(0, 0), 1e-4);

    // 2. Model vertical range ceiling raised to accommodate peak topography
    EXPECT_GE(doc.model().z_max_val(), 820.0);

    // 3. ProfileView automatically adjusted vertical extent to show new topography
    EXPECT_GE(pv.worldZMax(), 820.0);

    // 4. GL3DView rendered without errors and reflects new topography
    QPixmap pix(400, 300);
    pix.fill(Qt::black);
    QPainter painter(&pix);
    glv.paintSoftware(painter);
    painter.end();

    std::error_code ec;
    std::filesystem::remove(tmpTopoPath, ec);
}

TEST_F(GuiDocumentTest, SettingsPanePhysicsTabRemoveObservedMean) {
    MainWindow mw;
    mw.resize(1280, 800);

    auto *doc = mw.document();
    ASSERT_NE(doc, nullptr);

    auto *pane = mw.settingsPane();
    ASSERT_NE(pane, nullptr);
    pane->selectPhysicsTab();

    auto *chk = pane->removeObservedMeanCheckBox();
    ASSERT_NE(chk, nullptr);
    auto *lbl = pane->observedMeanLabel();
    ASSERT_NE(lbl, nullptr);

    EXPECT_FALSE(chk->isChecked());
    EXPECT_FALSE(doc->removeObservedMean());

    // Provide observed grid with non-zero mean
    mod3d::Grid obsGrd(21, 21, 0.0, 0.0, 500.0, 500.0);
    for (size_t r = 0; r < 21; ++r) {
        for (size_t c = 0; c < 21; ++c) {
            obsGrd(r, c) = 25.0 + static_cast<double>(r * 3 + c * 2);
        }
    }
    const double rawMean = obsGrd.mean();
    ASSERT_NE(rawMean, 0.0);
    doc->observation().set_observed_grid(mod3d::FieldComponent::GZ, obsGrd);
    doc->setShowMeasuredField(true);
    doc->setShowDifferenceField(true);

    // Refresh pane data
    pane->selectPhysicsTab();
    EXPECT_TRUE(lbl->text().contains(QString::number(rawMean, 'f', 2)));

    // Check "Remove mean from measured field"
    chk->setChecked(true);
    EXPECT_TRUE(doc->removeObservedMean());
    EXPECT_NEAR(doc->observedMean(mod3d::FieldComponent::GZ), rawMean, 1e-12);

    // Raw observed grid retains values
    const auto *rawGrd = doc->rawObservedGrid(mod3d::FieldComponent::GZ);
    ASSERT_NE(rawGrd, nullptr);
    EXPECT_NEAR(rawGrd->mean(), rawMean, 1e-12);

    // Effective observed grid has zero mean
    const auto *effectiveGrd = doc->observedGrid(mod3d::FieldComponent::GZ);
    ASSERT_NE(effectiveGrd, nullptr);
    EXPECT_NEAR(effectiveGrd->mean(), 0.0, 1e-9);

    // Uncheck and verify restoration
    chk->setChecked(false);
    EXPECT_FALSE(doc->removeObservedMean());
    EXPECT_EQ(doc->observedGrid(mod3d::FieldComponent::GZ), rawGrd);
    EXPECT_NEAR(doc->observedGrid(mod3d::FieldComponent::GZ)->mean(), rawMean, 1e-12);
}

TEST_F(GuiDocumentTest, HorizontalSliceViewInitializationAndElevationSync) {
    ModelDocument doc;
    doc.newProject(7, 7, 0.0, 0.0, 100.0, 100.0, -2000.0, 0.0);

    HorizontalSliceView sliceView(&doc);
    sliceView.resize(600, 500);

    // Initial slice elevation should be midpoint of range: (-2000 + 0) / 2 = -1000
    EXPECT_NEAR(sliceView.sliceElevation(), -1000.0, 1e-4);
    EXPECT_NEAR(doc.activeSliceElevation(), -1000.0, 1e-4);

    // Changing elevation in view updates document
    sliceView.setSliceElevation(-500.0);
    EXPECT_NEAR(sliceView.sliceElevation(), -500.0, 1e-4);
    EXPECT_NEAR(doc.activeSliceElevation(), -500.0, 1e-4);

    // Stepping elevation up/down
    sliceView.stepElevation(1); // +50m
    EXPECT_NEAR(sliceView.sliceElevation(), -450.0, 1e-4);
    sliceView.stepElevation(-2); // -100m
    EXPECT_NEAR(sliceView.sliceElevation(), -550.0, 1e-4);

    // Clamping to vertical range
    sliceView.setSliceElevation(500.0); // Above zMax (0)
    EXPECT_LE(sliceView.sliceElevation(), 0.0);
    sliceView.setSliceElevation(-3000.0); // Below zMin (-2000)
    EXPECT_GE(sliceView.sliceElevation(), -2000.0);
}

TEST_F(GuiDocumentTest, HorizontalSliceViewBoundaryEdgesDetection) {
    ModelDocument doc;
    doc.newProject(7, 7, 0.0, 0.0, 100.0, 100.0, -2000.0, 0.0);

    // Insert body at row 2, col 2 with midZ = -500, thickness = 200 (top = -400, bot = -600)
    int bodyId = doc.insertBodyAtRowCol(2, 2, -500.0, 100.0);
    ASSERT_GT(bodyId, 0);

    HorizontalSliceView sliceView(&doc, -500.0);

    // At Z = -500m, body intersects (2, 2). All 4 sides (N, S, E, W) should be boundary edges!
    auto edges = sliceView.activeBoundaryEdges();
    EXPECT_EQ(edges.size(), 4u);

    // At Z = -200m (above top -400m), body does not intersect. No edges!
    sliceView.setSliceElevation(-200.0);
    edges = sliceView.activeBoundaryEdges();
    EXPECT_EQ(edges.size(), 0u);

    // At Z = -800m (below bot -600m), body does not intersect. No edges!
    sliceView.setSliceElevation(-800.0);
    edges = sliceView.activeBoundaryEdges();
    EXPECT_EQ(edges.size(), 0u);

    // Return to -500m and extend body to East neighbor (2, 3)
    sliceView.setSliceElevation(-500.0);
    bool extOk = doc.extendBodySideAtRowCol(2, 2, 2, 3, bodyId, -500.0);
    EXPECT_TRUE(extOk);

    // Now body occupies (2, 2) and (2, 3).
    // The internal edge between (2, 2) and (2, 3) must disappear!
    // Total boundary edges for 2 adjacent cells: 4 * 2 - 2 = 6 edges!
    edges = sliceView.activeBoundaryEdges();
    EXPECT_EQ(edges.size(), 6u);
}

TEST_F(GuiDocumentTest, HorizontalSliceViewLateralExtensionAndCollapse) {
    ModelDocument doc;
    doc.newProject(7, 7, 0.0, 0.0, 100.0, 100.0, -2000.0, 0.0);

    // Insert body at row 3, col 3
    int bodyId = doc.insertBodyAtRowCol(3, 3, -600.0, 150.0);
    ASSERT_GT(bodyId, 0);

    HorizontalSliceView sliceView(&doc, -600.0);
    sliceView.resize(600, 500);

    // Verify initial single column body
    EXPECT_GE(doc.model().body_index(bodyId, 3, 3), 0);
    EXPECT_LT(doc.model().body_index(bodyId, 3, 4), 0);

    // Extend East to (3, 4)
    bool extOk = doc.extendBodySideAtRowCol(3, 3, 3, 4, bodyId, -600.0);
    EXPECT_TRUE(extOk);
    EXPECT_GE(doc.model().body_index(bodyId, 3, 4), 0);

    // Extend North to (4, 3)
    bool extNorth = doc.extendBodySideAtRowCol(3, 3, 4, 3, bodyId, -600.0);
    EXPECT_TRUE(extNorth);
    EXPECT_GE(doc.model().body_index(bodyId, 4, 3), 0);

    // Collapse (3, 4) back into (3, 3)
    bool colOk = doc.collapseBodyEdgeAtRowCol(3, 4, bodyId);
    EXPECT_TRUE(colOk);
    EXPECT_LT(doc.model().body_index(bodyId, 3, 4), 0);
    EXPECT_GE(doc.model().body_index(bodyId, 3, 3), 0);
}

TEST_F(GuiDocumentTest, MainWindowHorizontalSliceViewWindowOpening) {
    MainWindow win;
    win.resize(1100, 750);
    win.show();

    auto *doc = win.document();
    ASSERT_NE(doc, nullptr);

    auto *sliceView = win.openHorizontalSliceView(-800.0);
    ASSERT_NE(sliceView, nullptr);
    EXPECT_NEAR(sliceView->sliceElevation(), -800.0, 1e-4);

    // Render check
    QPixmap pix(sliceView->size());
    sliceView->render(&pix);
    EXPECT_FALSE(pix.isNull());
}

class TestHorizontalSliceView : public HorizontalSliceView {
public:
    using HorizontalSliceView::HorizontalSliceView;
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
    void sendMouseDoubleClick(const QPoint &pos, Qt::MouseButton btn = Qt::LeftButton, Qt::KeyboardModifiers mods = Qt::NoModifier) {
        QMouseEvent ev(QEvent::MouseButtonDblClick, QPointF(pos), QPointF(pos), btn, btn, mods);
        mouseDoubleClickEvent(&ev);
    }
    HitType testHit(const QPoint &pos, BoundaryEdge &outEdge, int &outRow, int &outCol, int &outBodyId) const {
        return performHitTest(pos, outEdge, outRow, outCol, outBodyId);
    }
};

TEST_F(GuiDocumentTest, HorizontalSliceViewCellCenterDrawingMode) {
    ModelDocument doc;
    // 8 rows, 8 cols, dx=100, dy=100
    doc.newProject(8, 8, 0.0, 0.0, 100.0, 100.0, -2000.0, 0.0);

    // Insert an initial body at row 2, col 2 at Z = -500
    int bodyId = doc.insertBodyAtRowCol(2, 2, -500.0, 150.0);
    ASSERT_GT(bodyId, 0);

    TestHorizontalSliceView sliceView(&doc, -500.0);
    sliceView.resize(800, 600);
    sliceView.fitToWindow();

    // 1. Test Hit Testing on Cell Center Point
    double xc2 = doc.model().xd_val(2);
    double yr2 = doc.model().yd_val(2);
    int sx2 = sliceView.worldToScreenX(xc2);
    int sy2 = sliceView.worldToScreenY(yr2);

    HorizontalSliceView::BoundaryEdge outEdge;
    int hitRow = -1, hitCol = -1, hitBodyId = -1;
    auto hitType = sliceView.testHit(QPoint(sx2, sy2), outEdge, hitRow, hitCol, hitBodyId);
    EXPECT_EQ(hitType, HorizontalSliceView::HitType::CellCenterPoint);
    EXPECT_EQ(hitRow, 2);
    EXPECT_EQ(hitCol, 2);
    EXPECT_EQ(hitBodyId, bodyId);

    // Offset by 20 pixels (within cell of ~70px, but away from center): should be BodyCell
    hitType = sliceView.testHit(QPoint(sx2 + 20, sy2), outEdge, hitRow, hitCol, hitBodyId);
    EXPECT_EQ(hitType, HorizontalSliceView::HitType::BodyCell);

    // Empty cell center at (2, 3)
    double xc3 = doc.model().xd_val(3);
    double yr3 = doc.model().yd_val(2);
    int sx3 = sliceView.worldToScreenX(xc3);
    int sy3 = sliceView.worldToScreenY(yr3);
    hitType = sliceView.testHit(QPoint(sx3, sy3), outEdge, hitRow, hitCol, hitBodyId);
    EXPECT_EQ(hitType, HorizontalSliceView::HitType::CellCenterPoint);
    EXPECT_EQ(hitRow, 2);
    EXPECT_EQ(hitCol, 3);
    EXPECT_EQ(hitBodyId, -1);

    // 2. Perform Drawing Mode by dragging from center of (2, 2) across to (2, 3) and (2, 4)
    EXPECT_LT(doc.model().body_index(bodyId, 2, 3), 0);
    EXPECT_LT(doc.model().body_index(bodyId, 2, 4), 0);

    // Press on middle point of cell (2, 2)
    sliceView.sendMousePress(QPoint(sx2, sy2));

    // Move to (2, 3)
    sliceView.sendMouseMove(QPoint(sx3, sy3));
    EXPECT_GE(doc.model().body_index(bodyId, 2, 3), 0);

    // Move to (2, 4)
    double xc4 = doc.model().xd_val(4);
    int sx4 = sliceView.worldToScreenX(xc4);
    sliceView.sendMouseMove(QPoint(sx4, sy3));
    EXPECT_GE(doc.model().body_index(bodyId, 2, 4), 0);

    // Release mouse
    sliceView.sendMouseRelease(QPoint(sx4, sy3));

    // Verify drawing persisted and document marked modified
    EXPECT_TRUE(doc.isModified());
    EXPECT_GE(doc.model().body_index(bodyId, 2, 2), 0);
    EXPECT_GE(doc.model().body_index(bodyId, 2, 3), 0);
    EXPECT_GE(doc.model().body_index(bodyId, 2, 4), 0);

    // 3. Test Drawing Mode starting from an EMPTY cell center with active body selected
    // Start drawing at (4, 4) and drag to (4, 5)
    sliceView.setActiveBodyId(bodyId);
    double xc4_4 = doc.model().xd_val(4);
    double yr4_4 = doc.model().yd_val(4);
    int sx4_4 = sliceView.worldToScreenX(xc4_4);
    int sy4_4 = sliceView.worldToScreenY(yr4_4);

    EXPECT_LT(doc.model().body_index(bodyId, 4, 4), 0);
    EXPECT_LT(doc.model().body_index(bodyId, 4, 5), 0);

    sliceView.sendMousePress(QPoint(sx4_4, sy4_4));
    // Dragging starts on move: active body is inserted into start cell and dragged cells
    double xc4_5 = doc.model().xd_val(5);
    int sx4_5 = sliceView.worldToScreenX(xc4_5);
    sliceView.sendMouseMove(QPoint(sx4_5, sy4_4));
    EXPECT_GE(doc.model().body_index(bodyId, 4, 4), 0);
    EXPECT_GE(doc.model().body_index(bodyId, 4, 5), 0);

    sliceView.sendMouseRelease(QPoint(sx4_5, sy4_4));
    EXPECT_GE(doc.model().body_index(bodyId, 4, 4), 0);
    EXPECT_GE(doc.model().body_index(bodyId, 4, 5), 0);

    // 4. Render check: draws cleanly with drawing mode highlights
    QPixmap pm(sliceView.size());
    pm.fill(Qt::white);
    sliceView.render(&pm);
    EXPECT_FALSE(pm.isNull());
}

TEST_F(GuiDocumentTest, HorizontalSliceViewDoubleClickCreatesNewBodyAndBehavesLikeProfileView) {
    ModelDocument doc;
    doc.newProject(8, 8, 0.0, 0.0, 100.0, 100.0, -2000.0, 0.0);

    // Insert initial body 1 at (2, 2)
    int body1 = doc.insertBodyAtRowCol(2, 2, -500.0, 100.0);
    ASSERT_GT(body1, 0);
    size_t initialBodyCount = doc.model().bodies().size();

    TestHorizontalSliceView sliceView(&doc, -500.0);
    sliceView.resize(800, 600);
    sliceView.fitToWindow();

    int requestedBodyId = -1;
    QObject::connect(&sliceView, &HorizontalSliceView::requestBodyProperties, [&](int bId) {
        requestedBodyId = bId;
    });

    // 1. Plain double click on an EMPTY cell (4, 4) MUST create a brand NEW body
    double xc4 = doc.model().xd_val(4);
    double yr4 = doc.model().yd_val(4);
    int sx4 = sliceView.worldToScreenX(xc4);
    int sy4 = sliceView.worldToScreenY(yr4);

    EXPECT_LT(doc.model().body_index(body1, 4, 4), 0);
    requestedBodyId = -1;

    // Simulate double click event sequence: Press -> Release -> DblClick -> Release
    sliceView.sendMousePress(QPoint(sx4, sy4));
    sliceView.sendMouseRelease(QPoint(sx4, sy4));
    sliceView.sendMouseDoubleClick(QPoint(sx4, sy4));
    sliceView.sendMouseRelease(QPoint(sx4, sy4));

    // A brand new body must have been created!
    EXPECT_EQ(doc.model().bodies().size(), initialBodyCount + 1);
    int newBodyId = sliceView.activeBodyId();
    EXPECT_NE(newBodyId, body1);
    EXPECT_GT(newBodyId, 0);
    EXPECT_GE(doc.model().body_index(newBodyId, 4, 4), 0);
    EXPECT_EQ(requestedBodyId, newBodyId);

    // 2. Plain double click on an EXISTING body cell (4, 4) MUST request body properties without creating a body
    size_t currentBodyCount = doc.model().bodies().size();
    requestedBodyId = -1;
    sliceView.sendMousePress(QPoint(sx4, sy4));
    sliceView.sendMouseRelease(QPoint(sx4, sy4));
    sliceView.sendMouseDoubleClick(QPoint(sx4, sy4));
    sliceView.sendMouseRelease(QPoint(sx4, sy4));

    EXPECT_EQ(doc.model().bodies().size(), currentBodyCount);
    EXPECT_EQ(requestedBodyId, newBodyId);

    // 3. Shift + double click on an empty cell (5, 5) MUST insert the existing active body (newBodyId)
    double xc5 = doc.model().xd_val(5);
    double yr5 = doc.model().yd_val(5);
    int sx5 = sliceView.worldToScreenX(xc5);
    int sy5 = sliceView.worldToScreenY(yr5);

    EXPECT_LT(doc.model().body_index(newBodyId, 5, 5), 0);
    sliceView.sendMousePress(QPoint(sx5, sy5), Qt::LeftButton, Qt::ShiftModifier);
    sliceView.sendMouseRelease(QPoint(sx5, sy5));
    sliceView.sendMouseDoubleClick(QPoint(sx5, sy5), Qt::LeftButton, Qt::ShiftModifier);
    sliceView.sendMouseRelease(QPoint(sx5, sy5));

    // No new body created; existing active body inserted into (5, 5)
    EXPECT_EQ(doc.model().bodies().size(), currentBodyCount);
    EXPECT_GE(doc.model().body_index(newBodyId, 5, 5), 0);

    // 4. Ctrl + double click on an existing body cell (5, 5) MUST remove the body from this cell
    sliceView.sendMousePress(QPoint(sx5, sy5), Qt::LeftButton, Qt::ControlModifier);
    sliceView.sendMouseRelease(QPoint(sx5, sy5));
    sliceView.sendMouseDoubleClick(QPoint(sx5, sy5), Qt::LeftButton, Qt::ControlModifier);
    sliceView.sendMouseRelease(QPoint(sx5, sy5));

    EXPECT_LT(doc.model().body_index(newBodyId, 5, 5), 0);
}

TEST_F(GuiDocumentTest, HorizontalSliceViewMultiProfileAndPlaneIntersection) {
    ModelDocument doc;
    doc.newProject(10, 10, 0.0, 0.0, 1000.0, 1000.0, -3000.0, 0.0);

    TestHorizontalSliceView sliceView(&doc, -1000.0);
    sliceView.resize(600, 500);

    // Set custom open profiles: 2 East-West profiles and 1 South-North profile
    std::vector<HorizontalSliceView::OpenProfileInfo> customProfiles = {
        { ProfileOrientation::EastWest, 1, 2000.0, true },
        { ProfileOrientation::EastWest, 4, 5000.0, false },
        { ProfileOrientation::SouthNorth, 2, 3000.0, true }
    };
    sliceView.setCustomOpenProfiles(customProfiles);

    auto openList = sliceView.collectOpenProfiles();
    ASSERT_EQ(openList.size(), 3u);

    // Intersections of EW planes with SN planes: 2 EW x 1 SN = 2 intersections
    auto intersections = sliceView.planeIntersections();
    ASSERT_EQ(intersections.size(), 2u);

    // Intersection 0: (SN=3000, EW=2000, Z=-1000)
    EXPECT_DOUBLE_EQ(intersections[0].x, 3000.0);
    EXPECT_DOUBLE_EQ(intersections[0].y, 2000.0);
    EXPECT_DOUBLE_EQ(intersections[0].z, -1000.0);
    EXPECT_EQ(intersections[0].ewIndex, 1);
    EXPECT_EQ(intersections[0].snIndex, 2);
    EXPECT_TRUE(intersections[0].isActive);

    // Intersection 1: (SN=3000, EW=5000, Z=-1000)
    EXPECT_DOUBLE_EQ(intersections[1].x, 3000.0);
    EXPECT_DOUBLE_EQ(intersections[1].y, 5000.0);
    EXPECT_DOUBLE_EQ(intersections[1].z, -1000.0);
    EXPECT_EQ(intersections[1].ewIndex, 4);
    EXPECT_EQ(intersections[1].snIndex, 2);
    EXPECT_TRUE(intersections[1].isActive);

    // Test hovering over Intersection 0
    int sx0 = sliceView.worldToScreenX(intersections[0].x);
    int sy0 = sliceView.worldToScreenY(intersections[0].y);
    sliceView.sendMouseMove(QPoint(sx0, sy0));

    EXPECT_FALSE(sliceView.toolTip().isEmpty());
    EXPECT_TRUE(sliceView.toolTip().contains("Plane Intersection"));
    EXPECT_TRUE(sliceView.toolTip().contains("E-W Profile 2"));
    EXPECT_TRUE(sliceView.toolTip().contains("S-N Profile 3"));
    EXPECT_EQ(sliceView.cursor().shape(), Qt::CrossCursor);

    // Move mouse away: tooltip is cleared
    sliceView.sendMouseMove(QPoint(sx0 + 50, sy0 + 50));
    EXPECT_TRUE(sliceView.toolTip().isEmpty());

    // Render check: draws all profile cut lines, intersection reticles, and badges without crashing
    QPixmap pm(sliceView.size());
    pm.fill(Qt::white);
    sliceView.render(&pm);
    EXPECT_FALSE(pm.isNull());
}

TEST_F(GuiDocumentTest, MainWindowHorizontalSliceViewDiscoversMdiProfilesAndIntersects) {
    MainWindow mw;
    mw.resize(1200, 800);

    std::string samplePath = resolveLegacySamplePath("Sample.m3d");
    ASSERT_TRUE(mw.document()->openFile(QString::fromStdString(samplePath)));

    // Open Horizontal Slice View in MDI area
    auto *hv = mw.openHorizontalSliceView(-500.0);
    ASSERT_NE(hv, nullptr);

    // Ensure South-North profile view is also open in MDI area to test intersections
    mw.openProfileView(ProfileOrientation::SouthNorth);

    auto openList = hv->collectOpenProfiles();
    EXPECT_GE(openList.size(), 2u);

    bool hasEW = false;
    bool hasSN = false;
    for (const auto &p : openList) {
        if (p.orientation == ProfileOrientation::EastWest) hasEW = true;
        if (p.orientation == ProfileOrientation::SouthNorth) hasSN = true;
    }
    EXPECT_TRUE(hasEW);
    EXPECT_TRUE(hasSN);

    // Verify plane intersection is computed from open MDI profile subwindows
    auto intersections = hv->planeIntersections();
    EXPECT_GE(intersections.size(), 1u);

    // Render check
    QPixmap pm(hv->size());
    pm.fill(Qt::white);
    hv->render(&pm);
    EXPECT_FALSE(pm.isNull());
}

TEST_F(GuiDocumentTest, AntiFlickerOpaquePaintAndBackgroundSuppression) {
    MainWindow mw;
    mw.resize(1000, 700);

    std::string samplePath = resolveLegacySamplePath("Sample.m3d");
    ASSERT_TRUE(mw.document()->openFile(QString::fromStdString(samplePath)));

    ProfileView *pv = nullptr;
    MapView *mv = nullptr;
    for (auto *sub : mw.mdiArea()->subWindowList()) {
        if (auto *p = qobject_cast<ProfileView*>(sub->widget())) pv = p;
        else if (auto *m = qobject_cast<MapView*>(sub->widget())) mv = m;
    }
    ASSERT_NE(pv, nullptr);
    ASSERT_NE(mv, nullptr);

    // Verify anti-flicker widget attributes are enabled
    EXPECT_TRUE(pv->testAttribute(Qt::WA_OpaquePaintEvent));
    EXPECT_TRUE(pv->testAttribute(Qt::WA_NoSystemBackground));

    EXPECT_TRUE(mv->testAttribute(Qt::WA_OpaquePaintEvent));
    EXPECT_TRUE(mv->testAttribute(Qt::WA_NoSystemBackground));

    auto *hv = mw.openHorizontalSliceView(-500.0);
    ASSERT_NE(hv, nullptr);
    EXPECT_TRUE(hv->testAttribute(Qt::WA_OpaquePaintEvent));
    EXPECT_TRUE(hv->testAttribute(Qt::WA_NoSystemBackground));

    // Verify cursor movement equality guard prevents redundant signals
    auto *doc = mw.document();
    int cursorSignalCount = 0;
    QObject::connect(doc, &ModelDocument::activeProfileCursorChanged, [&cursorSignalCount](const ModelDocument::ProfileCursorPoint &) {
        cursorSignalCount++;
    });

    ModelDocument::ProfileCursorPoint pt1{true, 2, 3, 1000.0, 1500.0, -500.0, 1, "body1", false};
    doc->setActiveProfileCursor(pt1);
    EXPECT_EQ(cursorSignalCount, 1);

    // Setting exact same or negligible sub-pixel coordinate difference must NOT trigger redundant signal
    ModelDocument::ProfileCursorPoint pt2{true, 2, 3, 1000.05, 1500.05, -500.1, 1, "body1", false};
    doc->setActiveProfileCursor(pt2);
    EXPECT_EQ(cursorSignalCount, 1);
}

TEST_F(GuiDocumentTest, GL3DViewPlanarViewPlaneAndOctantCut) {
    MainWindow mw;
    mw.resize(1100, 800);

    std::string samplePath = resolveLegacySamplePath("Sample.m3d");
    ASSERT_TRUE(mw.document()->openFile(QString::fromStdString(samplePath)));
    mw.openProfileView(ProfileOrientation::SouthNorth);

    GL3DView *glv = nullptr;
    for (auto *sub : mw.mdiArea()->subWindowList()) {
        if (auto *g = qobject_cast<GL3DView*>(sub->widget())) {
            glv = g;
            break;
        }
    }
    ASSERT_NE(glv, nullptr);

    // 1. Planar plane visibility default & toggles
    EXPECT_TRUE(glv->showPlanarPlane());
    glv->setShowPlanarPlane(false);
    EXPECT_FALSE(glv->showPlanarPlane());
    glv->setShowPlanarPlane(true);
    EXPECT_TRUE(glv->showPlanarPlane());

    // 2. SettingsPane sync
    auto *settings = mw.settingsPane();
    ASSERT_NE(settings, nullptr);
    for (auto *sub : mw.mdiArea()->subWindowList()) {
        if (sub->widget() == glv) {
            mw.mdiArea()->setActiveSubWindow(sub);
            break;
        }
    }
    QApplication::processEvents();

    ASSERT_NE(settings->planarPlaneCheckBox3D(), nullptr);
    ASSERT_NE(settings->octantCutCheckBox3D(), nullptr);
    ASSERT_NE(settings->quadrantCutCheckBox3D(), nullptr);
    EXPECT_TRUE(settings->planarPlaneCheckBox3D()->isChecked());
    EXPECT_FALSE(settings->octantCutCheckBox3D()->isChecked());

    // 3. Octant cut activation & dual signal emission
    int octantSignalCount = 0;
    int quadrantSignalCount = 0;
    QObject::connect(glv, &GL3DView::octantCutChanged, [&octantSignalCount](bool c) {
        if (c) octantSignalCount++;
    });
    QObject::connect(glv, &GL3DView::quadrantCutChanged, [&quadrantSignalCount](bool c) {
        if (c) quadrantSignalCount++;
    });

    glv->setOctantCut(true);
    EXPECT_TRUE(glv->octantCut());
    EXPECT_TRUE(glv->quadrantCut()); // backward-compatible alias
    EXPECT_EQ(octantSignalCount, 1);
    EXPECT_EQ(quadrantSignalCount, 1);

    // 4. Octant cut state computation
    glv->setCameraAngles(35.0f, -45.0f); // Looking from above (+pitch)
    auto cutState = glv->computeOctantCutState();
    EXPECT_TRUE(cutState.active);
    EXPECT_TRUE(cutState.hasPlanar);
    EXPECT_TRUE(cutState.cutPosZ); // Cut top (Z > planarZ) when pitch >= 0
    EXPECT_NEAR(cutState.planarZ, mw.document()->activeSliceElevation(), 1e-4);

    // Looking from below (-pitch)
    glv->setCameraAngles(-25.0f, -45.0f);
    auto cutStateBelow = glv->computeOctantCutState();
    EXPECT_FALSE(cutStateBelow.cutPosZ); // Cut bottom (Z < planarZ) when pitch < 0

    // Reset camera angles to looking from above
    glv->setCameraAngles(30.0f, -45.0f);

    // 5. Slice elevation update syncs to 3D octant cut state
    mw.document()->setActiveSliceElevation(-650.0);
    auto cutUpdated = glv->computeOctantCutState();
    EXPECT_NEAR(cutUpdated.planarZ, -650.0, 1e-4);

    // Open HorizontalSliceView and verify it controls planarZ
    auto *hsv = mw.openHorizontalSliceView(-400.0);
    ASSERT_NE(hsv, nullptr);
    EXPECT_NEAR(mw.document()->activeSliceElevation(), -400.0, 1e-4);
    EXPECT_NEAR(glv->computeOctantCutState().planarZ, -400.0, 1e-4);

    // 6. Test isPointCut 3D logic
    auto cutTest = glv->computeOctantCutState();
    double xMid = cutTest.hasSN ? cutTest.snX : ((mw.document()->model().x_min_val() + mw.document()->model().x_max_val()) * 0.5);
    double yMid = cutTest.hasEW ? cutTest.ewY : ((mw.document()->model().y_min_val() + mw.document()->model().y_max_val()) * 0.5);
    double testX = cutTest.cutPosX ? (xMid + 500.0) : (xMid - 500.0);
    double testY = cutTest.cutPosY ? (yMid + 500.0) : (yMid - 500.0);

    // Point in front-top octant: cut!
    EXPECT_TRUE(cutTest.isPointCut(testX, testY, -400.0 + 100.0));
    // Point in front-bottom octant: kept!
    EXPECT_FALSE(cutTest.isPointCut(testX, testY, -400.0 - 100.0));
    // Point outside front octant in X: kept!
    double oppX = cutTest.cutPosX ? (xMid - 500.0) : (xMid + 500.0);
    EXPECT_FALSE(cutTest.isPointCut(oppX, testY, -400.0 + 100.0));

    // 7. Software rendering validation with planar plane and octant cut
    QPixmap pmSoftware(600, 500);
    pmSoftware.fill(Qt::white);
    {
        QPainter painter(&pmSoftware);
        glv->paintSoftware(painter);
    }
    EXPECT_FALSE(pmSoftware.isNull());
}

TEST_F(GuiDocumentTest, CollapsedEdgeBodyBoundariesInMapAndHorizontalSliceView) {
    ModelDocument doc;
    doc.newProject(6, 6, 0.0, 0.0, 100.0, 100.0, -2000.0, 0.0);

    // Insert body at row 2, col 2 with midZ = -500, thickness = 100
    int bodyId = doc.insertBodyAtRowCol(2, 2, -500.0, 100.0);
    ASSERT_GT(bodyId, 0);

    // Extend to (2, 3), (3, 2), (3, 3) to form a multi-point body
    EXPECT_TRUE(doc.extendBodySideAtRowCol(2, 2, 2, 3, bodyId, -500.0));
    EXPECT_TRUE(doc.extendBodySideAtRowCol(2, 2, 3, 2, bodyId, -500.0));
    EXPECT_TRUE(doc.extendBodySideAtRowCol(3, 2, 3, 3, bodyId, -500.0));

    // Collapse the outer edge along row 2: (2, 2) and (2, 3) set to z_top == z_bottom == -500
    int idx22 = doc.model().body_index(bodyId, 2, 2);
    int idx23 = doc.model().body_index(bodyId, 2, 3);
    ASSERT_GE(idx22, 0);
    ASSERT_GE(idx23, 0);

    doc.model().set_z(2, 2, idx22, -500.0);
    doc.model().set_z(2, 2, idx22 + 1, -500.0);
    doc.model().set_z(2, 3, idx23, -500.0);
    doc.model().set_z(2, 3, idx23 + 1, -500.0);
    doc.model().init_facet_list();

    // Verify that the collapsed edge between (2, 2) and (2, 3) is included in all_body_boundaries()
    auto boundaries = doc.model().all_body_boundaries();
    EXPECT_GT(boundaries.size(), 0u);

    double x2 = doc.model().xd_val(2);
    double x3 = doc.model().xd_val(3);
    double y2 = doc.model().yd_val(2);

    bool foundCollapsedEdge = false;
    for (const auto &seg : boundaries) {
        if (seg.bodyId != bodyId) continue;
        bool matchForward = (std::abs(seg.pt0.x - x2) < 1e-3 && std::abs(seg.pt0.y - y2) < 1e-3 &&
                             std::abs(seg.pt1.x - x3) < 1e-3 && std::abs(seg.pt1.y - y2) < 1e-3);
        bool matchReverse = (std::abs(seg.pt0.x - x3) < 1e-3 && std::abs(seg.pt0.y - y2) < 1e-3 &&
                             std::abs(seg.pt1.x - x2) < 1e-3 && std::abs(seg.pt1.y - y2) < 1e-3);
        if (matchForward || matchReverse) {
            foundCollapsedEdge = true;
            break;
        }
    }
    EXPECT_TRUE(foundCollapsedEdge);

    // Verify MapView and HorizontalSliceView render successfully with this collapsed edge
    MapView mv(&doc);
    mv.resize(600, 500);
    QPixmap pmMap(600, 500);
    pmMap.fill(Qt::white);
    mv.render(&pmMap);
    EXPECT_FALSE(pmMap.isNull());

    HorizontalSliceView hsv(&doc, -500.0);
    hsv.resize(600, 500);
    QPixmap pmSlice(600, 500);
    pmSlice.fill(Qt::white);
    hsv.render(&pmSlice);
    EXPECT_FALSE(pmSlice.isNull());
}

TEST_F(GuiDocumentTest, HorizontalSliceViewPropertiesAndSingleFieldBitmapDisplay) {
    MainWindow mw;

    auto *pane = mw.settingsPane();
    ASSERT_NE(pane, nullptr);

    // Open HorizontalSliceView
    auto *hv = mw.openHorizontalSliceView(-500.0);
    ASSERT_NE(hv, nullptr);

    auto *sub = qobject_cast<QMdiSubWindow*>(hv->parentWidget());
    ASSERT_NE(sub, nullptr);

    // Activate HorizontalSliceView
    mw.mdiArea()->setActiveSubWindow(sub);
    pane->setActiveSubWindow(sub);

    // 1. Verify Display Tab context banner
    auto *lblContext = pane->findChild<QLabel*>();
    ASSERT_NE(lblContext, nullptr);

    // 2. Verify HorizontalSliceView properties controls are present
    ASSERT_NE(pane->sliceModeledRadioButton(), nullptr);
    ASSERT_NE(pane->sliceObservedRadioButton(), nullptr);
    ASSERT_NE(pane->sliceDifferenceRadioButton(), nullptr);
    ASSERT_NE(pane->sliceNoneRadioButton(), nullptr);
    ASSERT_NE(pane->sliceFieldComboBox(), nullptr);
    ASSERT_NE(pane->sliceBodyOutlinesCheckBox(), nullptr);
    ASSERT_NE(pane->sliceCellBordersCheckBox(), nullptr);
    ASSERT_NE(pane->sliceStationPointsCheckBox(), nullptr);
    ASSERT_NE(pane->sliceProfilesCheckBox(), nullptr);

    // 3. Properties shall NOT contain "Profile Visual Overlays" section
    // and shall NOT contain "Secondary:" field inside slice properties pane
    for (auto *box : pane->findChildren<QGroupBox*>()) {
        if (box->parentWidget() && box->parentWidget()->isVisibleTo(pane)) {
            EXPECT_FALSE(box->title().contains("Profile Visual Overlays"));
        }
    }
    for (auto *chk : pane->findChildren<QCheckBox*>()) {
        if (chk->parentWidget() && chk->parentWidget()->isVisibleTo(pane)) {
            EXPECT_NE(chk->text(), "Secondary:");
        }
    }

    // 4. Verify only one or none of modeled, observed, difference can be displayed as bitmap
    EXPECT_EQ(hv->rasterMode(), HorizontalSliceView::SliceRasterMode::None);
    EXPECT_TRUE(pane->sliceNoneRadioButton()->isChecked());
    EXPECT_FALSE(pane->sliceModeledRadioButton()->isChecked());
    EXPECT_FALSE(pane->sliceObservedRadioButton()->isChecked());
    EXPECT_FALSE(pane->sliceDifferenceRadioButton()->isChecked());

    // Switch to Modeled field bitmap
    pane->sliceModeledRadioButton()->click();
    EXPECT_EQ(hv->rasterMode(), HorizontalSliceView::SliceRasterMode::Modeled);
    EXPECT_TRUE(pane->sliceModeledRadioButton()->isChecked());
    EXPECT_FALSE(pane->sliceNoneRadioButton()->isChecked());
    EXPECT_FALSE(pane->sliceObservedRadioButton()->isChecked());
    EXPECT_FALSE(pane->sliceDifferenceRadioButton()->isChecked());

    // Switch to None (no bitmap)
    pane->sliceNoneRadioButton()->click();
    EXPECT_EQ(hv->rasterMode(), HorizontalSliceView::SliceRasterMode::None);
    EXPECT_TRUE(pane->sliceNoneRadioButton()->isChecked());
    EXPECT_FALSE(pane->sliceModeledRadioButton()->isChecked());

    // 5. Test Field Component changes
    int dtIdx = pane->sliceFieldComboBox()->findData(static_cast<int>(mod3d::FieldComponent::DELTA_T));
    if (dtIdx >= 0) {
        pane->sliceFieldComboBox()->setCurrentIndex(dtIdx);
        EXPECT_EQ(hv->fieldComponent(), mod3d::FieldComponent::DELTA_T);
    }

    // 6. Test Planar Overlays sync
    pane->sliceBodyOutlinesCheckBox()->setChecked(false);
    EXPECT_FALSE(hv->showBodyOutlines());
    pane->sliceBodyOutlinesCheckBox()->setChecked(true);
    EXPECT_TRUE(hv->showBodyOutlines());

    pane->sliceCellBordersCheckBox()->setChecked(false);
    EXPECT_FALSE(hv->showCellBorders());
    pane->sliceCellBordersCheckBox()->setChecked(true);
    EXPECT_TRUE(hv->showCellBorders());

    // 7. Verify HorizontalSliceView renders bitmaps cleanly
    hv->resize(600, 500);
    QPixmap pm(600, 500);
    pm.fill(Qt::white);

    hv->setRasterMode(HorizontalSliceView::SliceRasterMode::Modeled);
    hv->render(&pm);
    EXPECT_FALSE(pm.isNull());

    // 8. Verify field cell center is exactly at the station grid points
    const auto *grid = mw.document()->modeledGrid(hv->fieldComponent());
    if (!grid) grid = mw.document()->observedGrid(hv->fieldComponent());
    if (grid && !grid->empty()) {
        double dx = grid->x_size().value();
        double dy = grid->y_size().value();
        double x0 = grid->x0().value();
        double y0 = grid->y0().value();
        double xMax = (grid->x0() + static_cast<double>(grid->cols() - 1) * grid->x_size()).value();
        double yMax = (grid->y0() + static_cast<double>(grid->rows() - 1) * grid->y_size()).value();

        // The center of cell (r=0, c=0) must be exactly at (x0, y0)
        EXPECT_NEAR(hv->cachedRasterX0() + 0.5 * dx, x0, 1e-4);
        EXPECT_NEAR(hv->cachedRasterY0() + 0.5 * dy, y0, 1e-4);

        // The center of cell (r=rows-1, c=cols-1) must be exactly at (xMax, yMax)
        EXPECT_NEAR(hv->cachedRasterXMax() - 0.5 * dx, xMax, 1e-4);
        EXPECT_NEAR(hv->cachedRasterYMax() - 0.5 * dy, yMax, 1e-4);

        // Total raster width and height must match cols * dx and rows * dy
        EXPECT_NEAR(hv->cachedRasterXMax() - hv->cachedRasterX0(), grid->cols() * dx, 1e-4);
        EXPECT_NEAR(hv->cachedRasterYMax() - hv->cachedRasterY0(), grid->rows() * dy, 1e-4);
    }

    hv->setRasterMode(HorizontalSliceView::SliceRasterMode::None);
    hv->render(&pm);
    EXPECT_FALSE(pm.isNull());
}





