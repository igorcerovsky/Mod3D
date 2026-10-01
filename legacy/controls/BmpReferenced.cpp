#include "stdafx.h"
#include "bmpreferenced.h"
#include <comdef.h>
#include <afxstr.h>
#include <math.h>
#include "DlgImportPicture.h"
#include "Mod3DDoc.h"


IMPLEMENT_SERIAL( CImgReferenced, CMyObject, VERSIONABLE_SCHEMA|1 )

CImgReferenced::CImgReferenced(void)
{
	m_bVisible = TRUE;
	m_bVisibleRow = TRUE;
	m_bVisibleCol = TRUE;

	m_nType = IMG_MAP;
	m_nProfOrient = -1;
	m_nProf = 0;
	m_nOpacity = 150;

	m_xs = 0;
	m_ys = 8000;
	m_zs = 0;

	m_xe = 8000;
	m_ye = 0;
	m_ze = 0;

	m_strName = "Enter Description!";

	m_logFont.lfHeight = 100;	// !!! CreatePointFontIndirect
	m_logFont.lfWidth = 0;
	m_logFont.lfEscapement = 0;
	m_logFont.lfOrientation = 0;
	m_logFont.lfWeight = FW_NORMAL;
	m_logFont.lfItalic = FALSE;
	m_logFont.lfUnderline = FALSE;
	m_logFont.lfStrikeOut = 0;
	m_logFont.lfCharSet = ANSI_CHARSET;
	m_logFont.lfOutPrecision = OUT_DEFAULT_PRECIS;
	m_logFont.lfClipPrecision = CLIP_CHARACTER_PRECIS;
	m_logFont.lfQuality = DEFAULT_QUALITY;
	m_logFont.lfPitchAndFamily = DEFAULT_PITCH | FF_SWISS;
	strcpy(m_logFont.lfFaceName, "Arial");    

	m_logPen.lopnColor = RGB(100, 100, 100);
	m_logPen.lopnStyle = PS_SOLID;
	m_logPen.lopnWidth.x = 1;

	m_pen.CreatePenIndirect( &m_logPen );
}

CImgReferenced::~CImgReferenced(void)
{
}

void CImgReferenced::Serialize(CArchive& ar)
{
	CMyObject::Serialize( ar );

	if (ar.IsStoring())	{
		ar	<< m_strFilePath << m_strFilePathRelative
			<< m_nType 
			<< m_nProfOrient << m_nProf
			<< m_bVisible << m_bVisibleCol << m_bVisibleRow;
		ar	<< m_nOpacity;
		ar	<< m_xs << m_ys << m_zs;
		ar	<< m_xe << m_ye << m_ze;
		// font
		ar << m_logPen.lopnColor << m_logPen.lopnStyle << m_logPen.lopnWidth.x;
	}
	else	{	
		ar	>> m_strFilePath >> m_strFilePathRelative
			>> m_nType 
			>> m_nProfOrient >> m_nProf
			>> m_bVisible >> m_bVisibleCol >> m_bVisibleRow;
		ar	>> m_nOpacity;
		ar	>> m_xs >> m_ys >> m_zs;
		ar	>> m_xe >> m_ye >> m_ze;
		// font
		ar >> m_logPen.lopnColor >> m_logPen.lopnStyle >> m_logPen.lopnWidth.x;

		// load picture
		HRESULT hResult;
		hResult = m_img.Load( m_strFilePath );
		if (FAILED(hResult)) {
			//CString fmt;
			//fmt.Format("Load image failed: %s\nLoad new image?", m_strFilePathRelative );
			//int nRet = AfxMessageBox(fmt, MB_YESNO | MB_ICONQUESTION);
			//if( nRet == IDYES ) {
			//	Open();
			//}
		}
		else {
			Flip();
		}
		// set font angle
		m_logFont.lfEscapement = (int) (-10 * (180/PI) * atan( (m_ys-m_ye)/(m_xs-m_xe) ) );

		//m_img.Load( m_strFilePath );
	}
}

int CImgReferenced::Properties(void)
{
	CDlgImportPicture dlg;

	dlg.m_bVisible = m_bVisible;
	dlg.m_nPicType = m_nType;
	dlg.m_nOpacity = m_nOpacity;

	dlg.m_xs = (float) m_xs;
	dlg.m_ys = (float) m_ys;
	dlg.m_zs = (float) m_zs;

	dlg.m_xe = (float) m_xe;
	dlg.m_ye = (float) m_ye;
	dlg.m_ze = (float) m_ze;

	dlg.m_bCol = m_bVisibleCol;
	dlg.m_bRow = m_bVisibleRow;

	dlg.m_nProfOrient = m_nProfOrient;
	dlg.m_nProfNum = m_nProf;
	dlg.m_strName  = m_strName;

	dlg.m_pImg = this;
	dlg.m_strFilePath = m_strFilePath;

	dlg.m_lpLine = m_logPen;

	if( dlg.DoModal() == IDOK ) {
		SetName( dlg.m_strName );
		m_bVisible = dlg.m_bVisible;
		m_nType = dlg.m_nPicType;
		m_nProfOrient = dlg.m_nProfOrient;
		m_nProf = dlg.m_nProfNum;
		m_nOpacity = dlg.m_nOpacity;

		m_bVisibleCol = dlg.m_bCol;
		m_bVisibleRow = dlg.m_bRow;

		m_xs = dlg.m_xs;
		m_ys = dlg.m_ys;
		m_zs = dlg.m_zs;

		m_xe = dlg.m_xe;
		m_ye = dlg.m_ye;
		m_ze = dlg.m_ze;

		m_strFilePath = dlg.m_strFilePath;

		// set font angle
		m_logFont.lfEscapement = (int) (-10 * (180/PI) * atan( (m_ys-m_ye)/(m_xs-m_xe) ) );

		m_logPen = dlg.m_lpLine;

		return TRUE;
	}

	return 0;
}

BOOL CImgReferenced::Open(void)
{
	CString strFilter;
	CSimpleArray<GUID> aguidFileTypes;
	HRESULT hResult;

	hResult = m_img.GetExporterFilterString(strFilter, aguidFileTypes, _T( "All Image Files" ));
	if (FAILED(hResult)) {
		CString fmt;
		fmt.Format("GetExporterFilter failed:\n%x - %s", hResult, _com_error(hResult).ErrorMessage());
		::AfxMessageBox(fmt);
		return false;
	}

	CFileDialog dlg(TRUE, NULL, NULL, OFN_FILEMUSTEXIST, strFilter);
	dlg.m_ofn.nFilterIndex = 1;
	hResult = (int)dlg.DoModal();
	if(hResult != IDOK) {
		return false;
	}
	m_strFilePath = dlg.GetPathName();

	m_img.Destroy();
	hResult = m_img.Load( dlg.GetPathName() );
	m_strFilePath = dlg.GetPathName();
	if (FAILED(hResult)) {
		CString fmt;
		fmt.Format("Load image failed:\n%x - %s", hResult, _com_error(hResult).ErrorMessage());
		::AfxMessageBox(fmt);
		return false;
	}

	Flip();

	return true;
}

BOOL CImgReferenced::Flip()
{
	if ( m_img.IsNull() ) return FALSE;

	int width = m_img.GetWidth();
	int height = m_img.GetHeight();

	int pitch, absPitch, bpp;
	BYTE* imgData;
	BYTE* imgPitch;

	imgData = (BYTE*) m_img.GetBits();
	pitch = m_img.GetPitch();
	bpp = m_img.GetBPP();

	if(pitch < 0) {
		absPitch = -pitch;
	}
	imgPitch = new BYTE[absPitch];
	for(int i=0; i < height/2; i++) {
		memcpy(imgPitch, imgData+i*pitch, absPitch);
		memcpy(imgData+i*pitch, imgData+(height-1-i)*pitch, absPitch);
		memcpy(imgData+(height-1-i)*pitch, imgPitch, absPitch);
	}

	int w, h;
	w = m_img.GetWidth();
	h = m_img.GetHeight();
	if( !m_imgFlipH.IsNull() ) {
		m_imgFlipH.ReleaseDC();
		m_imgFlipH.Destroy();
	}
	m_imgFlipH.Create(w, h, m_img.GetBPP());
	BOOL b = m_img.StretchBlt(m_imgFlipH.GetDC(), w, 0, -w, h, 0, 0, w, h);

	return TRUE;
}



int CImgReferenced::DrawProfile(CDC* pDC, double xSc, double xOff, double ySc, double yOff, int nProfType, double xy)
{
	if( m_img.IsNull() ) return 0;

	int		x, y, w, h;
	double	xx, yy;
	double rot;
	CSize	szArrow(3,8);
	pDC->DPtoLP(&szArrow);
	rot = m_xe-m_xs;
	if( nProfType == PRF_HRZ && m_bVisibleRow && rot!=0) {
		x = (int) ( xOff+ xSc*min(m_xs, m_xe) );
		w = (int) ( xSc*fabs(m_xs-m_xe) );
		y = (int) ( yOff + ySc*m_ze );
		h = (int) ( ySc*(m_zs-m_ze) );
		if( rot > 0 )
			m_img.AlphaBlend( pDC->GetSafeHdc(), x, y, w, h, 0, 0, m_img.GetWidth(), m_img.GetHeight(), m_nOpacity );
		else
			m_imgFlipH.AlphaBlend( pDC->GetSafeHdc(), x, y, w, h, 0, 0, m_img.GetWidth(), m_img.GetHeight(), m_nOpacity );

		if( FindIntersection( nProfType, xy, xx, yy ) ) {
			CPen pen(PS_SOLID, 1, m_logPen.lopnColor), *pPen;
			pPen = pDC->SelectObject( &pen );
			pDC->MoveTo( (int)(xOff + xSc*xx), y-szArrow.cy ); 
			pDC->LineTo( (int)(xOff + xSc*xx), y ); 
			pDC->LineTo( (int)(xOff + xSc*xx)- szArrow.cx, y-szArrow.cx ); 
			pDC->MoveTo( (int)(xOff + xSc*xx), y ); 
			pDC->LineTo( (int)(xOff + xSc*xx)+ szArrow.cx, y-szArrow.cx ); 

			pDC->MoveTo( (int)(xOff + xSc*xx), y+h+szArrow.cy ); 
			pDC->LineTo( (int)(xOff + xSc*xx), y+h ); 
			pDC->LineTo( (int)(xOff + xSc*xx)- szArrow.cx, y+h+szArrow.cx ); 
			pDC->MoveTo( (int)(xOff + xSc*xx), y+h ); 
			pDC->LineTo( (int)(xOff + xSc*xx)+ szArrow.cx, y+h+szArrow.cx ); 

			pDC->SelectObject( pPen );
		}
	}

	rot = m_ye-m_ys;
	if( nProfType == PRF_VRT && m_bVisibleCol && rot!=0) {
		x = (int) ( xOff + xSc*min(m_ys, m_ye) );
		w = (int) ( xSc*fabs(m_ys-m_ye) );
		y = (int) ( yOff + ySc*m_ze );
		h = (int) ( ySc*(m_zs-m_ze) );
		if( rot > 0 )
			m_img.AlphaBlend( pDC->GetSafeHdc(), x, y, w, h, 0, 0, m_img.GetWidth(), m_img.GetHeight(), m_nOpacity );
		else
			m_imgFlipH.AlphaBlend( pDC->GetSafeHdc(), x, y, w, h, 0, 0, m_img.GetWidth(), m_img.GetHeight(), m_nOpacity );

		if( FindIntersection( nProfType, xy, xx, yy ) ) {
			CPen pen(PS_SOLID, 1, m_logPen.lopnColor), *pPen;
			pPen = pDC->SelectObject( &pen );
			pDC->MoveTo( (int)(xOff + xSc*yy), y-szArrow.cy ); 
			pDC->LineTo( (int)(xOff + xSc*yy), y ); 
			pDC->LineTo( (int)(xOff + xSc*yy)- szArrow.cx, y-szArrow.cx ); 
			pDC->MoveTo( (int)(xOff + xSc*yy), y ); 
			pDC->LineTo( (int)(xOff + xSc*yy)+ szArrow.cx, y-szArrow.cx ); 

			pDC->MoveTo( (int)(xOff + xSc*yy), y+h+szArrow.cy ); 
			pDC->LineTo( (int)(xOff + xSc*yy), y+h ); 
			pDC->LineTo( (int)(xOff + xSc*yy)- szArrow.cx, y+h+szArrow.cx ); 
			pDC->MoveTo( (int)(xOff + xSc*yy), y+h ); 
			pDC->LineTo( (int)(xOff + xSc*yy)+ szArrow.cx, y+h+szArrow.cx ); 

			pDC->SelectObject( pPen );
		}
	}

	return 0;
}

int CImgReferenced::DrawMap(CDC* pDC, double xSc, double xOff, double ySc, double yOff)
{
	if( m_img.IsNull() || !m_bVisible ) return 0;

	int x, y, x1, y1, w, h;
	CSize	sz(3, 3);
	pDC->DPtoLP(&sz);

	if( m_nType == IMG_MAP ) {
		x = (int) ( xOff + xSc*min(m_xs, m_xe) );
		w = (int) ( xSc*fabs(m_xs-m_xe) );
		y = (int) ( yOff + ySc*min(m_ys, m_ye) );
		h = (int) ( ySc*fabs(m_ys-m_ye) );
		m_img.AlphaBlend( pDC->GetSafeHdc(), x, y, w, h, 0, 0, m_img.GetWidth(), m_img.GetHeight(), m_nOpacity );
	}

	if( m_nType == IMG_PRF ) {
		CPen *pPen, pen;
		pen.CreatePenIndirect( &m_logPen );
		pPen = pDC->SelectObject( &pen );
		COLORREF	txtColor = pDC->SetTextColor( m_logPen.lopnColor );
		x = (int) ( xOff + xSc*m_xe );
		y = (int) ( yOff + ySc*m_ye );
		pDC->MoveTo( x, y );
		x1 = (int) ( xOff + xSc*m_xs );
		y1 = (int) ( yOff + ySc*m_ys );
		pDC->LineTo( x1, y1 );
		pDC->Ellipse(x1-sz.cx, y1-sz.cy, x1+sz.cx, y1+sz.cy);

		// draw decription
		CFont font, *pFont;
		font.CreatePointFontIndirect( &m_logFont, pDC );
		pFont = pDC->SelectObject( &font );
		pDC->SetTextAlign( TA_BOTTOM | TA_CENTER );
		pDC->TextOut((x+x1)/2, (y+y1)/2, m_strName );
		pDC->SelectObject( pFont );
		pDC->SelectObject( pPen );
		pDC->SetTextColor( txtColor );
	}

	return 0;
}

// finds if this picture belongs to profile
BOOL CImgReferenced::IsOnProfile(int nProfType, double xy, double x0, double y0, double xs, double ys)
{
	// nProfType - horizontal or vertical
	// xz - either x or y coordinate of profile, depends on nProfType
	// x0, y0 grid lover left corner
	// xs, ys gris cell size
	double mid;

	if( m_img.IsNull() || m_nType == IMG_MAP )		return FALSE;

	// horizontal profile
	if( nProfType == PRF_HRZ ) {
		if( min(m_ys, m_ye) <= xy && max(m_ys, m_ye) >= xy && (max(m_xs, m_xe)-min(m_xs, m_xe)) > xs )
			return TRUE;
		mid = ( max(m_ys, m_ye)+min(m_ys, m_ye) ) / 2;
		if( mid-ys < xy && mid+ys > xy  && (max(m_ys, m_ye)-min(m_ys, m_ye)) < ys)		
			return TRUE;
	}

	if( nProfType == PRF_VRT ) {
		if( min(m_xs, m_xe) <= xy && max(m_xs, m_xe) >= xy && (max(m_ys, m_ye)-min(m_ys, m_ye)) > ys) 		
			return TRUE;
		mid = ( max(m_xs, m_xe)+min(m_xs, m_xe) ) / 2;
		if( mid-xs < xy && mid+xs > xy && (max(m_xs, m_xe)-min(m_xs, m_xe)) < xs)					
			return TRUE;
	}

	return FALSE;
}

int CImgReferenced::FindIntersection(int nProfType, double xy, double& x, double& y)
{
	// xy -  coordinate of profile
	// profOrientation - SN or EW profile
	//
	// temporary fills m_pt3DArrayProf
	// finds intersections with profile line 
	// fills variable x, y with intersection point

	// vertical profile
	double a1, a2, a12;
	if( nProfType == PRF_VRT ) {
		a1 = fabs(xy - m_xs);
		a2 = fabs(xy - m_xe);
		a12 = fabs(m_xs-m_xe);

		if( a1 <= a12 && a2 <= a12 ) {
			// find intersection
			x = xy;	y = m_ys;
			if( (m_xs-m_xe) != 0.0 )
				y  = m_ys + (xy-m_xs)*(m_ys-m_ye)/(m_xs-m_xe);
				//y = pt1.y + (xy-pt1.x)*(pt1.y-pt2.y)/(pt1.x-pt2.x);
			return 1;
		}
	}
	// horizontal profile
	if( nProfType == PRF_HRZ ) {
		a1 = fabs(xy - m_ys);			//a1 = fabs(xy - pt1.y);
		a2 = fabs(xy - m_ye);			//a2 = fabs(xy - pt2.y);
		a12 = fabs(m_ys-m_ye);			//a12 = fabs(pt1.y-pt2.y);

		if( a1 <= a12 && a2 <= a12 ) {
			// find intersection
			x = m_xs; y = xy;			//x = pt1.x;	y = xy; z = m_z;
			if( (m_ys-m_ye) != 0.0 )
				x  = m_xs + (xy-m_ys)*(m_xs-m_xe)/(m_ys-m_ye);
				//x = pt1.x + (xy-pt1.y)*(pt1.x-pt2.x)/(pt1.y-pt2.y);
			return 1;
		}
	}

	return 0;
}

void CImgReferenced::GetTopLeft(double& t, double& l)
{
	//if( m_nType == IMG_PRF ) {
	//	t = m_ys;
	//	l = m_xs;
	//}
	if( m_nType == IMG_MAP ) {
		t = m_ys;
		l = m_xs;
	}
}

void CImgReferenced::GetBotRight(double& b, double& r)
{
	if( m_nType == IMG_MAP ) {
		b = m_ye;
		r = m_xe;
	}
}

void CImgReferenced::GetGlDimensionsPow2(int& w, int& h)
{
	int width = m_img.GetWidth();
	int height = m_img.GetHeight();
	int ch = (int) ceil(log(width)/log(2));
	int cw = (int) ceil(log(height)/log(2));
	int wu = (int) pow( 2, ch );
	int hu = (int) pow( 2, cw );
	int wd = (int) pow( 2, ch-1 );
	int hd = (int) pow( 2, cw-1 );

	if(wu!=width) {
		if( ((double)(width-wd))/(wu-width) < 0.5 || width>2048) 
			w = wd;
		else 
			w = wu;
	}
	else
		w = wu;

	if(hu!=height) {
		if( ((double)(height-hd))/(hu-height) < 0.5 || height>2048) 
			h = hd;
		else 
			h = hu;
	}
	else
		h = hu;

	int n = max(h,w);
	h = n;
	w = n;
}

void CImgReferenced::GetPixels(BYTE* pPix, int w, int h, int nFormat)
{
	ASSERT(nFormat==4);
	ASSERT(!m_img.IsNull());
	if(m_img.IsNull()) {
		pPix=NULL;
		return;
	}
	int f=nFormat;

	CImage imgTmp;
	imgTmp.Create(w, h, 32);
	m_img.StretchBlt(imgTmp.GetDC(), 0, 0, w, h, 0, 0, m_img.GetWidth(), m_img.GetHeight());

	COLORREF color;
	COLORREF white = RGB(255, 255, 255);
	for(int i=0; i < h; i++) {
		for(int j=0; j < w; j++) {
			//color = imgTmp.GetPixel(i, j);
			color = imgTmp.GetPixel(j, h-1-i);
			pPix[f*(j*h + i) + 0] = GetRValue(color);
			pPix[f*(j*h + i) + 1] = GetGValue(color);
			pPix[f*(j*h + i) + 2] = GetBValue(color);
			if(color==white) 
				pPix[f*(j*h + i) + 3] = 0;
			else
				pPix[f*(j*h + i) + 3] = 255;
		}
	}
	imgTmp.Destroy();

}
