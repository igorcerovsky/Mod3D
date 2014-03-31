#pragma once


// CDlgPpCompGrav dialog

class CDlgPpCompGrav : public CPropertyPage
{
	DECLARE_DYNAMIC(CDlgPpCompGrav)

public:
	CDlgPpCompGrav();
	virtual ~CDlgPpCompGrav();

// Dialog Data
	enum { IDD = IDD_DLG_COMPUTE_GRAV };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	double m_dGravSens;
	int m_nUnits;
	double m_dUnits;
	double m_dDensRef;
	double m_dDGx;
	double m_dDGy;
	double m_dDGz;
	double m_dDOx;
	double m_dDOy;
	double m_dDOz;
	double m_dGradUnits;
	virtual void OnOK();
	virtual BOOL OnInitDialog();
	BOOL m_bMeanGrv;
	BOOL m_bMeanTns;
};
