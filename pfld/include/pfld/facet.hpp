#pragma once

#include <vector>
#include <array>
#include <cmath>
#include <limits>
#include <atomic>
#include <algorithm>
#include <numbers>
#include <span>
#include <concepts>
#include <initializer_list>
#include <utility>

#include "point.hpp"

namespace pfld {

namespace constants {
    template <typename T = double>
    inline constexpr T eps = static_cast<T>(1.0e-12);

    template <typename T = double>
    inline constexpr T pi = std::numbers::pi_v<T>;

    template <typename T = double>
    inline constexpr T two_pi = static_cast<T>(2.0) * std::numbers::pi_v<T>;

    template <typename T = double>
    inline constexpr T G = static_cast<T>(6.6738480e-11);
} // namespace constants

// Backwards-compatible constants in namespace pfld
inline constexpr double EPS = constants::eps<double>;
inline constexpr double PI = constants::pi<double>;
inline constexpr double PI2 = constants::two_pi<double>;
inline constexpr double GRAVCONST = constants::G<double>;

/**
 * @brief Computes the sign of a scalar value (-1, 0, or 1).
 */
template <typename T>
[[nodiscard]] constexpr T sign(T val) noexcept {
	return static_cast<T>((T(0) < val) - (val < T(0)));
}

/**
 * @brief Representation of an N-sided planar polygonal facet for 3D potential field calculations.
 *
 * Implements forward gravity and magnetic field calculation algorithms:
 *  - Nagy-Okabe/Vlado closed-form line-integral solution for homogeneous polyhedral facets.
 *  - Extended closed-form solution for linear density gradients: rho(r) = rho0 + ro * r.
 *  - Guptasarma & Singh (1999) solid angle and edge-integral method for gravity and magnetic fields.
 *
 * @tparam T Floating-point precision (default: double).
 */
template <std::floating_point T = double>
class Facet
{
public:
	enum class FacetType { normal, opposite, oposite = opposite };

	using value_type = T;
	using point = Point3D<T>;
	using valvec = std::vector<T>;
	using ptvec = std::vector<point>;
	using facetvec = std::vector<Facet<T>>;

#ifdef FIELD_ATOMIC_DOUBLE
	using double_pfld = std::atomic<T>;
#else
	using double_pfld = T;
#endif

	// --- Constructors and Special Member Functions (Rule of Zero / Five) ---
	constexpr Facet() noexcept = default;
	~Facet() = default;

	Facet(const Facet&) = default;
	Facet& operator=(const Facet&) = default;
	Facet(Facet&&) noexcept = default;
	Facet& operator=(Facet&&) noexcept = default;

	/**
	 * @brief Constructs a facet from a list of vertices.
	 * @param pts Point sequence defining the polygon perimeter.
	 * @param auto_init If true, automatically computes normal and edge geometry.
	 */
	explicit Facet(const ptvec& pts, bool auto_init = false) : _pts(pts) {
		if (auto_init) {
			Init();
		}
	}

	explicit Facet(ptvec&& pts, bool auto_init = false) noexcept : _pts(std::move(pts)) {
		if (auto_init) {
			Init();
		}
	}

	explicit Facet(std::span<const point> pts, bool auto_init = false) : _pts(pts.begin(), pts.end()) {
		if (auto_init) {
			Init();
		}
	}

	Facet(std::initializer_list<point> pts, bool auto_init = false) : _pts(pts) {
		if (auto_init) {
			Init();
		}
	}

	// --- Comparisons ---
	[[nodiscard]] bool operator==(const Facet& other) const noexcept {
		return _pts == other._pts && _n == other._n;
	}

	[[nodiscard]] bool is_parallel(const Facet& other) const noexcept {
		return _n == other._n;
	}

	// --- Functor Call Operators ---
	void operator()(const point& r, point& grv) const {
		Fld_G(r, grv);
	}

	void operator()(const point& r, double_pfld& g) const {
		Fld_Gz(r, g);
	}

	[[nodiscard]] point operator()(const point& r) const {
		return field_g(r);
	}

	// --- Initialization ---
	/**
	 * @brief Initializes geometry: unit normal vector, edge vectors, in-plane normals, and lengths.
	 */
	void Init();

	void Init(const ptvec& pts) {
		_pts = pts;
		_initialized = false;
		Init();
	}

	void Init(ptvec&& pts) {
		_pts = std::move(pts);
		_initialized = false;
		Init();
	}

	void Init(std::span<const point> pts) {
		_pts.assign(pts.begin(), pts.end());
		_initialized = false;
		Init();
	}

	void Init(const ptvec& pts, T densityCCW, T densityCW = T{0}) {
		(void)densityCCW;
		(void)densityCW;
		Init(pts);
	}

	// --- Accessors ---
	[[nodiscard]] ptvec& Data() noexcept { return _pts; }
	[[nodiscard]] const ptvec& Data() const noexcept { return _pts; }
	[[nodiscard]] ptvec& points() noexcept { return _pts; }
	[[nodiscard]] const ptvec& points() const noexcept { return _pts; }

	[[nodiscard]] const point& normal() const noexcept { return _n; }
	[[nodiscard]] size_t size() const noexcept { return _sz; }
	[[nodiscard]] bool empty() const noexcept { return _pts.empty(); }
	[[nodiscard]] bool is_initialized() const noexcept { return _initialized; }
	[[nodiscard]] int id() const noexcept { return _id; }
	void set_id(int id) noexcept { _id = id; }

	// --- Field Computation: Constant Density (Vlado / Nagy-Okabe method) ---
	/**
	 * @brief Computes 3D gravity attraction vector and accumulates into grv: grv += g.
	 */
	void Fld_G(const point& r, point& grv) const;

	/**
	 * @brief Computes vertical gravity component Gz and accumulates into g: g += gz.
	 */
	void Fld_Gz(const point& r, double_pfld& g) const;

	/**
	 * @brief Returns computed 3D gravity vector for constant unit density.
	 */
	[[nodiscard]] point field_g(const point& r) const;

	/**
	 * @brief Returns computed vertical gravity Gz for constant unit density.
	 */
	[[nodiscard]] T field_gz(const point& r) const;

	// --- Field Computation: Linear Density Gradient rho(r) = ro0 + ro * r ---
	/**
	 * @brief Computes 3D gravity vector under linear density and accumulates into grv: grv += g.
	 */
	void Fld_G(const point& r, const point& ro, const T& ro0, point& grv) const;

	/**
	 * @brief Computes vertical gravity Gz under linear density and accumulates into gz: gz += gz_val.
	 */
	void Fld_Gz(const point& r, const point& ro, const T& ro0, T& gz) const;

	[[nodiscard]] point field_g(const point& r, const point& ro, const T& ro0) const;
	[[nodiscard]] T field_gz(const point& r, const point& ro, const T& ro0) const;

	// --- Field Computation: Guptasarma & Singh (1999) Solid Angle Method ---
	/**
	 * @brief Computes magnetic and gravity fields simultaneously and accumulates them.
	 */
	void FldGS(const point& r, const point& M, point& mag, point& grv) const;

	/**
	 * @brief Computes magnetic field anomaly vector and accumulates into mag: mag += m.
	 */
	void FldGS_M(const point& r, const point& M, point& mag) const;

	/**
	 * @brief Computes gravity field vector via GS method and accumulates into grv: grv += g.
	 */
	void FldGS_G(const point& r, point& grv) const;

	/**
	 * @brief Computes vertical gravity Gz via GS method and accumulates into gz: gz += gz_val.
	 */
	void FldGS_Gz(const point& r, T& gz) const;

	[[nodiscard]] point field_gs_m(const point& r, const point& M) const;
	[[nodiscard]] point field_gs_g(const point& r) const;
	[[nodiscard]] T field_gs_gz(const point& r) const;

	// --- Solid Angle Calculation ---
	/**
	 * @brief Computes the solid angle subtended by the facet at the origin.
	 */
	[[nodiscard]] T SolidAngle(std::span<const point> pts) const {
		if (pts.size() < 2) return T{0};
		return SolidAngle(pts, _n * pts[1], _sz);
	}

	[[nodiscard]] T SolidAngle(const ptvec& pts) const {
		return SolidAngle(std::span<const point>(pts));
	}

	[[nodiscard]] static T SolidAngle(std::span<const point> pts, const T inOut, const size_t sz);

	[[nodiscard]] static T SolidAngle(const ptvec& pts, const T inOut, const size_t sz) {
		return SolidAngle(std::span<const point>(pts), inOut, sz);
	}

	// --- Gravity Gradient Tensor Computation ---
	void FldVladoGrd(const point& r, T refDensity,
	                 T& gxx, T& gyy, T& gzz,
	                 T& gxy, T& gxz, T& gyz,
	                 T densityOpos = T{0}, T density = T{1000.0}) const;

	// --- Edge and Vector Geometry Accessors ---
	[[nodiscard]] const ptvec& L() const noexcept { return _L; }
	[[nodiscard]] const ptvec& mi() const noexcept { return _mi; }
	[[nodiscard]] const ptvec& ni() const noexcept { return _ni; }
	[[nodiscard]] const valvec& edge_lengths() const noexcept { return _len; }

	void FldVlado(const point& r, T& f) const;
	[[nodiscard]] T FldVlado(const point& r) const;

	void FldGS(const point& r, point& f) const;
	[[nodiscard]] point FldGS(const point& r) const;

protected:
	bool   _initialized{false}; ///< True if normal, edge vectors, and lengths have been initialized
	size_t _sz{0};              ///< Number of vertices
	int    _id{-1};             ///< Facet identifier
	ptvec  _pts{};              ///< Polygon vertices (counter-clockwise orientation)
	ptvec  _L{};                ///< Edge vectors: L[i] = pts[i+1] - pts[i]
	ptvec  _mi{};               ///< Unit edge vectors: mi[i] = unit(L[i])
	ptvec  _ni{};               ///< In-plane outward unit normals: ni[i] = mi[i] x n
	point  _n{};                ///< Facet unit normal vector
	valvec _len{};              ///< Side lengths of edges
};


// Default double-precision aliases
using facet = Facet<double>;
using facet_vec = std::vector<facet>;
using facetvec = facet_vec;
using valvec = std::vector<double>;
using point = Point3D<double>;
using ptvec = std::vector<Point3D<double>>;
#ifdef FIELD_ATOMIC_DOUBLE
using double_pfld = std::atomic<double>;
#else
using double_pfld = double;
#endif

// --- Template Implementation ---

template <std::floating_point T>
void Facet<T>::Init()
{
	if (_initialized) {
		return;
	}
	_sz = _pts.size();
	if (_sz < 3) {
		_initialized = false;
		return;
	}

	_n = (_pts[0] - _pts[1]).cross(_pts[1] - _pts[2]);
	_n.Unit();

	_mi.resize(_sz);
	_ni.resize(_sz);
	_L.resize(_sz);
	_len.resize(_sz);

	for (size_t i = 0; i < _sz; ++i) {
		const size_t next = (i + 1 == _sz) ? 0 : i + 1;
		point edge = _pts[next] - _pts[i];
		_L[i] = edge;
		_len[i] = edge.norm();
		edge.Unit();
		_mi[i] = edge;
		_ni[i] = edge.cross(_n);
	}

	_initialized = true;
}

template <std::floating_point T>
void Facet<T>::FldVlado(const point& r, T& f) const
{
	if (_sz < 3) return;
	const T z = std::abs(_n * (_pts[0] - r)) + constants::eps<T>;

	for (size_t i = 0; i < _sz; ++i) {
		const point tmp = _pts[i] - r;
		const T u = _mi[i] * tmp;
		const T w = _ni[i] * tmp;
		const T v = u + _len[i];

		const T W2 = w * w + z * z;
		const T U = std::sqrt(u * u + W2);
		const T V = std::sqrt(v * v + W2);
		const T W = std::sqrt(W2);
		const T TT = U + V;
		const T d = _len[i];

		const T log_term = sign(v) * std::log((V + std::abs(v)) / W) -
		                   sign(u) * std::log((U + std::abs(u)) / W);
		const T atan_denom = (TT + d) * std::abs(TT - d) + static_cast<T>(2.0) * TT * z;
		const T atan_term = static_cast<T>(2.0) * z * std::atan((static_cast<T>(2.0) * w * d) / atan_denom);

		f += w * log_term - atan_term;
	}
	f *= constants::G<T>;
}

template <std::floating_point T>
T Facet<T>::FldVlado(const point& r) const
{
	T f{0};
	FldVlado(r, f);
	return f;
}

template <std::floating_point T>
void Facet<T>::Fld_G(const point& r, point& grv) const
{
	grv += _n * FldVlado(r);
}

template <std::floating_point T>
Point3D<T> Facet<T>::field_g(const point& r) const
{
	return _n * FldVlado(r);
}

template <std::floating_point T>
void Facet<T>::Fld_Gz(const point& r, double_pfld& g) const
{
	const T val = FldVlado(r) * _n.z;
	if constexpr (std::is_same_v<double_pfld, std::atomic<T>>) {
		g.fetch_add(val, std::memory_order_relaxed);
	} else {
		g += val;
	}
}

template <std::floating_point T>
T Facet<T>::field_gz(const point& r) const
{
	return FldVlado(r) * _n.z;
}

template <std::floating_point T>
void Facet<T>::Fld_Gz(const point& r, const point& ro, const T& ro0, T& gz) const
{
	if (_sz < 3) return;
	T f{0};
	const T ro_r = ro0 + ro * r;
	const T Z = _n * (_pts[0] - r);
	const T z = std::abs(Z) + constants::eps<T>;
	const T ronz = ro * _n * Z;

	for (size_t i = 0; i < _sz; ++i) {
		const point ptTmp1 = _pts[i] - r;
		const T u = _mi[i] * ptTmp1;
		const T v = u + _len[i];
		const T w = _ni[i] * ptTmp1;

		const T W2 = w * w + z * z;
		const T U = std::sqrt(u * u + W2);
		const T V = std::sqrt(v * v + W2);
		const T Tsum = U + V;
		const T d = _len[i];
		const T atan_denom = (Tsum + d) * std::abs(Tsum - d) + static_cast<T>(2.0) * Tsum * z;
		const T A = -std::atan((static_cast<T>(2.0) * w * d) / atan_denom);

		T L{0};
		if (sign(u) == sign(v)) {
			L = sign(v) * std::log((V + std::abs(v)) / (U + std::abs(u)));
		} else {
			L = std::log((V + std::abs(v)) * (U + std::abs(u)) / W2);
		}
		const T Fi = w * L + static_cast<T>(2.0) * z * A;
		const T Fi2 = d * static_cast<T>(0.25) * ((v + u) * (v + u) / Tsum + Tsum) + W2 * L * static_cast<T>(0.5);
		f += _n.z * (Fi * (ro_r + ronz) + ro * _ni[i] * Fi2) - ro.z * (Fi * Z * static_cast<T>(0.5));
	}
	f *= constants::G<T>;
	gz += f;
}

template <std::floating_point T>
T Facet<T>::field_gz(const point& r, const point& ro, const T& ro0) const
{
	T gz{0};
	Fld_Gz(r, ro, ro0, gz);
	return gz;
}

template <std::floating_point T>
void Facet<T>::Fld_G(const point& r, const point& ro, const T& ro0, point& grv) const
{
	if (_sz < 3) return;
	point f{};
	const T ro_r = ro0 + ro * r;
	const T Z = _n * (_pts[0] - r);
	const T z = std::abs(Z) + constants::eps<T>;
	const T ronz = ro * _n * Z;

	for (size_t i = 0; i < _sz; ++i) {
		const point ptTmp1 = _pts[i] - r;
		const T u = _mi[i] * ptTmp1;
		const T v = u + _len[i];
		const T w = _ni[i] * ptTmp1;

		const T W2 = w * w + z * z;
		const T U = std::sqrt(u * u + W2);
		const T V = std::sqrt(v * v + W2);
		const T Tsum = U + V;
		const T d = _len[i];
		const T atan_denom = (Tsum + d) * std::abs(Tsum - d) + static_cast<T>(2.0) * Tsum * z;
		const T A = -std::atan((static_cast<T>(2.0) * w * d) / atan_denom);

		T L{0};
		if (sign(u) == sign(v)) {
			L = sign(v) * std::log((V + std::abs(v)) / (U + std::abs(u)));
		} else {
			L = std::log((V + std::abs(v)) * (U + std::abs(u)) / W2);
		}
		const T Fi = w * L + static_cast<T>(2.0) * z * A;
		const T Fi2 = d * static_cast<T>(0.25) * ((v + u) * (v + u) / Tsum + Tsum) + W2 * L * static_cast<T>(0.5);
		f += _n * (Fi * (ro_r + ronz) + ro * _ni[i] * Fi2) - ro * (Fi * Z * static_cast<T>(0.5));
	}
	f = f * constants::G<T>;
	grv += f;
}

template <std::floating_point T>
Point3D<T> Facet<T>::field_g(const point& r, const point& ro, const T& ro0) const
{
	point grv{};
	Fld_G(r, ro, ro0, grv);
	return grv;
}

template <std::floating_point T>
void Facet<T>::FldGS(const point& r, const point& M, point& mag, point& grv) const
{
	const point f = FldGS(r);
	const T s = M * _n;
	mag += f * s;

	const T d = (_pts[0] - r) * _n;
	grv += f * d * constants::G<T>;
}

template <std::floating_point T>
void Facet<T>::FldGS_M(const point& r, const point& M, point& mag) const
{
	const point f = FldGS(r);
	mag += f * (M * _n);
}

template <std::floating_point T>
Point3D<T> Facet<T>::field_gs_m(const point& r, const point& M) const
{
	point mag{};
	FldGS_M(r, M, mag);
	return mag;
}

template <std::floating_point T>
void Facet<T>::FldGS_G(const point& r, point& grv) const
{
	const point f = FldGS(r);
	grv += f * ((_pts[0] - r) * _n) * constants::G<T>;
}

template <std::floating_point T>
Point3D<T> Facet<T>::field_gs_g(const point& r) const
{
	point grv{};
	FldGS_G(r, grv);
	return grv;
}

template <std::floating_point T>
void Facet<T>::FldGS_Gz(const point& r, T& gz) const
{
	const point f = FldGS(r);
	gz += f.z * ((_pts[0] - r) * _n) * constants::G<T>;
}

template <std::floating_point T>
T Facet<T>::field_gs_gz(const point& r) const
{
	T gz{0};
	FldGS_Gz(r, gz);
	return gz;
}

template <std::floating_point T>
void Facet<T>::FldGS(const point& r, point& f) const
{
	f = FldGS(r);
}

template <std::floating_point T>
Point3D<T> Facet<T>::FldGS(const point& r) const
{
	if (_sz < 3) {
		return point{};
	}

	constexpr size_t kStackLimit = 16;
	std::array<point, kStackLimit> stack_spts;
	std::vector<point> heap_spts;
	std::span<const point> spts;

	if (_sz <= kStackLimit) {
		for (size_t i = 0; i < _sz; ++i) {
			stack_spts[i] = _pts[i] - r;
		}
		spts = std::span<const point>(stack_spts.data(), _sz);
	} else {
		heap_spts.resize(_sz);
		for (size_t i = 0; i < _sz; ++i) {
			heap_spts[i] = _pts[i] - r;
		}
		spts = std::span<const point>(heap_spts.data(), _sz);
	}

	T P{0}, Q{0}, R{0};
	const T dOmega = SolidAngle(spts, _n * spts[1], _sz);

	for (size_t i = 0; i < _sz; ++i) {
		const T r_len = spts[i].norm();
		const T b = static_cast<T>(2.0) * (spts[i] * _L[i]);
		const T two_len = static_cast<T>(2.0) * _len[i];
		const T b_term = b / two_len;
		const T h = r_len + b_term;
		const T inv_len = static_cast<T>(1.0) / _len[i];

		T I{0};
		if (h > constants::eps<T>) {
			const T term = std::sqrt(_len[i] * _len[i] + b + r_len * r_len) + _len[i] + b_term;
			I = inv_len * std::log(term / h);
		} else {
			I = inv_len * std::log(std::abs(_len[i] - r_len) / r_len);
		}

		P += I * _L[i].x;
		Q += I * _L[i].y;
		R += I * _L[i].z;
	}

	return point{
		dOmega * _n.x + Q * _n.z - R * _n.y,
		dOmega * _n.y + R * _n.x - P * _n.z,
		dOmega * _n.z + P * _n.y - Q * _n.x
	};
}

template <std::floating_point T>
T Facet<T>::SolidAngle(std::span<const point> pts, const T inOut, const size_t sz)
{
	if (inOut == T{0} || sz < 3) {
		return T{0};
	}

	constexpr size_t kStackLimit = 32;
	std::array<const point*, kStackLimit> stack_pp;
	std::vector<const point*> heap_pp;
	const point** pp = nullptr;

	if (sz + 2 <= kStackLimit) {
		pp = stack_pp.data();
	} else {
		heap_pp.resize(sz + 2);
		pp = heap_pp.data();
	}

	pp[0] = &pts[sz - 1];
	for (size_t j = 0; j < sz; ++j) {
		pp[j + 1] = &pts[j];
	}
	pp[sz + 1] = &pts[0];

	if (inOut > T{0}) {
		std::reverse(pp, pp + (sz + 2));
	}

	T dFi = T{0};
	for (size_t i = 0; i < sz; ++i) {
		point n1 = pp[i + 1]->cross(*pp[i]);
		n1.Unit();
		point n2 = pp[i + 2]->cross(*pp[i + 1]);
		n2.Unit();
		const T dPerp = (*pp[i + 2]) * n1;
		const T b = std::clamp(n1 * n2, static_cast<T>(-1.0), static_cast<T>(1.0));
		T a = constants::pi<T> - std::acos(b);
		if (dPerp < T{0}) {
			a = constants::two_pi<T> - a;
		}
		dFi += a;
	}

	T Omega = dFi - static_cast<T>(sz - 2) * constants::pi<T>;
	if (inOut > T{0}) {
		Omega = -Omega;
	}
	return Omega;
}

template <std::floating_point T>
void Facet<T>::FldVladoGrd(const point& r, T refDensity,
                           T& gxx, T& gyy, T& gzz,
                           T& gxy, T& gxz, T& gyz,
                           T densityOpos, T density) const
{
	if (_sz < 3) return;
	const point* v_a = &_pts[0];
	const T Z = _n * (*v_a - r);
	const T z = std::abs(Z);
	const T e = sign(Z);

	point tmpFldGrd{0, 0, 0};
	for (size_t i = 0; i < _sz; ++i) {
		const point tmp = _pts[i] - r;
		const T u = _mi[i] * tmp;
		const T v = u + _len[i];
		const T w = _ni[i] * tmp;

		const T z_eps = z + constants::eps<T>;
		const T W2 = w * w + z_eps * z_eps;
		const T U = std::sqrt(u * u + W2);
		const T V = std::sqrt(v * v + W2);
		const T W = std::sqrt(W2);
		const T TT = U + V;
		const T d = _len[i];
		const T A = -std::atan((static_cast<T>(2.0) * w * d) / (TT * TT - (v - u) * (v - u) + static_cast<T>(2.0) * TT * z_eps));
		T L{0};
		if (sign(u) == sign(v)) {
			L = sign(v) * std::log((V + std::abs(v)) / (U + std::abs(u)));
		} else {
			L = std::log((V + std::abs(v)) * (U + std::abs(u)) / (W * W));
		}
		tmpFldGrd += _ni[i] * L + _n * (static_cast<T>(2.0) * e * A);
	}
	tmpFldGrd = tmpFldGrd * constants::G<T>;

	const T dens = (densityOpos != static_cast<T>(0.0)) ? (density - densityOpos) : (density - refDensity);

	gxx = dens * tmpFldGrd.x * _n.x;
	gyy = dens * tmpFldGrd.y * _n.y;
	gzz = dens * tmpFldGrd.z * _n.z;
	gyz = dens * static_cast<T>(0.5) * (tmpFldGrd.y * _n.z + tmpFldGrd.z * _n.y);
	gxy = dens * static_cast<T>(0.5) * (tmpFldGrd.x * _n.y + tmpFldGrd.y * _n.x);
	gxz = dens * static_cast<T>(0.5) * (tmpFldGrd.x * _n.z + tmpFldGrd.z * _n.x);
}

} // namespace pfld