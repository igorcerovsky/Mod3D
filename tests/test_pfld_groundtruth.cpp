#include <gtest/gtest.h>
#include "mod3d/Point3D.h"
#include "mod3d/Facet3Pt.h"
#include "mod3d/PotField.h"

#include <cmath>
#include <fstream>
#include <string>
#include <vector>
#include <limits>

using namespace mod3d;

namespace {

template<typename T>
bool almost_equal(T x, T y, int ulp = 2) {
    auto diff = std::abs(x - y);
    auto tol = std::numeric_limits<T>::epsilon() * std::abs(x + y) * std::pow(10, ulp);
    return diff < tol || diff < std::numeric_limits<T>::min();
}

void AssertPoints(const Point3D &pt1, const Point3D &pt2, double eps) {
    EXPECT_NEAR(pt1.x, pt2.x, eps);
    EXPECT_NEAR(pt1.y, pt2.y, eps);
    EXPECT_NEAR(pt1.z, pt2.z, eps);
}

bool LoadFacets(const std::string &path, std::vector<Facet3Pt> &facets, int maxCount) {
    std::ifstream in(path);
    if (!in.is_open()) return false;

    std::string tag;
    int id = 0, nPts = 0;
    while (in >> tag >> id >> nPts && static_cast<int>(facets.size()) < maxCount) {
        double x0, y0, z0, x1, y1, z1, x2, y2, z2;
        if (in >> x0 >> y0 >> z0 >> x1 >> y1 >> z1 >> x2 >> y2 >> z2) {
            Facet3Pt f(Point3D(x0, y0, z0), Point3D(x1, y1, z1), Point3D(x2, y2, z2));
            facets.push_back(f);
        }
    }
    return true;
}

bool LoadPoints(const std::string &path, std::vector<Point3D> &points, int maxCount) {
    std::ifstream in(path);
    if (!in.is_open()) return false;

    double x, y, z;
    while (in >> x >> y >> z && static_cast<int>(points.size()) < maxCount) {
        points.emplace_back(x, y, z);
    }
    return true;
}

bool LoadResults(const std::string &path, std::vector<double> &results, int maxCount) {
    std::ifstream in(path);
    if (!in.is_open()) return false;

    double val;
    while (in >> val && static_cast<int>(results.size()) < maxCount) {
        results.push_back(val);
    }
    return true;
}

} // namespace

// 1. Test_Body: Authentic tetrahedron test from pfld_UnitTest
TEST(PfldGroundTruthTest, Test_Body) {
    double a = 1000.0;
    std::vector<Point3D> v = {
        Point3D(0, 0, 0),
        Point3D(0, 0, -a),
        Point3D(a, 0, -a),
        Point3D(0, a, -a)
    };

    std::vector<std::vector<Point3D>> vf = {
        { v[0], v[1], v[2] },
        { v[0], v[2], v[3] },
        { v[0], v[3], v[1] },
        { v[1], v[3], v[2] }
    };

    std::vector<Facet3Pt> facets(4);
    for (size_t i = 0; i < 4; ++i) {
        facets[i].Init(vf[i]);
    }

    Point3D r(500, 500, 1);
    Point3D g(0, 0, 0);
    Point3D M(1, 10, 100), gGS(0, 0, 0), m(0, 0, 0);
    double gzV = 0.0, gzGS = 0.0;

    for (auto &fct : facets) {
        fct.Fld_G(r, g);
        fct.Fld_Gz(r, gzV);
        fct.FldGS(r, M, m, gGS);
        fct.FldGS_Gz(r, gzGS);
    }

    const Point3D expected(5.1341030021201644e-009, 5.1341030021201644e-009, 1.2401175118216113e-008);
    const double eps = 1.0e-16;

    AssertPoints(g, expected, eps);
    AssertPoints(gGS, expected, eps);

    EXPECT_NEAR(g.z, gzV, eps);
    EXPECT_NEAR(gGS.z, gzGS, eps);
}

// 2. Test_Facet: Single facet constant density test
TEST(PfldGroundTruthTest, Test_Facet) {
    const double eps = 1.0e-16;
    std::vector<Point3D> v = {
        Point3D(0, 0, -1000),
        Point3D(1000, 0, 0),
        Point3D(0, 1000, 0)
    };

    Facet3Pt fct;
    fct.Init(v);

    Point3D r(0, 0, 1);
    Point3D g(0, 0, 0);
    fct.Fld_G(r, g);

    const double resval = 4.8207079871718046e-008;
    Point3D expected(-resval, -resval, resval);

    AssertPoints(g, expected, eps);
}

// 3. Test_Facet_Lin0: Linear density formula with zero gradient
TEST(PfldGroundTruthTest, Test_Facet_Lin0) {
    const double eps = 1.0e-16;
    std::vector<Point3D> v = {
        Point3D(0, 0, -1000),
        Point3D(1000, 0, 0),
        Point3D(0, 1000, 0)
    };

    Facet3Pt fct;
    fct.Init(v);

    const double ro0 = 1.0;
    Point3D ro(0, 0, 0);
    Point3D r(0, 0, 1);
    Point3D g(0, 0, 0);
    fct.Fld_G(r, ro, ro0, g);

    const double resval = 4.8207079871718046e-008;
    Point3D expected(-resval, -resval, resval);

    AssertPoints(g, expected, eps);
}

// 4. Test_Facet_Lin: Linear density gradient non-zero test
TEST(PfldGroundTruthTest, Test_Facet_Lin) {
    const double eps = 1.0e-16;
    std::vector<Point3D> v = {
        Point3D(0, 0, -1000),
        Point3D(1000, 0, 0),
        Point3D(0, 1000, 0)
    };

    Facet3Pt fct;
    fct.Init(v);

    const double ro0 = 1000.0;
    Point3D ro(0, 0, 1.0);
    Point3D r(0, 0, 1.0);
    Point3D g(0, 0, 0);
    fct.Fld_G(r, ro, ro0, g);

    Point3D expected(-3.2142476436014269e-005, -3.2142476436014269e-005, 5.6270119911809142e-005);
    AssertPoints(g, expected, eps);

    double gz = 0.0;
    fct.Fld_Gz(r, ro, ro0, gz);
    EXPECT_NEAR(gz, expected.z, eps);
}

// 5. Ground truth dataset comparison against pfld_UnitTest (100 facets, 100 points)
TEST(PfldGroundTruthTest, Test_DatasetComparison_UnitTest100) {
    std::string facetsFile = std::string(PFLD_TEST_DATA_DIR) + "/pfld_facets.txt";
    std::string pointsFile = std::string(PFLD_TEST_DATA_DIR) + "/pfld_points.txt";
    std::string resultsFile = std::string(PFLD_TEST_DATA_DIR) + "/pfld_test_results.txt";

    const int maxFacets = 100;
    const int maxPoints = 100;

    std::vector<Facet3Pt> facets;
    ASSERT_TRUE(LoadFacets(facetsFile, facets, maxFacets)) << "Failed to load " << facetsFile;
    EXPECT_EQ(static_cast<int>(facets.size()), maxFacets);

    std::vector<Point3D> fldPts;
    ASSERT_TRUE(LoadPoints(pointsFile, fldPts, maxPoints)) << "Failed to load " << pointsFile;
    EXPECT_EQ(static_cast<int>(fldPts.size()), maxPoints);

    std::vector<double> expectedResults;
    ASSERT_TRUE(LoadResults(resultsFile, expectedResults, maxPoints)) << "Failed to load " << resultsFile;
    EXPECT_EQ(static_cast<int>(expectedResults.size()), maxPoints);

    // Compute Gz across points from facets
    std::vector<double> computedResults(fldPts.size(), 0.0);
    for (size_t p = 0; p < fldPts.size(); ++p) {
        double gz = 0.0;
        for (const auto &fct : facets) {
            fct.Fld_Gz(fldPts[p], gz);
        }
        computedResults[p] = gz;
    }

    // Verify each point against ground truth with 1e-12 precision
    for (size_t i = 0; i < expectedResults.size(); ++i) {
        EXPECT_NEAR(computedResults[i], expectedResults[i], 1e-12)
            << "Mismatch at point " << i
            << ": computed=" << computedResults[i]
            << ", expected=" << expectedResults[i];
    }
}

// 6. Ground truth dataset comparison against pfld_app (1000 facets, 1000 points)
TEST(PfldGroundTruthTest, Test_DatasetComparison_App1000) {
    std::string facetsFile = std::string(PFLD_APP_DATA_DIR) + "/pfld_facets.txt";
    std::string pointsFile = std::string(PFLD_APP_DATA_DIR) + "/pfld_points.txt";
    std::string resultsFile = std::string(PFLD_APP_DATA_DIR) + "/pfld_results.txt";

    const int maxFacets = 1000;
    const int maxPoints = 1000;

    std::vector<Facet3Pt> facets;
    ASSERT_TRUE(LoadFacets(facetsFile, facets, maxFacets)) << "Failed to load " << facetsFile;
    EXPECT_EQ(static_cast<int>(facets.size()), maxFacets);

    std::vector<Point3D> fldPts;
    ASSERT_TRUE(LoadPoints(pointsFile, fldPts, maxPoints)) << "Failed to load " << pointsFile;
    EXPECT_EQ(static_cast<int>(fldPts.size()), maxPoints);

    std::vector<double> expectedResults;
    ASSERT_TRUE(LoadResults(resultsFile, expectedResults, maxPoints)) << "Failed to load " << resultsFile;
    EXPECT_EQ(static_cast<int>(expectedResults.size()), maxPoints);

    // Compute Gz across points from facets
    std::vector<double> computedResults(fldPts.size(), 0.0);
    for (size_t p = 0; p < fldPts.size(); ++p) {
        double gz = 0.0;
        for (const auto &fct : facets) {
            fct.Fld_Gz(fldPts[p], gz);
        }
        computedResults[p] = gz;
    }

    // Verify each point against ground truth with 1e-12 precision
    for (size_t i = 0; i < expectedResults.size(); ++i) {
        EXPECT_NEAR(computedResults[i], expectedResults[i], 1e-12)
            << "Mismatch at point " << i
            << ": computed=" << computedResults[i]
            << ", expected=" << expectedResults[i];
    }
}
