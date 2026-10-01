#pragma once
#include "afxwin.h"
#include "CheckPoint.h"


// CDlgEditCheckMarks dialog

class CDlgEditCheckMarks : public CDialog
{
	DECLARE_DYNAMIC(CDlgEditCheckMarks)

public:
	CDlgEditCheckMarks(CWnd* pParent = NULL);   // standard constructor
	virtual ~CDlgEditCheckMarks();

// Dialog Data
	enum { IDD = IDD_DLG_MODEL_GUIDELINE };

	MyObPtrArray*	m_pObPtrArray;
	CGuideMark*		m_pChm;
	CListBox		m_list;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedDelete();
	afx_msg void OnBnClickedProprties();
	afx_msg void OnBnClickedShow();
	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedAddNew();
	afx_msg void OnLbnSelchangeListBodies();

protected:
	COLORREF m_txtColor;
	COLORREF m_randColor;
	CPen	m_randPen;
	CPen	m_randPenCur;
};
