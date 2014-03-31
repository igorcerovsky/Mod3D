#pragma once


// CDlgPpBodyComp dialog

class CDlgPpBodyComp : public CPropertyPage
{
	DECLARE_DYNAMIC(CDlgPpBodyComp)

public:
	CDlgPpBodyComp();
	virtual ~CDlgPpBodyComp();

// Dialog Data
	enum { IDD = IDD_DLG_BODY_COMPUTATION };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	BOOL m_bActive;
	BOOL m_bLocked;
	virtual BOOL OnApply();
	virtual BOOL OnSetActive();
};
