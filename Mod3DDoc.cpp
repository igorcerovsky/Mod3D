// Mod3DDoc.cpp : implementation of the CMod3DDoc class
//

#include <math.h>
#include <string.h>
#include "stdafx.h"
#include "Mod3D.h"
#include "MainFrm.h"
#include "Mod3DDoc.h"
#include "VersionType.h"
#include "Routines.h"

// dialogs
#include "DlgBodyModf.h"
#include "DlgInducingField.h"
#include "DlgVerticalRange.h"
#include "DlgEditBodies.h"
#include "DlgDefineObs.h"
#include "DlgBodyImport.h"
#include "DlgBodyExport.h"
#include "DlgSelectBody.h"
#include "DlgImportPicture.h"
#include "DlgPictureManager.h"
#include "DlgEditCheckMarks.h"
#include "DlgModelActiveGrd.h"
#include "DlgModelFldIndicator.h"
#include "GridObj.h"
#include "DlgImpExpFld.h"

#include "DlgPpBodyComp.h"
#include "DlgPpBodyDraw.h"
#include "DlgPpBodyDescr.h"
#include "DlgPpBodyGrav.h"
#include "DlgPpBodyMag.h"

#include "DlgPpCompComp.h"
#include "DlgPpCompGrav.h"
#include "DlgPpCompMag.h"

#include "DlgObservations.h"
#include "DlgStart.h"
#include "DlgModGrids.h"
#include "DlgSelGridProp.h"

extern long g_nTotComp;	// rows*num_ofBodies computation prcentage
extern long g_nComp;


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern double	g_x, g_y, g_z;
extern int		g_row, g_col;
extern int		g_nIndFld;
extern CString	g_strFld;
extern double	g_dFld;

/////////////////////////////////////////////////////////////////////////////
// CMod3DDoc

IMPLEMENT_DYNCREATE(CMod3DDoc, CDocument)

BEGIN_MESSAGE_MAP(CMod3DDoc, CDocument)

	ON_COMMAND(ID_COMPUTE_COMPUTE, OnComputeCompute)
	ON_COMMAND(ID_MODEL_DEFINE_OBSERVATION, OnModelDefineObservation)
	ON_UPDATE_COMMAND_UI(ID_MODEL_DEFINE_OBSERVATION, OnUpdateModelDefineObservation)
	ON_COMMAND(ID_MODEL_IMPORT_OBSERVATION, OnModelImportObservation)
	ON_UPDATE_COMMAND_UI(ID_MODEL_IMPORT_OBSERVATION, OnUpdateModelImportObservation)
	ON_COMMAND(ID_MODEL_INDUCING_FIELD, OnModelInducingField)
	ON_COMMAND(ID_MODEL_CHANGE_VERTICAL_RANGE, OnModelChangeVerticalRange)
	ON_COMMAND(ID_BODY_EDIT, OnBodyEdit)
	ON_COMMAND(ID_MODEL_IMPORT_MEASURED_FIELD, OnModelImportMeasuredField)
	ON_COMMAND(ID_COMPUTE_PROPERTIES, OnComputeProperties)
	ON_COMMAND(ID_VIEW_EDIT_PICTURE, OnViewEditPicture)
	ON_COMMAND(ID_COMPUTE_ACTIVE_FIELD, OnComputeActiveField)
	ON_COMMAND(ID_VIEW_SETFIELDINDICATOR, OnViewSetfieldindicator)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SETFIELDINDICATOR, OnUpdateViewSetfieldindicator)
	ON_COMMAND(ID_COMPUTE_REAL_TIME, OnComputeRealTime)
	ON_UPDATE_COMMAND_UI(ID_COMPUTE_REAL_TIME, OnUpdateComputeRealTime)
	ON_COMMAND(ID_MODEL_PROPERTIES, OnModelProperties)
	ON_COMMAND(ID_IMPORT_BITMAP, OnImportBitmap)
	ON_UPDATE_COMMAND_UI(ID_IMPORT_BITMAP, OnUpdateImportBitmap)
	ON_COMMAND(ID_EDIT_GUIDELINES, OnEditGuidelines)
	ON_UPDATE_COMMAND_UI(ID_EDIT_GUIDELINES, OnUpdateEditGuidelines)
	ON_COMMAND(ID_IMPORT_BODY, OnImportBody)
	ON_UPDATE_COMMAND_UI(ID_IMPORT_BODY, OnUpdateImportBody)
	ON_UPDATE_COMMAND_UI(ID_IMPORT_GUIDE_LINE, OnUpdateImportGuideLine)
	ON_COMMAND(ID_IMPORT_MEASURED_FIELD, OnImportMeasuredField)
	ON_UPDATE_COMMAND_UI(ID_IMPORT_MEASURED_FIELD, OnUpdateImportMeasuredField)

	ON_COMMAND(ID_IMPORT_GUIDE_LINE, OnImportGuideLine)
	ON_COMMAND(ID_EXPORT_GUIDE_LINE, OnExportGuideLine)
	ON_UPDATE_COMMAND_UI(ID_EXPORT_GUIDE_LINE, OnUpdateExportGuideLine)

	ON_COMMAND(ID_IMPORT_GRID, OnImportGrid)
	ON_COMMAND(ID_EXPORT_MODELED_FIELD, OnExportModeledField)
	ON_COMMAND(ID_COMPUTE_FIT_1D, OnComputeFit1d)
	ON_COMMAND(ID_COMPUTE_DENSITY_INVERSION_PROPERTIES, OnComputeDensityInversionProperties)
	ON_COMMAND(ID_EXPORT_BODY, OnExportBody)
	ON_UPDATE_COMMAND_UI(ID_EXPORT_BODY, OnUpdateExportBody)
	ON_COMMAND(ID_MODEL_BLANK_GRID, OnModelBlankGrid)
	ON_COMMAND(ID_MODEL_OBSERVATION_PROPERTIES, OnModelObservationProperties)
	ON_COMMAND(ID_IMPORT_WELL, OnImportWell)
	ON_COMMAND(ID_EDIT_FIELD_GRID, OnEditFieldGrid)

	//ON_COMMAND(ID_OBJECT_MANAGER, OnEditObjectManager)
	//ON_UPDATE_COMMAND_UI(ID_OBJECT_MANAGER, OnUpdateEditObjectManager)

	ON_COMMAND(ID_IMPORT_3DDATA, OnImport3ddata)
	ON_COMMAND(ID_COMPUTE_ACTIVATE_GRID, OnComputeActivateGrid)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMod3DDoc construction/destruction

CMod3DDoc::CMod3DDoc()
{

	// current verion
	m_nVersion = 20040419;				// date: 2004 04 11		// remove mean		
	//m_nVersion = 20031113;			// date: 2003 11 13		// separate observation grids		
	//m_nVersion = 20031105;			// date: 2003 11 05			
	//m_nVersion = 20031129;			// date: 2003 10 29		// fitting properties additions
	//m_nVersion = 20031121;			// date: 2003 10 21		I was 'temny' and made error in data
	//m_nVersion = 20030921;			// date: 2003 09 21
	// versions history
	//		m_nVersion = 2001;		// introduced 21.9.2003
	//		header: "Mod3D 2.001"
	//		header: "Mod3D 2.00"
	/////////////////////////////


	m_nIndicatorFld = GRDMOD_GZ;
	m_nAllComponents = 60;	
	m_nModComponents= 20;	
	m_nDifComponents = 40;
	m_nGrids = 60;
	m_nID = 0;

	m_nComputationType = CMP_NONE;
	m_pCompThread = NULL;
	m_pThreadFit = NULL;
	m_bSherComp = FALSE;
	m_bRemoveMeanGrv = FALSE;
	m_bRemoveMeanTns = FALSE;
	m_bRemoveMeanMag = FALSE;
	
	// gravity
	m_nGrvFormula = POHANKA;
	m_dGrvSens = 0.001;
	m_nGrvObsTag = OBS_SENS_HEIGHT;
	m_dGrvUnits = 1.0e6;
	m_dGrvDensRef = 2670.0;

	// magnetics
	m_nMagFormula = GUPTASARMA_SINGH;
	m_dMagSens = 1.0;
	m_dMagUnits = 1.0;
	m_nMagObsTag = OBS_SENS_HEIGHT;
	// inducing field
	m_vMagIndFld.x = 60.0;
	m_vMagIndFld.y = 1.0;
	m_vMagIndFld.z = 48000.0;

	// gravity tensor
	m_gvrTens.bCompute = TRUE;
	m_gvrTens.dFlightElev = 700;
	m_gvrTens.nTag = OBS_FLIGHT_ELEVATON;
	m_gvrTens.dHeight = 100;
	m_gvrTens.dUnits = 1.0e9;


	// body vertex move
	//m_dBodyCreatRatio = 0.95;
	//m_bodyVrtxMove.bConstTop = FALSE;
	//m_bodyVrtxMove.bConstBot = FALSE;
	//m_bodyVrtxMove.dTop = 0;
	//m_bodyVrtxMove.dBot = 0;
	//m_bodyVrtxMove.bMoveBot = TRUE;
	//m_bodyVrtxMove.bMoveTop = TRUE;
	//m_bodyVrtxMove.bFixBot = FALSE;
	//m_bodyVrtxMove.bFixRel = FALSE;
	//m_bMoveBodyVertical = FALSE;


	m_ObsRel.SetColorTable(CTBL_REL);
	for(int i=0; i<m_nAllComponents; i++ )
		m_grdActiveCompute[i] =0;

	// fitting
	m_nFitVrtxMeth = FIT_BRENT;
	m_nFitVrtxMaxIter = 100;
	m_nFitFld = -1;
	m_dFitVrtxTol = 3.0e-8;
	m_dFitVrtxEps = 0.1;
	m_nFitVrtxChar = FIT_DRV;
	m_bFitVrtxLog = TRUE;
	m_dFitVrtxEpsAuto = 0.01;
	m_bFitVrtxEpsAuto = TRUE;
	m_nFitDensChar = FIT_DRV;
	m_dFitDensEps = 1.0;
	m_nFitDensMeth = FIT_BRENT;
	m_nFitDensMaxIter = 100;
	m_dFitDensTol = 3.0e-8;
	m_bFitDensLog = TRUE;
	m_strFitDensLog = "DensLog.dat";
	m_strFitVrtxLog = "VertexLog.dat";

	m_nColCur = -1;
	m_nRowCur = -1;
}

CMod3DDoc::~CMod3DDoc()
{
	// free memory if allocated
	m_ObsRel.Close();
	for(int i = 0; i < m_nGrids; i++) {
		m_grd[i].Close();
	}
}


BOOL CMod3DDoc::OnNewDocument()
{
	CDlgStart dlg;

	dlg.m_nTag = 0;

	dlg.DoModal();

	switch( dlg.m_nTag) {
		case 0:
			OnModelDefineObservation();
			break;
		case 1:
			OnModelImportObservation();
			break;
		default:
			OnModelDefineObservation();
			break;
	}

	if( !CDocument::OnNewDocument() )
		return FALSE;


	UpdateAllViews( NULL, UPDATE_NEW_MODEL );

	return TRUE;
}

BOOL CMod3DDoc::OnOpenDocument(LPCTSTR lpszPathName) 
{
	if (!CDocument::OnOpenDocument(lpszPathName))
		return FALSE;
	
	return TRUE;
}

void CMod3DDoc::OnCloseDocument() 
{
	if( m_pCompThread != NULL ) {
		m_pCompThread->SuspendThread();
		int ret = AfxMessageBox("The computation is currently running.\n Close anyway?", MB_YESNO | MB_ICONQUESTION);
		if( ret == IDYES ) {
			m_pCompThread->Delete();
			m_pCompThread = NULL;
		}
		else {
			m_pCompThread->ResumeThread();
			return;
		}
	}
	
	AfxGetMainWnd()->SendMessage(WM_UPDATE_OBJMNG, 0, -1);
	CDocument::OnCloseDocument();
}

// CMy3dDefDoc commands
CModel* CMod3DDoc::GetModel()	
{
	if( m_model.IsInitialized() )
		return &m_model;
	return NULL;
}

// sets all the values to zero
// typically called before computation
void CMod3DDoc::ZeroModelGrids(void)
{
	for(int k=0; k<m_nDifComponents; k++) {
		if( m_grdActiveCompute[k] ) {
			//m_grd[k].ZeroData();
			m_grd[k].DummyData();
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// CMod3DDoc diagnostics

#ifdef _DEBUG
void CMod3DDoc::AssertValid() const
{
	CDocument::AssertValid();
}

void CMod3DDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG



/////////////////////////////////////////////////////////////////////////////
// CMod3DDoc commands
BOOL CMod3DDoc::IsEmpty()
{
	return m_model.IsEmpty();
}

CMyObject* CMod3DDoc::GetObject(int nIndex)
{
	if( nIndex < 0 || nIndex > m_objPtrArray.GetUpperBound() )
		return NULL;

	return m_objPtrArray.GetAt(nIndex);;
}


double** CMod3DDoc::GetObservationData()
{
	if(!m_ObsRel.IsEmpty()) 
		return m_ObsRel.GetData();
	else
		return NULL;
}

OBSHDR CMod3DDoc::GetObservInfo()
{
	return m_obsHdr;
}

void CMod3DDoc::SetCurrentPos(int row, int col)
{
	m_nRowCur = row;
	m_nColCur = col;
}

void CMod3DDoc::GetCurrentPos(int &row, int &col)
{
	 row = m_nRowCur;
	 col = m_nColCur;
}

void CMod3DDoc::UpdateMinMaxFldVal()
{
	for(int n=0; n<m_nDifComponents; n++) {
		if( m_grdActiveCompute[n] ) {
			m_grd[n].UpdateMinMaxValue();
		}
	}
}

// sets modified flag && updates field
void CMod3DDoc::UpdateMinMaxFldValModified()
{
	for(int n=0; n<m_nDifComponents; n++) {
		if( m_grdActiveCompute[n] ) {
			m_grd[n].SetModifiedFlag();
			m_grd[n].UpdateMinMaxValue();
		}
	}
}

// sets modified flag to active model && difference grids
void CMod3DDoc::SetModifiedFlagModFld(int bModified)
{
	for(int n=0; n<m_nDifComponents; n++) {
		if( m_grdActiveCompute[n] ) {
			m_grd[n].SetModifiedFlag( bModified );
		}
	}
}

void CMod3DDoc::GetObsMinMax(double &min, double &max)
{
	m_ObsRel.UpdateMinMaxValue();
	m_ObsRel.GetMinMaxValue(min, max);
}

void CMod3DDoc::OnModelInducingField() 
{
	CDlgInducingField dlg;

	dlg.m_dIncl = m_vMagIndFld.x;
	dlg.m_dDecl = m_vMagIndFld.y;
	dlg.m_dTot  = m_vMagIndFld.z;
	if(dlg.DoModal() == IDOK) {
		m_vMagIndFld.x = dlg.m_dIncl;
		m_vMagIndFld.y = dlg.m_dDecl;
		m_vMagIndFld.z = dlg.m_dTot;
		ComputeAmbientField();
		int ret = AfxMessageBox("Recalculate field?", MB_YESNO | MB_ICONQUESTION);
		if(ret == IDYES) {
			OnComputeCompute();
		}
		SetModifiedFlag();
	}
}

double** CMod3DDoc::GetGridData(int nFld)
{
	if( !m_grd[nFld].IsEmpty() )
		return m_grd[nFld].GetData();
	return NULL;
}

// get grids data only if grid is active for computation
double** CMod3DDoc::GetGridDataComp(int nFld)
{
	if( !m_grd[nFld].IsEmpty() && m_grdActiveCompute[nFld] )
		return m_grd[nFld].GetData();
	return NULL;
}

// return data from inversion grids
double** CMod3DDoc::GetGridDataInv(int nFld, BOOL bActiveOnly)
{
	ASSERT(nFld>=0);
	ASSERT(nFld<19);
	if( !m_grdInv[nFld].IsEmpty() && (m_grdActiveCompute[nFld] || !bActiveOnly) )
		return m_grdInv[nFld].GetData();
	return NULL;
}

CGrid* CMod3DDoc::GetGrid(int nTag)
{
	if(nTag < 0 || nTag>m_nAllComponents)
		return NULL;

	if( !m_grd[nTag].IsEmpty() && !IsEmpty() )
		return &m_grd[nTag];

	return NULL;
}

/////////////////////////////////////////////////////////////////////////////
//************* model commands ******************************************
BOOL CMod3DDoc::MakeObservations()
{
	OBSHDR* oh;
	oh = &m_obsHdr;

	m_vGrvDensOrigo.x = (m_obsHdr.dMaxX + m_obsHdr.dMinX)/2;
	m_vGrvDensOrigo.y = (m_obsHdr.dMaxY + m_obsHdr.dMinY)/2;
	m_vGrvDensOrigo.z = m_obsHdr.dMaxZ;

	m_ObsGrv.New(oh->nRows, oh->nCols, oh->x0, oh->y0, oh->xSize, oh->ySize);
	m_ObsRel.CopyData( &m_ObsGrv );
	m_ObsGrv += m_dGrvSens;
	m_ObsMag.New(oh->nRows, oh->nCols, oh->x0, oh->y0, oh->xSize, oh->ySize);
	m_ObsRel.CopyData( &m_ObsMag );
	m_ObsMag += m_dMagSens;
	m_ObsTns.New(oh->nRows, oh->nCols, oh->x0, oh->y0, oh->xSize, oh->ySize);
	m_ObsRel.CopyData( &m_ObsTns );
	m_ObsTns += m_gvrTens.dHeight;

	// select fields for modelling
	CDlgModGrids dlg;
	dlg.m_grd = m_grd;
	dlg.m_nCols = m_obsHdr.nCols;
	dlg.m_nRows = m_obsHdr.nRows;
	dlg.m_x0 = m_obsHdr.x0;
	dlg.m_y0 = m_obsHdr.y0;
	dlg.m_xSize = m_obsHdr.xSize;
	dlg.m_ySize = m_obsHdr.ySize;

	dlg.DoModal();

	// set modeled fields active
	for(int i = 0; i < GRDMOD_MXX; i++) {
		if( !m_grd[i].IsEmpty() )
			m_grdActiveCompute[i] = TRUE;
	}

	m_dGrvElev = oh->dMaxZ + m_dGrvSens;
	m_dMagElev = oh->dMaxZ + m_dMagSens;
	m_gvrTens.dFlightElev = oh->dMaxZ + m_gvrTens.dHeight;

	OnModelObservationProperties();

	m_model.SetComputeRealTime( m_nComputationType );
	
	return TRUE;
}

void CMod3DDoc::OnModelDefineObservation() 
{
	if( !m_model.IsEmpty() )	return;

	CDlgDefineObs dlg;
	OBSHDR *oh = &m_obsHdr;

	if( dlg.DoModal() == IDOK ) {
		oh->x0 = dlg.m_dMinX;
		oh->y0 = dlg.m_dMinY;
		oh->dMinX = dlg.m_dMinX;
		oh->dMaxX = dlg.m_dMaxX;
		oh->dMinY = dlg.m_dMinY;
		oh->dMaxY = dlg.m_dMaxY;
		oh->dMinZ = dlg.m_dMinZ;
		oh->dMaxZ = dlg.m_dMaxZ;
		if(dlg.m_nTag == CDlgDefineObs::obsdNormal ) {
			oh->xSize = dlg.m_dIncrX;
			oh->ySize = dlg.m_dIncrY;
			oh->nRows = (int) ceil( (oh->dMaxY - oh->dMinY) / oh->ySize ) + 1;
			oh->nCols = (int) ceil( (oh->dMaxX - oh->dMinX) / oh->xSize ) + 1;
		}
		else {
			oh->nRows = dlg.m_nRows;
			oh->nCols = dlg.m_nCols;
			oh->xSize = dlg.m_csx;
			oh->ySize = dlg.m_csy;
		}
	}
	else {
		AfxMessageBox("Default observationa area will be created.", MB_OK | MB_ICONINFORMATION);
		oh->x0 = -100;
		oh->y0 = -100;
		oh->dMinX = -100;
		oh->dMaxX = 100;
		oh->dMinY = -100;
		oh->dMaxY = 100;
		oh->dMinZ = -200;
		oh->dMaxZ = 0;
		oh->xSize = 10;
		oh->ySize = 10;
		oh->nRows = (int) ceil( (oh->dMaxY - oh->dMinY) / oh->ySize ) + 1;
		oh->nCols = (int) ceil( (oh->dMaxX - oh->dMinX) / oh->xSize ) + 1;
	}

	m_model.Init(oh->nRows, oh->nCols, oh->x0, oh->y0, oh->xSize, oh->ySize, oh->dMinZ, oh->dMaxZ);
	m_ObsRel.New(oh->nRows, oh->nCols, oh->x0, oh->y0, oh->xSize, oh->ySize);
	m_ObsRel.SetValue(dlg.m_dMaxZ);
	MakeObservations();
	SetModifiedFlag();
	// !!! demo

	CheckDemo();
}

void CMod3DDoc::OnUpdateModelDefineObservation(CCmdUI* pCmdUI) 
{
	if( !m_model.IsEmpty() ) {		pCmdUI->Enable(FALSE);	}
}

void CMod3DDoc::OnModelImportObservation() 
{
	if( !m_model.IsEmpty() )	return;

	if(m_ObsRel.Open()) {
		OBSHDR *oh = &m_obsHdr;
		GridInfo gi = m_ObsRel.GetInfo();
		oh->x0 = gi.x0;
		oh->y0 = gi.y0;
		oh->dMinX = gi.dMinX;
		oh->dMaxX = gi.dMaxX;
		oh->dMinY = gi.dMinY;
		oh->dMaxY = gi.dMaxY;
		oh->dMinZ = gi.dMinZ;
		oh->dMaxZ = gi.dMaxZ;
		oh->xSize = gi.xSize;
		oh->ySize = gi.ySize;
		oh->nRows = gi.nRow;
		oh->nCols = gi.nCol;

		if( !CheckDemo() ) return;

		if ( m_ObsRel.CheckDummies( -12000.0, 30000.0) > 0 ) {
			if( AfxMessageBox( "Dummies detected. Replace?", MB_YESNO | MB_ICONQUESTION) == IDYES) {
				m_ObsRel.FillDummies( 0.0, -12000.0, 30000.0 );
			}
			else {
				AfxMessageBox( "Operation Canceled" );
				m_ObsRel.Close();
				return;
			}
		}

		m_model.Init( &m_ObsRel, oh->dMinZ, oh->dMaxZ );
		ChangeVerticalRange();
		MakeObservations();
		SetModifiedFlag();
	}
	else {
		AfxMessageBox("Observation Data not imported.", MB_OK | MB_ICONINFORMATION);
	}
}


void CMod3DDoc::OnUpdateModelImportObservation(CCmdUI* pCmdUI) 
{
	if( !m_model.IsEmpty() ) {	pCmdUI->Enable(FALSE);	}
}

void CMod3DDoc::OnModelChangeVerticalRange() 
{
	ChangeVerticalRange();
}

void CMod3DDoc::OnModelImportMeasuredField() 
{
	CDlgImpExpFld dlg;
	
	GetFldStringArray(dlg.m_strExpAr);
	GetFldStringArray(dlg.m_strImpAr, GRDMES_GX, GRDMES_M+1, TRUE, TRUE);

	dlg.m_nType = CDlgImpExpFld::dlgImport;
	dlg.m_pDoc = this;
	dlg.m_fldGrd = m_grd;						// poiner to CGrid array
	dlg.m_obsHdr = GetObservationHeader();		// observation header
	dlg.m_grdActive = m_grdActiveCompute;		// pointer to int array
	dlg.DoModal();
	SetModifiedFlag();
	UpdateAllViews(NULL);
}

void CMod3DDoc::NewDifferenceField(int nMeasuredField)
{
	CGrid* pGrdDif, *pGrdMes;

	pGrdMes = GetGrid(nMeasuredField);
	pGrdDif = GetGrid(nMeasuredField-20);

	if( pGrdDif == NULL ) {
		GridInfo gi;
		gi = pGrdMes->GetInfo();
		ASSERT(pGrdMes != NULL);
		m_grd[nMeasuredField-20].New(gi.nRow, gi.nCol, gi.x0, gi.y0, gi.xSize, gi.ySize);
		m_grd[nMeasuredField-20].SetColorTable( CTBL_DIF );
	}

	m_grdActiveCompute[nMeasuredField] = TRUE;
	m_grdActiveCompute[nMeasuredField-20] = TRUE;
	ComputeDifferenceField();
	UpdateAllViews(NULL);
}

void CMod3DDoc::ChangeVerticalRange()
{
	CDlgVerticalRange dlgR;
	dlgR.m_dMaxZ = (float) m_obsHdr.dMaxZ;
	dlgR.m_dMinZ = (float) m_obsHdr.dMinZ;
	if(dlgR.m_dMinZ == dlgR.m_dMaxZ )
		dlgR.m_dMinZ -= 1000; 
	dlgR.m_strMaxX.Format("%6.1f", (float) m_obsHdr.dMaxX);
	dlgR.m_strMaxY.Format("%6.1f", (float) m_obsHdr.dMaxY);
	dlgR.m_strMinX.Format("%6.1f", (float) m_obsHdr.dMinX);
	dlgR.m_strMinY.Format("%6.1f", (float) m_obsHdr.dMinY);
	if(dlgR.DoModal() == IDOK) {
		m_obsHdr.dMaxZ = dlgR.m_dMaxZ;
		m_obsHdr.dMinZ = dlgR.m_dMinZ;
	}
	m_model.SetVerticalRange(dlgR.m_dMinZ, dlgR.m_dMaxZ);
	SetModifiedFlag();
	//UpdateAllViews(NULL);
}

int CMod3DDoc::ExportFacetLst(FacetList& fctLst)
{
	// controlling facet list
	CStdioFile file("FacetList.fct", CFile::modeCreate | CFile::modeWrite | CFile::typeText);
	CString str, str1;
	CFacet3Pt *pFct;
	POSITION pos = fctLst.GetHeadPosition();
	while(pos) {
		pFct = &fctLst.GetNext( pos );
		//str.Format(" %d",pFct->nType );
		CPoint3D cntr = pFct->Centroid();
		str.Format(" %d 'x=%f y=%f z=%f'",pFct->nType, (float)cntr.x, (float)cntr.y, (float)cntr.z );
		if( pFct->pBody )		str += " "+pFct->pBody->GetID();
		else					str += " NULL";
		if( pFct->pBodyOpos )	str += " "+pFct->pBodyOpos->GetID();
		else					str += " NULL";
		str += "\n";
		file.WriteString( LPCTSTR(str) );
		// facet points
		str1.Format(" %3.9f, %3.9f, %3.9f   :   %3.9f, %3.9f, %3.9f   :   %3.9f, %3.9f, %3.9f \n\n", 
			(float)pFct->pts[0].x, (float)pFct->pts[0].y, (float)pFct->pts[0].z,
			(float)pFct->pts[1].x, (float)pFct->pts[1].y, (float)pFct->pts[1].z,
			(float)pFct->pts[2].x, (float)pFct->pts[2].y, (float)pFct->pts[2].z);
		file.WriteString( LPCTSTR(str1) );
		str.ReleaseBuffer();
	}
	file.Close();

	return 1;
}

//************* end model commands ******************************************
/////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////
//************* compute commands ********************************************
void CMod3DDoc::OnComputeCompute() 
{
	// if computing thread is running
	if( m_pCompThread != NULL ) {
		m_pCompThread->SuspendThread();
		int ret = AfxMessageBox("The computation is currently running.\n Restart Computation?", MB_YESNO | MB_ICONQUESTION);
		if( ret == IDYES ) {
			m_pCompThread->SuspendThread();
			m_pCompThread->Delete();
			m_pCompThread = NULL;
		}
		else {
			m_pCompThread->ResumeThread();
			return;
		}
	}

	if( !m_model.IsEmpty() ) {
		m_pCompThread = AfxBeginThread(ComputeFldThread, this, THREAD_PRIORITY_NORMAL);
	}
	else {
		AfxMessageBox("No body is defined.");
	}
}

void CMod3DDoc::OnComputeProperties() 
{
	CPropertySheet sheet;
	CDlgPpCompComp	pageCmp;
	CDlgPpCompGrav	pageGrv;
	CDlgPpCompMag	pageMag;

	sheet.AddPage(&pageCmp);
	sheet.AddPage(&pageGrv);
	sheet.AddPage(&pageMag);

	pageCmp.m_bSpherComp = m_bSherComp;
	pageCmp.m_nComputationType = m_nComputationType;

	pageGrv.m_dGravSens = m_dGrvSens;
	pageGrv.m_dDensRef =  m_dGrvDensRef;
	pageGrv.m_dDGx =  m_vGrvDensGrad.x;
	pageGrv.m_dDGy =  m_vGrvDensGrad.y;
	pageGrv.m_dDGz =  m_vGrvDensGrad.z;
	pageGrv.m_dDOx =  m_vGrvDensOrigo.x;
	pageGrv.m_dDOy =  m_vGrvDensOrigo.y;
	pageGrv.m_dDOz =  m_vGrvDensOrigo.z;
	pageGrv.m_dUnits = m_dGrvUnits;
	pageGrv.m_dGradUnits = m_gvrTens.dUnits;
	pageGrv.m_bMeanGrv = m_bRemoveMeanGrv;
	pageGrv.m_bMeanTns = m_bRemoveMeanTns;

	pageMag.m_dIncl = m_vMagIndFld.x;
	pageMag.m_dDecl = m_vMagIndFld.y;
	pageMag.m_dTot  = m_vMagIndFld.z;
	pageMag.m_dUnits = m_dMagUnits;
	pageMag.m_bMeanMag = m_bRemoveMeanMag;

	sheet.SetTitle("Computation Properties");
	if( sheet.DoModal() == IDOK ) {
		BOOL bRecomp=FALSE;
		bRecomp |= SetChange(m_bSherComp, pageCmp.m_bSpherComp);
		bRecomp |= SetChange(m_nComputationType, pageCmp.m_nComputationType);

		bRecomp |= SetChange(m_dGrvSens, pageGrv.m_dGravSens);
		bRecomp |= SetChange(m_dGrvDensRef, pageGrv.m_dDensRef);
		bRecomp |= SetChange(m_vGrvDensGrad.x, pageGrv.m_dDGx);
		bRecomp |= SetChange(m_vGrvDensGrad.y, pageGrv.m_dDGy);
		bRecomp |= SetChange(m_vGrvDensGrad.z, pageGrv.m_dDGz);
		bRecomp |= SetChange(m_vGrvDensOrigo.x, pageGrv.m_dDOx);
		bRecomp |= SetChange(m_vGrvDensOrigo.y, pageGrv.m_dDOy);
		bRecomp |= SetChange(m_vGrvDensOrigo.z, pageGrv.m_dDOz);
		bRecomp |= SetChange(m_dGrvUnits, pageGrv.m_dUnits);
		bRecomp |= SetChange(m_gvrTens.dUnits, pageGrv.m_dGradUnits);
		bRecomp |= SetChange(m_bRemoveMeanGrv, pageGrv.m_bMeanGrv);
		bRecomp |= SetChange(m_bRemoveMeanTns, pageGrv.m_bMeanTns);

		bRecomp |= SetChange(m_dMagUnits, pageMag.m_dUnits);
		bRecomp |= SetChange(m_vMagIndFld.x, pageMag.m_dIncl);
		bRecomp |= SetChange(m_vMagIndFld.y, pageMag.m_dDecl);
		bRecomp |= SetChange(m_vMagIndFld.z, pageMag.m_dTot);
		bRecomp |= SetChange(m_bRemoveMeanMag, pageMag.m_bMeanMag);
		
		if(bRecomp) {
			if(AfxMessageBox("Model recomputation necessary. Recompute now?", MB_YESNO | MB_ICONQUESTION) == IDYES )
				OnComputeCompute();
			else
				AfxMessageBox("Recomputation is necssary.", MB_OK | MB_ICONINFORMATION);
		}
	}

}

void CMod3DDoc::ComputationCanceled(void)
{
	m_pCompThread = NULL;
}

void CMod3DDoc::ComputationFinished(void)
{
	ComputeTotalField();
	ComputeDifferenceField();
	SetModifiedFlagModFld();
	UpdateMinMaxFldVal();
	if( m_bRemoveMeanGrv || m_bRemoveMeanTns || m_bRemoveMeanMag ) {
		RemoveMeanDif();
		SetModifiedFlagModFld();
		UpdateMinMaxFldVal();
	}
	SetModifiedFlag();
	m_pCompThread = NULL;
	m_pThreadFit = NULL;
	UpdateAllViews( NULL, UPDATE_COMPUTATION_FINISHED );
}
//************* end compute commands ****************************************
/////////////////////////////////////////////////////////////////////////////


void CMod3DDoc::OnBodyEdit() 
{
	EditBodies(NULL);	
	SetModifiedFlag();
}

void CMod3DDoc::EditBodies(CBody *pSelBody)
{
	CDlgEditBodies dlg;

	dlg.m_pBodyArray = m_model.GetBodies();
	dlg.m_pModel = GetModel();
	dlg.m_pBody = pSelBody;
	dlg.m_pDoc = this;

	if( dlg.DoModal() == IDOK ) {
	}
	SetModifiedFlag();

}

void CMod3DDoc::OnViewEditPicture()
{
	CDlgObjectManager dlg;

	dlg.m_pObPtrArray = &m_objPtrArray;
	dlg.DoModal();
	//UpdateMod3DViews();
}

void CMod3DDoc::OnImportBitmap()
{
	CImgReferenced *pImg;
	CString	strId;


	pImg = new CImgReferenced();
	pImg->Properties();
	// not succesfull or canceled
	if( pImg->m_img.IsNull() ) {
		delete pImg;
		return;
	}
	// add image to objects array
	//pImg->Flip();
	strId.Format("Bitmap %d", m_nID++);
	pImg->SetID(strId);
	AddObject( pImg );
	//m_objPtrArray.Add(pImg);
	SetModifiedFlag();
	UpdateAllViews(NULL, UPDATE_BITMAP_NEW);
	//UpdateAllViews(NULL, UPDATE_PROPERTIES_REDRAW);
}

void CMod3DDoc::OnUpdateImportBitmap(CCmdUI *pCmdUI)
{
	// TODO: Add your command update UI handler code here
}

CGuideMark* CMod3DDoc::NewGuideMark(void)
{
	CGuideMark	*pChm;

	pChm = new CGuideMark( CreateID( "Check Mark" ) );
	//m_objPtrArray.Add( pChm );
	AddObject( pChm );
	SetModifiedFlag();
	return pChm;
}

CWell* CMod3DDoc::NewWell(void)
{
	CWell	*pWell;
	CString	strID;

	pWell = new CWell();
	pWell->m_dRadiusProf = max(m_obsHdr.xSize, m_obsHdr.ySize)*2;
	pWell->m_dRadiusLog = max(m_obsHdr.xSize, m_obsHdr.ySize)*2;
	pWell->m_dRadiusLog3D = max(m_obsHdr.xSize, m_obsHdr.ySize)*2;
	strID = CreateID("Well");
	pWell->SetID( strID );
	AddObject( pWell );
	SetModifiedFlag();
	return pWell;
}

CData3D* CMod3DDoc::NewData3D(void)
{
	CData3D	*pObj;
	CString	strID;

	pObj = new CData3D();
	strID = CreateID("Data3D");
	pObj->SetID( strID );
	pObj->SetProfDis(max(m_obsHdr.xSize, m_obsHdr.ySize));
	pObj->SetDrawRadius(min(m_obsHdr.xSize, m_obsHdr.ySize)/20);
	AddObject( pObj );
	SetModifiedFlag();
	return pObj;
}

void CMod3DDoc::CreateCheckMark()
{
	CGuideMark	*pChm;

	pChm = new CGuideMark( CreateID( "Check Mark" ) );

	pChm->m_nCol = m_nColCur;
	pChm->m_nRow = m_nRowCur;
	pChm->m_x = m_x;
	pChm->m_y = m_y;
	pChm->m_z = m_z;
	if( pChm->Properties() == IDOK ) 
	{
		//m_objPtrArray.Add( pChm );
		AddObject( pChm );
		SetModifiedFlag();
		return;
	}
	//
	delete pChm;
}

CString CMod3DDoc::CreateID(CString str)
{
	CString strA;
	strA.Format(" %d", m_nID);
	str += strA;
	m_nID++;
	return str;
}

int CMod3DDoc::InitializeProfileCheckMarks(int profType, double xy)
{
	for(int i=0; i<m_objPtrArray.GetSize(); i++) {
		CMyObject* pObj = m_objPtrArray.GetAt(i);
		if( pObj->IsKindOf( RUNTIME_CLASS( CGuideMark ) )  ) {
			CGuideMark* pChm = (CGuideMark*) pObj;
			pChm->FindIntersections( profType, xy );
		}
	}
	return 0;
}


int CMod3DDoc::SetIndicators(int row, int col, int bElev)
{
	CGrid*	pGrd;

	pGrd = GetGrid( m_nIndicatorFld );

	g_row = row;
	g_col = col;
	double	vz = m_ObsRel.GetValue(row, col);;
	if( bElev && vz != DUMMY)	
		g_z = vz;
	if( pGrd ) {
		g_nIndFld = m_nIndicatorFld;
		g_strFld = GetFieldString( m_nIndicatorFld );
		double v = pGrd->GetValue(row, col);
		if( v!=DUMMY )
			g_dFld = pGrd->GetValue(row, col);
	}
	return 0;
}

void CMod3DDoc::OnViewSetfieldindicator()
{
	CDlgModelFldIndicator dlg;
	
	dlg.m_nFld = m_nIndicatorFld;
	if( dlg.DoModal() == IDOK ) {
		m_nIndicatorFld = dlg.m_nFld;
	}
}

void CMod3DDoc::OnUpdateViewSetfieldindicator(CCmdUI *pCmdUI)
{
	if( IsEmpty() ) pCmdUI->Enable( 0 );
	else			pCmdUI->Enable( 1 );
}


CString CMod3DDoc::GetFieldString(int n)
{
	CString str;

	switch(n) {
	case GRDMOD_GX:		str = "Gx";		break;
	case GRDMOD_GY:		str = "Gy";		break;
	case GRDMOD_GZ:		str = "Gz";		break;
	case GRDMOD_G:		str = "Gt";		break;
	case GRDMOD_MX:		str = "Mx";		break;
	case GRDMOD_MY:		str = "My";		break;
	case GRDMOD_MZ:		str = "Mz";		break;
	case GRDMOD_M:		str = "Mt";		break;
	case GRDMOD_GXX:	str = "Gxx";	break;
	case GRDMOD_GXY:	str = "Gxy";	break;
	case GRDMOD_GXZ:	str = "Gxz";	break;
	case GRDMOD_GYY:	str = "Gyy";	break;
	case GRDMOD_GYZ:	str = "Gyz";	break;
	case GRDMOD_GZZ:	str = "Gzz";	break;

	case GRDMES_GX:		str = "mGx";	break;
	case GRDMES_GY:		str = "mGy";	break;
	case GRDMES_GZ:		str = "mGz";	break;
	case GRDMES_G:		str = "mGt";		break;
	case GRDMES_MX:		str = "mMx";	break;
	case GRDMES_MY:		str = "mMy";	break;
	case GRDMES_MZ:		str = "mMz";	break;
	case GRDMES_M:		str = "mMt";		break;
	case GRDMES_GXX:	str = "mGxx";	break;
	case GRDMES_GXY:	str = "mGxy";	break;
	case GRDMES_GXZ:	str = "mGxz";	break;
	case GRDMES_GYY:	str = "mGyy";	break;
	case GRDMES_GYZ:	str = "mGyz";	break;
	case GRDMES_GZZ:	str = "mGzz";	break;

	case GRDDIF_GX:		str = "dGx";	break;
	case GRDDIF_GY:		str = "dGy";	break;
	case GRDDIF_GZ:		str = "dGz";	break;
	case GRDDIF_G:		str = "dGt";		break;
	case GRDDIF_MX:		str = "dMx";	break;
	case GRDDIF_MY:		str = "dMy";	break;
	case GRDDIF_MZ:		str = "dMz";	break;
	case GRDDIF_M:		str = "dMt";		break;
	case GRDDIF_GXX:	str = "dGxx";	break;
	case GRDDIF_GXY:	str = "dGxy";	break;
	case GRDDIF_GXZ:	str = "dGxz";	break;
	case GRDDIF_GYY:	str = "dGyy";	break;
	case GRDDIF_GYZ:	str = "dGyz";	break;
	case GRDDIF_GZZ:	str = "dGzz";	break;
	}

	return str;
}

void CMod3DDoc::OnComputeActiveField()
{
	CDlgModelActiveGrd dlg;

	// modeled
	dlg.m_bG	= m_grdActiveCompute[ GRDMOD_G ];		if( m_grd[GRDMOD_G].IsEmpty() )		dlg.m_bG = -1;
	dlg.m_bGx	= m_grdActiveCompute[ GRDMOD_GX ];		if( m_grd[GRDMOD_GX].IsEmpty() )	dlg.m_bGx = -1;
	dlg.m_bGy	= m_grdActiveCompute[ GRDMOD_GY ];		if( m_grd[GRDMOD_GY].IsEmpty() )	dlg.m_bGy = -1;
	dlg.m_bGz	= m_grdActiveCompute[ GRDMOD_GZ ];		if( m_grd[GRDMOD_GZ].IsEmpty() )	dlg.m_bGz = -1;
	dlg.m_bGxx	= m_grdActiveCompute[ GRDMOD_GXX ];		if( m_grd[GRDMOD_GXX].IsEmpty() )	dlg.m_bGxx = -1;
	dlg.m_bGyy	= m_grdActiveCompute[ GRDMOD_GYY ];		if( m_grd[GRDMOD_GYY].IsEmpty() )	dlg.m_bGyy = -1;
	dlg.m_bGzz	= m_grdActiveCompute[ GRDMOD_GZZ ];		if( m_grd[GRDMOD_GZZ].IsEmpty() )	dlg.m_bGzz = -1;
	dlg.m_bGxy	= m_grdActiveCompute[ GRDMOD_GXY ];		if( m_grd[GRDMOD_GXY].IsEmpty() )	dlg.m_bGxy = -1;
	dlg.m_bGxz	= m_grdActiveCompute[ GRDMOD_GXZ ];		if( m_grd[GRDMOD_GXZ].IsEmpty() )	dlg.m_bGxz = -1;
	dlg.m_bGyz	= m_grdActiveCompute[ GRDMOD_GYZ ];		if( m_grd[GRDMOD_GYZ].IsEmpty() )	dlg.m_bGxy = -1;
	dlg.m_bM	= m_grdActiveCompute[ GRDMOD_M ];		if( m_grd[GRDMOD_M].IsEmpty() )		dlg.m_bM = -1;
	dlg.m_bMx	= m_grdActiveCompute[ GRDMOD_MX ];		if( m_grd[GRDMOD_MX].IsEmpty() )	dlg.m_bMx = -1;
	dlg.m_bMy	= m_grdActiveCompute[ GRDMOD_MY ];		if( m_grd[GRDMOD_MY].IsEmpty() )	dlg.m_bMy = -1;
	dlg.m_bMz	= m_grdActiveCompute[ GRDMOD_MZ ];		if( m_grd[GRDMOD_MZ].IsEmpty() )	dlg.m_bMz = -1;
	dlg.m_bMxx	= m_grdActiveCompute[ GRDMOD_MXX ];		if( m_grd[GRDMOD_MXX].IsEmpty() )	dlg.m_bMxx = -1;
	dlg.m_bMyy	= m_grdActiveCompute[ GRDMOD_MYY ];		if( m_grd[GRDMOD_MYY].IsEmpty() )	dlg.m_bMyy = -1;
	dlg.m_bMzz	= m_grdActiveCompute[ GRDMOD_MZZ ];		if( m_grd[GRDMOD_MZZ].IsEmpty() )	dlg.m_bMzz = -1;
	dlg.m_bMxy	= m_grdActiveCompute[ GRDMOD_MXY ];		if( m_grd[GRDMOD_MXY].IsEmpty() )	dlg.m_bMxy = -1;
	dlg.m_bMxz	= m_grdActiveCompute[ GRDMOD_MXZ ];		if( m_grd[GRDMOD_MXZ].IsEmpty() )	dlg.m_bMxz = -1;
	dlg.m_bMyz	= m_grdActiveCompute[ GRDMOD_MYZ ];		if( m_grd[GRDMOD_MYZ].IsEmpty() )	dlg.m_bMyz = -1;

	// difference
	dlg.m_bdG	= m_grdActiveCompute[ GRDDIF_G ];		if( m_grd[GRDDIF_G].IsEmpty() )		dlg.m_bdG = -1;
	dlg.m_bdGx	= m_grdActiveCompute[ GRDDIF_GX ];		if( m_grd[GRDDIF_GX].IsEmpty() )	dlg.m_bdGx = -1;
	dlg.m_bdGy	= m_grdActiveCompute[ GRDDIF_GY ];		if( m_grd[GRDDIF_GY].IsEmpty() )	dlg.m_bdGy = -1;
	dlg.m_bdGz	= m_grdActiveCompute[ GRDDIF_GZ ];		if( m_grd[GRDDIF_GZ].IsEmpty() )	dlg.m_bdGz = -1;
	dlg.m_bdGxx = m_grdActiveCompute[ GRDDIF_GXX ];		if( m_grd[GRDDIF_GXX].IsEmpty() )	dlg.m_bdGxx = -1;
	dlg.m_bdGyy = m_grdActiveCompute[ GRDDIF_GYY ];		if( m_grd[GRDDIF_GYY].IsEmpty() )	dlg.m_bdGyy = -1;
	dlg.m_bdGzz = m_grdActiveCompute[ GRDDIF_GZZ ];		if( m_grd[GRDDIF_GZZ].IsEmpty() )	dlg.m_bdGzz = -1;
	dlg.m_bdGxy = m_grdActiveCompute[ GRDDIF_GXY ];		if( m_grd[GRDDIF_GXY].IsEmpty() )	dlg.m_bdGxy = -1;
	dlg.m_bdGxz = m_grdActiveCompute[ GRDDIF_GXZ ];		if( m_grd[GRDDIF_GXZ].IsEmpty() )	dlg.m_bdGxz = -1;
	dlg.m_bdGyz = m_grdActiveCompute[ GRDDIF_GYZ ];		if( m_grd[GRDDIF_GYZ].IsEmpty() )	dlg.m_bdGyz = -1;
	dlg.m_bdM	= m_grdActiveCompute[ GRDDIF_M ];		if( m_grd[GRDDIF_M].IsEmpty() )		dlg.m_bdM = -1;
	dlg.m_bdMx	= m_grdActiveCompute[ GRDDIF_MX ];		if( m_grd[GRDDIF_MX].IsEmpty() )	dlg.m_bdMx = -1;
	dlg.m_bdMy	= m_grdActiveCompute[ GRDDIF_MY ];		if( m_grd[GRDDIF_MY].IsEmpty() )	dlg.m_bdMy = -1;
	dlg.m_bdMz	= m_grdActiveCompute[ GRDDIF_MZ ];		if( m_grd[GRDDIF_MZ].IsEmpty() )	dlg.m_bdMz = -1;
	dlg.m_bdMxx = m_grdActiveCompute[ GRDDIF_MXX ];		if( m_grd[GRDDIF_MXX].IsEmpty() )	dlg.m_bdMxx = -1;
	dlg.m_bdMyy = m_grdActiveCompute[ GRDDIF_MYY ];		if( m_grd[GRDDIF_MYY].IsEmpty() )	dlg.m_bdMyy = -1;
	dlg.m_bdMzz = m_grdActiveCompute[ GRDDIF_MZZ ];		if( m_grd[GRDDIF_MZZ].IsEmpty() )	dlg.m_bdMzz = -1;
	dlg.m_bdMxy = m_grdActiveCompute[ GRDDIF_MXY ];		if( m_grd[GRDDIF_MXY].IsEmpty() )	dlg.m_bdMxy = -1;
	dlg.m_bdMxz = m_grdActiveCompute[ GRDDIF_MXZ ];		if( m_grd[GRDDIF_MXZ].IsEmpty() )	dlg.m_bdMxz = -1;
	dlg.m_bdMyz = m_grdActiveCompute[ GRDDIF_MYZ ];		if( m_grd[GRDDIF_MYZ].IsEmpty() )	dlg.m_bdMyz = -1;

	// measured
	dlg.m_bmG	= m_grdActiveCompute[ GRDMES_G ];		if( m_grd[GRDMES_G].IsEmpty() )		dlg.m_bmG = -1;
	dlg.m_bmGx	= m_grdActiveCompute[ GRDMES_GX ];		if( m_grd[GRDMES_GX].IsEmpty() )	dlg.m_bmGx = -1;
	dlg.m_bmGy	= m_grdActiveCompute[ GRDMES_GY ];		if( m_grd[GRDMES_GY].IsEmpty() )	dlg.m_bmGy = -1;
	dlg.m_bmGz	= m_grdActiveCompute[ GRDMES_GZ ];		if( m_grd[GRDMES_GZ].IsEmpty() )	dlg.m_bmGz = -1;
	dlg.m_bmGxx = m_grdActiveCompute[ GRDMES_GXX ];		if( m_grd[GRDMES_GXX].IsEmpty() )	dlg.m_bmGxx = -1;
	dlg.m_bmGyy = m_grdActiveCompute[ GRDMES_GYY ];		if( m_grd[GRDMES_GYY].IsEmpty() )	dlg.m_bmGyy = -1;
	dlg.m_bmGzz = m_grdActiveCompute[ GRDMES_GZZ ];		if( m_grd[GRDMES_GZZ].IsEmpty() )	dlg.m_bmGzz = -1;
	dlg.m_bmGxy = m_grdActiveCompute[ GRDMES_GXY ];		if( m_grd[GRDMES_GXY].IsEmpty() )	dlg.m_bmGxy = -1;
	dlg.m_bmGxz = m_grdActiveCompute[ GRDMES_GXZ ];		if( m_grd[GRDMES_GXZ].IsEmpty() )	dlg.m_bmGxz = -1;
	dlg.m_bmGyz = m_grdActiveCompute[ GRDMES_GYZ ];		if( m_grd[GRDMES_GYZ].IsEmpty() )	dlg.m_bmGyz = -1;
	dlg.m_bmM	= m_grdActiveCompute[ GRDMES_M ];		if( m_grd[GRDMES_M].IsEmpty() )		dlg.m_bmM = -1;
	dlg.m_bmMx	= m_grdActiveCompute[ GRDMES_MX ];		if( m_grd[GRDMES_MX].IsEmpty() )	dlg.m_bmMx = -1;
	dlg.m_bmMy	= m_grdActiveCompute[ GRDMES_MY ];		if( m_grd[GRDMES_MY].IsEmpty() )	dlg.m_bmMy = -1;
	dlg.m_bmMz	= m_grdActiveCompute[ GRDMES_MZ ];		if( m_grd[GRDMES_MZ].IsEmpty() )	dlg.m_bmMz = -1;
	dlg.m_bmMxx = m_grdActiveCompute[ GRDMES_MXX ];		if( m_grd[GRDMES_MXX].IsEmpty() )	dlg.m_bmMxx = -1;
	dlg.m_bmMyy = m_grdActiveCompute[ GRDMES_MYY ];		if( m_grd[GRDMES_MYY].IsEmpty() )	dlg.m_bmMyy = -1;
	dlg.m_bmMzz = m_grdActiveCompute[ GRDMES_MZZ ];		if( m_grd[GRDMES_MZZ].IsEmpty() )	dlg.m_bmMzz = -1;
	dlg.m_bmMxy = m_grdActiveCompute[ GRDMES_MXY ];		if( m_grd[GRDMES_MXY].IsEmpty() )	dlg.m_bmMxy = -1;
	dlg.m_bmMxz = m_grdActiveCompute[ GRDMES_MXZ ];		if( m_grd[GRDMES_MXZ].IsEmpty() )	dlg.m_bmMxz = -1;
	dlg.m_bmMyz = m_grdActiveCompute[ GRDMES_MYZ ];		if( m_grd[GRDMES_MYZ].IsEmpty() )	dlg.m_bmMyz = -1;

	if( dlg.DoModal() == IDOK ) {
		// modeled
		m_grdActiveCompute[ GRDMOD_G ] =	 dlg.m_bG;
		m_grdActiveCompute[ GRDMOD_GX ] =    dlg.m_bGx;
		m_grdActiveCompute[ GRDMOD_GY ] =    dlg.m_bGy;
		m_grdActiveCompute[ GRDMOD_GZ ] =    dlg.m_bGz;
		m_grdActiveCompute[ GRDMOD_GXX ] =   dlg.m_bGxx;
		m_grdActiveCompute[ GRDMOD_GYY ] =   dlg.m_bGyy;
		m_grdActiveCompute[ GRDMOD_GZZ ] =   dlg.m_bGzz;
		m_grdActiveCompute[ GRDMOD_GXY ] =   dlg.m_bGxy;
		m_grdActiveCompute[ GRDMOD_GXZ ] =   dlg.m_bGxz;
		m_grdActiveCompute[ GRDMOD_GYZ ] =   dlg.m_bGyz;
		m_grdActiveCompute[ GRDMOD_M ] =	 dlg.m_bM;
		m_grdActiveCompute[ GRDMOD_MX ] =    dlg.m_bMx;
		m_grdActiveCompute[ GRDMOD_MY ] =    dlg.m_bMy;
		m_grdActiveCompute[ GRDMOD_MZ ] =    dlg.m_bMz;
		m_grdActiveCompute[ GRDMOD_MXX ] =   dlg.m_bMxx;
		m_grdActiveCompute[ GRDMOD_MYY ] =   dlg.m_bMyy;
		m_grdActiveCompute[ GRDMOD_MZZ ] =   dlg.m_bMzz;
		m_grdActiveCompute[ GRDMOD_MXY ] =   dlg.m_bMxy;
		m_grdActiveCompute[ GRDMOD_MXZ ] =   dlg.m_bMxz;
		m_grdActiveCompute[ GRDMOD_MYZ ] =   dlg.m_bMyz;

		// differenc
		m_grdActiveCompute[ GRDDIF_G ] =    dlg.m_bdG;
		m_grdActiveCompute[ GRDDIF_GX ] =   dlg.m_bdGx;
		m_grdActiveCompute[ GRDDIF_GY ] =   dlg.m_bdGy;
		m_grdActiveCompute[ GRDDIF_GZ ] =   dlg.m_bdGz;
		m_grdActiveCompute[ GRDDIF_GXX ] =	dlg.m_bdGxx; 
		m_grdActiveCompute[ GRDDIF_GYY ] =	dlg.m_bdGyy;
		m_grdActiveCompute[ GRDDIF_GZZ ] =	dlg.m_bdGzz;
		m_grdActiveCompute[ GRDDIF_GXY ] =	dlg.m_bdGxy;
		m_grdActiveCompute[ GRDDIF_GXZ ] =	dlg.m_bdGxz;
		m_grdActiveCompute[ GRDDIF_GYZ ] =	dlg.m_bdGyz;
		m_grdActiveCompute[ GRDDIF_M ] =    dlg.m_bdM;
		m_grdActiveCompute[ GRDDIF_MX ] =   dlg.m_bdMx;
		m_grdActiveCompute[ GRDDIF_MY ] =   dlg.m_bdMy;
		m_grdActiveCompute[ GRDDIF_MZ ] =   dlg.m_bdMz;
		m_grdActiveCompute[ GRDDIF_MXX ] =	dlg.m_bdMxx; 
		m_grdActiveCompute[ GRDDIF_MYY ] =	dlg.m_bdMyy;
		m_grdActiveCompute[ GRDDIF_MZZ ] =	dlg.m_bdMzz;
		m_grdActiveCompute[ GRDDIF_MXY ] =	dlg.m_bdMxy;
		m_grdActiveCompute[ GRDDIF_MXZ ] =	dlg.m_bdMxz;
		m_grdActiveCompute[ GRDDIF_MYZ ] =	dlg.m_bdMyz;

						      // measured
		m_grdActiveCompute[ GRDMES_G ] =    dlg.m_bmG;
		m_grdActiveCompute[ GRDMES_GX ] =   dlg.m_bmGx;
		m_grdActiveCompute[ GRDMES_GY ] =   dlg.m_bmGy;
		m_grdActiveCompute[ GRDMES_GZ ] =   dlg.m_bmGz;
		m_grdActiveCompute[ GRDMES_GXX ] =	dlg.m_bmGxx; 
		m_grdActiveCompute[ GRDMES_GYY ] =	dlg.m_bmGyy;
		m_grdActiveCompute[ GRDMES_GZZ ] =	dlg.m_bmGzz;
		m_grdActiveCompute[ GRDMES_GXY ] =	dlg.m_bmGxy;
		m_grdActiveCompute[ GRDMES_GXZ ] =	dlg.m_bmGxz;
		m_grdActiveCompute[ GRDMES_GYZ ] =	dlg.m_bmGyz;
		m_grdActiveCompute[ GRDMES_M ] =    dlg.m_bmM;
		m_grdActiveCompute[ GRDMES_MX ] =   dlg.m_bmMx;
		m_grdActiveCompute[ GRDMES_MY ] =   dlg.m_bmMy;
		m_grdActiveCompute[ GRDMES_MZ ] =   dlg.m_bmMz;
		m_grdActiveCompute[ GRDMES_MXX ] =	dlg.m_bmMxx; 
		m_grdActiveCompute[ GRDMES_MYY ] =	dlg.m_bmMyy;
		m_grdActiveCompute[ GRDMES_MZZ ] =	dlg.m_bmMzz;
		m_grdActiveCompute[ GRDMES_MXY ] =	dlg.m_bmMxy;
		m_grdActiveCompute[ GRDMES_MXZ ] =	dlg.m_bmMxz;
		m_grdActiveCompute[ GRDMES_MYZ ] =	dlg.m_bmMyz;
	}
}


void CMod3DDoc::OnComputeRealTime()
{
	if( m_nComputationType == CMP_REALTIME ){
		m_nComputationType = CMP_NONE;
		m_model.SetComputeRealTime(FALSE);
		return;
	}
	if( m_nComputationType == CMP_NONE ) {
		m_nComputationType = CMP_REALTIME;
		m_model.SetComputeRealTime(TRUE);
		int nCompute = AfxMessageBox("Changing to real-time computation mode may need recomputing (recommended). Compute?", MB_YESNO | MB_ICONINFORMATION);
		if( nCompute == IDYES ) {
			OnComputeCompute();
		}
		return;
	}
	SetModifiedFlag();
}

void CMod3DDoc::OnUpdateComputeRealTime(CCmdUI *pCmdUI)
{
	if( m_nComputationType == CMP_REALTIME )	pCmdUI->SetCheck(1);
	else										pCmdUI->SetCheck(0);
}

void CMod3DDoc::OnModelProperties()
{
	m_model.Properties();
	SetModifiedFlag();
}


void CMod3DDoc::OnEditGuidelines()
{
	CDlgEditCheckMarks dlg;

	dlg.m_pObPtrArray = &m_objPtrArray;
	dlg.DoModal();
	SetModifiedFlag();
	UpdateAllViews(NULL);
}

void CMod3DDoc::OnUpdateEditGuidelines(CCmdUI *pCmdUI)
{
	CMyObject* pObj;
	BOOL	bEnable=FALSE;

	for( int i=0; i<m_objPtrArray.GetSize(); i++) {
		pObj = m_objPtrArray.GetAt(i);
		if( pObj->IsKindOf( RUNTIME_CLASS( CGuideMark ) )  ) {
			bEnable = TRUE;
			break;
		}
	}

	if( bEnable )	pCmdUI->Enable(TRUE);
	else			pCmdUI->Enable(FALSE);
}

//void CMod3DDoc::OnEditObjectManager()
//{
//	CDlgObjectManager dlg;
//
//	dlg.m_pObPtrArray = &m_objPtrArray;
//	dlg.m_pDoc = this;
//	dlg.DoModal();
//	SetModifiedFlag();
//	UpdateAllViews(NULL);
//}
//
//void CMod3DDoc::OnUpdateEditObjectManager(CCmdUI *pCmdUI)
//{
//	BOOL	bEnable=FALSE;
//
//	if( !m_objPtrArray.IsEmpty() ) 
//		bEnable = TRUE;
//
//	if( bEnable )	pCmdUI->Enable(TRUE);
//	else			pCmdUI->Enable(FALSE);
//}

void CMod3DDoc::OnImportBody()
{
	if( m_pCompThread != NULL ) {
		AfxMessageBox("Computation is running. Canceling.", MB_OK | MB_ICONINFORMATION);
		return;
	}

	if( m_model.ImportBody()==1 && m_pCompThread==NULL ) {
		if(  AfxMessageBox("Recompute model now?", MB_YESNO | MB_ICONQUESTION) == IDYES ) {
			OnComputeCompute();
		}
	}

	AfxGetMainWnd()->SendMessage(WM_UPDATE_OBJMNG);
}

void CMod3DDoc::OnUpdateImportBody(CCmdUI *pCmdUI)
{
}

void CMod3DDoc::OnImportMeasuredField()
{
	CDlgImpExpFld dlg;
	
	GetFldStringArray(dlg.m_strExpAr);
	GetFldStringArray(dlg.m_strImpAr, GRDMES_GX, GRDMES_M+1, TRUE, TRUE);

	dlg.m_nType = CDlgImpExpFld::dlgImport;
	dlg.m_pDoc = this;
	dlg.m_fldGrd = m_grd;						// poiner to CGrid array
	dlg.m_obsHdr = GetObservationHeader();		// observation header
	dlg.m_grdActive = m_grdActiveCompute;		// pointer to int array
	dlg.DoModal();
	SetModifiedFlag();
	AfxGetMainWnd()->SendMessage(WM_UPDATE_OBJMNG);
}

void CMod3DDoc::OnUpdateImportMeasuredField(CCmdUI *pCmdUI)
{
	if( m_model.IsEmpty() )		pCmdUI->Enable(FALSE);
	else						pCmdUI->Enable(TRUE);
}

void CMod3DDoc::OnImportGuideLine()
{
	CFileDialog dlg(TRUE, "*.dat", "imp.dat", OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "ASCII data file (*.dat)|*.dat||");
	if( dlg.DoModal() != IDOK ) return;

	CStdioFile f;
	if( !f.Open( dlg.GetPathName(), CFile::modeRead | CFile::typeText ) ) {
		#ifdef _DEBUG
			afxDump << "Unable to open file" << "\n";
		#endif
		return;
	};
    
	float	x, y, z;
	CString	strLine;
	int		nIntems;
	CGuideMark*	pGM=NULL;;
	while( f.ReadString(strLine) ) {
		if(strLine.Find("line") != -1 ) {
			 pGM = NewGuideMark();
			 strLine.Right(strLine.GetLength()-5);
			 pGM->SetType(CHM_LINE);
			 pGM->SetName( strLine.Right(strLine.GetLength()-5) );
		}
		else {
			nIntems = sscanf( strLine, "%f %f %f", &x, &y, &z );
			if( (nIntems>2) && (pGM != NULL) ) {
				pGM->m_pt3DArray.Add( CPoint3D(x, y, z) );
			}
		}
	}
	SetModifiedFlag();
	UpdateAllViews(NULL);
	AfxGetMainWnd()->SendMessage(WM_UPDATE_OBJMNG);
}

void CMod3DDoc::OnUpdateImportGuideLine(CCmdUI *pCmdUI)
{
	if( IsEmpty() ) pCmdUI->Enable(FALSE);
}

void CMod3DDoc::OnExportGuideLine()
{
	CFileDialog dlg(TRUE, "*.dat", "imp.dat", OFN_OVERWRITEPROMPT, "ASCII data file (*.dat)|*.dat||");
	if( dlg.DoModal() != IDOK ) return;

	CStdioFile f;
	if( !f.Open( dlg.GetPathName(), CFile::modeCreate | CFile::modeWrite | CFile::typeText ) ) {
		#ifdef _DEBUG
			afxDump << "Unable to open file" << "\n";
		#endif
		exit( 0 );
	};
    
	//CString	strLine;
	//CGuideMark*	pGM;
	//CPoint3D*	pPt;
	//int	n = (int)pGM->m_pt3DArray.GetSize();
	//for(int i=0; i<n; i++) {
	//	pPt = &pGM->m_pt3DArray.GetAt(i);
	//	strLine.Format("%f %f %f", pPt->x, pPt->y, pPt->z);
	//	f.WriteString( strLine );
	//}
}

void CMod3DDoc::OnUpdateExportGuideLine(CCmdUI *pCmdUI)
{
	CMyObject* pObj;
	BOOL	bEnable=FALSE;

	for( int i=0; i<m_objPtrArray.GetSize(); i++) {
		pObj = m_objPtrArray.GetAt(i);
		if( pObj->IsKindOf( RUNTIME_CLASS( CGuideMark ) )  ) {
			bEnable = TRUE;
			break;
		}
	}

	if( bEnable )	pCmdUI->Enable(TRUE);
	else			pCmdUI->Enable(FALSE);
}


void CMod3DDoc::OnImportGrid()
{
	CString strID;
	strID.Format("Grid Object %d", m_nID++);
	CGridObj* pGO = new CGridObj(strID);
	//m_objPtrArray.Add( pGO );
	pGO->Properties();
	if( pGO->GetGrid()->IsEmpty() ) {
		delete pGO;
		return;
	}
	AddObject( pGO );
	SetModifiedFlag();
	UpdateAllViews(NULL, UPDATE_LEADHRZ_NEW);
}

void CMod3DDoc::OnExportModeledField()
{
	CDlgImpExpFld dlg;
	
	GetFldStringArray(dlg.m_strExpAr);
	GetFldStringArray(dlg.m_strImpAr, GRDMES_GX, GRDMES_M+1, TRUE, TRUE);

	dlg.m_nType = CDlgImpExpFld::dlgExport;
	dlg.m_pDoc = this;
	dlg.m_fldGrd = m_grd;						// poiner to CGrid array
	dlg.m_obsHdr = GetObservationHeader();		// observation header
	dlg.m_grdActive = m_grdActiveCompute;		// pointer to int array
	dlg.DoModal();
	SetModifiedFlag();
	UpdateAllViews(NULL);
}


// returns names of grids which are not empty 
void CMod3DDoc::GetGridsNonEmptyString(CStringArray& strA)
{
	strA.Add("None");
	strA.Add("Relief");
	for(int i=0; i<60; i++) {
		if(!m_grd[i].IsEmpty() && !m_grd[i].IsPlanar()) {
			strA.Add( GetFieldString(i) );
		}
	}

}

// adds to string array requested components 
// bEmpty == TRUE: adds also empty components
// bPlanar == TRUE: adds also planar grids
void CMod3DDoc::GetFldStringArray(CStringArray& strA, int nFrom, int nTo, BOOL bEmpty, BOOL bPlanar)
{
	if( nFrom == -1)	nFrom = 0;
	if( nTo == -1)		nTo = 60;

	for(int i=nFrom; i<nTo; i++) {
		if(!m_grd[i].IsEmpty() || bEmpty ) {
			if( !m_grd[i].IsPlanar() || bPlanar) {
				strA.Add( GetFieldString(i) );
			}
		}
	}

}


CGrid* CMod3DDoc::GetGrid(CString str, int& nFld)
{
	if( str == "Relief" ) return &m_ObsRel;
	nFld = -1;
	for(int i=0; i<60; i++) {
		if( GetFieldString(i) == str ) {
			nFld = i;
			return &m_grd[i];
		}
	}
	return NULL;
}



void CMod3DDoc::BodyPropertiesChanged(int nBodyID, CPropertySheet* pSheet)
{
	CBody*	pBody;
	int		nUpdateTag=0;
	BOOL	bRecomp=FALSE;			
	BOOL	bRedraw=FALSE;
	BOOL	bRedraw2D=FALSE;

	pBody = m_model.GetBody(nBodyID);
	ASSERT( pBody != NULL  && pSheet != NULL);

	if( pBody == NULL || pSheet == NULL)	return;

	CDlgPpBodyComp*	pgComp;
	CDlgPpBodyDraw*	pgDraw;
	CDlgPpBodyDescr* pgDscr;
	CDlgPpBodyGrav*	pgGrav;
	CDlgPpBodyMag*	pgMagn;

	pgGrav = (CDlgPpBodyGrav*) pSheet->GetPage(0);
	pgMagn = (CDlgPpBodyMag*) pSheet->GetPage(1);
	pgDraw = (CDlgPpBodyDraw*) pSheet->GetPage(2);
	pgComp = (CDlgPpBodyComp*) pSheet->GetPage(3);
	pgDscr = (CDlgPpBodyDescr*) pSheet->GetPage(4);

	// color
	bRedraw |= SetChange(pBody->m_lpProf.lopnColor, pgDraw->m_logPen.lopnColor);
	bRedraw |= SetChange(pBody->m_lpProf.lopnStyle, pgDraw->m_logPen.lopnStyle);
	bRedraw |= SetChange(pBody->m_lpProf.lopnWidth.x, pgDraw->m_logPen.lopnWidth.x);
	// brush
	if(pgDraw->m_bInit) {
		COLORREF color = /*pgDraw->m_logBrush.lbColor*/pgDraw->m_butBrush.GetColor();
		bRedraw |= SetChange(pBody->m_lbProf.lbColor, color );
		bRedraw |= SetChange(pBody->m_lbProf.lbHatch, pgDraw->m_logBrush.lbHatch);
		bRedraw |= SetChange(pBody->m_lbProf.lbStyle, pgDraw->m_logBrush.lbStyle);
	}
	// color next profile
	bRedraw |= SetChange(pBody->m_lpProfNext.lopnColor, pgDraw->m_logPenNext.lopnColor);
	bRedraw |= SetChange(pBody->m_lpProfNext.lopnStyle, pgDraw->m_logPenNext.lopnStyle);
	bRedraw |= SetChange(pBody->m_lpProfNext.lopnWidth.x, pgDraw->m_logPenNext.lopnWidth.x);
	// color previous profile
	bRedraw |= SetChange(pBody->m_lpProfPrev.lopnColor, pgDraw->m_logPenPrev.lopnColor);
	bRedraw |= SetChange(pBody->m_lpProfPrev.lopnStyle, pgDraw->m_logPenPrev.lopnStyle);
	bRedraw |= SetChange(pBody->m_lpProfPrev.lopnWidth.x, pgDraw->m_logPenPrev.lopnWidth.x);
	bRedraw2D |= SetChange(pBody->m_bFill, pgDraw->m_bFill		);
	bRedraw |= SetChange(pBody->m_bTransparent, pgDraw->m_bTransparent );
	bRedraw |= SetChange(pBody->m_fAlpha, pgDraw->m_fAlpha		);
	bRedraw |= SetChange(pBody->m_bShow, pgDraw->m_bVisible3D );

	bRedraw |= SetChange(pBody->m_bLocked, pgComp->m_bLocked );
	SetChange(pBody->m_strDescription, pgDscr->m_strDescription );
	SetChange(pBody->m_strName, pgDscr->m_strName );
	
	if( bRedraw2D && !bRedraw ) {
		UpdateAllViews(NULL, UPDATE_PROPERTIES_BODY_REDRAW_2D);
	}
	if( bRedraw ) {
		m_model.UpdateGLFacets();
		UpdateAllViews(NULL, UPDATE_PROPERTIES_BODY_REDRAW);
	}

	if(m_pCompThread == NULL) {
		bRecomp |= SetChange(pBody->m_dDensity, pgGrav->m_dDensity );
		bRecomp |= SetChange(pBody->m_vDensGrad.x, pgGrav->m_dGradX );
		bRecomp |= SetChange(pBody->m_vDensGrad.y, pgGrav->m_dGradY );
		bRecomp |= SetChange(pBody->m_vDensGrad.z, pgGrav->m_dGradZ );
		bRecomp |= SetChange(pBody->m_vDensOrg.x, pgGrav->m_dOrigX );
		bRecomp |= SetChange(pBody->m_vDensOrg.y, pgGrav->m_dOrigY );
		bRecomp |= SetChange(pBody->m_vDensOrg.z, pgGrav->m_dOrigZ );

		bRecomp |= SetChange(pBody->m_dSusc, pgMagn->m_dSusc		);
		double remIncl = -pgMagn->m_dRemIncl;
		bRecomp |= SetChange(pBody->m_vMagRem.x, remIncl); // minus sign
		bRecomp |= SetChange(pBody->m_vMagRem.y, pgMagn->m_dRemDecl	);
		bRecomp |= SetChange(pBody->m_vMagRem.z, pgMagn->m_dRemInt	);

		bRecomp |= SetChange(pBody->m_bActive, pgComp->m_bActive );
	    
		if( bRecomp ) {
			if(AfxMessageBox("Model recomputation necessary. Recompute now?", MB_YESNO | MB_ICONQUESTION) == IDYES )
				OnComputeCompute();
			else
				AfxMessageBox("Recomputation is necessary.", MB_OK | MB_ICONINFORMATION);
		}
	}
	else {
		AfxMessageBox("Cannot update body properties! Computation is currently running.");
	}

	//AfxGetMainWnd()->SendMessage(WM_UPDATE_OBJMNG);
	SetModifiedFlag();
}

void CMod3DDoc::ComputeRelativePaths()
{
	CMyObject *pObj;
	for(int i=0; i<m_objPtrArray.GetSize(); i++) {
		pObj = m_objPtrArray.GetAt( i );
		if( pObj->IsKindOf( RUNTIME_CLASS( CImgReferenced ) ) ) {
			CImgReferenced* pIm = (CImgReferenced*) pObj;
			char szOut[MAX_PATH] = "";
			PathRelativePathTo( szOut,
				GetPathName(),
				FILE_ATTRIBUTE_DIRECTORY,
				pIm->m_strFilePath,
				FILE_ATTRIBUTE_NORMAL);
			pIm->m_strFilePathRelative = _T(szOut);
		}
	}
}


void CMod3DDoc::OnExportBody()
{
	ExportBody();
}

void CMod3DDoc::OnUpdateExportBody(CCmdUI *pCmdUI)
{
	if(m_model.IsBodyArrayEmpty())	pCmdUI->Enable(FALSE);
	else							pCmdUI->Enable(TRUE);
}

void CMod3DDoc::ExportBody(int nID)
{
	m_model.ExportBody(nID);
}

BOOL CMod3DDoc::IsEnabled(int nTag)
{
	switch(nTag) {
		case MODF_MODEL:
			if( m_pCompThread == NULL ) return 1;
			else						return 0;
			break;
	}

	return 0;
}

void CMod3DDoc::OnModelBlankGrid()
{
	CGrid* pGrd;

	pGrd = GetGrid(GRDMOD_GZ);
	pGrd->Blank();
}

void CMod3DDoc::OnModelObservationProperties()
{
	if( !IsEnabled(MODF_MODEL) ) {
		AfxMessageBox("Model modification is disabled!", MB_OK | MB_ICONEXCLAMATION);
		return;
	}

	CDlgObservations dlg;

	dlg.m_pGrdRel = &m_ObsRel;
	dlg.m_pGrdGrv = &m_ObsGrv;
	dlg.m_pGrdMag = &m_ObsMag;
	dlg.m_pGrdTns = &m_ObsTns;

	dlg.m_dSensHeightGrv = m_dGrvSens;
	dlg.m_dSensHeightMag = m_dMagSens;
	dlg.m_dSensHeightTns = m_gvrTens.dHeight;

	dlg.m_dConstElevGrv = m_dGrvElev;
	dlg.m_dConstElevMag= m_dMagElev;
	dlg.m_dConstElevTns = m_gvrTens.dFlightElev;

	dlg.m_nObsTagGrv = m_nGrvObsTag;
	dlg.m_nObsTagMag = m_nGrvObsTag;
	dlg.m_nObsTagTns = m_gvrTens.nTag;

	dlg.m_nFld = 0;

	dlg.DoModal();

	m_dGrvSens = dlg.m_dSensHeightGrv;
	m_dMagSens = dlg.m_dSensHeightMag;
	m_gvrTens.dHeight = dlg.m_dSensHeightTns;

	m_dGrvElev = dlg.m_dConstElevGrv;
	m_dMagElev = dlg.m_dConstElevMag;
	m_gvrTens.dFlightElev = dlg.m_dConstElevTns;

	m_nGrvObsTag = dlg.m_nObsTagGrv;
	m_nGrvObsTag = dlg.m_nObsTagMag;
	m_gvrTens.nTag = dlg.m_nObsTagTns;

	if(dlg.m_bRecompute) {
		if( AfxMessageBox("Model recomputation is necessary. Recompute now?", MB_YESNO | MB_ICONQUESTION) == IDYES )
            OnComputeCompute();
		else
			AfxMessageBox("Model will be updated after recomputation.", MB_OK | MB_ICONINFORMATION);
	}

	SetModifiedFlag();
}


void CMod3DDoc::OnImportWell()
{
	CFileDialog dlg(TRUE, "*.wel", "*.wel", OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "ASCII well file (*.wel)|*.wel||");
	if( dlg.DoModal() != IDOK ) return;

	CStdioFile f;
	if( !f.Open( dlg.GetPathName(), CFile::modeRead | CFile::typeText ) ) {
		#ifdef _DEBUG
			afxDump << "Unable to open file" << "\n";
		#endif
		return;
	};
    
	double d;
	CString	strLine, strToken;
	int		nTokenPos;
	int		tag;
	int		line=0;
	int		n;
	int		k;
	WELLIT	wl;
	BYTE	color;
	CWell*	pWell=NULL;

	while( f.ReadString(strLine) ) {
		if( strLine == "" ) {
			tag = -1;
		}
		// header
		if( strLine.Find("WellBegin", 0) != -1 ) {
			// create new well
			if(pWell!=NULL) pWell->IndexWell();
			pWell = NewWell();
			// coordinates
			f.ReadString(strLine);
			nTokenPos=0;
			strToken = strLine.Tokenize(" ,", nTokenPos);
			pWell->m_x = atof( strToken.GetBuffer() );
			strToken= strLine.Tokenize(" ,", nTokenPos);
			pWell->m_y = atof(strToken.GetBuffer());
			strToken= strLine.Tokenize(" ,", nTokenPos);
			pWell->m_z = atof(strToken.GetBuffer());
			// name
			f.ReadString(pWell->m_strName);
			tag = 0;
			line = 0;
		}

		// lithology
		if( strLine.Find("WellLithology", 0) != -1 ) {
			f.ReadString(strLine);
			nTokenPos = 0;
			tag = 1;
		}

		// data
		if( strLine.Find("WellData", 0) != -1 ) {
			// first line -> format
			f.ReadString(strLine);
			pWell->m_nFormat = atoi(strLine)+1;
			pWell->InitPens();
			// second line -> names
			f.ReadString(strLine);
			nTokenPos = 0;
			strToken = strLine.Tokenize(" ", nTokenPos);
			while( strToken != "" )	{
				pWell->m_strData.Add( strToken.GetBuffer() );
				strToken = strLine.Tokenize(" ", nTokenPos);
			};	
			if( pWell->m_strData.GetSize() < pWell->m_nFormat-1 ) {
				CString str;
				str.Format("%s: Format number doesn't match to channel number!", pWell->GetName());
				AfxMessageBox(str, MB_OK | MB_ICONEXCLAMATION);
				pWell->m_strData.SetSize( pWell->m_nFormat-1 );
			}
			f.ReadString(strLine);
			tag = 2;
		}

		switch(tag) {
			// header
			case 0:
				break;
			// lithology
			case 1:
				nTokenPos = 0;
				strToken = strLine.Tokenize(" ,", nTokenPos);
				wl.nType = atoi(strToken);
				strcpy(wl.strType, (strLine.Tokenize(" ,", nTokenPos)).GetBuffer());
				strToken = strLine.Tokenize(" ", nTokenPos);
				k=0;
				while( strToken != "" && k<3)	{
					color = atoi(strToken);
					switch(k) {
					case 0:
						wl.r = (BYTE) color;
						wl.rd = color / 255.0;
						break;
					case 1:
						wl.g = (BYTE) color;
						wl.gd = color / 255.0;
						break;
					case 2:
						wl.b = (BYTE) color;
						wl.bd = color / 255.0;
						break;
					}
					strToken = strLine.Tokenize(" ", nTokenPos);
					k++;
				};	
				if(k<3) {
					wl.r = 255;
					wl.rd = 1.0;
					wl.g = 0;
					wl.gd = 0.0;
					wl.b = 0;
					wl.bd = 0.0;
				}
				pWell->m_welLit.Add(wl);
				break;
			// data
			case 2:
				nTokenPos = 0;
				strToken = strLine.Tokenize(" ,", nTokenPos);
				n=0;
				while( strToken != "" && n<pWell->m_nFormat)	{
					d = atof(strToken.GetBuffer() );
					pWell->m_data.Add( d );
					n++;
					strToken= strLine.Tokenize(" ,", nTokenPos);
				};	
				while(n < pWell->m_nFormat-1)	{
					pWell->m_data.Add( DUMMY );
					n++;
				}
				pWell->m_data.Add( d );	// index Channel
				break;
		}
	}
	f.Close();
	if(pWell) pWell->IndexWell();

	SetModifiedFlag();
	AfxGetMainWnd()->SendMessage(WM_UPDATE_OBJMNG);
	UpdateAllViews(NULL);
}

void CMod3DDoc::OnEditFieldGrid()
{
	CDlgSelGridProp dlg;

	dlg.m_pDoc = this;

	dlg.DoModal();
}

int CMod3DDoc::AddObject(CMyObject* pObj)
{
	m_objPtrArray.Add( pObj );
	AfxGetMainWnd()->SendMessage(WM_UPDATE_OBJMNG);

	return 1;
}

void CMod3DDoc::OnImport3ddata()
{
	CFileDialog dlg(TRUE, "*.xyz", "*.xyz", OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "ASCII 3D data (*.xyz)|*.xyz||");
	if( dlg.DoModal() != IDOK ) return;

	CStdioFile f;
	CString strFilePath = dlg.GetPathName();
	if( !f.Open( strFilePath, CFile::modeRead | CFile::typeText ) ) {
		#ifdef _DEBUG
			afxDump << "Unable to open file" << "\n";
		#endif
		return;
	};
    
	CString	strLine, strA;
	float v[4];
	DoubleArray da;
	int n=0;

	while( f.ReadString(strLine) ) {
		int nItems = sscanf(_T(strLine),"%f %f %f %f", v, v+1, v+2, v+3);
		if( nItems >= 3 ) {
			if( nItems==3) v[3]=0;
			da.Add( v[0] );
			da.Add( v[1] );
			da.Add( v[2] );
			da.Add( v[3] );
			n++;
		}
	}
	f.Close();

	if( n==0) {
		AfxMessageBox("Invalid file format. Import sipped.", MB_OK | MB_ICONINFORMATION);
		return;
	}

	CData3D* pObj = NewData3D();

	if( pObj == NULL) {
		AfxMessageBox("Object creation failed.", MB_OK | MB_ICONINFORMATION);
		return;
	}
	
	pObj->SetData(da.GetData(), da.GetCount()/4);
	pObj->UpdateMimMax();
	//pObj->SetFilePathA(_T(strFilePath));

	strA.Format("%d items successfully imported.", n);
	AfxMessageBox(strA, MB_OK | MB_ICONINFORMATION);

	AfxGetMainWnd()->SendMessage(WM_UPDATE_OBJMNG);
	UpdateAllViews(NULL);
}

void CMod3DDoc::OnComputeActivateGrid()
{
	CDlgModGrids dlg;
	dlg.m_grd = m_grd;
	dlg.m_nCols = m_obsHdr.nCols;
	dlg.m_nRows = m_obsHdr.nRows;
	dlg.m_x0 = m_obsHdr.x0;
	dlg.m_y0 = m_obsHdr.y0;
	dlg.m_xSize = m_obsHdr.xSize;
	dlg.m_ySize = m_obsHdr.ySize;

	if( !m_grd[GRDMOD_GX].IsEmpty() )	dlg.m_bGx=TRUE;
	if( !m_grd[GRDMOD_GY].IsEmpty() )	dlg.m_bGy=TRUE;
	if( !m_grd[GRDMOD_GZ].IsEmpty() )	dlg.m_bGz=TRUE;
	if( !m_grd[GRDMOD_G].IsEmpty() )	dlg.m_bG=TRUE;

	if( !m_grd[GRDMOD_MX].IsEmpty() )	dlg.m_bMx=TRUE;
	if( !m_grd[GRDMOD_MY].IsEmpty() )	dlg.m_bMy=TRUE;
	if( !m_grd[GRDMOD_MZ].IsEmpty() )	dlg.m_bMz=TRUE;
	if( !m_grd[GRDMOD_M].IsEmpty() )	dlg.m_bM=TRUE;

	if( !m_grd[GRDMOD_GXX].IsEmpty() )	dlg.m_bGxx=TRUE;
	if( !m_grd[GRDMOD_GYY].IsEmpty() )	dlg.m_bGyy=TRUE;
	if( !m_grd[GRDMOD_GZZ].IsEmpty() )	dlg.m_bGzz=TRUE;
	if( !m_grd[GRDMOD_GXY].IsEmpty() )	dlg.m_bGxy=TRUE;
	if( !m_grd[GRDMOD_GXZ].IsEmpty() )	dlg.m_bGxz=TRUE;
	if( !m_grd[GRDMOD_GYZ].IsEmpty() )	dlg.m_bGyz=TRUE;

	dlg.DoModal();

	// set modeled fields active
	for(int i = 0; i < GRDMOD_MXX; i++) {
		if( !m_grd[i].IsEmpty() )
			m_grdActiveCompute[i] = TRUE;
		else
			m_grdActiveCompute[i] = FALSE;
	}

	UpdateAllViews(NULL);
}
