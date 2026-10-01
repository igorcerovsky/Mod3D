#pragma once
#include "Mod3D.h"
#include <atlimage.h>
#include "bmpreferenced.h"

// CDlgImportPicture dialog

class CDlgImportPicture : public CDialog
{
	DECLARE_DYNAMIC(CDlgImportPicture)

public:
	CDlgImportPicture(CWnd* pParent = NULL);   // standard constructor
	virtual ~CDlgImportPicture();

// Dialog Data
	enum { IDD = IDD_DLG_MODEL_IMPORT_PICTURE };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedButtonBrowse();
	afx_msg void OnBnClickedButtonPen();
protected:
	virtual void OnOK();
public:
	float	m_xs, m_ys, m_zs;
	float	m_xe, m_ye, m_ze;
	CString m_strName;
	// picture type
	int		m_nPicType;
	int		m_nProfOrient;
	UINT	m_nProfNum;
	BYTE	m_nOpacity;
	BOOL	m_bRow;
	BOOL	m_bCol;
	CString m_strFilePath;
	CImgReferenced* m_pImg;
	BOOL	m_bVisible;
	LOGPEN	m_lpLine;
};
