#pragma once
#include "afxcmn.h"


// CDlgViewMapProperties dialog

class CDlgViewMapProperties : public CDialog
{
	DECLARE_DYNAMIC(CDlgViewMapProperties)

public:
	CDlgViewMapProperties(CWnd* pParent = NULL);   // standard constructor
	virtual ~CDlgViewMapProperties();
	virtual BOOL	OnInitDialog();

// Dialog Data
	enum { IDD = IDD_DLG_VIEW_MAP };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnOK();

	DECLARE_MESSAGE_MAP()
public:
	CSliderCtrl	m_slideQual;

	int		m_nImgQual;
	BOOL	m_bObsPoints;
	BOOL	m_bShowObjects;
	BOOL	m_bShowProfiles;
	BOOL	m_bBodies;
	BOOL	m_bScaleEq;
	BOOL	m_bObsImg;
	BOOL	m_bObsCnt;
	BOOL	m_bGrdHrz;
	BOOL	m_bGrdVrt;
	float m_fAlphaImg;
	float m_fAlphaRel;
};
