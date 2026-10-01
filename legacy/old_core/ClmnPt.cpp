// ClmnPt.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "ClmnPt.h"


// CClmnPt
IMPLEMENT_SERIAL( CClmnPt, CObject, 1);

CClmnPt::CClmnPt()
{
	m_z = 0;
	m_nBodyID = -1;
	m_pBody = NULL;
	m_bModified = TRUE;
}

CClmnPt::CClmnPt( double z)
{
	m_z = z;
	m_nBodyID = -1;
	m_pBody = NULL;
	m_bModified = TRUE;
}

CClmnPt::CClmnPt(int nBodyID, double z)
{
	m_z = z;
	m_nBodyID = nBodyID;
	m_pBody = NULL;
	m_bModified = TRUE;
}

CClmnPt::CClmnPt(int nBodyID, double z, void* pBody)
{
	m_z = z;
	m_nBodyID = nBodyID;
	m_pBody = pBody;
	m_bModified = TRUE;
}

CClmnPt::CClmnPt(const CClmnPt& mpt)
{
	m_nBodyID = mpt.m_nBodyID;
	m_pBody = mpt.m_pBody;
	m_z = mpt.m_z;
	m_bModified = TRUE;
}

CClmnPt::~CClmnPt()
{
}

// CClmnPt member functions

void CClmnPt::Serialize(CArchive& ar)
{
	if (ar.IsStoring())
	{	// storing code
		ar << m_nBodyID;
		ar << m_z;
		ar << m_pt.x << m_pt.y << m_pt.z;
	}
	else
	{	// loading code
		ar >> m_nBodyID;
		ar >> m_z;
		ar >> m_pt.x >> m_pt.y >> m_pt.z;
	}
}
