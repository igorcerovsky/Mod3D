#pragma once


// CDlgBodyMove dialog

class CDlgBodyMove : public CDialog
{
	DECLARE_DYNAMIC(CDlgBodyMove)

public:
	CDlgBodyMove(CWnd* pParent = NULL);   // standard constructor
	virtual ~CDlgBodyMove();

// Dialog Data
	enum { IDD = IDD_DLG_BODY_MOVE };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	double m_dShiftTop;
	double m_dShiftBot;
	int m_nTag;
protected:
	virtual void OnOK();
public:
	BOOL m_bTop;
	BOOL m_bBot;
};
