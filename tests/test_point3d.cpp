#include <gtest/gtest.h>
#include "mod3d/Point3D.h"

using namespace mod3d;

TEST(Point3DTest, UnitVector) {
    Point3D pt1(0.0, 2.0, 0.0);
    pt1.Unit();
    EXPECT_DOUBLE_EQ(pt1.x, 0.0);
    EXPECT_DOUBLE_EQ(pt1.y, 1.0);
    EXPECT_DOUBLE_EQ(pt1.z, 0.0);

    Point3D pt2(3.0, 0.0, 0.0);
    pt2.Unit();
    EXPECT_DOUBLE_EQ(pt2.x, 1.0);
    EXPECT_DOUBLE_EQ(pt2.y, 0.0);
    EXPECT_DOUBLE_EQ(pt2.z, 0.0);

    Point3D pt3(0.0, 0.0, 4.0);
    pt3.Unit();
    EXPECT_DOUBLE_EQ(pt3.x, 0.0);
    EXPECT_DOUBLE_EQ(pt3.y, 0.0);
    EXPECT_DOUBLE_EQ(pt3.z, 1.0);
}

TEST(Point3DTest, DotAndCrossProduct) {
    Point3D a(1.0, 0.0, 0.0);
    Point3D b(0.0, 1.0, 0.0);

    // Dot product via operator*
    EXPECT_DOUBLE_EQ(a * b, 0.0);
    EXPECT_DOUBLE_EQ(a * a, 1.0);

    // Cross product via operator/ (legacy Mod3D convention)
    Point3D c = a / b;
    EXPECT_DOUBLE_EQ(c.x, 0.0);
    EXPECT_DOUBLE_EQ(c.y, 0.0);
    EXPECT_DOUBLE_EQ(c.z, 1.0);

    // Cross product via explicit method
    Point3D c2 = a.cross(b);
    EXPECT_EQ(c, c2);
}

TEST(Point3DTest, ArithmeticOperators) {
    Point3D p1(1.0, 2.0, 3.0);
    Point3D p2(4.0, 5.0, 6.0);

    Point3D pSum = p1 + p2;
    EXPECT_EQ(pSum, Point3D(5.0, 7.0, 9.0));

    Point3D pDiff = p2 - p1;
    EXPECT_EQ(pDiff, Point3D(3.0, 3.0, 3.0));

    Point3D pScaled = p1 * 2.0;
    EXPECT_EQ(pScaled, Point3D(2.0, 4.0, 6.0));

    Point3D pDiv = p2 / 2.0;
    EXPECT_EQ(pDiv, Point3D(2.0, 2.5, 3.0));
}
