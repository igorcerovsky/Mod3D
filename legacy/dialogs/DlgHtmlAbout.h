#pragma once


// CDlgHtmlAbout dialog

class CDlgHtmlAbout : public CDHtmlDialog
{
	DECLARE_DYNCREATE(CDlgHtmlAbout)

public:
	CDlgHtmlAbout(CWnd* pParent = NULL);   // standard constructor
	virtual ~CDlgHtmlAbout();
// Overrides
	HRESULT OnButtonOK(IHTMLElement *pElement);
	HRESULT OnButtonCancel(IHTMLElement *pElement);

// Dialog Data
	enum { IDD = IDD_DLGHTML_ABOUT, IDH = IDR_HTML_DLGHTMLABOUT };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()
	DECLARE_DHTML_EVENT_MAP()
};
