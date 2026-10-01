#pragma once


// CDlgPp3dSetings dialog

class CDlgPp3dSetings : public CPropertyPage
{
	DECLARE_DYNAMIC(CDlgPp3dSetings)

public:
	CDlgPp3dSetings();
	virtual ~CDlgPp3dSetings();

// Dialog Data
	enum { IDD = IDD_DLG_3DVIEW_SETTINGS };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	int		m_nMode;
	float	m_mX;
	float	m_mY;
	float	m_mZ;
	float	m_rX;
	float	m_rY;
	float	m_rZ;
	float	m_fRotX;
	float	m_fRotY;
	float	m_fRotZ;
	double	m_dScX;
	double	m_dScY;
	double	m_dScZ;

	double m_dProjAngle;
	virtual BOOL OnApply();
	afx_msg void OnBnClickedRadioMode();
	afx_msg void OnBnClickedRadio7();
	afx_msg void OnEnChangeEditProjAngle();
	afx_msg void OnEnChangeEditMotionY();
	afx_msg void OnEnChangeEditMotionZ();
	afx_msg void OnEnChangeEditRotationX();
	afx_msg void OnEnChangeEditRotationY();
	afx_msg void OnEnChangeEditRotationZ();
	afx_msg void OnBnClickedButSn();
	afx_msg void OnBnClickedButNs();
	afx_msg void OnBnClickedButEw();
	afx_msg void OnBnClickedButWe();
	afx_msg void OnBnClickedButMap();
	float m_fSelSensitivity;
	afx_msg void OnEnChangeSc3dX();
	afx_msg void OnEnChangeSc3dY();
	afx_msg void OnEnChangeSc3dZ();
	afx_msg void OnEnChangeEditSelSensitivity();
};
