// ScaleBar.h: interface for the CScaleBar class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "stdafx.h"

typedef enum ScaleBarClk {
	SB_NON = 0,
	SB_MIN,
	SB_MAX,
	SB_MIDDLE
};

typedef enum MoveTag {
	AMT_PLUS,
	AMT_MINUS,
};

class CScaleAxis : public CObject  
{
public:
	CScaleAxis();
	CScaleAxis(int nType);
	virtual ~CScaleAxis();

public:
	void	Zoom(CPoint lPt, BOOL bIn=TRUE);
	void	Zoom(double nin, double max);
	void	SetType(int nType)		{m_nType = nType;}
	void	GetDataExtent(double &min, double &max);
	CRect	GetBoundRect() {return CRect(m_nL, m_nT, m_nR, m_nB);}
	void	SetMaxDataExtent(double min, double max);
	void	GetMaxDataExtent(double& min, double& max) { min = m_dMinData; max = m_dMaxData; }
	void	Move(CPoint lPt);
	void	Scale();
	int		IsClicked(CPoint lPt);
	void	SetDataExtent(double min, double max);
	void	SetDrawRect(int left, int up, int right, int bottom);
	void	SetDrawRect(CRect rc);
	void	Draw(CDC *pDC);
	void	GetScOff(double& sc, double& off);
	void	GetDataMinMax(double& min, double& max);
	int		ZoomToMax(void);
	double	GetDataMin();
	double	GetDataMax();
	int		GetLeft()			{return m_nL;}
	void	SetWidth(int w)		{m_nWidth = w; }
	void	SetCustomWidth(BOOL b=TRUE);
	BOOL	GetAuto()			{return m_bAutoTL;}

public:
	int 	m_nType;		// csale bar type horizontal or vertical
	BOOL	m_bAutoTL;		// automatic tick & labels
	double	m_dMinData;		// minimum data extent
	double	m_dMaxData;		
	double	m_dMin;			// current minimum
	double	m_dMax;			// current maximum
	int		m_nMinExt;		// in logical coordinates
	int		m_nMaxExt;
	int		m_nMin;
	int		m_nMax;
	int		m_ndl;			// difference for moving the position
	int		m_ndr;
	double  m_sc;			// scale
	double	m_off;			// offset
	int		m_nT, m_nL, m_nB, m_nR;		//in logical coordinates top, left,bottom, right
	int		m_nClk;
	double	m_dZoomIn;		//	zoom ratio
	double	m_dZoomOut;		//

	// grid lines
	BOOL	m_bGLMajor;
	BOOL	m_bGLMinor;

	// drawing
	CBrush	m_brushH1, m_brushH2;
	CPen	m_penH1, m_penH2;
	int		m_nWidth;		// custom axis width if(m_bWidth)
	BOOL	m_bWidth;

///////////////////////////////////////////////////////////////////////////
// axis
public:
	CSize	m_ar;			// arrow
	CSize	m_tickM;		// major ticks
	CSize	m_tickN;		// minor ticks
	LOGFONT m_lfTickMajor;	// font
	LOGFONT m_lfTickMinor;
	int		m_nOrientLabel;
	BOOL	m_bTickMajor;
	BOOL	m_bTickMinor;
	BOOL	m_bLabelMajor;
	BOOL	m_bLabelMinor;
	double	m_dTickStart;
	double	m_dTickStep;
	int		m_nDigitsMajor;
	int		m_nDigitsMinor;
	int		m_nTicks;		// number of ticks
public:
	void	FindAxisMinMaxStep(DWORD n=-1);
	void	DrawAxisX(CDC* pDC);
	void	DrawAxisXl(CDC* pDC);
	void	DrawAxisY(CDC* pDC);
	void	DrawAxisYl(CDC* pDC);
	int		Properties();
	void	Move(int nTag);
	void	DrawGridLines(CDC* pDC, CRect lpRect);
	int		GetWidth(CDC* pDC);
	// scales the axis
	void	ZoomAxis(double dMid=1.0e+100, double dFactor=1.0);
};
