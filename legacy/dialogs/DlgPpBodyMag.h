#pragma once


// CDlgPpBodyMag dialog

class CDlgPpBodyMag : public CPropertyPage
{
	DECLARE_DYNAMIC(CDlgPpBodyMag)

public:
	CDlgPpBodyMag();
	virtual ~CDlgPpBodyMag();

// Dialog Data
	enum { IDD = IDD_DLG_BODY_MAG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	double m_dSusc;
	double m_dRemIncl;
	double m_dRemDecl;
	double m_dRemInt;
	virtual BOOL OnApply();
	virtual BOOL OnSetActive();
};
