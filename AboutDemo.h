#pragma once


// CAboutDemo dialog

class CAboutDemo : public CDialog
{
	DECLARE_DYNAMIC(CAboutDemo)

public:
	CAboutDemo(CWnd* pParent = NULL);   // standard constructor
	virtual ~CAboutDemo();

// Dialog Data
	enum { IDD = IDD_ABOUT_DEMO };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
};
