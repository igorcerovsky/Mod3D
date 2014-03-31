// Facet3Pt.cpp: implementation of the CFacet3Pt class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "PotField.h"
#include "Facet3Pt.h"
#include "Body.h"
#include <math.h>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

#define EPS	1.0e-12
#define PI 3.1415926535897932384626433832795
#define GRAVCONST 6.67259e-11

template<class T> T sign (T d) { return ( ( d==0 ) ? 0.0 : ( (d<0) ? -1 : 1 ) ); }

template<class T> T sph (T pt, T shf) {
	#define ER 6375000.0
	CPoint3D sPt = pt + shf;
	// compute on sphere coordinates
	double r = sPt.z + ER;
	double ro = sqrt(sPt.x*sPt.x + sPt.y*sPt.y);
	if(ro != 0) {
		double th = ro/r;
		double l = r*sin(th);
		sPt.x = l/ro*sPt.x;
		sPt.y = l/ro*sPt.y;
		sPt.z = sPt.z - r + r*cos(th);
	}

	return sPt;
}
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CFacet3Pt::CFacet3Pt()
{
	nType = FCT_NORMAL;
	dSign = 1.0;
	pBody = NULL;
	pBodyOpos = NULL;
	density = 1000.0;
	densityOpos = 0.0;
}

CFacet3Pt::~CFacet3Pt()
{
}

void CFacet3Pt::Init(CPoint3D &pt0, CPoint3D &pt1, CPoint3D &pt2)
{
	pts[0] = pt0;
	pts[1] = pt1;
	pts[2] = pt2;
	Init();
}

void CFacet3Pt::Init(CPoint3D &pt0, CPoint3D &pt1, CPoint3D &pt2, double densityCCW, double densityCW)
{
	pts[0] = pt0;
	pts[1] = pt1;
	pts[2] = pt2;
	density = densityCCW;
	densityOpos = densityCW;
	Init();
}

void CFacet3Pt::Init(CPoint3D &pt0, CPoint3D &pt1, CPoint3D &pt2, double densityCCW, CPoint3D v_densGradCCW)
{
	pts[0] = pt0;
	pts[1] = pt1;
	pts[2] = pt2;
	density = densityCCW;
	v_densGrad = v_densGradCCW;
	Init();
}

void CFacet3Pt::SetOpositDensity( double dDensity, CPoint3D v_grad)
{
	densityOpos = dDensity;
	v_densGradOpos = v_grad;
	bLinOpos = v_densGradOpos.IsZero();
}

void CFacet3Pt::Init()
{
	//TRACE("\n Facet Init.\n");
	// side normal vector n; only if the angle between l_th and (l-1)th edge is smaller then PI (180 deg), what is true for triangle 
	//v_n = ( pts[2] - pts[1] ) / ( pts[1] - pts[0] );	// side normal vector (operator /  is vector multiplication see class CPoint 3D)
	v_n = (pts[0] - pts[1]) / (pts[1] - pts[2]);
	v_n.Unit();				
	//// TRACE("Side normal vector is (%f, %f, %f)\n", (float) v_n.x, (float) v_n.y, (float) v_n.z);

	// for each edge
	int n =3;
	for(int i = 0; i < n; i++) {
		//TRACE("   x=%6.1f, y=%6.1f z=%6.1f \n", (float) pts[i].x, (float) pts[i].y, (float) pts[i].z);
		// (a) length and unit vector v_mi	
		if(i != n-1) {
			v_mi[i] = pts[i+1] - pts[i];
			v_L[i] = v_mi[i];
			len[i] = v_mi[i].Abs();				// edge length
			v_mi[i].Unit();
		}
		else {
			v_mi[i] = pts[0] - pts[i];
			v_L[i] = v_mi[i];
			len[i] = v_mi[i].Abs();				// edge length
			v_mi[i].Unit();
		}
		// TRACE("   length is   %f\n", (float) len[i]);
		// TRACE("   v_mi vector is (%f, %f, %f)\n", (float) v_mi[i].x, (float) v_mi[i].y, (float) v_mi[i].z);

		// edge unit vector v_ni
		v_ni[i] = v_mi[i] / v_n;
		//// TRACE("   v_ni vector is (%f, %f, %f)\n", (float) pt.x, (float) pt.y, (float) pt.z);
	}

	// what kind of computation?
	bLin = v_densGrad.IsZero();
	bLinOpos = v_densGradOpos.IsZero();
}

void CFacet3Pt::Init(CPoint3D *ppts)
{
	// side normal vector n; only if the angle between l_th and (l-1)th edge is smaller then PI (180 deg), what is true for triangle 
	//v_n = ( ppts[2] - ppts[1] ) / ( ppts[1] - ppts[0] );	// side normal vector (operator /  is vector multiplication see class CPoint 3D)
	v_n = (ppts[0] - ppts[1]) / (ppts[1] - ppts[2]);
	v_n.Unit();				
	//// TRACE("Side normal vector is (%f, %f, %f)\n", (float) v_n.x, (float) v_n.y, (float) v_n.z);

	// for each edge
	int n =3;
	for(int i = 0; i < n; i++) {
		TRACE("   x=%f, y=%f z=%f \n", (float) ppts[i].x, (float) ppts[i].z, (float) ppts[i].z);
		// (a) length and unit vector v_mi	
		if(i != n-1) {
			v_mi[i] = ppts[i+1] - ppts[i];
			v_L[i] = v_mi[i];
			len[i] = v_mi[i].Abs();				// edge length
			v_mi[i].Unit();
		}
		else {
			v_mi[i] = ppts[0] - ppts[i];
			v_L[i] = v_mi[i];
			len[i] = v_mi[i].Abs();				// edge length
			v_mi[i].Unit();
		}
		// TRACE("   length is   %f\n", (float) len[i]);
		// TRACE("   v_mi vector is (%f, %f, %f)\n", (float) v_mi[i].x, (float) v_mi[i].y, (float) v_mi[i].z);

		// edge unit vector v_ni
		v_ni[i] = v_mi[i] / v_n;
		//// TRACE("   v_ni vector is (%f, %f, %f)\n", (float) pt.x, (float) pt.y, (float) pt.z);
	}

	// what kind of computation?
	bLin = v_densGrad.IsZero();
	bLinOpos = v_densGradOpos.IsZero();

}







//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//********** field Vlado Pohanka *****************************************************************************
/*
	computes gravity field after Vlado Pohanka
	new field will be added to the current field
*/

//++++++++++++++++++++++++ general function +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// gravity field computes all
double* CFacet3Pt::FldVladoAll(CPoint3D &v_r, CPoint3D &v_rGrad)
{
	// !!! Init MUST be called first
	// v_r pointer to point distance vector
	// **pts array of pointers to facet points
	// all variables must be filled
	// densityOposit can be reference density

	int n =3;	// number of vertices
	// second part of algorithm, computing field 
	double z, u, v, w, W2, W, U, V, T, L, A, d, Z, e, Fi, Fi2;

	// zero tmps
	tmpFld.Zero();
	tmpFldGrd.Zero();
	tmpFldOpos.Zero();

	CPoint3D *v_a = &pts[0];
	Z = v_n * (*v_a - v_r);
	z = fabs(Z);
	e = sign( Z );

	//TRACE("New Facet\n");
	for(int i = 0; i < n; i++) {
		//TRACE("x = %.1f, y = %.1f, z = %.1f\n", (float) (pts[i]).x, (float) (pts[i]).y, (float) (pts[i]).z);
		u = v_mi[i] * (pts[i] - v_r);
		v = u + len[i];
		w = v_ni[i] * (pts[i] - v_r);
				
		z = z + EPS;
		W2 = w*w + z*z;
		U = sqrt(u*u + W2);
		V = sqrt(v*v + W2);
		W = sqrt(W2);
		T = U + V;
		d = len[i];
		A = -atan( (2*w*d) / ( T*T - (v-u)*(v-u) + 2*T*z ) );
		if( sign(u) == sign(v) ) {
			L = sign(v)*log( (V+fabs(v))/(U+fabs(u)) );
		}
		else {
			L = log( (V+fabs(v))*(U+fabs(u))/(W*W) );
		}

		Fi = w*L + 2*z*A;
		Fi2 = d/4 *( (v+u)*(v+u)/T+T ) + W*W*L/2;

		if( bLin) {
			// linear density
			tmpFld += v_n*(Fi*(density+v_densGrad*v_r+v_densGrad*v_n*Z)+v_densGrad*v_ni[i]*Fi2) - v_densGrad*(Fi*Z/2);
		}
		else {
			// constant density
			tmpFld =  v_n*Fi*density;
		}

		if( bLinOpos ) {
			tmpFldOpos += v_n*(Fi*(densityOpos+v_densGradOpos*v_r+v_densGradOpos*v_n*Z)+v_densGradOpos*v_ni[i]*Fi2) - v_densGradOpos*(Fi*Z/2);
		}
		else {
			tmpFldOpos =  v_n*Fi*densityOpos;
		}

		// gradients
		tmpFldGrd += v_ni[i]*L + v_n*2*e*A;
	}
	tmpFld = (tmpFld-tmpFldOpos)*GRAVCONST;
	tmpFldGrd = tmpFldGrd*GRAVCONST*(density-densityOpos);	
	
	g[0] = tmpFld.x;
	g[1] = tmpFld.y;
	g[2] = tmpFld.z;
	g[3] = tmpFldGrd.x * v_n.x;
	g[4] = tmpFldGrd.y * v_n.y;
	g[5] = tmpFldGrd.z * v_n.z;
	g[6] = 1/2.0f*(tmpFldGrd.y * v_n.z + tmpFldGrd.z * v_n.y);
	g[7] = 1/2.0f*(tmpFldGrd.x * v_n.y + tmpFldGrd.y * v_n.x);
	g[8] = 1/2.0f*(tmpFldGrd.x * v_n.z + tmpFldGrd.z * v_n.x);
	
	return g;
}
//+++++++++++++++++++++++++++++++ end general ++++++++++++++++++++++++++++++++++++++++++++++++++++++

void CFacet3Pt::FldVlado(CPoint3D &v_r, CPoint3D &v_Grv)
{
	// !!! Init MUST be called first
	// v_r pointer to point distance vector
	// **pts array of pointers to facet points
	// n number of facet points

	int n =3;
	// second part of algorithm, computing field 
	double z, u, v, w, W2, W, U, V, T, f = 0.0, ff = 0.0;

	CPoint3D *v_a = &pts[0];
	z = fabs(v_n * (*v_a - v_r));

	for(int i = 0; i < n; i++) {
		// TRACE("p1.x = %6.4f, p1.y = %6.4f, p1.z = %6.4f\n", (float) (pts[i]).x, (float) (pts[i]).y, (float) (pts[i]).z);
		u = v_mi[i] * (pts[i] - v_r);
		v = u + len[i];
		w = v_ni[i] * (pts[i] - v_r);
				
		z = z + EPS;
		W2 = w*w + z*z;
		U = sqrt(u*u + W2);
		V = sqrt(v*v + W2);
		W = sqrt(W2);
		T = U + V;
		f += w * ( sign(v)*log((V+fabs(v))/W) - sign(u)*log((U + fabs(u))/W) ) - 
			2*z*atan( (2*w*len[i]) / ((T+len[i])*fabs(T-len[i])+2*T*z) );
	}
	f *=  GRAVCONST;	

	//return CPoint3D(f * v_n.x, f * v_n.y, f * v_n.z);  // x, y, z components of meassured field
	v_Grv += v_n*f; 
	// TRACE("Vlado returns f = %6.12f\n", (float) f * v_n.z);
}

// gravity field for variable density
// ro density gradient vector
// ro0 density in origin of coordinate system
void CFacet3Pt::FldVlado(CPoint3D &v_r, CPoint3D &v_Grv, CPoint3D ro, double ro0 )
{
	// !!! Init MUST be called first
	// v_r pointer to point distance vector
	// **pts array of pointers to facet points
	// n number of facet points
	// ro density gradient
	// ro0 density in origin

	int n =3;
	// second part of algorithm, computing field 
	double z, u, v, w, W2, W, U, V, T, L, A, Fi, Fi2, d, Z;
	CPoint3D f;

	CPoint3D *v_a = &pts[0];
	Z = v_n * (*v_a - v_r);
	z = fabs(Z);

	for(int i = 0; i < n; i++) {
		// TRACE("p1.x = %6.4f, p1.y = %6.4f, p1.z = %6.4f\n", (float) (pts[i]).x, (float) (pts[i]).y, (float) (pts[i]).z);
		u = v_mi[i] * (pts[i] - v_r);
		v = u + len[i];
		w = v_ni[i] * (pts[i] - v_r);
				
		z = z + EPS;
		W2 = w*w + z*z;
		U = sqrt(u*u + W2);
		V = sqrt(v*v + W2);
		W = sqrt(W2);
		T = U + V;
		d = len[i];
		A = -atan( (2*w*d) / ((T+d)*fabs(T-d)+2*T*z) );
		if( sign(u) == sign(v) ) {
			L = sign(v)*log( (V+fabs(v))/(U+fabs(u)) );
		}
		else {
			L = log( (V+fabs(v))*(U+fabs(u))/(W*W) );
		}
		Fi = w*L + 2*z*A;
		Fi2 = d/4 *( (v+u)*(v+u)/T+T ) + W*W*L/2;
		f += v_n*(Fi*(ro0+ro*v_r+ro*v_n*Z)+ro*v_ni[i]*Fi2) - ro*(Fi*Z/2);
	}
	f =  f*GRAVCONST;	
	v_Grv += f; 
	// TRACE("Vlado returns f = %6.12f\n", (float) f * v_n.z);
}

// gravity field for gradient components
void CFacet3Pt::FldVladoGrd(CPoint3D &v_r, double refDensity, double &gxx, double &gyy, double &gzz, double &gxy, double &gxz, double &gyz )
{
	// !!! Init MUST be called first
	// v_r pointer to point distance vector
	// **pts array of pointers to facet points
	// n number of facet points
	// ro density gradient
	// ro0 density in origin

	int n =3;
	// second part of algorithm, computing field 
	double z, u, v, w, W2, W, U, V, T, L, A, d, Z, e;
	double dens;

	CPoint3D *v_a = &pts[0];
	Z = v_n * (*v_a - v_r);
	z = fabs(Z);
	e = sign( Z );

	//TRACE("New Facet\n");
	tmpFldGrd.Zero();
	for(int i = 0; i < n; i++) {
		//TRACE("x = %.1f, y = %.1f, z = %.1f\n", (float) (pts[i]).x, (float) (pts[i]).y, (float) (pts[i]).z);
		u = v_mi[i] * (pts[i] - v_r);
		v = u + len[i];
		w = v_ni[i] * (pts[i] - v_r);
				
		z = z + EPS;
		W2 = w*w + z*z;
		U = sqrt(u*u + W2);
		V = sqrt(v*v + W2);
		W = sqrt(W2);
		T = U + V;
		d = len[i];
		A = -atan( (2*w*d) / ( T*T - (v-u)*(v-u) + 2*T*z ) );
		if( sign(u) == sign(v) ) {
			L = sign(v)*log( (V+fabs(v))/(U+fabs(u)) );
		}
		else {
			L = log( (V+fabs(v))*(U+fabs(u))/(W*W) );
		}
		// gradients
		tmpFldGrd += v_ni[i]*L + v_n*2*e*A;
	}
	tmpFldGrd =  tmpFldGrd*GRAVCONST;	
	
	if( densityOpos != 0.0 )
		dens = density - densityOpos;
	else
		dens = density - refDensity;
	gxx = dens * tmpFldGrd.x * v_n.x;
	gyy = dens * tmpFldGrd.y * v_n.y;
	gzz = dens * tmpFldGrd.z * v_n.z;
	gyz = dens * 1/2.0f*(tmpFldGrd.y * v_n.z + tmpFldGrd.z * v_n.y);
	gxy = dens * 1/2.0f*(tmpFldGrd.x * v_n.y + tmpFldGrd.y * v_n.x);
	gxz = dens * 1/2.0f*(tmpFldGrd.x * v_n.z + tmpFldGrd.z * v_n.x);
	
	return;
}

//********** end field Vlado Pohanka *************************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////




//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//********** field Singh && Guptasarma ***********************************************************************

void CFacet3Pt::FldGS(CPoint3D &v_r, CPoint3D v_M, CPoint3D &v_Mag, CPoint3D &v_Grv)
{
	// cpomputing point MUST be coordinate system origin, first transform points
	// n = 3		number of points
	// v_M			magnetization vector
	// v_Mag		output magnetic field
	// v_Grv		output gravity field
	// *v_r			"measuring" point
	// **spts		array of pointers to polygon (triangle) points

	int n = 3;

	// shift computing point to Origo
	CPoint3D	spts[3];	// !!! shifted points, make local copy of shifted points, do not change origanal points
	CPoint3D	shf;
	shf = v_r * (-1);
	// TRACE("//// p1.x = %6.4f, p1.y = %6.4f, p1.z = %6.4f\n", (float) shf.x, (float) shf.y, (float) shf.z);
	for(int i=0; i<n; i++) {
		spts[i] = pts[i] + shf;
		 //TRACE("---- p1.x = %6.4f, p1.y = %6.4f, p1.z = %6.4f\n", (float) (pts[i]).x, (float) (pts[i]).y, (float) (pts[i]).z);
		// TRACE("++++ p1.x = %6.4f, p1.y = %6.4f, p1.z = %6.4f\n", (float) spts[i].x, (float) spts[i].y, (float) spts[i].z);
	}

	/*CPoint3D v_L;			// "length" vector
	CPoint3D v_u;			// unit outward normal vector
	CPoint3D v_e1, v_e2;	// edge vectors*/
	CPoint3D v_rr;			// distance vector
	double r, L, b, I, h;
	double P = 0.0, Q =0.0, R = 0.0;
	double s, d;			// surface pole density, d
	double dOmega;			// solid angle

	// compute only if soloid angle is nonzero
	// TRACE("omega: %6.8f     ", (float) dOmega);
	dOmega = SolidAngle(spts);
	if( dOmega == 0.0 )	return;

	// for all edges compute length, P, Q, R
	for(int i=0; i<n; i++) {
		// TRACE("Edge = %d\n", i);
		// TRACE("p1.x = %6.4f, p1.y = %6.4f, p1.z = %6.4f\n", (float) spts[i].x, (float) spts[i].y, (float) spts[i].z);
		v_rr = spts[i];
		r = spts[i].Abs();
		L = len[i];				//L = v_L.Abs();
		b = 2*(v_rr*v_L[i]);
		h = r + b/(2*L);
		if(h != 0)
			I = (1/L)*log((sqrt(L*L + b + r*r) + L + b/(2*L))/h );
		if(h == 0) {
			I = (1/L) * log(fabs(L-r)/r);
		}
		P += I*v_L[i].x;
		Q += I*v_L[i].y;
		R += I*v_L[i].z;
		// TRACE("L = %6.4f, Lx = %6.4f, Ly = %6.4f, Lz = %6.4f\n", (float) L, (float) v_L[i].x, (float) v_L[i].y, (float) v_L[i].z);
		// TRACE("P = %6.4f, Q = %6.4f, R = %6.4f\n",(float) (I*v_L[i].x), (float) (I*v_L[i].y), (float) (I*v_L[i].z));
		// TRACE("I = %6.4f\n\n", I);
	}

	CPoint3D v_f;
	v_f.x = dOmega*v_n.x + Q*v_n.z - R*v_n.y;
	v_f.y = dOmega*v_n.y + R*v_n.x - P*v_n.z;
	v_f.z = dOmega*v_n.z + P*v_n.y - Q*v_n.x;

	// magnetic field
	s = v_M*v_n;
	v_Mag += v_f * s;

	// TRACE("s = %6.6f\n", (float) (s));
	// CPoint3D f = v_f * s;
	// TRACE("m.x = %6.6f, m.y = %6.6f, m.z = %6.6f\n", (float) f.x, (float) f.y, (float) f.z);
	// TRACE("m.x = %6.6f, m.y = %6.6f, m.z = %6.6f\n", (float) v_Mag.x, (float) v_Mag.y, (float) v_Mag.z);

	// gravity field
	d = spts[0]*v_n;
	v_Grv += v_f * d * GRAVCONST;	// kappa
}


void CFacet3Pt::FldGS(CPoint3D &v_r, CPoint3D v_M, CPoint3D &v_Mag, double dSign)
{
	// cpomputing point MUST be coordinate system origin, first transform points
	// n = 3		number of points
	// v_M			magnetization vector
	// v_Mag		output magnetic field
	// *v_r			"measuring" point
	// **spts		array of pointers to polygon (triangle) points

	int n = 3;

	// shift computing point to Origo
	CPoint3D	spts[3];	// !!! shifted points, make local copy of shifted points, do not change origanal points
	CPoint3D	shf;
	shf = v_r * (-1);
	// TRACE("//// p1.x = %6.4f, p1.y = %6.4f, p1.z = %6.4f\n", (float) shf.x, (float) shf.y, (float) shf.z);
	for(int i=0; i<n; i++) {
		spts[i] = pts[i] + shf;
		 //TRACE("---- p1.x = %6.4f, p1.y = %6.4f, p1.z = %6.4f\n", (float) (pts[i]).x, (float) (pts[i]).y, (float) (pts[i]).z);
		// TRACE("++++ p1.x = %6.4f, p1.y = %6.4f, p1.z = %6.4f\n", (float) spts[i].x, (float) spts[i].y, (float) spts[i].z);
	}

	CPoint3D v_rr;			// distance vector
	double r, L, b, I, h;
	double P = 0.0, Q =0.0, R = 0.0;
	double s;				// surface pole density, d
	double dOmega;			// solid angle

	// compute only if soloid angle is nonzero
	//TRACE("----------------------------------------\n ");
	dOmega = SolidAngle(spts);
	//if(dOmega == 0.0) return;
	//if( dOmega < 1.0e-12 )	return;

	// for all edges compute length, P, Q, R
	for(int i=0; i<n; i++) {
		// TRACE("Edge = %d\n", i);
		// TRACE("p1.x = %6.4f, p1.y = %6.4f, p1.z = %6.4f\n", (float) spts[i].x, (float) spts[i].y, (float) spts[i].z);
		v_rr = spts[i];
		r = spts[i].Abs();
		L = len[i];				//L = v_L.Abs();
		b = 2*(v_rr*v_L[i]);
		h = r + b/(2*L);
		double tmp = (sqrt(L*L + b + r*r) + L + b/(2*L))/h ;
		if(h != 0.0)
			I = (1/L)*log( (sqrt(L*L + b + r*r) + L + b/(2*L))/h );
		else
			I = (1/L) * log(fabs(L-r)/r);

		P += I*v_L[i].x;
		Q += I*v_L[i].y;
		R += I*v_L[i].z;
		//TRACE("L = %6.4f, Lx = %6.4f, Ly = %6.4f, Lz = %6.4f\n", (float) L, (float) v_L[i].x, (float) v_L[i].y, (float) v_L[i].z);
		//TRACE("P = %6.4f, Q = %6.4f, R = %6.4f\n",(float) (I*v_L[i].x), (float) (I*v_L[i].y), (float) (I*v_L[i].z));
		//TRACE("I = %g;   ", I);
	}

	CPoint3D v_f;
	v_f.x = dOmega*v_n.x + Q*v_n.z - R*v_n.y;
	v_f.y = dOmega*v_n.y + R*v_n.x - P*v_n.z;
	v_f.z = dOmega*v_n.z + P*v_n.y - Q*v_n.x;

	// magnetic field
	s = dSign*(v_M*v_n);
	v_Mag += v_f * s;

	//TRACE("  s = %g;    omega=%g\n", s, dOmega );
	//TRACE("  m.x = %g, m.y = %g, m.z = %g\n", v_f.x, v_f.y, v_f.z);
}


double CFacet3Pt::SolidAngle(CPoint3D *spts)
{
	// computes solid angle as seen from the ORIGIN of coordinate system
	// pts			array  of pointers to polygon points
	// n			number of points
	double Omega = 0.0;	// solid angle

	int n = 3;
	// is the polygon seen from the otside or inside?
	double dInOut, dPerp, dFi=0, a, b;
	dInOut = v_n * spts[1];
	// TRACE("InOut = %f    ", dInOut);
	if(dInOut == 0.0) 
		return 0.0;
	
	CPoint3D *p1, *p2, *p3, *p;
	for(int i=0; i<n; i++) {
		if( i == 0 ) {
			p1 = &spts[n-1];	// last point
			p2 = &spts[0];		// i = 0
			p3 = &spts[1]; 
		}
		if( (i > 0) && (i < (n-1)) ) {
			p1 = &spts[i-1]; 
			p2 = &spts[i]; 
			p3 = &spts[i+1]; 
		}
		if( i == (n-1) ) {
			p1 = &spts[i-1]; 
			p2 = &spts[i]; 
			p3 = &spts[0]; 
		}
		if(dInOut >  0.0) {
			// change points order
			p  = p1;
			p1 = p3;
			p3 = p;
		}
		CPoint3D n1, n2;	
		n1 = *p2 / *p1;	n1.Unit();
		n2 = *p3 / *p2;	n2.Unit();
		dPerp = *p3 * n1;
		b = n1 * n2;
		//ASSERT(!(b<-1.0));
		//ASSERT(!(b>1.0));
		if(b<-1)	{b=-1.0;}
		if(b>1)		{b=1.0;}
		a = PI - acos(b);
		if(dPerp < 0) {
			a = 2*PI - a;
		}
		dFi += a;
	}
	Omega = dFi - (n-2)*PI;
	if(dInOut > 0)
		Omega = -Omega;

	return Omega;
}

//********** end field Singh && Guptasarma *******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////






















//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//************** spherical coputing **************************************************************************

void CFacet3Pt::FldSpherGS(CPoint3D &v_r, CPoint3D v_M, CPoint3D &v_Mag, CPoint3D &v_Grv)
{
	// cpomputin point MUST be coordinate system origin
	// n			number of points
	// v_M			magnetization vector
	// v_Mag		output magnetic field
	// v_Grv		output gravity field
	//TRACE("\nComputing polygon with %d sides\n", n);

	CPoint3D	spts[3];	// !!! shifted points, make local copy of shifted points, do not change origanal points

	// transfer points to sphere
	for(int i=0; i<3; i++) {
		spts[i] = sph(pts[i], v_r * (-1));
	}

	CPoint3D v_L;			// "length" vector
	CPoint3D v_rr;			// distance vector
	CPoint3D v_n;			// unit outward normal vector
	CPoint3D v_e1, v_e2;	// edge vectors
	double r, L, b, I, h;
	double P = 0.0, Q =0.0, R = 0.0;
	double s, d;			// surface pole density, d
	double dOmega;			// solid angle

	// !!!this computes only if the angle between l_th and (l-1)th edge is smaller then PI (180 deg)
	v_n = (spts[0] - spts[1]) / (spts[1] - spts[2]);

	v_n.Unit();
	int n =3;

	// compute only if soloid angle is nonzero
	//TRACE("omega: %6.8f     ", (float) dOmega);
	dOmega = SolidAngle(spts);
	if( dOmega == 0.0 )	return;

	// for all edges compute length, P, Q, R
	for(int i=0; i<n; i++) {
		//TRACE("Edge = %d\n", i);
		v_rr = spts[i];
		if( i < (n-1) ) {
			v_L = spts[i+1] - spts[i]; 
		}
		if( i == (n-1) ) {
			v_L = spts[0] - spts[i]; 
		}
		//TRACE("p1.x = %6.4f, p1.y = %6.4f, p1.z = %6.4f\n", (float) spts[i].x, (float) spts[i].y, (float) spts[i].z);
		//TRACE("p2.x = %6.4f, p2.y = %6.4f, p2.z = %6.4f\n", (float) spts[i+1].x, (float) spts[i+1].y, (float) spts[i+1].z);
		r = spts[i].Abs();
		L = v_L.Abs();
		b = 2*(v_rr*v_L);
		h = r + b/(2*L);
		if(h != 0)
			I = (1/L)*log((sqrt(L*L + b + r*r) + L + b/(2*L))/h );
		if(h == 0) {
			I = (1/L) * log(fabs(L-r)/r);
		}
		P += I*v_L.x;
		Q += I*v_L.y;
		R += I*v_L.z;
		//TRACE("L = %6.4f, Lx = %6.4f, Ly = %6.4f, Lz = %6.4f\n", (float) L, (float) v_L.x, (float) v_L.y, (float) v_L.z);
		//TRACE("P = %6.4f, Q = %6.4f, R = %6.4f\n",(float) (I*v_L.x), (float) (I*v_L.y), (float) (I*v_L.z));
		//TRACE("I = %6.4f\n\n", I);
	}

	CPoint3D v_f;
	v_f.x = dOmega*v_n.x + Q*v_n.z - R*v_n.y;
	v_f.y = dOmega*v_n.y + R*v_n.x - P*v_n.z;
	v_f.z = dOmega*v_n.z + P*v_n.y - Q*v_n.x;

	// magnetic field
	s = v_M*v_n;
	v_Mag += v_f * s;

	//TRACE("s = %6.6f\n", (float) (s));
	CPoint3D f = v_f * s;
	//TRACE("   m.x = %6.6f, m.y = %6.6f, m.z = %6.6f\n", (float) f.x, (float) f.y, (float) f.z);
	//TRACE("m.x = %6.6f, m.y = %6.6f, m.z = %6.6f\n", (float) v_Mag.x, (float) v_Mag.y, (float) v_Mag.z);

	// gravity field
	d = spts[0]*v_n;
	v_Grv += v_f * d * GRAVCONST;	// kappa
}

void CFacet3Pt::FldSpherVlado(CPoint3D &v_r, CPoint3D &v_Grv)
{
	// shift computing point to Origo
	CPoint3D	spts[3];	// !!! shifted points, make local copy of shifted points, do not change origanal points
	// TRACE("//// p1.x = %6.4f, p1.y = %6.4f, p1.z = %6.4f\n", (float) shf.x, (float) shf.y, (float) shf.z);

	// transfer points to sphere
	for(int i=0; i<3; i++) {
		spts[i] = sph(pts[i], v_r * (-1));
		// TRACE("---- p1.x = %6.4f, p1.y = %6.4f, p1.z = %6.4f\n", (float) (pts[i]).x, (float) (pts[i]).y, (float) (pts[i]).z);
		// TRACE("++++ p1.x = %6.4f, p1.y = %6.4f, p1.z = %6.4f\n", (float) spts[i].x, (float) spts[i].y, (float) spts[i].z);
	}
	
	Init(spts);

	int n =3;
	double z, u, v, w, W2, W, U, V, T, f = 0.0, ff = 0.0;

	CPoint3D *v_a = &spts[0];
	z = fabs(v_n * (*v_a));

	for(int i = 0; i < n; i++) {
		// TRACE("p1.x = %6.4f, p1.y = %6.4f, p1.z = %6.4f\n", (float) (spts[i]).x, (float) (spts[i]).y, (float) (spts[i]).z);
		u = v_mi[i] * (spts[i]);
		v = u + len[i];
		w = v_ni[i] * (spts[i]);
				
		z = z + EPS;
		W2 = w*w + z*z;
		U = sqrt(u*u + W2);
		V = sqrt(v*v + W2);
		W = sqrt(W2);
		T = U + V;
		f += w * ( sign(v)*log((V+fabs(v))/W) - sign(u)*log((U + fabs(u))/W) ) - 
			2*z*atan( (2*w*len[i]) / ((T+len[i])*fabs(T-len[i])+2*T*z) );
	}
	f *=  6.67e-11;	// kappa*rho=1000 kg*m3

	//return CPoint3D(f * v_n.x, f * v_n.y, f * v_n.z);  // x, y, z components of meassured field
	v_Grv += v_n*f; 
	// TRACE("Vlado returns f = %6.12f\n", (float) f * v_n.z);

}

double CFacet3Pt::GetMeanElevation(void)
{
    return (pts[0].z + pts[1].z + pts[3].z)/3;
}

BOOL CFacet3Pt::IsOposit(const CFacet3Pt &fct)
{
	CPoint3D c1, c2;	// centroids

	// !!! the order is important, absurd?
	c1 = pts[0] + pts[2] + pts[1];
	c2 = fct.pts[0] + fct.pts[1] + fct.pts[2];
	return ( c1 == c2 );

}


CPoint3D CFacet3Pt::Centroid(void)
{
	return pts[0] + pts[1] + pts[2];
}

// checks if this facet contains  given point
// if the facet contains the point returns the pointer to given point; otherwise NULL 
CPoint3D* CFacet3Pt::ContainsVertex(const CPoint3D* pt)
{
	for(int i = 0; i < 3; i++) {
		if(pts[i] == *pt)
			return &pts[i];
	}

	return NULL;
}

// reverses the facet, changes opositBody to normal body
void CFacet3Pt::Reverse(void)
{
	CPoint3D	pt;
	CBody*		pBd;

	pt = pts[0];
	pts[0] = pts[2];
	pts[2] = pt;

	pBd = pBody;
	pBody = pBodyOpos;
	pBodyOpos = pBd;

	Init();
}





BOOL ComputeGravityVlado(CPoint3D &v_r, CFacet3Pt &facet, CPoint3D &fGrv, double dRefDensity, int nTag, CPoint3D vRefDensGrad, double dRefDensOrg) 
{
	CBody	*pBody, *pBodyOpos;
	CPoint3D fb, fOb;

	pBody = facet.pBody;
	pBodyOpos = facet.pBodyOpos;
	// outside facet
	if( pBodyOpos == NULL ) {
		// constant density
		if( pBody->GetDensityGradient().IsZero() ) {
			if( nTag == 1 ) {
				facet.FldVlado(v_r, fb);
				fb = fb*pBody->GetDensity();
				facet.FldVlado(v_r, fOb, vRefDensGrad, dRefDensOrg );
				fGrv = fb - fOb;
			}
			else {
				facet.FldVlado(v_r, fGrv);
				fGrv = fGrv*( pBody->GetDensity() - dRefDensity);
			}
			// do not forget reference model !!!
		}
		// linear density
		else {
			facet.FldVlado(v_r, fb, pBody->GetDensityGradient(), pBody->GetDensityAtOrigin() );
			if( nTag == 1 ) {
				facet.FldVlado(v_r, fOb, vRefDensGrad, dRefDensOrg );
			}
			else {
				facet.FldVlado(v_r, fOb); 
				fOb = fOb*dRefDensity;
			}
			// do not forget reference model !!!
			fGrv = fb - fOb;
		}
	}
	// oposit facets
	else {
		//----- body ----------------------------------------------------------------------------------------------
		// constant density
		if( pBody->GetDensityGradient().IsZero() ) {
			facet.FldVlado(v_r, fb);
			fb = fb * pBody->GetDensity();
		}
		// linear density
		else {
			facet.FldVlado(v_r, fb, pBody->GetDensityGradient(), pBody->GetDensityAtOrigin() );
		}
		//----- body oposit ----------------------------------------------------------------------------------------
		// constant density
		if( pBodyOpos->GetDensityGradient().IsZero() ) {
			facet.FldVlado(v_r, fOb);
			fOb = fOb * pBodyOpos->GetDensity();
		}
		// linear density
		else {
			facet.FldVlado(v_r, fOb, pBodyOpos->GetDensityGradient(), pBodyOpos->GetDensityAtOrigin() );
		}
		fGrv += fb - fOb;
	}

	return 1;
}


void CFacet3Pt::Compute(
	double* gx, double* gy, double* gz,
	double* gxx, double* gyy, double* gzz, double* gxy, double* gxz, double* gyz,
	double* mx, double* my, double* mz,
	double& dRefDens, double& dRefDensOrg, CPoint3D& v_refDens, int nRdm,
	CPoint3D& v_rGrv, CPoint3D& v_rTen, CPoint3D& v_rMag,
	double& dUnitGrv, double& dUnitMag, double& dUnitTns  ) 
{
	if( GetType()== FCT_NULL )
		return;

	CPoint3D	fGrv, fMag;
	double		txx, tyy, tzz, txy, txz, tyz;

	// gravity field
	if( (pBody->GetDensity()!=0.0 || dRefDens!=0.0)
		|| ((pBodyOpos!=NULL) ? (pBodyOpos->GetDensity()!=0.0 || dRefDens!=0.0) : FALSE) ) 
	{
		if( gxx || gyy || gzz || gxy || gxz || gyz )  {
			FldVladoGrd(v_rTen, dRefDens, txx, tyy, tzz, txy, txz, tyz); 
			// gradients
			if(gxx != NULL)	{ *gxx += dUnitTns * dSign * txx; }
			if(gyy != NULL)	{ *gyy += dUnitTns * dSign * tyy; }
			if(gzz != NULL)	{ *gzz += dUnitTns * dSign * tzz; }
			if(gxy != NULL)	{ *gxy += dUnitTns * dSign * txy; }
			if(gxz != NULL)	{ *gxz += dUnitTns * dSign * txz; }
			if(gyz != NULL)	{ *gyz += dUnitTns * dSign * tyz; }
		}
		if( gx || gy || gz ) {
			fGrv.Zero();
			ComputeGravityVlado(v_rGrv, *this, fGrv, dRefDens, nRdm, v_refDens, dRefDensOrg);
			fGrv = fGrv;
			// add field to components grids
			if(gx != NULL)	{ *gx += dUnitGrv * dSign * fGrv.x; }
			if(gy != NULL)	{ *gy += dUnitGrv * dSign * fGrv.y; }
			if(gz != NULL)	{ *gz += dUnitGrv * dSign * fGrv.z; }
		}
	}

	// magnetic field
	if( mx || my || mz ) {
		fMag.Zero();
		if( !pBody->GetMagnetizationVector().IsZero() && pBody->IsActive() ) {
			FldGS(v_rMag, pBody->GetMagnetizationVector(), fMag, 1.0);
		}
		if( pBodyOpos != NULL ) {
			if( !pBodyOpos->GetMagnetizationVector().IsZero() && pBodyOpos->IsActive() ) {
				FldGS(v_rMag, pBodyOpos->GetMagnetizationVector(), fMag, -1.0);	// oposit facet => oposit sign
			}
		}
		//TRACE("!!!  %g  %g  %g\n", fMag.x, fMag.x, fMag.z);
		if(mx != NULL)	{ *mx -= dUnitMag * dSign * fMag.x; }
		if(my != NULL)	{ *my -= dUnitMag * dSign * fMag.y; }
		if(mz != NULL)	{ *mz -= dUnitMag * dSign * fMag.z; }
	}
}
