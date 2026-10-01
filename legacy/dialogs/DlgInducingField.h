#pragma once
// DlgInducingField.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CDlgInducingField dialog

class CDlgInducingField : public CDialog
{
// Construction
public:
	CDlgInducingField(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	enum { IDD = IDD_DLG_MODEL_INDUCING_FIELD };
	double	m_dDecl;
	double	m_dIncl;
	double	m_dTot;

// Overrides
	// ClassWizard generated virtual function overrides
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:

	// Generated message map functions
	DECLARE_MESSAGE_MAP()

};

