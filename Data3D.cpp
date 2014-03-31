#include "stdafx.h"
#include "data3d.h"

#include "DlgData3D.h"
#include "math.h"

IMPLEMENT_SERIAL( CData3D, CMyObject, VERSIONABLE_SCHEMA|1 )

CData3D::CData3D(void)
{
	m_nVersion = 20040401;
	m_nCount = 0;
	m_nFormat = 4;
	m_pData = NULL;

	m_bUpdateMinMax = TRUE;

	m_bUpdateMinMax = TRUE;
	m_logPen.lopnStyle = PS_SOLID;
	m_logPen.lopnColor = RGB(255,0,0);
	m_logPen.lopnWidth.x = 0;
	m_dAlpha = 1.0;

	m_bColorDstrb = FALSE;
	m_bConnectPts = FALSE;
	m_bCustomRange = FALSE;
	m_bPoints = TRUE;

	m_nSlices = 6;
	m_nStacks = 6;
}

CData3D::~CData3D(void)
{
	FreeData();
}

void CData3D::Serialize(CArchive& ar)
{
	CMyObject::Serialize(ar);
	CString strID;
	UINT nSize, nSizeL;
	if( ar.IsStoring() ) {
		strID = _T("_CData3D");
	    ar	<< strID ;
		ar	<< m_nVersion;

		ar	<< m_nCount;
		ar	<< m_nFormat;
		nSize = m_nCount*m_nFormat*sizeof(double);
		ar	<< nSize;
		if(m_nCount!=0 ) {
			ASSERT(m_pData!=NULL);
			ar.Write((void*)m_pData, nSize);
		}

		ar << m_dProfDis;
		ar << m_logPen.lopnColor << m_logPen.lopnStyle << m_logPen.lopnWidth.x << m_logPen.lopnWidth.y;
		ar << m_dAlpha;
		ar << m_dMinData << m_dMaxData;
		ar << m_dMinDataCust << m_dMaxDataCust;
		ar << m_dRadiusDraw;
		ar << m_bColorDstrb;
		ar << m_bConnectPts;
		ar << m_bCustomRange;
		ar << m_bPoints;
		ar << m_nSlices;
		ar << m_nStacks;
	}
	else {
        ar	>> strID ;
		ar	>> m_nVersion;

		ar	>> m_nCount;
		ar	>> m_nFormat;
		nSize = m_nCount*m_nFormat*sizeof(double);
		ar	>> nSizeL;
		ASSERT(nSize==nSizeL);
		if( m_nCount!=0 ) {
			m_pData = new double[m_nCount*m_nFormat];
			ar.Read((void*)m_pData, nSize);
		}
		ar >> m_dProfDis;
		ar >> m_logPen.lopnColor >> m_logPen.lopnStyle >> m_logPen.lopnWidth.x >> m_logPen.lopnWidth.y;
		ar >> m_dAlpha;
		ar >> m_dMinData >> m_dMaxData;
		ar >> m_dMinDataCust >> m_dMaxDataCust;
		ar >> m_dRadiusDraw;
		ar >> m_bColorDstrb;
		ar >> m_bConnectPts;
		ar >> m_bCustomRange;
		ar >> m_bPoints;
		ar >> m_nSlices;
		ar >> m_nStacks;
	}

	m_clrGrad.Serialize(ar);
	if( m_bCustomRange ) {
		m_clrGrad.SetHstMin( m_dMinDataCust );
		m_clrGrad.SetHstMax( m_dMaxDataCust );
	}
	else {
		m_clrGrad.SetHstMin( m_dMinData );
		m_clrGrad.SetHstMax( m_dMaxData );
	}
}

void CData3D::FreeData()
{
	m_nCount = 0;
	m_nFormat = 4;
	if( m_pData != NULL)
		delete [] m_pData;
	m_pData = NULL;
}

void CData3D::SetData(double* pNew, int nCount, int nFormat)
{
	ASSERT(pNew!=NULL);
	ASSERT(nFormat==4);
	ASSERT(nCount!=0);
	if( pNew==NULL ) return;

	if(m_pData!=NULL)
		delete [] m_pData;

	m_nCount = nCount;
	m_nFormat = nFormat;

	m_pData = new double[nCount*nFormat];

	memcpy((void*) m_pData, (void*) pNew, nCount*nFormat*sizeof(double));
	m_bUpdateMinMax = TRUE;
	UpdateMimMax();
	m_dMaxDataCust = m_dMaxData;
	m_dMinDataCust = m_dMinData;
	m_clrGrad.SetHstMin(m_dMinDataCust);
	m_clrGrad.SetHstMax(m_dMaxDataCust);
}

void CData3D::UpdateMimMax()
{
	if(!m_bUpdateMinMax) return;
	ASSERT(m_pData!=NULL);
	if(m_pData==NULL) return;

	m_dMinData = m_dMaxData =  m_pData[0+3];
	for(int i=m_nFormat; i<m_nCount*m_nFormat; i+=m_nFormat) {
		m_dMinData = min(m_dMinData, m_pData[i+3]);
		m_dMaxData = max(m_dMaxData, m_pData[i+3]);
	}
	if(m_dMinData== m_dMaxData) {
		m_dMinData = 0;
		m_dMaxData = 1;
	}
}

void CData3D::DrawGL()
{
	ASSERT(wglGetCurrentContext()!=NULL);

	if( m_pData==NULL ) return;
	if( !IsVisible() )	return;
	if( (!m_bPoints) && (!m_bConnectPts) ) return;

	float r, g, b;
	r = GetRValue(m_logPen.lopnColor)/255.0f;
	g = GetGValue(m_logPen.lopnColor)/255.0f;
	b = GetBValue(m_logPen.lopnColor)/255.0f;
	//glDisable(GL_LIGHTING);
	//for(int i=0; i<m_nCount*m_nFormat; i+=m_nFormat) {
	//	glPointSize( 5.0f );
	//	glBegin(GL_POINTS);
	//		glColor4d(r, g, b, 1.0);
	//		glVertex3d(m_pData[i+0], m_pData[i+2], -m_pData[i+1]);
	//	glEnd();
	//}

 	glEnable(GL_LIGHTING);
	glEnable(GL_BLEND);
	GLUquadricObj*	pQuad;
	float color[4]={r, g, b, 1.0f};
	float colorN[4]={r, g, b, 1.0f};
	float matSpec[4] = {0.5f,0.5f,0.5f,0.1f};
	float matEm[4] = {0.1f, 0.1f, 0.1f, 0.1f};
	pQuad = gluNewQuadric();
	gluQuadricNormals(pQuad, GLU_SMOOTH);
	
	glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, matEm);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, color);
	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, color);
	glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 120.0f);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, matSpec);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	if( m_bPoints ) {
		for(int i=0; i<m_nCount*m_nFormat; i+=m_nFormat) {
			if( m_bColorDstrb ) {
				m_clrGrad.GetColorFromValue(m_pData[i+3], r, g, b, FALSE);
				color[0] = r/255.0f;	color[1] = g/255.0f;	color[2] = b/255.0f;
				glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, color);
				glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, color);
			}
			glPushMatrix(); 
				glTranslated(m_pData[i+0], m_pData[i+2], -m_pData[i+1]);
				gluSphere(pQuad, m_dRadiusDraw, 20, 20);
			glPopMatrix();
		}
	}

	if( m_bConnectPts ) {
		glDisable(GL_LIGHTING);
		for(int i=m_nFormat; i<m_nCount*m_nFormat; i+=m_nFormat) {
			if( m_bColorDstrb ) {
				m_clrGrad.GetColorFromValue(m_pData[i+3], r, g, b, FALSE);
				color[0] = r/255.0f;	color[1] = g/255.0f;	color[2] = b/255.0f;
				m_clrGrad.GetColorFromValue(m_pData[i-m_nFormat+3], r, g, b, FALSE);
				colorN[0] = r/255.0f;	colorN[1] = g/255.0f;	colorN[2] = b/255.0f;
				glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, color);
				glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, color);
			}
			glBegin( GL_LINES );
				glColor4fv(color);
				glVertex3d(m_pData[i+0], m_pData[i+2], -m_pData[i+1]);
				glColor4fv(colorN);
				glVertex3d(m_pData[i-m_nFormat+0], m_pData[i-m_nFormat+2], -m_pData[i-m_nFormat+1]);
			glEnd();
		}
	}

}

int CData3D::DrawProfile(CDC* pDC, double xSc, double xOff, double ySc, double yOff, int nProfType, double xy)
{
	if( !IsVisible() ) return 0;

	int		x, y;
	double	xx, yy, zz;
	CSize	szCross(3,3);
	pDC->DPtoLP(&szCross);
	CPen pen, *pPen, *pPenD;
	pen.CreatePenIndirect(&m_logPen);
	pPen = pDC->SelectObject( &pen );
	CPen penD;
	LOGPEN lpD = m_logPen;
	for(int i=0; i<m_nCount*m_nFormat; i+=m_nFormat) {
		xx = m_pData[i+0];
		yy = m_pData[i+1];
		zz = m_pData[i+2];

		if( m_bColorDstrb ) {
			m_clrGrad.GetColorFromValue(m_pData[i+3], lpD.lopnColor, FALSE);
			penD.CreatePenIndirect(&lpD);
			pPenD = pDC->SelectObject( &penD );
		}
		if( nProfType==1 && fabs(xx-xy)<m_dProfDis ) {
			x = (int) ( xOff + xSc*yy );
			y = (int) ( yOff + ySc*zz );
			pDC->MoveTo(x+szCross.cx, y+szCross.cy);
			pDC->LineTo(x-szCross.cx, y-szCross.cy);
			pDC->MoveTo(x-szCross.cx, y+szCross.cy);
			pDC->LineTo(x+szCross.cx, y-szCross.cy);
		}
		if( nProfType==0 && fabs(yy-xy)<m_dProfDis ) {
			x = (int) ( xOff + xSc*xx );
			y = (int) ( yOff + ySc*zz );
			pDC->MoveTo(x+szCross.cx, y+szCross.cy);
			pDC->LineTo(x-szCross.cx, y-szCross.cy);
			pDC->MoveTo(x-szCross.cx, y+szCross.cy);
			pDC->LineTo(x+szCross.cx, y-szCross.cy);
		}
		if( m_bColorDstrb ) {
			pDC->SelectObject( pPenD );
			penD.DeleteObject();
		}
	}
	pDC->SelectObject( pPen );

	return 0;
}

void CData3D::DrawMap(CDC* pDC, double xSc, double xOff, double ySc, double yOff )
{
	if( !IsVisible() ) return;

	int		x, y;
	double	xx, yy, v;
	CSize	szCross(3,3);
	pDC->DPtoLP(&szCross);
	CPen pen, *pPen, *pPenD;
	pen.CreatePenIndirect(&m_logPen);
	pPen = pDC->SelectObject( &pen );
	CPen penD;
	LOGPEN lpD = m_logPen;
	for(int i=0; i<m_nCount*m_nFormat; i+=m_nFormat) {
		xx = m_pData[i+0];
		yy = m_pData[i+1];
		v = m_pData[i+3];

		if( m_bColorDstrb ) {
			m_clrGrad.GetColorFromValue(m_pData[i+3], lpD.lopnColor, FALSE);
			penD.CreatePenIndirect(&lpD);
			pPenD = pDC->SelectObject( &penD );
		}
		x = (int) ( xOff + xSc*xx );
		y = (int) ( yOff + ySc*yy );
		pDC->MoveTo(x+szCross.cx, y+szCross.cy);
		pDC->LineTo(x-szCross.cx, y-szCross.cy);
		pDC->MoveTo(x-szCross.cx, y+szCross.cy);
		pDC->LineTo(x+szCross.cx, y-szCross.cy);

		if( m_bColorDstrb ) {
			pDC->SelectObject( pPenD );
			penD.DeleteObject();
		}
	}
	pDC->SelectObject( pPen );
}

int CData3D::Properties()
{
	CDlgData3D dlg;

	//dlg.m_strFilePath = m_strFile;
	dlg.m_dMin = m_dMinDataCust;
	dlg.m_dMax = m_dMaxDataCust;
	dlg.m_dMinData = m_dMinData;
	dlg.m_dMaxData = m_dMaxData;
	dlg.m_lp = m_logPen;
	dlg.m_strName = GetName();
	dlg.m_bCustomRange = m_bCustomRange;
	dlg.m_bColorDistribution = m_bColorDstrb;
	dlg.m_bConnectPts = m_bConnectPts;
	dlg.m_dRadius = m_dRadiusDraw;
	dlg.m_pGradCtrl = &m_clrGrad;
	dlg.m_bPoints = m_bPoints;
	dlg.m_nSlices = m_nSlices;
	dlg.m_nStacks = m_nStacks;

	if(dlg.DoModal() == IDOK ) {
		//m_strFile = dlg.m_strFilePath;
		m_dMinDataCust = dlg.m_dMin;
		m_dMaxDataCust = dlg.m_dMax;
		m_logPen = dlg.m_lp;
		SetName( dlg.m_strName );
		m_bCustomRange = dlg.m_bCustomRange;
		m_bColorDstrb = dlg.m_bColorDistribution;
		m_bConnectPts = dlg.m_bConnectPts;
		m_dRadiusDraw = dlg.m_dRadius;
		m_bPoints = dlg.m_bPoints;
		m_nSlices = dlg.m_nSlices;
		m_nStacks = dlg.m_nStacks;
		if( m_bCustomRange ) {
			m_clrGrad.SetHstMin(m_dMinDataCust);
			m_clrGrad.SetHstMax(m_dMaxDataCust);
		}
	}

	return 1;
}
