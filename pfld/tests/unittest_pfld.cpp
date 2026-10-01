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
#include "pfld_test_io.h"

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

TEST(PfldTest, Test_Facet_ModernAPI)
{
	using point = pfld::Point3D<double>;
	const auto eps = 1.0e-15;

	// 1. Move semantics test
	static_assert(std::is_nothrow_move_constructible_v<pfld::facet>);
	static_assert(std::is_nothrow_move_assignable_v<pfld::facet>);

	// 2. Direct construction with initializer_list and auto_init
	const pfld::facet const_fct({ point(0, 0, -1000), point(1000, 0, 0), point(0, 1000, 0) }, true);
	EXPECT_TRUE(const_fct.is_initialized());
	EXPECT_EQ(const_fct.size(), 3u);
	EXPECT_FALSE(const_fct.empty());

	// 3. Const-correctness and value-returning methods
	const point r(0, 0, 1);
	const point g_val = const_fct.field_g(r);
	const double gz_val = const_fct.field_gz(r);
	const double resval = 4.8207079871718046e-008;
	const point expected(-resval, -resval, resval);
	AssertPoints(g_val, expected, eps);
	EXPECT_NEAR(gz_val, expected.z, eps);

	// 4. Functor call operator
	const point g_functor = const_fct(r);
	AssertPoints(g_functor, expected, eps);

	// 5. Value-returning linear density methods
	const double ro0 = 1000.0;
	const point ro(0, 0, 1);
	const point g_lin = const_fct.field_g(r, ro, ro0);
	const double gz_lin = const_fct.field_gz(r, ro, ro0);
	const point exp_lin(-3.2142476436014269e-005, -3.2142476436014269e-005, 5.6270119911809142e-005);
	AssertPoints(g_lin, exp_lin, eps);
	EXPECT_NEAR(gz_lin, exp_lin.z, eps);

	// 6. Guptasarma-Singh value-returning methods on const facet
	const point M(1, 10, 100);
	const point mag = const_fct.field_gs_m(r, M);
	const point g_gs = const_fct.field_gs_g(r);
	const double gz_gs = const_fct.field_gs_gz(r);
	EXPECT_NEAR(g_gs.z, gz_gs, eps);
	EXPECT_NE(mag.norm(), 0.0);

	// 7. Move constructor & assignment
	pfld::facet moved_src = const_fct;
	pfld::facet moved_dst(std::move(moved_src));
	EXPECT_TRUE(moved_dst.is_initialized());
	AssertPoints(moved_dst(r), expected, eps);

	pfld::facet assigned_dst;
	assigned_dst = std::move(moved_dst);
	EXPECT_TRUE(assigned_dst.is_initialized());
	AssertPoints(assigned_dst(r), expected, eps);

	// 8. Re-initialization bug fix test: initializing with new points
	pfld::facet reinit_fct({ point(0, 0, -1000), point(1000, 0, 0), point(0, 1000, 0) }, true);
	EXPECT_TRUE(reinit_fct.is_initialized());
	// Now reinit with horizontal facet at z = -500
	reinit_fct.Init(std::vector<point>{ point(0, 0, -500), point(1000, 0, -500), point(0, 1000, -500) });
	EXPECT_TRUE(reinit_fct.is_initialized());
	// The normal should now be vertical (0, 0, 1)
	AssertPoints(reinit_fct.normal(), point(0, 0, 1), 1e-12);

	// 9. Degenerate facet handling (fewer than 3 vertices)
	pfld::facet degenerate({ point(0, 0, 0), point(1, 0, 0) }, true);
	EXPECT_FALSE(degenerate.is_initialized());
	EXPECT_EQ(degenerate.field_g(r), point(0, 0, 0));
	EXPECT_EQ(degenerate.field_gz(r), 0.0);
}

TEST(PfldTest, Test_FieldCompute_ModernAPI)
{
	using point = pfld::Point3D<double>;
	const auto eps = 1.0e-15;

	pfld::facetvec facets{
		pfld::facet({ point(0, 0, -1000), point(1000, 0, 0), point(0, 1000, 0) }, true)
	};
	pfld::ptvec pts{
		point(0, 0, 1),
		point(100, 100, 50),
		point(-50, 200, 10)
	};

	// 1. Value-returning Field_Gz
	pfld::valvec gz_res = pfld::Field_Gz(facets, pts);
	ASSERT_EQ(gz_res.size(), pts.size());

	// 2. Pre-allocated Field_Gz
	pfld::valvec gz_prealloc(pts.size(), 0.0);
	pfld::Field_Gz(facets, pts, gz_prealloc);
	for (size_t i = 0; i < pts.size(); ++i) {
		EXPECT_NEAR(gz_res[i], gz_prealloc[i], eps);
	}

	// 3. Span-based Field_Gz
	pfld::valvec gz_span(pts.size(), 0.0);
	pfld::Field_Gz(std::span<const pfld::facet>(facets),
	               std::span<const point>(pts),
	               std::span<double>(gz_span));
	for (size_t i = 0; i < pts.size(); ++i) {
		EXPECT_NEAR(gz_span[i], gz_prealloc[i], eps);
	}

	// 4. Value-returning Field_G vs Serial Field_G
	pfld::ptvec g_res = pfld::Field_G(facets, pts);
	ASSERT_EQ(g_res.size(), pts.size());

	pfld::ptvec g_serial(pts.size());
	pfld::Field_G(facets, pts, g_serial);
	for (size_t i = 0; i < pts.size(); ++i) {
		AssertPoints(g_res[i], g_serial[i], eps);
		EXPECT_NEAR(g_res[i].z, gz_res[i], eps);
	}
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