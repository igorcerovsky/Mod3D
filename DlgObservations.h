#pragma once
#include "afxwin.h"
#include "grid.h"


// CDlgObservations dialog

class CDlgObservations : public CDialog
{
	DECLARE_DYNAMIC(CDlgObservations)

public:
	CDlgObservations(CWnd* pParent = NULL);   // standard constructor
	virtual ~CDlgObservations();

// Dialog Data
	enum { IDD = IDD_DLG_OBSERVATIONS };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	CGrid* m_pGrdRel;
	CGrid* m_pGrdGrv;
	CGrid* m_pGrdMag;
	CGrid* m_pGrdTns;

	double m_dSensHeight;
	double m_dSensHeightGrv;
	double m_dSensHeightMag;
	double m_dSensHeightTns;

	double m_dConstElev;
	double m_dConstElevGrv;
	double m_dConstElevMag;
	double m_dConstElevTns;

	afx_msg void OnBnClickedButtonBlank();
	afx_msg void OnBnClickedRadioField();
	afx_msg void OnBnClickedRadio2();
	afx_msg void OnBnClickedRadio3();
	afx_msg void OnBnClickedRadio6();
	CButton m_butProceed;
	afx_msg void OnBnClickedRadioObsType();
	afx_msg void OnBnClickedRadio5();
	virtual BOOL OnInitDialog();

	BOOL	m_bRecompute;
	CString m_strElevGrid;
	CString m_strBlank;
	CButton m_radioObs;

	CGrid*	GetGrid();
	void	UpdateMyData(BOOL bFromControls=TRUE);
	BOOL m_nFld;
	int m_nObsTag;
	int m_nObsTagGrv;
	int m_nObsTagMag;
	int m_nObsTagTns;
	afx_msg void OnEnChangeEditSensorHeight();
	afx_msg void OnEnChangeEditSensorConst();
};
