// Grid.cpp: implementation of the CGrid class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Grid.h"
#include <math.h>
#include "Routines.h"

#include "DlgGridProperties.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

#define PI 3.1415926535897932384626433832795
#define PIR 0.017453292519943295769236907684886		// PI/180

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IMPLEMENT_SERIAL( CGrid, CObject, VERSIONABLE_SCHEMA|2 )

CGrid::CGrid()
{
	m_nVersion = 20040326;
	//m_nVersion = 20031210;

	m_bVisible = FALSE;
	m_bFlag = FALSE;
	m_strFilePath = "out.grd";
	m_nType = SRF_7;
	m_nRow = 0;
	m_nCol = 0;
	m_x0 = 0;
	m_y0 = 0;
	m_xSize = 0;
	m_ySize = 0;
	m_dMinX = 0;
	m_dMinY = 0;
	m_dMinZ = 0;
	m_dMaxX = 0;
	m_dMaxY = 0;
	m_dMaxZ = 0;
	m_pData = NULL;
	m_bAllocated = FALSE;
	m_bModified = TRUE;
	m_bModifiedMinMax = TRUE;
	m_bModifiedContours = TRUE;
	m_bModifiedImage = TRUE;
	m_bModfHistogram = TRUE;
	m_bModifiedColorTable = TRUE;
	m_dRot = 0.0;
	m_dRotCos = cos(m_dRot*PIR);
	m_dRotSin = sin(m_dRot*PIR);

	// contouring part
	m_nContNum = 30;
	m_bShowContour = FALSE;
	m_dRMS = 0.0;
	m_dDrv = 0.0;

	m_bHistClr = FALSE;
	m_bCustomRange = FALSE;
	m_dMaxHstCst = DUMMY;
	m_dMinHstCst = DUMMY;

	m_dMean = DUMMY;
	m_dMeanP = DUMMY;

	m_nClrTbl = CTBL_FLD;
	InitColorTables();
	InitColorTable();
}

CGrid::CGrid(DWORD row, DWORD col, double x0, double y0, double xSize, double ySize, double rot)
{
	m_nVersion = 20040326;
	//m_nVersion = 20031210;

	m_bVisible  = FALSE;
	m_bFlag = FALSE;
	m_strFilePath = "out.grd";
	m_nType = SRF_7;
	m_nRow = row;
	m_nCol = col;
	m_x0 = x0;
	m_y0 = y0;
	m_xSize = xSize;
	m_ySize = ySize;
	m_dRot = rot;
	InitVariables();
	//m_dRotCos = cos(m_dRot*PIR);
	//m_dRotSin = sin(m_dRot*PIR);
	//m_dMinX = x0;
	//m_dMaxX = x0 + xSize*(col-1);
	//m_dMinY = y0;
	//m_dMaxY = y0 + ySize*(row-1);
	m_dMinZ = 0;
	m_dMaxZ = 0;
	m_pData = NULL;

	m_nContNum = 30;
	m_bShowContour = FALSE;
	m_nClrTbl = CTBL_FLD;
	m_bModified = TRUE;
	m_bModifiedContours = TRUE;
	m_bModifiedImage = TRUE;
	m_bModifiedMinMax = TRUE;
	m_bModfHistogram = TRUE;
	m_bModifiedColorTable = TRUE;
	m_dRot = 0.0;
	m_dRotCos = cos(m_dRot*PIR);
	m_dRotSin = sin(m_dRot*PIR);

	// contouring part
	m_nContNum = 30;
	m_bShowContour = FALSE;
	m_dRMS = 0.0;
	m_dDrv = 0.0;

	m_bHistClr = FALSE;
	m_bCustomRange = FALSE;
	m_dMaxHstCst = DUMMY;
	m_dMinHstCst = DUMMY;

	m_dMean = DUMMY;
	m_dMeanP = DUMMY;

	m_nClrTbl = CTBL_FLD;
	AllocateGrid();
	InitColorTables();
	InitColorTable();
}


CGrid::~CGrid()
{
	if(m_bAllocated)
		AllocateGrid(FALSE);
}

void CGrid::InitVariables(void)
{
	m_dMinX = min( min(GetX(0,0), GetX(0, m_nCol-1)), min(GetX(m_nRow-1,m_nCol-1),GetX(m_nRow-1, 0)) );
	m_dMaxX = max( max(GetX(0,0), GetX(0, m_nCol-1)), max(GetX(m_nRow-1,m_nCol-1),GetX(m_nRow-1, 0)) );
	m_dMinY = min( min(GetY(0,0), GetY(0, m_nCol-1)), min(GetY(m_nRow-1,m_nCol-1),GetY(m_nRow-1, 0)) );
	m_dMaxY = max( max(GetY(0,0), GetY(0, m_nCol-1)), max(GetY(m_nRow-1,m_nCol-1),GetY(m_nRow-1, 0)) );
	m_dRotCos = cos(m_dRot*PIR);
	m_dRotSin = sin(m_dRot*PIR);
	if( m_pData != NULL ) UpdateMinMaxValue();
}

void CGrid::Serialize(CArchive &ar)
{
	if( ar.IsStoring() ) {
		m_strID = _T("_gridMod3D");
        ar	<< m_strID;
		ar	<< m_nVersion;
		ar	<< m_nType
			<< m_nRow << m_nCol 
			<< m_x0 << m_y0 << m_xSize << m_ySize 
			<< m_dMinX << m_dMaxX
			<< m_dMinY << m_dMaxY
			<< m_dMinZ << m_dMaxZ;
		ar	<< m_nClrTbl;
		ar	<< m_nContNum;
		ar	<< m_dRMS;
		ar	<< m_dRot;
		ar	<< m_dDrv;
		// write "grid"
		for(DWORD i = 0; i < m_nRow; i++) {
			for(DWORD j = 0; j < m_nCol; j++) {
				ar << m_pData[i][j];
			}
		}

		m_clrGrad.Serialize(ar);

		// color range
		ar << m_bHistClr;
		ar << m_bCustomRange;
		ar << m_dMinHstCst;
		ar << m_dMaxHstCst;
	}
    else {
        ar	>> m_strID ;
		if( m_strID.Find("_gridMod3D", 0) == -1 )  {
			SerializeOld(ar);
			return;
		}

		ar	>> m_nVersion;
		ar	>> m_nType
			>> m_nRow >> m_nCol 
			>> m_x0 >> m_y0 >> m_xSize >> m_ySize 
			>> m_dMinX >> m_dMaxX
			>> m_dMinY >> m_dMaxY
			>> m_dMinZ >> m_dMaxZ;
		ar	>> m_nClrTbl;
		ar	>> m_nContNum;
		ar	>> m_dRMS;
		ar	>> m_dRot;
		ar	>> m_dDrv;

		// read "grid"
		AllocateGrid();
		for(DWORD i = 0; i < m_nRow; i++) {
			for(DWORD j = 0; j < m_nCol; j++) {
				ar >> m_pData[i][j];
			}
		}

		m_clrGrad.Serialize(ar);

		if(m_nVersion>=20040326) {
			switch(m_nVersion) {
				case 20040326: 
					{
					// color range
					ar >> m_bHistClr;
					ar >> m_bCustomRange;
					ar >> m_dMinHstCst;
					ar >> m_dMaxHstCst;
					}
				default: 
					break;
			}
		}

		// set version to current
		m_nVersion = 20040326;
		InitVariables();
	}
}

void CGrid::SerializeOld(CArchive &ar)
{
	if( ar.IsStoring() ) {
		ar	<< m_strID;
        ar	<< m_nType
			<< m_nRow << m_nCol 
			<< m_x0 << m_y0 << m_xSize << m_ySize 
			<< m_dMinX << m_dMaxX
			<< m_dMinY << m_dMaxY
			<< m_dMinZ << m_dMaxZ;
		ar	<< m_nClrTbl;
		ar	<< m_nContNum;
		ar	<< m_dRMS;
		ar	<< m_dRot;
		ar	<< m_dDrv;
		// write "grid"
		for(DWORD i = 0; i < m_nRow; i++) {
			for(DWORD j = 0; j < m_nCol; j++) {
				ar << m_pData[i][j];
			}
		}
	}
    else {
		//ar	>> m_strID;
		ar	>> m_nType
			>> m_nRow >> m_nCol 
			>> m_x0 >> m_y0 >> m_xSize >> m_ySize 
			>> m_dMinX >> m_dMaxX
			>> m_dMinY >> m_dMaxY
			>> m_dMinZ >> m_dMaxZ;
		ar	>> m_nClrTbl;
		ar	>> m_nContNum;
		ar	>> m_dRMS;
		ar >> m_dRot;
		ar >> m_dDrv;

		// read "grid"
		AllocateGrid();
		if(m_pData != NULL) {
			for(DWORD i = 0; i < m_nRow; i++) {
				for(DWORD j = 0; j < m_nCol; j++) {
					ar >> m_pData[i][j];
				}
			}
		}
		m_dRot=0.0;
		InitVariables();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//****** operators ************************************************************************************************************
void CGrid::operator+=(double d)
{
	ASSERT(!IsEmpty());

	for(DWORD i = 0; i < m_nRow; i++) {
		for(DWORD j = 0; j < m_nCol; j++) {
			m_pData[i][j] += d;
		}
	}
}

void CGrid::operator-=(double d)
{
	ASSERT(!IsEmpty());

	for(DWORD i = 0; i < m_nRow; i++) {
		for(DWORD j = 0; j < m_nCol; j++) {
			if(m_pData[i][j] != DUMMY)
				m_pData[i][j] -= d;
		}
	}
}

void CGrid::operator*=(double d)
{
	ASSERT(!IsEmpty());

	for(DWORD i = 0; i < m_nRow; i++) {
		for(DWORD j = 0; j < m_nCol; j++) {
			if(m_pData[i][j] != DUMMY)
				m_pData[i][j] *= d;
		}
	}
}

void CGrid::operator/=(double d)
{
	ASSERT(!IsEmpty());

	for(DWORD i = 0; i < m_nRow; i++) {
		for(DWORD j = 0; j < m_nCol; j++) {
			if(m_pData[i][j] != DUMMY)
				m_pData[i][j] /= d;
		}
	}
}

void CGrid::operator+=(CGrid* pGrd)
{
	ASSERT( pGrd!=NULL );
	ASSERT( !pGrd->IsEmpty() );
	ASSERT( IsSameSize(pGrd) );

	double** data;

	data = pGrd->GetData();
	ASSERT(data!=NULL);

	for(DWORD i = 0; i < m_nRow; i++) {
		for(DWORD j = 0; j < m_nCol; j++) {
			if(data[i][j] != DUMMY && m_pData[i][j]!=DUMMY)
				m_pData[i][j] += data[i][j];
		}
	}
}

void CGrid::operator-=(CGrid* pGrd)
{
	ASSERT( pGrd!=NULL );
	ASSERT( !pGrd->IsEmpty() );
	ASSERT( IsSameSize(pGrd) );

	double** data;

	data = pGrd->GetData();
	ASSERT(data!=NULL);

	for(DWORD i = 0; i < m_nRow; i++) {
		for(DWORD j = 0; j < m_nCol; j++) {
			if(data[i][j] != DUMMY && m_pData[i][j]!=DUMMY)
				m_pData[i][j] -= data[i][j];
		}
	}
}

void CGrid::operator*=(CGrid* pGrd)
{
	ASSERT( pGrd!=NULL );
	ASSERT( !pGrd->IsEmpty() );
	ASSERT( IsSameSize(pGrd) );

	double** data;

	data = pGrd->GetData();
	ASSERT(data!=NULL);

	for(DWORD i = 0; i < m_nRow; i++) {
		for(DWORD j = 0; j < m_nCol; j++) {
			if(data[i][j] != DUMMY && m_pData[i][j]!=DUMMY)
				m_pData[i][j] *= data[i][j];
		}
	}
}

void CGrid::operator/=(CGrid* pGrd)
{
	ASSERT( pGrd!=NULL );
	ASSERT( !pGrd->IsEmpty() );
	ASSERT( IsSameSize(pGrd) );

	double** data;

	data = pGrd->GetData();
	ASSERT(data!=NULL);

	for(DWORD i = 0; i < m_nRow; i++) {
		for(DWORD j = 0; j < m_nCol; j++) {
			if(data[i][j] != DUMMY && m_pData[i][j]!=DUMMY)
				m_pData[i][j] /= data[i][j];
		}
	}
}

//****** end operators ********************************************************************************************************
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////




void CGrid::InitColorTable(void)
{
	float f;
	m_clrGrad.RemoveAllPegs();
	switch(m_nClrTbl) {
		case CTBL_FLD:
			f=6;
			m_clrGrad.SetStartPegColour( RGB(0,	0,		153));
			m_clrGrad.AddPeg( RGB(0,	204,	255), 1/f);
			m_clrGrad.AddPeg( RGB(0,	255,	0), 2/f);
			m_clrGrad.AddPeg( RGB(255,	255,	0), 3/f);
			m_clrGrad.AddPeg( RGB(255,	0,		0), 4/f);
			m_clrGrad.AddPeg( RGB(255,	0,		255), 5/f);
			m_clrGrad.SetEndPegColour( RGB(255,	200,	255));
			break;
		case CTBL_REL:
			f=10;
			m_clrGrad.SetStartPegColour( RGB(221, 250, 218));
			m_clrGrad.AddPeg( RGB(168, 251, 185), 1/f);
			m_clrGrad.AddPeg( RGB(27, 206, 96), 2/f);
			m_clrGrad.AddPeg( RGB(58, 160, 81), 3/f);
			m_clrGrad.AddPeg( RGB(238, 247, 189), 4/f);
			m_clrGrad.AddPeg( RGB(236, 230, 168), 5/f);
			m_clrGrad.AddPeg( RGB(231, 219, 167), 6/f);
			m_clrGrad.AddPeg( RGB(161, 127, 111), 7/f);
			m_clrGrad.AddPeg( RGB(108, 74, 76), 8/f);
			m_clrGrad.AddPeg( RGB(147, 137, 133), 9/f);
			m_clrGrad.SetEndPegColour( RGB(249, 245, 231));
			break;
		case CTBL_DIF:
			m_clrGrad.SetStartPegColour( RGB(0, 0, 160) );
			m_clrGrad.AddPeg( RGB(255, 255, 255), 0.5f);
			m_clrGrad.SetEndPegColour( RGB(160, 0, 0) );
			break;
		case CTBL_BW:
			m_clrGrad.SetStartPegColour( RGB(20, 20, 20) );
			m_clrGrad.SetEndPegColour( RGB(220, 220, 220) );
			break;
		default:
			f=6;
			m_clrGrad.SetStartPegColour( RGB(0,	0,		153));
			m_clrGrad.AddPeg( RGB(0,	204,	255), 1/f);
			m_clrGrad.AddPeg( RGB(0,	255,	0), 2/f);
			m_clrGrad.AddPeg( RGB(255,	255,	0), 3/f);
			m_clrGrad.AddPeg( RGB(255,	0,		0), 4/f);
			m_clrGrad.AddPeg( RGB(255,	0,		255), 5/f);
			m_clrGrad.SetEndPegColour( RGB(255,	200,	255));
	}
}

void CGrid::InitColorTables(void)
{
	// coor table black and white
	m_rgbTableBW.SetSize(2);
	m_rgbTableBW.GetAt(0).rgbRed = 20;	m_rgbTableBW.GetAt(0).rgbGreen = 20;	m_rgbTableBW.GetAt(0).rgbBlue = 20;
	m_rgbTableBW.GetAt(1).rgbRed = 220;	m_rgbTableBW.GetAt(1).rgbGreen = 220;	m_rgbTableBW.GetAt(1).rgbBlue = 220;
	//m_rgbTableBW.GetAt(2).rgbRed = 191;		m_rgbTableBW.GetAt(2).rgbGreen = 191;	m_rgbTableBW.GetAt(2).rgbBlue = 191;

	// color table difference field
	m_rgbTableDif.SetSize(3);
	m_rgbTableDif.GetAt(0).rgbRed = 0;		m_rgbTableDif.GetAt(0).rgbGreen = 0;	m_rgbTableDif.GetAt(0).rgbBlue = 160;
	m_rgbTableDif.GetAt(1).rgbRed = 255;	m_rgbTableDif.GetAt(1).rgbGreen = 255;	m_rgbTableDif.GetAt(1).rgbBlue = 255;
	m_rgbTableDif.GetAt(2).rgbRed = 160;	m_rgbTableDif.GetAt(2).rgbGreen = 0;	m_rgbTableDif.GetAt(2).rgbBlue = 0;

	// color table field
	m_rgbTable.SetSize(7);
	m_rgbTable.GetAt(0).rgbRed = 0;	m_rgbTable.GetAt(0).rgbGreen = 0;	m_rgbTable.GetAt(0).rgbBlue = 153;
	m_rgbTable.GetAt(1).rgbRed = 0;	m_rgbTable.GetAt(1).rgbGreen = 204;	m_rgbTable.GetAt(1).rgbBlue = 255;
	m_rgbTable.GetAt(2).rgbRed = 0;	m_rgbTable.GetAt(2).rgbGreen = 255;	m_rgbTable.GetAt(2).rgbBlue = 0;
	m_rgbTable.GetAt(3).rgbRed = 255;	m_rgbTable.GetAt(3).rgbGreen = 255;	m_rgbTable.GetAt(3).rgbBlue = 0;
	m_rgbTable.GetAt(4).rgbRed = 255;	m_rgbTable.GetAt(4).rgbGreen = 0;	m_rgbTable.GetAt(4).rgbBlue = 0;
	m_rgbTable.GetAt(5).rgbRed = 255;	m_rgbTable.GetAt(5).rgbGreen = 0;	m_rgbTable.GetAt(5).rgbBlue = 255;
	m_rgbTable.GetAt(6).rgbRed = 255;	m_rgbTable.GetAt(6).rgbGreen = 200;	m_rgbTable.GetAt(6).rgbBlue = 255;

	// color table relief
	m_rgbTableRel.SetSize(11);
	m_rgbTableRel.GetAt(0).rgbRed = 221;	m_rgbTableRel.GetAt(0).rgbGreen = 250;	m_rgbTableRel.GetAt(0).rgbBlue = 218;
	m_rgbTableRel.GetAt(1).rgbRed = 168;	m_rgbTableRel.GetAt(1).rgbGreen = 251;	m_rgbTableRel.GetAt(1).rgbBlue = 185;
	m_rgbTableRel.GetAt(2).rgbRed = 27;		m_rgbTableRel.GetAt(2).rgbGreen = 206;	m_rgbTableRel.GetAt(2).rgbBlue = 96;
	m_rgbTableRel.GetAt(3).rgbRed = 58;		m_rgbTableRel.GetAt(3).rgbGreen = 160;	m_rgbTableRel.GetAt(3).rgbBlue = 81;
	m_rgbTableRel.GetAt(4).rgbRed = 238;	m_rgbTableRel.GetAt(4).rgbGreen = 247;	m_rgbTableRel.GetAt(4).rgbBlue = 189;
	m_rgbTableRel.GetAt(5).rgbRed = 236;	m_rgbTableRel.GetAt(5).rgbGreen = 230;	m_rgbTableRel.GetAt(5).rgbBlue = 168;
	m_rgbTableRel.GetAt(6).rgbRed = 231;	m_rgbTableRel.GetAt(6).rgbGreen = 219;	m_rgbTableRel.GetAt(6).rgbBlue = 167;
	m_rgbTableRel.GetAt(7).rgbRed = 161;	m_rgbTableRel.GetAt(7).rgbGreen = 127;	m_rgbTableRel.GetAt(7).rgbBlue = 111;
	m_rgbTableRel.GetAt(8).rgbRed = 108;	m_rgbTableRel.GetAt(8).rgbGreen = 74;	m_rgbTableRel.GetAt(8).rgbBlue = 76;
	m_rgbTableRel.GetAt(9).rgbRed = 147;	m_rgbTableRel.GetAt(9).rgbGreen = 137;	m_rgbTableRel.GetAt(9).rgbBlue = 133;
	m_rgbTableRel.GetAt(10).rgbRed = 249;	m_rgbTableRel.GetAt(10).rgbGreen = 245;	m_rgbTableRel.GetAt(10).rgbBlue = 231;
}

BOOL CGrid::AllocateGrid(BOOL bAllocate)
{
	DWORD i;

	// allocate memory for grid
	if(bAllocate) {
		if( m_nRow > 0 && m_nCol > 0) {
			m_pData = (double **) malloc(m_nRow * sizeof(double *));
			for(i = 0; i < m_nRow; i++)
				m_pData[i] = (double *) malloc(m_nCol * sizeof(double));
			m_bAllocated = TRUE;
		}
		else
			m_bAllocated = FALSE;
	}
	// free memory
	else {
		if(m_bAllocated) {
			for(i = 0; i < m_nRow; i++)
				free(m_pData[i]);
			free(m_pData);
			m_pData=NULL;
			m_bAllocated = FALSE;
		}
	}
	return TRUE;
}

BOOL CGrid::New(DWORD row, DWORD col, double x0, double y0, double xSize, double ySize, double rot)
{
	// the grid MUST be closed
	// set parameters for new grid and allocate memory
	m_strFilePath = "out.grd";
	m_nType = SRF_7;
	m_bModified = TRUE;
	m_bModifiedContours = TRUE;
	m_bModifiedImage = TRUE;
	m_bModfHistogram = TRUE;
	m_bModifiedMinMax = TRUE;
	m_bModifiedColorTable = TRUE;
	m_nRow = row;
	m_nCol = col;
	m_x0 = x0;
	m_y0 = y0;
	m_xSize = xSize;
	m_ySize = ySize;
	m_dMinZ = 0;
	m_dMaxZ = 0;
	m_dRot = rot;
	InitVariables();
	//m_dMinX = min( min(GetX(0,0), GetX(0, col-1)), min(GetX(row-1,col-1),GetX(row-1, 0)) );
	//m_dMaxX = max( max(GetX(0,0), GetX(0, col-1)), max(GetX(row-1,col-1),GetX(row-1, 0)) );
	//m_dMinY = min( min(GetY(0,0), GetY(0, col-1)), min(GetY(row-1,col-1),GetY(row-1, 0)) );
	//m_dMaxY = max( max(GetY(0,0), GetY(0, col-1)), max(GetY(row-1,col-1),GetY(row-1, 0)) );
	//m_dRotCos = cos(m_dRot*PIR);
	//m_dRotSin = sin(m_dRot*PIR);
	AllocateGrid();
	return TRUE;
}

BOOL CGrid::New(CString strFilePath, int nType, DWORD row, DWORD col, double x0, double y0, double z, double xSize, double ySize, double rot)
{
	// the grid MUST be closed
	// set parameters for new grid and allocate memory
	m_strFilePath = strFilePath;
	m_nType = nType;
	m_bModified = TRUE;
	m_bModifiedContours = TRUE;
	m_bModifiedImage = TRUE;
	m_bModfHistogram = TRUE;
	m_bModifiedMinMax = TRUE;
	m_bModifiedColorTable = TRUE;
	m_nRow = row;
	m_nCol = col;
	m_x0 = x0;
	m_y0 = y0;
	m_xSize = xSize;
	m_ySize = ySize;
	m_dMinZ = 0;
	m_dMaxZ = 0;
	m_dRot = rot;
	InitVariables();
	//m_dMinX = x0;
	//m_dMaxX = x0 + xSize*(col-1);
	//m_dMinY = y0;
	//m_dMaxY = y0 + ySize*(row-1);
	//m_dRotCos = cos(m_dRot*PIR);
	//m_dRotSin = sin(m_dRot*PIR);
	for(DWORD i = 0; i < m_nRow; i++) {
		for(DWORD j = 0; j < m_nCol; j++) {
			m_pData[i][j] = z;
		}
	}
	Save();
	return TRUE;
}

BOOL CGrid::Open()
{
	BOOL bOK;

	if( !IsEmpty() ) {
		int ret = AfxMessageBox("Grid is not empty! Replace?", MB_YESNO | MB_ICONQUESTION);
		if( ret==IDNO || ret==IDCANCEL ) return FALSE;
	}

	CFileDialog dlg(TRUE, "grd", "*.grd", OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "Surfer 6.0 Grid (*.grd)|*.grd|Surfer 7.0 Grid (*.grd)|*.grd|Geosoft Grid (*.grd)|*.grd||");
	if(dlg.DoModal() == IDOK) {
		m_strFilePath = dlg.GetPathName();
		//switch(dlg.m_ofn.nFilterIndex) {
		//	case 1:
		//		m_nType = SRF_6;	// surfer 6.0
		//		bOK = OpenSrf6Grid(m_strFilePath);
		//		break;
		//	case 2:
		//		m_nType = SRF_7;	// surfer 7.0 
		//		bOK = OpenSrf7Grid(m_strFilePath);
		//		break;
		//	case 3:
		//		m_nType = GEOS_KX1;	// geosoft default kx = 1;
		//		AfxMessageBox("Not available");
		//		bOK = FALSE;
		//		//bOK = OpenGeosoftGrid(strFileName);
		//		break;
		//}
		int m_nType = ReadType(m_strFilePath);
		if( m_nType == -1 ) return FALSE;
		else {
			switch(m_nType) {
				case SRF_6: 
					bOK = OpenSrf6Grid(m_strFilePath);
					break;
				case SRF_7: 
					bOK = OpenSrf7Grid(m_strFilePath);
					break;
			}
		}
		CheckDummies();
		UpdateMinMaxValue();
		return bOK;
	}
	else 
		return FALSE;
}

BOOL CGrid::Open(CString strFilePath, int nType)
{
	m_strFilePath = strFilePath;
	m_nType = (GrdType) nType;
	if(	m_nType == SRF_6)	// surfer 6.0
		return OpenSrf6Grid(m_strFilePath);
	if(	m_nType == SRF_7 )	// surfer 7.0 
		return OpenSrf7Grid(m_strFilePath);
	if( m_nType == GEOS_KX1 )	{// geosoft default kx = 1;
		AfxMessageBox("Not available");
		return FALSE;
	}
	
	return FALSE;
}

BOOL CGrid::Save()
{
	BOOL bOK;

	if(!IsEmpty()) {
		switch(m_nType) {
			case SRF_6:
				bOK = SaveSrf6Grid(m_strFilePath);
				break;
			case SRF_7:
				bOK = SaveSrf7Grid(m_strFilePath);
				break;
			case GEOS_KX1:
				AfxMessageBox("Not available");
				bOK = FALSE;
				//bOK = OpenGeosoftGrid(strFileName);
				break;
		}
		if(bOK)
			m_bModified = FALSE;
		return bOK;
	}
	else
		return FALSE;
}


BOOL CGrid::SaveAs()
{
	BOOL bOK;

	if(!IsEmpty()) {
		CFileDialog dlg(FALSE, "grd", "*.grd", OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "Surfer 6.0 Grid (*.grd)|*.grd|Surfer 7.0 Grid (*.grd)|*.grd|Geosoft Grid (*.grd)|*.grd||");
		dlg.m_ofn.nFilterIndex = m_nType+1;
		if(dlg.DoModal() == IDOK) {
			m_strFilePath = dlg.GetPathName();
			switch(dlg.m_ofn.nFilterIndex) {
				case 1:
					bOK = SaveSrf6Grid(m_strFilePath);
					break;
				case 2:
					bOK = SaveSrf7Grid(m_strFilePath);
					break;
				case 3:
					AfxMessageBox("Not available");
					bOK = FALSE;
					//bOK = OpenGeosoftGrid(strFileName);
					break;
			}
			if(bOK)
				m_bModified = FALSE;
			return bOK;
		}
		else 
			return FALSE;
	}
	else
		return FALSE;
}


BOOL CGrid::OpenGeosoftGrid(CString strFileName, double tx, double ty, double cr)
{
	// opens geosoft binary grid, and windows grid for topocorrection 
	// at point tx. ty for given circle radius cr
	// only for grids without compression
	// names based on Geosoft help; Grid File Format (*.GRD)
	long	nES, nSF, nNE, nNV, nKX;
	double	dDE, dDV, dX0, dY0, dROT, dZBASE, dZMULT;
	char	strLABEL[49], strMAPNO[17];
	long	nPROJ, nUNITX, nUNITY, nUNITZ, nNVPTS;
	float	fIZMIN, fIZMAX, fIZMED, fIZMEA;
	double	dZVAR;
	long	nPRCS;
	char	strUSER[325];
	
	// mine variables, fuck off geosoft variable names
	long nGCol, nGRow;

	CFile file;
	file.Open((LPCSTR) strFileName, CFile::modeRead);
	//++++++++++ read header 512 byte ++++++++++++++++++++++++++++++
	file.Read(&nES, 4);
	file.Read(&nSF, 4);
	file.Read(&nNE, 4);
	file.Read(&nNV, 4);
	file.Read(&nKX, 4);
	file.Read(&dDE, 8);
	file.Read(&dDV, 8);
	file.Read(&dX0, 8);
	file.Read(&dY0, 8);
	file.Read(&dROT, 8);
	file.Read(&dZBASE, 8);
	file.Read(&dZMULT, 8);
	file.Read(&strLABEL, 48);
	file.Read(&strMAPNO, 16);
	file.Read(&nPROJ, 4);
	file.Read(&nUNITX, 4);
	file.Read(&nUNITY, 4);
	file.Read(&nUNITZ, 4);
	file.Read(&nNVPTS, 4);
	file.Read(&fIZMIN, 4);
	file.Read(&fIZMAX, 4);
	file.Read(&fIZMED, 4);
	file.Read(&fIZMEA, 4);
	file.Read(&dZVAR, 8);
	file.Read(&nPRCS, 4);
	file.Read(&strUSER, 324);
	m_xSize = dDE;
	m_ySize = dDV;
	nGCol = nNE;
	nGRow = nNV;
	//++++++++++ end header ++++++++++++++++++++++++++++++++++++++++
	// shift T coordinates to Origo 
	double tx0, ty0;
	tx0 = tx - dX0;
	ty0 = ty - dY0;
	// find window grid center
	long nCWRow, nCWCol;		// Center Window Row, Column
	nCWCol = (long) floor(tx0/m_xSize);
	nCWRow = (long) floor(ty0/m_ySize);
	// number of cells within circle radius
	long k, l;	// k-collumns, l-rows
	k = (long) ceil(cr/m_xSize);
	l = (long) ceil(cr/m_ySize);
	m_nRow = 2*l+2;	// zero based
	m_nCol = 2*k+2;
	// window grid lower lweft corner
	long nWLLRow, nWLLCol;
	nWLLRow = nCWRow - l;
	nWLLCol = nCWCol - k;
	m_x0 = dX0 + nWLLCol*m_xSize;
	m_y0 = dY0 + nWLLRow*m_ySize;
	// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

	// fill grid data
	if((tx0 > (cr+m_xSize)) && (ty0 > (cr+m_ySize))) {
		AllocateGrid();
		for(DWORD i = 0; i < m_nRow; i++) {
			DWORD nReadPos = 512 + (nWLLRow + i)*nGCol*4 + nWLLCol*4;
			file.Seek(nReadPos, CFile::begin);
			for(DWORD j = 0; j < m_nCol; j++) {
				float data;
				file.Read(&data, 4);
				m_pData[i][j] = data;
			}
		}
		file.Close();
		return TRUE;
	}
	else {
		// AfxMessageBox("Circle Out Of Grid");
		return FALSE;
	}
}

BOOL CGrid::OpenSrf7Grid(CString strFileName)
{
	// opens surfer binary grid 
	// header section	0x42525344 hex	
	long	nId, nSize, nVersion;
	// grid section		0x44495247 hex
	long	nGRow, nGCol;
	double	xLL, yLL, xSize, ySize, zMin, zMax, dRotation, dBlank;

	CFile file;
	file.Open((LPCSTR) strFileName, CFile::modeRead);
	while(file.Read(&nId, 4)) {
		// header
		if(nId == 0x42525344) {
			file.Read(&nSize, 4);
			file.Read(&nVersion, 4);
		}
		// grid section
		if(nId == 0x44495247) {
			file.Read(&nSize, 4);
			file.Read(&nGRow, 4);		// number of rows in the grid
			file.Read(&nGCol, 4);		// number of columns in the grid
			file.Read(&xLL, 8);			// X coordinate of the lower left corner of the grid
			file.Read(&yLL, 8);			// Y coordinate of the lower left corner of the grid
			file.Read(&xSize, 8);		// spacing between adjacent nodes in the X direction  (between columns)
			file.Read(&ySize, 8);		// spacing between adjacent nodes in the Y direction (between rows)
			file.Read(&zMin, 8);		// minimum Z value within the grid
			file.Read(&zMax, 8);		// maximum Z value within the grid
			file.Read(&dRotation, 8);	// not currently used
			file.Read(&dBlank, 8);		// nodes are blanked if  greater or equal to this value
			m_xSize = xSize;
			m_ySize = ySize;
			m_nRow = nGRow;
			m_nCol = nGCol;
			if( m_dRot<-360.0 || m_dRot>360.0 )
				m_dRot = 0.0;
			else
				m_dRot = dRotation;
		}
		// data section
		if(nId == 0x41544144) {
			file.Read(&nSize, 4);
			long nDataPos = (long) file.GetPosition();
			m_x0 = xLL;
			m_y0 = yLL;
			// fill grid data
			if(!m_bAllocated) {
				AllocateGrid();
				for(DWORD i = 0; i < m_nRow; i++) {
					for(DWORD j = 0; j < m_nCol; j++) {
						double data;
						file.Read(&data, 8);
						if(data == dBlank)
							m_pData[i][j] = DUMMY;
						else
							m_pData[i][j] = data;
					}
				}
			}
			else {
				AfxMessageBox("Grid allocating problem!");
				file.Close();
				Close();
				return FALSE;
			}
			file.Seek(0, CFile::end);
		}
	}
	file.Close();

	InitVariables();
	//CheckDummies(-1.0e15, 1.0e15, TRUE);
	UpdateMinMaxValue();
	SetModifiedFlag();
	return TRUE;
}

GridInfo CGrid::ReadSrf7Info(CString strFilePath)
{ 
	// reads only srf 7 header
	// opens surfer binary grid 
	// header section	0x42525344 hex	
	long	nId, nSize, nVersion;
	// grid section		0x44495247 hex
	long	nGRow, nGCol;
	double	xLL, yLL, xSize, ySize, zMin, zMax, dRotation, dBlank;
	GridInfo gi;

	CFile file;
	file.Open((LPCSTR) strFilePath, CFile::modeRead);

	file.Read(&nId, 4);
	// header
	if(nId == 0x42525344) {
		file.Read(&nSize, 4);
		file.Read(&nVersion, 4);
	}
	// grid section
	file.Read(&nId, 4);
	if(nId == 0x44495247) {
		file.Read(&nSize, 4);
		file.Read(&nGRow, 4);		// number of rows in the grid
		file.Read(&nGCol, 4);		// number of columns in the grid
		file.Read(&xLL, 8);			// X coordinate of the lower left corner of the grid
		file.Read(&yLL, 8);			// Y coordinate of the lower left corner of the grid
		file.Read(&xSize, 8);		// spacing between adjacent nodes in the X direction  (between columns)
		file.Read(&ySize, 8);		// spacing between adjacent nodes in the Y direction (between rows)
		file.Read(&zMin, 8);		// minimum Z value within the grid
		file.Read(&zMax, 8);		// maximum Z value within the grid
		file.Read(&dRotation, 8);	// not currently used
		file.Read(&dBlank, 8);		// nodes are blanked if  greater or equal to this value
	}
	file.Close();

	// fill grid info structure
	gi.strFilePath = strFilePath;
	gi.nType = SRF_7;
	gi.x0 = xLL;
	gi.y0 = yLL;
	gi.xSize = xSize;
	gi.ySize = ySize;
	gi.nCol = nGCol;
	gi.nRow = nGRow;
	gi.dRot = dRotation;

	return gi;
}

BOOL CGrid::SaveSrf7Grid(CString strFileName)
{
	// opens surfer binary grid 
	
	// header section	0x42525344 hex	
	long	nId, nSize, nVersion;
	// grid section		0x44495247 hex
	long	nGRow, nGCol;
	double	xLL, yLL, xSize, ySize, zMin, zMax, dRotation, dBlank;

	CFile file;
	file.Open((LPCSTR) strFileName, CFile::modeCreate | CFile::modeWrite);
	// header
	nId = 0x42525344;
	nSize = sizeof(long);
	nVersion = 1;
	file.Write(&nId, 4);
	file.Write(&nSize, 4);
	file.Write(&nVersion, 4);
	// grid section
	nId = 0x44495247;
	nSize = 8*sizeof(double) + 2*sizeof(long);
	nGRow = m_nRow;
	nGCol = m_nCol;
	xLL = m_x0;
	yLL = m_y0;
	xSize = m_xSize;
	ySize = m_ySize;
	dRotation = m_dRot;
	zMin = m_dMinZ;
	zMax = m_dMaxZ;
	dBlank = DUMMY;
	GetMinMaxValue(zMin, zMax);
	file.Write(&nId, 4);
	file.Write(&nSize, 4);
	file.Write(&nGRow, 4);		// number of rows in the grid
	file.Write(&nGCol, 4);		// number of columns in the grid
	file.Write(&xLL, 8);		// X coordinate of the lower left corner of the grid
	file.Write(&yLL, 8);		// Y coordinate of the lower left corner of the grid
	file.Write(&xSize, 8);		// spacing between adjacent nodes in the X direction  (between columns)
	file.Write(&ySize, 8);		// spacing between adjacent nodes in the Y direction (between rows)
	file.Write(&zMin, 8);		// minimum Z value within the grid
	file.Write(&zMax, 8);		// maximum Z value within the grid
	file.Write(&dRotation, 8);	// not currently used
	file.Write(&dBlank, 8);		// nodes are blanked if  greater or equal to this value
	// data section
	nId = 0x41544144;
	nSize = m_nRow * m_nCol * sizeof(double);
	file.Write(&nId, 4);
	file.Write(&nSize, 4);
	// fill grid data
	for(DWORD i = 0; i < m_nRow; i++) {
		for(DWORD j = 0; j < m_nCol; j++) {
			double data;
			data = m_pData[i][j];
			file.Write(&data, 8);
		}
	}
	file.Close();
	return TRUE;
}

BOOL CGrid::OpenSrf6Grid(CString strFilePath)
{
	// surfer 6 grid
	char	id[4];
	short	nx, ny;
	double	xlo, xhi, ylo, yhi, zlo, zhi;

	CFile file;
	if( file.Open((LPCSTR) strFilePath, CFile::modeRead) ) {
		file.Read(&id, 4);
		if(strstr(id, "DSBB") != NULL) {
			file.Read(&nx, 2);
			file.Read(&ny, 2);
			file.Read(&xlo, 8);
			file.Read(&xhi, 8);
			file.Read(&ylo, 8);
			file.Read(&yhi, 8);
			file.Read(&zlo, 8);
			file.Read(&zhi, 8);
			m_nRow = ny;
			m_nCol = nx;
			m_x0 = xlo;
			m_y0 = ylo;
			m_xSize = (xhi - xlo) / (nx - 1);
			m_ySize = (yhi - ylo) / (ny - 1);
			m_dMinX = xlo;
			m_dMaxX = xhi;
			m_dMinY = ylo;
			m_dMaxY = yhi;
			m_dMinZ = zlo;
			m_dMaxZ = zhi;

			if(!m_bAllocated) {
				AllocateGrid();
				for(DWORD i = 0; i < m_nRow; i++) {
					for(DWORD j = 0; j < m_nCol; j++) {
						float data;
						file.Read(&data, 4);
						m_pData[i][j] = (double) data;		
					}
				}
				InitVariables();
				CheckDummies(-1.0e15, 1.0e15, TRUE);
				UpdateMinMaxValue();
				SetModifiedFlag();
				return TRUE;
			}
			else {
				AfxMessageBox(" grid alocating problem!");
				return FALSE;
			}
		}
		else {
			AfxMessageBox("Grid type does not match.");}
	}
	return FALSE;
}


GridInfo CGrid::ReadSrf6Info(CString strFilePath)
{
	// reads srf 6 grid header and returns it in GridInfo structure
	// surfer 6 grid
	char	id[4];
	short	nx, ny;
	double	xlo, xhi, ylo, yhi, zlo, zhi, xSize, ySize;
	GridInfo gi;

	CFile file;
	file.Open((LPCSTR) strFilePath, CFile::modeRead);
	file.Read(&id, 4);
	file.Read(&nx, 2);
	file.Read(&ny, 2);
	file.Read(&xlo, 8);
	file.Read(&xhi, 8);
	file.Read(&ylo, 8);
	file.Read(&yhi, 8);
	file.Read(&zlo, 8);
	file.Read(&zhi, 8);
	xSize = (xhi - xlo) / (nx - 1);
	ySize = (yhi - ylo) / (ny - 1);
	// fill gi structure
	gi.strFilePath = strFilePath;
	gi.nType = SRF_6;
	gi.x0 = xlo;
	gi.y0 = ylo;
	gi.xSize = xSize;
	gi.ySize = ySize;
	gi.nCol = nx;
	gi.nRow = ny;
	gi.dRot = m_dRot;

	return gi;
}


BOOL CGrid::SaveSrf6Grid(CString strFileName)
{
	CFile cfile;
	cfile.Open(m_strFilePath, CFile::modeCreate | CFile::modeWrite);
	char	id[] = "DSBB";
	short	i, j, nx, ny;
	double	xlo, xhi, ylo, yhi, zlo, zhi;
	double csx, csy;
	float data;
	nx = (short) m_nCol;
	ny = (short) m_nRow;
	xlo = m_x0;
	xhi = m_x0 + m_xSize*(m_nCol-1);
	ylo = m_y0;
	yhi = m_y0 + m_ySize*(m_nRow-1);
	csx = m_xSize;
	csy = m_ySize;

	GetMinMaxValue(zlo, zhi);

	cfile.Write(&id, 4);
	cfile.Write(&nx, 2);
	cfile.Write(&ny, 2);
	cfile.Write(&xlo, 8);
	cfile.Write(&xhi, 8);
	cfile.Write(&ylo, 8);
	cfile.Write(&yhi, 8);
	cfile.Write(&zlo, 8);
	cfile.Write(&zhi, 8);
	for(i = 0; i < ny; i++) {
		for(j = 0; j < nx; j++) {
			data = (float) m_pData[i][j];
			cfile.Write(&data, 4);
		}
	}
	cfile.Close();

	return TRUE;
}


void CGrid::Close()
{
	// frees the memory and creates an empty grid 
	AllocateGrid(FALSE);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//************** aditional routines ***************************************************************
void CGrid::UpdateMinMaxValue()
{
	if( m_bModifiedMinMax ) {
		int nx = m_nCol;
		int ny = m_nRow;
		int n = 0;
		double d, dx, dy;
		m_dRMSold = m_dRMS;
		m_dRMS = 0.0;
		m_dDrvOld = m_dDrv;
		m_dDrv = 0.0;
		m_dMean = 0.0;
		m_dMeanP = 0.0;
		BOOL bVal = FALSE;
		for(int i = 0; i < ny; i++) {
			for(int j = 0; j < nx; j++) {
				d = m_pData[i][j];
				if(d != DUMMY) {
					n++;
					m_dRMS += d*d;
					m_dMean += d;
					if(!bVal) {
						m_dMinZ = d; m_dMaxZ = d; bVal = TRUE;
					}
					if(m_dMinZ > d) {
						m_dMinZ = d;
						m_nRowMin = i;
						m_nColMin = j;
					}
					if(m_dMaxZ < d) {
						m_dMaxZ = d;
						m_nRowMax = i;
						m_nColMax = j;
					}

					// compute fake derivative
					if( j < nx-1) {
						dx = m_pData[i][j+1];
						if(d!=DUMMY && dx!=DUMMY) {
							m_dDrv += fabs(dx-d)/m_xSize;
						}
					}
					if( i < ny-1) {
						dy = m_pData[i+1][j];
						if(d!=DUMMY && dy!=DUMMY) {
							m_dDrv += fabs(dy-d)/m_ySize;
						}
					}
				}

			} // for(j)
		}	// for(i)
		m_dRMS = sqrt( m_dRMS/(n) );
		m_dMean /= n;
		m_bModifiedMinMax = FALSE;
	}
}

void CGrid::UpdateDerivative()
{
	if( m_bModifiedDrv ) {
		int nx = m_nCol;
		int ny = m_nRow;
		double d, dx, dy;
		m_dDrvOld = m_dDrv;
		m_dDrv = 0.0;
		for(int i = 0; i < ny-1; i++) {
			for(int j = 0; j < nx-1; j++) {
				d = m_pData[i][j];
				dx = m_pData[i][j+1];
				dy = m_pData[i+1][j];
				if(d!=DUMMY && dx!=DUMMY) {
					m_dDrv += fabs(dx-d)/m_xSize;
				}
				if(d!=DUMMY && dy!=DUMMY) {
					m_dDrv += fabs(dy-d)/m_ySize;
				}
			}
		}
		m_bModifiedDrv = FALSE;
	}
}

void CGrid::GetMinMaxValue(double &dMin, double &dMax)
{
	UpdateMinMaxValue();
	dMin = m_dMinZ;
	dMax = m_dMaxZ;
}



void CGrid::SetModifiedFlag(BOOL bFlag)
{
	m_bModified = bFlag;
	m_bModifiedMinMax = bFlag;
	m_bModifiedContours = bFlag;
	m_bModifiedImage = bFlag;
	m_bModfHistogram = bFlag;
}

GridInfo CGrid::GetInfo()
{
	GridInfo grdInf;

	grdInf.nCol = m_nCol;
	grdInf.nRow = m_nRow;
	grdInf.nType = m_nType;
	grdInf.x0 = m_x0;
	grdInf.y0 = m_y0;
	grdInf.xSize = m_xSize;
	grdInf.ySize = m_ySize;
	grdInf.strFilePath = m_strFilePath;
	grdInf.dMaxX = m_dMaxX;
	grdInf.dMaxY = m_dMaxY;
	grdInf.dMaxZ = m_dMaxZ;
	grdInf.dMinX = m_dMinX;
	grdInf.dMinY = m_dMinY;
	grdInf.dMinZ = m_dMinZ;
	grdInf.dRot = m_dRot;

	return grdInf;
}

GridInfo CGrid::GetInfo(CString strFilePath, int nType)
{
	GridInfo gi;
	m_nType = (GrdType) nType;
	m_strFilePath = strFilePath;

	// opens a grid and reads only header
	if(	m_nType == SRF_6)	// surfer 6.0
		return ReadSrf6Info(m_strFilePath);
	if(	m_nType == SRF_7 )	// surfer 7.0 
		return ReadSrf7Info(m_strFilePath);
	if( m_nType == GEOS_KX1 )	{// geosoft default kx = 1;
		//AfxMessageBox("Not available");
		//return FALSE;
	}
	gi.nType = -1;
	return gi;
}

GridInfo CGrid::GetInfoBrowse()
{
	// browse grid and returne grid infos
	// !!! use only for this purpose
	CString strFilePath;
	GridInfo gi;
	gi.nType = -1;	// dummy value for checking

	CFileDialog dlg(TRUE, "grd", "*.grd", OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "Surfer 6.0 Grid (*.grd)|*.grd|Surfer 7.0 Grid (*.grd)|*.grd|Geosoft Grid (*.grd)|*.grd||");
	if(dlg.DoModal() == IDOK) {
		strFilePath = dlg.GetPathName();
		int nType = ReadType(dlg.GetPathName());
		switch(nType) {
			case SRF_6:
				return ReadSrf6Info(strFilePath);	// surfer 6.0
			case SRF_7:
				return ReadSrf7Info(strFilePath);	// surfer 7.0 
		}
	}

	return gi;
}

BOOL CGrid::GetInfoBrowse(GridInfo& gi)
{
	// browse grid and returne grid infos
	// !!! use only for this purpose
	CString strFilePath;
	gi.nType = -1;	// dummy value for checking

	CFileDialog dlg(TRUE, "grd", "*.grd", OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "Surfer 6.0 Grid (*.grd)|*.grd|Surfer 7.0 Grid (*.grd)|*.grd|Geosoft Grid (*.grd)|*.grd||");
	if(dlg.DoModal() == IDOK) {
		strFilePath = dlg.GetPathName();
		int nType = ReadType(dlg.GetPathName());
		switch(nType) {
			case SRF_6:
				gi =  ReadSrf6Info(strFilePath);	// surfer 6.0
			case SRF_7:
				gi =  ReadSrf7Info(strFilePath);	// surfer 7.0 
			default: 
				return FALSE;
		}
		return TRUE;
	}

	return FALSE;
}

double** CGrid::GetData()
{
	if(m_bAllocated) 
		return m_pData;
	else
		return NULL;
}


BOOL CGrid::IsEmpty()
{
	if( m_bAllocated )
		return FALSE;
	else
		return TRUE;
}

BOOL CGrid::IsVisible()
{
	if( m_bAllocated && m_bVisible )
		return TRUE;
	else
		return FALSE;
}

void CGrid::SetInfo(CString strFilePath, int nType)
{
	m_strFilePath = strFilePath;
	m_nType = (GrdType) nType;
}

void CGrid::ZeroData()
{
	for(DWORD i = 0; i < m_nRow; i++) {
		for(DWORD j = 0; j < m_nCol; j++) {
			m_pData[i][j] = 0;
		}
	}
}

void CGrid::DummyData()
{
	SetValue(DUMMY);
}

void CGrid::SetValue(double val)
{
	for(DWORD i = 0; i < m_nRow; i++) {
		for(DWORD j = 0; j < m_nCol; j++) {
			m_pData[i][j] = val;
		}
	}
}

int CGrid::SetValueEx(double val, int tag)
{
	ASSERT(tag!=-1 || tag!=1);
	double v;
	int n=0;

	for(DWORD i = 0; i < m_nRow; i++) {
		for(DWORD j = 0; j < m_nCol; j++) {
			v = m_pData[i][j];
			switch(tag) {
				case 1:
					if( val > v)	{ m_pData[i][j] = val; n++;}
					else			m_pData[i][j] = DUMMY;
					break;
				case -1:
					if( val < v)	{ m_pData[i][j] = val; n++;}
					else			m_pData[i][j] = DUMMY;
					break;
			}
		}
	}

	return n;
}


void CGrid::SetValue(int i, int j, double val)
{
	m_pData[i][j] = val;
}

int CGrid::SetColorTable(int nTag)
{
	// sets color table; returns previous color table
	int prevTbl = m_nClrTbl;
	m_nClrTbl =	nTag;
	m_bModifiedColorTable = TRUE;
	InitColorTable();
	return prevTbl;
}


// contouring part
#define xsect(p1,p2) (h[p2]*xh[p1]-h[p1]*xh[p2])/(h[p2]-h[p1])
#define ysect(p1,p2) (h[p2]*yh[p1]-h[p1]*yh[p2])/(h[p2]-h[p1])

BOOL CGrid::Contour()
{
	if( m_bModifiedContours ) {
		if( CreateContourLevels() ) {
			double *x;
			double *y;
			x = (double *) malloc(m_nRow * sizeof(double));
			y = (double *) malloc(m_nCol  * sizeof(double));
			int a;
			for(a = 0; a < (int) m_nRow; a++) {
				x[a] = m_dMinY + m_ySize*a;
			}
			for(a = 0; a < (int) m_nCol; a++) {
				y[a] = m_dMinX + m_xSize*a;
			}

			
			// m_pData           ! matrix of data to contour
			// x               ! data matrix column coordinates
			// y               ! data matrix row coordinates
			// nc              ! number of contour levels
			// z               ! contour levels in increasing order
			int nc = m_cntrs.GetSize();
			int m1, m2, m3, case_value;
			double dmin, dmax, x1, x2, y1, y2;
			register int i, j, k, m;
			double h[5];
			int sh[5];
			double xh[5],yh[5];
			//===========================================================================
			// The indexing of im and jm should be noted as it has to start from zero
			// unlike the fortran counter part
			//===========================================================================
			//int im[4] = {0,1,1,0},jm[4]={0,0,1,1};
			int im[4] = {0,0,1,1}, jm[4]={0,1,1,0};
			//===========================================================================
			// Note that castab is arranged differently from the FORTRAN code because
			// Fortran and C/C++ arrays are transposed of each other, in this case
			// it is more tricky as castab is in 3 dimension
			//===========================================================================
			int castab[3][3][3] =
			{
				{
					{0,0,8},{0,2,5},{7,6,9}
				},
				{
					{0,3,4},{1,3,1},{4,3,0}
				},
				{
					{9,6,7},{5,2,0},{8,0,0}
				}
			};
			for( i=0; i < (int)m_nRow-1; i++) {
				for( j=0; j <(int)m_nCol-1; j++) {
					double temp1,temp2;
					temp1 = min(m_pData[i][j], m_pData[i][j+1]);
					temp2 = min(m_pData[i+1][j], m_pData[i+1][j+1]);
					dmin = min(temp1, temp2);
					temp1 = max(m_pData[i][j], m_pData[i][j+1]);
					temp2 = max(m_pData[i+1][j], m_pData[i+1][j+1]);
					dmax = max(temp1, temp2);
					if (dmax >= m_cntrs[0].z && dmin <= m_cntrs[nc-1].z) {
						for (k=0; k<nc; k++) {
							if (m_cntrs[k].z >= dmin && m_cntrs[k].z <= dmax) {
								for (m=4; m>=0; m--) {
									if (m>0) {
										//=============================================================
										// The indexing of im and jm should be noted as it has to
										// start from zero
										//=============================================================
										h[m] = m_pData[i+im[m-1]][j+jm[m-1]] - m_cntrs[k].z;
										xh[m] = x[i+im[m-1]];
										yh[m] = y[j+jm[m-1]];
									} 
									else {
										h[0] = 0.25*(h[1]+h[2]+h[3]+h[4]);
										xh[0]=0.5*(x[i]+x[i+1]);
										yh[0]=0.5*(y[j]+y[j+1]);
									}
									if (h[m]>0.0) {
										sh[m] = 1;
									}
									else if (h[m]<0.0) {
										sh[m] = -1;
									}
									else
										sh[m] = 0;
								}
								//=================================================================
								//
								// Note: at this stage the relative heights of the corners and the
								// centre are in the h array, and the corresponding coordinates are
								// in the xh and yh arrays. The centre of the box is indexed by 0
								// and the 4 corners by 1 to 4 as shown below.
								// Each triangle is then indexed by the parameter m, and the 3
								// vertices of each triangle are indexed by parameters m1,m2,and
								// m3.
								// It is assumed that the centre of the box is always vertex 2
								// though this isimportant only when all 3 vertices lie exactly on
								// the same contour level, in which case only the side of the box
								// is drawn.
								//
								//
								//      vertex 4 +-------------------+ vertex 3
								//               | \               / |
								//               |   \    m=3    /   |
								//               |     \       /     |
								//               |       \   /       |
								//               |  m=2    X   m=2   |       the centre is vertex 0
								//               |       /   \       |
								//               |     /       \     |
								//               |   /    m=1    \   |
								//               | /               \ |
								//      vertex 1 +-------------------+ vertex 2
								//
								//
								//
								//               Scan each triangle in the box
								//
								//=================================================================
								for (m=1;m<=4;m++) {
									m1 = m;
									m2 = 0;
									if (m!=4)
										m3 = m+1;
									else
										m3 = 1;
									case_value = castab[sh[m1]+1][sh[m2]+1][sh[m3]+1];
									if (case_value!=0) {
										switch (case_value) {
											//===========================================================
											//     Case 1 - Line between vertices 1 and 2
											//===========================================================
										case 1:
											x1=xh[m1];
											y1=yh[m1];
											x2=xh[m2];
											y2=yh[m2];
											break;
											//===========================================================
											//     Case 2 - Line between vertices 2 and 3
											//===========================================================
										case 2:
											x1=xh[m2];
											y1=yh[m2];
											x2=xh[m3];
											y2=yh[m3];
											break;
											//===========================================================
											//     Case 3 - Line between vertices 3 and 1
											//===========================================================
										case 3:
											x1=xh[m3];
											y1=yh[m3];
											x2=xh[m1];
											y2=yh[m1];
											break;
											//===========================================================
											//     Case 4 - Line between vertex 1 and side 2-3
											//===========================================================
										case 4:
											x1=xh[m1];
											y1=yh[m1];
											x2=xsect(m2,m3);
											y2=ysect(m2,m3);
											break;
											//===========================================================
											//     Case 5 - Line between vertex 2 and side 3-1
											//===========================================================
										case 5:
											x1=xh[m2];
											y1=yh[m2];
											x2=xsect(m3,m1);
											y2=ysect(m3,m1);
											break;
											//===========================================================
											//     Case 6 - Line between vertex 3 and side 1-2
											//===========================================================
										case 6:
											x1=xh[m1];
											y1=yh[m1];
											x2=xsect(m1,m2);
											y2=ysect(m1,m2);
											break;
											//===========================================================
											//     Case 7 - Line between sides 1-2 and 2-3
											//===========================================================
										case 7:
											x1=xsect(m1,m2);
											y1=ysect(m1,m2);
											x2=xsect(m2,m3);
											y2=ysect(m2,m3);
											break;
											//===========================================================
											//     Case 8 - Line between sides 2-3 and 3-1
											//===========================================================
										case 8:
											x1=xsect(m2,m3);
											y1=ysect(m2,m3);
											x2=xsect(m3,m1);
											y2=ysect(m3,m1);
											break;
											//===========================================================
											//     Case 9 - Line between sides 3-1 and 1-2
											//===========================================================
										case 9:
											x1=xsect(m3,m1);
											y1=ysect(m3,m1);
											x2=xsect(m1,m2);
											y2=ysect(m1,m2);
											break;
										default:
											break;
										}
										//=============================================================
										DBLPOINT dPt;
										dPt.x = y1; dPt.y = x1;
										m_cntrs[k].pts.Add(dPt);
										dPt.x = y2; dPt.y = x2;
										m_cntrs[k].pts.Add(dPt);
										// colors 
										//=============================================================
										//printf("%f %f %f %f %f\n",x1,y1,x2,y2,z[k]);
									}
						}
					}
					}
				}
				}
			}
			free(x);
			free(y);
			m_bModifiedContours = FALSE;
			m_bContoured = TRUE;
			}	//if(CreateContourLevels()
			else {	 
				m_bContoured = FALSE;
			}
	}

	return TRUE;
}

BOOL CGrid::CreateContourLevels(void)
{
	double	dz;	
	double	v;
	int	nc = m_nContNum;		// number of colours

	UpdateMinMaxValue();
	if( m_dMaxZ == m_dMinZ ) {
		AfxMessageBox("Grid is planar image creation skipped.", MB_OK | MB_ICONINFORMATION);
		return 0;
	}

	ComputeHistogram();

	m_cntrs.RemoveAll();
	m_cntrs.SetSize(nc);

	double dMinHst, dMaxHst;
	GetHistogramMinMax( dMinHst, dMaxHst );
	dz = (dMaxHst-dMinHst)/(nc-1);
	for(int i = 0; i < nc; i++) {
		v = m_cntrs[i].z = dMinHst + dz*i;
		 m_clrGrad.GetColorFromValue(v, m_cntrs[i].color, m_bHistClr);
		//if( v >= dMinHst && v <= dMaxHst) {
		//	v = GetHistogramValue(v);
		//	m_cntrs[i].color = m_clrGrad.ColourFromPosition( (float)v );;
		//}
	}

	return TRUE;
}

BOOL CGrid::DrawContours(CDC* pDC, double scx, double scy, double offx, double offy)
{
	if(m_bAllocated /*&& m_bShowContour*/) {
		if(IsPlanar()) return 0;
		Contour();
		if( m_bContoured ){
			for(int i = 0; i < m_cntrs.GetSize(); i++) {
				CPen pen;
				COLORREF clr;
				m_clrGrad.GetColorFromValue( m_cntrs[i].z, clr, m_bHistClr);
				pen.CreatePen(PS_SOLID, 1, clr/*m_cntrs[i].color*/);
				CPen *pPen = pDC->SelectObject(&pen);
				int x, y;
				for(int j = 0; j < m_cntrs[i].pts.GetSize(); j++) {
					DBLPOINT dPt = m_cntrs[i].pts.GetAt(j); j++;
					x = (int) (offx + scx*dPt.x); y = (int) (offy + scy*dPt.y);
					pDC->MoveTo( x, y);
					dPt = m_cntrs[i].pts.GetAt(j);
					x = (int) (offx + scx*dPt.x); y = (int) (offy + scy*dPt.y);
					pDC->LineTo( x, y);

				}
				pDC->SelectObject(pPen);
			}
		}
	}
	else {
		TRACE("Trying to draw an empty grid in CGrig.Draw(). \n");
	}
	return FALSE;
}

void CGrid::ShowContours(BOOL show)
{
	 m_bShowContour = show;
}

BOOL CGrid::IsVisibleContour(void)
{
	if(m_bAllocated && m_bShowContour) 
		return TRUE;
	return FALSE;
}


int CGrid::CheckDummies(double min, double max, BOOL bReplace)
{
	int n;

	n=0;
	for(DWORD i = 0; i < m_nRow; i++) {
		for(DWORD j = 0; j < m_nCol; j++) {
			if(	m_pData[i][j] <= min || m_pData[i][j] >= max ) {
				if(bReplace) m_pData[i][j]=DUMMY;	
				n++;
			}
		}
	}
	return n;
}

int CGrid::CheckDummies()
{
	int n;
	double min=-1.0e7, max=1.0e+7;

	n=0;
	for(DWORD i = 0; i < m_nRow; i++) {
		for(DWORD j = 0; j < m_nCol; j++) {
			if(	m_pData[i][j] <= min || m_pData[i][j] >= max ) {
				m_pData[i][j] = DUMMY;
				n++;
			}
		}
	}
	UpdateMinMaxValue();
	return n;
}

int CGrid::FillDummies( double dVal, double dTrashMin, double dTrashMax)
{
	int n;

	n=0;
	for(DWORD i = 0; i < m_nRow; i++) {
		for(DWORD j = 0; j < m_nCol; j++) {
			if(	m_pData[i][j] <= dTrashMin || m_pData[i][j] >= dTrashMax ) {
				m_pData[i][j] = dVal;
				n++;
			}
		}
	}
	UpdateMinMaxValue();
	return n;
}

double CGrid::GetValue(int row, int col)
{
	if( !IsEmpty() && 
		row>=0 && row<(int)m_nRow &&
		col>=0 && col<(int)m_nCol ) {
		return m_pData[row][col];
	}
	return DUMMY;
}

BOOL CGrid::GetPt3D( int row, int col, CPoint3D& pt)
{
	if( !IsEmpty() ) {
		pt.x = m_x0 + col*m_xSize; 
		pt.y = m_y0 + row*m_ySize;	
		pt.z = m_pData[row][col];
		return	!IsDummy(row, col);
	}
	return FALSE;
}


//int CGrid::CreateImage(CImage &img, int nWidth, int nHeiht, int nColStart, int nColEnd, int nRowStart, int nRowEnd,int nTag)
//{
//}

int CGrid::CreateImage(CImage &img, int nWidth, int nHeiht, int nTag)
{
	int		k, l;
	int		i, j;

	UpdateMinMaxValue();
	if( m_dMaxZ == m_dMinZ ) {
		AfxMessageBox("Grid is planar image creation skipped.", MB_OK | MB_ICONINFORMATION);
		return 0;
	}

	// compute histogram
	ComputeHistogram();

	// initializing
	img.Destroy();
	if( nWidth < (int)m_nCol)
		nWidth = m_nCol;
	if( nHeiht < (int)m_nRow)
		nHeiht = m_nRow;

	if( nWidth >= 800)	nWidth = 800;
	if( nHeiht >= 800)	nHeiht = 800;

	double ck, cl;	// cell size k, l
	double a, b;	// cell size k, l
	double v, v00, v01, v11, v10;
	//float fc;
	cl = (double)(m_nCol-1) / (nWidth-1);
	ck = (double)(m_nRow-1) / (nHeiht-1);

	COLORREF clr;
	COLORREF white=RGB(255,255,255);
	double dMinHst, dMaxHst;
	GetHistogramMinMax( dMinHst, dMaxHst );
	if(img.Create(nWidth, nHeiht, 32) ) {
		for( k = 0; k < (int)nHeiht; k++) {
			for( l = 0; l < (int)nWidth; l++) {
				i = (int) floor( k*ck );	// (k*ck)/1 cell size x & y are assumed 1.0
				j = (int) floor( l*cl );
				if( i > (int)m_nRow-2) i= m_nRow-2;
				if( j > (int)m_nCol-2) j= m_nCol-2;
				
				v00 = m_pData[i][j];
				v01 = m_pData[i][j+1];
				v11 = m_pData[i+1][j+1];
				v10 = m_pData[i+1][j];
				if( v00!=DUMMY && v01!=DUMMY && v11!=DUMMY && v10!=DUMMY ) {
					a = k*ck - i;
					b = l*cl - j;
					v = (1 - b)*(1 - a)*v00 + (1 - b)*a*v10 + b*(1 - a)*v01 + b*a*v11;
					
					m_clrGrad.GetColorFromValue(v, clr, m_bHistClr);
					img.SetPixel(l, k, clr );
				}
				else {
					img.SetPixel(l, k, white );
				}
				////m_clrGrad.
				//if( v > m_dMaxZ ) 
				//	img.SetPixelRGB(l, k, 255, 255, 255);
				//else {
				//	if( v < dMinHst) 
				//		v = dMinHst;
				//	if( v > dMaxHst) 
				//		v = dMaxHst;
				//	
				//	fc = (float) GetHistogramValue(v);;
				//	img.SetPixel(l, k, m_clrGrad.ColourFromPosition(fc) );
				//}
			}
		}
		return 1;
	}
	else {
		AfxMessageBox("Problem creating Image.");
	}

	return 0;
}

BOOL CGrid::GetGL(DoubleArray* pData, int nFormat, double min, double max)
{
	UpdateMinMaxValue();
	if( m_dMaxZ == m_dMinZ ) {
		AfxMessageBox("Grid is planar image creation skipped.", MB_OK | MB_ICONINFORMATION);
		return 0;
	}

	// floatData size: rows * cols * nFormat {7 = 3[x,y,z] + 3[nx,ny,nz] + colorFromColorTable}
	pData->SetSize(m_nRow*m_nCol*nFormat);

	// compute histogram
	ComputeHistogram();

	int		nIndex;

	// data scaling
	double scD, shD;	// data scale & shift
	double dMinHst, dMaxHst;
	GetHistogramMinMax( dMinHst, dMaxHst );
	BOOL	bScale=FALSE;
	if( (min != 1.0) || (max != 1.0) ) {
		scD = (max-min)/(dMaxHst-dMinHst);
		shD = min - scD*dMinHst;
		bScale = TRUE;
	}
	else {
		scD = 1.0;
		shD = 0.0;
	}

	int m = m_clrTbl.GetSize();
	for(int i = 0; i < (int)m_nRow; i++) {
		for(int j = 0; j < (int)m_nCol; j++) {
			double v, vv;
			vv = v = m_pData[i][j];
			//dc = GetHistogramValue(v);
			nIndex = nFormat*(i*m_nCol+j);

			// fill data array
			if(bScale && v != DUMMY)	v = shD + scD*v;	
			pData->SetAt(nIndex + 0, GetX(i, j)	);				// xGL
			pData->SetAt(nIndex + 1, v	);						// yGL
			pData->SetAt(nIndex + 2, -GetY(i, j) );				// zGL

			pData->SetAt(nIndex + 3, 0.0f);						// nx
			pData->SetAt(nIndex + 4, 0.0f);						// ny
			pData->SetAt(nIndex + 5, 0.0f);						// nz

			pData->SetAt(nIndex + 6, vv);			// color
		}
	}

	return 1;
}

BOOL CGrid::GetGlRel(DoubleArray* pData, int nFormat, double min, double max, CGrid* pRel)
{
	if( pRel==NULL || !IsSameSize(pRel) ) {
		return GetGL(pData, nFormat, min, max);
	}

	UpdateMinMaxValue();
	if( m_dMaxZ == m_dMinZ ) {
		AfxMessageBox("Grid is planar image creation skipped.", MB_OK | MB_ICONINFORMATION);
		return 0;
	}

	// floatData size: rows * cols * nFormat {7 = 3[x,y,z] + 3[nx,ny,nz] + colorFromColorTable}
	pData->SetSize(m_nRow*m_nCol*nFormat);

	// compute histogram
	ComputeHistogram();

	int		nIndex;

	// data scaling
	double scD, shD;	// data scale & shift
	BOOL	bScale=FALSE;
	double dMinHst, dMaxHst;
	GetHistogramMinMax( dMinHst, dMaxHst );
	if( (min != 1.0) || (max != 1.0) ) {
		scD = (max-min)/(dMaxHst-dMinHst);
		shD = min - scD*dMinHst;
		bScale = TRUE;
	}
	else {
		scD = 1.0;
		shD = 0.0;
	}

	double** pRelData = pRel->GetData();
	int m = m_clrTbl.GetSize();
	for(int i = 0; i < (int)m_nRow; i++) {
		for(int j = 0; j < (int)m_nCol; j++) {
			double v, vv;
			vv = v = m_pData[i][j];
			//dc = GetHistogramValue(v);
			nIndex = nFormat*(i*m_nCol+j);

			// fill data array
			if(v != DUMMY)	v = pRelData[i][j];
			pData->SetAt(nIndex + 0, GetX(i, j)/*m_x0 + j*m_xSize*/	);			// xGL
			pData->SetAt(nIndex + 1, v);							// yGL
			pData->SetAt(nIndex + 2, -GetY(i, j)/*(m_y0 + i*m_ySize)*/ );		// zGL

			pData->SetAt(nIndex + 3, 0.0f);						// nx
			pData->SetAt(nIndex + 4, 0.0f);						// ny
			pData->SetAt(nIndex + 5, 0.0f);						// nz

			pData->SetAt(nIndex + 6, vv);						// color
		}
	}

	return 1;
}

int	CGrid::CreateImageLegeng(CImage &img, int nWidth, int nHeiht, int nTag)
{
	UpdateMinMaxValue();
	if( m_dMaxZ == m_dMinZ ) {
		AfxMessageBox("Grid is planar image creation skipped.", MB_OK | MB_ICONINFORMATION);
		return 0;
	}

	// histogram shoul be computed compute histogram
	//ComputeHistogram();

	// initializing
	img.Destroy();

	//float	fc;
	double	dk, v;
	COLORREF clr;

	double dMinHst, dMaxHst;
	GetHistogramMinMax( dMinHst, dMaxHst );
	dk = (dMaxHst-dMinHst)/(nHeiht);

	if( img.Create(nWidth, nHeiht, 32) ) {
		for(int k = 0; k < (int)nHeiht; k++) {
			v = dMinHst + dk*k;
			m_clrGrad.GetColorFromValue(v, clr, m_bHistClr);

			//fc = (float)GetHistogramValue(v);
			//clr = m_clrGrad.ColourFromPosition(fc);
			for(int l = 0; l < (int)nWidth; l++) {
				img.SetPixel(l, k, clr );
			}
		}
	}
	else {
		AfxMessageBox("Problem creating legeng image.");
	}

	return 0;
}

void Normal(double &nx, double &ny, double &nz, double x1, double y1, double z1,  double x2, double y2, double z2) 
{
	nx = +y1*z2 - z1*y2;
	ny = +z1*x2 - x1*z2;
	nz = +x1*y2 - y1*x2;
}


BOOL CGrid::IsPlanar(void)
{
	return m_dMinZ == m_dMaxZ;
}

BOOL CGrid::IsSameSize(int rows, int cols, double x0, double y0, double xSize, double ySize)
{
	if(	rows == m_nRow &&
		cols == m_nCol &&
		x0 == m_x0 &&
		y0 == m_y0 &&
		xSize == m_xSize &&
		ySize == m_ySize ) 
	{
		return TRUE;
	}

	return 0;
}

BOOL CGrid::IsSameSize(GridInfo gi)
{
	if(	gi.nCol == m_nCol &&
		gi.nRow == m_nRow &&
		gi.x0 == m_x0 &&
		gi.y0 == m_y0 &&
		gi.xSize == m_xSize &&
		gi.ySize == m_ySize &&
		gi.dRot == m_dRot) 
	{
		return TRUE;
	}

	return 0;
}

BOOL CGrid::IsSameSize(CGrid* pGrd)
{
	GridInfo gi;
	gi = pGrd->GetInfo();

	return IsSameSize(gi);
}

//inline double CGrid::GetHistogramValue(double val)
//{
//	int		n, i;
//	double	ret;
//	double	dMinHst, dMaxHst;
//
//	//ASSERT(val>=m_dMinHst || val<=m_dMaxHst);
//
//	GetHistogramMinMax(dMinHst, dMaxHst);
//	if(val<dMinHst)
//		val = dMinHst;
//	if( val>dMaxHst)
//		val = dMaxHst;
//
//	if( m_bHistClr ) {
//		n = m_hist.GetCount();
//		i = (int) floor((n-1)*(val - dMinHst) / (dMaxHst-dMinHst));
//		ASSERT(i<n);
//		ret =  m_hist[i];
//	}
//	else {
//		ret = (val - dMinHst) / (dMaxHst-dMinHst);
//		if(ret<0.0) ret = 0;
//		if(ret>1.0) ret = 1.0;
//	}
//
//	return ret;
//}

inline void CGrid::GetHistogramMinMax(double& min, double& max)
{
	if( !m_bCustomRange ) {
		min = m_dMinHst;
		max = m_dMaxHst;
	}
	else {
		min = m_dMinHstCst;
		max = m_dMaxHstCst;
	}
}

int CGrid::ComputeHistogram(void)
{
	if( m_bModfHistogram ) {
		// difference field color
		double dMinHst, dMaxHst;
		if( !m_bCustomRange ) {
			if( m_nClrTbl == CTBL_DIF) {
				double tmp;
				tmp = max( fabs(m_dMaxZ), fabs(m_dMinZ) );
				m_dMinHst = -tmp;
				m_dMaxHst = tmp;
			}
			else {
				m_dMinHst = m_dMinZ;
				m_dMaxHst = m_dMaxZ;
			}
			dMinHst = m_dMinHst;
			dMaxHst = m_dMaxHst;
		}
		else {
			dMinHst = m_dMinHstCst;
			dMaxHst = m_dMaxHstCst;
		}
		m_clrGrad.SetHstMin(dMinHst);
		m_clrGrad.SetHstMax(dMaxHst);
		if(m_dMinHstCst==DUMMY || m_dMinHstCst==DUMMY ) {
			m_dMinHstCst = dMinHst;
			m_dMaxHstCst = dMaxHst;
		}

		// initialization
		int i, j, k;
		int n = m_nCol*m_nRow;
		if( n > 1024 )	n = 1024;
		double dv = (dMaxHst-dMinHst)/(n-1);
		DoubleArray* pHstA;
		double* pHst;

		pHstA = m_clrGrad.GetHistogram();
		pHstA->SetSize( n );
		pHst = pHstA->GetData();

		for( i = 0; i < n; i++)
			pHst[i]=0;
		for( i = 0; i < (int)m_nRow; i++) {
			for( j = 0; j < (int)m_nCol; j++) {
				double d = m_pData[i][j];
				if( d != DUMMY ) {
					k = (int) floor((n-1)*(d - dMinHst) / (dMaxHst-dMinHst));
					if( k<0 )		k=0;
					if( k>(n-1) )	k=n-1;
					ASSERT(k<n);
					pHst[k]+=1.0;
				}
			}
		}
		// kumulativ
		for( i = 1; i < n; i++) {
			pHst[i] += pHst[i-1];
		}
		
		double tmp = pHst[0];
		double m = (double)(n)/(m_nCol*m_nRow);
		for( i = 0; i < n; i++) {
			// equalize & normalize
			pHst[i] = (pHst[i]-tmp)/(pHst[n-1]-tmp);
		}

		m_bModfHistogram = FALSE;
	}
	return 0;
}


int CGrid::CreateColorTableGL(FloatArray& fA, int n, int nFormat, int nClrTable, float fAlpha)
{
	// pTbl pointer to array of 1D texture
	// number of elements in pTbl = n*nFormat; nFormat must be 3, 4
	// n MUST be power of 2
	//
	TRACE("Entering CGrid::CreateColorTableGL(int n)\n");

	if( (nFormat!=3) && (nFormat!=4) ) {
		AfxMessageBox("Invalid format in Grid::CreateColorTableGL(...)");
		return 0;
	}

	ComputeHistogram();

	//double		d;
	//float		fr, fg, fb;
	float*		pTbl;				// pointer to float data

	fA.SetSize(n*nFormat);
	pTbl = fA.GetData();

	m_clrGrad.CreateColorTableGL(fA.GetData(), n, nFormat, fAlpha, m_bHistClr);
	
	return 0;
}

//void CGrid::GetColorf(int i, int j, float &r, float &g, float &b, int *h, int nHst, double dv, double th)
//{
//	int		nnh, nClrTbl;	// histogram entry number
//	double	v;		// grid value
//	double	dColor;
//	RGBQUAD rgbQ0, rgbQ1;
//
//	v = m_pData[i][j];
//	nnh = (int) ((v-m_dMinZ)/dv);
//	dColor = th * h[nnh];
//	nClrTbl = (int)floor(dColor);
//	dColor = dColor - floor(dColor);
//	rgbQ0 = m_rgbTable.GetAt(nClrTbl);
//	rgbQ1 = m_rgbTable.GetAt(nClrTbl+1);
//	r = (float) ( rgbQ0.rgbRed		+ dColor*(rgbQ1.rgbRed		- rgbQ0.rgbRed) )/255.0f;
//	g = (float) ( rgbQ0.rgbGreen	+ dColor*(rgbQ1.rgbGreen	- rgbQ0.rgbGreen) )/255.0f;
//	b = (float) ( rgbQ0.rgbBlue		+ dColor*(rgbQ1.rgbBlue		- rgbQ0.rgbBlue) )/255.0f;
//}
//
int CGrid::ReadType(CString strFilePath)
{
	CFile file;
	if( file.Open((LPCSTR) strFilePath, CFile::modeRead) ) {

		// srf binary grid
		char id[4];
		file.Read(&id, 4);
		if(strstr(id, "DSBB") != NULL) {
			file.Close();
			return SRF_6;
		}

		// srf 7 grid
		file.SeekToBegin();
		long	nId, nSize, nVersion;
		file.Read(&nId, 4);
		// header
		if(nId == 0x42525344) {
			file.Read(&nSize, 4);
			file.Read(&nVersion, 4);
			if(nVersion == 1) {
				file.Close();
				return SRF_7;
			}
		}

		AfxMessageBox("Unknown file type.", MB_OK | MB_ICONINFORMATION);
		file.Close();
	}
	else {
		AfxMessageBox("Failed to open grid file", MB_OK | MB_ICONINFORMATION);
	}
	return -1;
}

void CGrid::GetLegendRange(double& min, double& max)
{
	UpdateMinMaxValue();


	GetHistogramMinMax(min, max);

	return;

	//if( m_nClrTbl == CTBL_DIF) {
	//	double tmp;
	//	tmp = max( fabs(m_dMaxZ), fabs(m_dMinZ) );
	//	min = -tmp;
	//	max = tmp;
	//}
	//else {
	//	min = m_dMinZ;
	//	max = m_dMaxZ;
	//}
}


BOOL CGrid::GetRowColMin(int& row, int& col)
{
	if( !IsEmpty() && m_nRowMin>=0 && m_nRowMin<(int)m_nRow && m_nColMin>=0 && m_nColMin<(int)m_nCol ) {
		row = m_nRowMin;
		col = m_nColMin;
		return TRUE;
	}

	return FALSE;
}

BOOL CGrid::GetRowColMax(int& row, int& col)
{
	if( !IsEmpty() && m_nRowMax>=0 && m_nRowMax<(int)m_nRow && m_nColMax>=0 && m_nColMax<(int)m_nCol ) {
		row = m_nRowMax;
		col = m_nColMax;
		return TRUE;
	}

	return FALSE;
}

// copies grid data to another buffer
// pCopy MUST be of the same size
void CGrid::CopyData(double** pCopy)
{
	ASSERT(pCopy!=NULL);

	for(DWORD i = 0; i < m_nRow; i++) {
		for(DWORD j = 0; j < m_nCol; j++) {
			pCopy[i][j] = m_pData[i][j];
		}
	}
}

// copies grid dada to pGrd
void CGrid::CopyData(CGrid* pGrd)
{
	ASSERT( pGrd!=NULL );
	ASSERT( !pGrd->IsEmpty() );
	ASSERT( IsSameSize(pGrd) );

	double** data;

	data = pGrd->GetData();
	ASSERT(data!=NULL);
	CopyData(data);

}

// compares this grid with pGrdCompare and dummies values according to tag
int CGrid::DummyDataEx(CGrid* pGrdCompare, int tag)
{
	ASSERT( pGrdCompare!=NULL );
	ASSERT( !pGrdCompare->IsEmpty() );
	ASSERT( IsSameSize(pGrdCompare) );

	double** data;

	data = pGrdCompare->GetData();
	ASSERT(data!=NULL);

	int n=0;
	for(DWORD i = 0; i < m_nRow; i++) {
		for(DWORD j = 0; j < m_nCol; j++) {
			switch(tag) {
				case DMX_EQUAL:
					if(data[i][j]!=DUMMY && m_pData[i][j]!=data[i][j] ) {
						m_pData[i][j] = DUMMY;
						n++;
					}
					break;
				case DMX_LESS:
					if(data[i][j]!=DUMMY && m_pData[i][j]>=data[i][j] ) {
						m_pData[i][j] = DUMMY;
						n++;
					}
					break;
				case DMX_GRATER:
					if(data[i][j]!=DUMMY && m_pData[i][j]<=data[i][j] ) {
						m_pData[i][j] = DUMMY;
						n++;
					}
					break;
				case DMX_LESSEQ:
					if(data[i][j]!=DUMMY && m_pData[i][j]>data[i][j] ) {
						m_pData[i][j] = DUMMY;
						n++;
					}
					break;
				case DMX_GRATEREQ:
					if(data[i][j]!=DUMMY && m_pData[i][j]<data[i][j] ) {
						m_pData[i][j] = DUMMY;
						n++;
					}
					break;

			}
		}
	}

	return n;
}

// returns fake derivative in grid point
double CGrid::GetDrv(int row, int col)
{
	double	drv=0.0;
	int		i, j;

	if( !IsEmpty() && row>=0 && row<(int)m_nRow && col>=0 && col<(int)m_nCol ) {
		i = row;
		j = col;
		if( (i-1)>=0 )
			drv = fabs(m_pData[i][j] - m_pData[i-1][j]) / m_ySize ;
		if( (i+1)<(int)m_nRow )
			drv += fabs(m_pData[i][j]- m_pData[i+1][j]) / m_ySize ;
		if( (j+1)<(int)m_nCol )
			drv += fabs(m_pData[i][j]- m_pData[i][j+1]) / m_xSize ;
		if( (j-1)>=0 )
			drv += fabs(m_pData[i][j]- m_pData[i][j-1]) / m_xSize ;

		return drv;
	}

	return 0.0;
}

#undef PI
#undef PIR


int CGrid::Blank(void)
{
	CFileDialog dlg(TRUE, "bln", "*.bln", OFN_HIDEREADONLY, "Blanking text file (*.bln)|*.bln||");
	//dlg.SetWindowText("Open Blanking File");
	if(dlg.DoModal() != IDOK)  return 0;

	// read poygon from file
	CString		strFilePath = dlg.GetPathName();
	CStdioFile	file;
	CString		strLine;
	int			nItems, len, tag;
	float		d[5];
	DoubleArray	plg;
	BOOL		bData = FALSE;

	file.Open((LPCSTR) strFilePath, CFile::modeRead );	
	while( file.ReadString(strLine) ) {
		if( !strLine.IsEmpty() && bData ) {
			strLine.Replace(",", " ");
			nItems = sscanf( (LPCTSTR) strLine, "%f %f %f %f %f", d, d+1, d+2, d+3, d+4);
			if( nItems >1 ) {
				plg.Add(*d);
				plg.Add(*(d+1));
			}
			else 
				break;
		}
		if( !strLine.IsEmpty() && !bData ) {
			strLine.Replace(",", " ");
			nItems = sscanf( (LPCTSTR) strLine, "%d %d", &len, &tag);
			bData = TRUE;
		}
	}
	file.Close();

	if(plg.IsEmpty())	return 0;
	if( plg.GetAt(0) != plg.GetAt(plg.GetUpperBound()-1) ) {
		plg.Add( plg.GetAt(0) );
		plg.Add( plg.GetAt(1) );
	}

	int n = Blank(plg.GetData(), plg.GetSize()/2, 2, tag);

	CString strRes;
	strRes.Format("%d nodes blanked", n);
	AfxMessageBox(strRes, MB_OK | MB_ICONINFORMATION);

	return 0;
}

// blanks grid
// pPlg: pointer to polygon array
// nLen: number of polygon vertices within array
// nTorfat: number of entries per vertex
// nBlank: blanking tag, either inside or outside
BOOL CGrid::Blank(double* pPlg, int nLen, int nFormat, int nBlank)
{
	double	x, y;
	int		n=0;
	BOOL	b;

	for(DWORD i = 0; i < m_nRow; i++) {
		for(DWORD j = 0; j < m_nCol; j++) {
			GetXY(i, j, x, y);
			b = IsPointInPolygon(pPlg, nFormat, nLen, x, y);
			if( !b ) {
				m_pData[i][j] = DUMMY;
				n++;
			}
			//if( (!b && nBlank) || ((!b) && (!nBlank)) ) {
			//	m_pData[i][j] = DUMMY;
			//	n++;
			//}
		}
	}

	return n;
}


int CGrid::Properties(void)
{
	CDlgGridProperties dlg;

	dlg.m_dCsX = m_xSize;
	dlg.m_dCsY = m_ySize;
	dlg.m_dMaxX = m_dMaxX;
	dlg.m_dMinX = m_dMinX;
	dlg.m_dMaxY = m_dMaxY;
	dlg.m_dMinY = m_dMinY;
	dlg.m_dMaxZ = m_dMaxZ;
	dlg.m_dMinZ = m_dMinZ;

	ComputeHistogram();
	dlg.m_bHst = m_bHistClr;
	dlg.m_pGrad = &m_clrGrad;
	dlg.m_pGrid = this;
	dlg.m_strFilePath = GetFilePath();

	dlg.m_strName = m_strID;
	dlg.m_bCustomRange = m_bCustomRange;
	dlg.m_dCustomMin = m_dMinHstCst;
	dlg.m_dCustomMax = m_dMaxHstCst;

	if( dlg.DoModal() == IDOK ) {
		m_bHistClr = dlg.m_bHst;
		m_bCustomRange = dlg.m_bCustomRange;
		m_dMinHstCst = dlg.m_dCustomMin;
		m_dMaxHstCst = dlg.m_dCustomMax;

		SetModifiedFlag();
	}

	return 0;
}
