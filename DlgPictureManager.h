#pragma once
#include "afxcmn.h"
#include "Mod3DDoc.h"


// CDlgObjectManager dialog

class CDlgObjectManager : public CDialog
{
	DECLARE_DYNAMIC(CDlgObjectManager)

public:
	CDlgObjectManager(CWnd* pParent = NULL);   // standard constructor
	virtual ~CDlgObjectManager();

// Dialog Data
	enum { IDD = IDD_DLG_OBJECT_MANAGER };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL	OnInitDialog();
	CTreeCtrl		m_tree;
	MyObPtrArray*	m_pObPtrArray;
	CMod3DDoc*		m_pDoc;

public:
	CMyObject*	FindObj(HTREEITEM hI);
	CObject*	FindBody(HTREEITEM hI);

protected:
	COLORREF	m_txtColor;
	COLORREF	m_randColor;
	CPen		m_randPen;
	CPen		m_randPenCur;
	//CMyObject*	m_pObj;
	HTREEITEM	m_hI;

public:
	afx_msg void OnNMClickTree(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnNMRclickTree(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnObjmngDelete();
	afx_msg void OnObjmngProperties();
	afx_msg void OnNMDblclkTree(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized);
	afx_msg void OnSize(UINT nType, int cx, int cy);
};
