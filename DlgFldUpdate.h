#pragma once


// CDlgFldUpdate dialog

class CDlgFldUpdate : public CDialog
{
	DECLARE_DYNAMIC(CDlgFldUpdate)

public:
	CDlgFldUpdate(CWnd* pParent = NULL);   // standard constructor
	virtual ~CDlgFldUpdate();

// Dialog Data
	enum { IDD = IDD_DLG_FIELD_UPDATE };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
};
