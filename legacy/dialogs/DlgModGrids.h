#pragma once

#include "Grid.h"

// CDlgModGrids dialog

class CDlgModGrids : public CDialog
{
	DECLARE_DYNAMIC(CDlgModGrids)

public:
	CDlgModGrids(CWnd* pParent = NULL);   // standard constructor
	virtual ~CDlgModGrids();

// Dialog Data
	enum { IDD = IDD_DLG_DOC_INIT_FIELD };

	CGrid*	m_grd;	// all fields grid
	int		m_nRows, m_nCols;
	double	m_x0, m_y0;
	double	m_xSize, m_ySize;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedButCreate();
	afx_msg void OnBnClickedButClose();
	afx_msg void OnBnClickedOk();
	int CreateGrid(int nGrd);

	BOOL m_bGx;
	BOOL m_bGy;
	BOOL m_bGz;
	BOOL m_bG;
	BOOL m_bMx;
	BOOL m_bMy;
	BOOL m_bMz;
	BOOL m_bM;

	BOOL m_bGxx;
	BOOL m_bGyy;
	BOOL m_bGzz;
	BOOL m_bGxy;
	BOOL m_bGxz;
	BOOL m_bGyz;

	int CreateAllGrids(void);
	int CloseGrids(void);
protected:
	virtual void OnCancel();
};
