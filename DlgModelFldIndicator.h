#pragma once


// CDlgModelFldIndicator dialog

class CDlgModelFldIndicator : public CDialog
{
	DECLARE_DYNAMIC(CDlgModelFldIndicator)

public:
	CDlgModelFldIndicator(CWnd* pParent = NULL);   // standard constructor
	virtual ~CDlgModelFldIndicator();

// Dialog Data
	enum { IDD = IDD_DLG_MODEL_FLD_INDICATOR };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	int m_nFld;
};
