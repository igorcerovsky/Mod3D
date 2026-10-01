#pragma once
// DlgVerticalRange.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CDlgVerticalRange dialog

class CDlgVerticalRange : public CDialog
{
// Construction
public:
	CDlgVerticalRange(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	enum { IDD = IDD_DLG_MODEL_RANGE_Z };
	float	m_dMinZ;
	CString	m_strMaxX;
	CString	m_strMaxY;
	CString	m_strMinX;
	CString	m_strMinY;
	float	m_dMaxZ;

// Overrides
	// ClassWizard generated virtual function overrides
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	// Generated message map functions
	virtual void OnOK();
	DECLARE_MESSAGE_MAP()

public:
	float m_dSc;
};
