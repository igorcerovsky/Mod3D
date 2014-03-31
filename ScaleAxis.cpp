// ScaleBar.cpp: implementation of the CScaleAxis class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Mod3D.h"
#include "ScaleAxis.h"
#include <math.h>
#include "DlgAxis.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CScaleAxis::CScaleAxis()
{
	m_nType = SB_HORZ;
	m_dZoomIn = 1.2;
	m_dZoomOut = 0.8;

	m_brushH1.CreateSolidBrush(RGB(180,180, 180));
	m_brushH2.CreateSolidBrush(RGB(100,100, 100));
	m_penH1.CreatePen(PS_SOLID, 0, RGB(180,180, 180) );
	m_penH2.CreatePen(PS_SOLID, 0, RGB(100,100, 100) );
	// axis
	m_bAutoTL = TRUE;
	m_ar = CSize(8,8);			// arrow in points
	m_tickM = CSize(6,6);		// major ticks in points
	m_tickN = CSize(4,4);		// minor ticks
	m_nOrientLabel = -1;
	m_bTickMajor = TRUE;
	m_bTickMinor = TRUE;
	m_bLabelMajor = TRUE;
	m_bLabelMinor = TRUE;
	m_nDigitsMajor = 0;
	m_nDigitsMinor = 0;
	m_nTicks = 4;
	// fill LOFFONT structure for major labels
	memset(&m_lfTickMajor, 0, sizeof(LOGFONT));			// zero out structure
	m_lfTickMajor.lfHeight = 80;						// !!! CreatePointFontIndirect
	strcpy(m_lfTickMajor.lfFaceName, "Arial");        	
	memcpy(&m_lfTickMinor,&m_lfTickMajor,sizeof(LOGFONT));
	m_lfTickMinor.lfHeight = 60;	// !!! CreatePointFontIndirect

	m_bGLMajor = FALSE;
	m_bGLMinor = FALSE;

	m_bWidth = FALSE;
	m_dTickStart = DBL_MAX;
}

CScaleAxis::CScaleAxis(int nType)
{
	m_nType = nType;
	m_dZoomIn = 1.2;
	m_dZoomOut = 0.8;

	m_brushH1.CreateSolidBrush(RGB(180,180, 180));
	m_brushH2.CreateSolidBrush(RGB(100,100, 100));
	m_penH1.CreatePen(PS_SOLID, 0, RGB(180,180, 180) );
	m_penH2.CreatePen(PS_SOLID, 0, RGB(100,100, 100) );
	// axis
	m_ar = CSize(8,8);		// arrow in points
	m_tickM = CSize(6,6);		// major ticks in points
	m_tickN = CSize(4,4);		// minor ticks
	m_nOrientLabel = -1;
	m_bTickMajor = TRUE;
	m_bTickMinor = TRUE;
	m_bLabelMajor = TRUE;
	m_bLabelMinor = TRUE;
	m_nDigitsMajor = 0;
	m_nDigitsMinor = 0;
	// fill LOFFONT structure for major labels
	memset(&m_lfTickMajor, 0, sizeof(LOGFONT));			// zero out structure
	m_lfTickMajor.lfHeight = 80;						// !!! CreatePointFontIndirect
	strcpy(m_lfTickMajor.lfFaceName, "Arial");        	
	memcpy(&m_lfTickMinor,&m_lfTickMajor,sizeof(LOGFONT));
	m_lfTickMinor.lfHeight = 60;						// !!! CreatePointFontIndirect

	m_bWidth = FALSE;
	m_dTickStart = DBL_MAX;
}

CScaleAxis::~CScaleAxis()
{

}

void CScaleAxis::Draw(CDC *pDC)
{
	CSize	s(1, 1);
	//int width;	// screen coordinates
	CPen	*pPen, nullPen(PS_NULL, 0, RGB(0, 0,0));
	CBrush	*pBr;
	int	nx;

	pDC->DPtoLP( &s );
	if(m_nType == SB_HORZ) {
		if( !pDC->IsPrinting() ) {
			pBr = pDC->SelectObject(&m_brushH2);
			pPen = pDC->SelectObject(&m_penH2);
			pDC->MoveTo(m_nR, m_nT);
			pDC->LineTo(m_nR, m_nB);
			pDC->Rectangle(m_nL, m_nT, m_nR, m_nB);
			pDC->SelectObject(pPen);
			pDC->SelectObject(pBr);
			pBr = pDC->SelectObject(&m_brushH1);
			pPen = pDC->SelectObject(&m_penH1);
			pDC->Rectangle(m_nMin, m_nT, m_nMax, m_nB);
			pDC->SelectObject(pBr);
			pDC->SelectObject(pPen);
		}
		DrawAxisX(pDC);
	}
	if(m_nType == SB_VERT) {
		if( !pDC->IsPrinting() ) {
			nx = m_nR + GetWidth(pDC);
			pBr = pDC->SelectObject(&m_brushH2);
			pPen = pDC->SelectObject(&m_penH2);
			pDC->MoveTo(m_nR, m_nT);
			pDC->LineTo(m_nR, m_nB);
			pDC->Rectangle(m_nL, m_nT, m_nR, m_nB);
			//pDC->Rectangle(nx, m_nT, nx+(m_nL-m_nR), m_nB);
			pDC->SelectObject(pPen);
			pDC->SelectObject(pBr);
			pBr = pDC->SelectObject(&m_brushH1);
			pPen = pDC->SelectObject(&m_penH1);
			pDC->Rectangle(m_nL, m_nMax, m_nR, m_nMin);
			//pDC->Rectangle(nx, m_nMax, nx+(m_nL-m_nR), m_nMin);
			pDC->SelectObject(pBr);
			pDC->SelectObject(pPen);
		}
		DrawAxisY(pDC);
	}
	//pDC->SelectObject(pPen);
}

void CScaleAxis::SetDrawRect(int left, int top, int right, int bottom)
{
	m_nT = top;
	m_nL = left;
	m_nR = right;
	m_nB = bottom;
}

void CScaleAxis::SetDrawRect(CRect rc)
{
	m_nT = rc.top;
	m_nL = rc.left;
	m_nR = rc.right;
	m_nB = rc.bottom;
}

void CScaleAxis::SetMaxDataExtent(double min, double max)
{
	if( (max-min) == 0 ) return;

	if( m_bAutoTL ) {
		m_dMinData = min;
		m_dMaxData = max;
	}
}

void CScaleAxis::SetDataExtent(double min, double max)
{
	if( (max-min) == 0 ) return;

	if(min >= m_dMinData)
		m_dMin = min;
	else
		m_dMin = m_dMinData;

	if(max <= m_dMaxData)
		m_dMax = max;
	else
		m_dMax = m_dMaxData;

	if(m_dMin < m_dMax) {
		Scale();
		m_nMax = (int) (m_off + m_sc*m_dMax);
		m_nMin = (int) (m_off + m_sc*m_dMin);
	}
}

int CScaleAxis::IsClicked(CPoint lPt)
{
	int l, *pB;
	long *px, *py;

	if(m_nType == SB_VERT) {
		px = &lPt.y;
		py = &lPt.x;
		l = (m_nR - m_nL);
		pB = &m_nL;
	}
	if(m_nType == SB_HORZ) {
		px = &lPt.x;
		py = &lPt.y;
		pB = &m_nB;
		l = (m_nT - m_nB);
	}

	if( ((m_nMin-l/2) < (*px)) && ((m_nMin) >= (*px)) && ((*pB) < (*py)) && ((*pB+l) > (*py)) ) {
		m_nClk = SB_MIN;
		return SB_MIN;
	}
	if( ((m_nMax) <= (*px)) && ((m_nMax+l/2) > (*px)) && ((*pB) < (*py)) && ((*pB+l) > (*py)) ) {
		m_nClk = SB_MAX;
		return SB_MAX;
	}
	if( ((m_nMin) < (*px)) && ((m_nMax) > (*px)) && ((*pB) < (*py)) && ((*pB+l) > (*py)) ) {
		m_nClk = SB_MIDDLE;
		m_ndl = *px - m_nMin;
		m_ndr = m_nMax - *px;
		return SB_MIDDLE;
	}
	return SB_NON;
}

void CScaleAxis::Scale()
{
	int b, a;
	if(m_nType == SB_HORZ) {
		b = (m_nR-m_nL);
		a = m_nL;
	}
	if(m_nType == SB_VERT) {
		b = (m_nT-m_nB);
		a = m_nB;
	}

	m_sc  = (double) b / (m_dMaxData-m_dMinData);
	m_off = a - m_sc*m_dMinData;
}

void CScaleAxis::Move(CPoint lPt)
{
	long *px, *py;
	int  *pL, *pR;

	if(m_nType == SB_VERT) {
		px = &lPt.y;
		py = &lPt.x;
		pL = &m_nB;
		pR = &m_nT;
	}
	if(m_nType == SB_HORZ) {
		px = &lPt.x;
		py = &lPt.y;
		pL = &m_nL;
		pR = &m_nR;
	}

	if( m_nClk == SB_MIN ){
		if(*px > *pL && *px < m_nMax)
			m_nMin = *px;
		else
			m_nMin = *pL;
	}

	if( m_nClk == SB_MAX ) {
		if(*px < *pR && *px > m_nMin)
			m_nMax = *px;
		else
			m_nMax = *pR;
	}

	if( m_nClk == SB_MIDDLE ) {
		if( (*pL < (*px - m_ndl)) && (*pR > (*px + m_ndr)) ) {
			m_nMin = *px - m_ndl;
			m_nMax = *px + m_ndr; 
		}
		else {
			if( *pL > (*px - m_ndl) ) {
				m_nMin = *pL;
				m_nMax = *pL + m_ndr + m_ndl;
			}
			if( *pR < (*px + m_ndr) ) {
				m_nMax = *pR;
				m_nMin = *pR - m_ndr - m_ndl;
			}
		}
	}

	double min = (m_nMin - m_off)/m_sc;
	double max = (m_nMax - m_off)/m_sc;
	if( min < m_dMinData ) min = m_dMinData;
	if( max > m_dMaxData ) max = m_dMaxData;
	m_dMin = min;
	m_dMax = max;
}

void CScaleAxis::Zoom(double min, double max)
{
	SetDataExtent(min, max);
}

void CScaleAxis::GetDataExtent(double &min, double &max)
{
	min = m_dMin;
	max = m_dMax;
	//min = (m_nMin - m_off)/m_sc;
	//max = (m_nMax - m_off)/m_sc;
	//if( min < m_dMinData ) min = m_dMinData;
	//if( max > m_dMaxData ) max = m_dMaxData;
}

double CScaleAxis::GetDataMin()
{
	double min = (m_nMin - m_off)/m_sc;
	if( min < m_dMinData ) min = m_dMinData;
	return min;
}

double CScaleAxis::GetDataMax()
{
	double max = (m_nMax - m_off)/m_sc;
	if( max > m_dMaxData ) max = m_dMaxData;
	return max;
}


void CScaleAxis::Zoom(CPoint lPt, BOOL bIn)
{
	double zoom;
	if(bIn)
		zoom = m_dZoomIn;
	else
		zoom = m_dZoomOut;

	long *px, *py;
	int  *pL, *pR;

	if(m_nType == SB_VERT) {
		px = &lPt.y;
		py = &lPt.x;
		pL = &m_nB;
		pR = &m_nT;
	}
	if(m_nType == SB_HORZ) {
		px = &lPt.x;
		py = &lPt.y;
		pL = &m_nL;
		pR = &m_nR;
	}

	int ndl = (int) ( ((double)(m_nMax - m_nMin)/2)*zoom);
	
	if( (*pL < (*px - ndl)) && (*pR > (*px + ndl)) ) {
		m_nMin = *px - ndl;
		m_nMax = *px + ndl; 
	}
	else {
		m_nMin = *px - ndl;
		m_nMax = *px + ndl; 
		if( *pL > m_nMin ) {
			m_nMin = *pL;
		}
		if( *pR < m_nMax ) {
			m_nMax = *pR;
		}
	}
}

void CScaleAxis::GetScOff(double& sc, double& off)
{
	double min, max;
	int b, a;
	if(m_nType == SB_HORZ) {
		b = (m_nR-m_nL);
		a = m_nL;
	}
	if(m_nType == SB_VERT) {
		b = (m_nT-m_nB);
		a = m_nB;
	}

	min = (m_nMin - m_off)/m_sc;
	max = (m_nMax - m_off)/m_sc;

	sc  = (double) b / (max-min);
	off = a - sc*min;
}

void CScaleAxis::GetDataMinMax(double& min, double& max)
{
	min = m_dMin;
	max = m_dMax;
}

int CScaleAxis::ZoomToMax(void)
{
	SetDataExtent(m_dMinData, m_dMaxData);
	return 0;
}






/////////////////////////////////////////////////////////////////////////
//******* axis **********************************************************

void DoubleToStr(CString& str, double x, int decimals)
{
	CString strFormat;
	CString strDec;
	
	strDec.Format("%d", decimals);
	strFormat = "%." + strDec + "f";
	str.Format(strFormat, x);
}

void CScaleAxis::FindAxisMinMaxStep(DWORD n)
{
	// quit if automatic if dasabled
	if( !m_bAutoTL && m_dTickStart!=DBL_MAX ) return;
	if( n == -1 ) n = m_nTicks;
	double a, b, end, dec;

	a = m_dMax - m_dMin;
	if(a == 0) return;
	b = 1.0;
	if(a > 1) {
		b = 1.0;
		while(a > 10) {
			a /= 10;
			b *= 10;
		}
	}
	else {
		b = 1.0;
		while(a <= 1) {
			a *= 10;
			b /= 10;
		}
	}
	m_dTickStart = floor(m_dMin/b) * b;
	end = ceil(m_dMax/b) * b;
	a = (end - m_dTickStart) / n;
	b = 1.0;
	if(a > 1) {
		while(a >= 10) {
			a /= 10;
			b *= 10;
		}
	}
	else {
		while(a <= 1) {
			a *= 10;
			b /= 10;
		}
	}
	m_dTickStep = floor(((end - m_dTickStart) /n)/b) * b;

	dec = log10(m_dTickStep/20);
	if( dec < 0 ) {
		dec = ceil( fabs(dec) )-1;
		m_nDigitsMajor = (int)dec;
		m_nDigitsMinor = (int)dec+1;
	}
}

void CScaleAxis::DrawAxisX(CDC* pDC)
{
	int x;
	double	sc, off;
	int		w, dw, dwm, ny;
	sc  = (double) (m_nR-m_nL) / ((m_nMax - m_nMin)/m_sc);
	off = m_nL - sc*(( m_nMin-m_off)/m_sc);

	w = m_nT-m_nB;
	dw = w/3;
	dwm = w/6;
	ny = m_nB + w + GetWidth(pDC);

	FindAxisMinMaxStep(m_nTicks);
	CString label;
	CSize	sl=m_ar;					// arrow
	CSize	sa=m_ar;					// arrow
	CSize	smt = m_tickM;				// major ticks
	CSize	snt = m_tickN;				// minor ticks

	if( !pDC->IsPrinting() ) {
		pDC->DPtoLP( &sl );
		pDC->DPtoLP( &sa );
		pDC->DPtoLP( &smt );
		pDC->DPtoLP( &snt );
	}
	else {
		smt.cy = 300;
		snt.cy = 150;
	}

	// create pen
	CPen pen(PS_SOLID, 0, RGB(80,80,80));
	CPen* pPen = pDC->SelectObject(&pen);
	// create fonts
	CFont fontML, fontNL;				// major and minor lalbel fonts
	CFont *pFontML, *pFontNL;
	fontML.CreatePointFontIndirect(&m_lfTickMajor, pDC);
	fontNL.CreatePointFontIndirect(&m_lfTickMinor, pDC);

	//int nl = m_len+sl.cx;

	//************* ticks ***********************
	double	xx = m_dTickStart;
	double	step = m_dTickStep;
	int		ud = m_nOrientLabel = 1;			// labels Up or Down
	pDC->MoveTo(m_nR, ny);
	pDC->LineTo(m_nR, ny - smt.cy*ud);
	pDC->MoveTo(m_nL, ny);
	pDC->LineTo(m_nL, ny - smt.cy*ud);
	if( !pDC->IsPrinting()) {
		pDC->MoveTo(m_nR, m_nT );
		pDC->LineTo(m_nR, m_nB);
	}
	while(xx <= m_dMax) {
		//+++++++++++ major ticks +++++++++++
		if(m_bTickMajor && xx >= m_dMin) {
			x=(int) (off + sc * xx);
			pDC->MoveTo(x, ny);
			pDC->LineTo(x , ny - smt.cy * ud);
			if( !pDC->IsPrinting()) {
				pDC->MoveTo(x , m_nT - dwm);
				pDC->LineTo(x , m_nT - 5*dwm);
			}
			if(m_bLabelMajor) {
				pFontML = pDC->SelectObject(&fontML);
				DoubleToStr(label, xx, m_nDigitsMajor);
				if(ud == -1) {
					pDC->SetTextAlign(TA_BOTTOM | TA_CENTER);
				}
				else {
					pDC->SetTextAlign(TA_CENTER);
				}
				pDC->TextOut((int) (off + sc*xx /*+ m_majorTL.nLOffsetX*/), 
					ny - smt.cy * ud /*- m_majorTL.nLOffsetY * ud*/, label);
				pDC->SelectObject(pFontML);
			}
		}
		//++++++++++ end major ticks ++++++++
 		xx += step;
		//-------------minor ticks ----------
		double xn = xx - step/2;
		if( m_bTickMinor && xn<=m_dMax  && xn>=m_dMin ) {
			x = (int) (off + sc * xn);
			pDC->MoveTo(x , ny);
			pDC->LineTo(x , ny - snt.cy * ud);
			if( !pDC->IsPrinting()) {
				pDC->MoveTo(x , m_nT - dw);
				pDC->LineTo(x , m_nT - 2*dw);
			}
			if(m_bLabelMinor) {
				pFontNL = pDC->SelectObject(&fontNL);
				DoubleToStr(label, xn, m_nDigitsMinor);
				if(ud == -1) {
					pDC->SetTextAlign(TA_BOTTOM | TA_CENTER);
				}
				else {
					pDC->SetTextAlign(TA_CENTER);
				}
				pDC->TextOut((int) (off + sc * xn/* + m_minorTL.nLOffsetX*/), 
					ny - smt.cy * ud/* - m_minorTL.nLOffsetY * ud*/, label);
				pDC->SelectObject(pFontNL);
			}
		}
		
		//---------- end minor ticks --------
	}
	//************ end ticks ********************
	fontML.DeleteObject();
	fontNL.DeleteObject();
	pDC->SelectObject(pPen);

	if(m_bAutoTL) {
		pDC->MoveTo(m_nL, ny);
		pDC->LineTo(m_nR, ny);
	}
	else {
		CPen penR(PS_SOLID, 0, RGB(100,0,0));
		CPen* pen2 = pDC->SelectObject(&penR);
			pDC->MoveTo(m_nL, ny);
			pDC->LineTo(m_nR, ny);
		pDC->SelectObject(pen2);
	}

}

void CScaleAxis::DrawAxisXl(CDC* pDC)
{
	double sc, off;
	sc  = (double) (m_nR-m_nL) / ((m_nMax - m_nMin)/m_sc);
	off = m_nL - sc*(( m_nMin-m_off)/m_sc);

	FindAxisMinMaxStep(m_nTicks);
	CString label;
	CSize	sl=m_ar;					// arrow
	CSize	sa=m_ar;					// arrow
	CSize	smt = m_tickM;				// major ticks
	CSize	snt = m_tickN;				// minor ticks

	pDC->DPtoLP( &sl );
	pDC->DPtoLP( &sa );
	pDC->DPtoLP( &smt );
	pDC->DPtoLP( &snt );

	// create pen
	CPen pen(PS_SOLID, 0, RGB(80,80,80));
	CPen* pPen = pDC->SelectObject(&pen);
	// create fonts
	CFont fontML, fontNL;				// major and minor lalbel fonts
	CFont *pFontML, *pFontNL;
	fontML.CreatePointFontIndirect(&m_lfTickMajor, pDC);
	fontNL.CreatePointFontIndirect(&m_lfTickMinor, pDC);

	//int nl = m_len+sl.cx;
	pDC->MoveTo(m_nL, m_nT);
	pDC->LineTo(m_nR, m_nT);

	//************* ticks ***********************
	double	xx = m_dTickStart;
	double	step = m_dTickStep;
	int		ud = m_nOrientLabel;			// labels Up or Down
	pDC->MoveTo(m_nR, m_nT);
	pDC->LineTo(m_nR, m_nT - smt.cy*ud);
	while(xx <= m_dMax) {
		//+++++++++++ major ticks +++++++++++
		if(m_bTickMajor && xx >= m_dMin) {
			pDC->MoveTo((int) (off + sc * xx) , m_nT);
			pDC->LineTo((int) (off + sc * xx) , m_nT - smt.cy * ud);
			if(m_bLabelMajor) {
				pFontML = pDC->SelectObject(&fontML);
				DoubleToStr(label, xx, m_nDigitsMajor);
				if(ud == -1) {
					pDC->SetTextAlign(TA_BOTTOM | TA_CENTER);
				}
				else {
					pDC->SetTextAlign(TA_CENTER);
				}
				pDC->TextOut((int) (off + sc*xx /*+ m_majorTL.nLOffsetX*/), 
					m_nT - smt.cy * ud /*- m_majorTL.nLOffsetY * ud*/, label);
				pDC->SelectObject(pFontML);
			}
		}
		//++++++++++ end major ticks ++++++++
 		xx += step;
		//-------------minor ticks ----------
		double xn = xx - step/2;
		if( m_bTickMinor && xn<=m_dMax  && xn>=m_dMin ) {
			pDC->MoveTo((int) (off + sc * xn) , m_nT);
			pDC->LineTo((int) (off + sc * xn) , m_nT - snt.cy * ud);
			if(m_bLabelMinor) {
				pFontNL = pDC->SelectObject(&fontNL);
				DoubleToStr(label, xn, m_nDigitsMinor);
				if(ud == -1) {
					pDC->SetTextAlign(TA_BOTTOM | TA_CENTER);
				}
				else {
					pDC->SetTextAlign(TA_CENTER);
				}
				pDC->TextOut((int) (off + sc * xn/* + m_minorTL.nLOffsetX*/), 
					m_nT - smt.cy * ud/* - m_minorTL.nLOffsetY * ud*/, label);
				pDC->SelectObject(pFontNL);
			}
		}
		
		//---------- end minor ticks --------
	}
	//************ end ticks ********************
	fontML.DeleteObject();
	fontNL.DeleteObject();
	pDC->SelectObject(pPen);
}



void CScaleAxis::DrawAxisYl(CDC *pDC)
{
	double sc, off;
	sc  = (double) (m_nT-m_nB) / ((m_nMax - m_nMin)/m_sc);
	off = m_nB - sc*((m_nMin-m_off)/m_sc);

	FindAxisMinMaxStep(m_nTicks);
	CString label;
	CSize	sl=m_ar;					// arrow
	CSize	sa=m_ar;					// arrow
	CSize	smt = m_tickM;				// major ticks
	CSize	snt = m_tickN;				// minor ticks
	CSize	sfm(2, -4);					// minor ticks
	CSize	sfn(2, -3);					// minor ticks

	pDC->DPtoLP( &sl );
	pDC->DPtoLP( &sa );
	pDC->DPtoLP( &smt );
	pDC->DPtoLP( &snt );
	pDC->DPtoLP( &sfm );
	pDC->DPtoLP( &sfn );

	// create pen
	CPen pen(PS_SOLID, 0, RGB(80,80,80));
	CPen* pen1 = pDC->SelectObject(&pen);
	// create fonts
	CFont fontML, fontNL;				// major and minor lalbel fonts
	CFont *pFontML, *pFontNL;
	fontML.CreatePointFontIndirect(&m_lfTickMajor, pDC);
	fontNL.CreatePointFontIndirect(&m_lfTickMinor, pDC);
	
	pDC->SetTextAlign(TA_CENTER | TA_BASELINE);
	int lr = m_nOrientLabel;	// labels Left or Right
	
	pDC->MoveTo(m_nR, m_nB);
	pDC->LineTo(m_nR, m_nT);
	
	//************* ticks ***********************
	pDC->MoveTo(m_nR, m_nB);
	pDC->LineTo(m_nR - smt.cy*lr, m_nB );
	double yy = m_dTickStart;
	double step = m_dTickStep;
	while(yy <= m_dMax) {
		//+++++++++++ major ticks +++++++++++
		if(m_bTickMajor && yy >= m_dMin) {
			pDC->MoveTo(m_nR, (int) ((off +  sc * yy)));
			pDC->LineTo(m_nR - smt.cy * lr, (int) ((off + sc * yy)));
			if(m_bLabelMajor) {
				pFontML = pDC->SelectObject(&fontML);
				DoubleToStr(label, yy, m_nDigitsMajor);
				if(lr == 1) {
					pDC->SetTextAlign(TA_RIGHT | TA_BASELINE);
				}
				else {
					pDC->SetTextAlign(TA_LEFT | TA_BASELINE);
				}
				pDC->TextOut(m_nR - smt.cy * lr + sfm.cx, (int) (off + yy*sc + sfm.cy), label);
				pDC->SelectObject(pFontML);
			}
		}
		//++++++++++ end major ticks ++++++++
		yy += step;
		//-------------minor ticks ----------
		double yn = yy - step/2;
		if((m_bTickMinor) && (yn <= m_dMax) && yn >= m_dMin) {
			pDC->MoveTo(m_nR, (int) (off + sc * yn) );
			pDC->LineTo(m_nR - snt.cy * lr, (int) (off + sc * yn) );
			if(m_bLabelMinor) {
				pFontNL = pDC->SelectObject(&fontNL);
				DoubleToStr(label, yn, m_nDigitsMinor);
				if(lr == 1) {
					pDC->SetTextAlign(TA_RIGHT | TA_BASELINE);
				}
				else {
					pDC->SetTextAlign(TA_LEFT | TA_BASELINE);
				}
				pDC->TextOut(m_nR - snt.cy * lr + sfn.cx, (int) (off + sc*yn + sfn.cy), label);
				pDC->SelectObject(pFontNL);;
			}
		}
		//---------- end minor ticks --------
	}
	//************ end ticks ********************
	pDC->SelectObject(pen1);
	fontML.DeleteObject();
	fontNL.DeleteObject();
}

void CScaleAxis::DrawAxisY(CDC *pDC)
{
	double	sc, off;
	int		y, nx, w, dw, dwm;
	sc  = (double) (m_nT-m_nB) / ((m_nMax - m_nMin)/m_sc);
	off = m_nB - sc*((m_nMin-m_off)/m_sc);

	w = m_nR-m_nL;
	dw = w/3;
	dwm = w/6;
	//nx = w + m_nWidth;
	if(m_bWidth)
		nx = m_nWidth;
	else
		nx = w + m_nWidth;

	FindAxisMinMaxStep(m_nTicks);
	CString label;
	CSize	sl=m_ar;					// arrow
	CSize	sa=m_ar;					// arrow
	CSize	smt = m_tickM;				// major ticks
	CSize	snt = m_tickN;				// minor ticks
	CSize	sfm(2, -4);					// minor ticks
	CSize	sfn(2, -3);					// minor ticks

	if( !pDC->IsPrinting() ) {
		pDC->DPtoLP( &sl );
		pDC->DPtoLP( &sa );
		pDC->DPtoLP( &smt );
		pDC->DPtoLP( &snt );
		//pDC->DPtoLP( &sfm );
		//pDC->DPtoLP( &sfn );
	}
	else {
		smt.cx = 300;
		snt.cx = 150;
	}


	// create pen
	CPen pen(PS_SOLID, 0, RGB(80,80,80));
	CPen* pen1 = pDC->SelectObject(&pen);
	// create fonts
	CFont fontML, fontNL;				// major and minor lalbel fonts
	CFont *pFontML, *pFontNL;
	fontML.CreatePointFontIndirect(&m_lfTickMajor, pDC);
	fontNL.CreatePointFontIndirect(&m_lfTickMinor, pDC);
	
	pDC->SetTextAlign(TA_CENTER | TA_BASELINE);
	int lr = m_nOrientLabel = 1;	// labels Left or Right
	

	if(m_bAutoTL) {
		pDC->MoveTo(nx, m_nB);
		pDC->LineTo(nx, m_nT);
	}
	else {
		CPen penR(PS_SOLID, 0, RGB(100,0,0));
		CPen* pen2 = pDC->SelectObject(&penR);
			pDC->MoveTo(nx, m_nB);
			pDC->LineTo(nx, m_nT);
		pDC->SelectObject(pen2);
	}

	//************* ticks ***********************
	pDC->MoveTo(nx, m_nT); 
	pDC->LineTo(nx - smt.cy * lr, m_nT );
	pDC->MoveTo(nx, m_nB); 
	pDC->LineTo(nx - smt.cy * lr, m_nB );
	if( !pDC->IsPrinting()) {
		pDC->MoveTo(m_nR, m_nB);
		pDC->LineTo(m_nR - w, m_nB );
	}
	double yy = m_dTickStart;
	double step = m_dTickStep;
	while(yy <= m_dMax) {
		//+++++++++++ major ticks +++++++++++
		if(m_bTickMajor && yy >= m_dMin) {
			y = (int) ((off +  sc * yy));
			pDC->MoveTo(nx, y);
			pDC->LineTo(nx - smt.cx * lr, y);
			if( !pDC->IsPrinting()) {
				pDC->MoveTo(m_nR - 2*dwm, y);
				pDC->LineTo(m_nR - dwm*6, y);
			}
			if(m_bLabelMajor) {
				pFontML = pDC->SelectObject(&fontML);
				DoubleToStr(label, yy, m_nDigitsMajor);
				label = " " + label + " ";
				//label += " ";
				if(lr == 1) {
					pDC->SetTextAlign(TA_RIGHT | TA_BOTTOM);
				}
				else {
					pDC->SetTextAlign(TA_LEFT | TA_BOTTOM);
				}
				sfm = pDC->GetTextExtent(label);
				sfm.cy /= 2;
				pDC->TextOut(nx - smt.cx * lr/* + sfm.cx*/, (int) (off + yy*sc - sfm.cy), label);
				pDC->SelectObject(pFontML);
			}
		}
		//++++++++++ end major ticks ++++++++
		yy += step;
		//-------------minor ticks ----------
		double yn = yy - step/2;
		if((m_bTickMinor) && (yn <= m_dMax) && yn >= m_dMin) {
			y = (int) (off + sc * yn);
			pDC->MoveTo(nx, y );
			pDC->LineTo(nx - snt.cx * lr, y );
			if( !pDC->IsPrinting()) {
				pDC->MoveTo(m_nR - dw, y );
				pDC->LineTo(m_nR - 2*dw, y );
			}
			if(m_bLabelMinor) {
				pFontNL = pDC->SelectObject(&fontNL);
				DoubleToStr(label, yn, m_nDigitsMinor);
				label += " ";
				if(lr == 1) {
					pDC->SetTextAlign(TA_RIGHT | TA_BOTTOM);
				}
				else {
					pDC->SetTextAlign(TA_LEFT | TA_BOTTOM);
				}
				sfn = pDC->GetTextExtent(label);
				sfn.cy /= -2;
				pDC->TextOut(nx - snt.cx * lr/* + sfn.cx*/, (int) (off + sc*yn + sfn.cy), label);
				pDC->SelectObject(pFontNL);;
			}
		}
		//---------- end minor ticks --------
	}
	if( !pDC->IsPrinting()) {
		pDC->MoveTo(m_nR, m_nT);
		pDC->LineTo(m_nR - w, m_nT);
	}
	//************ end ticks ********************
	pDC->SelectObject(pen1);
	fontML.DeleteObject();
	fontNL.DeleteObject();
}

int CScaleAxis::Properties()
{
	// returns TRUE if changed FALSE otherwise
	CDlgAxis	dlg;

	dlg.m_nType = m_nType;
	dlg.m_bAutoTL = m_bAutoTL;
	dlg.m_bLabelMajor = m_bLabelMajor;
	dlg.m_bLabelMinor = m_bLabelMinor;
	dlg.m_bTickMajor = m_bTickMajor;
	dlg.m_bTickMinor = m_bTickMinor;
	
	dlg.m_fDataMax = (float) m_dMaxData;
	dlg.m_fDataMin = (float) m_dMinData;
	dlg.m_fMax = (float)m_dMax;
	dlg.m_fMin = (float)m_dMin;
	dlg.m_fTickStep = (float)m_dTickStep;
	dlg.m_fTickStart = (float)m_dTickStart;

	dlg.m_nDigitsMajor = m_nDigitsMajor;
	dlg.m_nDigitsMinor = m_nDigitsMinor;
	dlg.m_nTicks = m_nTicks;

	dlg.m_lfLabelMajor = m_lfTickMajor;
	dlg.m_lfLabelMinor = m_lfTickMinor;
	
	dlg.m_bGLMajor = m_bGLMajor;
	dlg.m_bGLMinor = m_bGLMinor;
	if( dlg.DoModal() == IDOK ) {
		m_nType = dlg.m_nType;
		m_bAutoTL = dlg.m_bAutoTL;
		m_bLabelMajor = dlg.m_bLabelMajor;
		m_bLabelMinor = dlg.m_bLabelMinor;
		m_bTickMajor = dlg.m_bTickMajor;
		m_bTickMinor = dlg.m_bTickMinor;
		
		m_dMaxData = dlg.m_fDataMax;
		m_dMinData = dlg.m_fDataMin;
		SetDataExtent( dlg.m_fMin, dlg.m_fMax );
		m_dTickStep = dlg.m_fTickStep;
		if( !m_bAutoTL )
			m_dTickStart = dlg.m_fTickStart;

		m_nDigitsMajor = dlg.m_nDigitsMajor;
		m_nDigitsMinor = dlg.m_nDigitsMinor;
		m_nTicks = dlg.m_nTicks;

		m_lfTickMajor = dlg.m_lfLabelMajor;
		m_lfTickMinor = dlg.m_lfLabelMinor;
		m_bGLMajor = dlg.m_bGLMajor;
		m_bGLMinor = dlg.m_bGLMinor;
		return TRUE;
	}
	return FALSE;
}

void CScaleAxis::Move(int nTag)
{
	double s = (m_dMax-m_dMin)/100;
	switch( nTag ) {
		case AMT_MINUS:
			if(m_dMin-s > m_dMinData)
				SetDataExtent(m_dMin-s, m_dMax-s);
			if(m_dMin-s < m_dMinData && m_dMin != m_dMinData)
				SetDataExtent(m_dMinData, m_dMax-s);
			break;
		case AMT_PLUS:
			if(m_dMax+s < m_dMaxData)
				SetDataExtent(m_dMin+s, m_dMax+s);
			if(m_dMax+s > m_dMaxData && m_dMax != m_dMaxData)
				SetDataExtent(m_dMin+s, m_dMaxData);
			break;
	}
}

void CScaleAxis::DrawGridLines(CDC* pDC, CRect lpRect)
{
	double sc, off;
	sc  = (double) (m_nT-m_nB) / ((m_nMax - m_nMin)/m_sc);
	off = m_nB - sc*((m_nMin-m_off)/m_sc);

	// create pen
	CPen pen( PS_SOLID, 0, RGB(100, 100, 100) );
	CPen* pPen = pDC->SelectObject(&pen);

	double yy = m_dTickStart;
	double step = m_dTickStep;
	if( m_nType == SB_VERT ) {
		double yy = m_dTickStart;
		double step = m_dTickStep;
		while(yy <= m_dMax) {
			//+++++++++++ major Grid Lines +++++++++++
			if(m_bGLMajor && yy >= m_dMin) {
				pDC->MoveTo(lpRect.left, (int) (off + sc * yy));
				pDC->LineTo(lpRect.right, (int) (off + sc * yy) );
			}
			//++++++++++ end major ticks ++++++++
			yy += step;
			//-------------minor ticks ----------
			double yn = yy - step/2;
			if((m_bGLMinor) && (yn <= m_dMax) && yn >= m_dMin) {
				pDC->MoveTo(lpRect.left, (int) (off + sc * yn));
				pDC->LineTo(lpRect.right, (int) (off + sc * yn) );
			}
			//---------- end minor ticks --------
		}
	}

	sc  = (double) (m_nR-m_nL) / ((m_nMax - m_nMin)/m_sc);
	off = m_nL - sc*(( m_nMin-m_off)/m_sc);
	double	xx = m_dTickStart;
	if( m_nType == SB_HORZ ) {
		while(xx <= m_dMax) {
			//+++++++++++ major ticks +++++++++++
			if(m_bGLMajor && xx >= m_dMin) {
				pDC->MoveTo((int) (off + sc * xx) , lpRect.bottom);
				pDC->LineTo((int) (off + sc * xx) , lpRect.top);
			}
			//++++++++++ end major ticks ++++++++
 			xx += step;
			//-------------minor ticks ----------
			double xn = xx - step/2;
			if( m_bGLMinor && xn<=m_dMax  && xn>=m_dMin ) {
				pDC->MoveTo((int) (off + sc * xn) , lpRect.bottom);
				pDC->LineTo((int) (off + sc * xn) , lpRect.top);
			}
			//---------- end minor ticks --------
		}
	}

	pDC->SelectObject(pPen);
}

int CScaleAxis::GetWidth(CDC* pDC)
{
	CString	str;
	CSize	szMin, szMax;
	CSize	smt;
	int width;
	CFont font;				// major and minor lalbel fonts
	CFont *pFont;
	font.CreatePointFontIndirect(&m_lfTickMajor, pDC);
	pFont = pDC->SelectObject(&font);
	
	DoubleToStr(str, m_dMax, m_nDigitsMajor);
	str = " " + str + " ";
	szMax = pDC->GetTextExtent(str);
	DoubleToStr(str, m_dMin, m_nDigitsMajor);
	str = " " + str + " ";
	szMin = pDC->GetTextExtent(str);


	if( !pDC->IsPrinting() ) {
		smt = m_tickM;				// major ticks
		pDC->DPtoLP( &smt );
	}
	else {
		smt.cy = 300;
		smt.cx = 300;
	}

	if(m_nType == SB_VERT) {
		width = max(szMin.cx, szMax.cx) + smt.cx;
	}
	if(m_nType == SB_HORZ) {
		width = max(szMin.cy, szMax.cy) + smt.cy;
	}

	if(!m_bWidth)
		m_nWidth = width;

	pDC->SelectObject(pFont);
	return width;
}

void CScaleAxis::SetCustomWidth(BOOL b)		
{
	m_bWidth = b; 
}

// scales the axis
// if dFactor < 1.0 -> ZoomIn
// if dFactor > 1.0 -> ZoomOut
void CScaleAxis::ZoomAxis(double dMid, double dFactor)
{
	double d, s, rem;
	double min, max;

	d = (m_dMax - m_dMin);
	s = d*dFactor;

	if( fabs(s) > (m_dMaxData - m_dMinData) ) {
		m_dMax = m_dMaxData;
		m_dMin = m_dMinData;
		return;
	}

	if( (dMid == 1.0e+100) || (dMid<m_dMin) || (dMid >m_dMax) )
		dMid = (m_dMax + m_dMin)/2;	// middle point
	min = dMid - s/2;
	max = dMid + s/2;

	if( min > max )		return;;
	if( m_dMinData > min )	{
		rem = m_dMinData-min;
		min = m_dMinData;
		max += rem;
	}
	if( m_dMaxData < max )	{
		rem = m_dMaxData-max;
		max = m_dMaxData;
		min += rem;
	}
	if( m_dMinData > min )	{
		min = m_dMinData;
	}
	m_dMax = max;
	m_dMin = min;


	Scale();
	m_nMax = (int) (m_off + m_sc*m_dMax);
	m_nMin = (int) (m_off + m_sc*m_dMin);

}
