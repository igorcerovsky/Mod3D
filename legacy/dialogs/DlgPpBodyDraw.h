#pragma once
#include "afxwin.h"
#include "afxcmn.h"
#include "colorbutton.h"


// CDlgPpBodyDraw dialog

class CDlgPpBodyDraw : public CPropertyPage
{
	DECLARE_DYNAMIC(CDlgPpBodyDraw)

public:
	CDlgPpBodyDraw();
	virtual ~CDlgPpBodyDraw();

// Dialog Data
	enum { IDD = IDD_DLG_BODY_DRAW };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	BOOL m_bTransparent;
	BOOL m_bFill;
	BOOL m_bVisible3D;
	float m_fAlpha;

	LOGPEN		m_logPen;
	LOGPEN		m_logPenPrev;
	LOGPEN		m_logPenNext;
	LOGBRUSH	m_logBrush;
	BOOL		m_bInit;

	afx_msg void OnBnClickedButtonPen();
	afx_msg void OnBnClickedButtonPenNext();
	afx_msg void OnBnClickedButtonPenPrev();
	virtual BOOL OnInitDialog();
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	CComboBox m_comboBrushStyle;
	CComboBox m_comboBrushHatch;
	CSpinButtonCtrl m_spinAlpha;
	virtual BOOL OnApply();
	virtual BOOL OnSetActive();
	CColorButton m_butBrush;
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	virtual void OnOK();
};
