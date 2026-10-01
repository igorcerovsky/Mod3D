#pragma once
// DlgCompProp.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CDlgCompProp dialog

class CDlgCompProp : public CDialog
{
// Construction
public:
	CDlgCompProp(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	enum { IDD = IDD_DLG_MODEL_COMP_PROP };
	CSpinButtonCtrl	m_sp4;
	CSpinButtonCtrl	m_sp3;
	CSpinButtonCtrl	m_sp2;
	CSpinButtonCtrl	m_sp1;
	BOOL	m_bGrv;
	BOOL	m_bMag;
	BOOL	m_bSpherComp;
	DWORD	m_nColEnd;
	DWORD	m_nColStart;
	float	m_fSensHigh;
	int		m_nFormula;
	DWORD	m_nRowEnd;
	DWORD	m_nRowStart;
	BOOL	m_bExtEdg;
	double	m_dExtEdg;
	int		m_nUnits;
	double	m_dUnits;

	DWORD m_nRow;
	DWORD m_nCol;
// Overrides
	// ClassWizard generated virtual function overrides
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	// background
	CBrush m_bkBrush;
	CBitmap m_bmpBk;

// Implementation
protected:

	// Generated message map functions
	afx_msg void OnButAllCols();
	afx_msg void OnButAllRows();
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	DECLARE_MESSAGE_MAP()
public:
	CButton m_butOK;

public:
	float	m_dTot;
	float	m_dIncl;
	float	m_dDecl;
	float	m_dRefDens;
	float	m_dDGx;
	float	m_dDGy;
	float	m_dDGz;
	float	m_dDOx;
	float	m_dDOy;
	float	m_dDOz;
	int		m_nGradElevTag;
	float	m_fFlightElev;
	float	m_fFlightHeight;
	int		m_nComputationType;
	BOOL	m_bGrvTens;
};

