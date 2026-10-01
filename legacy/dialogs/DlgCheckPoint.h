#pragma once


// CDlgCheckPoint dialog

class CDlgCheckPoint : public CDialog
{
	DECLARE_DYNAMIC(CDlgCheckPoint)

public:
	CDlgCheckPoint(CWnd* pParent = NULL);   // standard constructor
	virtual ~CDlgCheckPoint();

// Dialog Data
	enum { IDD = IDD_DLG_CHECKMARK };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	CString m_strDescription;
	UINT	m_nRow;
	UINT	m_nCol;
	double	m_x, m_z, m_y;
	BOOL m_bVisible;
};
