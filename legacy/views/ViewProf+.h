// 3dDefView.h : interface of the CViewProfPlus class
//


#pragma once
#include "ViewProf.h"

class CViewProfPlus : public CViewProf
{
protected: // create from serialization only
	CViewProfPlus();
	DECLARE_DYNCREATE(CViewProfPlus)

// Attributes
public:

	// Operations
public:

// Overrides
	public:
	virtual void OnDraw(CDC* pDC);  // overridden to draw this view
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
protected:
	virtual BOOL OnPreparePrinting(CPrintInfo* pInfo);
	virtual void OnBeginPrinting(CDC* pDC, CPrintInfo* pInfo);
	virtual void OnEndPrinting(CDC* pDC, CPrintInfo* pInfo);

// Implementation
public:
	virtual ~CViewProfPlus();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:
	// profile type
	int		m_nPrfType;
	
// Generated message map functions
protected:
	DECLARE_MESSAGE_MAP()

public:
	int GetRow()		{return m_nRow;}
	int GetCol()		{return m_nCol;}
	int	GetProfType()	{return m_nPrfType;}

protected:
	virtual void OnPrepareDC(CDC* pDC, CPrintInfo* pInfo = NULL);
	virtual void OnInitialUpdate();

	// update functions
	void	UpdatePlgAll();

	// drawing functions
	void	DrawInit(CDC* pDC) ;
	void	Draw(CDC* pDC);
	void	DrawField(CDC *pDC);
	void	DrawGrdLines(CDC* pDC);
	void	DrawPlg(CDC* pDC);
	void	CreatePenBrush(void);
	void	InitPenFld();
	void	DrawImages(CDC* pDC);
	void	DrawRelief(CDC* pDC);
	void	DrawCheckMarks(CDC* pDC);
	void	DrawWells(CDC* pDC);

	// coordinate transform functions
	void	Scale(CDC *pDC);
	void	ScaleFieldAxis();
	void	UpdateScale(void);
	inline void WPtoLP(int& x, int& y, double xw, double yw);		// world coordinates to DC logical coordinates
	inline void WPtoLP(CPoint& pt, double xw, double yw);			// world coordinates to DC logical coordinates
	inline void LPtoWP(double& xw, double& yw, int xl, int yl);		// DC logical coordinates to world coordinates 
	inline void WPtoDP(int& x, int& y, double xw, double yw);		// world coordinates to device coordinates (points)
	inline void DPtoWP(CPoint pt, double &xw, double &yw);			// device coordinates to world coordinates
	inline void WPtoLP(CPoint &pt, int nIndex, int row, int col);
	inline void WPtoDP(CPoint& pt, double xw, double yw);
	virtual void DPtoLP(CPoint& pt);

	inline	void	FindIndex(CPoint pt);											// find profile index
	inline	double	GetXYd(int n);													// get x or y world draw coordinate for given profile
	inline	void	GetXZd(int nIndex, int row, int col, double& x, double& z);		// get x or y world draw coordinate for given profile and z coordinate for given index
	inline	void	GetRowCol(int &row, int &col, int nPrfIndexOld) ;

	// cursor over....;
	int IsCursorOverLine(CPoint &pt, int row, int col);
	int IsCursorOverScaleBar(CPoint &pt);
	int IsCursorOverDiv(CPoint pt);


protected:
	void	SetVariables();
	void	MoveProfile(int incr);
	void	MoveProfileTo(int nPrf);
	void	CopyProfile(int nIncr);

public:
	// floating body
	afx_msg void OnViewNextProfile();
	afx_msg void OnViewPreviousProfile();

	// profile
	afx_msg void OnProfileEW();
	afx_msg void OnUpdateProfileEW(CCmdUI *pCmdUI);
	afx_msg void OnProfileSN();
	afx_msg void OnUpdateProfileSN(CCmdUI *pCmdUI);

	int		CraeteGuideMarks(void);
	void	DrawLeadingHorizon(CDC* pDC);
	int		GetMargin(CDC* pDC, CSize& szMargin);
	void	GetProfRect(double& xt, double& yt, double& zt, double& xb, double& yb, double& zb);
	afx_msg void OnProfileExtrem();
	afx_msg void OnUpdateProfileExtrem(CCmdUI *pCmdUI);
	afx_msg void OnProfileSetExtremField();
	afx_msg void OnUpdateProfileSetExtremField(CCmdUI *pCmdUI);
	afx_msg void OnProfileJumptomax();
	afx_msg void OnUpdateProfileJumptomax(CCmdUI *pCmdUI);
	afx_msg void OnProfileJumptomin();
	afx_msg void OnUpdateProfileJumptomin(CCmdUI *pCmdUI);
	afx_msg void OnProfileSaveCurrentSettings();
	void	LoadProfileSettings(PRFVIEWSET profSet);
	void	DrawProfileIntersections(CDC* pDC);
};



#ifndef _DEBUG  // debug version in 3dDefView.cpp
inline CMod3DDoc* CViewProfPlus::GetDocument() const
   { return reinterpret_cast<CMod3DDoc*>(m_pDocument); }
#endif

