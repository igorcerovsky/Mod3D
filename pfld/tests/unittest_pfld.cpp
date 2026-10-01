#include <gtest/gtest.h>
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <chrono>
#include <memory>
#include <limits>
#include <algorithm>

#include "pfld/facet.hpp"
#include "pfld/pfld_compute.hpp"
#include "pfld/pfld_test_io.h"

namespace {

template<class T>
typename std::enable_if<!std::numeric_limits<T>::is_integer, bool>::type
almost_equal(T x, T y, int ulp)
{
	return std::abs(x - y) < std::numeric_limits<T>::epsilon() * std::abs(x + y) * std::pow(10, ulp + 1)
		|| std::abs(x - y) < std::numeric_limits<T>::min();
}

template<typename T>
void AssertPoints(pfld::Point3D<T> pt1, pfld::Point3D<T> pt2, T eps)
{
	EXPECT_NEAR(static_cast<double>(pt1.x), static_cast<double>(pt2.x), static_cast<double>(eps));
	EXPECT_NEAR(static_cast<double>(pt1.y), static_cast<double>(pt2.y), static_cast<double>(eps));
	EXPECT_NEAR(static_cast<double>(pt1.z), static_cast<double>(pt2.z), static_cast<double>(eps));
}

void Compute(void(*FieldFn)(pfld::facetvec&, pfld::ptvec&, pfld::valvec&),
	pfld::facetvec& facets, pfld::ptvec& fldPoints, pfld::valvec& outFld,
	const std::string& message)
{
	(void)message;
	using namespace std::chrono;
	high_resolution_clock::time_point t1 = high_resolution_clock::now();
	FieldFn(facets, fldPoints, outFld);
	high_resolution_clock::time_point t2 = high_resolution_clock::now();
	duration<double> time_span = duration_cast<duration<double>>(t2 - t1);
	(void)time_span;
}

template<typename T>
void RunBodyTest(const T eps)
{
	using flt = T;
	using facet = pfld::Facet<flt>;
	using point = typename facet::point;
	using ptvec = typename facet::ptvec;
	facet fct;
	flt a{ 1000. };
	ptvec v{ point(0, 0, 0), point(0, 0, -a), point(a, 0, -a), point(0, a, -a) };
	std::vector<ptvec> vf{
		{ v[0], v[1], v[2] },
		{ v[0], v[2], v[3] },
		{ v[0], v[3], v[1] },
		{ v[1], v[3], v[2] } };
	std::vector<facet> facets(4);
	auto itVf = vf.begin();
	for (auto it = facets.begin(); it != facets.end(); ++it, ++itVf)
		it->Init(*itVf);

	point r(500, 500, 1), g;
	point M{ 1, 10, 100 }, gGS, m;
	flt gzGS{ 0. }, gzV{ 0. };
	for (auto it = facets.begin(); it != facets.end(); ++it)
	{
		it->Fld_G(r, g);
		it->Fld_Gz(r, gzV);
		it->FldGS(r, M, m, gGS);
		it->FldGS_Gz(r, gzGS);
	}

	const point result(T(5.1341030021201644e-009),
		T(5.1341030021201644e-009),
		T(1.2401175118216113e-008));
	AssertPoints(g, result, eps);
	AssertPoints(gGS, result, eps);
	AssertPoints(g, result, eps);
	EXPECT_NEAR(static_cast<double>(g.z), static_cast<double>(gGS.z), static_cast<double>(eps));
	EXPECT_NEAR(static_cast<double>(gGS.z), static_cast<double>(gzGS), static_cast<double>(eps));
	EXPECT_TRUE(almost_equal(g.z, gzV, 2));
	EXPECT_TRUE(almost_equal(gGS.z, gzGS, 2));
}

TEST(PfldTest, Test_Point3D_Unit)
{
	using point = pfld::Point3D<double>;
	point pt1(0.0, 2.0, 0.0);
	pt1.Unit();
	EXPECT_TRUE(pt1 == point(0, 1, 0));
	point pt2(3.0, 0.0, 0.0);
	pt2.Unit();
	EXPECT_TRUE(pt2 == point(1, 0, 0));
	point pt3(0.0, 0.0, 4.0);
	pt3.Unit();
	EXPECT_TRUE(pt3 == point(0, 0, 1));
}

TEST(PfldTest, Test_Body)
{
	RunBodyTest<double>(1.0e-16);
}

TEST(PfldTest, Test_Body_T_float)
{
	RunBodyTest<float>(1.0e-13f);
}

TEST(PfldTest, Test_Body_T_double)
{
	RunBodyTest<double>(1.0e-20);
}

TEST(PfldTest, Test_Facet)
{
	const auto eps = 1.0e-16;
	using point = pfld::Point3D<double>;
	using ptvec = pfld::ptvec;
	pfld::facet fct;
	ptvec v{ point(0, 0, -1000), point(1000, 0, 0), point(0, 1000, 0) };
	fct.Init(v);
	point r(0, 0, 1), g, g2, g3;
	fct.Fld_G(r, g);
	const double resval = 4.8207079871718046e-008;
	point result(-resval, -resval, resval);
	AssertPoints(g, result, eps);

	pfld::facet fctCopy(fct);
	fctCopy.Fld_G(r, g2);
	AssertPoints(g2, result, eps);

	pfld::facet fctAssign = fct;
	fctAssign.Fld_G(r, g3);
	AssertPoints(g3, result, eps);
}

TEST(PfldTest, Test_Facet_Lin0)
{
	using point = pfld::Point3D<double>;
	using ptvec = pfld::ptvec;
	pfld::facet fct;
	ptvec v{ point(0, 0, -1000), point(1000, 0, 0), point(0, 1000, 0) };
	const double ro0{ 1. };
	point ro{ 0., 0., 0. };
	const double resval = 4.8207079871718046e-008;
	const auto eps = 1.0e-16;
	point result(-resval, -resval, resval);

	fct.Init(v);
	point r(0, 0, 1), g, g2, g3;
	fct.Fld_G(r, ro, ro0, g);
	AssertPoints(g, result, eps);

	pfld::facet fctCopy(fct);
	fctCopy.Fld_G(r, g2);
	AssertPoints(g2, result, eps);

	pfld::facet fctAssign = fct;
	fctAssign.Fld_G(r, g3);
	AssertPoints(g3, result, eps);
}

TEST(PfldTest, Test_Facet_Lin)
{
	const auto eps = 1.0e-16;
	using point = pfld::Point3D<double>;
	using ptvec = pfld::ptvec;
	pfld::facet fct;
	const double ro0{ 1000. };
	point ro{ 0., 0., 1. };
	ptvec v{ point(0, 0, -1000), point(1000, 0, 0), point(0, 1000, 0) };
	fct.Init(v);
	point r(0, 0, 1), g, g2, g3;
	fct.Fld_G(r, ro, ro0, g);
	point result(-3.2142476436014269e-005, -3.2142476436014269e-005, 5.6270119911809142e-005);
	AssertPoints(g, result, eps);

	pfld::facet fctCopy(fct);
	fctCopy.Fld_G(r, ro, ro0, g2);
	AssertPoints(g2, result, eps);

	pfld::facet fctAssign = fct;
	fctAssign.Fld_G(r, ro, ro0, g3);
	AssertPoints(g3, result, eps);

	pfld::facet fctGz = fct;
	double gz = 0.0;
	fctGz.Fld_Gz(r, ro, ro0, gz);
	EXPECT_TRUE(almost_equal(gz, result.z, 2));
}

TEST(PfldTest, Test_Facet_Parallell)
{
	const char* file_facets = "test_data/pfld_facets.txt";
	const char* file_points = "test_data/pfld_points.txt";
	const char* file_results = "test_data/pfld_test_results.txt";
	const int max_facets_to_load = 100;
	const int max_points_to_load = 100;

	pfld::facetvec facets;
	pfld::GetFacets(facets, file_facets, max_facets_to_load, false);

	pfld::ptvec fldPts;
	pfld::GetFieldPoints(fldPts, file_points, max_points_to_load, false);
	void(*FieldFn)(pfld::facetvec&, pfld::ptvec&, pfld::valvec&);
	pfld::valvec outFld; // uninitialized for this version
	FieldFn = pfld::Field_Gz_;
	Compute(FieldFn, facets, fldPts, outFld, "computing facets parallel future approach...");

	pfld::valvec res;
	pfld::LoadResults(res, file_results, static_cast<int>(outFld.size()));
	auto itCmp = outFld.begin();
	for (auto it = res.begin(); it != res.end(); ++it, ++itCmp)
	{
		bool bEQ = almost_equal(*it, *itCmp, 2);
		EXPECT_TRUE(bEQ);
	}

	pfld::valvec outFld1(fldPts.size()); // uninitialized for this version
	FieldFn = pfld::Field_Gz;
	Compute(FieldFn, facets, fldPts, outFld1, "computing facets parallel approach...");
	itCmp = outFld1.begin();
	for (auto it = res.begin(); it != res.end(); ++it, ++itCmp)
	{
		bool bEQ = almost_equal(*it, *itCmp, 2);
		EXPECT_TRUE(bEQ);
	}
}

} // namespace