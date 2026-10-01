#pragma once

#include "ColorButton.h"
#include "ClrGrad\gradientctrl.h"
#include "ColorButton.h"
#include "resource.h"
#include "afxwin.h"
#include "arrays.h"

// CDlgColorGrad dialog

class CDlgColorGrad : public CDialog
{
	DECLARE_DYNAMIC(CDlgColorGrad)

public:
	CDlgColorGrad(CWnd* pParent = NULL);   // standard constructor
	virtual ~CDlgColorGrad();

// Dialog Data
	enum { IDD = IDD_COLOR_GRAD };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()


// Dialog Data
	CComboBox	m_wndMethodCombo;
	//CWnd		m_wndHst;
	CGradientCtrl m_wndGradientCtrl;
	CColorButton m_NewPegColour;
	CColorButton m_SelPegColour;
	CColorButton m_BackgroundColour;
	BOOL	m_UseBackground;
	float	m_SelPegPosition;
	BOOL	m_EnableQuantization;
	int		m_QuantizationEntries;
	float	m_NewPegPosition;
	BOOL	m_bShowToolTips;


// Implementation
protected:
	void SetControls();

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	//afx_msg HCURSOR OnQueryDragIcon();
	afx_msg long OnSelEndOK(UINT /*lParam*/, LONG /*wParam*/);
	afx_msg void OnAddPeg();
	afx_msg void OnDelPeg();
	afx_msg void OnOpen();
	afx_msg void OnSaveas();
	afx_msg void OnChangeSelpegPosition();
	//afx_msg void OnAbout();
	afx_msg void OnUseBackground();
	afx_msg void OnSelchangeMethodCombo();
	//afx_msg void OnEnableQuantization();
	//afx_msg void OnChangeQuantizationEntries();
	afx_msg void OnShowtooltips();
	//afx_msg void OnHorizontal();
	//afx_msg void OnVertical();
	//afx_msg void OnRightTop();
	//afx_msg void OnLeftBottom();

	afx_msg void OnNotifyChangeSelPeg(NMHDR * pNotifyStruct, LRESULT *result);
	afx_msg void OnNotifyPegMove(NMHDR * pNotifyStruct, LRESULT *result);
	afx_msg void OnNotifyPegMoved(NMHDR * pNotifyStruct, LRESULT *result);
	afx_msg void OnNotifyPegRemoved(NMHDR * pNotifyStruct, LRESULT *result);
	afx_msg void OnNotifyDoubleClickCreate(NMHDR * pNotifyStruct, LRESULT *result);
	afx_msg void OnNotifyEditPeg(NMHDR * pNotifyStruct, LRESULT *result);


	// Mod3D grid data
public:
	CGradient*	m_pGrad;
	double m_dMin, m_dMax;
	DoubleArray m_hst;

protected:
	virtual void OnOK();
public:
	afx_msg void OnBnClickedSave();
	afx_msg void OnBnClickedLoad();
	afx_msg void OnEnChangeValue();
	double m_dVal;
	double PosToVal(double pos);
	float ValToPos(double dVap);
protected:
	virtual void OnCancel();
};
