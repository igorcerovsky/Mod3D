// Point3D.h: interface for the CPoint3D class.
//
// point in space
// including bacic operations -, +, += ...
//
// can be used for vectos in three dimensions
// including basic vector arithmetics
//
// I. Cerovsky Jan. 2000
//////////////////////////////////////////////////////////////////////

#pragma once

#include <afxtempl.h>

class CPoint3D : public CObject  
{

public:
	// Constructors
	// default constructor
	CPoint3D();
	// create from three doubles
	CPoint3D(double initX, double initY, double initZ);
	// copy constructor
	CPoint3D(const CPoint3D& point) 
	{
		x = point.x;
		y = point.y;
		z = point.z;
		b = false;
	}

	virtual ~CPoint3D();
// Operations

// translate the point
	void Offset(double xOffset, double yOffset, double zOffset);
	void Offset(CPoint3D point);

	const CPoint3D& operator=(const CPoint3D& point)
	{
		x = point.x;
		y = point.y;
		z = point.z;
		b = point.b;
		return *this;
	}
	BOOL operator==(CPoint3D point) const;
	BOOL operator!=(CPoint3D point) const;
	void operator+=(CPoint3D point);
	void operator-=(CPoint3D point);

// Operators returning CPoint3D values
	CPoint3D operator-(CPoint3D point) const;
	CPoint3D operator+(CPoint3D point) const;

// vector arithmetics
	// scalar multiplication of two vectors in 3D
	double operator *(CPoint3D point) const;
	// vector * number
	CPoint3D operator *(double a) const;

	// vector multiplication of two vectors in 3D
	CPoint3D operator /(CPoint3D point) const;
	// vector / number
	CPoint3D operator /(double a);

	// another operator maybe useful sometimes
	CPoint3D operator+(double a) const;

	// manipulations
	inline void Zero();
	inline void TurnSign();

	// vector length
	inline double Abs() const;

	// unit vector
	inline void Unit();

	// make all values >=0
	inline void Positive();

	BOOL b;
	double x;
	double y;
	double z;

	inline int IsZero(void);
	inline double Angle(const CPoint3D v);
	inline double AngleZ(void);
};

#include <math.h>

inline double CPoint3D::Abs() const
{
	return sqrt(x*x + y*y + z*z);
}

inline void CPoint3D::Unit()
{
	double l = this->Abs();
	if (l != 0) {
		x /= l;
		y /= l;
		z /= l;
	}
	else {
		x = 0;
		y = 0;
		z = 0;
	}
}

inline void CPoint3D::Positive()
{
	x = fabs(x);
	y = fabs(y);
	z = fabs(z);
}

inline void CPoint3D::TurnSign()
{
	x = -x;
	y = -y;
	z = -z;
}

inline void CPoint3D::Zero()
{
	x = 0.0;
	y = 0.0;
	z = 0.0;
}

inline int CPoint3D::IsZero(void)
{
	return ( x==0.0 && y==0.0 && z==0.0 );
}


// angle of two vectors
inline double CPoint3D::Angle(const CPoint3D v)
{
	double angle;

	angle = (v*(*this)) / sqrt( (v*v) * ((*this)*(*this)) );
	return 0;
}

// angle of this vector and z axis
inline double CPoint3D::AngleZ(void)
{	
	return z / ( sqrt(x*x + y*y +z*z) );
}

inline void CPoint3D::Offset(double xOffset, double yOffset, double zOffset)
{
	x += xOffset;
	y += yOffset;
	z += zOffset;
}

inline void CPoint3D::Offset(CPoint3D point)
{
	x += point.x;
	y += point.y;
	z += point.z;
}



typedef CArray<CPoint3D, CPoint3D&> Pt3DArray;
typedef CArray<Pt3DArray, Pt3DArray&> Pt3DArray2D;
typedef CTypedPtrArray<CObArray, CPoint3D*> Pt3DPtrArray;
