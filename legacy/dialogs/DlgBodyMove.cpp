// DlgBodyMove.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgBodyMove.h"


// CDlgBodyMove dialog

IMPLEMENT_DYNAMIC(CDlgBodyMove, CDialog)
CDlgBodyMove::CDlgBodyMove(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgBodyMove::IDD, pParent)
	, m_dShiftTop(0)
	, m_dShiftBot(0)
	, m_nTag(0)
	, m_bTop(FALSE)
	, m_bBot(FALSE)
{
}

CDlgBodyMove::~CDlgBodyMove()
{
}

void CDlgBodyMove::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_SHIFT_BODY_TOP, m_dShiftTop);
	DDX_Text(pDX, IDC_SHIFT_BODY_BOT, m_dShiftBot);
	DDX_Radio(pDX, IDC_MOVE_TAG, m_nTag);
	DDX_Check(pDX, IDC_MOVETOP, m_bTop);
	DDX_Check(pDX, IDC_MOVEBOT, m_bBot);
}


BEGIN_MESSAGE_MAP(CDlgBodyMove, CDialog)
END_MESSAGE_MAP()


// CDlgBodyMove message handlers

void CDlgBodyMove::OnOK()
{
	UpdateData();

	CString str, strAd;
	str = "Confirm the operartion.\n\n";
	if(m_nTag == 0) {
		str += "Shift body:";
		if(m_bTop)
			strAd.Format("\n   Top boundary shift = %f", m_dShiftTop);
		else
			strAd = "\n   Top boundary shift disabled.";
		str += strAd;
		if(m_bBot)
			strAd.Format("\n   Bottom boundary shift = %f", m_dShiftBot);
		else
			strAd = "\n   Bottom boundary shift disabled.";
		str += strAd;
	}
	if(m_nTag == 1) {
		str += "Move body boundary to:";
		if(m_bTop)
			strAd.Format("\n   Top boundary = %f", m_dShiftTop);
		else
			strAd = "\n   Top boundary disabled.";
		str += strAd;
		if(m_bBot)
			strAd.Format("\n   Bottom boundary = %f", m_dShiftBot);
		else
			strAd = "\n   Bottom boundary disabled.";
		str += strAd;
	}

	int ret;
	ret = AfxMessageBox(str, MB_YESNO | MB_ICONQUESTION);

	if( ret == IDYES )
        CDialog::OnOK();
}
