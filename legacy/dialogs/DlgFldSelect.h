#pragma once
#include "afxwin.h"


// CDlgFldSelect dialog

class CDlgFldSelect : public CDialog
{
	DECLARE_DYNAMIC(CDlgFldSelect)

public:
	CDlgFldSelect(CWnd* pParent = NULL);   // standard constructor
	virtual ~CDlgFldSelect();

// Dialog Data
	enum { IDD = IDD_DLG_FIELD_SELECT };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	CStringArray	m_strFld;		// active field
	CComboBox		m_comboFld;
	virtual BOOL OnInitDialog();
	CString m_strCombo;
};
