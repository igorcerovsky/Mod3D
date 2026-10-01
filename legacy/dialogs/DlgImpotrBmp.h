#pragma once


// CDlgImpotrBmp dialog

class CDlgImpotrBmp : public CDialog
{
	DECLARE_DYNAMIC(CDlgImpotrBmp)

public:
	CDlgImpotrBmp(CWnd* pParent = NULL);   // standard constructor
	virtual ~CDlgImpotrBmp();

// Dialog Data
	enum { IDD = IDD_DIALOG_IMPORT_BITMAP };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
};
