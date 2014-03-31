// Mod3DDoc.cpp : implementation of the CMod3DDoc class
//

#include <string.h>
#include "stdafx.h"
#include "Mod3D.h"
#include "Mod3DDoc.h"
#include "VersionType.h"

/////////////////////////////////////////////////////////////////////////////
// CMod3DDoc serialization

// CURRENT VERSION ONLY
//void CMod3DDoc::Serialize_20040419(CArchive& ar)
void CMod3DDoc::Serialize(CArchive& ar)
{
	OBSHDR *oh = &m_obsHdr;
	char hdr[256];

	if( ar.IsStoring() ) {
		// compute relative paths for pictures...
		ComputeRelativePaths();
		CString str;
		str.Format("Mod3D %d", m_nVersion);
		strcpy(hdr, str);
		// observation header
		ar.Write(hdr, 256);
		ar	<< m_nVersion;
		ar	<< m_nID
			<< oh->nRows << oh->nCols 
			<< oh->x0 << oh->y0 
			<< oh->xSize << oh->ySize 
			<< oh->dMinX << oh->dMaxX 
			<< oh->dMinY << oh->dMaxY 
			<< oh->dMinZ << oh->dMaxZ;
		CheckDemo();
		// computation
		ar	<< m_nComputationType;
		ar	<< m_bSherComp;

		// gravity
		ar << m_nGrvFormula;
		ar << m_dGrvDensRef << m_dGrvUnits;
		ar << m_vGrvDensGrad.x << m_vGrvDensGrad.y << m_vGrvDensGrad.z;
		ar << m_vGrvDensOrigo.x << m_vGrvDensOrigo.y << m_vGrvDensOrigo.z;
		ar << m_gvrTens.nTag << m_gvrTens.bCompute << m_gvrTens.dFlightElev << m_gvrTens.dHeight << m_gvrTens.dUnits;
		ar << m_dGrvSens << m_dGrvElev;
		ar << m_nGrvObsTag;
		ar << m_bRemoveMeanGrv;
		ar << m_bRemoveMeanTns;

		// magnetic
		ar	<< m_nMagFormula;
		ar	<< m_dMagSens << m_dMagElev;
        ar	<< m_vMagIndFld.x << m_vMagIndFld.y << m_vMagIndFld.z;
		ar	<< m_nMagObsTag;	
		ar << m_bRemoveMeanMag;

		// fitting parameters
		ar  << m_nFitFld;
		ar	<< m_nFitVrtxChar << m_nFitVrtxMaxIter << m_nFitVrtxMeth;
		ar	<< m_dFitVrtxEps << m_dFitVrtxEpsAuto << m_dFitVrtxTol;
		ar  << m_bFitVrtxEpsAuto << m_bFitVrtxLog;
		ar	<< m_nFitDensChar << m_nFitDensMaxIter << m_nFitDensMeth;
		ar	<< m_dFitDensEps << m_dFitDensTol;
		ar	<< m_bFitDensLog;

		m_model.Serialize( ar );
		m_model.SetComputeRealTime( m_nComputationType );
		m_objPtrArray.Serialize( ar );

		// grids
		m_ObsRel.Serialize( ar );
		m_ObsGrv.Serialize( ar );
		m_ObsMag.Serialize( ar );
		m_ObsTns.Serialize( ar );
		for(int i =0; i < m_nAllComponents; i++) {
			m_grd[i].Serialize(ar);
			ar << m_grdActiveCompute[i];
		}

		//setting for views
		m_view3dSet.Serialize(ar);
		m_viewPrfSet.Serialize(ar);
	}
	else {
		//----- remove in old verions ---------------------------------------------
		int nVersion;
		ar.Read(hdr, 256);
		if(strstr(hdr,"Mod3D 2.001") || strstr(hdr,"Mod3D 2.00") ) {
			SerializeOld(ar);
			return;
		}
		ar	>> nVersion;
		if(nVersion < m_nVersion) {
			SerializeLoad(ar, nVersion);
			return;
		}
		//-------------------------------------------------------------------------

		ar	>> m_nID
			>> oh->nRows >> oh->nCols 
			>> oh->x0 >> oh->y0 
			>> oh->xSize >> oh->ySize 
			>> oh->dMinX >> oh->dMaxX 
			>> oh->dMinY >> oh->dMaxY 
			>> oh->dMinZ >> oh->dMaxZ;
		// computation
		ar	>> m_nComputationType;
		ar	>> m_bSherComp;

		// gravity
		ar >> m_nGrvFormula;
		ar >> m_dGrvDensRef >> m_dGrvUnits;
		ar >> m_vGrvDensGrad.x >> m_vGrvDensGrad.y >> m_vGrvDensGrad.z;
		ar >> m_vGrvDensOrigo.x >> m_vGrvDensOrigo.y >> m_vGrvDensOrigo.z;
		ar >> m_gvrTens.nTag >> m_gvrTens.bCompute >> m_gvrTens.dFlightElev >> m_gvrTens.dHeight >> m_gvrTens.dUnits;
		ar >> m_dGrvSens >> m_dGrvElev;
		ar >> m_nGrvObsTag;
		ar >> m_bRemoveMeanGrv;
		ar >> m_bRemoveMeanTns;

		// magnetic
		ar	>> m_nMagFormula;
		ar	>> m_dMagSens >> m_dMagElev;
        ar	>> m_vMagIndFld.x >> m_vMagIndFld.y >> m_vMagIndFld.z;
		ar	>> m_nMagObsTag;
		ar	>> m_bRemoveMeanMag;

		// fitting parameters
		ar  >> m_nFitFld;
		ar	>> m_nFitVrtxChar >> m_nFitVrtxMaxIter >> m_nFitVrtxMeth;
		ar	>> m_dFitVrtxEps >> m_dFitVrtxEpsAuto >> m_dFitVrtxTol;
		ar  >> m_bFitVrtxEpsAuto >> m_bFitVrtxLog;
		ar	>> m_nFitDensChar >> m_nFitDensMaxIter >> m_nFitDensMeth;
		ar	>> m_dFitDensEps >> m_dFitDensTol;
		ar	>> m_bFitDensLog;

		m_model.Serialize( ar );
		m_model.SetComputeRealTime( m_nComputationType );
		m_objPtrArray.Serialize( ar );

		// grids
		m_ObsRel.Serialize( ar );
		m_ObsGrv.Serialize( ar );
		m_ObsMag.Serialize( ar );
		m_ObsTns.Serialize( ar );
		for(int i =0; i < m_nAllComponents; i++) {
			m_grd[i].Serialize(ar);
			ar >> m_grdActiveCompute[i];
		}

		//setting for views
		m_view3dSet.Serialize(ar);
		m_viewPrfSet.Serialize(ar);

		// ----------- load images ---------------------------------------------
		for(int i=0; i<m_objPtrArray.GetCount(); i++) {
			CMyObject* pObj = m_objPtrArray.GetAt(i);
			if( pObj->IsKindOf( RUNTIME_CLASS(CImgReferenced) ) ){
				CImgReferenced* pIm = (CImgReferenced*) pObj;
				if( pIm->m_img.IsNull() ) {
					CString strPathRel, strPathDoc, strDest;
					strPathRel = pIm->m_strFilePathRelative;
					strPathDoc = ar.m_strFileName;
					PathRemoveFileSpec( strPathDoc.GetBuffer() );
					char buffer_1[MAX_PATH] = "";
					char *lpStr1;
					lpStr1 = buffer_1;

					strPathRel.Delete(0,3);
					PathCombine(lpStr1, strPathDoc, strPathRel);
					HRESULT hResult;
					hResult = pIm->m_img.Load(lpStr1);
					if (FAILED(hResult)) {
						CString fmt;
						fmt.Format("Load image failed: %s\nLoad new image?", lpStr1 );
						int nRet = AfxMessageBox(fmt, MB_YESNO | MB_ICONQUESTION);
						if( nRet == IDYES ) {
							pIm->Open();
						}
					}
					TRACE("%s", strDest);
				}
			}
		}
		// ---------------------------------------------------------------------
		AfxGetMainWnd()->SendMessage(WM_UPDATE_OBJMNG, (WPARAM)this, (LPARAM)1);
	}
	CheckDemo();

}

// loads old versions
void CMod3DDoc::SerializeLoad(CArchive& ar, int nVersion)
{
	switch(nVersion) {
		//case 20040419:
		//	Serialize_20040419(ar);
		//	break;
		case 20031113:
			Serialize_20031113(ar);
			break;
		case 20031105:
			Serialize_20031105(ar);
			break;
		case 20031129:
			Serialize_20031029(ar);
			break;
		case 20031121:
			Serialize_20031021(ar);
			break;
	}
}

void CMod3DDoc::Serialize_20031113(CArchive& ar)
//void CMod3DDoc::Serialize(CArchive& ar)
{
	OBSHDR *oh = &m_obsHdr;
	char hdr[256];

	if( ar.IsStoring() ) {
		// compute relative paths for pictures...
		ComputeRelativePaths();
		CString str;
		str.Format("Mod3D %d", m_nVersion);
		strcpy(hdr, str);
		// observation header
		ar.Write(hdr, 256);
		ar	<< m_nVersion;
		ar	<< m_nID
			<< oh->nRows << oh->nCols 
			<< oh->x0 << oh->y0 
			<< oh->xSize << oh->ySize 
			<< oh->dMinX << oh->dMaxX 
			<< oh->dMinY << oh->dMaxY 
			<< oh->dMinZ << oh->dMaxZ;
		CheckDemo();
		// computation
		ar	<< m_nComputationType;
		ar	<< m_bSherComp;

		// gravity
		ar << m_nGrvFormula;
		ar << m_dGrvDensRef << m_dGrvUnits;
		ar << m_vGrvDensGrad.x << m_vGrvDensGrad.y << m_vGrvDensGrad.z;
		ar << m_vGrvDensOrigo.x << m_vGrvDensOrigo.y << m_vGrvDensOrigo.z;
		ar << m_gvrTens.nTag << m_gvrTens.bCompute << m_gvrTens.dFlightElev << m_gvrTens.dHeight << m_gvrTens.dUnits;
		ar << m_dGrvSens << m_dGrvElev;
		ar << m_nGrvObsTag;

		// magnetic
		ar	<< m_nMagFormula;
		ar	<< m_dMagSens << m_dMagElev;
        ar	<< m_vMagIndFld.x << m_vMagIndFld.y << m_vMagIndFld.z;
		ar	<< m_nMagObsTag;	

		// fitting parameters
		ar  << m_nFitFld;
		ar	<< m_nFitVrtxChar << m_nFitVrtxMaxIter << m_nFitVrtxMeth;
		ar	<< m_dFitVrtxEps << m_dFitVrtxEpsAuto << m_dFitVrtxTol;
		ar  << m_bFitVrtxEpsAuto << m_bFitVrtxLog;
		ar	<< m_nFitDensChar << m_nFitDensMaxIter << m_nFitDensMeth;
		ar	<< m_dFitDensEps << m_dFitDensTol;
		ar	<< m_bFitDensLog;

		m_model.Serialize( ar );
		m_model.SetComputeRealTime( m_nComputationType );
		m_objPtrArray.Serialize( ar );

		// grids
		m_ObsRel.Serialize( ar );
		m_ObsGrv.Serialize( ar );
		m_ObsMag.Serialize( ar );
		m_ObsTns.Serialize( ar );
		for(int i =0; i < m_nAllComponents; i++) {
			m_grd[i].Serialize(ar);
			ar << m_grdActiveCompute[i];
		}

		//setting for views
		m_view3dSet.Serialize(ar);
		m_viewPrfSet.Serialize(ar);
	}
	else {
		////----- remove in old verions ---------------------------------------------
		//int nVersion;
		//ar.Read(hdr, 256);
		//if(strstr(hdr,"Mod3D 2.001") || strstr(hdr,"Mod3D 2.00") ) {
		//	SerializeOld(ar);
		//	return;
		//}
		//ar	>> nVersion;
		//if(nVersion < m_nVersion || nVersion==20031129 || nVersion==20031121) {
		//	SerializeLoad(ar, nVersion);
		//	m_ObsGrv.New(oh->nRows, oh->nCols, oh->x0, oh->y0, oh->xSize, oh->ySize);
		//	m_ObsRel.CopyData( &m_ObsGrv );
		//	m_ObsGrv += m_dGrvSens;
		//	m_ObsMag.New(oh->nRows, oh->nCols, oh->x0, oh->y0, oh->xSize, oh->ySize);
		//	m_ObsRel.CopyData( &m_ObsMag );
		//	m_ObsMag += m_dMagSens;
		//	m_ObsTns.New(oh->nRows, oh->nCols, oh->x0, oh->y0, oh->xSize, oh->ySize);
		//	m_ObsRel.CopyData( &m_ObsTns );
		//	m_ObsTns += m_gvrTens.dHeight;

		//	m_dGrvElev = oh->dMaxZ + m_dGrvSens;
		//	m_dMagElev = oh->dMaxZ + m_dMagSens;
		//	m_gvrTens.dFlightElev = oh->dMaxZ + m_gvrTens.dHeight;

		//	AfxGetMainWnd()->SendMessage(WM_UPDATE_OBJMNG, (WPARAM)this, (LPARAM)1);
		//	return;
		//}
		////-------------------------------------------------------------------------

		ar	>> m_nID
			>> oh->nRows >> oh->nCols 
			>> oh->x0 >> oh->y0 
			>> oh->xSize >> oh->ySize 
			>> oh->dMinX >> oh->dMaxX 
			>> oh->dMinY >> oh->dMaxY 
			>> oh->dMinZ >> oh->dMaxZ;
		// computation
		ar	>> m_nComputationType;
		ar	>> m_bSherComp;

		// gravity
		ar >> m_nGrvFormula;
		ar >> m_dGrvDensRef >> m_dGrvUnits;
		ar >> m_vGrvDensGrad.x >> m_vGrvDensGrad.y >> m_vGrvDensGrad.z;
		ar >> m_vGrvDensOrigo.x >> m_vGrvDensOrigo.y >> m_vGrvDensOrigo.z;
		ar >> m_gvrTens.nTag >> m_gvrTens.bCompute >> m_gvrTens.dFlightElev >> m_gvrTens.dHeight >> m_gvrTens.dUnits;
		ar >> m_dGrvSens >> m_dGrvElev;
		ar >> m_nGrvObsTag;

		// magnetic
		ar	>> m_nMagFormula;
		ar	>> m_dMagSens >> m_dMagElev;
        ar	>> m_vMagIndFld.x >> m_vMagIndFld.y >> m_vMagIndFld.z;
		ar	>> m_nMagObsTag;

		// fitting parameters
		ar  >> m_nFitFld;
		ar	>> m_nFitVrtxChar >> m_nFitVrtxMaxIter >> m_nFitVrtxMeth;
		ar	>> m_dFitVrtxEps >> m_dFitVrtxEpsAuto >> m_dFitVrtxTol;
		ar  >> m_bFitVrtxEpsAuto >> m_bFitVrtxLog;
		ar	>> m_nFitDensChar >> m_nFitDensMaxIter >> m_nFitDensMeth;
		ar	>> m_dFitDensEps >> m_dFitDensTol;
		ar	>> m_bFitDensLog;

		m_model.Serialize( ar );
		m_model.SetComputeRealTime( m_nComputationType );
		m_objPtrArray.Serialize( ar );

		// grids
		m_ObsRel.Serialize( ar );
		m_ObsGrv.Serialize( ar );
		m_ObsMag.Serialize( ar );
		m_ObsTns.Serialize( ar );
		for(int i =0; i < m_nAllComponents; i++) {
			m_grd[i].Serialize(ar);
			ar >> m_grdActiveCompute[i];
		}

		//setting for views
		m_view3dSet.Serialize(ar);
		m_viewPrfSet.Serialize(ar);

		// ----------- load images ---------------------------------------------
		for(int i=0; i<m_objPtrArray.GetCount(); i++) {
			CMyObject* pObj = m_objPtrArray.GetAt(i);
			if( pObj->IsKindOf( RUNTIME_CLASS(CImgReferenced) ) ){
				CImgReferenced* pIm = (CImgReferenced*) pObj;
				if( pIm->m_img.IsNull() ) {
					CString strPathRel, strPathDoc, strDest;
					strPathRel = pIm->m_strFilePathRelative;
					strPathDoc = ar.m_strFileName;
					PathRemoveFileSpec( strPathDoc.GetBuffer() );
					char buffer_1[MAX_PATH] = "";
					char *lpStr1;
					lpStr1 = buffer_1;

					strPathRel.Delete(0,3);
					PathCombine(lpStr1, strPathDoc, strPathRel);
					HRESULT hResult;
					hResult = pIm->m_img.Load(lpStr1);
					if (FAILED(hResult)) {
						CString fmt;
						fmt.Format("Load image failed: %s\nLoad new image?", lpStr1 );
						int nRet = AfxMessageBox(fmt, MB_YESNO | MB_ICONQUESTION);
						if( nRet == IDYES ) {
							pIm->Open();
						}
					}
					TRACE("%s", strDest);
				}
			}
		}
		// ---------------------------------------------------------------------
		AfxGetMainWnd()->SendMessage(WM_UPDATE_OBJMNG, (WPARAM)this, (LPARAM)1);
	}
	CheckDemo();

}

void CMod3DDoc::Serialize_20031105(CArchive& ar)
{
	char	hdr[256];
	OBSHDR *oh = &m_obsHdr;
	BODYVRTXMOVE mb;	memset(&mb, 0, sizeof(BODYVRTXMOVE));	// memset to avoid warnings

	double	dTmp=1.0;
	BOOL	bTmp=FALSE;
	int		nTmp=0;

	if( ar.IsStoring() ) {
		// compute relative paths for pictures...
		ComputeRelativePaths();
		CString str;
		str.Format("Mod3D %d", m_nVersion);
		strcpy(hdr, str);
		// observation header
		ar.Write(hdr, 256);
		ar	<< m_nVersion;
		ar	<< m_nID
			<< oh->nRows << oh->nCols 
			<< oh->x0 << oh->y0 
			<< oh->xSize << oh->ySize 
			<< oh->dMinX << oh->dMaxX 
			<< oh->dMinY << oh->dMaxY 
			<< oh->dMinZ << oh->dMaxZ
			<< nTmp << nTmp
			<< nTmp << nTmp;
		CheckDemo();
		// computation
		ar	<< m_nComputationType;
		ar	<< m_bSherComp;

		// gravity
		ar << m_nGrvFormula;
		ar << m_dGrvDensRef << m_dGrvSens << m_dGrvUnits;
		ar << m_vGrvDensGrad.x << m_vGrvDensGrad.y << m_vGrvDensGrad.z;
		ar << m_vGrvDensOrigo.x << m_vGrvDensOrigo.y << m_vGrvDensOrigo.z;
		ar << m_gvrTens.nTag << m_gvrTens.bCompute << m_gvrTens.dFlightElev << m_gvrTens.dHeight << m_gvrTens.dUnits;

		// magnetic
		ar	<< m_nMagFormula;
		ar	<< m_dMagSens;
        ar	<< m_vMagIndFld.x << m_vMagIndFld.y << m_vMagIndFld.z;

		ar	<< bTmp 
			<< mb.bConstBot << mb.bConstTop << mb.bFixBot
			<< mb.bFixRel << mb.bMoveBot << mb.bMoveTop
			<< mb.dBot << mb.dTop << mb.nFlag;
		ar	<< dTmp;

		// fitting parameters
		ar  << m_nFitFld;
		ar	<< m_nFitVrtxChar << m_nFitVrtxMaxIter << m_nFitVrtxMeth;
		ar	<< m_dFitVrtxEps << m_dFitVrtxEpsAuto << m_dFitVrtxTol;
		ar  << m_bFitVrtxEpsAuto << m_bFitVrtxLog;
		ar	<< m_nFitDensChar << m_nFitDensMaxIter << m_nFitDensMeth;
		ar	<< m_dFitDensEps << m_dFitDensTol;
		ar	<< m_bFitDensLog;

		m_model.Serialize( ar );
		m_model.SetComputeRealTime( m_nComputationType );
		m_objPtrArray.Serialize( ar );

		// grids
		m_ObsRel.Serialize( ar );
		for(int i =0; i < m_nAllComponents; i++) {
			m_grd[i].Serialize(ar);
			ar << m_grdActiveCompute[i];
		}

		//setting for views
		m_view3dSet.Serialize(ar);
		m_viewPrfSet.Serialize(ar);
	}
	else {
		// data section
		ar	>> m_nID
			>> oh->nRows >> oh->nCols 
			>> oh->x0 >> oh->y0 
			>> oh->xSize >> oh->ySize 
			>> oh->dMinX >> oh->dMaxX 
			>> oh->dMinY >> oh->dMaxY 
			>> oh->dMinZ >> oh->dMaxZ
			>> nTmp >> nTmp 
			>> nTmp >> nTmp;
		CheckDemo();
		// computation
		ar	>> m_nComputationType;
		ar	>> m_bSherComp;

		// gravity
		ar >> m_nGrvFormula;
		ar >> m_dGrvDensRef >> m_dGrvSens >> m_dGrvUnits;
		ar >> m_vGrvDensGrad.x >> m_vGrvDensGrad.y >> m_vGrvDensGrad.z;
		ar >> m_vGrvDensOrigo.x >> m_vGrvDensOrigo.y >> m_vGrvDensOrigo.z;
		ar >> m_gvrTens.nTag >> m_gvrTens.bCompute >> m_gvrTens.dFlightElev >> m_gvrTens.dHeight >> m_gvrTens.dUnits;

		// magnetic
		ar	>> m_nMagFormula;
		ar	>> m_dMagSens;
        ar	>> m_vMagIndFld.x >> m_vMagIndFld.y >> m_vMagIndFld.z;

		ar	>> bTmp
			>> mb.bConstBot >> mb.bConstTop >> mb.bFixBot
			>> mb.bFixRel >> mb.bMoveBot >> mb.bMoveTop
			>> mb.dBot >> mb.dTop >> mb.nFlag;
		ar	>> dTmp;

		// fitting parameters
		ar  >> m_nFitFld;
		ar	>> m_nFitVrtxChar >> m_nFitVrtxMaxIter >> m_nFitVrtxMeth;
		ar	>> m_dFitVrtxEps >> m_dFitVrtxEpsAuto >> m_dFitVrtxTol;
		ar  >> m_bFitVrtxEpsAuto >> m_bFitVrtxLog;
		ar	>> m_nFitDensChar >> m_nFitDensMaxIter >> m_nFitDensMeth;
		ar	>> m_dFitDensEps >> m_dFitDensTol;
		ar	>> m_bFitDensLog;

		m_model.Serialize( ar );
		m_model.SetComputeRealTime( m_nComputationType );
		m_objPtrArray.Serialize( ar );

		// grids
		m_ObsRel.Serialize( ar );
		for(int i =0; i < m_nAllComponents; i++) {
			m_grd[i].Serialize(ar);
			ar >> m_grdActiveCompute[i];
		}

		//setting for views
		m_view3dSet.Serialize(ar);
		m_viewPrfSet.Serialize(ar);
	}

}

void CMod3DDoc::Serialize_20031029(CArchive& ar)
{
	char	hdr[256];
	OBSHDR *oh = &m_obsHdr;
	BODYVRTXMOVE mb; memset(&mb, 0, sizeof(BODYVRTXMOVE));

	double	dTmp=1.0;
	BOOL	bTmp=FALSE;
	int		nTmp=0;

	if( ar.IsStoring() ) {
		// compute relative paths for pictures...
		ComputeRelativePaths();
		CString str;
		str.Format("Mod3D %d", m_nVersion);
		strcpy(hdr, str);
		// observation header
		ar.Write(hdr, 256);
		ar	<< m_nVersion;
		ar	<< m_nID
			<< oh->nRows << oh->nCols 
			<< oh->x0 << oh->y0 
			<< oh->xSize << oh->ySize 
			<< oh->dMinX << oh->dMaxX 
			<< oh->dMinY << oh->dMaxY 
			<< oh->dMinZ << oh->dMaxZ
			<< nTmp << nTmp 
			<< nTmp << nTmp;
		CheckDemo();
		// document variables
		ar	<< bTmp << bTmp << m_bSherComp;
		ar	<< m_nGrvFormula << m_nMagFormula;

        ar	<< m_dMagSens
			<< m_vMagIndFld.x << m_vMagIndFld.y << m_vMagIndFld.z;

		ar	<< m_bSherComp;

		ar	<< FALSE 
			<< mb.bConstBot << mb.bConstTop << mb.bFixBot
			<< mb.bFixRel << mb.bMoveBot << mb.bMoveTop
			<< mb.dBot << mb.dTop << mb.nFlag;
		ar	<< 1.0 
			<< m_dGrvDensRef
			<< m_vGrvDensGrad.x << m_vGrvDensGrad.y << m_vGrvDensGrad.z
			<< m_vGrvDensOrigo.x << m_vGrvDensOrigo.y << m_vGrvDensOrigo.z;
		ar	<< bTmp 
			<< m_gvrTens.bCompute << m_gvrTens.dFlightElev << m_gvrTens.dHeight << m_gvrTens.nTag;
		ar	<< m_nAllComponents;
		ar	<< m_nComputationType;

		// fitting aditions version"Mod3D 20030921"
		ar  << m_nFitFld;
		ar	<< m_dFitVrtxEps << m_dFitVrtxTol;

		m_model.Serialize( ar );
		m_model.SetComputeRealTime( m_nComputationType );
		m_objPtrArray.Serialize( ar );
		// grids
		m_ObsRel.Serialize( ar );
		for(int i =0; i < m_nAllComponents; i++) {
			m_grd[i].Serialize(ar);
			ar << m_grdActiveCompute[i];
		}

		//setting for views
		m_view3dSet.Serialize(ar);
		m_viewPrfSet.Serialize(ar);
	}
	else {
		// data section
		ar	>> m_nID
			>> oh->nRows >> oh->nCols 
			>> oh->x0 >> oh->y0 
			>> oh->xSize >> oh->ySize 
			>> oh->dMinX >> oh->dMaxX 
			>> oh->dMinY >> oh->dMaxY 
			>> oh->dMinZ >> oh->dMaxZ
			>> nTmp >> nTmp 
			>> nTmp >> nTmp;
		CheckDemo();
		// document variables
		ar	>> bTmp >> bTmp >> m_bSherComp;
		ar	>> m_nGrvFormula >> m_nMagFormula;

        ar	>> m_dMagSens
			>> m_vMagIndFld.x >> m_vMagIndFld.y >> m_vMagIndFld.z;

		ar	>> m_bSherComp;

		ar	>> bTmp 
			>> mb.bConstBot >> mb.bConstTop >> mb.bFixBot
			>> mb.bFixRel >> mb.bMoveBot >> mb.bMoveTop
			>> mb.dBot >> mb.dTop >> mb.nFlag;
		ar	>> dTmp
			>> m_dGrvDensRef
			>> m_vGrvDensGrad.x >> m_vGrvDensGrad.y >> m_vGrvDensGrad.z
			>> m_vGrvDensOrigo.x >> m_vGrvDensOrigo.y >> m_vGrvDensOrigo.z;
		ar	>> bTmp 
			>> m_gvrTens.bCompute >> m_gvrTens.dFlightElev >> m_gvrTens.dHeight >> m_gvrTens.nTag;
		ar	>> m_nAllComponents;
		ar	>> m_nComputationType;

		// fitting aditions version"Mod3D 20030921"
		ar  >> m_nFitFld;
		ar	>> m_dFitVrtxEps >> m_dFitVrtxTol;

		m_model.Serialize( ar );
		m_model.SetComputeRealTime( m_nComputationType );
		m_objPtrArray.Serialize( ar );
		// grids
		m_ObsRel.Serialize( ar );
		for(int i =0; i < m_nAllComponents; i++) {
			m_grd[i].Serialize(ar);
			ar >> m_grdActiveCompute[i];
		}

		//setting for views
		m_view3dSet.Serialize(ar);
		m_viewPrfSet.Serialize(ar);
	}

}


void CMod3DDoc::Serialize_20031021(CArchive& ar)
{
	char	hdr[256];
	OBSHDR *oh = &m_obsHdr;
	BODYVRTXMOVE mb;	memset(&mb, 0, sizeof(BODYVRTXMOVE));	// avoid warnings

	double	dTmp=1.0;
	BOOL	bTmp=FALSE;
	int		nTmp=0;


	if( ar.IsStoring() ) {
		// compute relative paths for pictures...
		ComputeRelativePaths();
		CString str;
		str.Format("Mod3D %d", m_nVersion);
		strcpy(hdr, str);
		// observation header
		ar.Write(hdr, 256);
		ar	<< m_nVersion;
		ar	<< m_nID
			<< oh->nRows << oh->nCols 
			<< oh->x0 << oh->y0 
			<< oh->xSize << oh->ySize 
			<< oh->dMinX << oh->dMaxX 
			<< oh->dMinY << oh->dMaxY 
			<< oh->dMinZ << oh->dMaxZ
			<< nTmp << nTmp 
			<< nTmp << nTmp;
		// document variables
		ar	<< bTmp << bTmp << m_bSherComp;
		ar	<< m_nGrvFormula << m_nMagFormula;

        ar	<< m_dMagSens
			<< m_vMagIndFld.x << m_vMagIndFld.y << m_vMagIndFld.z;

		ar	<< m_bSherComp;

		//version "Mod3D 02" additions
		ar	<< bTmp
			<< mb.bConstBot << mb.bConstTop << mb.bFixBot
			<< mb.bFixRel << mb.bMoveBot << mb.bMoveTop
			<< mb.dBot << mb.dTop << mb.nFlag;
		ar	<< dTmp 
			<< m_dGrvDensRef
			<< m_vGrvDensGrad.x << m_vGrvDensGrad.y << m_vGrvDensGrad.z
			<< m_vGrvDensOrigo.x << m_vGrvDensOrigo.y << m_vGrvDensOrigo.z;
		ar	<< bTmp 
			<< m_gvrTens.bCompute << m_gvrTens.dFlightElev << m_gvrTens.dHeight << m_gvrTens.nTag;
		ar	<< m_nAllComponents;
		ar	<< m_nComputationType;
		for(int i =0; i < m_nAllComponents; i++) {
			ar << m_grdActiveCompute[i];
		}

		m_model.Serialize( ar );
		m_objPtrArray.Serialize( ar );
		// grids
		m_ObsRel.Serialize( ar );
		for(int i =0; i < m_nAllComponents; i++) {
			m_grd[i].Serialize(ar);
		}
		m_viewPrfSet.Serialize(ar);

		CheckDemo();
		m_model.SetComputeRealTime( m_nComputationType );

		// fitting aditions version"Mod3D 20030921"
		ar  << m_nFitFld;
		ar	<< m_dFitVrtxEps << m_dFitVrtxTol;

		// version 20031121 addition
		m_view3dSet.Serialize(ar);
	
	}
	else {
		// data section
		ar	>> m_nID
			>> oh->nRows >> oh->nCols 
			>> oh->x0 >> oh->y0 
			>> oh->xSize >> oh->ySize 
			>> oh->dMinX >> oh->dMaxX 
			>> oh->dMinY >> oh->dMaxY 
			>> oh->dMinZ >> oh->dMaxZ
			>> nTmp >> nTmp
			>> nTmp >> nTmp;
		// document variables
		ar	>> bTmp >> bTmp >> m_bSherComp;
		ar	>> m_nGrvFormula >> m_nMagFormula;

		ar	>> m_dMagSens
			>> m_vMagIndFld.x >> m_vMagIndFld.y >> m_vMagIndFld.z;

		ar >> m_bSherComp;

		ar	>> bTmp 
			>> mb.bConstBot >> mb.bConstTop >> mb.bFixBot
			>> mb.bFixRel >> mb.bMoveBot >> mb.bMoveTop
			>> mb.dBot >> mb.dTop >> mb.nFlag;
		ar	>> dTmp
			>> m_dGrvDensRef
			>> m_vGrvDensGrad.x >> m_vGrvDensGrad.y >> m_vGrvDensGrad.z
			>> m_vGrvDensOrigo.x >> m_vGrvDensOrigo.y >> m_vGrvDensOrigo.z;
		ar	>> bTmp 
			>> m_gvrTens.bCompute >> m_gvrTens.dFlightElev >> m_gvrTens.dHeight >> m_gvrTens.nTag;
		ar	>> m_nAllComponents;
		ar	>> m_nComputationType;

		for(int i = 0; i < m_nAllComponents; i++) {
			ar >> m_grdActiveCompute[i];
		}

		m_model.Serialize( ar );
		m_objPtrArray.Serialize( ar );
		// grids
		m_ObsRel.Serialize( ar );
		for(int i =0; i < m_nAllComponents; i++) {
			m_grd[i].Serialize(ar);
		}
		m_viewPrfSet.Serialize(ar);

		CheckDemo();
		m_model.SetComputeRealTime( m_nComputationType );

		// fitting aditions version"Mod3D 20030921"
		ar  >> m_nFitFld;
		ar	>> m_dFitVrtxEps >> m_dFitVrtxTol;

		if( m_nVersion >= 20031121 ) {
			m_view3dSet.Serialize(ar);
		}
	}

}

// loads old versions
void CMod3DDoc::SerializeOld(CArchive& ar)
{
	char	hdr[256];
	OBSHDR *oh = &m_obsHdr;
	BODYVRTXMOVE mb;

	double	dTmp=1.0;
	BOOL	bTmp=FALSE;
	int		nTmp=0;

	if( !ar.IsStoring() ) {
		ar	>> m_nID
			>> oh->nRows >> oh->nCols 
			>> oh->x0 >> oh->y0 
			>> oh->xSize >> oh->ySize 
			>> oh->dMinX >> oh->dMaxX 
			>> oh->dMinY >> oh->dMaxY 
			>> oh->dMinZ >> oh->dMaxZ
			>> nTmp >> nTmp
			>> nTmp >> nTmp;
		// document variables
		ar	>> bTmp >> bTmp >> m_bSherComp;
		ar	>> m_nGrvFormula >> m_nMagFormula;

		ar	>> m_dMagSens
			>> m_vMagIndFld.x >> m_vMagIndFld.y >> m_vMagIndFld.z;

		ar >> m_bSherComp;

		ar	>> bTmp 
			>> mb.bConstBot >> mb.bConstTop >> mb.bFixBot
			>> mb.bFixRel >> mb.bMoveBot >> mb.bMoveTop
			>> mb.dBot >> mb.dTop >> mb.nFlag;
		ar	>> dTmp
			>> m_dGrvDensRef
			>> m_vGrvDensGrad.x >> m_vGrvDensGrad.y >> m_vGrvDensGrad.z
			>> m_vGrvDensOrigo.x >> m_vGrvDensOrigo.y >> m_vGrvDensOrigo.z;
		ar	>> bTmp 
			>> m_gvrTens.bCompute >> m_gvrTens.dFlightElev >> m_gvrTens.dHeight >> m_gvrTens.nTag;
		ar	>> m_nAllComponents;
		ar	>> m_nComputationType;
		for(int i = 0; i < m_nAllComponents; i++) {
			ar >> m_grdActiveCompute[i];
		}
	}

	m_model.Serialize( ar );
	m_objPtrArray.Serialize( ar );
	// grids
	m_ObsRel.Serialize( ar );
	for(int i =0; i < m_nAllComponents; i++) {
		m_grd[i].Serialize(ar);
	}
	if( strstr(hdr, "Mod3D 2.001") )
		m_viewPrfSet.Serialize(ar);

	CheckDemo();
	m_model.SetComputeRealTime( m_nComputationType );
}

