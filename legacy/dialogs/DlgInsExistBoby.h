#pragma once
// DlgInsExistBoby.h : header file
//
#include "Body.h"
/////////////////////////////////////////////////////////////////////////////
// CDlgInsExistBoby dialog

class CDlgInsExistBoby : public CDialog
{
// Construction
public:
	CDlgInsExistBoby(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	enum { IDD = IDD_DLG_MODEL_INSERT_EXISTING_BODY };
	CListBox	m_list;
	double	m_dWidth;

	BodyList *m_pBodyLst;
	CBody	*m_pBody;

// Overrides
	// ClassWizard generated virtual function overrides
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg void OnSelchangeListBodies();

	DECLARE_MESSAGE_MAP()
};
