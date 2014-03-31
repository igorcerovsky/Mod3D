// MainFrm.h : interface of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////


#pragma once

#include "Messages.h"
#include "DlgEditBodies.h"
#include "DlgPictureManager.h"


class CMainFrame : public CMDIFrameWnd
{
	friend class CMod3DDoc;
	DECLARE_DYNAMIC(CMainFrame)
public:
	CMainFrame();

// Attributes
public:

// Operations
public:
	CStatusBar*	GetStatusBar() { return &m_wndStatusBar;}
// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMainFrame)
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CMainFrame();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:  // control bar embedded members
	CStatusBar  m_wndStatusBar;
	CToolBar    m_wndToolBar;
	CToolBar	m_wndProfViewBar;
	CToolBar	m_wndMapViewBar;
	CToolBar	m_wndFldBar;
	CDlgObjectManager	m_wndObjMng;

protected:
	CPropertySheet* m_pPropSheetBody;

// Generated message map functions
protected:
	afx_msg int		OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void	OnNewWindowMap();
	afx_msg void	OnNewWindow3D();
	afx_msg void	OnViewProfileToolbar();
	afx_msg void	OnUpdateViewProfileToolbar(CCmdUI* pCmdUI);
	afx_msg void	OnViewToolbarMapView();
	afx_msg void	OnUpdateViewToolbarMapView(CCmdUI* pCmdUI);
	afx_msg void	OnViewFieldToolbar();
	afx_msg void	OnUpdateViewFieldToolbar(CCmdUI* pCmdUI);
	afx_msg void	OnUpdateIndicatorFld(CCmdUI *pCmdUI);
	afx_msg void	OnUpdateRow(CCmdUI *pCmdUI);
	afx_msg void	OnUpdateCol(CCmdUI *pCmdUI);
	afx_msg void	OnUpdateCoordinateX(CCmdUI *pCmdUI);
	afx_msg void	OnUpdateCoordinateY(CCmdUI *pCmdUI);
	afx_msg void	OnUpdateCoordinateZ(CCmdUI *pCmdUI);
	afx_msg void	OnUpdateComp(CCmdUI *pCmdUI);
	afx_msg LRESULT	OnUpdateObjMng(WPARAM wParam, LPARAM lParam);

	DECLARE_MESSAGE_MAP()
public:
	afx_msg LRESULT OnBodyProperties(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnComputationFinished(WPARAM wParam, LPARAM lParam);
	afx_msg void	OnTimer(UINT nIDEvent);
	afx_msg void	OnUpdateWindow3d(CCmdUI *pCmdUI);
	void SetPropSheetBody(CPropertySheet* pSheet);
	CPropertySheet* GetPropSheetBody()						{ return m_pPropSheetBody;}
	afx_msg void OnObjectManager();
	afx_msg void OnUpdateObjectManagerUI(CCmdUI *pCmdUI);
	afx_msg void OnWindowNewgridwindow();
	int SnapWindows(CMDIChildWnd* pChildWnd);
protected:
	virtual BOOL OnWndMsg(UINT message, WPARAM wParam, LPARAM lParam, LRESULT* pResult);
};

/////////////////////////////////////////////////////////////////////////////
