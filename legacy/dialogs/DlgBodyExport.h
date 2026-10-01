#pragma once
// DlgBodyExport.h : header file
//

#include "Body.h"

/////////////////////////////////////////////////////////////////////////////
// CDlgBodyExport dialog

class CDlgBodyExport : public CDialog
{
// Construction
public:
	CDlgBodyExport(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	enum { IDD = IDD_DLG_BODY_EXPORT };
	CString	m_strFilePathBdy;
	CString	m_strFilePathBot;
	CString	m_strFilePathTop;

	// body
	CBody *m_pBody;

// Overrides
	// ClassWizard generated virtual function overrides
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);

// Implementation
protected:
	// Generated message map functions
	afx_msg void OnButBdy();
	afx_msg void OnButTop();
	afx_msg void OnButBot();
	DECLARE_MESSAGE_MAP()
};

