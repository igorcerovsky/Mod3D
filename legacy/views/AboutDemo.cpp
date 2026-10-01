// AboutDemo.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "AboutDemo.h"


// CAboutDemo dialog

IMPLEMENT_DYNAMIC(CAboutDemo, CDialog)
CAboutDemo::CAboutDemo(CWnd* pParent /*=NULL*/)
	: CDialog(CAboutDemo::IDD, pParent)
{
}

CAboutDemo::~CAboutDemo()
{
}

void CAboutDemo::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CAboutDemo, CDialog)
END_MESSAGE_MAP()


// CAboutDemo message handlers
