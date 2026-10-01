#include "stdafx.h"
#include "checkpoint.h"
#include "DlgCheckMark.h"
#include "Mod3DDoc.h"
#include <math.h>

IMPLEMENT_SERIAL(CGuideMark, CMyObject, 1)

void CGuideMark::InitFonts(void)
{
	m_logFont.lfHeight = 80;	// !!! CreatePointFontIndirect
	m_logFont.lfWidth = 0;
	m_logFont.lfEscapement = 0;
	m_logFont.lfOrientation = 0;
	m_logFont.lfWeight = FW_BOLD;
	m_logFont.lfItalic = FALSE;
	m_logFont.lfUnderline = FALSE;
	m_logFont.lfStrikeOut = 0;
	m_logFont.lfCharSet = ANSI_CHARSET;
	m_logFont.lfOutPrecision = OUT_DEFAULT_PRECIS;
	m_logFont.lfClipPrecision = CLIP_CHARACTER_PRECIS;
	m_logFont.lfQuality = DEFAULT_QUALITY;
	m_logFont.lfPitchAndFamily = DEFAULT_PITCH | FF_SWISS;
	strcpy(m_logFont.lfFaceName, "Arial");   

	m_bVisible = TRUE;
	m_bShowText = TRUE;
	m_nType = CHM_POINT;
	
	// pen
	m_logPen.lopnColor = RGB(200, 200, 200);
	m_logPen.lopnStyle = PS_SOLID;
	m_logPen.lopnWidth = CPoint(1,1);
}

CGuideMark::~CGuideMark(void)
{
}

void CGuideMark::Serialize(CArchive& ar)
{
	CMyObject::Serialize( ar );
	if (ar.IsStoring())	{	
		ar	<< m_nCol << m_nRow << m_x << m_y << m_z << m_nType 
			<< m_logPen.lopnColor << m_logPen.lopnStyle << m_logPen.lopnWidth 
			<< m_bShowText;
	}
	else{	// loading code
		ar	>> m_nCol >> m_nRow >> m_x >> m_y >> m_z >> m_nType 
			>> m_logPen.lopnColor >> m_logPen.lopnStyle >> m_logPen.lopnWidth
			>> m_bShowText;
	}
	m_pt3DArray.Serialize( ar );
}

void CGuideMark::Draw(CDC* pDC, double scx, double offx, double scy, double offy)
{
	if( !IsVisible() ) return;

	CSize s(10, 0);
	CSize l(3, 4);
	int x, y;
	double xd, yd;

	pDC->DPtoLP( &s );
	pDC->DPtoLP( &l );
	CFont font;
	font.CreatePointFontIndirect(&m_logFont, pDC);
	CFont* def_font = pDC->SelectObject(&font);
	pDC->SetTextAlign(TA_BOTTOM | TA_CENTER);

	if( m_nType == CHM_POINT ) {
		x = (int) (offx + scx*m_x);
		y = (int) (offy + scy*m_z);
		pDC->MoveTo( x, y);
		pDC->LineTo( x, y+s.cx);
		pDC->MoveTo( x-l.cx, y+l.cy);
		pDC->LineTo( x, y);
		pDC->LineTo( x+l.cx, y+l.cy);
		pDC->TextOut( x, y+s.cx, m_strName);
	}
	if( m_nType == CHM_LINE ) {
		for( int i=0; i<m_pt3DArrayProf.GetSize(); i++ ) {
			if( m_nProfType == PRF_VRT )
				xd = m_pt3DArrayProf.GetAt(i).y;
			if( m_nProfType == PRF_HRZ )
				xd = m_pt3DArrayProf.GetAt(i).x;
			yd = m_pt3DArrayProf.GetAt(i).z;
			x = (int) (offx + scx*xd);
			y = (int) (offy + scy*yd);
			pDC->MoveTo( x, y);
			pDC->LineTo( x, y+s.cx);
			pDC->MoveTo( x-l.cx, y+l.cy);
			pDC->LineTo( x, y);
			pDC->LineTo( x+l.cx, y+l.cy);
			pDC->TextOut( x, y+s.cx, m_strName);
		}
	}
	
	pDC->SelectObject(def_font);

}

void CGuideMark::Draw(CDC* pDC, double scx, double offx, double scy, double offy, double z)
{
	if( !IsVisible() ) return;

	CSize s(10, 0);
	CSize l(3, 4);
	int x, y;
	double xd;

	pDC->DPtoLP( &s );
	pDC->DPtoLP( &l );
	// pen
	CPen	pen, *pPen;
	pen.CreatePenIndirect(&m_logPen);
	pPen = pDC->SelectObject(&pen);
	// font
	CFont font;
	font.CreatePointFontIndirect(&m_logFont, pDC);
	CFont* def_font = pDC->SelectObject(&font);
	pDC->SetTextAlign(TA_BOTTOM | TA_CENTER);
	COLORREF txtColor = pDC->SetTextColor(m_logPen.lopnColor);

	if( m_nType == CHM_POINT ) {
		x = (int) (offx + scx*m_x);
		y = (int) (offy + scy*z);
		pDC->MoveTo( x, y);
		pDC->LineTo( x, y+s.cx);
		pDC->MoveTo( x-l.cx, y+l.cy);
		pDC->LineTo( x, y);
		pDC->LineTo( x+l.cx, y+l.cy);
		pDC->TextOut( x, y+s.cx, m_strName);
	}
	if( m_nType == CHM_LINE ) {
		for( int i=0; i<m_pt3DArrayProf.GetSize(); i++ ) {
			if( m_nProfType == PRF_VRT )
				xd = m_pt3DArrayProf.GetAt(i).y;
			if( m_nProfType == PRF_HRZ )
				xd = m_pt3DArrayProf.GetAt(i).x;
			x = (int) (offx + scx*xd);
			y = (int) (offy + scy*z);
			pDC->MoveTo( x, y);
			pDC->LineTo( x, y+s.cx);
			pDC->MoveTo( x-l.cx, y+l.cy);
			pDC->LineTo( x, y);
			pDC->LineTo( x+l.cx, y+l.cy);
			pDC->TextOut( x, y+s.cx, m_strName);
		}
	}

	pDC->SetTextColor(txtColor);
	pDC->SelectObject(pPen);
	pDC->SelectObject(def_font);
}

void CGuideMark::DrawMap(CDC* pDC, double scx, double offx, double scy, double offy)
{
	if( !IsVisible() ) return;

	CSize l(4, 4);
	int x, y;

	pDC->DPtoLP( &l );
	CPen pen, *pPen;
	
	pen.CreatePenIndirect( &m_logPen );
	pPen = pDC->SelectObject( &pen);

	if( m_nType == CHM_POINT ) {
		x = (int) (offx + scx*m_x);
		y = (int) (offy + scy*m_y);
		pDC->MoveTo( x-l.cx, y+l.cy);
		pDC->LineTo( x+l.cx, y-l.cx);
		pDC->MoveTo( x-l.cx, y-l.cy);
		pDC->LineTo( x+l.cx, y+l.cx);
		
		CFont font;
		font.CreatePointFontIndirect(&m_logFont, pDC);
		CFont* def_font = pDC->SelectObject(&font);
		pDC->SetTextAlign(TA_BOTTOM | TA_CENTER);

		pDC->TextOut( x, y+l.cx, m_strName);
		pDC->SelectObject(def_font);
	}

	if( m_nType == CHM_LINE && !m_pt3DArray.IsEmpty() ) {
		int x,y;
		x = (int) (offx + scx*m_pt3DArray.GetAt(0).x);
		y = (int) (offy + scy*m_pt3DArray.GetAt(0).y);
		pDC->MoveTo( x, y );
		for(int i=1; i<m_pt3DArray.GetSize(); i++) {
			x = (int) (offx + scx*m_pt3DArray.GetAt(i).x);
			y = (int) (offy + scy*m_pt3DArray.GetAt(i).y);
			pDC->LineTo( x, y);
		}
	}
	pDC->SelectObject( pPen);
}

int CGuideMark::Properties()
{ 
	CDlgCheckMark dlg;
	
	dlg.m_bVisible = m_bVisible;
	dlg.m_strText = m_strName;
	dlg.m_nCol = m_nCol;
	dlg.m_nRow = m_nRow;
	dlg.m_x = m_x;
	dlg.m_y = m_y;
	dlg.m_z = m_z;
	dlg.m_bVisible = m_bVisible;
	dlg.m_bShowText = m_bShowText;
	dlg.m_nType = m_nType;
	dlg.m_lp = m_logPen;
	if( dlg.DoModal() == IDOK ) {
		m_bVisible = dlg.m_bVisible;
		m_strName = dlg.m_strText;
		m_nCol =  dlg.m_nCol;
		m_nRow = dlg.m_nRow;
		m_x = dlg.m_x;
		m_y = dlg.m_y;
		m_z = dlg.m_z;
		m_bVisible = dlg.m_bVisible;
		m_bShowText = dlg.m_bShowText;
		m_logPen = dlg.m_lp;

		return IDOK;
	}

	return IDCANCEL;
}



int CGuideMark::FindIntersections(int profType, double xy)
{
	// xy -  coordinate of profile
	// profOrientation - SN or EW profile
	//
	// temporary fills m_pt3DArrayProf
	// finds intersections with profile line 

	if( m_nType == CHM_LINE ) {
		double x, y, z;		// intersection points

		m_nProfType = profType;
		m_pt3DArrayProf.RemoveAll();
		for( int i=0; i<m_pt3DArray.GetSize()-1; i++) {
			CPoint3D pt1 = m_pt3DArray.GetAt(i);
			CPoint3D pt2 = m_pt3DArray.GetAt(i+1);
			// vertical profile
			double a1, a2, a12;
			if( profType == PRF_VRT ) {
				a1 = fabs(xy - pt1.x);
				a2 = fabs(xy - pt2.x);
				a12 = fabs(pt1.x-pt2.x);

				if( a1 <= a12 && a2 <= a12 ) {
					// find intersection
					x = xy;	y = pt1.y; z = m_z;
					if( (pt1.x-pt2.x) != 0.0 )
						y = pt1.y + (xy-pt1.x)*(pt1.y-pt2.y)/(pt1.x-pt2.x);
					//if( (pt1.z-pt2.z) != 0.0 ) {
					//	z = pt1.z + (xy-pt1.x)*(pt1.z-pt2.z)/(pt1.x-pt2.x);
					//}
					m_pt3DArrayProf.Add( CPoint3D(x, y, z) );
				}
			}
			// horizontal profile
			if( profType == PRF_HRZ ) {
				a1 = fabs(xy - pt1.y);
				a2 = fabs(xy - pt2.y);
				a12 = fabs(pt1.y-pt2.y);

				if( a1 <= a12 && a2 <= a12 ) {
					// find intersection
					x = pt1.x;	y = xy; z = m_z;
					if( (pt1.y-pt2.y) != 0.0 )
						x = pt1.x + (xy-pt1.y)*(pt1.x-pt2.x)/(pt1.y-pt2.y);
					//if( (pt1.z-pt2.z) != 0.0 ) {
					//	z = pt1.z + (xy-pt1.x)*(pt1.z-pt2.z)/(pt1.x-pt2.x);
					//}
					m_pt3DArrayProf.Add( CPoint3D(x, y, z) );
				}
			}
		}
		return 1;
	}
	return 0;
}

const CGuideMark& CGuideMark::operator=(const CGuideMark& chm)
{
	m_nProfType = chm.m_nProfType;
	m_nType = chm.m_nType;
	m_nCol = chm.m_nCol;
	m_nRow = chm.m_nRow;
	m_x = chm.m_x;
	m_y = chm.m_y;
	m_z = chm.m_z;
	m_bShowText = chm.m_bShowText;
	m_pt3DArray.Append( chm.m_pt3DArray );
	m_pt3DArrayProf.Append( chm.m_pt3DArrayProf );
	m_bVisible = chm.m_bVisible;
	m_strID = chm.m_strID;
	m_strName = chm.m_strName;
	m_logFont = chm.m_logFont;
	m_logPen = chm.m_logPen;
	return *this;
}