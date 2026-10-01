#pragma once
// DlgBodyModf.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CDlgBodyModf dialog

class CDlgBodyModf : public CDialog
{
// Construction
public:
	CDlgBodyModf(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	enum { IDD = IDD_DLG_BODY_CREATION };
	CEdit	m_editTop;
	CEdit	m_editBot;
	CButton	m_checkNone;
	CButton	m_checkTop;
	CButton	m_checkBot;
	CButton	m_butConstTop;
	CButton	m_butConstBot;
	double	m_dBodyCreationFactor;
	BOOL	m_bNon;
	int		m_nJoin;
	double	m_dBot;
	double	m_dTop;
	int		m_nFlag;

// Overrides
	// ClassWizard generated virtual function overrides
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:

	// Generated message map functions
	virtual void OnOK();
	afx_msg void OnRadioJoin();
	afx_msg void OnRadio2();
	afx_msg void OnRadio3();
	afx_msg void OnRadio4();
	DECLARE_MESSAGE_MAP()

public:
	BOOL m_bMoveTop;
	BOOL m_bMoveBot;
	BOOL m_bFixRel;
	BOOL m_bFixBot;
	BOOL m_bCurrentProfile;
	BOOL m_bConstTop;
	BOOL m_bConstBot;
};
