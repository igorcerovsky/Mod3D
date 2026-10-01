// DlgHtmlAbout.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgHtmlAbout.h"


// CDlgHtmlAbout dialog

IMPLEMENT_DYNCREATE(CDlgHtmlAbout, CDHtmlDialog)

CDlgHtmlAbout::CDlgHtmlAbout(CWnd* pParent /*=NULL*/)
	: CDHtmlDialog(CDlgHtmlAbout::IDD, CDlgHtmlAbout::IDH, pParent)
{
}

CDlgHtmlAbout::~CDlgHtmlAbout()
{
}

void CDlgHtmlAbout::DoDataExchange(CDataExchange* pDX)
{
	CDHtmlDialog::DoDataExchange(pDX);
}

BOOL CDlgHtmlAbout::OnInitDialog()
{
	CDHtmlDialog::OnInitDialog();
	return TRUE;  // return TRUE  unless you set the focus to a control
}

BEGIN_MESSAGE_MAP(CDlgHtmlAbout, CDHtmlDialog)
END_MESSAGE_MAP()

BEGIN_DHTML_EVENT_MAP(CDlgHtmlAbout)
	DHTML_EVENT_ONCLICK(_T("ButtonOK"), OnButtonOK)
	DHTML_EVENT_ONCLICK(_T("ButtonCancel"), OnButtonCancel)
END_DHTML_EVENT_MAP()



// CDlgHtmlAbout message handlers

HRESULT CDlgHtmlAbout::OnButtonOK(IHTMLElement* /*pElement*/)
{
	OnOK();
	return S_OK;  // return TRUE  unless you set the focus to a control
}

HRESULT CDlgHtmlAbout::OnButtonCancel(IHTMLElement* /*pElement*/)
{
	OnCancel();
	return S_OK;  // return TRUE  unless you set the focus to a control
}
