// ChildFrm.cpp : implementation of the CChildFrame class
//

#include "stdafx.h"
#include "Mod3D.h"

#include "ChildFrm.h"
#include "MainFrm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CChildFrame

IMPLEMENT_DYNCREATE(CChildFrame, CMDIChildWnd)

BEGIN_MESSAGE_MAP(CChildFrame, CMDIChildWnd)
	//{{AFX_MSG_MAP(CChildFrame)
	ON_WM_CREATE()
	ON_WM_PAINT()
	//}}AFX_MSG_MAP
//	ON_WM_DRAWITEM()
ON_WM_SIZING()
ON_WM_MOVING()
ON_WM_WINDOWPOSCHANGING()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CChildFrame construction/destruction

CChildFrame::CChildFrame()
{
	// TODO: add member initialization code here
}

CChildFrame::~CChildFrame()
{
}

BOOL CChildFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: Modify the Window class or styles here by modifying the CREATESTRUCT cs
	cs.style&=~(LONG)FWS_ADDTOTITLE;
 	if( !CMDIChildWnd::PreCreateWindow(cs) )
		return FALSE;

	return TRUE;
}



/////////////////////////////////////////////////////////////////////////////
// CChildFrame diagnostics

#ifdef _DEBUG
void CChildFrame::AssertValid() const
{
	CMDIChildWnd::AssertValid();
}

void CChildFrame::Dump(CDumpContext& dc) const
{
	CMDIChildWnd::Dump(dc);
}

#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CChildFrame message handlers

int CChildFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CMDIChildWnd::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	return 0;
}

