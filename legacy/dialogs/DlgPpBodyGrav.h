#pragma once


// CDlgPpBodyGrav dialog

class CDlgPpBodyGrav : public CPropertyPage
{
	DECLARE_DYNAMIC(CDlgPpBodyGrav)

public:
	CDlgPpBodyGrav();
	virtual ~CDlgPpBodyGrav();

	void OnGravClicked(UINT nCmdID);

// Dialog Data
	enum { IDD = IDD_DLG_BODY_GRAV };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	double m_dDensity;
	double m_dGradX;
	double m_dGradY;
	double m_dGradZ;
	double m_dOrigX;
	double m_dOrigY;
	double m_dOrigZ;
	virtual BOOL OnApply();
	virtual BOOL OnSetActive();

	int m_nBodyID;
};
