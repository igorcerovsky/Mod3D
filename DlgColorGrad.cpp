// DlgColorGrad.cpp : implementation file
//

#include "stdafx.h"
//#include "Inv3D.h"
#include "DlgColorGrad.h"


//// CDlgColorGrad dialog
//
IMPLEMENT_DYNAMIC(CDlgColorGrad, CDialog)

CDlgColorGrad::~CDlgColorGrad()
{
}

CDlgColorGrad::CDlgColorGrad(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgColorGrad::IDD, pParent)
	, m_dVal(0)
{
	m_UseBackground = FALSE;
	m_SelPegPosition = 0.0f;
	m_bShowToolTips = TRUE;
	m_pGrad = NULL;
}

void CDlgColorGrad::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_METHOD_COMBO, m_wndMethodCombo);
	DDX_Control(pDX, IDC_GRADIENT, m_wndGradientCtrl);
	DDX_Control(pDX, IDC_NEWPEG_COLOUR, m_NewPegColour);
	DDX_Control(pDX, IDC_SELPEG_COLOUR, m_SelPegColour);
	//DDX_Control(pDX, IDC_BACKGROUND_COLOUR, m_BackgroundColour);
	DDX_Text(pDX, IDC_SELPEG_POSITION, m_SelPegPosition);
	DDV_MinMaxFloat(pDX, m_SelPegPosition, 0.f, 1.f);
	//DDX_Check(pDX, IDC_ENABLE_QUANTIZATION, m_EnableQuantization);
	//DDX_Text(pDX, IDC_QUANTIZATION_ENTRIES, m_QuantizationEntries);
	DDV_MinMaxInt(pDX, m_QuantizationEntries, 1, 1000);
	DDX_Text(pDX, IDC_NEWPEG_POSITION, m_NewPegPosition);
	//DDV_MinMaxFloat(pDX, m_NewPegPosition, 0.f, 1.f);
	DDX_Check(pDX, IDC_SHOWTOOLTIPS, m_bShowToolTips);
	DDX_Text(pDX, IDC_VALUE, m_dVal);
}

BEGIN_MESSAGE_MAP(CDlgColorGrad, CDialog)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_MESSAGE(CPN_SELENDOK, OnSelEndOK)
	ON_BN_CLICKED(IDC_ADD_PEG, OnAddPeg)
	ON_BN_CLICKED(IDC_DEL_PEG, OnDelPeg)

	//ON_COMMAND(ID_OPEN, OnOpen)
	//ON_COMMAND(ID_SAVEAS, OnSaveas)
	ON_EN_CHANGE(IDC_SELPEG_POSITION, OnChangeSelpegPosition)
	//ON_COMMAND(ID_ABOUT, OnAbout)
	//ON_BN_CLICKED(IDC_USE_BACKGROUND, OnUseBackground)
	ON_CBN_SELCHANGE(IDC_METHOD_COMBO, OnSelchangeMethodCombo)
	//ON_BN_CLICKED(IDC_ENABLE_QUANTIZATION, OnEnableQuantization)
	//ON_EN_CHANGE(IDC_QUANTIZATION_ENTRIES, OnChangeQuantizationEntries)
	ON_BN_CLICKED(IDC_SHOWTOOLTIPS, OnShowtooltips)
	//ON_BN_CLICKED(IDC_HORIZONTAL, OnHorizontal)
	//ON_BN_CLICKED(IDC_VERTICAL, OnVertical)
	//ON_BN_CLICKED(IDC_RIGHT_TOP, OnRightTop)
	//ON_BN_CLICKED(IDC_LEFT_BOTTOM, OnLeftBottom)

	ON_NOTIFY(GC_SELCHANGE, IDC_GRADIENT, OnNotifyChangeSelPeg)
	ON_NOTIFY(GC_PEGMOVE, IDC_GRADIENT, OnNotifyPegMove)
	ON_NOTIFY(GC_PEGMOVED, IDC_GRADIENT, OnNotifyPegMove)
	ON_NOTIFY(GC_PEGREMOVED, IDC_GRADIENT, OnNotifyPegRemoved)
	ON_NOTIFY(GC_CREATEPEG, IDC_GRADIENT, OnNotifyDoubleClickCreate)
	ON_NOTIFY(GC_EDITPEG, IDC_GRADIENT, OnNotifyEditPeg)
	ON_BN_CLICKED(IDC_SAVE, OnBnClickedSave)
	ON_BN_CLICKED(IDC_LOAD, OnBnClickedLoad)
	ON_EN_CHANGE(IDC_VALUE, OnEnChangeValue)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDlgColorGrad message handlers

BOOL CDlgColorGrad::OnInitDialog()
{
	ASSERT(m_pGrad!=NULL);
	//----- Setup the Gradient -----//

	CGradient &gradient = m_wndGradientCtrl.GetGradient( );

	PegArray	*pPegsOld, *pPegsThis;
	pPegsThis =  gradient.GetPegArray();
	pPegsOld = m_pGrad->GetPegArray();

	pPegsThis->RemoveAll();
	gradient.SetStartPegColour(m_pGrad->GetEndPegColour());
	for(int i=0; i<pPegsOld->GetSize(); i++) {
		CPeg peg = pPegsOld->GetAt(i);
		gradient.AddPeg(peg.colour, 1-peg.position);
	}
	gradient.SetEndPegColour(m_pGrad->GetStartPegColour());

	gradient.SetInterpolationMethod( m_pGrad->GetInterpolationMethod() );
	
	gradient.SetHstMin(m_pGrad->GetHstMin());
	gradient.SetHstMax(m_pGrad->GetHstMax());
	gradient.GetHistogram()->Append( *(m_pGrad->GetHistogram()) );

	m_NewPegPosition = (float)(m_pGrad->GetHstMin()+m_pGrad->GetHstMax())/2.0f;
	m_BackgroundColour.SetColor(gradient.GetBackgroundColour());
	m_NewPegColour.SetColor(0x000000FF);
	m_UseBackground = true;
	m_QuantizationEntries = 32;

	m_dVal = m_pGrad->GetHstMin();

	CDialog::OnInitDialog();

	//----- Update the controls -----//
	SetControls();
	
	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CDlgColorGrad::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
//HCURSOR CDlgColorGrad::OnQueryDragIcon()
//{
//	return (HCURSOR) m_hIcon;
//}

long CDlgColorGrad::OnSelEndOK(UINT /*lParam*/, LONG /*wParam*/)
{
	CGradient &gradient = m_wndGradientCtrl.GetGradient();
	int selindex = m_wndGradientCtrl.GetSelIndex();
	
	if(selindex != -1 && selindex > -4)
	{
		const CPeg &peg = gradient.GetPeg(selindex);
		gradient.SetPeg(selindex, m_SelPegColour.GetColor(), peg.position);
	}
	
	gradient.SetBackgroundColour(m_BackgroundColour.GetColor());

	m_wndGradientCtrl.Invalidate();

	//UpdateFirePalette();

	return 0;
}

void CDlgColorGrad::OnNotifyChangeSelPeg(NMHDR * pNotifyStruct, LRESULT *result)
{
	PegNMHDR* pPegNotifyStruct = (PegNMHDR*)pNotifyStruct;
	
	CString str;

	if(pPegNotifyStruct->index == -1)
	{
		m_SelPegColour.EnableWindow(FALSE);
		GetDlgItem(IDC_SELPEG_POSITION)->EnableWindow(FALSE);
		GetDlgItem(IDC_VALUE)->EnableWindow(FALSE);
		GetDlgItem(IDC_DEL_PEG)->EnableWindow(FALSE);
		GetDlgItem(IDC_SELPEG)->SetWindowText(_T("Selected Peg - ID -1 [None]"));
	}
	else
	{	
		m_SelPegColour.EnableWindow(TRUE);
		GetDlgItem(IDC_DEL_PEG)->EnableWindow(TRUE);

		m_SelPegColour.SetColor(pPegNotifyStruct->peg.colour);
		m_SelPegPosition = max(0, pPegNotifyStruct->peg.position);
		UpdateData(FALSE);

		GetDlgItem(IDC_SELPEG_POSITION)->EnableWindow(pPegNotifyStruct->peg.position != -1);
		GetDlgItem(IDC_VALUE)->EnableWindow(pPegNotifyStruct->peg.position != -1);
		GetDlgItem(IDC_DEL_PEG)->EnableWindow(pPegNotifyStruct->peg.position != -1);

		CString append;
		
		switch(pPegNotifyStruct->index)
		{
		case STARTPEG: append = _T(" [Start Peg]"); break;
		case ENDPEG: append = _T(" [End Peg]"); break;
		default: append = _T(""); break;
		}
		str.Format(_T("Selected Peg - ID %d%s"), pPegNotifyStruct->index, append);
		GetDlgItem(IDC_SELPEG)->SetWindowText(str);
	}

	*result = 0;
}

void CDlgColorGrad::OnNotifyPegMove(NMHDR * pNotifyStruct, LRESULT *result)
{
	PegNMHDR* pPegNotifyStruct = (PegNMHDR*)pNotifyStruct;
	
	m_SelPegPosition = pPegNotifyStruct->peg.position;
	m_dVal = PosToVal(m_SelPegPosition);
	UpdateData(FALSE);

	*result = 0;
}

void CDlgColorGrad::OnNotifyPegMoved(NMHDR* , LRESULT *result)
{
	*result = 0;
}

void CDlgColorGrad::OnNotifyPegRemoved(NMHDR* , LRESULT *result)
{
	//UpdateFirePalette();
	*result = 0;
}

void CDlgColorGrad::OnDelPeg() 
{
	m_wndGradientCtrl.DeleteSelected(TRUE);
	//UpdateFirePalette();
}

double CDlgColorGrad::PosToVal(double dPos)
{
	double minD, maxD;
	minD=m_wndGradientCtrl.GetGradient().GetHstMin();
	maxD=m_wndGradientCtrl.GetGradient().GetHstMax();
	
	return (minD + (1-dPos)*(maxD-minD));
}

float CDlgColorGrad::ValToPos(double dVal)
{
	double minD, maxD;
	minD=m_wndGradientCtrl.GetGradient().GetHstMin();
	maxD=m_wndGradientCtrl.GetGradient().GetHstMax();
	if(dVal<minD || dVal>maxD) {
		AfxMessageBox("Minimum or maximum overflow!", MB_OK | MB_ICONINFORMATION);
		return -1;
	}
	return (float)(1 - (dVal-minD)/(maxD-minD));
}

void CDlgColorGrad::OnAddPeg() 
{
	UpdateData(TRUE);
	float newPos;
	newPos = ValToPos(m_NewPegPosition);
	m_wndGradientCtrl.GetGradient().AddPeg(m_NewPegColour.GetColor(), newPos);
	m_wndGradientCtrl.Invalidate();

	//UpdateFirePalette();
}

void CDlgColorGrad::OnEnChangeValue()
{
	UpdateData(TRUE);
	m_SelPegPosition = ValToPos(m_dVal);;

	int newsel = m_wndGradientCtrl.MoveSelected(m_SelPegPosition, TRUE); 
		
	CString append,str;
		
	switch(newsel)
	{
	case STARTPEG: append = _T(" [Start Peg]"); break;
	case ENDPEG: append = _T(" [End Peg]"); break;
	default: append = _T(""); break;
	}
	//str.Format(_T("Selected Peg - ID %d%s"), newsel, append);
	//GetDlgItem(IDC_SELPEG)->SetWindowText(str);
}

void CDlgColorGrad::OnChangeSelpegPosition() 
{
	UpdateData(TRUE);

	int newsel = m_wndGradientCtrl.MoveSelected(m_SelPegPosition, TRUE); 
		
	CString append,str;
		
	switch(newsel)
	{
	case STARTPEG: append = _T(" [Start Peg]"); break;
	case ENDPEG: append = _T(" [End Peg]"); break;
	default: append = _T(""); break;
	}
	str.Format(_T("Selected Peg - ID %d%s"), newsel, append);
	GetDlgItem(IDC_SELPEG)->SetWindowText(str);

	//UpdateFirePalette();		
}

//void CDlgColorGrad::OnAbout() 
//{
//	CAboutDlg dlg;
//	dlg.DoModal();
//}

void CDlgColorGrad::OnNotifyDoubleClickCreate(NMHDR * pNotifyStruct, LRESULT *result)
{
	PegCreateNMHDR* pPegCreateNotifyStruct = (PegCreateNMHDR*)pNotifyStruct;
	
	UpdateData(TRUE);
	m_wndGradientCtrl.GetGradient().AddPeg(pPegCreateNotifyStruct->colour,
		pPegCreateNotifyStruct->position);
	m_wndGradientCtrl.Invalidate();

	//UpdateFirePalette();

	*result = 0;
}

void CDlgColorGrad::OnNotifyEditPeg(NMHDR * pNotifyStruct, LRESULT *result)
{
	PegNMHDR* pPegNotifyStruct = (PegNMHDR*)pNotifyStruct;
	
	CString message;
	message.Format(_T("Position: %.2f\nColour: RGB(%u, %u, %u)"), pPegNotifyStruct->peg.position,
		GetRValue(pPegNotifyStruct->peg.colour),
		GetGValue(pPegNotifyStruct->peg.colour),
		GetBValue(pPegNotifyStruct->peg.colour));
	MessageBox(message, _T("You just double clicked on a peg!"));

	*result = 0;
}

void CDlgColorGrad::OnUseBackground() 
{
	UpdateData();
	m_wndGradientCtrl.GetGradient().SetUseBackground(m_UseBackground);
	m_wndGradientCtrl.Invalidate();
	//UpdateFirePalette();
}

void CDlgColorGrad::OnSelchangeMethodCombo() 
{
	int sel = m_wndMethodCombo.GetCurSel();
	if(sel == -1)
		return;
	m_wndGradientCtrl.GetGradient().
	SetInterpolationMethod((CGradient::InterpolationMethod)sel);
	m_wndGradientCtrl.Invalidate();
	//UpdateFirePalette();
}

//void CDlgColorGrad::OnEnableQuantization() 
//{
//	UpdateData();
//	//GetDlgItem(IDC_QUANTIZATION_ENTRIES)->EnableWindow(m_EnableQuantization);
//
//	m_wndGradientCtrl.GetGradient().SetQuantization(m_EnableQuantization? m_QuantizationEntries : -1);
//
//	m_wndGradientCtrl.Invalidate();
//	UpdateFirePalette();
//}

//void CDlgColorGrad::OnChangeQuantizationEntries() 
//{
//	UpdateData();
//	m_wndGradientCtrl.GetGradient().
//		SetQuantization(m_EnableQuantization
//		? m_QuantizationEntries : -1);
//	m_wndGradientCtrl.Invalidate();
//	UpdateFirePalette();
//}

void CDlgColorGrad::OnShowtooltips() 
{
	UpdateData();
	m_wndGradientCtrl.ShowTooltips(m_bShowToolTips);
}

void CDlgColorGrad::SetControls()
{
	//----- Set the interpolation method -----//
	m_wndMethodCombo.SetCurSel((int)m_wndGradientCtrl.GetGradient().GetInterpolationMethod());
	//GetDlgItem(IDC_QUANTIZATION_ENTRIES)->EnableWindow(false);
	
	//----- Set the dialog's selection -----//
	LRESULT result;
	PegNMHDR nmhdr;
	nmhdr.nmhdr.code = 0;
	nmhdr.nmhdr.idFrom = 0;
	nmhdr.nmhdr.hwndFrom = 0;
	nmhdr.index = -1;
	OnNotifyChangeSelPeg((NMHDR*) &nmhdr, &result);

	//----- Select the right radio button -----//
	m_wndGradientCtrl.SetPegSide(true, true);
	//((CButton*)GetDlgItem(IDC_RIGHT_TOP))->SetCheck(1);
	//m_wndGradientCtrl.SetPegSide(false, false);
	//((CButton*)GetDlgItem(IDC_LEFT_BOTTOM))->SetCheck(0);
	
	CRect rc;
	GetClientRect(&rc);
	GetDlgItem(IDC_GRADIENT)->SetWindowPos(GetDlgItem(IDC_GRADIENT), 11, 11, rc.Width()-240, rc.Height()-32, SWP_NOZORDER);
	m_wndGradientCtrl.SetOrientation(CGradientCtrl::ForceVertical);
	//((CButton*)GetDlgItem(IDC_VERTICAL))->SetCheck(1);
	//((CButton*)GetDlgItem(IDC_HORIZONTAL))->SetCheck(0);
}

//void CDlgColorGrad::OnHorizontal() 
//{
//	GetDlgItem(IDC_GRADIENT)->SetWindowPos(GetDlgItem(IDC_GRADIENT), 10,225, 830, 120, SWP_NOZORDER);
//}
//
//void CDlgColorGrad::OnVertical() 
//{
//	GetDlgItem(IDC_GRADIENT)->SetWindowPos(GetDlgItem(IDC_GRADIENT), 11,11, 107, 330, SWP_NOZORDER);
//}
//
//void CDlgColorGrad::OnRightTop() 
//{
//	m_wndGradientCtrl.SetPegSide(true, ((CButton*)GetDlgItem(IDC_RIGHT_TOP))->GetCheck());
//	m_wndGradientCtrl.Invalidate();
//}
//
//void CDlgColorGrad::OnLeftBottom() 
//{
//	m_wndGradientCtrl.SetPegSide(false, ((CButton*)GetDlgItem(IDC_LEFT_BOTTOM))->GetCheck());
//	m_wndGradientCtrl.Invalidate();
//}

void CDlgColorGrad::OnOK()
{
	CGradient &gradient = m_wndGradientCtrl.GetGradient( );

	PegArray	*pPegsOld, *pPegsThis;
	pPegsThis =  gradient.GetPegArray();
	pPegsOld = m_pGrad->GetPegArray();

	pPegsOld->RemoveAll();

	m_pGrad->SetStartPegColour(gradient.GetEndPegColour());
	for(int i=0; i<pPegsThis->GetSize(); i++) {
		CPeg peg = pPegsThis->GetAt(i);
		m_pGrad->AddPeg(peg.colour, 1-peg.position);
	}
	m_pGrad->SetEndPegColour(gradient.GetStartPegColour());

	m_pGrad->SetInterpolationMethod( gradient.GetInterpolationMethod() );

	CDialog::OnOK();
}

void CDlgColorGrad::OnBnClickedSave()
{
	CFileDialog dlg(FALSE, _T("cip"), _T("Untitled.cip"),
		OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_EXPLORER,
			_T("Colour Scheme (*.cip)|*.cip||"));

	if(dlg.DoModal() == IDOK)
	{
		CFile file(dlg.GetPathName(), CFile::modeWrite | CFile::modeCreate | CFile::shareDenyWrite );
		CArchive ar(&file, CArchive::store);
		m_wndGradientCtrl.GetGradient().Serialize(ar);
	}
}

void CDlgColorGrad::OnBnClickedLoad()
{
	CFileDialog dlg(TRUE, _T("cip"), _T("Untitled.cip"),
		OFN_HIDEREADONLY | OFN_EXPLORER,
			_T("Colour Scheme (*.cip)|*.cip||"));

	if(dlg.DoModal() == IDOK)
	{
		CFile file(dlg.GetPathName(), CFile::modeRead | CFile::shareDenyWrite );
		CArchive ar(&file, CArchive::load);
		
		m_wndGradientCtrl.GetGradient().Serialize(ar);

		m_wndGradientCtrl.SetSelIndex(-1);
		m_wndGradientCtrl.Invalidate();
		
		SetControls();
	}
}



void CDlgColorGrad::OnCancel()
{
	OnOK();
}
