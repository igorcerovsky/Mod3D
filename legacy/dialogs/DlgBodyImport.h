#pragma once
// DlgBodyImport.h : header file
//

#include "Grid.h"
#include "afxwin.h"
/////////////////////////////////////////////////////////////////////////////
// CDlgBodyImport dialog

class CDlgBodyImport : public CDialog
{
// Construction
public:
	CDlgBodyImport(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	enum { IDD = IDD_DLG_BODY_IMPORT };
	double	m_dLow;
	double	m_dUp;
	int		m_nLB;
	int		m_nUB;
	CString	m_strFilePathLow;
	CString	m_strFilePathUp;
	CString	m_strFilePathLB;
	CString	m_strFilePathUB;

	CGrid m_grdTop, m_grdBot;

// Overrides
	// ClassWizard generated virtual function overrides
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:

	// Generated message map functions
	afx_msg void OnButtonLb();
	afx_msg void OnButtonUb();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedOk();
};

