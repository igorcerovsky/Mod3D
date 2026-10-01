#pragma once



// CView3dSheet

class CView3dSheet : public CPropertySheet
{
	DECLARE_DYNAMIC(CView3dSheet)

public:
	CView3dSheet(UINT nIDCaption, CWnd* pParentWnd = NULL, UINT iSelectPage = 0);
	CView3dSheet(LPCTSTR pszCaption, CWnd* pParentWnd = NULL, UINT iSelectPage = 0);
	virtual ~CView3dSheet();

protected:
	DECLARE_MESSAGE_MAP()
};


