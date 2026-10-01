// Model.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "Model.h"

#include "DlgSelectBody.h"
#include "DlgBodyModf.h"
#include "DlgEditBodies.h"
#include "DlgModelProperties.h"
#include "DlgModelImportBody.h"

// CModel
IMPLEMENT_SERIAL(CModel, CObject, 1);

CModel::CModel()
{
	m_bInitialized = FALSE;

	m_nID = 0;
	m_nBodyID = 0;
	m_bExtend = TRUE;
	m_dExE = 5.0;
	m_dExW = 5.0;
	m_dExN = 5.0;
	m_dExS = 5.0;

	m_strInfo = "...";

	m_nCreationTag = BCR_NONE;			// BodyCreate Enum
	m_bConstTop = FALSE;				// constant top boundary
	m_dConstTop = -400.0;
	m_bConstBot = FALSE;				// constant bottom boundary
	m_dConstBot = -800.0;
	m_dCreationRatio = 1.0;				// body cereation ratio

	m_bComputeRealTime = TRUE;

	m_nImportUB = BDI_GRID;
	m_nImportLB = BDI_GRID;
	m_dImportUB = 0.1;
	m_dImportLB = 0.1;
}

CModel::~CModel()
{
}

// serialization
void CModel::Serialize(CArchive& ar)
{
	TRACE("Entering CModel::Serialize(CArchive& ar) \n");
	if (ar.IsStoring())
	{	// storing code
		ar << m_nID << m_strInfo;
		ar << m_nBodyID;

		ar << m_nRows << m_nCols;
		ar << m_x0 << m_xSize;
		ar << m_y0 << m_ySize;
		ar << m_xMin << m_xMax;
		ar << m_yMin << m_yMax;
		ar << m_zMin << m_zMax;

		ar << m_bExtend;
		ar << m_dExE << m_dExW << m_dExN << m_dExS;

		for( int i=0; i<m_data.GetSize(); i++ ) {
			m_data.GetAt(i).Serialize( ar );
		}
		m_bodies.Serialize(ar);
	}
	else
	{	// loading code
		ar >> m_nID >> m_strInfo;
		ar >> m_nBodyID;

		ar >> m_nRows >> m_nCols;
		ar >> m_x0 >> m_xSize;
		ar >> m_y0 >> m_ySize;
		ar >> m_xMin >> m_xMax;
		ar >> m_yMin >> m_yMax;
		ar >> m_zMin >> m_zMax;

		ar >> m_bExtend;
		ar >> m_dExE >> m_dExW >> m_dExN >> m_dExS;

		m_data.SetSize(m_nRows*m_nCols);
		for( int i=0; i<m_data.GetSize(); i++ ) {
			m_data.GetAt(i).Serialize( ar );
		}
		m_bodies.Serialize( ar);
		UpdateBodyIndex();

		m_fct.SetSize(m_nRows*m_nCols);
		m_glFacets.SetSize(m_nRows*m_nCols);
		m_glSelPt.SetSize(m_nRows*m_nCols);
		m_bodyBound.SetSize(m_nRows*m_nCols);
		
		BOOL bComputeRealTime;
		bComputeRealTime = m_bComputeRealTime;
		m_bComputeRealTime = FALSE;
		InitFacetList();
		m_bComputeRealTime = bComputeRealTime;
		m_fctLstUpdate.RemoveAll();

		m_bInitialized = TRUE;
	}
}

// initialization
BOOL CModel::Init( int nRows, int nCols, double x0, double y0, double xSize, double ySize, double zMin, double zMax)
{
	m_nRows = nRows+2;
	m_nCols = nCols+2;
	m_x0 = x0;
	m_y0 = y0;
	m_xSize  = xSize;
	m_ySize = ySize;
	m_xMin = m_x0;		
	m_xMax = m_x0 + (m_nCols-3)*m_xSize;
	m_yMin = m_y0;		
	m_yMax = m_y0 + (m_nRows-3)*m_ySize;
	m_zMin = zMin;		
	m_zMax = zMax;

	int nSizeMod = m_nRows*m_nCols;

#ifndef MOD3D_FULL
	m_nRows = min(m_nRows, 64);
	m_nCols = min(m_nCols, 64);
	nSizeMod = m_nRows*m_nCols;
	nSizeMod = min(nSizeMod, 576);
#endif

	m_data.SetSize(nSizeMod);
	CClmnPt* pTop;
	CClmnPt* pBot;
	for(int i=0; i<m_nRows; i++) {
		for(int j=0; j<m_nCols; j++) {
			m_data[i*( m_nCols ) + j].SetSize(0, 50);
			pTop = new CClmnPt( m_zMax );
			pBot = new CClmnPt( m_zMin );
			Add( i, j, pTop );
			Add( i, j, pBot );
		}
	}
	m_fct.SetSize(nSizeMod);
	m_bodyBound.SetSize(nSizeMod);
	m_glFacets.SetSize(nSizeMod);
	m_glSelPt.SetSize(nSizeMod);

	m_bInitialized = TRUE;

	return TRUE;
}

BOOL CModel::Init( CGrid* pGrd, double zMin, double zMax)
{
	GridInfo gi;
	gi = pGrd->GetInfo();

	m_nRows = gi.nRow+2;
	m_nCols = gi.nCol+2;
	m_x0 = gi.x0;
	m_y0 = gi.y0;
	m_xSize  = gi.xSize;
	m_ySize = gi.ySize;
	m_xMin = m_x0;		
	m_xMax = m_x0 + (m_nCols-3)*m_xSize;
	m_yMin = m_y0;		
	m_yMax = m_y0 + (m_nRows-3)*m_ySize;
	m_zMin = zMin;		
	m_zMax = zMax;

	int nSizeMod = m_nRows*m_nCols;

#ifndef MOD3D_FULL
	m_nRows = min(m_nRows, 64);
	m_nCols = min(m_nCols, 64);
	nSizeMod = m_nRows*m_nCols;
	nSizeMod = min(nSizeMod, 576);
#endif

	m_data.SetSize(nSizeMod);
	CClmnPt* pTop;
	CClmnPt* pBot;
	double** pData = pGrd->GetData();
	int ii, jj;
	double v;
	for(int i=0; i<m_nRows; i++) {
		for(int j=0; j<m_nCols; j++) {
			ii = i-1;
			jj = j-1;
			if( ii<=1 ) ii = 0;
			if( ii>=(int)gi.nRow-1 ) ii = gi.nRow-1;
			if( jj<=1 ) jj = 0;
			if( jj>=(int)gi.nCol-1 ) jj = gi.nCol-1;
			v = pData[ii][jj];
			m_data[i*( m_nCols ) + j].SetSize(0, 50);
			pTop = new CClmnPt( v );
			pBot = new CClmnPt( m_zMin );
			Add( i, j, pTop );
			Add( i, j, pBot );
		}
	}
	m_fct.SetSize(nSizeMod);
	m_bodyBound.SetSize(nSizeMod);
	m_glFacets.SetSize(nSizeMod);
	m_glSelPt.SetSize(nSizeMod);

	m_bInitialized = TRUE;

	return TRUE;
}

void CModel::SetVerticalRange(double min, double max)
{
	m_zMin = min;		
	m_zMax = max;
	for(int i=0; i<m_nRows; i++) {
		for(int j=0; j<m_nCols; j++) {
			int n = m_data[i*( m_nCols ) + j].GetUpperBound();
			m_data[i*( m_nCols ) + j].GetAt(n)->m_z = min;
			m_data[i*( m_nCols ) + j].GetAt(n)->m_pt.z = min;
		}
	}
}

void CModel::CreationProperties(void)
{
	CDlgBodyModf dlg;

	dlg.m_bConstBot = m_bConstBot;
	dlg.m_dBot = m_dConstBot;
	dlg.m_bConstTop = m_bConstTop;
	dlg.m_dTop = m_dConstTop;
	dlg.m_nJoin = m_nCreationTag;
	dlg.m_dBodyCreationFactor = m_dCreationRatio;

	if( dlg.DoModal() == IDOK ) {
		m_bConstBot = dlg.m_bConstBot;
		m_dConstBot = dlg.m_dBot;
		m_bConstTop = dlg.m_bConstTop;
		m_dConstTop = dlg.m_dTop;
		m_nCreationTag = dlg.m_nFlag;
		m_dCreationRatio = dlg.m_dBodyCreationFactor;
	}
}

void CModel::Properties() 
{
	CDlgModelProperties dlg;


	dlg.m_bExtendBoundaries = m_bExtend;
	dlg.m_fExE = m_dExE;
	dlg.m_fExN = m_dExN;
	dlg.m_fExS = m_dExS;
	dlg.m_fExW = m_dExW;

	dlg.m_fMaxX = m_xMax;
	dlg.m_fMinX = m_xMin;
	dlg.m_fMaxY = m_yMax;
	dlg.m_fMinY = m_yMin;
	dlg.m_fMaxZ = m_zMax;
	dlg.m_fMinZ = m_zMin;
	dlg.m_xSize = m_xSize;
	dlg.m_ySize = m_ySize;

	dlg.m_strInfo = m_strInfo;

	if(dlg.DoModal() == IDOK) {
		m_bExtend = dlg.m_bExtendBoundaries;
		m_dExE = dlg.m_fExE;
		m_dExN = dlg.m_fExN;
		m_dExS = dlg.m_fExS;
		m_dExW = dlg.m_fExW;

		m_xMax = dlg.m_fMaxX;
		m_xMin = dlg.m_fMinX;
		m_yMax = dlg.m_fMaxY;
		m_yMin = dlg.m_fMinY;
		m_zMax = dlg.m_fMaxZ;
		m_zMin = dlg.m_fMinZ;

		m_strInfo = dlg.m_strInfo;
	}

}
//////////////////////////////////////////////////////////////////////////////////////////////
//******** coordinate functions **************************************************************
double CModel::GetZ(int row, int col, int nIndex)
{
	if( nIndex < 0 )
		return GetHeaven();
	if( nIndex > GetUpperBound(row, col) )
		return GetHell();

	CClmnPt*	pC;
	pC = GetAt(row, col, nIndex);

	return pC->m_z;
}

double CModel::GetZmid(int row, int col, int nIndex)
{
	// index must be valid
	if( nIndex<0 || nIndex > GetUpperBound(row, col)-1 )
		return DUMMY;

	return (GetAt(row, col, nIndex)->m_z + GetAt(row, col, nIndex+1)->m_z)/2;
}

void CModel::SetZ(int row, int col, int nIndex, double z)
{
	if( nIndex > 0 && nIndex < GetCount(row, col) ){
		GetAt(row, col, nIndex)->m_z = z;
		GetAt(row, col, nIndex)->m_pt.z = z;
		return;
	}

	ASSERT("Critical error. SetZ index problem.");
}

void CModel::GetXZd(int nIndex, int row, int col, double& x, double& z)
{
	z = GetZ(row, col, nIndex);
	x = GetXd(col);
}

void CModel::GetYZd(int nIndex, int row, int col, double& y, double& z)
{
	z = GetZ(row, col, nIndex);
	y = GetYd(row);
}

double CModel::GetXe(int col)					
{ 
	if( col == 0 ) return m_x0 - m_dExW;
	if( col == m_nCols-1 ) 
		return m_x0 + (col-2)*m_xSize + m_dExE;
	return m_x0 + (col-1)*m_xSize;
}

double CModel::GetYe(int row)					
{ 
	if( row == 0 ) return m_y0 - m_dExS;
	if( row == m_nRows-1 ) 
		return m_y0 + (row-2)*m_ySize + m_dExN;
	return m_y0 + (row-1)*m_ySize;
}

double CModel::GetX2e(int col)
{ 
	if( col == 0 ) return m_x0 - m_dExW/2;
	if( col == m_nCols-2 ) 
		return m_x0 + (col-1)*m_xSize + m_dExE/2.0;
	return m_x0 + (col-1)*m_xSize + m_xSize/2;
}

double CModel::GetY2e(int row)					
{ 
	if( row == 0 ) return m_y0 - m_dExS/2;
	if( row == m_nRows-2 ) 
		return m_y0 + (row-1)*m_ySize + m_dExN/2;
	return m_y0 + (row-1)*m_ySize + m_ySize/2;
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//******** input - output model array and body array functions **********************************************
// returns 1D array index
// when the index is not valid returns -1
int CModel::GetIndex(int row, int col)
{
	int n = row*( m_nCols ) + col;
	if( n < (m_nCols)*(m_nRows) ) 
		return n;
	else
		return -1;
}

FacetList* CModel::GetFacetList(int row, int col)
{
	if( row < 0 )			return NULL;
	if( col < 0 )			return NULL;
	if( row > m_nRows-2 )	return NULL;
	if( col > m_nCols-2 )	return NULL;

	int n = row*( m_nCols ) + col;
	if( n < (m_nCols)*(m_nRows) ) 
		return &m_fct.GetAt( n );

	return NULL;
}

void CModel::GetFacetListEx(int row, int col, FacetList* fctLst)
{
	FacetList* pTmp;

	pTmp = GetFacetList(row-1, col-1);
	if( pTmp!=NULL && !pTmp->IsEmpty() ) fctLst->AddTail(pTmp);

	pTmp = GetFacetList(row-1, col);
	if( pTmp!=NULL && !pTmp->IsEmpty() ) fctLst->AddTail(pTmp);
	
	pTmp = GetFacetList(row, col-1);
	if( pTmp!=NULL && !pTmp->IsEmpty() ) fctLst->AddTail(pTmp);
	
	pTmp = GetFacetList(row, col);
	if( pTmp!=NULL && !pTmp->IsEmpty() ) fctLst->AddTail(pTmp);
}

ClmnPtPtrArray* CModel::GetAt(int row, int col)
{
	int n = row*( m_nCols ) + col;
	if( n < (m_nCols)*(m_nRows) ) 
		return &m_data.GetAt( n );

	return NULL;
}

CClmnPt* CModel::GetAt(int row, int col, int nIndex)
{
	ClmnPtPtrArray* pAr = GetAt( row, col );
	if( pAr ) {
		if( nIndex < pAr->GetSize() && nIndex >= 0 )
			return pAr->GetAt( nIndex );
		else
			return NULL;
	}

	return NULL;
}

void CModel::Add( int row, int col, CClmnPt* pMpt  )
{
	SetClmnPt(pMpt, row, col, pMpt->m_z);
	GetAt( row, col )->Add( pMpt );
}

void CModel::InsertAt( int row, int col, int nIndex, CClmnPt* pMpt  )
{
	SetClmnPt(pMpt, row, col, pMpt->m_z);
	GetAt( row, col )->InsertAt( nIndex, pMpt );
}

void CModel::RemoveAt( int row, int col, int nIndex, int nCount )
{
	CClmnPt* pPt;
	for( int i=0; i<nCount; i++) {
		//pPt = GetAt(row, col)->GetAt( nIndex );
		pPt = GetAt(row, col, nIndex);
		if( pPt == NULL )
			return;
		GetAt(row, col)->RemoveAt( nIndex );
		delete pPt;
	}
}

int CModel::GetUpperBound(int row, int col)
{
	return (int)GetAt(row, col)->GetUpperBound();
}

int CModel::GetCount(int row, int col)
{
	return (int)GetAt(row, col)->GetCount();
}

int CModel::IsEmpty(int row, int col)
{
	return GetAt(row, col)->IsEmpty();
}

void CModel::SetClmnPt( CClmnPt* cPt, int row, int col, double z)						// set CPoint3D
{
	cPt->m_pt.x = GetXe(col);
	cPt->m_pt.y = GetYe(row);
	cPt->m_pt.z = z;
}

void CModel::GetVertexPt(int row, int col, int nIndex, CPoint3D& pt)
{
	CClmnPt* cPt;

	cPt = GetAt(row, col, nIndex);
	ASSERT(cPt != NULL);
	pt = cPt->m_pt;
}

double CModel::GetThicknes(int row, int col, int nIndex)
{
	if( nIndex == -1 || (nIndex % 2) == 0 )	return DUMMY;

	CClmnPt* pClpt, *pClptNext;;
	pClpt = GetAt(row, col, nIndex);
	pClptNext = GetAt(row, col, nIndex+1);
	if( pClpt == NULL || pClptNext== NULL) return DUMMY;

	return pClpt->m_z - pClptNext->m_z;
}

double CModel::GetThicknesNew(int row, int col, int nIndex)
{
	if( nIndex == -1 || (nIndex % 2) == 0 )	return DUMMY;

	CClmnPt* pClpt, *pClptNext;;
	pClpt = GetAt(row, col, nIndex);
	pClptNext = GetAt(row, col, nIndex+1);
	if( pClpt == NULL || pClptNext == NULL) return DUMMY;

	return m_dCreationRatio * (pClpt->m_z - pClptNext->m_z);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// ****** body array functions *******************************************************************
void CModel::UpdateBodyIndex(void)
{
	// MUST be called after each modification of body array
	int n = (int)m_bodies.GetSize();
	CBody* pBd;
	for(int i=0; i<n; i++) {
		pBd = m_bodies.GetAt(i);
		pBd->SetIndex(i);
	}
}

CBody* CModel::NewBody(void)
{
	// allocates memory for new body, sets ID, adds to body array
	CBody* pBody = new CBody( /*m_lp[m_nBodyID]*/ );
	pBody->SetID( m_nBodyID );
	m_nBodyID++;
	m_bodies.Add( pBody );
	UpdateBodyIndex();
	AfxGetMainWnd()->SendMessage(WM_UPDATE_OBJMNG);

	return pBody;
}

int CModel::DeleteBody(int nID)
{
	// deletes body from body array
	
	if( !m_bodies.IsEmpty() ) {
		for(int i=0; i < m_bodies.GetSize(); i++) {
			CBody* pBody = m_bodies.GetAt(i);
			if( pBody->GetID() == nID ) {
				// remove body from model
				for( int row=0; row<m_nRows; row++ ) {
					for( int col=0; col<m_nCols; col++) {
						int nIndex = GetBodyIndex(nID, row, col);
						if( nIndex != -1 ) {
							RemoveBody(nIndex, row, col);
						}
					}
				}
				// delete body
				m_bodies.RemoveAt(i);
				delete pBody;
				UpdateBodyIndex();
				int nCompType = m_bComputeRealTime;
				m_bComputeRealTime = FALSE;
				InitFacetList();
				m_bComputeRealTime = nCompType;
				AfxGetMainWnd()->SendMessage(WM_UPDATE_OBJMNG);
				return 1;
			}
		}
	}
	
	return 0;
}

CBody* CModel::GetBody(int nIndex, int row, int col)
{
	//TRACE("Entering CModel::GetBody(int nIndex, int row, int col)\n");

	int id = GetID(nIndex, row, col);
	if( id != -1 )
		return GetBody(id);
	else
		return NULL;
}

CBody* CModel::GetBody(int nID)
{
	if( m_bodies.IsEmpty() )
		return NULL;

	for(int i=0; i<m_bodies.GetCount(); i++) {
		CBody* pBody;
		pBody = m_bodies.GetAt(i);
		if( pBody->GetID() == nID )
			return pBody;
	}

	return NULL;
}

int CModel::GetBodyIndex(int nID)
{
	if( m_bodies.IsEmpty() )
		return -1;

	for(int i=0; i<m_bodies.GetCount(); i++) {
		CBody* pBody;
		pBody = m_bodies.GetAt(i);
		if( pBody->GetID() == nID )
			return pBody->GetIndex();
	}

	return -1;
}

int	 CModel::GetBodyIndex(int nID, int row, int col)
{
	for( int i=1; i<GetCount(row, col); i +=2 ) {
		if( GetAt(row, col, i)->m_nBodyID == nID )
			return i;
	}
	return -1;
}

int	 CModel::GetBodyTopBot(int nID, int row, int col, double &t, double &b)
{
	CClmnPt* pClm;
	for( int i=1; i<GetCount(row, col)-1; i +=2 ) {
		pClm = GetAt(row, col, i);
		if( pClm->m_nBodyID == nID ) {
			t = pClm->m_z;
			pClm = GetAt(row, col, i+1);
			b = pClm->m_z;
			return i;
		}
	}
	return -1;
}

int CModel::GetID(int nIndex, int row, int col)
{
	if( nIndex > 0 && nIndex < GetCount(row, col)-1 )
		return GetAt( row, col, nIndex )->m_nBodyID;

	return -1;
}

BDLINArray* CModel::GetBodyBoundAt(int row, int col)
{
	if( row < 0 )			return NULL;
	if( col < 0 )			return NULL;
	if( row > m_nRows-2 )	return NULL;
	if( col > m_nCols-2 )	return NULL;

	int n = row*( m_nCols ) + col;
	if( n < (m_nCols)*(m_nRows) ) 
		return &m_bodyBound.GetAt( n );

	return NULL;
}

void CModel::AddBodyBoundAt(int row, int col, BDLIN pt )
{
	int n = row*( m_nCols ) + col;
	if( n < (m_nCols)*(m_nRows) ) 
		m_bodyBound.GetAt(n).Add( pt );

}


//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
//******* model modification functions *******************************************************
// finds constrains for constrained vertical vertex motion 
void CModel::GetConstrainZ(int row, int col, int index, double& zU, double& zD)
{
	double zC;	// zUp, zDown
	int indexMax;
	int	i = index, j = index;

	ASSERT(index != -1 || index != 0);

	indexMax = GetCount(row, col);	
	zC = GetZ(row, col, index);
	zU = GetZ(row, col, index-1);
	zD = GetZ(row, col, index+1);
	j++;
	while( j < indexMax ) {
		zD = GetZ(row, col, j);
		if( zC >  zD ) 
			break;
		j++;
	}
}

// returns: -1 when failed
//			0 when succesful
//			2 when in constrained mode the point is outside boundaries
int CModel::MoveVertex(int &nIndex, int row, int col, double z, int nTag)
{
	double	zU, zD, zC, zR;	// zUp, zDown
	int		indexMax;
	int		i = nIndex, j = nIndex;
	CBody*	pBody;

	if( nIndex == -1 ) {
		return -1;
	}
	if( nIndex == 0 ) {
		AfxMessageBox("Relief!", MB_OK | MB_ICONINFORMATION);
		return -1;
	}

	// ++++++ limits+++++++++++++++++++++++++++++
	indexMax = GetCount(row, col)-1;		//(int)m_mod[row][col].GetCount();
	zR = GetZ(row, col, 0);
	zC = GetZ(row, col, nIndex);
	pBody = GetBody(nIndex, row, col);
	if( pBody->IsLocked() )  {
		AfxMessageBox("Locked!", MB_OK | MB_ICONINFORMATION);
		return -1;
	}
	zU = GetZ(row, col, nIndex-1);
	zD = GetZ(row, col, nIndex+1);
	j++;
	while( j < indexMax+1 ) {
		zD = GetZ(row, col, j);
		if( zC >  zD) 
			break;
		if( j == indexMax ) {
			zD = GetZ(row, col, j);	// bottom
			break;
		}
		pBody = GetBody(j, row, col);
		if( pBody->IsLocked() )  {
			AfxMessageBox("Locked!", MB_OK | MB_ICONINFORMATION);
			return -1;
		}
		j++;
	}
	//if( j == indexMax) j--;
	//++++++++++++++++++++++++++++++++++++++++++++

	//------- set values -------------------------
	// set current row and column 
	m_nRow = row;
	m_nCol = col;

	if( nTag == BMT_NORMAL ) {
		if( z >= zU ) {
			z = zU;
			nIndex--;		// !!! unpdate index
		}
		if( z <= zD )
			z = zD;
		while( i < j ) {
			SetZ(row, col, i, z);
			i++;
		}
		if(z == zR) {
			nIndex = 1;
			//SetZ(row, col, nIndex, z);
			return 0;
			//AfxMessageBox("Out of range on relief!", MB_OK | MB_ICONINFORMATION);
			//return -1;
		}
		if(z == m_zMin) {
			return 0;
			//AfxMessageBox("Out of range at bottom!", MB_OK | MB_ICONINFORMATION);
			//return -1;
		}
	}

	if( nTag == BMT_SPLIT ) {
		if( z >= zC && z < zU ) {
			SetZ(row, col, nIndex, z);
		}
		if( z <= zC && z > zD) {
			nIndex = j-1;
			SetZ(row, col, nIndex, z);
		}
	}

	if( nTag == BMT_CONSTRAINED ) {
		if( z > zD && z < zU ) {
			SetZ(row, col, nIndex, z);
		}
		else
			return 2;
	}
	//--------------------------------------------

	// update facet list for given column
	UpdateFacetList(row, col);

	return 0;
}

int CModel::InsertBody(int row, int col, double z, double t, BOOL bNew, int nBodyID, double zT, double zB, BOOL bZ)
{
	// inserts body to model column
	// z - middle value
	// t = thickness/2
	// bNew if true new body is created
	// nBodyID if is valid this body will be inserted, if -1 select new body manually

	CBody	*pBody=NULL;

	if( bNew ) {
		pBody = NewBody();
	}
	else {
		if( nBodyID == -1 ) {
			nBodyID = SelectBody();
			if( nBodyID == -1 ) {
				//AfxMessageBox("Add body failed!");
				return -1;
			}
		}
		pBody = GetBody( nBodyID );
	}

	if( pBody == NULL ) {
		//AfxMessageBox("Insert body failed!");
		return -1;
	}
	nBodyID = pBody->GetID();

	//double	zT, zB;	// top and bottom body coordiates
	double  zU, zD;	// Up ond Down limits

	if(bZ) {
		if( t == -1.0 )	t=GetThicknes();
		zT = z+t;	// body top
		zB = z-t;	// body bottom
	}
	else {
		z = (zT+zB)/2.0;
		t = zT-zB;
	}
	if( m_bConstTop ) zT = m_dConstTop;
	if( m_bConstBot ) zB = m_dConstBot;

	// verify insert range, should never happen
	zD = m_zMin;
	zU = GetZ( row, col, 0 );
	if( ! (zD < z || z < zU) ) {
		//AfxMessageBox("Insert position out of range!", MB_OK | MB_ICONINFORMATION);
		return -1;
	}

	// find insert index
	int nIndex=1;	// insert position
	int iMax = (int) GetCount(row, col);
	while( nIndex < iMax ) {
		zU = GetZ(row, col, nIndex-1);
		zD = GetZ(row, col, nIndex);
		if( zD < z && z < zU) 
			break;
		nIndex += 2;
	}

	// invalid index
	if( nIndex > iMax ) return -1;

	// insert position check
	if( !IsInsPosOK(nIndex, nBodyID, row, col) ) {
		return -1;
	}

	switch( m_nCreationTag ) {
		case BCR_NONE:
			// leave current values
			break;
		case BCR_JOINTOP:
			zT = zU;
			break;
		case BCR_JOINBOT:
			zB = zD;
			break;
		case BCR_JOINTOPBOT:
			zB = zD;
			zT = zU;
			break;
	}

	// insert body
	zT = min(zU, zT);
	zB = max(zD, zB);

	// insert body to column
	CClmnPt* pBot = new CClmnPt( nBodyID, zB, pBody );
	CClmnPt* pTop = new CClmnPt( nBodyID, zT, pBody );
	InsertAt(row, col, nIndex, pBot);
	InsertAt(row, col, nIndex, pTop);

	// set current row and column 
	m_nRow = row;
	m_nCol = col;

	// update facet list for given column
	UpdateFacetList(row, col);

	return nIndex;
}

int CModel::RemoveBody(int nIndex, int row, int col)
{
	// removes body in verical column, leaves body in body list

	// set current row and column 
	m_nRow = row;
	m_nCol = col;
	RemoveAt(row, col, nIndex, 2);
	
	// update facet list for given column
	UpdateFacetList(row, col);

	return 1;
}

int CModel::CopyBody(int nID,int nFrom, int nTo, int nTag)
{
	// nFrom -> current row or column from which to copy
	// nTo -> row or columnt to which the body will be copied
	int		i, *p_i, *p_j;			// pointers to switch wariables
	int		*p_iTo, *p_jTo;			// pointers to switch wariables
	int		rc;						// number of rows or columns on profile
	int		nProfs;					// number of profiles perpendicular to current profile direction
	int		nIndex;					// body index in column
	double	t, z;					// thickness, middle insert z

	if( nTag == PRF_ROW ) {rc = m_nCols; nProfs = m_nRows; p_i = &nFrom; p_j = &i; p_iTo = &nTo; p_jTo = &i;}
	if( nTag == PRF_COL ) {rc = m_nRows; nProfs = m_nCols; p_i = &i; p_j = &nFrom; p_iTo = &i; p_jTo = &nTo;}

	// chcek if profiles are valid
	if( !( nFrom>=0 && nTo>=0 && nFrom<nProfs && nTo<nProfs) ) {
		AfxMessageBox("Testing???", MB_OK | MB_ICONQUESTION);
		return 0;
	}

	for(i=0; i<rc; i++) {
		nIndex = GetBodyIndex(nID, *p_i, *p_j);
		if( nIndex != -1 ) {
			t = GetThicknes(*p_i, *p_j, nIndex);
			z = GetZmid(*p_i, *p_j, nIndex);
			if( z==DUMMY || t == DUMMY ) return 0;
			InsertBody(*p_iTo, *p_jTo, z, t/2, FALSE, nID);
		}
	}

	return 1;
}

int CModel::RemoveBodyFromProfile(int nID, int nFrom, int nTag)
{
	int		i, *p_i, *p_j;			// pointers to switch wariables
	int		rc;						// number of rows or columns
	int		nIndex;					// body index in column
	int		nProfs;					// number of profiles perpendicular to current profile direction

	if( nTag == PRF_ROW ) {rc = m_nCols; nProfs = m_nRows; p_i = &nFrom;	p_j = &i; }
	if( nTag == PRF_COL ) {rc = m_nRows; nProfs = m_nCols; p_i = &i;		p_j = &nFrom; }

	// chcek if profiles are valid
	if( !( nFrom>=0 && nFrom<nProfs ) ) {
		AfxMessageBox("Testing???", MB_OK | MB_ICONQUESTION);
		return 0;
	}

	for(i=0; i<rc; i++) {
		nIndex = GetBodyIndex(nID, *p_i, *p_j);
		if( nIndex != -1 ) {
			RemoveBody(nIndex, *p_i, *p_j);
		}
	}

	return 1;
}

void CModel::CheckIndex(int &nIndex, int row, int col) 
{
	int indexMax = GetCount(row, col)-1;
	double zi;
	double z;
	z = GetZ(row, col, nIndex);
	for(int i=1; i<indexMax; i++) {
		zi = GetZ(row, col, i);
		if( zi == z) {
			nIndex = i;
			return;
		}
	}
}

int CModel::MoveBody(int nID, double dTop, double dBot, int nTag, BOOL bAbsolute)
{
	int		nIndexTop;
	int		nIndexBot;
	int		nTagVrtx;
	double	zt, zb;

	
	nTagVrtx = BMT_NORMAL;
	BOOL	bShfInit = TRUE;
	for(int i=0; i<m_nRows; i++) {
		for(int j=0; j<m_nCols; j++) {
			nIndexTop = GetBodyIndex(nID, i, j);
			nIndexBot = nIndexTop+1;
			if(bAbsolute==FALSE) {
				nIndexTop = GetBodyIndex(nID, i, j);
				CheckIndex(nIndexTop, i, j);
				if( nIndexTop != -1 ) {
					zt = GetZ(i, j, nIndexTop);
					zb = GetZ(i, j, nIndexBot);
					if(nTag==BMF_TOP || nTag==(BMF_TOP|BMF_BOT) ) {
						zt += dTop;
						MoveVertex(nIndexTop, i, j, zt, nTagVrtx);
					}
					if((nTag==BMF_BOT || nTag==(BMF_TOP|BMF_BOT)) && zb!=zt) {
						zb += dBot;
						MoveVertex(nIndexBot, i, j, zb, nTagVrtx);
					}
				}
			}

			// set body boubdary
			if(bAbsolute) {
				nIndexTop = GetBodyIndex(nID, i, j);
				if( nIndexTop != -1 ) {
					zt = dTop;
					zb = dBot;
					if(nTag==BMF_TOP || nTag==(BMF_TOP|BMF_BOT) ) {
						MoveVertex(nIndexTop, i, j, zt, nTagVrtx);
					}
					if(nTag==BMF_BOT || nTag==(BMF_TOP|BMF_BOT) && zb!=zt) {
						MoveVertex(nIndexBot, i, j, zb, nTagVrtx);
					}
				}
			}
		}
	}

	return 0;
}
//******* end model modification functions ************************************************************************************************
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//******* modification helper functions ***************************************************************************************************
BOOL CModel::IsInsPosOK(int nIndex, int nID, int row, int col)
{
	// same body on column
	for( int i=0; i<GetCount(row, col)-1; i += 2 ) {
		if( nID == GetID( i, row, col ) ) {
			CString str;
			str.Format("Body insert conflict at row=%d, col=%d, body ID=%d", row, col, nID);
			AfxMessageBox(str, MB_OK | MB_ICONSTOP);
			return FALSE;
		}
	}
	//next columns
	for( int i=1; i<GetCount(row, col)-1; i += 2 ) {
		int nTag=BCH_NONE;;
		if( i < nIndex ) nTag = BCH_NEXT;
		if( i >= nIndex ) nTag = BCH_PREV;
		int nIdCheck = GetID( i, row, col );
		if( !IsInsPosOK_1(nTag, nID, nIdCheck, row-1, col) )	return FALSE;
		if( !IsInsPosOK_1(nTag, nID, nIdCheck, row+1, col) )	return FALSE;
		if( !IsInsPosOK_1(nTag, nID, nIdCheck, row, col-1) )	return FALSE;
		if( !IsInsPosOK_1(nTag, nID, nIdCheck, row, col+1) )	return FALSE;
		// cross positions
		if( !IsInsPosOK_1(nTag, nID, nIdCheck, row+1, col+1) )	return FALSE;
		if( !IsInsPosOK_1(nTag, nID, nIdCheck, row-1, col-1) )	return FALSE;
		if( !IsInsPosOK_1(nTag, nID, nIdCheck, row+1, col-1) )	return FALSE;
		if( !IsInsPosOK_1(nTag, nID, nIdCheck, row-1, col+1) )	return FALSE;
	}

	return TRUE;
}

BOOL CModel::IsInsPosOK_1(int nTag, int nID, int nIdCheck, int row, int col)
{
	// checks if neighbouring posirions are OK
	if( row < 0 ) return TRUE;
	if( col < 0 ) return TRUE;
	if( row > m_nRows-1 ) return TRUE;
	if( col > m_nCols-1 ) return TRUE;

	for( int i=0; i<GetCount(row, col)-1; i += 2 ) {
		if( nID == GetID( i, row, col ) )
			return IsInsPosOK_2(nTag, nID, nIdCheck, row, col);
	}

	return TRUE;
}

BOOL CModel::IsInsPosOK_2(int nTag, int nID, int nIdCheck, int row, int col)
{
	int nTagHere = BCH_PREV;
	for( int i=0; i<GetCount(row, col)-1; i += 2 ) {
		if( nID == GetID( i, row, col ) ) {
			nTagHere = BCH_NEXT;
		}
		if( nTag == nTagHere && nIdCheck == GetID( i, row, col ) ) {
			CString str;
			str.Format("Body insert conflict at row=%d, col=%d, body ID=%d", row, col, nID);
			AfxMessageBox(str, MB_OK | MB_ICONSTOP);
			return FALSE;
		}
	}
	return TRUE;
}

int CModel::SelectBody(void)
{
	// returns selected body ID

	CDlgSelectBody dlg;
	if( m_bodies.IsEmpty() ) {
		AfxMessageBox("There is no body!");
		return -1;
	}

	dlg.m_pBodyArr = &m_bodies;

	if( dlg.DoModal() == IDOK ) {
		if( dlg.m_pBody )
			return dlg.m_pBody->GetID();
	}

	AfxMessageBox("No body selected! Operation canceled.", MB_OK | MB_ICONINFORMATION);
	return -1;
}

void CModel::UpdateMoveIndex(int &nIndex, int nIndexOld, int row, int col, int bodyID)
{
	int iPlus = (nIndexOld+1) % 2;
	int indexMax = GetCount(row, col);
	int i=1;
	while( i < indexMax ) {
		if( bodyID == GetID(i, row, col) ) { 
			nIndex = i+iPlus;
			return;
		}
		i+=2;
	}
	nIndex = -1;
}

//******* end modification helper functions *****************************************************************************************
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////






///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//********* GENERATE FACETS ***********************************************************************************************************************************************************

// facett list for computation
// dSign for all facets is set to 1.0
int	CModel::GetFacetsComputation(FacetList& fctLst)
{
	for( int i=0; i<m_fct.GetCount(); i++ ) {
		fctLst.AddTail( &m_fct.GetAt(i) );
	}

	POSITION pos, posRem;
	pos=fctLst.GetHeadPosition();
	while(pos) {
		posRem = pos;
		fctLst.GetNext(pos).SetSign(1.0);
	}

	return fctLst.GetCount();
}

void CModel::InitFacetList(void)
{
	for(int i=0; i<m_nRows-1; i++) {
		for(int j=0; j<m_nCols-1; j++) {
			UpdateFacetColumn(i, j);
			FacetList* pFctLst = GetFacetList(i,j);
			if( !pFctLst->IsEmpty() ) {
				// set facets as computed -> facet.dSign = -1
				POSITION pos;
				pos = pFctLst->GetHeadPosition();
				while(pos) {
					pFctLst->GetNext(pos).dSign = -1;
				}
			}
		}
	}
}

void CModel::UpdateFacetColumn(int row, int col)
{
	FacetList*	pFctLst;

	pFctLst = GetFacetList(row, col);
	// !!! add body boundary for Map View
	m_pBodyBoundCol = GetBodyBoundAt(row, col);

	if( pFctLst ) {
		//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		// add OLD facets to update list with NEGATIVE sign
		// !!! already computed facet is signed as -1 !!!
		//m_fctLstUpdate.AddTail( pFctLst );
		if( m_bComputeRealTime ) {
			FacetList	fctLstOld;
			FacetList	fctLstNew;
			POSITION	pos;
			pos = pFctLst->GetHeadPosition();
			while(pos) {
				CFacet3Pt* pFct = &pFctLst->GetNext(pos);
				pFct->SetSign(-1.0);
				m_fctLstUpdate.AddTail(*pFct);
			}
		}
		//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

		pFctLst->RemoveAll();
		m_pBodyBoundCol->RemoveAll();
		GenerateFacets(*pFctLst, row, col);

		//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		// add NEW facets to update list with POSITIVE sign (default)
		if( m_bComputeRealTime ) 
			m_fctLstUpdate.AddTail( pFctLst );
		//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

		UpdateGLFacets(row, col);
		UpdateGLSelPt(row,col);
	}
}

void CModel::UpdateFacetList(int row, int col)
{
	// updates list for given changed (row, column)
	UpdateFacetColumn(row-1, col-1);
	UpdateFacetColumn(row-1, col);
	UpdateFacetColumn(row, col-1);
	UpdateFacetColumn(row, col);
}

int CModel::AddFctGen(FctGenArray& fga, int row, int col, int nTag)
{
	// nTag -> point coding

	FCTGENER fg, *pFg;
	fg.b[nTag] = FALSE;
	for( int l=0; l<4; l++) fg.b[l] = FALSE;
	for(int i=1; i<GetCount(row, col)-1; i += 2) {
		int id = GetID(i, row, col);
		BOOL bAdded = FALSE;
		for(int j=0; j < fga.GetCount(); j++) {
			pFg = &fga.GetAt(j);
			if( pFg->id == id ) {
				//pFg->id = id;
				pFg->idPrev[nTag] = GetID(i-1, row, col);
				pFg->idNext[nTag] = GetID(i+1, row, col);
				pFg->top[nTag] = GetAt(row, col, i)->m_pt;
				pFg->bot[nTag] = GetAt(row, col, i+1)->m_pt;
				pFg->b[nTag] = TRUE;
				pFg->l++;
				bAdded = TRUE;
				break;
			}
		}
		if( !bAdded ) {
			fg.id = GetID(i, row, col);
			fg.idPrev[nTag] = GetID(i-1, row, col);
			fg.idNext[nTag] = GetID(i+1, row, col);
			fg.top[nTag] = GetAt(row, col, i)->m_pt;
			fg.bot[nTag] = GetAt(row, col, i+1)->m_pt;
			fg.b[nTag] = TRUE;
			fg.l = 1;
			fga.Add( fg );
		}
	}

	return (int )fga.GetCount();
}

// sorting routine, compares column bodies
int CompareClmn(const void *arg1, const void *arg2 ) 
{
	// all similar facets MUST be REMOVED !!!
	FCTGENER	*pF1, *pF2;

	pF1 = (FCTGENER*) arg1;
	pF2 = (FCTGENER*) arg2;

	int id = pF2->id;
	for(int i=0; i<4; i++) {
		if( pF1->bot[i].z < pF2->top[i].z && pF1->b[i] && pF2->b[i] )	
			return 1;
		if( pF1->top[i].z > pF2->bot[i].z && pF1->b[i] && pF2->b[i] )	
			return -1;
	}
	// should never happen
	return 0;
}

int CModel::GenerateFacets(FacetList& fctLst, int row, int col)
{
	// fills fctLst with facets
	// row, col -> lower left corner
	//TRACE("Entering CModel::GenerateFacets(FacetList& fctLst, int row, int col) \n");

	if( row < 0 )			return 0;
	if( col < 0 )			return 0;
	if( row > m_nRows-2 )	return 0;
	if( col > m_nCols-2 )	return 0;

	FctGenArray	fga;
	fga.SetSize(0, 500);

	// normal 
	AddFctGen(fga, row, col, 0);
	AddFctGen(fga, row, col+1, 1);
	AddFctGen(fga, row+1, col+1, 2);
	AddFctGen(fga, row+1, col, 3);

	if( fga.IsEmpty() ) return 0;

	// remove all "fa.l <= 2" and compute middle point
	for(int i=0; i<fga.GetSize(); i++) {
		FCTGENER *pFg = &fga.GetAt(i);
		FGtoFCTbound( pFg, &fctLst );
		if( pFg->l <= 2 ) {
			if( pFg->l == 2 ) InitSideFacets( pFg, &fctLst );
			fga.RemoveAt(i);
			i--;
		}
		// interpolate middle point
		else {
			double t=0.0, b=0.0;
			int l=0;
			for(int k=0; k<4; k++) {
				if( pFg->b[k] ) {
					t += pFg->top[k].z;
					b += pFg->bot[k].z;
					l++;
				}
			}
			if( (t-b) == 0.0 ) {
				// similar facets of the same body, remove them
				fga.RemoveAt(i);
				i--;
				//AfxMessageBox("Removing similar facets.", MB_OK | MB_ICONINFORMATION);
			}
			else {
				pFg->top[4].x = pFg->bot[4].x = GetX2e(col);	
				pFg->top[4].y = pFg->bot[4].y = GetY2e(row);
				pFg->top[4].z = t/l;
				pFg->bot[4].z = b/l;
				if(	pFg->l==3 &&  (	(pFg->bot[0].z==pFg->top[0].z && pFg->bot[2].z==pFg->top[2].z && pFg->b[0] && pFg->b[2] )  ||
									(pFg->bot[1].z==pFg->top[1].z && pFg->bot[3].z==pFg->top[3].z && pFg->b[1] && pFg->b[1] ) )) {
					pFg->top[4].z = pFg->bot[4].z  = (t+b)/(2*l);
				}
			}
		}
	}

	if( fga.IsEmpty() ) return 0;

	// sort them in ascending order
	// for compare routine all similar facets MUST be REMOVED
	qsort(fga.GetData(), fga.GetSize(), sizeof( FCTGENER ), CompareClmn );

	// check middle point intersections && generate side facets
	FCTGENER *pC, *pN;		// Current and Next
	int n = (int)fga.GetSize();
	for(int i=0; i<n; i++) {
		pC = &fga.GetAt(i);
		if( i < n-1 ) {
			pN = &fga.GetAt(i+1);
			BOOL b01 = 	((pC->bot[0].z) == (pN->top[0].z)) && ((pC->bot[1].z) == (pN->top[1].z));		
			BOOL b12 = 	((pC->bot[1].z) == (pN->top[1].z)) && ((pC->bot[2].z) == (pN->top[2].z));		
			BOOL b23 = 	((pC->bot[2].z) == (pN->top[2].z)) && ((pC->bot[3].z) == (pN->top[3].z));		
			BOOL b30 = 	((pC->bot[3].z) == (pN->top[3].z)) && ((pC->bot[0].z) == (pN->top[0].z));		
			if( (pC->l == 3 || pN->l == 3 ) &&
				(
				(pC->b[0] && pN->b[0] && pC->b[1] && pN->b[1] && b01 ) ||
				(pC->b[1] && pN->b[1] && pC->b[2] && pN->b[2] && b12 ) ||
				(pC->b[2] && pN->b[2] && pC->b[3] && pN->b[3] && b23 ) ||
				(pC->b[3] && pN->b[3] && pC->b[0] && pN->b[0] && b30 )   
				)
				)
			{
				if(pN->top[4].z == pN->bot[4].z)	{pN->top[4].z = pN->bot[4].z = pC->bot[4].z;}
				pN->top[4].z = pC->bot[4].z;
				if(pN->top[4].z < pN->bot[4].z)		pN->bot[4].z = pN->top[4].z;
			}
		}
		InitSideFacets(pC, &fctLst);
	}

	FGtoFCT( &fga, &fctLst, 0, 1 );
	FGtoFCT( &fga, &fctLst, 1, 2 );
	FGtoFCT( &fga, &fctLst, 2, 3 );
	FGtoFCT( &fga, &fctLst, 3, 0 );

	return (int)fctLst.GetCount();
}

void CModel::FGtoFCT(FctGenArray* fga, FacetList* pFctLst, int i0, int i1) 
{
	// fga MUST not be empty
	// i2 MUST be middle point
	int		i2 = 4;
	int		i, n;
	BOOL	bFirst=TRUE;
	FCTGENER *pC=NULL;	// current 
	FCTGENER *pN=NULL;	// next
	FCTGENER *pB=NULL;	// Bottom

	// fill given column
	n = (int)fga->GetSize();
	i=0;
	while( i < n ) {
		pC = &fga->GetAt(i);
		i++;
		if( pC->b[i0] && pC->b[i1] && (pC->top[i0]!=pC->bot[i0] || pC->top[i1]!=pC->bot[i1] || pC->top[i2]!=pC->bot[i2]) ) {
			if( bFirst ) {
				InitFacetTBM(pC, pFctLst, i0, i1, FG_TOPMOST);					// top-most facets
				//if( i == n)	InitFacetTBM(pC, pFctLst, i0, i1, FG_BOTMOST);		// bottom-most facets
				bFirst = FALSE;
			}
			pB = pC;
			while( i < n ) {
				pN = &fga->GetAt(i);
				if( pN->b[i0] && pN->b[i1] && (pN->top[i0]!=pN->bot[i0] || pN->top[i1]!=pN->bot[i1] || pN->top[i2]!=pN->bot[i2]) ) {
					InitFacet(pC, pN, pFctLst, i0, i1);	// generate facets
					pB = pN;
					break;
				}
				else i++;
			}
		}
	}
	// bottom-most facets
	if( pB )	InitFacetTBM(pB, pFctLst, i0, i1, FG_BOTMOST);
}

void CModel::InitFacet(FCTGENER *pU, FCTGENER *pL, FacetList* pFctLst, int i0, int i1)
{
	// inits facet and ads to the facet list
	// pU -> Upper 
	// pL -> Lower
	// pU && pL MUST be valid, NOT CHECKED!!!
	// i2 MUST be middle point
	// i0 && i1 MUST be valid points

	int i2 = 4;	
	CFacet3Pt fct;
	fct.pBody = GetBody( pU->id );
	
	// common facets
	if( pU->bot[i0]==pL->top[i0] && pU->bot[i1]==pL->top[i1] && pU->bot[i2]==pL->top[i2] ) 	{
		fct.pBodyOpos = GetBody( pU->id );
		fct.pBody = GetBody( pL->id );
		fct.Init( pL->top[i0], pL->top[i1], pL->top[i2] );
		pFctLst->AddTail( fct );
	}
	// facets are not common
	else {
		// bottom facet of upper FCTGEN
		if( pU->b[i0] && pU->b[i1] ) {
			fct.pBody = GetBody( pU->id );
			fct.Init( pU->bot[i0], pU->bot[i2], pU->bot[i1] );
			pFctLst->AddTail( fct );
		}
		// top facet lower FCTGEN
		if( pL->b[i0] && pL->b[i1] ) {
			fct.pBody = GetBody( pL->id );
			fct.Init( pL->top[i0], pL->top[i1], pL->top[i2] );
			pFctLst->AddTail( fct );
		}
	}
}

void CModel::InitFacetTBM(FCTGENER *pFg, FacetList* pFctLst, int i0, int i1, int nTag)
{
	// nTag -> differenciate top or bottom facets; MUST be FG_TOPMOST OR FG_BOTMOST
	// inits top-most facet
	// i2 MUST be middle point
	// i0 && i1 MUST be valid points
	int		i2 =4;
	CFacet3Pt fct;

	fct.pBody = GetBody( pFg->id );
	if( nTag == FG_TOPMOST )		fct.Init( pFg->top[i0], pFg->top[i1], pFg->top[i2] );
	if( nTag == FG_BOTMOST )		fct.Init( pFg->bot[i0], pFg->bot[i2], pFg->bot[i1] );
	pFctLst->AddTail( fct );

	return;
}

void CModel::InitSideFacets_2(FCTGENER *pFg, FacetList* pFctLst, int i0, int i1) 
{
	// avoid to generate boundary facets here
	if( pFg->b[0] && pFg->b[1] && pFg->top[0].y == m_yMin-m_dExS )	return;		// south
	if( pFg->b[2] && pFg->b[3] && pFg->top[2].y == m_yMax+m_dExN)	return;		// nord
	if( pFg->b[3] && pFg->b[0] && pFg->top[3].x == m_xMin-m_dExW )	return;		// west
	if( pFg->b[1] && pFg->b[2] && pFg->top[1].x == m_xMax+m_dExE )	return;		// east

	// i0 -> i1 must be CW as seen from the outside of the body
	if( pFg->b[i0] && pFg->b[i1] && ( (pFg->top[i0] != pFg->bot[i0]) || (pFg->top[i1] != pFg->bot[i1]) ) ) {
		CFacet3Pt fct;
		fct.nType = FCT_SIDE;
		CBody*	pBd = GetBody( pFg->id );
		fct.pBody = pBd;
		if( pFg->top[i0] != pFg->bot[i0] ) {
			fct.Init( pFg->top[i0], pFg->bot[i0], pFg->top[i1] );
			pFctLst->AddTail( fct );
		}
		if( pFg->top[i1] != pFg->bot[i1] ) {
			fct.Init( pFg->bot[i0], pFg->bot[i1], pFg->top[i1] );
			pFctLst->AddTail( fct );
		}
		// !!! add body boundary for Map View
		BDLIN bl;
		bl.n = pBd->GetIndex();
		bl.pt = pFg->bot[i0];
		m_pBodyBoundCol->Add( bl );
		bl.pt = pFg->bot[i1];
		m_pBodyBoundCol->Add( bl );
	}
}

void CModel::InitSideFacets_3(FCTGENER *pFg, FacetList* pFctLst, int i0, int i1) 
{
	// !!! add body boundary for Map View
	CBody* pBd = GetBody( pFg->id );
	BDLIN bl;
	bl.n = pBd->GetIndex();
	bl.pt = pFg->bot[i0];
	m_pBodyBoundCol->Add( bl );
	bl.pt = pFg->bot[i1];
	m_pBodyBoundCol->Add( bl );

	// i0 -> i1 must be CW as seen from the outside of the body
	if( (pFg->top[i0] != pFg->bot[i0]) || (pFg->top[i1] != pFg->bot[i1]) ) {
		CFacet3Pt fct;
		fct.nType = FCT_SIDE;
		fct.pBody = pBd;
		if( pFg->top[i0] != pFg->bot[i0] ) {
			fct.Init( pFg->top[i0], pFg->bot[i0], pFg->top[i1] );
			pFctLst->AddTail( fct );
		}
		if( pFg->top[i1] != pFg->bot[i1] ) {
			fct.Init( pFg->bot[i0], pFg->bot[i1], pFg->top[i1] );
			pFctLst->AddTail( fct );
		}
	}
}

void CModel::InitSideFacets(FCTGENER *pFg, FacetList* pFctLst) 
{
	if( pFg->l == 2 ) {
		// the facet is seen from the outside !!!
		InitSideFacets_2(pFg, pFctLst, 1, 0);
		InitSideFacets_2(pFg, pFctLst, 2, 1);
		InitSideFacets_2(pFg, pFctLst, 3, 2);
		InitSideFacets_2(pFg, pFctLst, 0, 3);
	}
	if( pFg->l == 3 ) {
		if( pFg->b[0] && pFg->b[1] && pFg->b[2] ) {
			InitSideFacets_3(pFg, pFctLst, 2, 4);
			InitSideFacets_3(pFg, pFctLst, 4, 0);
		}
		if( pFg->b[1] && pFg->b[2] && pFg->b[3] ) {
			InitSideFacets_3(pFg, pFctLst, 3, 4);
			InitSideFacets_3(pFg, pFctLst, 4, 1);
		}
		if( pFg->b[2] && pFg->b[3] && pFg->b[0] ) {
			InitSideFacets_3(pFg, pFctLst, 0, 4);
			InitSideFacets_3(pFg, pFctLst, 4, 2);
		}
		if( pFg->b[3] && pFg->b[0] && pFg->b[1] ) {
			InitSideFacets_3(pFg, pFctLst, 1, 4);
			InitSideFacets_3(pFg, pFctLst, 4, 3);
		}
	}
}

void CModel::FGtoFCTbound(FCTGENER *pFg, FacetList* pFctLst) 
{
	if( pFg->l == 2 ) return;

	if( pFg->b[0] && pFg->b[1] && pFg->top[0].y == m_yMin-m_dExS ) 
		InitSideFacetsBound(pFg, pFctLst, 0, 1);	// south	
	if( pFg->b[2] && pFg->b[3] && pFg->top[2].y == m_yMax+m_dExN) 
		InitSideFacetsBound(pFg, pFctLst, 2, 3);	// north
	if( pFg->b[3] && pFg->b[0] && pFg->top[3].x == m_xMin-m_dExW ) 
		InitSideFacetsBound(pFg, pFctLst, 3, 0);	// west
	if( pFg->b[1] && pFg->b[2] && pFg->top[1].x == m_xMax+m_dExE ) 
		InitSideFacetsBound(pFg, pFctLst, 1, 2);	// east
}


void CModel::InitSideFacetsBound(FCTGENER *pFg, FacetList* pFctLst, int i0, int i1) 
{
	// i0 -> i1 must be CW as seen from the outside of the body
	/* 
		i0_top  ->  i1_top
		   |		  |
		   |     	  |
		   |		  |
		i0_top  <-  i1_top
	*/

	// !!! add body boundary for Map View
	CBody* pBd = GetBody( pFg->id );
	BDLIN bl;
	bl.n = pBd->GetIndex();
	bl.pt = pFg->bot[i0];
	m_pBodyBoundCol->Add( bl );
	bl.pt = pFg->bot[i1];
	m_pBodyBoundCol->Add( bl );

	if( (pFg->top[i0] == pFg->bot[i0]) && (pFg->top[i1] == pFg->bot[i1])  ) 	return;

	CFacet3Pt fct;
	fct.pBody = pBd;
	if( pFg->top[i0] != pFg->bot[i0] ) {
		fct.Init( pFg->top[i0], pFg->bot[i0], pFg->top[i1] );
		pFctLst->AddTail( fct );
	}
	if( pFg->top[i1] != pFg->bot[i1] ) {
		fct.Init( pFg->bot[i0], pFg->bot[i1], pFg->top[i1] );
		pFctLst->AddTail( fct );
	}
}



/////////////////////////////////////////////////////////////////////////////////////////////
//*********** generate GL facets ************************************************************
void CModel::UpdateGLFacets(void)
{
	for(int i=0; i<m_nRows-1; i++) {
		for(int j=0; j<m_nCols-1; j++) {
			UpdateGLFacets(i, j);
			UpdateGLSelPt(i, j);
		}
	}
}

void CModel::UpdateGLFacets(int row, int col)
{
	int	n;					// 1D array index
	int m;					// number of facets in facet list
	int i;
	FacetList*	pFctLst;	// facet list for given row, col
	DoubleArray* pFa;		// 
	POSITION	pos;
	CFacet3Pt*	pFacet;
	double*		pF;

	n = GetIndex(row, col);
	if(n==-1) return;

	pFctLst = GetFacetList(row, col);
	m = pFctLst->GetCount();
	pFa = &m_glFacets.GetAt(n);
	pFa->RemoveAll();
	if( m == 0 )	return;


	// generate new facets and fill arrays
	pFa->SetSize(m*20, 0);
	pF  = pFa->GetData();
	i = 0;
	pos = pFctLst->GetHeadPosition();
	while(pos) {
		pFacet = &pFctLst->GetNext(pos);
		if( !pFacet->IsNull() ) {
			// triangle facet points in GL coordinates
			pF[i + 0] = pFacet->pts[0].x;
			pF[i + 1] = pFacet->pts[0].z;
			pF[i + 2] = -pFacet->pts[0].y;
			pF[i + 3] = pFacet->pts[1].x;
			pF[i + 4] = pFacet->pts[1].z;
			pF[i + 5] = -pFacet->pts[1].y;
			pF[i + 6] = pFacet->pts[2].x;
			pF[i + 7] = pFacet->pts[2].z;
			pF[i + 8] = -pFacet->pts[2].y;

			// normal vector to facet in each point
			pF[i + 9] =  pFacet->v_n.x;
			pF[i + 10] = pFacet->v_n.z;
			pF[i + 11] = -pFacet->v_n.y;

			// inner facet colors
			if( pFacet->pBodyOpos != NULL ) {
				if( pFacet->pBodyOpos->IsVisisble() &&  pFacet->pBodyOpos->IsVisibleTransparent() ) {
					// color from inside of body
					pFacet->pBodyOpos->GetColorFill(pF[i + 12], pF[i + 13], pF[i + 14], pF[i + 15]);
					// color from outside of body
					pFacet->pBody->GetColorFill(pF[i + 16], pF[i + 17], pF[i + 18], pF[i + 19]);
				}
				else {
					pFacet->pBody->GetColorFill(pF[i + 12], pF[i + 13], pF[i + 14], pF[i + 15]);
					pFacet->pBody->GetColorFill(pF[i + 16], pF[i + 17], pF[i + 18], pF[i + 19]);
				}
			}
			else {
				pFacet->pBody->GetColorFill(pF[i + 12], pF[i + 13], pF[i + 14], pF[i + 15]);
				pFacet->pBody->GetColorFill(pF[i + 16], pF[i + 17], pF[i + 18], pF[i + 19]);
			}

			// incremenet i
			i += 20;
		}
	}

	pFa->SetSize(i, 0);
}

void CModel::UpdateGLSelPt(int row, int col)
{
	int		nIndex;					// 1D array index
	int		nMax;
	DoubleArray* pAr;		// 
	double*		pData;

	nIndex = GetIndex(row, col);
	ASSERT(nIndex != -1);
	if( nIndex==-1 )	return;

	pAr = &m_glSelPt.GetAt(nIndex);
	pAr->RemoveAll();

	nMax = GetCount(row, col);
	if( nMax==0 || nMax==2 )		return;

	double		z, zTmp;		// z value
	int			ii, i, j;
	CClmnPt*	pClPt;
	CBody* pBody;

	pAr->SetSize( 13*nMax );
	pData = pAr->GetData();
	//nMax = pAr->GetCount();
	for( i=1,ii=0; i<nMax-1; i++, ii++) {
		pClPt = GetAt(row, col, i);
		z = pClPt->m_z;						//z = GetZ(row, col, i);
		j = i+1;
		// fill the GL Array
		pData[ii*13 + 0] = pClPt->m_pt.x;
		pData[ii*13 + 1] = pClPt->m_pt.z;
		pData[ii*13 + 2] = -pClPt->m_pt.y;
		pData[ii*13 + 3] = pClPt->m_nBodyID;
		pData[ii*13 + 4] = row;
		pData[ii*13 + 5] = col;
		pData[ii*13 + 6] = nIndex;
		pData[ii*13 + 7] = i;
		pData[ii*13 + 8] = ii;
		//pBody = (CBody*)pClPt->m_pBody;
		pBody = GetBody(pClPt->m_nBodyID);
		pBody->GetColor(pData[ii*13 + 9], pData[ii*13 + 10], pData[ii*13 + 11], pData[ii*13 + 12]);
		while( j < nMax ) {
			zTmp = GetZ(row, col, j);
			if( z >  zTmp) 
				break;
			j++;
			i++;
		}
	}
	pAr->SetSize(ii*13);
}

DDArray* CModel::GetGLFacets()
{
	return	&m_glFacets;
}

DDArray* CModel::GetGLSelPt()
{
	return	&m_glSelPt;
}

void CModel::ComputeBodyMagnetizationVector(CPoint3D indFld)
{
	for(int i=0; i<m_bodies.GetSize(); i++) {
		m_bodies.GetAt(i)->ComputeMagnetizationVector(indFld);
	}
}

int CModel::IsBodyArrayEmpty(void)
{
	return m_bodies.IsEmpty();
}


int CModel::PropertiesBody(int nID)
{
	CBody* pBody;

	pBody = GetBody(nID);
	if( pBody )			pBody->Properties();
	UpdateGLFacets();

	return 0;
}


// imports body from grid file
int CModel::ImportBody(void)
{
	CDlgModelImportBody dlg;

	// model grid
	dlg.m_nCols = m_nCols-2;
	dlg.m_nRows = m_nRows-2;
	dlg.m_x0 = m_x0;
	dlg.m_y0 = m_y0;
	dlg.m_xSize = m_xSize;
	dlg.m_ySize = m_ySize;
	// tags
	dlg.m_nTagUB = m_nImportUB;
	dlg.m_nTagLB = m_nImportLB;
	dlg.m_dUB = m_dImportUB;
	dlg.m_dLB = m_dImportLB;
	dlg.m_dDumMin = m_zMin;
	dlg.m_dDumMax = m_zMax;

	if( dlg.DoModal() == IDOK ) {
		// store model creattion properties
		int		nCreationTag = m_nCreationTag;			
		BOOL	bConstTop = m_bConstTop;				
		double	dConstTop = m_dConstTop;
		BOOL	bConstBot = m_bConstBot;				
		double	dConstBot = m_dConstBot;

		// set body creation properties for import
		// upper boundary
		switch( dlg.m_nTagUB ) {
			case BDI_GRID:
				m_nCreationTag = BCR_NONE;
				m_bConstTop = FALSE;
				break;
			case BDI_CONST:
				m_nCreationTag = BCR_NONE;
				m_bConstTop = TRUE;
				m_dConstTop = dlg.m_dConstUB;
				break;
			case BDI_COPY:
				m_nCreationTag = BCR_NONE;
				m_bConstTop = FALSE;
				break;
			case BDI_JOIN:
				m_nCreationTag = BCR_JOINTOP;
				break;
		}
		// lower boundary
		switch( dlg.m_nTagLB ) {
			case BDI_GRID:
				m_nCreationTag = BCR_NONE;
				m_bConstBot = FALSE;
				break;
			case BDI_CONST:
				m_nCreationTag = BCR_NONE;
				m_bConstBot = TRUE;
				m_dConstBot = dlg.m_dConstLB;
				break;
			case BDI_COPY:
				m_nCreationTag = BCR_NONE;
				m_bConstBot = FALSE;
				break;
			case BDI_JOIN:
				m_nCreationTag = BCR_JOINBOT;
				break;
		}

		CGrid* pGrdU;	// upper grid	
		CGrid* pGrdL;	// lower grid
		
		pGrdU = &dlg.m_grdUp;
		pGrdL = &dlg.m_grdDwn;

		// ceck grid for dummies
		if(!pGrdU->IsEmpty()) 
			pGrdU->CheckDummies(dlg.m_dDumMin, dlg.m_dDumMax, TRUE);
		if(!pGrdL->IsEmpty()) 
			pGrdL->CheckDummies(dlg.m_dDumMin, dlg.m_dDumMax, TRUE);
		
		// import body
		CBody*	pBd;	// new body
		int		nID;	// new body ID
		double u, l;
		double **pU = pGrdU->GetData();
		double **pL = pGrdL->GetData();
		pBd = NewBody();
		nID = pBd->GetID();
		for(int i = 0; i < dlg.m_nRows; i++) {
			for(int j = 0; j < dlg.m_nCols; j++) {
				// upper boundary
				switch( dlg.m_nTagUB ) {
					case BDI_GRID:
						u = pU[i][j];
						break;
					case BDI_COPY:
						u = pL[i][j]+dlg.m_dUB;
						break;
					case BDI_JOIN:
						u = m_zMax;
						break;
				}

				// bottom boundary
				switch( dlg.m_nTagLB ) {
					case BDI_GRID:
						l = pL[i][j];
						break;
					case BDI_COPY:
						l = pU[i][j]-dlg.m_dLB;
						break;
					case BDI_JOIN:
						l = m_zMin;
						break;
				}
				if(u!=DUMMY && l!=DUMMY)
					InsertBody(i+1, j+1, 0.0, 0.0, FALSE, nID, u, l, FALSE);
			} // for(j)
		}	// for(i)
		// restore model creattion properties
		m_nCreationTag = nCreationTag;			
		m_bConstTop = bConstTop;				
		m_dConstTop = dConstTop;
		m_bConstBot = bConstBot;				
		m_dConstBot = dConstBot;
		pBd->Properties();
		return 1;
	}
	return 0;

}

int CModel::ExportBody(int nID)
{
	double	u, l;
	CBody*	pBd;
	CGrid	grdU;
	CGrid	grdL;
	CString	strU="UpperGrid.grd", strL="LowerGrid.grd";

	pBd = GetBody(nID);
	if(pBd == NULL)	{
		nID = SelectBody();
		pBd = GetBody(nID);
	}
	if(pBd == NULL)	return 0;

	grdU.New(m_nRows-2, m_nCols-2, m_x0, m_y0, m_xSize, m_ySize);
	grdU.SetValue(DUMMY);
	grdL.New(m_nRows-2, m_nCols-2, m_x0, m_y0, m_xSize, m_ySize);
	grdL.SetValue(DUMMY);

	for(int i = 1; i < m_nRows-2; i++) {
		for(int j = 1; j < m_nCols-2; j++) {
			if( GetBodyTopBot(nID, i, j, u, l) != -1 ) {
				grdU.SetValue(i-1, j-1, u);
				grdL.SetValue(i-1, j-1, l);
			}
		} // for(j)
	}	// for(i)


	//grdU.SetFilePath(strU);
	grdU.SaveAs();
	//grdL.SetFilePath(strL);
	grdL.SaveAs();

	return 0;
}

