// Mod3D.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "Mod3D.h"

#include "MainFrm.h"
#include "ChildFrm.h"
#include "Mod3DDoc.h"

#include "ViewMap.h"
#include "ViewProf.h"
#include "ViewGL3D.h"
#include "ViewGrid.h"

#include "DlgHtmlAbout.h"
#include "AboutDlg.h"
#include "AboutDemo.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CMod3DApp

BEGIN_MESSAGE_MAP(CMod3DApp, CWinApp)
	ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
	// Standard file based document commands
	ON_COMMAND(ID_FILE_NEW, CWinApp::OnFileNew)
	ON_COMMAND(ID_FILE_OPEN, CWinApp::OnFileOpen)
	// Standard print setup command
	ON_COMMAND(ID_FILE_OPEN_LAST_DOCUMENT, OnFileOpenLastDocument)
	ON_UPDATE_COMMAND_UI(ID_FILE_OPEN_LAST_DOCUMENT, OnUpdateFileOpenLastDocument)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMod3DApp construction

CMod3DApp::CMod3DApp()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
	EnableHtmlHelp();
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CMod3DApp object

CMod3DApp theApp;

/////////////////////////////////////////////////////////////////////////////
// CMod3DApp initialization

BOOL CMod3DApp::InitInstance()
{
	AfxOleInit();
	AfxEnableControlContainer();

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	//  of your final executable, you should remove from the following
	//  the specific initialization routines you do not need.

/*
#ifdef _AFXDLL
	Enable3dControls();			// Call this when using MFC in a shared DLL
#else
	Enable3dControlsStatic();	// Call this when linking to MFC statically
#endif
*/
	// Change the registry key under which our settings are stored.
	// TODO: You should modify this string to be something appropriate
	// such as the name of your company or organization.
	SetRegistryKey(_T("Mod3D"));
	
	int nOpenLastDoc;
	nOpenLastDoc = GetProfileInt("ReadAtStart", "OpenLastDocument", 0);

#ifndef MOD3D_FULL
	if( ReadRegistryCode() == 0 ) return 0;
	WriteRegistryCode();
#endif

	LoadStdProfileSettings();  // Load standard INI file options (including MRU)

	// Register the application's document templates.  Document templates
	//  serve as the connection between documents, frame windows and views.

	//CMultiDocTemplate* pDocTemplate;
	m_pTemplatePrf = new CMultiDocTemplate(
		IDR_PROFTYPE,
		RUNTIME_CLASS(CMod3DDoc),
		RUNTIME_CLASS(CChildFrame), // custom MDI child frame
		RUNTIME_CLASS(CViewProf) );
	AddDocTemplate(m_pTemplatePrf);

	// grid row or col profile View 
	m_pTemplateMap = new CMultiDocTemplate(
		IDR_MAPTYPE,
		RUNTIME_CLASS(CMod3DDoc),
		RUNTIME_CLASS(CChildFrame), // custom MDI child frame
		RUNTIME_CLASS(CViewMap) );
		//

	// 3D model view
	m_pTemplate3D = new CMultiDocTemplate(
		IDR_3DVIEW,
		RUNTIME_CLASS(CMod3DDoc),
		RUNTIME_CLASS(CChildFrame), // custom MDI child frame
		RUNTIME_CLASS(CViewGL3D) );
		//

	// 3D model view
	m_pTemplateGrd = new CMultiDocTemplate(
		IDR_GRIDTYPE,
		RUNTIME_CLASS(CMod3DDoc),
		RUNTIME_CLASS(CChildFrame), // custom MDI child frame
		RUNTIME_CLASS(CViewGrid) );
		//

	// create main MDI Frame window
	CMainFrame* pMainFrame = new CMainFrame;
	if (!pMainFrame->LoadFrame(IDR_MAINFRAME))
		return FALSE;
	m_pMainWnd = pMainFrame;

	// Enable drag/drop open
	m_pMainWnd->DragAcceptFiles();

	// Enable DDE Execute open
	EnableShellOpen();
	RegisterShellFileTypes(TRUE);

	// Parse command line for standard shell commands, DDE, file open
	CCommandLineInfo cmdInfo;
	ParseCommandLine(cmdInfo);

	// Dispatch commands specified on the command line
	cmdInfo.m_nShellCommand = CCommandLineInfo::FileNothing;
	if (!ProcessShellCommand(cmdInfo))
		return FALSE;

	// The main window has been initialized, so show and update it.
	pMainFrame->ShowWindow(m_nCmdShow);
	pMainFrame->UpdateWindow();

#ifndef MOD3D_FULL
	OnAppAbout();
#endif


	// open last document
	if(nOpenLastDoc ==1) {
		if( m_pRecentFileList->GetSize() != 0 )
			OpenDocumentFile( (*m_pRecentFileList)[0] );
	}

	return TRUE;
}

// App command to run the dialog
void CMod3DApp::OnAppAbout()
{
#ifndef MOD3D_FULL
	CAboutDemo dlg;
	dlg.DoModal();
#endif

#ifdef MOD3D_FULL
	CAboutDlg dlg;
	dlg.DoModal();
#endif
}

/////////////////////////////////////////////////////////////////////////////
// CMod3DApp message handlers


BOOL CMod3DApp::OnIdle(LONG lCount)
{
	return CWinApp::OnIdle(lCount);
}

int CMod3DApp::WriteRegistryCode(void)
{
	double	d=0.0;
	double	dFirstRun = 4686432132.18489655616;
	CTime	tToday = CTime::GetCurrentTime();
	CTime	tLastRun;
	CTime	tFirstRun;
	CTime	tCompile(2004,1,1,22,20,10);

	double	year, month, day, hour, min, sec;

	CTime* t;
	t = &tToday;
	year = t->GetYear()		/ 778686.156896;
	month = t->GetMonth()	/ 521826.48974987;
	day = t->GetDay()		/ 2817436871.7861;
	hour = t->GetHour()		/ 7891615.156484;
	min = t->GetMinute()	/ 5867863.786786;
	sec = t->GetSecond()	/ 15315976.1178963;

	WriteProfileBinary("Settings", "Set1", (LPBYTE)&year, sizeof(double));
	WriteProfileBinary("Settings", "Set2", (LPBYTE)&month, sizeof(double));
	WriteProfileBinary("Settings", "Set3", (LPBYTE)&day, sizeof(double));
	WriteProfileBinary("Settings", "Set4", (LPBYTE)&hour, sizeof(double));
	WriteProfileBinary("Settings", "Set5", (LPBYTE)&min, sizeof(double));
	WriteProfileBinary("Settings", "Set6", (LPBYTE)&sec, sizeof(double));

	return 0;
}

int CMod3DApp::ReadRegistryCode(void)
{
	CTime	tToday = CTime::GetCurrentTime();

	double	*year, *month, *day, *hour, *min, *sec;

	UINT n;
	BOOL b;
	b = GetProfileBinary("Settings", "Set1", (LPBYTE*)&year, &n);
	if(!b) return 1;
	GetProfileBinary("Settings", "Set2", (LPBYTE*)&month, &n);
	GetProfileBinary("Settings", "Set3", (LPBYTE*)&day, &n);
	GetProfileBinary("Settings", "Set4", (LPBYTE*)&hour, &n);
	GetProfileBinary("Settings", "Set5", (LPBYTE*)&min, &n);
	GetProfileBinary("Settings", "Set6", (LPBYTE*)&sec, &n);

	*year	*= 778686.156896;
	*month	*= 521826.48974987;
	*day	*= 2817436871.7861;
	*hour	*= 7891615.156484;
	*min	*= 5867863.786786;
	*sec	*= 15315976.1178963;

	int nYearLast = (int) (*year);
	int nMonthLast = (int) (*month);
	int nDayLast = (int) (*day);
	int nYear = tToday.GetYear();
	int nMonth = tToday.GetMonth();
	int nDay = tToday.GetDay();

	BOOL bClose=FALSE;
	if( nYearLast != 2004)		bClose = TRUE;
	if( nMonthLast > nMonth)	bClose = TRUE;
	if( nMonthLast >= nMonth && nDayLast > nDay ) {
		bClose = TRUE;
	}
	if(nMonth > 12)	bClose = TRUE;

	delete [] year;
	delete [] month;
	delete [] day;
	delete [] hour;
	delete [] min;
	delete [] sec;

	if(bClose) {
		AfxMessageBox("The free licence has expired! Contact the developer :-(", MB_OK | MB_ICONSTOP);
		AfxGetApp()->GetMainWnd()->PostMessage(WM_CLOSE);
		return 0;
	}

	return 1;
}

void CMod3DApp::OnFileOpenLastDocument()
{
	int bOpen = GetProfileInt("ReadAtStart", "OpenLastDocument", 0);
	bOpen = !bOpen;
	WriteProfileInt("ReadAtStart", "OpenLastDocument", bOpen);
}

void CMod3DApp::OnUpdateFileOpenLastDocument(CCmdUI *pCmdUI)
{
	int bOpen = GetProfileInt("ReadAtStart", "OpenLastDocument", 0);

	if(bOpen)	pCmdUI->SetCheck(1);
	else		pCmdUI->SetCheck(0);
}
