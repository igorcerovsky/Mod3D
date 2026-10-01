#pragma once
#include "Mod3D.h"
#include "afxwin.h"
#include "Grid.h"


// CDlgObjGrd dialog

class CDlgObjGrd : public CDialog
{
	DECLARE_DYNAMIC(CDlgObjGrd)

public:
	CDlgObjGrd(CWnd* pParent = NULL);   // standard constructor
	virtual ~CDlgObjGrd();

// Dialog Data
	enum { IDD = IDD_OBJ_GRD };

	CGrid*	m_pGrd;
	LOGPEN m_lp;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	CButton m_butBrowse;
	afx_msg void OnBnClickedBrowse();
	int m_nType;
	CString m_strName;
	CString m_strID;
	afx_msg void OnBnClickedPen();
	CString m_strFileName;
	afx_msg void OnBnClickedGrid();
};
