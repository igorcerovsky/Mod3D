#pragma once
#include "afxwin.h"


// CDlgModelProperties dialog

class CDlgModelProperties : public CDialog
{
	DECLARE_DYNAMIC(CDlgModelProperties)

public:
	CDlgModelProperties(CWnd* pParent = NULL);   // standard constructor
	virtual ~CDlgModelProperties();

// Dialog Data
	enum { IDD = IDD_DLG_MODEL };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	double	m_fMinX, m_fMaxX;
	double	m_fMinY, m_fMaxY;
	double	m_fMinZ, m_fMaxZ;

	CString m_strDescription;
	BOOL	m_bExtendBoundaries;
	double	m_fExW;
	double	m_fExE;
	double	m_fExN;
	double	m_fExS;
	CEdit	m_editInfo;
	CString m_strInfo;

	virtual BOOL OnInitDialog();

protected:
	virtual void OnOK();

public:
	double m_xSize;
	double m_ySize;
};
