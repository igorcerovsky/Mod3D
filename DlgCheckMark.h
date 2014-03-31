#pragma once

#include "Mod3D.h"


// CDlgCheckMark dialog

class CDlgCheckMark : public CDialog
{
	DECLARE_DYNAMIC(CDlgCheckMark)

public:
	CDlgCheckMark(CWnd* pParent = NULL);   // standard constructor
	virtual ~CDlgCheckMark();

// Dialog Data
	enum { IDD = IDD_DLG_MODEL_GUIDELINE };

	CString m_strText;
	UINT	m_nRow;
	UINT	m_nCol;
	double	m_x, m_z, m_y;
	BOOL	m_bVisible;
	BOOL	m_bShowText;
	int		m_nType;

	// 
	LOGPEN	m_lp;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedButtonPen();
};
