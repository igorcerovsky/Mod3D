#pragma once


// CDlgStart dialog

class CDlgStart : public CDialog
{
	DECLARE_DYNAMIC(CDlgStart)

public:
	CDlgStart(CWnd* pParent = NULL);   // standard constructor
	virtual ~CDlgStart();

// Dialog Data
	enum { IDD = IDD_DLG_START };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	int m_nTag;
};
