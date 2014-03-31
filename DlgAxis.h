#pragma once


// CDlgAxis dialog

class CDlgAxis : public CDialog
{
	DECLARE_DYNAMIC(CDlgAxis)

public:
	CDlgAxis(CWnd* pParent = NULL);   // standard constructor
	virtual ~CDlgAxis();

// Dialog Data
	enum { IDD = IDD_DLG_AXIS };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	float	m_fMin;
	float	m_fMax;
	float	m_fDataMin;
	float	m_fDataMax;
	float	m_fTickStart;
	float	m_fTickStep;
	BOOL	m_bAutoTL;
	BOOL	m_bTickMajor;
	BOOL	m_bLabelMajor;
	BOOL	m_bTickMinor;
	BOOL	m_bLabelMinor;
	int		m_nDigitsMinor;
	int		m_nDigitsMajor;
	int		m_nTicks;
	int		m_nType;
	LOGFONT m_lfLabelMajor;
	LOGFONT m_lfLabelMinor;

	afx_msg void OnBnClickedButtonFontMinor();
	afx_msg void OnBnClickedButtonFontMajorLbl();
protected:
	virtual void OnOK();
public:
	virtual BOOL OnInitDialog();
	BOOL m_bGLMajor;
	BOOL m_bGLMinor;
	afx_msg void OnBnClickedCheckAutomaticTl();
};
