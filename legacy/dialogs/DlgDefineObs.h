#pragma once
// DlgDefineObs.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CDlgDefineObs dialog

class CDlgDefineObs : public CDialog
{
public:
	enum ObsDef{
		obsdNormal = 1,
		obsdGrid = 2
	};
// Construction
public:
	CDlgDefineObs(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	int m_nTag;
	enum { IDD = IDD_DLG_MODEL_DEF_OBS };
	double	m_dIncrX;
	double	m_dIncrY;
	double	m_dMaxX;
	double	m_dMaxY;
	double	m_dMaxZ;
	double	m_dMinX;
	double	m_dMinY;
	double	m_dMinZ;

	double	m_dx0;
	double	m_dy0;
	double	m_csx;
	double	m_csy;
	int		m_nRows;
	int		m_nCols;
// Overrides
	// ClassWizard generated virtual function overrides
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:

	// Generated message map functions
	virtual BOOL OnInitDialog();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedGtfFromGrid();
	CString m_strGridFilePath;
};

