#pragma once

#include "afxwin.h"
#include "Mod3DDoc.h"
#include "ViewProf.h"

// CDlgProfileSettings dialog

class CDlgProfileSettings : public CDialog
{
	DECLARE_DYNAMIC(CDlgProfileSettings)

public:
	CDlgProfileSettings(CWnd* pParent = NULL);   // standard constructor
	virtual ~CDlgProfileSettings();

// Dialog Data
	enum { IDD = IDD_DLG_PROFILE_SETTINGS };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedButtonSave();
	afx_msg void OnBnClickedButtonLoad();
	afx_msg void OnBnClickedButtonRemove();
	afx_msg void OnBnClickedButtonRemoveAll();
	CListBox	m_list;
	CString		m_strName;
	PRFVIEWSET	m_prfSet;
	PrfSetArray* m_pPrfSetArray;
	CViewProf*	m_pView;

	virtual BOOL OnInitDialog();
	afx_msg void OnLbnDblclkList();
	afx_msg void OnBnClickedButtonApply();
};
