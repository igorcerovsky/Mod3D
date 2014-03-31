#pragma once
#include "afxwin.h"
#include "well.h"
#include "afxcmn.h"


// CDlgWell dialog

class CDlgWell : public CDialog
{
	DECLARE_DYNAMIC(CDlgWell)

public:
	CDlgWell(CWnd* pParent = NULL);   // standard constructor
	virtual ~CDlgWell();

// Dialog Data
	enum { IDD = IDD_DLG_WELL };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	CListCtrl m_listWell;
	CListCtrl m_listLith;

	CWell*	m_pWell;
	virtual BOOL OnInitDialog();
	CListCtrl m_listDrawChan;
	int m_nDrawWidth;
	int m_nDrawSide;
protected:
	virtual void OnOK();
public:
	afx_msg void OnNMDblclkListDrawChan(NMHDR *pNMHDR, LRESULT *pResult);
	double	m_dRadius;
	BOOL	m_bSolid3D;
	int		m_nDrawWidth3D;
	double	m_dRadiusLog;
	double	m_dRadiusLog3D;
	afx_msg void OnNMDblclkListLithology(NMHDR *pNMHDR, LRESULT *pResult);
	int m_nSamplingRing;
	int m_nSamplingLog;
	double m_dPieStart;
	double m_dPieEnd;
};
