#pragma once

#include "colorbutton.h"
#include "DlgColorGrad.h"
#include "afxwin.h"
// CDlgData3D dialog

class CDlgData3D : public CDialog
{
	DECLARE_DYNAMIC(CDlgData3D)

public:
	CDlgData3D(CWnd* pParent = NULL);   // standard constructor
	virtual ~CDlgData3D();

// Dialog Data
	enum { IDD = IDD_DATA3D };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	double m_dMin;
	double m_dMax;
	CGradient* m_pGradCtrl;
	BOOL m_bCustomRange;
	CString m_strFilePath;
	BOOL m_bConnectPts;
	virtual BOOL OnInitDialog();
	LOGPEN m_lp;
	afx_msg void OnBnClickedD3dColorgrad();
	afx_msg void OnBnClickedColorbut();
	CString m_strName;
	BOOL m_bColorDistribution;
	double m_dMinData;
	double m_dMaxData;
	double m_dRadius;
	BOOL m_bPoints;
	int m_nSlices;
	int m_nStacks;
	afx_msg void OnBnClickedCheck1();
};
