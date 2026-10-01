#pragma once
#include "afxwin.h"
#include "afxcmn.h"


// CDlgPp3dField dialog

class CDlgPp3dField : public CPropertyPage
{
	DECLARE_DYNAMIC(CDlgPp3dField)

public:
	CDlgPp3dField();
	virtual ~CDlgPp3dField();

// Dialog Data
	enum { IDD = IDD_DLG_3DVIEW_FIELD };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	BOOL		m_bFldShow;
	BOOL		m_bFldSolid;
	CComboBox	m_comboFld;
	float		m_fFldAlpha;
	CSpinButtonCtrl m_spinAlpha;
	CStringArray	m_strFld;;

	CString		m_strFldSel;
	virtual BOOL OnInitDialog();
	virtual BOOL OnApply();
	afx_msg void OnBnClickedCheckField();
	afx_msg void OnBnClickedCheckSrfSolid();
	afx_msg void OnDeltaposSpinAlphaFld(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnEnChangeEditAlphaField();
	afx_msg void OnCbnSelchangeCombo1();
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	int m_nFldPlacement;
	afx_msg void OnBnClickedSrfType();
	afx_msg void OnBnClickedRadio2();
	afx_msg void OnBnClickedRadio3();
};
