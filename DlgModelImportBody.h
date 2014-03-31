#pragma once

#include "Grid.h"

// CDlgModelImportBody dialog

class CDlgModelImportBody : public CDialog
{
	DECLARE_DYNAMIC(CDlgModelImportBody)

public:
	CDlgModelImportBody(CWnd* pParent = NULL);   // standard constructor
	virtual ~CDlgModelImportBody();

// Dialog Data
	enum { IDD = IDD_DLG_MODEL_BODY_IMPORT };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	int		m_nTagUB;
	int		m_nTagLB;
	CString m_strGidFileUB;
	CString m_strGridFileLB;
	double	m_dUB;
	double	m_dLB;
	afx_msg void OnBnClickedButtonUb();
	afx_msg void OnBnClickedButtonLb();

	// grid info for checking
	double	m_x0, m_y0, m_xSize, m_ySize;
	int		m_nRows, m_nCols;
	CGrid	m_grdUp;
	CGrid	m_grdDwn;

	void OpenGrid(CGrid* pGrd, int nGrid);
protected:
	virtual void OnOK();
public:
	double m_dDumMin;
	double m_dDumMax;
	double m_dConstUB;
	double m_dConstLB;
};
