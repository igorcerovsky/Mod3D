// Body.cpp: implementation of the CBody class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Mod3D.h"
#include "Body.h"
#include "Grid.h"
#include <math.h>
#include "MainFrm.h"

#include "DlgPpBodyComp.h"
#include "DlgPpBodyDraw.h"
#include "DlgPpBodyDescr.h"
#include "DlgPpBodyGrav.h"
#include "DlgPpBodyMag.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

extern long g_nComp;

#define GRV_UNIT 1.0e6

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
IMPLEMENT_SERIAL( CBody, CObject, VERSIONABLE_SCHEMA|3 )

CBody::CBody()
{
	m_bActive = TRUE;
	m_bFill = TRUE;
	m_bShow = TRUE;
	m_bLocked = FALSE;
	m_bTransparent = TRUE;
	m_fAlpha = 0.5f;

	m_strName = "Name the body...";
	m_strDescription = "Body description...";

	// physical parameters 
	m_dDensity = 2700;		// kg/m3
	m_dSusc = 0.01;

	// body pen
	m_lpProf.lopnColor = RGB(0, 0, 180);
	m_lpProf.lopnStyle = PS_SOLID;
	m_lpProf.lopnWidth = CPoint(0,0);
	m_lpProfPrev.lopnColor = RGB(0, 0, 220);
	m_lpProfPrev.lopnStyle = PS_SOLID;
	m_lpProfPrev.lopnWidth = CPoint(0,0);
	m_lpProfNext.lopnColor = RGB(0, 0, 140);
	m_lpProfNext.lopnStyle = PS_SOLID;
	m_lpProfNext.lopnWidth = CPoint(0,0);
	// body brush
	m_lbProf.lbColor = RGB(0, 0, 180);
	m_lbProf.lbHatch = HS_CROSS;
	m_lbProf.lbStyle = BS_SOLID;

	// body pen
	m_lpMap.lopnColor = RGB(0, 0, 180);
	m_lpMap.lopnStyle = PS_DASH;
	m_lpMap.lopnWidth = CPoint(0,0);
	// body brush
	m_lbMap.lbColor = RGB(0, 0, 180);
	m_lbMap.lbHatch = HS_CROSS;
	m_lbMap.lbStyle = BS_SOLID;
}

CBody::~CBody()
{
}

void CBody::Serialize(CArchive &ar)
{
	if( ar.IsStoring() ) {
		ar	<< m_nID << m_strName
			<< m_strDescription;

		// status
		ar	<< m_bActive << m_bLocked << m_bShow;

		// physical properties
		ar  << m_dDensity
			<< m_vDensGrad.x << m_vDensGrad.y << m_vDensGrad.z 
			<< m_vDensOrg.x << m_vDensOrg.y << m_vDensOrg.y
			<< m_dSusc 
			<< m_vMagRem.x << m_vMagRem.y << m_vMagRem.z ;

		// drawing atributes
		ar  << m_lpProf.lopnColor << m_lpProf.lopnStyle << m_lpProf.lopnWidth
			<< m_lbProf.lbColor << m_lbProf.lbHatch << m_lbProf.lbStyle
			<< m_lpMap.lopnColor << m_lpMap.lopnStyle << m_lpMap.lopnWidth
			<< m_lbMap.lbColor << m_lbMap.lbHatch << m_lbMap.lbStyle
			<< m_bFill;

		// version 3
		ar << m_bTransparent << m_fAlpha;
	}
	// loading code
	else {
		ar	>> m_nID >> m_strName
			>> m_strDescription;

		// status
		ar	>> m_bActive >> m_bLocked >> m_bShow;

		// physical properties
		ar  >> m_dDensity
			>> m_vDensGrad.x >> m_vDensGrad.y >> m_vDensGrad.z 
			>> m_vDensOrg.x >> m_vDensOrg.y >> m_vDensOrg.y
			>> m_dSusc 
			>> m_vMagRem.x >> m_vMagRem.y >> m_vMagRem.z ;

		// drawing atributes
		ar  >> m_lpProf.lopnColor >> m_lpProf.lopnStyle >> m_lpProf.lopnWidth
			>> m_lbProf.lbColor >> m_lbProf.lbHatch >> m_lbProf.lbStyle
			>> m_lpMap.lopnColor >> m_lpMap.lopnStyle >> m_lpMap.lopnWidth
			>> m_lbMap.lbColor >> m_lbMap.lbHatch >> m_lbMap.lbStyle
			>> m_bFill;
		DWORD ver = ar.GetObjectSchema();
		switch(ver) {
			case 3:
				ar >> m_bTransparent >> m_fAlpha;
				break;
		}
	}
}

void CBody::ComputeMagnetizationVector(CPoint3D vIndLld)
{
	// transform into my coord sys
	m_vMagVector = MagnetizationVector(m_dSusc, vIndLld.x, vIndLld.y, vIndLld.z, m_vMagRem.x, m_vMagRem.y, m_vMagRem.z);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//******* body properties ************************************************************************************************
void CBody::Properties()
{
	CPropertySheet	sheet("Body Properties");
	CMainFrame* pFrameWnd = STATIC_DOWNCAST(CMainFrame, AfxGetMainWnd());
	pFrameWnd->SetPropSheetBody(&sheet);

	CDlgPpBodyComp	pgComp;
	CDlgPpBodyDraw	pgDraw;
	CDlgPpBodyDescr pgDscr;
	CDlgPpBodyGrav	pgGrav;
	CDlgPpBodyMag	pgMagn;

	sheet.AddPage( &pgGrav );
	sheet.AddPage( &pgMagn );
	sheet.AddPage( &pgDraw );
	sheet.AddPage( &pgComp );
	sheet.AddPage( &pgDscr );

	pgGrav.m_nBodyID = m_nID;

	pgDraw.m_logPen		= m_lpProf;
	pgDraw.m_logBrush	= m_lbProf;
	pgDraw.m_bFill		= m_bFill;
	pgDraw.m_logPenNext = m_lpProfNext;
	pgDraw.m_logPenPrev = m_lpProfPrev;
	pgDraw.m_bTransparent = m_bTransparent;
	pgDraw.m_fAlpha		= m_fAlpha;
	pgDraw.m_bVisible3D = m_bShow;

	pgGrav.m_dDensity = m_dDensity;
	pgGrav.m_dGradX = m_vDensGrad.x;
	pgGrav.m_dGradY = m_vDensGrad.y;
	pgGrav.m_dGradZ = m_vDensGrad.z;
	pgGrav.m_dOrigX = m_vDensOrg.x;
	pgGrav.m_dOrigY = m_vDensOrg.y;
	pgGrav.m_dOrigZ = m_vDensOrg.z;

	pgMagn.m_dSusc		= m_dSusc;
	pgMagn.m_dRemIncl	=	-m_vMagRem.x;
	pgMagn.m_dRemDecl	=	m_vMagRem.y;
	pgMagn.m_dRemInt	=	m_vMagRem.z;

	pgComp.m_bActive = m_bActive;
	pgComp.m_bLocked = m_bLocked;

	pgDscr.m_nID = m_nID;
	pgDscr.m_strDescription = m_strDescription;
	pgDscr.m_strName = m_strName;

	if( sheet.DoModal() == IDOK ) {
		CFrameWnd* pFrameWnd = STATIC_DOWNCAST(CFrameWnd, AfxGetMainWnd());
		pFrameWnd->SendMessage(WM_BODY_PROPERTIES, m_nID, 0);
	}

	pFrameWnd->SetPropSheetBody(NULL);
}



