#pragma once
#include "afxwin.h"

// CDlgFit1D dialog

class CDlgFit1D : public CDialog
{
	DECLARE_DYNAMIC(CDlgFit1D)

public:
	CDlgFit1D(CWnd* pParent = NULL);   // standard constructor
	virtual ~CDlgFit1D();

// Dialog Data
	enum { IDD = IDD_DLG_FIT_1D };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()
public:
	double			m_dEpsBrak;
	double			m_dFitTol;
	CComboBox		m_combo;
	CString			m_strFld;
	CStringArray	m_strAr;
	BOOL m_bFitLog;
	int m_nFitType;
	int m_nMethod;
	int m_nMaxIter;
	BOOL m_bAutoBracket;
	double m_dAutoBraket;
	CString m_strLogFile;
	afx_msg void OnBnClickedButLogBrowse();
};
