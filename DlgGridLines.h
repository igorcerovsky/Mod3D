#pragma once


// CDlgGridLines dialog

class CDlgGridLines : public CDialog
{
	DECLARE_DYNAMIC(CDlgGridLines)

public:
	CDlgGridLines(CWnd* pParent = NULL);   // standard constructor
	virtual ~CDlgGridLines();

// Dialog Data
	enum { IDD = IDD_DLG_MODEL_GRIDLINES };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	BOOL m_bHrz;
	BOOL m_bVrt;
	float m_dTop;
	float m_dBot;
	float m_dStep;
};
