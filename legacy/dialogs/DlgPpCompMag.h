#pragma once


// CDlgPpCompMag dialog

class CDlgPpCompMag : public CPropertyPage
{
	DECLARE_DYNAMIC(CDlgPpCompMag)

public:
	CDlgPpCompMag();
	virtual ~CDlgPpCompMag();

// Dialog Data
	enum { IDD = IDD_DLG_COMPUTE_MAG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	double m_dTot;
	double m_dIncl;
	double m_dDecl;
	double m_dUnits;
	BOOL m_bMeanMag;
};
