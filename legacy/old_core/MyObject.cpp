// MyObject.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "MyObject.h"


// CMyObject
IMPLEMENT_SERIAL( CMyObject, CObject, VERSIONABLE_SCHEMA|1 )

CMyObject::~CMyObject()
{
}


// CMyObject member functions

void CMyObject::Serialize(CArchive& ar)
{
	if (ar.IsStoring())	{	
		ar << m_strID << m_strName << m_bVisible;
	}
	else	{	// loading code
		ar >> m_strID >> m_strName >> m_bVisible;
	}
}

void CMyObject::Draw(CDC* pDC, double scx, double offx, double scy, double offy)
{
}
