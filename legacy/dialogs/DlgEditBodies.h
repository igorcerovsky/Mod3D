
#pragma once
// DlgEditBodies.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CDlgEditBodies dialog

#include "Body.h"
#include "Mod3DDoc.h"

#define	WM_GOODBYE	WM_USER +5

class CDlgEditBodies : public CDialog
{
// Construction
public:
	CDlgEditBodies(CWnd* pParent = NULL);			// standard constructor
	BOOL Create();

// Dialog Data
	enum { IDD = IDD_DLG_MODEL_EDIT_BODIES };
	CButton		m_butShow;
	CListBox	m_list;

	BodyPtrArray* m_pBodyArray;
	CBody*		m_pBody;		// selected body
	CModel*		m_pModel;
	CMod3DDoc*	m_pDoc;

// Overrides
	// ClassWizard generated virtual function overrides
	public:
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support


// Implementation
protected:

	// Generated message map functions
	afx_msg void OnAddNew();
	afx_msg void OnDelete();
	afx_msg void OnProprties();
	afx_msg void OnImport();
	afx_msg void OnExport();
	afx_msg void OnSelchangeListBodies();
	afx_msg void OnPaint();
	virtual BOOL OnInitDialog();
	afx_msg void OnShowBody();
	virtual void OnCancel();
	virtual void OnOK();
	DECLARE_MESSAGE_MAP()
};

