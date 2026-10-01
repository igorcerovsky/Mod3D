#pragma once

#include "DlgColorGrad.h"
#include "resource.h"
#include "arrays.h"
#include "Grid.h"

// CDlgGridProperties dialog

class CDlgGridProperties : public CDialog
{
	DECLARE_DYNAMIC(CDlgGridProperties)

public:
	CDlgGridProperties(CWnd* pParent = NULL);   // standard constructor
	virtual ~CDlgGridProperties();

// Dialog Data
	enum { IDD = IDD_GRID_PROPERTIES };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedBrowse();
	afx_msg void OnBnClickedColorTable();
	double	m_dMinX;
	double	m_dMinY;
	double	m_dMaxY;
	double	m_dCsY;
	double	m_dCsX;
	double	m_dMinZ;
	double	m_dMaxZ;
	CGradient*	m_pGrad;
	CGrid*	m_pGrid;

	BOOL m_bHst;
	double m_dMaxX;
	CString m_strName;
	CString m_strFilePath;
	afx_msg void OnBnClickedCancel();
	BOOL m_bCustomRange;
	double m_dCustomMin;
	double m_dCustomMax;
};
