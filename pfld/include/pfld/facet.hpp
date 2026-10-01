#ifndef FACET_HPP_
#define FACET_HPP_

#include <vector>
#include <array>
#include <cmath>
#include <limits>
#include <atomic>
#include <algorithm>

#include "point.hpp"

namespace pfld {

#define EPS 1.0e-12
#define PI 3.1415926535897932384626433832795
#define PI2 6.283185307179586476925286766559
#define GRAVCONST 6.6738480e-11

template <typename T>
inline T sign(T val) {
	return (T(0) < val) - (val < T(0));
}

template<typename T = double>
class Facet
{
	enum class FacetType { normal, oposite };

public:
	using valvec = std::vector<T>;
	using point = Point3D<T>;
	using ptvec = std::vector<Point3D<T>>;

#ifdef FIELD_ATOMIC_DOUBLE
	using double_pfld = std::atomic<T>;
#else
	using double_pfld = T;
#endif
	using facetvec = std::vector<Facet<T>>;

	virtual ~Facet() = default;
	Facet();
	Facet(const ptvec& pts);
	Facet(const Facet& fct);

	Facet& operator=(const Facet& fct);
	bool operator==(const Facet& fct) const;

	void operator()(const point& r, point& grv);
	void operator()(const point& r, double_pfld& g);

protected:
	bool   _initialized; // is initialized?
	size_t _sz;
	int    _id;    // facet ID
	ptvec  _pts;   // points
	ptvec  _L;     // array of vector l vectors; l = pts[i+1] - pts[i]
	ptvec  _mi;    // array of unit vector mi vectors; mi = Unit(pts[i+1] - pts[i])
	ptvec  _ni;    // array of unit ni vectors; ni = _mi[i] / _n;
	point  _n;     // facet unit normal vector
	valvec _len;   // array of side lengths

public:
	void Init();
	void Init(ptvec& pts);
	void Init(ptvec& pts, double densityCCW, double densityCW = 0.0);

	void Fld_G(const point &r, point& grv);
	void Fld_Gz(const point &r, double_pfld& g);
	void Fld_G(const point& r, const point& ro, const T& ro0, point& grv);
	void Fld_Gz(const point& r, const point& ro, const T& ro0, T& gz);

	ptvec& Data() { return _pts; }
	const ptvec& Data() const { return _pts; }

	void FldGS(const point& r, const point M, point& mag, point& grv);
	void FldGS_M(const point& r, const point M, point& mag);
	void FldGS_G(const point& r, point& grv);
	void FldGS_Gz(const point& r, T& gz);

	T SolidAngle(ptvec& pts) {
		return SolidAngle(pts, _n * pts[1], _sz);
	}
	static T SolidAngle(ptvec& pts, const T inOut, const size_t sz);

protected:
	void FldVlado(const point &r, T& f);
	void FldGS(const point& r, point& f);
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

template<typename T>
Facet<T>::Facet() :
	_initialized(false),
	_sz(0),
	_id(-1),
	_pts(),
	_L(),
	_mi(),
	_ni(),
	_n(),
	_len()
{
}

template<typename T>
Facet<T>::Facet(const ptvec& pts) : Facet()
{
	_pts.assign(pts.begin(), pts.end());
	_initialized = false;
}

template<typename T>
Facet<T>::Facet(const Facet& fct) :
	_initialized(fct._initialized),
	_sz(fct._sz),
	_id(fct._id),
	_pts(fct._pts),
	_L(fct._L),
	_mi(fct._mi),
	_ni(fct._ni),
	_n(fct._n),
	_len(fct._len)
{
}

template<typename T>
Facet<T>& Facet<T>::operator=(const Facet<T>& fct)
{
	if (this != &fct) {
		_initialized = fct._initialized;
		_sz = fct._sz;
		_id = fct._id;
		_pts = fct._pts;
		_L = fct._L;
		_mi = fct._mi;
		_ni = fct._ni;
		_n = fct._n;
		_len = fct._len;
	}
	return *this;
}

template<typename T>
bool Facet<T>::operator==(const Facet& fct) const
{
	return _n == fct._n;
}

template<typename T>
void Facet<T>::Init()
{
	if (_initialized)
		return;
	point::Cross(_pts[0] - _pts[1], _pts[1] - _pts[2], _n);
	_n.Unit();

	_sz = _pts.size();
	_mi.resize(_sz);
	_ni.resize(_sz);
	_L.resize(_sz);
	_len.resize(_sz);
	size_t i = 0;
	for (; i < _sz - 1; ++i)
	{
		point::Sub(_pts[i + 1], _pts[i], _mi[i]);
		_L[i] = _mi[i];
		_len[i] = _mi[i].Abs();
		_mi[i].Unit();
		point::Cross(_mi[i], _n, _ni[i]);
	}
	point::Sub(_pts[0], _pts[i], _mi[i]);
	_L[i] = _mi[i];
	_len[i] = _mi[i].Abs();
	_mi[i].Unit();
	point::Cross(_mi[i], _n, _ni[i]);

	_initialized = true;
}

template<typename T>
void Facet<T>::Init(ptvec& pts)
{
	_pts.assign(pts.begin(), pts.end());
	Init();
}

template<typename T>
void Facet<T>::Init(ptvec& pts, double densityCCW, double densityCW)
{
	(void)densityCCW;
	(void)densityCW;
	Init(pts);
}

template<typename T>
void Facet<T>::FldVlado(const point& r, T& f)
{
	T z, u, v, w, W2, W, U, V, TT;
	z = std::abs(_n * (_pts.at(0) - r)) + T(EPS);

	for (size_t i = 0; i < _sz; i++)
	{
		point tmp;
		point::Sub(_pts[i], r, tmp);
		u = _mi[i] * tmp;
		w = _ni[i] * tmp;
		v = u + _len[i];

		W2 = w*w + z*z;
		U = std::sqrt(u*u + W2);
		V = std::sqrt(v*v + W2);
		W = std::sqrt(W2);
		TT = U + V;
		f += w * (sign(v)*std::log((V + std::abs(v)) / W) - sign(u)*std::log((U + std::abs(u)) / W)) -
			2 * z*std::atan((2 * w *_len[i]) / ((TT + _len[i])*std::abs(TT - _len[i]) + 2 * TT*z));
	}
	f *= T(GRAVCONST);
}

template<typename T>
void Facet<T>::Fld_Gz(const point& r, const point& ro, const T& ro0, T& gz)
{
	T f{ 0.0 };
	const T ro_r{ ro0 + ro*r };

	T Z{ _n * (_pts[0] - r) };
	T z{ std::abs(Z) + T(EPS) };

	const T ronz{ ro*_n*Z };
	for (size_t i = 0; i < _sz; ++i)
	{
		T u, v, w, W2, U, V, Tsum, L, A, Fi, Fi2;
		point ptTmp1;
		point::Sub(_pts[i], r, ptTmp1);
		u = _mi[i] * ptTmp1;
		v = u + _len[i];
		w = _ni[i] * ptTmp1;

		W2 = w*w + z*z;
		U = std::sqrt(u*u + W2);
		V = std::sqrt(v*v + W2);
		Tsum = U + V;
		T& d = _len[i];
		A = -std::atan((2.0 * w*d) / ((Tsum + d)*std::abs(Tsum - d) + 2.0 * Tsum*z));
		if (sign(u) == sign(v)) {
			L = sign(v)*std::log((V + std::abs(v)) / (U + std::abs(u)));
		}
		else {
			L = std::log((V + std::abs(v))*(U + std::abs(u)) / W2);
		}
		Fi = w*L + 2.0 * z*A;
		Fi2 = d * 0.25 * ((v + u)*(v + u) / Tsum + Tsum) + W2*L * 0.5;
		f += _n.z*(Fi*(ro_r + ronz) + ro*_ni[i] * Fi2) - ro.z*(Fi*Z * 0.5);
	}
	f = f*T(GRAVCONST);
	gz += f;
}

template<typename T>
void Facet<T>::Fld_G(const point& r, const point& ro, const T& ro0, point& grv)
{
	point f;
	const T ro_r{ ro0 + ro*r };

	T Z{ _n * (_pts[0] - r) };
	T z{ std::abs(Z) + T(EPS) };

	const T ronz = ro*_n*Z;
	for (size_t i = 0; i < _sz; ++i) 
	{
		T u, v, w, W2, U, V, Tsum, L, A, Fi, Fi2;
		point ptTmp1;
		point::Sub(_pts[i], r, ptTmp1);
		u = _mi[i] * ptTmp1;
		v = u + _len[i];
		w = _ni[i] * ptTmp1;

		W2 = w*w + z*z;
		U = std::sqrt(u*u + W2);
		V = std::sqrt(v*v + W2);
		Tsum = U + V;
		T& d = _len[i];
		A = -std::atan((2.0 * w*d) / ((Tsum + d)*std::abs(Tsum - d) + 2.0 * Tsum*z));
		if (sign(u) == sign(v)) {
			L = sign(v)*std::log((V + std::abs(v)) / (U + std::abs(u)));
		}
		else {
			L = std::log((V + std::abs(v))*(U + std::abs(u)) / W2);
		}
		Fi = w*L + 2.0 * z*A;
		Fi2 = d * 0.25 * ((v + u)*(v + u) / Tsum + Tsum) + W2*L * 0.5;
		f += _n*(Fi*(ro_r + ronz) + ro*_ni[i] * Fi2) - ro*(Fi*Z * 0.5);
	}
	f = f*T(GRAVCONST);
	grv += f;
}

template<typename T>
void Facet<T>::Fld_G(const point& r, point& grv)
{
	T f{ 0.0 };
	FldVlado(r, f);
	grv += _n*f;
}

template<typename T>
void Facet<T>::Fld_Gz(const point& r, double_pfld& g)
{
	T f = 0.0;
	FldVlado(r, f);
	f *= _n.z;
	g = g + f;
}

template<typename T>
void Facet<T>::operator()(const point& r, point& grv)
{
	Fld_G(r, grv);
}

template<typename T>
void Facet<T>::operator()(const point& r, double_pfld& g)
{
	Fld_Gz(r, g);
}

template<typename T>
void Facet<T>::FldGS(const point& r, const point M, point& mag, point& grv)
{
	point f;
	FldGS(r, f);

	T s = M * _n;
	mag += f * s;

	T d = (_pts[0] + (r*-1.)) * _n;
	grv += f * d * T(GRAVCONST);
}

template<typename T>
void Facet<T>::FldGS_M(const point& r, const point M, point& mag)
{
	point f;
	FldGS(r, f);
	mag += f * (M * _n);
}

template<typename T>
void Facet<T>::FldGS_G(const point& r, point& grv)
{
	point f;
	FldGS(r, f);
	grv += f * ((_pts[0] + (r*-1.)) * _n) * T(GRAVCONST);
}

template<typename T>
void Facet<T>::FldGS_Gz(const point& r, T& gz)
{
	point f;
	FldGS(r, f);
	gz += f.z * ((_pts[0] + (r*-1.)) * _n) * T(GRAVCONST);
}

template<typename T>
void Facet<T>::FldGS(const point& r, point& f)
{
	ptvec spts(_sz);
	point shf{ r * (-1) };
	for (size_t i = 0; i < _sz; i++) {
		point::Add(_pts[i], shf, spts[i]);
	}

	T P{ 0. }, Q{ 0. }, R{ 0. };
	T dOmega = SolidAngle(spts, _n * spts[1], _sz);

	for (size_t i = 0; i < _sz; i++) {
		T r_len = spts[i].Abs();
		T b = 2 * (spts[i] * _L[i]);
		T h = r_len + b / (2 * _len[i]);
		T I;
		if (h > EPS)
			I = (1 / _len[i])*std::log((std::sqrt(_len[i] * _len[i] + b + r_len*r_len) + _len[i] + b / (2 * _len[i])) / h);
		else
			I = (1 / _len[i]) * std::log(std::abs(_len[i] - r_len) / r_len);

		P += I*_L[i].x;
		Q += I*_L[i].y;
		R += I*_L[i].z;
	}

	f = point{ dOmega*_n.x + Q*_n.z - R*_n.y,
		dOmega*_n.y + R*_n.x - P*_n.z,
		dOmega*_n.z + P*_n.y - Q*_n.x };
}

template<typename T>
T Facet<T>::SolidAngle(ptvec& pts, const T inOut, const size_t sz)
{
	T Omega{ 0. };
	T dFi = 0.;
	if (inOut == 0.)
		return 0.;

	std::vector<point*> pp(sz + 2);
	auto itPP = pp.begin();
	*itPP = &(*(--pts.end()));
	itPP++;
	for (auto it = pts.begin(); it != pts.end(); ++it, ++itPP)
		*itPP = &(*it);
	*itPP = &(*(pts.begin()));
	if (inOut > 0.) {
		std::reverse(pp.begin(), pp.end());
	}

	for (size_t i = 0; i < sz; i++) {
		point n1, n2;
		point::Cross(*pp[i + 1], *pp[i], n1);
		n1.Unit();
		point::Cross(*pp[i + 2], *pp[i + 1], n2);
		n2.Unit();
		T dPerp = *pp[i + 2] * n1;
		T b = n1 * n2;
		if (b < -1.0) { b = -1.0; }
		if (b > 1.0)  { b = 1.0; }
		T a = T(PI) - std::acos(b);
		if (dPerp < 0.) {
			a = T(PI2) - a;
		}
		dFi += a;
	}
	Omega = dFi - (sz - 2)*T(PI);
	if (inOut > 0)
		Omega = -Omega;

	return Omega;
}

} // namespace pfld

#endif  // FACET_HPP_