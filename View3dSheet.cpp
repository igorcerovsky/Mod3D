// View3dSheet.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "View3dSheet.h"


// CView3dSheet

IMPLEMENT_DYNAMIC(CView3dSheet, CPropertySheet)
CView3dSheet::CView3dSheet(UINT nIDCaption, CWnd* pParentWnd, UINT iSelectPage)
	:CPropertySheet(nIDCaption, pParentWnd, iSelectPage)
{
}

CView3dSheet::CView3dSheet(LPCTSTR pszCaption, CWnd* pParentWnd, UINT iSelectPage)
	:CPropertySheet(pszCaption, pParentWnd, iSelectPage)
{
}

CView3dSheet::~CView3dSheet()
{
}


BEGIN_MESSAGE_MAP(CView3dSheet, CPropertySheet)
END_MESSAGE_MAP()

