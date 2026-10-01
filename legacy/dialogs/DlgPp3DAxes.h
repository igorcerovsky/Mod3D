#pragma once


// CDlgPp3DAxes dialog

class CDlgPp3DAxes : public CPropertyPage
{
	DECLARE_DYNAMIC(CDlgPp3DAxes)

public:
	CDlgPp3DAxes();
	virtual ~CDlgPp3DAxes();

// Dialog Data
	enum { IDD = IDD_DLG_3DVIEW_AXES };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	int m_nAxLabels;
	BOOL m_bGrdLineUpX;
	BOOL m_bGrdLineUpY;
	BOOL m_bGrdLineDwnX;
	BOOL m_bGrdLineDwnY;
	BOOL m_bGrdLineVrt;
	virtual BOOL OnApply();
	afx_msg void OnBnClickedCheckGrdUpX();
	afx_msg void OnBnClickedCheckGrdUpY();
	afx_msg void OnBnClickedCheckGrdDwnX();
	afx_msg void OnBnClickedCheckGrdDwnY();
	afx_msg void OnBnClickedCheckGrdLineVrt();
	afx_msg void OnBnClickedRadioAxisLabels();
	afx_msg void OnBnClickedRadio5();
	afx_msg void OnBnClickedRadio6();
};
