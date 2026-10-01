#pragma once


// CDlgBodyMoveProp dialog

class CDlgBodyMoveProp : public CDialog
{
	DECLARE_DYNAMIC(CDlgBodyMoveProp)

public:
	CDlgBodyMoveProp(CWnd* pParent = NULL);   // standard constructor
	virtual ~CDlgBodyMoveProp();

// Dialog Data
	enum { IDD = IDD_DLG_BOBY_MOVEPROPERTIES };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
};
