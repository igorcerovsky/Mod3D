// This file contains functions to compute "gravity and magnetic field" of a  polygonal facet
// 
// Igor Cerovsky
// Tubingen januar 2002
// 
//

#define EPS	1.0e-12

#include "PotField.h"
#include <math.h>

template<class T> T sign (T d) { return ( ( d==0 ) ? 0.0 : ( (d<0) ? -1 : 1 ) ); }


/* ///////////////////////////////////////////////////////////////////////////////
	s facet field in arbitrary point, facet field is added
	v_r			point in which field is computed
	*pts			array  of polygon points
	n			number of points
	v_M			magnetization vector
	v_Mag		reference to magnetic field vector, computed field is added
	v_Grv		reference to gravity field vector, computed field is added
	nFormula	formula to be used for computaion	
*/////////////////////////////////////////////////////////////////////////////////
void FacetField(CPoint3D &v_r, CPoint3D *pts, int n, CPoint3D v_M, CPoint3D &v_Mag, CPoint3D &v_Grv, int nFormula)
{

	CPoint3D p0;	// is (0, 0, 0)
	// compute anomalous field with Pohankas formula
	if(nFormula == POHANKA) {
		v_Grv += FacetPohanka(v_r, pts, n);
		return;
	}

	// compute anomalous field with Guptasarma&Singh formula
	if(nFormula == GUPTASARMA_SINGH) {
		FacetGS(v_r, pts, n, v_M, v_Mag, v_Grv);
	}
}

void FacetField(CPoint3D *pts, int n, CPoint3D v_M, CPoint3D &v_Mag, CPoint3D &v_Grv, int nFormula)
{
	// !!! cpomputing point MUST be in coordinate system origin

	CPoint3D p0;	// is (0, 0, 0)
	// compute anomalous field with Pohankas formula
	if(nFormula == POHANKA) {
		v_Grv += FacetPohanka(pts, n);
		return;
	}

	// compute anomalous field with Guptasarma&Singh formula
	if(nFormula == GUPTASARMA_SINGH) {
		FacetGS(pts, n, v_M, v_Mag, v_Grv);
	}
}





///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//****************** Singh & Guptasarma *****************************************************************************************************************
// Singh & Guptasarma 1999, 2000 Geophysisics
// variable names after above papers
// version which shifts computing point to origin
void FacetGS(CPoint3D &v_r, CPoint3D *pts, int n, CPoint3D v_M, CPoint3D &v_Mag, CPoint3D &v_Grv)
{
	// *v_r			"measuring" point

	// shift computing point to Origo
	CPoint3D	spts[10];	// !!! shifted points, do not change origanal points could be use again 
	CPoint3D	shf;
	shf = v_r * (-1);
	for(int i=0; i<n; i++) {
		spts[i] = pts[i] + shf;
	}
	FacetGS(spts, n, v_M, v_Mag, v_Grv);
}

void FacetPnt(CPoint3D &v_r, CPoint3D **pts, int n, CPoint3D v_M, CPoint3D &v_Mag, CPoint3D &v_Grv)
{
	// *v_r			"measuring" point
	// **pts		array  of pointers to polygon points

	// shift computing point to Origo
	CPoint3D	spts[10];	// !!! shifted points, do not change origanal points could be use again 
	CPoint3D	shf;
	shf = v_r * (-1);
	for(int i=0; i<n; i++) {
		spts[i] = *pts[i] + shf;
	}
	FacetGS(spts, n, v_M, v_Mag, v_Grv);
}

void FacetGS(CPoint3D *pts, int n, CPoint3D v_M, CPoint3D &v_Mag, CPoint3D &v_Grv)
{
	// cpomputin point MUST be coordinate system origin
	// *pts			array  of polygon points
	// n			number of points
	// v_M			magnetization vector
	// v_Mag		output magnetic field
	// v_Grv		output gravity field
	//TRACE("\nComputing polygon with %d sides\n", n);

	CPoint3D v_L;			// "length" vector
	CPoint3D v_r;			// distance vector
	CPoint3D v_u;			// unit outward normal vector
	CPoint3D v_e1, v_e2;	// edge vectors
	double r, L, b, I, h;
	double P = 0.0, Q =0.0, R = 0.0;
	double s, d;			// surface pole density, d
	double dOmega;			// solid angle

	// !!!this computes only if the angle between l_th and (l-1)th edge is smaller then PI (180 deg)
	v_u = (pts[0] - pts[1]) / (pts[1] - pts[2]);

	v_u.Unit();
	dOmega = SolidAngle(pts, n, v_u);
	// compute only if soloid angle is nonzero
	//TRACE("omega: %6.8f     ", (float) dOmega);
	if(dOmega != 0) {
		// for all edges compute length, P, Q, R
		for(int i=0; i<n; i++) {
			//TRACE("Edge = %d\n", i);
			v_r = pts[i];
			if( i < (n-1) ) {
				v_L = pts[i+1] - pts[i]; 
			}
			if( i == (n-1) ) {
				v_L = pts[0] - pts[i]; 
			}
			//TRACE("p1.x = %6.4f, p1.y = %6.4f, p1.z = %6.4f\n", (float) pts[i].x, (float) pts[i].y, (float) pts[i].z);
			//TRACE("p2.x = %6.4f, p2.y = %6.4f, p2.z = %6.4f\n", (float) pts[i+1].x, (float) pts[i+1].y, (float) pts[i+1].z);
			r = pts[i].Abs();
			L = v_L.Abs();
			b = 2*(v_r*v_L);
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
		v_f.x = dOmega*v_u.x + Q*v_u.z - R*v_u.y;
		v_f.y = dOmega*v_u.y + R*v_u.x - P*v_u.z;
		v_f.z = dOmega*v_u.z + P*v_u.y - Q*v_u.x;

		// magnetic field
		s = v_M*v_u;
		v_Mag += v_f * s;

		//TRACE("s = %6.6f\n", (float) (s));
		CPoint3D f = v_f * s;
		//TRACE("   m.x = %6.6f, m.y = %6.6f, m.z = %6.6f\n", (float) f.x, (float) f.y, (float) f.z);
		//TRACE("m.x = %6.6f, m.y = %6.6f, m.z = %6.6f\n", (float) v_Mag.x, (float) v_Mag.y, (float) v_Mag.z);

		// gravity field
		d = pts[0]*v_u;
		v_Grv += v_f * d * 6.67e-8;	// kappa*rho=1000 kg*m3
	}
	else {
		//TRACE("\n");
	}
}


//****************** end Singh & Guptasarma *****************************************************************************************************************
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////










////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//****************** Pohanka polygon field  **********************************************************************************************
/*//////////////////////////////////////////////////////
	PohankaInit() computes first part of Vlado's algorithm
	computes vectors mi, ni, n
//////////////////////////////////////////////////////*/
void PohankaInit(CPoint3D *pts, int n, CPoint3D &v_n, CPoint3D *v_mi, CPoint3D *v_ni, double *len) 
{
	// pts array of facet points
	// n number of facet points
	// v_n	side normal vector
	// v_mi array of mi vectors
	// v_ni array of ni vectors
	// len array of side lengths

	// (b) side normal vector n; !!! only if the angle between l_th and (l-1)th edge is smaller then PI (180 deg) 
	v_n = (pts[2]-pts[1]) / (pts[1]-pts[0]);	// side normal vector (operator /  is vector multiplication see class CPoint 3D)
	v_n.Unit();				
	//TRACE("Side normal vector is (%f, %f, %f)\n", (float) v_n.x, (float) v_n.y, (float) v_n.z);

	// for each edge
	for(int i = 0; i < n; i++) {
		// (a) length and unit vector v_mi	
		if(i != n-1) {
			v_mi[i] = pts[i+1] - pts[i];
			len[i] = v_mi[i].Abs();				// edge length
			v_mi[i].Unit();
		}
		else {
			v_mi[i] = pts[0] - pts[i];
			len[i] = v_mi[i].Abs();				// edge length
			v_mi[i].Unit();
		}
			//TRACE("   length is   %f\n", (float) len[i]);
			//TRACE("   v_mi vector is (%f, %f, %f)\n", (float) v_mi[i].x, (float) v_mi[i].y, (float) v_mi[i].z);

		// (c) edge unit vector v_ni (why here! because of VERY IMPORTANT); 
		v_ni[i] = v_mi[i] / v_n;
		//TRACE("   v_ni vector is (%f, %f, %f)\n", (float) pt.x, (float) pt.y, (float) pt.z);
	}
}

CPoint3D FacetPohanka(CPoint3D &v_r, CPoint3D *pts, int n)
{
	// v_r pointer to point distance vector
	// pts array of facet points
	// n number of facet points

	CPoint3D	v_n;					// vector n, side nornal vector 
	CPoint3D	v_mi[10], v_ni[10];	// arrays on stack, fastest !!! not checked for overflow !!!
	double		len[10];				// side length

	PohankaInit(pts, n, v_n, v_mi, v_ni, len);

	//++++++++++++++++ second part of algorithm, computing field +++++++++++++++++++++++++++++++++++++++++++++++++++
	double z, u, v, w, W2, W, U, V, T, f = 0.0, ff = 0.0;
	CPoint3D *v_a = &pts[0];
	// (a) -----------------------
	z = fabs(v_n * (*v_a - v_r));
	//----------------------------
	/*TRACE("       r  vector  = (%6.2f, %6.2f, %6.2f)\n", (float) v_r.x, (float) v_r.y, (float) v_r.z);
	TRACE("       n  vector  = (%6.2f, %6.2f, %6.2f)\n", (float) v_n.x, (float) v_n.y, (float) v_n.z);
	TRACE("       a1 vector  = (%6.2f, %6.2f, %6.2f)\n", (float) v_a.x, (float) v_a.y, (float) v_a.z);
	TRACE("       z = %6.2f\n", (float) z);*/

	for(int i = 0; i < n; i++) {
		/*TRACE("edge .\n");
		TRACE("       a  vector  = (%6.2f, %6.2f, %6.2f)\n", (float) pts[i].x, (float) pts[i].y, (float) pts[i].z);
		TRACE("       v_mi vector  = (%6.2f, %6.2f, %6.2f)\n", (float) v_mi[i].x, (float) v_mi[i].y, (float) v_mi[i].z);
		TRACE("       v_ni vector  = (%6.2f, %6.2f, %6.2f)\n", (float) v_ni[i].x, (float) v_ni[i].y, (float) v_ni[i].z);
		TRACE("       d = %6.2f\n", (float) len[i]);//*/

		// (b) ----------------
		u = v_mi[i] * (pts[i] - v_r);
		v = u + len[i];
		w = v_ni[i] * (pts[i] - v_r);
		// --------------------
				
		// (3) ------------------------------------------------------------------
		z = z + EPS;
		W2 = w*w + z*z;
		U = sqrt(u*u + W2);
		V = sqrt(v*v + W2);
		W = sqrt(W2);
		T = U + V;
		f += w * ( sign(v)*log((V+fabs(v))/W) - sign(u)*log((U + fabs(u))/W) ) - 
			2*z*atan( (2*w*len[i]) / ((T+len[i])*fabs(T-len[i])+2*T*z) );
		//------------------------------------------------------------------------

		/*TRACE("       u = %6.8f\n", (float) u);
		TRACE("       v = %6.8f\n", (float) v);
		TRACE("       w = %6.8f\n", (float) w);
		TRACE("       z = %6.8f\n", (float) z);
		TRACE("       W2= %6.8f\n", (float) W2);
		TRACE("       U = %6.8f\n", (float) U);
		TRACE("       V = %6.8f\n", (float) V);
		TRACE("       W = %6.8f\n", (float) W);
		TRACE("       T = %6.8f\n", (float) T);
		TRACE("       f of one edge = %f\n", (float) ff);*/
	}
	f *=  6.67e-8;	// kappa*rho=1000 kg*m3
	// TRACE("PlgField returns f = %6.12f\n", (float) f * v_n.z);
	return CPoint3D(f * v_n.x, f * v_n.y, f * v_n.z);  // x, y, z components of meassured field
	//++++++++++++++++++++++++++++++++++ end second  part ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////
}





/*////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	computes gravity field with formula of Vlado Pohanka
	!!! computing point is in the middle of coordinate systen p0(0, 0, 0)
	!!! facet ha maximum 10 points
	!!!!!!!!!!!!!!!  VERY IMPORTANT !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	this routine computes only if the angle between l_th and (l-1)th edge is smaller then PI (180 deg)
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////*/

CPoint3D FacetPohanka(CPoint3D *pts, int n)
{
	// pts array of facet points
	// n number of facet points

	CPoint3D	v_n;					// vector n, side nornal vector 
	CPoint3D	v_mi[10], v_ni[10];	// arrays on stack, fastest !!! not checked for overflow !!!
	double		len[10];				// side length

	// first part of algorithm
	PohankaInit(pts, n, v_n, v_mi, v_ni, len);

	// second part of algorithm, computing field
	double z, u, v, w, W2, W, U, V, T, f = 0.0, ff = 0.0;

	CPoint3D *v_a = &pts[0];
	z = fabs( *v_a * v_n );

	for(int i = 0; i < n; i++) {
		u = v_mi[i] * pts[i];
		v = u + len[i];
		w = v_ni[i] * pts[i];

		z = z + EPS;
		W2 = w*w + z*z;
		U = sqrt(u*u + W2);
		V = sqrt(v*v + W2);
		W = sqrt(W2);
		T = U + V;
		f += w * ( sign(v)*log((V+fabs(v))/W) - sign(u)*log((U + fabs(u))/W) ) - 
			2*z*atan( (2*w*len[i]) / ((T+len[i])*fabs(T-len[i])+2*T*z) );
	}
	f *=  6.67e-8;	// kappa*rho=1000 kg*m3

	return CPoint3D(f * v_n.x, f * v_n.y, f * v_n.z);  // x, y, z components of meassured field
}





//****************** Pohanka polygon field pointer version **********************************************************************************************
void PohankaInitPnt(CPoint3D **pts, int n, CPoint3D &v_n, CPoint3D *v_mi, CPoint3D *v_ni, double *len) 
{
	// pts array of pointers to facet points
	// n number of facet points
	// v_n	side normal vector
	// v_mi array of mi vectors
	// v_ni array of ni vectors
	// len array of side lengths

	// (b) side normal vector n; !!! only if the angle between l_th and (l-1)th edge is smaller then PI (180 deg) 
	v_n = (*pts[2]-*pts[1]) / (*pts[1]-*pts[0]);	// side normal vector (operator /  is vector multiplication see class CPoint 3D)
	v_n.Unit();				
	//TRACE("Side normal vector is (%f, %f, %f)\n", (float) v_n.x, (float) v_n.y, (float) v_n.z);

	// for each edge
	for(int i = 0; i < n; i++) {
		// (a) length and unit vector v_mi	
		if(i != n-1) {
			v_mi[i] = *pts[i+1] - *pts[i];
			len[i] = v_mi[i].Abs();				// edge length
			v_mi[i].Unit();
		}
		else {
			v_mi[i] = *pts[0] - *pts[i];
			len[i] = v_mi[i].Abs();				// edge length
			v_mi[i].Unit();
		}
			//TRACE("   length is   %f\n", (float) len[i]);
			//TRACE("   v_mi vector is (%f, %f, %f)\n", (float) v_mi[i].x, (float) v_mi[i].y, (float) v_mi[i].z);

		// (c) edge unit vector v_ni (why here! because of VERY IMPORTANT); 
		v_ni[i] = v_mi[i] / v_n;
		//TRACE("   v_ni vector is (%f, %f, %f)\n", (float) pt.x, (float) pt.y, (float) pt.z);
	}
}

CPoint3D FacetPohankaPnt(CPoint3D &v_r, CPoint3D **pts, int n)
{
	// v_r pointer to point distance vector
	// **pts array of pointers to facet points
	// n number of facet points

	CPoint3D	v_n;					// vector n, side nornal vector 
	CPoint3D	v_mi[10], v_ni[10];	// arrays on stack, fastest !!! not checked for overflow !!!
	double		len[10];				// side length

	// first part of algorithm
	PohankaInitPnt(pts, n, v_n, v_mi, v_ni, len);


	//++++++++++++++++ second part of algorithm, computing field +++++++++++++++++++++++++++++++++++++++++++++++++++
	double z, u, v, w, W2, W, U, V, T, f = 0.0, ff = 0.0;

	CPoint3D *v_a = pts[0];
	z = fabs(v_n * (*v_a - v_r));

	for(int i = 0; i < n; i++) {
		u = v_mi[i] * (*pts[i] - v_r);
		v = u + len[i];
		w = v_ni[i] * (*pts[i] - v_r);
				
		z = z + EPS;
		W2 = w*w + z*z;
		U = sqrt(u*u + W2);
		V = sqrt(v*v + W2);
		W = sqrt(W2);
		T = U + V;
		f += w * ( sign(v)*log((V+fabs(v))/W) - sign(u)*log((U + fabs(u))/W) ) - 
			2*z*atan( (2*w*len[i]) / ((T+len[i])*fabs(T-len[i])+2*T*z) );
	}
	f *=  6.67e-8;	// kappa*rho=1000 kg*m3

	return CPoint3D(f * v_n.x, f * v_n.y, f * v_n.z);  // x, y, z components of meassured field
}

CPoint3D FacetPohankaPnt(CPoint3D **pts, int n)
{
	// if measuring point is in the middle of coordinate system
	// v_r pointer to point distance vector
	// **pts array of pointers to facet points
	// n number of facet points

	CPoint3D	v_n;					// vector n, side nornal vector 
	CPoint3D	v_mi[10], v_ni[10];	// arrays on stack, fastest !!! not checked for overflow !!!
	double		len[10];				// side length

	// first part of algorithm
	PohankaInitPnt(pts, n, v_n, v_mi, v_ni, len);


	//++++++++++++++++ second part of algorithm, computing field +++++++++++++++++++++++++++++++++++++++++++++++++++
	double z, u, v, w, W2, W, U, V, T, f = 0.0, ff = 0.0;

	CPoint3D *v_a = pts[0];
	z = fabs(v_n * *v_a);

	for(int i = 0; i < n; i++) {
		u = v_mi[i] * *pts[i];
		v = u + len[i];
		w = v_ni[i] * *pts[i];
				
		z = z + EPS;
		W2 = w*w + z*z;
		U = sqrt(u*u + W2);
		V = sqrt(v*v + W2);
		W = sqrt(W2);
		T = U + V;
		f += w * ( sign(v)*log((V+fabs(v))/W) - sign(u)*log((U + fabs(u))/W) ) - 
			2*z*atan( (2*w*len[i]) / ((T+len[i])*fabs(T-len[i])+2*T*z) );
	}
	f *=  6.67e-8;	// kappa*rho=1000 kg*m3

	return CPoint3D(f * v_n.x, f * v_n.y, f * v_n.z);  // x, y, z components of meassured field
}













//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ********** aditional routines *****************************************************************************************

//+++++++++++ magnetic field routines ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
/*	Description:	
	right handed coordinates system
	x positive to the east
	y positive to the nord
	z positive up
*/
CPoint3D AmbientField(double iI, double iD, double iS)
{
	// iI, iD, iS	 inclination [deg], declination [deg], field strenght [nT] of inducing field

	CPoint3D af;	// ambient field

	iI *= PI/180; 
	iD *= PI/180;

	// ambient magnetic field
	af.y = cos(iI)*cos(iD);
	af.x = cos(iI)*sin(iD);
	af.z = -sin(iI);
	af = af*iS;

	return af;
}

CPoint3D MagnetizationVector(double susc, double iI, double iD, double iS, double rI, double rD, double rS)
{
	// iI, iD, iS	 inclination [deg], declination [deg], field strenght [nT] of  inducing field
	// rI, rD, rS	 inclination [deg], declination [deg], field strenght [nT] of  remanent magnetization
	CPoint3D vH, vI, vR;

	// indudced magnetization
	vH = AmbientField(iI, iD, iS);
	vI = vH*(susc/(4*PI));			// induced magnetization

	// remanent magnetization
	rI *= PI/180.0; 
	rD *= PI/180.0;
	vR.x = cos(rI)*cos(rD);			// remanent magnetization intensity
	vR.y = cos(rI)*sin(rD);
	vR.z = -sin(rI);
	vR = vR*rS;

	return  (vI + vR);
}

double	Mag_dT(CPoint3D v_mag, CPoint3D v_af, double hInt)
{
	// v_m computed magnetic field vector
	// v_af ambient magnetic field vector
	// hInt intensity of inducing field

	double fMagTot = sqrt( pow(v_mag.x + v_af.x, 2) + pow(v_mag.y + v_af.y, 2) + pow(v_mag.z + v_af.z, 2) );
	
	return (fMagTot - hInt);
}

double	Mag_dT(double mx, double my, double mz, double afx, double afy, double afz, double hInt)
{
	// v_m computed magnetic field vector
	// v_af ambient magnetic field vector
	// hInt intensity of inducing field

	double fMagTot = sqrt( pow(mx + afx, 2) + pow(my + afy, 2) + pow(mz + afz, 2) );
	
	return (fMagTot - hInt);
}
//+++++++++++ end magnetic field routines ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

double SolidAngle(CPoint3D *pts, int n, CPoint3D &v_u) 
{
	// computes solid angle as seen from the ORIGIN of coordinate system
	// *pts			array  of polygon points
	// n			number of points
	// v_u			unit outward normal vector
	double Omega = 0;	// solid angle

	// is the polygon seen from the otside or inside?
	double dInOut, dPerp, dFi=0, a, b;
	dInOut = v_u * pts[1];
	//TRACE("InOut = %f    ", dInOut);
	if(dInOut != 0) {
		CPoint3D *p1, *p2, *p3, *p;
		for(int i=0; i<n; i++) {
			if( i == 0 ) {
				p1 = &pts[n-1];	// last point
				p2 = &pts[0];	// i = 0
				p3 = &pts[1]; 
			}
			if( (i > 0) && (i < (n-1)) ) {
				p1 = &pts[i-1]; 
				p2 = &pts[i]; 
				p3 = &pts[i+1]; 
			}
			if( i == (n-1) ) {
				p1 = &pts[i-1]; 
				p2 = &pts[i]; 
				p3 = &pts[0]; 
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
			a = PI - acos(b);
			if(dPerp < 0) {
				a = 2*PI - a;
			}
			dFi += a;
		}
		Omega = dFi - (n-2)*PI;
		if(dInOut > 0)
			Omega = -Omega;
	}
	else 
		return 0.0;

	//TRACE("Omega = %2.6f\n", (float) Omega);
	return Omega;
}

// ********** end aditional routines *************************************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
