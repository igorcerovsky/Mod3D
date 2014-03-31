#pragma once
#include "afxcmn.h"


// CDlgPp3dDraw dialog

class CDlgPp3dDraw : public CPropertyPage
{
	DECLARE_DYNAMIC(CDlgPp3dDraw)

public:
	CDlgPp3dDraw();
	virtual ~CDlgPp3dDraw();

// Dialog Data
	enum { IDD = IDD_DLG_3DVIEW_DRAW };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	BOOL m_bBmpShow;
	BOOL m_bPrfShow;
	BOOL m_bPrfSolid;
	float m_fAlphaPrf;
	CSpinButtonCtrl m_spinProf;
	CSpinButtonCtrl m_spinRel;
	CSpinButtonCtrl m_spinLeadHrz;
	BOOL m_bRelShow;
	BOOL m_bRelSolid;
	BOOL m_bLeadHrzShow;
	BOOL m_bLeadHrzSolid;
	virtual BOOL OnInitDialog();
	float m_fAlphaLeadHrz;
	float m_fAlphaRel;
	afx_msg void OnBnClickedCheckDrawBitmaps();
	afx_msg void OnBnClickedCheckProfilePlanes();
	afx_msg void OnBnClickedCheckSolidRelief3();
	afx_msg void OnDeltaposSpinAlphaProf(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnBnClickedCheckRelief();
	afx_msg void OnBnClickedCheckSolidRelief();
	afx_msg void OnBnClickedCheckLeadingHrz();
	afx_msg void OnBnClickedCheckSolidLeadhrz();
	afx_msg void OnDeltaposSpinAlphaRel(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnDeltaposSpinAlphaLeadhrz(NMHDR *pNMHDR, LRESULT *pResult);
	virtual BOOL OnApply();
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
};
