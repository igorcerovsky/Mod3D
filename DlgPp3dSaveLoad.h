#pragma once
#include "afxwin.h"
#include "Mod3DDoc.h"

// CDlgPp3dSaveLoad dialog

class CDlgPp3dSaveLoad : public CPropertyPage
{
	DECLARE_DYNAMIC(CDlgPp3dSaveLoad)

public:
	CDlgPp3dSaveLoad();
	virtual ~CDlgPp3dSaveLoad();

// Dialog Data
	enum { IDD = IDD_DLG_3DVIEW_SAVE_LOAD };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedButtonSave();
	afx_msg void OnBnClickedButtonLoad();
	afx_msg void OnBnClickedButtonRemove();
	afx_msg void OnBnClickedButtonRemoveAll();

	View3dSet*	m_pView3DSet;
	VIEW3DSET	m_setings;
	virtual BOOL OnInitDialog();
	CString m_strSettings;
	CListBox m_list;
	afx_msg void OnLbnDblclkList();
};
