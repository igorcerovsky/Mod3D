#pragma once
// DlgExportField.h : header file
//

#include "Grid.h"
/////////////////////////////////////////////////////////////////////////////
// CDlgExportField dialog

class CDlgExportField : public CDialog
{
// Construction
public:
	CDlgExportField(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	enum { IDD = IDD_DLG_MODEL_FLD_EXPORT };

	CGrid *m_pFld;

// Overrides
	// ClassWizard generated virtual function overrides
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	// Generated message map functions
	DECLARE_MESSAGE_MAP()

public:
	afx_msg void OnBnClickedButG();
	afx_msg void OnBnClickedButGx();
	afx_msg void OnBnClickedButGy();
	afx_msg void OnBnClickedButGz();
	afx_msg void OnBnClickedButGxx();
	afx_msg void OnBnClickedButGxy();
	afx_msg void OnBnClickedButGxz();
	afx_msg void OnBnClickedButGyy();
	afx_msg void OnBnClickedButGyz();
	afx_msg void OnBnClickedButGzz();
	afx_msg void OnBnClickedButM();
	afx_msg void OnBnClickedButMx();
	afx_msg void OnBnClickedButMy();
	afx_msg void OnBnClickedButMz();
	afx_msg void OnBnClickedButMxx();
	afx_msg void OnBnClickedButMxy();
	afx_msg void OnBnClickedButMxz();
	afx_msg void OnBnClickedButMyy();
	afx_msg void OnBnClickedButMyz();
	afx_msg void OnBnClickedButMzz();
};
