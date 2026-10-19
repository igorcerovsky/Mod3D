#include <gtest/gtest.h>
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <chrono>
#include <memory>
#include <limits>
#include <algorithm>
#include <numeric>

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
	pt1.normalize();
	EXPECT_TRUE(pt1 == point(0, 1, 0));
	point pt2(3.0, 0.0, 0.0);
	pt2.normalize();
	EXPECT_TRUE(pt2 == point(1, 0, 0));
	point pt3(0.0, 0.0, 4.0);
	pt3.normalize();
	EXPECT_TRUE(pt3 == point(0, 0, 1));
}

TEST(PfldTest, Test_Point3D_STL_Interface)
{
	using point = pfld::Point3D<double>;

	point p(3.0, 4.0, 12.0);
	EXPECT_DOUBLE_EQ(p.norm_squared(), 169.0);
	EXPECT_DOUBLE_EQ(p.length_squared(), 169.0);
	EXPECT_DOUBLE_EQ(p.norm(), 13.0);
	EXPECT_DOUBLE_EQ(p.length(), 13.0);
	EXPECT_DOUBLE_EQ(p.abs(), 13.0);

	// In-place normalize and normalized copy
	point p_copy = p.normalized();
	EXPECT_DOUBLE_EQ(p_copy.norm(), 1.0);
	EXPECT_DOUBLE_EQ(p_copy.x, 3.0 / 13.0);
	EXPECT_DOUBLE_EQ(p_copy.y, 4.0 / 13.0);
	EXPECT_DOUBLE_EQ(p_copy.z, 12.0 / 13.0);

	p.normalize();
	EXPECT_DOUBLE_EQ(p.norm(), 1.0);
	EXPECT_DOUBLE_EQ(p.x, 3.0 / 13.0);

	// Zero & Empty
	point zero_pt;
	EXPECT_TRUE(zero_pt.is_zero());
	EXPECT_TRUE(zero_pt.empty());
	zero_pt.offset(1.0, 2.0, 3.0);
	EXPECT_FALSE(zero_pt.is_zero());
	zero_pt.reset();
	EXPECT_TRUE(zero_pt.is_zero());

	// Sign manipulation
	point neg(-2.0, -3.0, 5.0);
	neg.turn_sign();
	EXPECT_EQ(neg, point(2.0, 3.0, -5.0));
	neg.negate();
	EXPECT_EQ(neg, point(-2.0, -3.0, 5.0));
	neg.positive();
	EXPECT_EQ(neg, point(2.0, 3.0, 5.0));

	// Distance and angle
	point p1(0.0, 0.0, 0.0);
	point p2(0.0, 3.0, 4.0);
	EXPECT_DOUBLE_EQ(p1.distance(p2), 5.0);
	EXPECT_DOUBLE_EQ(p2.angle_z(), 4.0 / 5.0);

	point px(1.0, 0.0, 0.0);
	point py(0.0, 1.0, 0.0);
	EXPECT_NEAR(px.angle(py), 3.14159265358979323846 / 2.0, 1e-12);
}

TEST(PfldTest, Test_Point3D_Container_Interface)
{
	using point = pfld::Point3D<double>;
	point p(1.0, 2.0, 3.0);

	// Size & max_size
	EXPECT_EQ(point::size(), 3u);
	EXPECT_EQ(p.size(), 3u);
	EXPECT_EQ(p.max_size(), 3u);

	// Subscript operator and at()
	EXPECT_DOUBLE_EQ(p[0], 1.0);
	EXPECT_DOUBLE_EQ(p[1], 2.0);
	EXPECT_DOUBLE_EQ(p[2], 3.0);
	EXPECT_DOUBLE_EQ(p.at(0), 1.0);
	EXPECT_DOUBLE_EQ(p.at(1), 2.0);
	EXPECT_DOUBLE_EQ(p.at(2), 3.0);
	EXPECT_THROW((void)p.at(3), std::out_of_range);

	// Front and back
	EXPECT_DOUBLE_EQ(p.front(), 1.0);
	EXPECT_DOUBLE_EQ(p.back(), 3.0);

	// Data pointer
	double* d = p.data();
	EXPECT_EQ(d, &p.x);
	EXPECT_DOUBLE_EQ(d[0], 1.0);
	EXPECT_DOUBLE_EQ(d[1], 2.0);
	EXPECT_DOUBLE_EQ(d[2], 3.0);

	// Range-based for loop & std::accumulate
	double sum = std::accumulate(p.begin(), p.end(), 0.0);
	EXPECT_DOUBLE_EQ(sum, 6.0);

	// Fill
	p.fill(7.5);
	EXPECT_EQ(p, point(7.5, 7.5, 7.5));

	// Structured binding
	point p_bind(10.0, 20.0, 30.0);
	auto [bx, by, bz] = p_bind;
	EXPECT_DOUBLE_EQ(bx, 10.0);
	EXPECT_DOUBLE_EQ(by, 20.0);
	EXPECT_DOUBLE_EQ(bz, 30.0);
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

TEST(PfldTest, Test_Facet_FixedArray_N3)
{
	using DynFacet = pfld::Facet<double, 0>;
	using FixedFacet = pfld::Facet<double, 3>;
	using point = pfld::Point3D<double>;

	static_assert(FixedFacet::is_fixed, "FixedFacet must report is_fixed = true");
	static_assert(FixedFacet::fixed_size == 3, "FixedFacet fixed_size must be 3");
	static_assert(!DynFacet::is_fixed, "DynFacet must report is_fixed = false");
	static_assert(!std::is_polymorphic_v<FixedFacet>, "FixedFacet must not be polymorphic");

	point p0(0.0, 0.0, 100.0);
	point p1(50.0, 0.0, 100.0);
	point p2(0.0, 50.0, 100.0);

	// Construct dynamic facet
	DynFacet fDyn({p0, p1, p2}, true);
	EXPECT_EQ(fDyn.size(), 3u);
	EXPECT_FALSE(fDyn.empty());

	// Construct fixed facet using (p0, p1, p2) constructor
	FixedFacet fFix(p0, p1, p2, true);
	EXPECT_EQ(fFix.size(), 3u);
	EXPECT_FALSE(fFix.empty());

	// Verify points and normals match exactly
	EXPECT_DOUBLE_EQ(fFix[0].x, fDyn[0].x);
	EXPECT_DOUBLE_EQ(fFix[1].y, fDyn[1].y);
	EXPECT_DOUBLE_EQ(fFix[2].z, fDyn[2].z);
	EXPECT_DOUBLE_EQ(fFix.normal().x, fDyn.normal().x);
	EXPECT_DOUBLE_EQ(fFix.normal().y, fDyn.normal().y);
	EXPECT_DOUBLE_EQ(fFix.normal().z, fDyn.normal().z);

	// Cross-type comparison operator
	EXPECT_TRUE(fFix == fDyn);

	// Evaluate forward gravity field at multiple observation points
	std::vector<point> obsPoints = {
		point(20.0, 20.0, 50.0),
		point(0.0, 0.0, 0.0),
		point(100.0, 100.0, 200.0),
		point(-50.0, 25.0, 150.0)
	};

	for (const auto& obs : obsPoints) {
		// Fld_G
		point gDyn(0, 0, 0), gFix(0, 0, 0);
		fDyn.Fld_G(obs, gDyn);
		fFix.Fld_G(obs, gFix);
		EXPECT_DOUBLE_EQ(gFix.x, gDyn.x);
		EXPECT_DOUBLE_EQ(gFix.y, gDyn.y);
		EXPECT_DOUBLE_EQ(gFix.z, gDyn.z);

		// Fld_Gz
		double gzDyn = 0.0, gzFix = 0.0;
		fDyn.Fld_Gz(obs, gzDyn);
		fFix.Fld_Gz(obs, gzFix);
		EXPECT_DOUBLE_EQ(gzFix, gzDyn);

		// FldGS (Guptasarma-Singh magnetic and gravity)
		point M(10.0, 20.0, 30.0);
		point magDyn(0, 0, 0), magFix(0, 0, 0);
		point grvDyn(0, 0, 0), grvFix(0, 0, 0);
		fDyn.FldGS(obs, M, magDyn, grvDyn);
		fFix.FldGS(obs, M, magFix, grvFix);
		EXPECT_DOUBLE_EQ(magFix.x, magDyn.x);
		EXPECT_DOUBLE_EQ(magFix.y, magDyn.y);
		EXPECT_DOUBLE_EQ(magFix.z, magDyn.z);
		EXPECT_DOUBLE_EQ(grvFix.x, grvDyn.x);
		EXPECT_DOUBLE_EQ(grvFix.y, grvDyn.y);
		EXPECT_DOUBLE_EQ(grvFix.z, grvDyn.z);

		// FldVladoGrd (gravity gradient tensor)
		double gxxDyn, gyyDyn, gzzDyn, gxyDyn, gxzDyn, gyzDyn;
		double gxxFix, gyyFix, gzzFix, gxyFix, gxzFix, gyzFix;
		fDyn.FldVladoGrd(obs, 2670.0, gxxDyn, gyyDyn, gzzDyn, gxyDyn, gxzDyn, gyzDyn, 0.0, 2670.0);
		fFix.FldVladoGrd(obs, 2670.0, gxxFix, gyyFix, gzzFix, gxyFix, gxzFix, gyzFix, 0.0, 2670.0);
		EXPECT_DOUBLE_EQ(gxxFix, gxxDyn);
		EXPECT_DOUBLE_EQ(gyyFix, gyyDyn);
		EXPECT_DOUBLE_EQ(gzzFix, gzzDyn);
		EXPECT_DOUBLE_EQ(gxyFix, gxyDyn);
		EXPECT_DOUBLE_EQ(gxzFix, gxzDyn);
		EXPECT_DOUBLE_EQ(gyzFix, gyzDyn);
	}
}

TEST(PfldUnitsTest, Point3DUnitConstructorsAndAccessors)
{
	using namespace pfld::units;
	using namespace pfld::units::literals;

	// Construct Point3D with mixed length units
	pfld::Point3D pt(1.5_km, 250_m, -50_m);
	EXPECT_DOUBLE_EQ(pt.x, 1500.0);
	EXPECT_DOUBLE_EQ(pt.y, 250.0);
	EXPECT_DOUBLE_EQ(pt.z, -50.0);

	// Unit accessors
	EXPECT_DOUBLE_EQ(pt.x_qty().value(), 1500.0);
	EXPECT_DOUBLE_EQ(pt.x_as<tag::Kilometers>().value(), 1.5);
	EXPECT_DOUBLE_EQ(pt.y_as<tag::Meters>().value(), 250.0);
	EXPECT_DOUBLE_EQ(pt.z_as<tag::Meters>().value(), -50.0);

	pfld::Point3D pt2(0.0_m, 250.0_m, -50.0_m);
	EXPECT_DOUBLE_EQ(pt.distance_to_qty(pt2).in<tag::Kilometers>(), 1.5);
}

TEST(PfldUnitsTest, FacetUnitAwareFieldEvaluation)
{
	using namespace pfld::units;
	using namespace pfld::units::literals;

	using point = pfld::Point3D<double>;
	std::vector<point> pts = { point(0, 0, 0), point(1000, 0, 0), point(0, 1000, 0) };
	pfld::Facet<double, 3> fct(pts, true);

	point obs(500, 500, 100);

	// gravity_z returning units::SI_Acceleration
	SI_Acceleration gz_si = fct.gravity_z(obs);
	EXPECT_DOUBLE_EQ(gz_si.value(), fct.field_gz(obs));

	// Convert to mGal and g.u.
	MGal gz_mgal = gz_si.as<tag::MilliGal>();
	EXPECT_DOUBLE_EQ(gz_mgal.value(), gz_si.value() * 1.0e5);

	GU gz_gu = gz_si.as<tag::GravityUnit>();
	EXPECT_DOUBLE_EQ(gz_gu.value(), gz_si.value() * 1.0e6);

	// Gravity with density contrast
	SI_Acceleration gz_dense = fct.gravity_z(obs, 2670.0_kg_m3);
	EXPECT_DOUBLE_EQ(gz_dense.value(), gz_si.value() * 2670.0);

	// Marussi Gravity gradient tensor with units
	auto tensor = fct.gravity_tensor(obs, 2670.0_kg_m3, 2670.0_kg_m3);
	EXPECT_NEAR(tensor.trace().value(), 0.0, 1e-6);

	// Magnetic field evaluation in nT
	point M(10.0, 20.0, 30.0);
	point mag_nt = fct.field_magnetic_nt(obs, M);
	point mag_raw = fct.field_gs_m(obs, M);
	EXPECT_DOUBLE_EQ(mag_nt.x, mag_raw.x * 1.0e9);
	EXPECT_DOUBLE_EQ(mag_nt.y, mag_raw.y * 1.0e9);
	EXPECT_DOUBLE_EQ(mag_nt.z, mag_raw.z * 1.0e9);
}

TEST(PfldUnitsTest, ParallelComputeGravityZUnits)
{
	using namespace pfld::units;
	using namespace pfld::units::literals;

	using point = pfld::Point3D<double>;
	std::vector<point> pts = { point(0, 0, 0), point(1000, 0, 0), point(0, 1000, 0) };
	std::vector<pfld::Facet<double, 3>> facets;
	facets.emplace_back(pts, true);

	std::vector<point> grid_pts = {
		point(100, 100, 50),
		point(200, 200, 50),
		point(300, 300, 50)
	};

	// Compute directly in mGal
	auto results_mgal = pfld::compute_gravity_z<tag::MilliGal>(facets, grid_pts);
	ASSERT_EQ(results_mgal.size(), grid_pts.size());

	pfld::valvec raw_gz(grid_pts.size(), 0.0);
	pfld::Field_Gz(facets, grid_pts, raw_gz);

	for (size_t i = 0; i < grid_pts.size(); ++i) {
		EXPECT_DOUBLE_EQ(results_mgal[i].value(), raw_gz[i] * 1.0e5);
	}

	// Compute with density contrast in g.u.
	auto results_gu = pfld::compute_gravity_z<tag::GravityUnit>(facets, grid_pts, 2.67_g_cm3);
	ASSERT_EQ(results_gu.size(), grid_pts.size());
	for (size_t i = 0; i < grid_pts.size(); ++i) {
		EXPECT_DOUBLE_EQ(results_gu[i].value(), raw_gz[i] * 2670.0 * 1.0e6);
	}
}

} // namespace