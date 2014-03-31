// GridViewDemoView.h : interface of the CViewGrid class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GridCtrl_src/GridCtrl.h"
#include "Model.h"
#include "Mod3DDoc.h"

class CViewGrid : public CView
{
protected: // create from serialization only
	CViewGrid();
	DECLARE_DYNCREATE(CViewGrid)

// Attributes
public:
	CMod3DDoc* GetDocument();
	CGridCtrl* m_pGridCtrl;

// Operations
public:

// Overrides
	public:
	virtual void OnDraw(CDC* pDC);  // overridden to draw this view
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual void OnInitialUpdate();
	virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);
	protected:
	virtual BOOL OnPreparePrinting(CPrintInfo* pInfo);
	virtual void OnBeginPrinting(CDC* pDC, CPrintInfo* pInfo);
	virtual void OnEndPrinting(CDC* pDC, CPrintInfo* pInfo);
	virtual void OnPrint(CDC* pDC, CPrintInfo* pInfo);

// Implementation
public:
	virtual ~CViewGrid();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:

// Generated message map functions
protected:
	afx_msg void OnSize(UINT nType, int cx, int cy);
    //afx_msg void OnToggleReadonly();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	DECLARE_MESSAGE_MAP()
public:
	void InitProfile(void);
	void InitProfileHRZ(void);
	void InitProfileVRT(void);
	void UpdateGrid();

protected:
	virtual void OnUpdate(CView* /*pSender*/, LPARAM /*lHint*/, CObject* /*pHint*/);

public:
	int m_modRows, m_modCols;
	int m_modRow, m_modCol;
	int m_nPrfType;
	int m_nFixGrdRows;
	int m_nFixGrdCols;

};

#ifndef _DEBUG  // debug version in GridViewDemoView.cpp
inline CMod3DDoc* CViewGrid::GetDocument()
   { return (CMod3DDoc*)m_pDocument; }
#endif

/////////////////////////////////////////////////////////////////////////////

