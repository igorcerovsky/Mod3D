#pragma once
#include "afxcmn.h"


// CDlgPp3dModel dialog

class CDlgPp3dModel : public CPropertyPage
{
	DECLARE_DYNAMIC(CDlgPp3dModel)

public:
	CDlgPp3dModel();
	virtual ~CDlgPp3dModel();

// Dialog Data
	enum { IDD = IDD_DLG_3DVIEW_MODEL };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	BOOL m_bModelTransparent;
	BOOL m_bModelSolid;
	BOOL m_bModelVertFct;
	BOOL m_bModelOutFct;
	afx_msg void OnBnClickedButAllRows();
	afx_msg void OnBnClickedButAllCols();
	int m_nRowStart;
	int m_nRowEnd;
	int m_nRowMax;
	int m_nColStart;
	int m_nColEnd;
	int m_nColMax;
	CSpinButtonCtrl m_spinRowStart;
	CSpinButtonCtrl m_spinRowEnd;
	CSpinButtonCtrl m_spinColStart;
	CSpinButtonCtrl m_spinColEnd;
	virtual BOOL OnApply();
	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedCheckTransparentModel();
	afx_msg void OnBnClickedCheckSolidModel();
	afx_msg void OnBnClickedCheckVrtFacets();
	afx_msg void OnBnClickedCheckOutfct();
	afx_msg void OnDeltaposSpinRowStart(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnDeltaposSpinRowEnd(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnDeltaposSpinColStart(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnDeltaposSpinColEnd(NMHDR *pNMHDR, LRESULT *pResult);
	BOOL m_bCullFace;
	int m_nCullFacet;
	afx_msg void OnBnClickedCheckCullFacet();
	afx_msg void OnBnClickedRadioCullFace();
	afx_msg void OnBnClickedRadio2();
	float m_fDarkenInside;
	BOOL m_bDarkenInside;
	BOOL m_bBottom;
	afx_msg void OnBnClicked3dDrawBottom();
};
