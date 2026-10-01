#pragma once


// CDlgModelActiveGrd dialog

class CDlgModelActiveGrd : public CDialog
{
	DECLARE_DYNAMIC(CDlgModelActiveGrd)

public:
	CDlgModelActiveGrd(CWnd* pParent = NULL);   // standard constructor
	virtual ~CDlgModelActiveGrd();

// Dialog Data
	enum { IDD = IDD_DLG_MODEL_GRD_ACTIVE };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedFldG();
	afx_msg void OnBnClickedFldM();

	// modeled field
	int m_bGx;
	int m_bGy;
	int m_bGz;
	int m_bG;
	int m_bGxx;
	int m_bGyy;
	int m_bGzz;
	int m_bGxy;
	int m_bGxz;
	int m_bGyz;

	int m_bMx;
	int m_bMy;
	int m_bMz;
	int m_bM;
	int m_bMxx;
	int m_bMyy;
	int m_bMzz;
	int m_bMxy;
	int m_bMxz;
	int m_bMyz;

	// measured field
	int m_bmGx;
	int m_bmGy;
	int m_bmGz;
	int m_bmG;
	int m_bmGxx;
	int m_bmGyy;
	int m_bmGzz;
	int m_bmGxy;
	int m_bmGxz;
	int m_bmGyz;

	int m_bmMx;
	int m_bmMy;
	int m_bmMz;
	int m_bmM;
	int m_bmMxx;
	int m_bmMyy;
	int m_bmMzz;
	int m_bmMxy;
	int m_bmMxz;
	int m_bmMyz;

	// difference field
	int m_bdGx;
	int m_bdGy;
	int m_bdGz;
	int m_bdG;
	int m_bdGxx;
	int m_bdGyy;
	int m_bdGzz;
	int m_bdGxy;
	int m_bdGxz;
	int m_bdGyz;

	int m_bdMx;
	int m_bdMy;
	int m_bdMz;
	int m_bdM;
	int m_bdMxx;
	int m_bdMyy;
	int m_bdMzz;
	int m_bdMxy;
	int m_bdMxz;
	int m_bdMyz;

	virtual BOOL OnInitDialog();
protected:
	virtual void OnOK();
};
