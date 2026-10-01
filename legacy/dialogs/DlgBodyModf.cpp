// DlgBodyModf.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgBodyModf.h"
#include "Model.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CDlgBodyModf dialog


CDlgBodyModf::CDlgBodyModf(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgBodyModf::IDD, pParent)
	, m_bMoveTop(TRUE)
	, m_bMoveBot(TRUE)
	, m_bFixRel(TRUE)
	, m_bFixBot(FALSE)
	, m_bCurrentProfile(FALSE)
	, m_bConstTop(FALSE)
	, m_bConstBot(FALSE)
{
	m_dBodyCreationFactor = 1.0;
	m_bConstTop = FALSE;
	m_bConstBot = FALSE;
	m_nJoin = -1;
	m_dBot = 0.0;
	m_dTop = 0.0;
}


void CDlgBodyModf::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT_CONST_TOP, m_editTop);
	DDX_Control(pDX, IDC_EDIT_CONST_BOT, m_editBot);
	DDX_Control(pDX, IDC_CHECK_CONST_TOP, m_butConstTop);
	DDX_Control(pDX, IDC_CHECK_CONST_BOT, m_butConstBot);
	DDX_Text(pDX, IDC_EDIT_CREATION_FACTOR, m_dBodyCreationFactor);
	DDX_Radio(pDX, IDC_RADIO_JOIN, m_nJoin);
	DDX_Text(pDX, IDC_EDIT_CONST_BOT, m_dBot);
	DDX_Text(pDX, IDC_EDIT_CONST_TOP, m_dTop);
	DDX_Check(pDX, IDC_CHECK_MOVE_TOP, m_bMoveTop);
	DDX_Check(pDX, IDC_CHECK_MOVE_BOT, m_bMoveBot);
	DDX_Check(pDX, IDC_CHECK_FIX_RELIEF, m_bFixRel);
	DDX_Check(pDX, IDC_CHECK_FIX_BOTTOM, m_bFixBot);
	DDX_Check(pDX, IDC_CHECK_MOVE_PROFILE, m_bCurrentProfile);
	DDX_Check(pDX, IDC_CHECK_CONST_TOP, m_bConstTop);
	DDX_Check(pDX, IDC_CHECK_CONST_BOT, m_bConstBot);
}


BEGIN_MESSAGE_MAP(CDlgBodyModf, CDialog)
	ON_BN_CLICKED(IDC_RADIO_JOIN, OnRadioJoin)
	ON_BN_CLICKED(IDC_RADIO2, OnRadio2)
	ON_BN_CLICKED(IDC_RADIO3, OnRadio3)
	ON_BN_CLICKED(IDC_RADIO4, OnRadio4)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDlgBodyModf message handlers


void CDlgBodyModf::OnRadioJoin() 
{
	m_butConstTop.EnableWindow(TRUE);	
	m_butConstBot.EnableWindow(TRUE);
	m_editBot.EnableWindow();
	m_editTop.EnableWindow();
}

void CDlgBodyModf::OnRadio2() 
{
	m_butConstTop.EnableWindow(FALSE);	
	m_butConstBot.EnableWindow(TRUE);
	m_editTop.EnableWindow(FALSE);
	m_editBot.EnableWindow();
	m_butConstTop.SetCheck(0);
}

void CDlgBodyModf::OnRadio3() 
{
	m_butConstTop.EnableWindow(TRUE);	
	m_butConstBot.EnableWindow(FALSE);	
	m_editTop.EnableWindow();
	m_editBot.EnableWindow(FALSE);
	m_butConstBot.SetCheck(0);
}

void CDlgBodyModf::OnRadio4() 
{
	m_butConstTop.EnableWindow(FALSE);	
	m_butConstBot.EnableWindow(FALSE);		
	m_editTop.EnableWindow(FALSE);
	m_editBot.EnableWindow(FALSE);
	m_butConstTop.SetCheck(0);
	m_butConstBot.SetCheck(0);
}

void CDlgBodyModf::OnOK() 
{
	UpdateData();
	//AfxMessageBox("CDlgBodyModf::OnOK()");
	switch(m_nJoin) {
	case 0:
		m_nFlag = BCR_NONE;
		break;
	case 1:
		m_nFlag = BCR_JOINTOP;
		break;
	case 2:
		m_nFlag = BCR_JOINBOT;
		break;
	case 3:
		m_nFlag = BCR_JOINTOPBOT;
		break;
	}
	CDialog::OnOK();
}

