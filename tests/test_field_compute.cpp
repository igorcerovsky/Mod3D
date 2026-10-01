#include <gtest/gtest.h>
#include "mod3d/FieldCompute.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

using namespace mod3d;

namespace {

bool LoadFacets(const std::string &path, std::vector<Facet3Pt> &facets, int maxCount) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    std::vector<Point3D> tri(3);
    int count = 0;

    while (std::getline(file, line)) {
        if (line.empty() || line.find("Facet") != std::string::npos) {
            continue;
        }
        std::istringstream iss(line);
        std::vector<double> vals;
        double val;
        while (iss >> val) {
            vals.push_back(val);
        }
        if (vals.size() >= 9) {
            tri[0] = Point3D(vals[0], vals[1], vals[2]);
            tri[1] = Point3D(vals[3], vals[4], vals[5]);
            tri[2] = Point3D(vals[6], vals[7], vals[8]);
            Facet3Pt fct;
            fct.Init(tri);
            facets.push_back(fct);
            count++;
            if (count >= maxCount) break;
        }
    }
    return true;
}

bool LoadPoints(const std::string &path, std::vector<Point3D> &points, int maxCount) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    int count = 0;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        double x, y, z;
        if (iss >> x >> y >> z) {
            points.emplace_back(x, y, z);
            count++;
            if (count >= maxCount) break;
        }
    }
    return true;
}

bool LoadResults(const std::string &path, std::vector<double> &results, int maxCount) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    int count = 0;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        double val;
        if (iss >> val) {
            results.push_back(val);
            count++;
            if (count >= maxCount) break;
        }
    }
    return true;
}

} // anonymous namespace

TEST(FieldComputeTest, ParallelMatchesSerial_AndGroundTruthUnitTest) {
    std::string facetsFile = std::string(PFLD_TEST_DATA_DIR) + "/pfld_facets.txt";
    std::string pointsFile = std::string(PFLD_TEST_DATA_DIR) + "/pfld_points.txt";
    std::string resultsFile = std::string(PFLD_TEST_DATA_DIR) + "/pfld_test_results.txt";

    const int maxFacets = 100;
    const int maxPoints = 200;

    std::vector<Facet3Pt> facets;
    ASSERT_TRUE(LoadFacets(facetsFile, facets, maxFacets));
    ASSERT_EQ(static_cast<int>(facets.size()), maxFacets);

    std::vector<Point3D> points;
    ASSERT_TRUE(LoadPoints(pointsFile, points, maxPoints));
    ASSERT_EQ(static_cast<int>(points.size()), maxPoints);

    std::vector<double> expected;
    ASSERT_TRUE(LoadResults(resultsFile, expected, maxPoints));
    ASSERT_EQ(static_cast<int>(expected.size()), maxPoints);

    std::vector<double> gzSerial;
    FieldCompute::ComputeGzSerial(facets, points, gzSerial);

    std::vector<double> gzParallel;
    FieldCompute::ComputeGzParallel(facets, points, gzParallel);

    ASSERT_EQ(gzSerial.size(), gzParallel.size());

    for (size_t i = 0; i < gzSerial.size(); ++i) {
        // Serial and Parallel must match identically
        EXPECT_DOUBLE_EQ(gzSerial[i], gzParallel[i]);

        // Both must match ground truth to 1e-12
        EXPECT_NEAR(gzParallel[i], expected[i], 1e-12);
    }
}

TEST(FieldComputeTest, ParallelMatchesGroundTruthApp1000) {
    std::string facetsFile = std::string(PFLD_APP_DATA_DIR) + "/pfld_facets.txt";
    std::string pointsFile = std::string(PFLD_APP_DATA_DIR) + "/pfld_points.txt";
    std::string resultsFile = std::string(PFLD_APP_DATA_DIR) + "/pfld_results.txt";

    const int maxFacets = 1000;
    const int maxPoints = 1000;

    std::vector<Facet3Pt> facets;
    ASSERT_TRUE(LoadFacets(facetsFile, facets, maxFacets));
    ASSERT_EQ(static_cast<int>(facets.size()), maxFacets);

    std::vector<Point3D> points;
    ASSERT_TRUE(LoadPoints(pointsFile, points, maxPoints));
    ASSERT_EQ(static_cast<int>(points.size()), maxPoints);

    std::vector<double> expected;
    ASSERT_TRUE(LoadResults(resultsFile, expected, maxPoints));
    ASSERT_EQ(static_cast<int>(expected.size()), maxPoints);

    std::vector<double> gzParallel;
    FieldCompute::ComputeGzParallel(facets, points, gzParallel);

    ASSERT_EQ(gzParallel.size(), expected.size());

    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_NEAR(gzParallel[i], expected[i], 1e-12)
            << "Mismatch at index " << i
            << ": computed=" << gzParallel[i]
            << ", expected=" << expected[i];
    }
}

TEST(FieldComputeTest, ComputeG_VectorMatchesParallelAndSerial) {
    std::string facetsFile = std::string(PFLD_TEST_DATA_DIR) + "/pfld_facets.txt";
    std::string pointsFile = std::string(PFLD_TEST_DATA_DIR) + "/pfld_points.txt";

    const int maxFacets = 50;
    const int maxPoints = 100;

    std::vector<Facet3Pt> facets;
    ASSERT_TRUE(LoadFacets(facetsFile, facets, maxFacets));
    std::vector<Point3D> points;
    ASSERT_TRUE(LoadPoints(pointsFile, points, maxPoints));

    std::vector<Point3D> gSerial;
    FieldCompute::ComputeGSerial(facets, points, gSerial);

    std::vector<Point3D> gParallel;
    FieldCompute::ComputeGParallel(facets, points, gParallel);

    ASSERT_EQ(gSerial.size(), gParallel.size());

    for (size_t i = 0; i < gSerial.size(); ++i) {
        EXPECT_DOUBLE_EQ(gSerial[i].x, gParallel[i].x);
        EXPECT_DOUBLE_EQ(gSerial[i].y, gParallel[i].y);
        EXPECT_DOUBLE_EQ(gSerial[i].z, gParallel[i].z);
    }
}
