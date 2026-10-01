#pragma once
#include "afxcmn.h"
#include "afxwin.h"
#include "colorbutton.h"


// CDlgPen dialog

class CDlgPen : public CDialog
{
	DECLARE_DYNAMIC(CDlgPen)

public:
	CDlgPen(CWnd* pParent = NULL);   // standard constructor
	virtual ~CDlgPen();

// Dialog Data
	enum { IDD = IDD_DLG_PEN };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	//afx_msg void OnBnClickedButtonColor();
	int m_nWidth;
	CSpinButtonCtrl m_spin;
	int m_nStyle;
	virtual BOOL OnInitDialog();
	CComboBox m_combStyle;

	LOGPEN		m_lp;
	COLORREF	m_color;
protected:
	virtual void OnOK();
public:
	CColorButton m_butColor;
};
