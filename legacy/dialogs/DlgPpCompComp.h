#pragma once
#include "afxcmn.h"


// CDlgPpCompComp dialog

class CDlgPpCompComp : public CPropertyPage
{
	DECLARE_DYNAMIC(CDlgPpCompComp)

public:
	CDlgPpCompComp();
	virtual ~CDlgPpCompComp();

// Dialog Data
	enum { IDD = IDD_DLG_COMPUTE_COMP };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	BOOL m_bSpherComp;
	int m_nComputationType;
};
