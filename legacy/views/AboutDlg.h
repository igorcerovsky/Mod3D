#pragma once

#include "VersionType.h"
// CAboutDlg dialog

class CAboutDlg : public CDialog
{
	DECLARE_DYNAMIC(CAboutDlg)

public:
	CAboutDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CAboutDlg();

// Dialog Data
#ifdef MOD3D_NAPOLI
	enum { IDD = IDD_ABOUT_NAPOLI };
#endif

#ifdef MOD3D_OGS
	enum { IDD = IDD_ABOUT_TRIESTE };
#endif

#ifdef MOD3D_WIEN
	enum { IDD = IDD_ABOUT_WIEN };
#endif

#ifdef MOD3D_DEMO
	enum { IDD = IDD_DLG_ABOUT };
#endif

#ifdef MOD3D_ROMAN
	enum { IDD = IDD_ABOUT_ROMAN };
#endif

#ifdef MOD3D_IGOR
	enum { IDD = IDD_DLG_ABOUT };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
};
