#if !defined(AFX_DLGSELECTBODY_H__5C54A3D5_0037_4B85_879D_BF62FEAB7E3B__INCLUDED_)
#define AFX_DLGSELECTBODY_H__5C54A3D5_0037_4B85_879D_BF62FEAB7E3B__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// DlgSelectBody.h : header file
//
#include "Body.h"

/////////////////////////////////////////////////////////////////////////////
// CDlgSelectBody dialog

class CDlgSelectBody : public CDialog
{
// Construction
public:
	CDlgSelectBody(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CDlgSelectBody)
	enum { IDD = IDD_DLG_BODY_SELECT };
	CListBox	m_list;
	//}}AFX_DATA

	BodyPtrArray	*m_pBodyArr;
	CBody			*m_pBody;		// selected body

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDlgSelectBody)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CDlgSelectBody)
	afx_msg void OnSelchangeListBodies();
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DLGSELECTBODY_H__5C54A3D5_0037_4B85_879D_BF62FEAB7E3B__INCLUDED_)
