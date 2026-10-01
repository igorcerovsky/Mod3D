// Mod3D.h : main header file for the MOD3D application
//
#pragma once

#include "resource.h"       // main symbols
#include "Messages.h"


/////////////////////////////////////////////////////////////////////////////
// CMod3DApp:
// See Mod3D.cpp for the implementation of this class
//

class CMod3DApp : public CWinApp
{
public:
	CMod3DApp();
	CMultiDocTemplate* m_pTemplateMap;
	CMultiDocTemplate* m_pTemplatePrf;
	CMultiDocTemplate* m_pTemplate3D;
	CMultiDocTemplate* m_pTemplateGrd;
// Overrides
	public:
	virtual BOOL InitInstance();

// Implementation
	//{{AFX_MSG(CMod3DApp)
	afx_msg void OnAppAbout();
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	virtual BOOL OnIdle(LONG lCount);

private:
	int ReadRegistryCode();
	int WriteRegistryCode();

public:
	afx_msg void OnFileOpenLastDocument();
	afx_msg void OnUpdateFileOpenLastDocument(CCmdUI *pCmdUI);
};


/////////////////////////////////////////////////////////////////////////////
