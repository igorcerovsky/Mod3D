#pragma once
#include "afxwin.h"


// CDlgPpBodyDescr dialog

class CDlgPpBodyDescr : public CPropertyPage
{
	DECLARE_DYNAMIC(CDlgPpBodyDescr)

public:
	CDlgPpBodyDescr();
	virtual ~CDlgPpBodyDescr();

// Dialog Data
	enum { IDD = IDD_DLG_BODY_DESCRIPTION };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	CString m_strName;
	int m_nID;
	CString m_strDescription;
	CEdit m_editDescr;
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	virtual BOOL OnApply();
	virtual BOOL OnSetActive();
};
