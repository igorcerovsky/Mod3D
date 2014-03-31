// Well.cpp : implementation file
//

#include <stdlib.h>
#include <math.h>
#include "stdafx.h"
#include "Mod3D.h"
#include "Well.h"
#include "DlgWell.h"

// open GL
#include "gl/gl.h"
#include "gl/glu.h"

#define PI 3.1415926535897932384626433832795

// CWell
IMPLEMENT_SERIAL( CWell, CObject, VERSIONABLE_SCHEMA )

CWell::CWell()
{
	m_nVersion = 20031215;

	m_nID = -1;			// invalid
	m_nFormat = -1;		// invalid

	m_logPen.lopnColor = RGB(80, 80, 80);
	m_logPen.lopnStyle = PS_SOLID;
	m_logPen.lopnWidth.x = 0;

	m_nChanWidth = 50;
	m_nDravTag = drawMiddle;
	m_dRadiusLog = 1;
	m_dRadiusLog3D = 1;
	m_nWellWidth = 2;
	m_nWellWidth3D = 2;

	m_nSamplingRing = 16;
	m_nSamplingLog = 1;

	m_dLogPieStart = 0.0;
	m_dLogPieEnd = 360.0;
}

CWell::~CWell()
{
	m_data.RemoveAll();
	m_strData.RemoveAll();
	m_welLit.RemoveAll();
}


// CWell member functions

void CWell::Serialize(CArchive& ar)
{
	// identification string
	CString strID;

	if (ar.IsStoring())
	{	
		strID.Format("Mod3D_Well %d", m_nVersion);
		ar << strID;
		ar << m_nVersion;
		CMyObject::Serialize(ar);
		ar << m_nID;
		ar << m_strName << m_strLocation << m_strDescription;

		// position
		ar << m_x << m_y << m_z;

		// data
		ar << m_nFormat;
		m_strData.Serialize(ar);
		m_data.Serialize(ar);

		//drawing
		ar << m_nChanZ << m_nChanWidth;
		ar << m_dRadiusProf;
		ar << m_dRadiusLog;
		ar << m_dRadiusLog3D;
		ar << m_bSolidLog3D; 
		ar << m_nWellWidth;
		ar << m_nWellWidth3D;
		ar << m_nDravTag;
		ar << m_nSamplingLog;
		ar << m_nSamplingRing;
		ar << m_dLogPieStart;
		ar << m_dLogPieEnd;

		// lithology
		m_welLit.Serialize(ar);
		m_showChannel.Serialize(ar);

		// pens
		int nPens = m_penArray.GetSize();
		ar << nPens;
		for(int i=0; i<nPens; i++) {
			CPen* pPen;
			pPen = &m_penArray.GetAt(i);
			LOGPEN lp;
			pPen->GetLogPen(&lp);

			// serialize it
			ar << lp.lopnColor;
			ar << lp.lopnStyle;
			ar << lp.lopnWidth.x;
			ar << lp.lopnWidth.y;
		}
	}
	else
	{	// loading code
		ar >> strID;
		ar >> m_nVersion;
		CMyObject::Serialize(ar);
		if( m_nVersion < 20031215) {
			SerializeOld(ar);
			return;
		}
		ar >> m_nID;
		ar >> m_strName >> m_strLocation >> m_strDescription;

		// position
		ar >> m_x >> m_y >> m_z;

		// data
		ar >> m_nFormat;
		m_strData.Serialize(ar);
		m_data.Serialize(ar);

		//drawing
		ar >> m_nChanZ >> m_nChanWidth;
		ar >> m_dRadiusProf;
		ar >> m_dRadiusLog;
		ar >> m_dRadiusLog3D; 
		ar >> m_bSolidLog3D;
		ar >> m_nWellWidth;
		ar >> m_nWellWidth3D;
		ar >> m_nDravTag;
		ar >> m_nSamplingLog;
		ar >> m_nSamplingRing;
		ar >> m_dLogPieStart;
		ar >> m_dLogPieEnd;

		// lithology
		m_welLit.Serialize(ar);
		m_showChannel.Serialize(ar);

		int nPens;
		ar >> nPens;
		m_penArray.SetSize(nPens);
		for(int i=0; i<nPens; i++) {
			LOGPEN lp;
			ar >> lp.lopnColor;
			ar >> lp.lopnStyle;
			ar >> lp.lopnWidth.x;
			ar >> lp.lopnWidth.y;
			m_penArray[i].CreatePenIndirect( &lp );
		}

		//m_penArray.SetSize(m_nFormat);
		//for(int i=0; i<m_penArray.GetSize(); i++) {
		//	m_penArray[i].CreatePen( PS_SOLID, 0, RGB(160,160,160) );
		//}
	}
}

void CWell::SerializeOld(CArchive& ar)
{
	//switch(m_nVersion) {
	//	case :
	//		Serialize_(ar);
	//		break;
	//}
}

void CWell::DrawGL(void)
{
	if( m_data.IsEmpty() || !m_bVisible ) return;

	int n;
	int lit;
	int	ipl;
	double* wd;
	WELLIT* wl;

	n = m_data.GetSize();
	wd = m_data.GetData();

	BOOL bLighht =  glIsEnabled(GL_LIGHTING);
	glDisable(GL_LIGHTING);
	if(m_bSolidLog3D)		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	else	{
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}

	
	for(int i=m_nFormat*3, ip=m_nFormat*2; i<n; i += m_nFormat, ip += m_nFormat) {
		lit = (int) wd[i+m_nFormat-1];
		wl = &m_welLit.GetAt(lit);
		glColor3d(wl->rd, wl->gd, wl->bd);
		glLineWidth( 5 );
		glBegin(GL_LINES);
			glVertex3d(	wd[ip+0], wd[ip+2], -wd[ip+1]);
			glVertex3d(	wd[i+0],  wd[i+2],  -wd[i+1]);
		glEnd();

		// Draw current layer
		glLineWidth(1.0f);
		if( (i % m_nSamplingLog) == 0) {
			for(int j=4; j<m_nFormat-1; j++) {
				if(m_showChannel[j] == TRUE) {
					ipl = i - m_nFormat*m_nSamplingLog;
					if(ipl < m_nFormat*2) ipl = m_nFormat*2;
					double u=m_dLogPieStart*PI/180.0;
					double dU = 2*PI/m_nSamplingRing;	

					//glColor3f(1.0f, 0.0f, 0.0f);
					CPen* pPen = &m_penArray.GetAt(j);
					LOGPEN lp;
					double clr[3];
					pPen->GetLogPen(&lp);
					clr[0] = GetRValue(lp.lopnColor)/255.0;
					clr[1] = GetGValue(lp.lopnColor)/255.0;
					clr[2] = GetBValue(lp.lopnColor)/255.0;
					glColor3dv(clr);
					while (u<(m_dLogPieEnd*PI/180.0)) {
						double	P00[3],P01[3],P10[3],P11[3];
						double dRadip, dRadi;
						dRadi = m_dRadiusLog3D * (wd[i+j] - wd[0+j])/(wd[m_nFormat+j] - wd[0+j]);
						dRadip = m_dRadiusLog3D * (wd[ipl+j] - wd[0+j])/(wd[m_nFormat+j] - wd[0+j]);

						P00[0] = wd[ipl+0] + cos(u)*(dRadip);
						P00[1] = wd[ipl+2];
						P00[2] = -wd[ipl+1] + sin(u)*(dRadip);

						P01[0] = wd[ipl+0] + cos(u+dU)*(dRadip);
						P01[1] = wd[ipl+2];
						P01[2] = -wd[ipl+1] + sin(u+dU)*(dRadip);

						P11[0] = wd[i+0] + cos(u+dU)*(dRadi);
						P11[1] = wd[i+2];
						P11[2] = -wd[i+1] + sin(u+dU)*(dRadi);

						P10[0] = wd[i+0] + cos(u)*(dRadi);
						P10[1] = wd[i+2];
						P10[2] = -wd[i+1] + sin(u)*(dRadi);

						glBegin(GL_QUADS);
							glVertex3dv(P00);
							glVertex3dv(P01);
							glVertex3dv(P11);
							glVertex3dv(P10);
						glEnd();

						u+=dU;
					};
				}
			}
		}
	}
	if(bLighht)		glEnable(GL_LIGHTING);
	else			glDisable(GL_LIGHTING);
}

void CWell::IndexWell(void)
{
	WELLIT* wl;
	int n = m_data.GetSize();
	int	m = m_welLit.GetCount();
	int lit;
	double* wd = m_data.GetData();

	for(int i=0; i<n; i += m_nFormat) {
		lit = (int) wd[i+3];
		for(int l=0; l<m; l++) {
			wl = &m_welLit.GetAt(l);
			if(wl->nType == lit) {
				wd[i+m_nFormat-1] = l;
				break;
			}
		}
	}

	m_showChannel.SetSize( m_nFormat );
	m_showChannel.SetSize(m_nFormat);
	for(int i=0; i<m_showChannel.GetSize(); i++) {
		if(i>=3)	m_showChannel[i] = 1;
		else		m_showChannel[i] = 0;
	}
}

// draw to map
void CWell::DrawMap(CDC* pDC, double scx, double offx, double scy, double offy)
{
	if( !m_bVisible ) return;

	int		x, y;
	CPen	pen, *pPen;
	CSize	sz(5,5);

	pDC->DPtoLP(&sz);
	pen.CreatePenIndirect(&m_logPen);
	x = (int) (offx + m_x*scx);
	y = (int) (offy + m_y*scy);

	// draw some nonsense
	pPen = pDC->SelectObject(&pen);
	pDC->MoveTo(x-sz.cx, y-sz.cy);
	pDC->LineTo(x-sz.cx, y+sz.cy);
	pDC->LineTo(x+sz.cx, y+sz.cy);
	pDC->LineTo(x+sz.cx, y-sz.cy);
	pDC->LineTo(x-sz.cx, y-sz.cy);
	pDC->LineTo(x+sz.cx, y+sz.cy);
	pDC->MoveTo(x-sz.cx, y+sz.cy);
	pDC->LineTo(x+sz.cx, y-sz.cy);
	pDC->SelectObject(pPen);
}

// draw to profile
void CWell::DrawProfile(CDC* pDC, double scx, double offx, double scy, double offy, int nTag, double xy, double r)
{
	ASSERT(nTag==0 || nTag==1);

	if( m_data.IsEmpty() || !m_bVisible) return;

	int color;
	int n, m;
	int lit;
	int k;
	BOOL bDraw=FALSE;
	CPen pen, *pPen;
	CBrush	brush, *pBrush;
	CSize	sz(m_nWellWidth, m_nWellWidth);
	double	scl;
	double* wd;
	WELLIT* wl;
	CPoint	plg[4];
	PenArray penAr;
	BrushArray brAr;
	int		nDrawOff;
	int		nDrvTag;

	n = m_data.GetSize();
	wd = m_data.GetData();
	int x, y;

	pDC->DPtoLP(&sz);
	if(nTag==0 && fabs(xy-m_y)<m_dRadiusProf ) {
		k=0;
		bDraw=TRUE;
	}
	if(nTag==1 && fabs(xy-m_x)<m_dRadiusProf ) {
		k=1;
		bDraw=TRUE;
	}

	if(!bDraw) return;

	// create pens and brushes
	m = m_welLit.GetSize();
	penAr.SetSize(m);
	brAr.SetSize(m);
	for(int l=0; l<m; l++) {
		wl = &m_welLit.GetAt(l);
		color = RGB(wl->r, wl->g, wl->b);
		penAr.GetAt(l).CreatePen( PS_SOLID, 0, color );
		brAr.GetAt(l).CreateSolidBrush( color );
	}

	// draw offset
	switch( m_nDravTag ) {
		case 0:
			nDrawOff = 0;
			nDrvTag = -1;
			break;
		case 1:
			nDrawOff = 0;
			nDrvTag = 1;
			break;
		case 2:
			nDrawOff = int(-m_dRadiusLog/2);
			nDrvTag = 1;
			break;
	}

	for(int i=m_nFormat*3, ip=m_nFormat*2; i<n; i += m_nFormat, ip += m_nFormat) {
		lit = (int) wd[i+m_nFormat-1];
		wl = &m_welLit.GetAt(lit);

		//
		pPen = pDC->SelectObject(&penAr[lit]);
		pBrush = pDC->SelectObject(&brAr[lit]);
		double ax, ay, bx, by, b;
		ax = wd[i+k] - wd[ip+k];
		ay = wd[i+2] - wd[ip+2];
		b = sqrt(ax*ax + ay*ay);
		bx = -ay/b * sz.cx;
		by = ax/b * sz.cy;
		plg[0].x = (int) (offx + scx*wd[ip+k] - bx);		plg[0].y = (int) (offy + scy*wd[ip+2] - by);
		plg[1].x = (int) (offx + scx*wd[ip+k] + bx);		plg[1].y = (int) (offy + scy*wd[ip+2] + by);
		plg[2].x = (int) (offx + scx*wd[i+k] + bx);			plg[2].y = (int) (offy + scy*wd[i+2] + by);
		plg[3].x = (int) (offx + scx*wd[i+k] - bx);			plg[3].y = (int) (offy + scy*wd[i+2] - by);
		pDC->Polygon(plg, 4);
		pDC->SelectObject(pBrush);
		pDC->SelectObject(pPen);

		if( (i % m_nSamplingLog) == 0) {
			for(int j=4; j<m_nFormat-1; j++) {
				if(m_showChannel[j] == TRUE) {
					int ipl = i - m_nFormat*m_nSamplingLog;
					if(ipl < m_nFormat*2) ipl = m_nFormat*2;
					pPen = pDC->SelectObject(&m_penArray.GetAt(j));
						scl = m_dRadiusLog/(wd[m_nFormat+j] - wd[0+j]);
						x = (int) (offx + (wd[ipl+k] + nDrawOff + scl*nDrvTag*(wd[ipl+j]-wd[0+j]))*scx);
						y = (int) (offy + wd[ipl+2]*scy);
						pDC->MoveTo(x, y);
						x = (int) (offx + (wd[i+k] + nDrawOff + scl*nDrvTag*(wd[i+j]-wd[0+j]))*scx);
						y = (int) (offy + wd[i+2]*scy);
						pDC->LineTo(x, y);
					pDC->SelectObject(pPen);
				}
			}
		}
	}

}

int CWell::Properties()
{
	CDlgWell dlg;

	dlg.m_pWell = this;
	dlg.m_nDrawWidth = m_nChanWidth;
	dlg.m_nDrawSide = m_nDravTag;
	dlg.m_dRadius = m_dRadiusProf;
	dlg.m_bSolid3D = m_bSolidLog3D;
	dlg.m_nDrawWidth = m_nWellWidth;
	dlg.m_nDrawWidth3D = m_nWellWidth3D;
	dlg.m_dRadiusLog = m_dRadiusLog;
	dlg.m_dRadiusLog3D = m_dRadiusLog3D;
	dlg.m_nSamplingLog = m_nSamplingLog;
	dlg.m_nSamplingRing = m_nSamplingRing;
	dlg.m_dPieEnd = m_dLogPieEnd;
	dlg.m_dPieStart = m_dLogPieStart;

	if( dlg.DoModal() == IDOK ) {
		m_nChanWidth = dlg.m_nDrawWidth;
		m_nDravTag = dlg.m_nDrawSide;
		m_dRadiusProf = dlg.m_dRadius;
		m_dRadiusLog3D = dlg.m_dRadiusLog3D;
		m_bSolidLog3D = dlg.m_bSolid3D ;
		m_nWellWidth = dlg.m_nDrawWidth;
		m_nWellWidth3D = dlg.m_nDrawWidth3D;
		m_dRadiusLog = dlg.m_dRadiusLog ;
		m_dRadiusLog3D = dlg.m_dRadiusLog3D;
		m_nSamplingLog = dlg.m_nSamplingLog;
		m_nSamplingRing = dlg.m_nSamplingRing;
		m_dLogPieEnd = dlg.m_dPieEnd;
		m_dLogPieStart = dlg.m_dPieStart;
	}

	return 1;
}
void CWell::InitPens(void)
{
	if( m_nFormat == -1 ) return;

	m_penArray.SetSize(m_nFormat);
	for(int i=0; i<m_penArray.GetSize(); i++) {
		m_penArray[i].CreatePen( PS_SOLID, 0, RGB(160,160,160) );
	}
}
