// Point3D.cpp: implementation of the CPoint3D class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Point3D.h"
#include <math.h>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CPoint3D::CPoint3D()
{
	b  = FALSE;
	x  = 0.0;
	y  = 0.0;
	z  = 0.0;
}

CPoint3D::~CPoint3D()
{

}

CPoint3D::CPoint3D(double initX, double initY, double initZ)
{
	x = initX;
	y = initY;
	z = initZ;
	b = FALSE;
}

BOOL CPoint3D::operator==(CPoint3D point) const
{
	return (x == point.x && y == point.y && z == point.z);
}

BOOL CPoint3D::operator!=(CPoint3D point) const
{
	return (x != point.x || y != point.y || z != point.z);
}

void CPoint3D::operator+=(CPoint3D point)
{
	x += point.x;
	y += point.y;
	z += point.z;
}

void CPoint3D::operator-=(CPoint3D point)
{
	x -= point.x;
	y -= point.y;
	z -= point.z;
}

CPoint3D CPoint3D::operator-(CPoint3D point) const
{
	return CPoint3D(x - point.x, y - point.y, z - point.z);
}

CPoint3D CPoint3D::operator+(CPoint3D point) const
{
	return CPoint3D(x + point.x, y + point.y, z + point.z);
}

double CPoint3D::operator *(CPoint3D point) const
{
	return (x*point.x + y*point.y + z*point.z);
}

CPoint3D CPoint3D::operator *(double a) const
{
	return CPoint3D(x*a, y*a, z*a);
}

CPoint3D CPoint3D::operator/(CPoint3D point) const
{
	return CPoint3D(y*point.z - z*point.y,
					z*point.x - x*point.z,
					x*point.y - y*point.x);
}

CPoint3D CPoint3D::operator /(double a)
{
	if(a != 0)
		return CPoint3D(x/a, y/a, z/a);
	else
		return CPoint3D(0, 0, 0);
}

CPoint3D CPoint3D::operator+(double a) const
{
	return CPoint3D(x + a, y + a, z + a);
}

