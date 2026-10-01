// Mod3DDocComputing.cpp : implementation of the CMod3DDoc class
//
// computation functions are in this file


#include "stdafx.h"
#include "Mod3D.h"
#include "Mod3DDoc.h"
#include <math.h>
#include "afxmt.h"
extern long g_nTotComp;	// rows*num_ofBodies computation prcentage
extern long g_nComp;	


UINT ComputeFldThread(LPVOID pParam) 
{
	CCriticalSection cs;
	cs.Lock();
	// therad computing
	CMod3DDoc*	pDoc = (CMod3DDoc*) pParam;

	// get facets for computation
	FacetList	fctLst;
	CModel*	pMod = pDoc->GetModel();
	pMod->ComputeBodyMagnetizationVector( pDoc->GetInducedField() );
	int n = pMod->GetFacetsComputation( fctLst );
	if( n == 0 ) {
		AfxMessageBox("The active model is empty. Computation canceled.", MB_OK | MB_ICONINFORMATION);
		CMDIFrameWnd* pFrameWnd = (CMDIFrameWnd*) AfxGetMainWnd();
		pFrameWnd->SendMessage(WM_COMPUTETHREADFINISHED, (WPARAM)pDoc, WM_COMPUTETHREADCANCELED);
		return 0;
	}

	pDoc->ZeroModelGrids();
	int nRet = pDoc->ComputeField( &fctLst );

	if( nRet == 0 ) {
		CMDIFrameWnd* pFrameWnd = (CMDIFrameWnd*) AfxGetMainWnd();
		pFrameWnd->SendMessage(WM_COMPUTETHREADFINISHED, (WPARAM)pDoc, WM_COMPUTETHREADCANCELED);
		return 0;
	}

	// done :-))
	cs.Unlock();

	for(int i=0; i<3; i++) {
		MessageBeep( -1 );
		Sleep( 300 );
	}

	CMDIFrameWnd* pFrameWnd = (CMDIFrameWnd*) AfxGetMainWnd();
	pFrameWnd->SendMessage(WM_COMPUTETHREADFINISHED, (WPARAM)pDoc, WM_COMPUTETHREADFINISHED);

	return 1;
}

void CMod3DDoc::ComputeAmbientField()
{
	m_vMagH = AmbientField(m_vMagIndFld.x, m_vMagIndFld.y, m_vMagIndFld.z);
}

void CMod3DDoc::ComputeTotalField()
{
	// compute delta T,
	double	**pMx, **pMy, **pMz, **p_dT;
	double	**pGx, **pGy, **pGz, **pG;
	BOOL	bMag, bGrv;

	pGx =	GetGridDataComp(GRDMOD_GX);
	pGy =	GetGridDataComp(GRDMOD_GY);
	pGz =	GetGridDataComp(GRDMOD_GZ);
	pG =	GetGridDataComp(GRDMOD_G);
	pMx =	GetGridDataComp(GRDMOD_MX);
	pMy =	GetGridDataComp(GRDMOD_MY);
	pMz =	GetGridDataComp(GRDMOD_MZ);
	p_dT =	GetGridDataComp(GRDMOD_M);

	bMag = pMx && pMy && pMz;
	bGrv = pGx && pGy && pGz;
	if( !bMag && !bGrv )
		return;

	ComputeAmbientField();
	for(int i = 0; i < m_obsHdr.nRows; i++) {
		for(int j = 0; j < m_obsHdr.nCols; j++) {
			if( bMag ) {
				if( pMx[i][j] != DUMMY) {
					double fMagTot = sqrt( pow((pMx[i][j]+m_vMagH.x),2) + pow((pMy[i][j]+m_vMagH.y),2) + pow((pMz[i][j]+m_vMagH.z),2));
					double dT = fMagTot-m_vMagIndFld.z;	
					p_dT[i][j]  = -dT;
				}
			}
			if( bGrv ) {
				if( pGx[i][j] != DUMMY) {
					pG[i][j] = sqrt( pow(pGx[i][j], 2) + pow(pGy[i][j], 2) + pow(pGz[i][j], 2) ); }
			}
		}
	}
}

int SubstractGrids( CGrid* pRes, CGrid* pGrd1, CGrid* pGrd2 ) 
{
	double		**pG1, **pG2, **pR;
	GridInfo	gi;

	gi = pGrd2->GetInfo();
	if( pGrd1->IsSameSize( gi ) && pRes->IsSameSize( gi) ) {
		pG1 = pGrd1->GetData();
		pG2 = pGrd2->GetData();
		pR = pRes->GetData();
		if( pG1 && pG2 && pR ) {
			for(DWORD i = 0; i < gi.nRow; i++) {
				for(DWORD j = 0; j < gi.nCol; j++) {
					if(pG1[i][j]!=DUMMY && pG2[i][j]!=DUMMY)
						pR[i][j] = pG1[i][j] - pG2[i][j];
					else
						pR[i][j] = DUMMY;
				}
			}
		}
	}
	return 0;
}

int CMod3DDoc::ComputeDifferenceField(void)
{
	for(int n=m_nModComponents; n<m_nDifComponents; n++) {
		if( m_grdActiveCompute[n] ) {
			SubstractGrids( &m_grd[n], &m_grd[n-20], &m_grd[n+20] );
			m_grd[n].SetModifiedFlag();
		}
	}
	return 0;
}

int CMod3DDoc::RemoveMeanDif(void)
{
	if( m_bRemoveMeanGrv ) {
		for(int n=GRDDIF_GX; n<=GRDDIF_G; n++) {
			if( m_grdActiveCompute[n] ) {
				double m=m_grd[n].GetMean();
				m_grd[n-20] -= m;
				m_grd[n].SetModifiedFlag();
				SubstractGrids( &m_grd[n], &m_grd[n-20], &m_grd[n+20] );
				m_grd[n].SetModifiedFlag();
			}
		}
	}

	if( m_bRemoveMeanTns ) {
		for(int n=GRDDIF_GXX; n<=GRDDIF_GYZ; n++) {
			if( m_grdActiveCompute[n] ) {
				double m=m_grd[n].GetMean();
				m_grd[n-20] -= m;
				m_grd[n].SetModifiedFlag();
				SubstractGrids( &m_grd[n], &m_grd[n-20], &m_grd[n+20] );
				m_grd[n].SetModifiedFlag();
			}
		}
	}

	if( m_bRemoveMeanMag ) {
		for(int n=GRDDIF_MX; n<=GRDDIF_M; n++) {
			if( m_grdActiveCompute[n] ) {
				double m=m_grd[n].GetMean();
				m_grd[n-20] -= m;
				m_grd[n].SetModifiedFlag();
				SubstractGrids( &m_grd[n], &m_grd[n-20], &m_grd[n+20] );
				m_grd[n].SetModifiedFlag();
			}
		}
	}

	return 0;
}



int CMod3DDoc::UpdateField(BOOL bUpdateViews)
{
	TRACE("Entering UpdateField() \n");

	if( m_nComputationType != CMP_REALTIME ) return 0;

	if( m_pCompThread != NULL ) {
		AfxMessageBox("Computation is running. Real-time computing is disabled.", MB_OK | MB_ICONINFORMATION);
		return 0;
	}

	// get update facets
	FacetList* pFctLst;
	pFctLst = m_model.GetFacetsUpdate();

	if( pFctLst->IsEmpty() ) return 0;

	// show wait cursor
	CWaitCursor wait;

	// preprocessing - remove equal facets
	POSITION pos1, pos2;
	CFacet3Pt	*pFct, *pFct2;
	pos1 = pFctLst->GetHeadPosition();
	while(pos1) {
		pFct = &pFctLst->GetNext(pos1);
		if( !pFct->IsNull() ) {
			pos2 = pos1;
			while(pos2) {
				pFct2 = &pFctLst->GetNext(pos2);
				if( pFct == pFct2 && pFct->dSign == -pFct2->dSign ) {
					pFct->SetType( FCT_NULL );
					pFct2->SetType( FCT_NULL );
				}
			}
		}
	}

	if( pFctLst->IsEmpty() )  {
		wait.Restore();
		return 0;
	}

	ComputeField( pFctLst, FALSE );
	
	// !!! remove all facets
	pFctLst->RemoveAll();

	ComputeTotalField();
	ComputeDifferenceField();
	UpdateMinMaxFldValModified();
	SetModifiedFlag();
	if( bUpdateViews )
		UpdateAllViews(NULL, UPDATE_COMPUTATION_FINISHED);
	
	wait.Restore();
	return 1;
}


int CMod3DDoc::ComputeField(FacetList* pFctLst, BOOL bZeroVal)
{
	CCriticalSection cs;
	cs.Lock();

	CFacet3Pt*	pFacet;
	POSITION	pos;
	OBSHDR		obsHdr;
	CPoint3D	v_rGrv, v_rMag, v_rGrvT, fld;
	double		**gx, **gy, **gz, **mx, **my, **mz;
	double		**gxx, **gyy, **gzz, **gxy, **gxz, **gyz;
	double		*vgx, *vgy, *vgz, *vmx, *vmy, *vmz;
	double		*vgxx, *vgyy, *vgzz, *vgxy, *vgxz, *vgyz;
	int			nCols, nRows;			// number of grid rows, columns
	double		x0, y0, xSize, ySize;
	int			nRdm;
	BOOL		bGrv, bGrvT, bMag;		// compute or not
	CPoint3D	vRefDensGrad;
	CPoint3D	vRefDensOrig;
	double		dRefDensOrg;
	double		dRefDensity;
	double		dUnitGrv, dUnitMag, dUnitGrvT;
	GridInfo	grdInf;
	GRVTENS		gradInf;

	// grids header !!! the same for all grids
	grdInf = GetObservationGrid()->GetInfo();
	gradInf = GetGradStruct();
	nCols = grdInf.nCol;
	nRows = grdInf.nRow;
	x0 = grdInf.x0;
	y0 = grdInf.y0;
	xSize = grdInf.xSize;
	ySize = grdInf.ySize;

	// model grids data
	gx	= GetGridDataComp( GRDMOD_GX );	vgx = NULL;
	gy	= GetGridDataComp( GRDMOD_GY );	vgy	= NULL;
	gz	= GetGridDataComp( GRDMOD_GZ );	vgz	= NULL;
	mx	= GetGridDataComp( GRDMOD_MX );	vmx	= NULL;
	my	= GetGridDataComp( GRDMOD_MY );	vmy	= NULL;
	mz	= GetGridDataComp( GRDMOD_MZ );	vmz	= NULL;
	gxx	= GetGridDataComp( GRDMOD_GXX );	vgxx = NULL;
	gyy	= GetGridDataComp( GRDMOD_GYY );	vgyy = NULL;
	gzz	= GetGridDataComp( GRDMOD_GZZ );	vgzz = NULL;
	gxy	= GetGridDataComp( GRDMOD_GXY );	vgxy = NULL;
	gxz	= GetGridDataComp( GRDMOD_GXZ );	vgxz = NULL;
	gyz	= GetGridDataComp( GRDMOD_GYZ );	vgyz = NULL;

	bGrv		= gx || gy || gz;
	bGrvT		= gxx || gyy || gzz || gxy || gxz || gyz;
	bMag		= mx || my || mz;

	dUnitGrv	= GetGravUnits();
	dUnitMag	= GetMagUnits();
	dUnitGrvT	= GetGravTUnits();
	dRefDensity = GetReferenceDensity();
	dRefDensOrg = GetRefDensAtOrigo();
	vRefDensGrad = GetDensityGradient();
	if( vRefDensGrad.IsZero() ) nRdm = RDM_NORMAL;
	else						nRdm = RDM_LINEAR;
	
	m_model.ComputeBodyMagnetizationVector( GetInducedField() );
	if( !bGrv && !bGrvT && !bMag ) {
		AfxMessageBox("No active field is selected. Computation canceled.", MB_OK | MB_ICONINFORMATION);
		cs.Unlock();
		return 0;
	}

	g_nTotComp = nRows*nCols;
	g_nComp = 0;

	double	**obsGrv, **obsMag, **obsGrvT; 
	double	x, y;
	BOOL	bCompute;
	obsGrv = GetObservGrv()->GetData();
	obsMag = GetObservMag()->GetData();
	obsGrvT = GetObservTns()->GetData();

	// move over observation grids
	for(int i = 0; i < nRows; i++) {
		for(int j = 0; j < nCols; j++) {
			bCompute = FALSE;
			x = x0+j*xSize;
			y = y0+i*ySize;

			if( bGrv && obsGrv[i][j]!=DUMMY ) {
				v_rGrv.x = x;
				v_rGrv.y = y;
				v_rGrv.z = obsGrv[i][j];
				if(gx!=NULL)	{vgx = &gx[i][j];	if(bZeroVal) *vgx = 0.0;	bCompute |= TRUE;}
				if(gy!=NULL)	{vgy = &gy[i][j];	if(bZeroVal) *vgy = 0.0;	bCompute |= TRUE;}
				if(gz!=NULL)	{vgz = &gz[i][j];	if(bZeroVal) *vgz = 0.0;	bCompute |= TRUE;}
			}
			else {
				vgx=NULL;	vgy=NULL;	vgz=NULL;
			}

			if( bMag && obsMag[i][j]!=DUMMY ) {
				v_rMag.x = x+0.001;
				v_rMag.y = y+0.003;
				v_rMag.z = obsMag[i][j];
				if(mx!=NULL)	{vmx = &mx[i][j];	if(bZeroVal) *vmx = 0.0;	bCompute |= TRUE;}
				if(my!=NULL)	{vmy = &my[i][j];	if(bZeroVal) *vmy = 0.0;	bCompute |= TRUE;}
				if(mz!=NULL)	{vmz = &mz[i][j];	if(bZeroVal) *vmz = 0.0;	bCompute |= TRUE;}
			}
			else {
				vmx=NULL;	vmy=NULL;	vmz=NULL;
			}

			if( bGrvT && obsGrvT[i][j]!=DUMMY ) {
				v_rGrvT.x = x;
				v_rGrvT.y = y;
				v_rGrvT.z = obsGrvT[i][j];
				if(gxx!=NULL)	{vgxx = &gxx[i][j];	if(bZeroVal) *vgxx = 0.0;	bCompute |= TRUE;}
				if(gyy!=NULL)	{vgyy = &gyy[i][j];	if(bZeroVal) *vgyy = 0.0;	bCompute |= TRUE;}
				if(gzz!=NULL)	{vgzz = &gzz[i][j];	if(bZeroVal) *vgzz = 0.0;	bCompute |= TRUE;}
				if(gxy!=NULL)	{vgxy = &gxy[i][j];	if(bZeroVal) *vgxy = 0.0;	bCompute |= TRUE;}
				if(gxz!=NULL)	{vgxz = &gxz[i][j];	if(bZeroVal) *vgxz = 0.0;	bCompute |= TRUE;}
				if(gyz!=NULL)	{vgyz = &gyz[i][j];	if(bZeroVal) *vgyz = 0.0;	bCompute |= TRUE;}
			}
			else {
				vgxx=NULL;	vgyy=NULL;	vgzz=NULL;	vgxy=NULL;	vgxz=NULL;	vgyz=NULL;
			}

			if( bCompute) {
				pos = pFctLst->GetHeadPosition();
				while(pos) {
					pFacet = &pFctLst->GetNext(pos);
					pFacet->Compute( 
						vgx,	vgy,	vgz,	vgxx,	vgyy,	vgzz,	vgxy,	vgxz,	vgyz,
						vmx,	vmy,	vmz,
						dRefDensity,	dRefDensOrg,	vRefDensGrad,	nRdm,
						v_rGrv,			v_rGrvT,			v_rMag,
						dUnitGrv,		dUnitMag,		dUnitGrvT);

				}	// while( pos); facet list
			}
			g_nComp++;

		} //for(j)
	} //for(i)
	//----------------------------------------------------------------------------------------------------------------

	// done :-))
	cs.Unlock();

	return 1;
}
