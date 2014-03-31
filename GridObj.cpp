#include "stdafx.h"
#include "gridobj.h"
#include "DlgObjGrd.h"

IMPLEMENT_SERIAL(CGridObj, CMyObject, 1)

CGridObj::CGridObj(void) 
{
	m_nType = leadingHorizon;
	m_lp.lopnColor = RGB(180, 180, 190);
	m_lp.lopnStyle = PS_SOLID;
	m_lp.lopnWidth.x = 1;
}

CGridObj::CGridObj(const char* strID) : CMyObject(strID) 
{
	m_nType = leadingHorizon;
	m_lp.lopnColor = RGB(180, 180, 190);
	m_lp.lopnStyle = PS_SOLID;
	m_lp.lopnWidth.x = 1;
}

CGridObj::CGridObj(const char* strID, const char* strName) : CMyObject(strID, strName) 
{
	m_nType = leadingHorizon;
	m_lp.lopnColor = RGB(180, 180, 190);
	m_lp.lopnStyle = PS_SOLID;
	m_lp.lopnWidth.x = 1;
}

CGridObj::~CGridObj(void)
{
}

void CGridObj::Serialize(CArchive& ar)
{
	CMyObject::Serialize( ar );

	if (ar.IsStoring())	{
		ar << m_nType;
		ar << m_lp.lopnColor << m_lp.lopnStyle << m_lp.lopnWidth.x << m_lp.lopnWidth.y;
	}
	else	{	
		ar >> m_nType;
		ar >> m_lp.lopnColor >> m_lp.lopnStyle >> m_lp.lopnWidth.x >> m_lp.lopnWidth.y;	
	}

	m_grd.Serialize(ar);
}

int CGridObj::Properties()
{
	CDlgObjGrd dlg;

	dlg.m_nType = m_nType;
	dlg.m_pGrd = &m_grd;
	dlg.m_strID = m_strID;
	dlg.m_strName = m_strName;
	dlg.m_strFileName = m_grd.GetFilePath();
	dlg.m_lp = m_lp;
	if( dlg.DoModal() == IDOK ) {
		m_nType = dlg.m_nType;
		m_strID = dlg.m_strID;
		m_strName = dlg.m_strName;
		m_lp = dlg.m_lp;
	}
	return 0;
}