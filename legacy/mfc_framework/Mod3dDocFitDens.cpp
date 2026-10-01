#include "stdafx.h"
#include "Mod3D.h"
#include "Mod3DDoc.h"
#include <math.h>
#include "afxmt.h"
#include "DlgFit1D.h"

void CMod3DDoc::OnComputeDensityInversionProperties()
{
	DensityInversionProperties();
}

void CMod3DDoc::DensityInversionProperties(void)
{
	CDlgFit1D dlg;

	GetFldStringArray(dlg.m_strAr, GRDDIF_GX, GRDMES_GX);
	dlg.m_dEpsBrak = m_dFitDensEps;
	dlg.m_dFitTol = m_dFitDensTol;
	dlg.m_strFld = GetFieldString(m_nFitFld);
	dlg.m_bFitLog = m_bFitDensLog;
	dlg.m_nFitType = m_nFitDensChar;
	dlg.m_nMethod = m_nFitDensMeth;
	dlg.m_nMaxIter = m_nFitDensMaxIter;
	dlg.m_strLogFile = m_strFitDensLog;
	
	if( dlg.DoModal() == IDOK ) {
		m_dFitDensEps = dlg.m_dEpsBrak;
		m_dFitDensTol = dlg.m_dFitTol;
		m_bFitDensLog = dlg.m_bFitLog;
		m_nFitDensChar = dlg.m_nFitType;
		m_nFitDensMeth = dlg.m_nMethod;
		m_nFitDensMaxIter = dlg.m_nMaxIter;
		GetFieldString(m_nFitFld) = dlg.m_strFld;
		GetGrid(dlg.m_strFld, m_nFitFld);
		m_strFitDensLog = dlg.m_strLogFile;
	}
}

int CMod3DDoc::InvertDensity(int nBodyID)
{
	if( m_nFitFld == -1 ) {
		AfxMessageBox("Select fitting field.\nUse command 'Compute/1D Fit Properties'", MB_OK | MB_ICONINFORMATION);
		return -1;
	}
    
	if( m_nFitFld<GRDDIF_GX || m_nFitFld>GRDDIF_GZ ) {
		AfxMessageBox("Select gravity field component! Canceling.", MB_OK | MB_ICONINFORMATION);
		return -1;
	}
    
	if( !m_grdActiveCompute[ m_nFitFld ] || !m_grdActiveCompute[ m_nFitFld-20 ] ) {
		AfxMessageBox("Requested modeled or difference field is not activated.\nUse command 'Compute/Active Field'", MB_OK | MB_ICONINFORMATION);
		return -1;
	}

	// make sure that field is up to date
	if( GetComputationType() != CMP_REALTIME) {
		AfxMessageBox("Reltime computation mode needed! Fitting canceled.", MB_OK | MB_ICONINFORMATION);
		return -1;
	}

	// make sure no other computation thread is running
	if( m_pCompThread != NULL) {
		AfxMessageBox("The computation is currently running. Fitting canceled.", MB_OK | MB_ICONINFORMATION);
		return -1;
	}


	CBody*		pBody;		// body for density fitting
	pBody = m_model.GetBody(nBodyID);
	ASSERT(pBody!=NULL);
	if( !pBody->GetDensityGradient().IsZero() ) {
		AfxMessageBox("The density gradient is non-zero. Cannot Fit.", MB_OK | MB_ICONINFORMATION);
		return -1;
	}

	CWaitCursor wait;

	CGrid		grdFldFit;	// field for unit density
	CGrid		grdFldAll;	// field for all bodies
	FacetList	fctLst;		// list of facets for given body

	m_model.GetFacetsComputation(fctLst);
	if(fctLst.IsEmpty())	return -1;

	POSITION	pos, posRem;
	CFacet3Pt	*pFct;

	pos = fctLst.GetHeadPosition();
	while(pos) {
		posRem = pos;
		pFct = &fctLst.GetNext(pos);
		if(pFct->pBody!=pBody && pFct->pBodyOpos!=pBody) {
			fctLst.RemoveAt(posRem);
		}
		else if(pFct->pBodyOpos==pBody) {
			pFct->Reverse();
			pFct->pBodyOpos = NULL;
		}
		else {
			pFct->pBodyOpos = NULL;
		}
	}

	// initialization
	for(int i = 0; i < GRDMOD_MXX; i++) {
		if(m_grdInv[i].IsEmpty() ) {
			m_grdInv[i].New(m_obsHdr.nRows, m_obsHdr.nCols, m_obsHdr.x0, m_obsHdr.y0, m_obsHdr.xSize, m_obsHdr.ySize);
		}
		m_grdInv[i].ZeroData();
	}
	if(m_fitGrdFld.IsEmpty() ) 
		m_fitGrdFld.New(m_obsHdr.nRows, m_obsHdr.nCols, m_obsHdr.x0, m_obsHdr.y0, m_obsHdr.xSize, m_obsHdr.ySize);
	if(m_fitGrdDens.IsEmpty() ) 
		m_fitGrdDens.New(m_obsHdr.nRows, m_obsHdr.nCols, m_obsHdr.x0, m_obsHdr.y0, m_obsHdr.xSize, m_obsHdr.ySize);
	m_fitGrdFld.ZeroData();
	m_fitGrdDens.ZeroData();

	ComputeFieldInv(&fctLst);
	CGrid* pGrd;
	double dens;

	dens = pBody->GetDensity()-m_dGrvDensRef;
	//pGrd = &m_grdInv[m_nFitFld-20];
	pGrd = &m_grdInv[m_nFitFld-20];
	// set field without currently inverted body
	GetGrid(m_nFitFld-20)->CopyData(&m_fitGrdFld);
	m_fitGrdFld -= pGrd;
	*pGrd /= dens;
	
	m_fitGrdFld.UpdateMinMaxValue();
	if(m_fitGrdFld.GetRMS() == 0) {
		AfxMessageBox("RMS==0.0", MB_OK | MB_ICONINFORMATION);
		return -1;
	}
	//++++ fitting itself +++++++++++++++++++++
	CString	str;
	double	a, b, c, fa, fb, fc;
	double	min, res;
	if( m_bFitVrtxLog ) {
		m_strFitLog.Empty();
		m_strFitLog += "Density inversion\n\nBracketing minimum ...\ndifDens dens DRV RMS\n";
	}
	
	// measure duration
	clock_t start, finish;
	double	dDuration;
	start = clock();

	a = dens;
	b = a + m_dFitDensEps;

	MNbrak(&a, &b, &c, &fa, &fb, &fc);
	if(fabs(fa)<fabs(fb) && fabs(fb)>fabs(fc) ) {
		wait.Restore();
		AfxMessageBox("The minimum cannot be bracketed. Canceling.", MB_OK | MB_ICONINFORMATION);
		return -1;
	}

	if( m_bFitVrtxLog ) {
		finish = clock();
		dDuration = (double)(finish - start) / CLOCKS_PER_SEC;
		start  = finish;
		str.Format("Bracketing elapsed time: %gs\n\nFitting ...\nFitting characteristic: %d\n", dDuration, m_nFitDensChar);
		m_strFitLog += str;
	}

	// now minimize
	switch(m_nFitDensMeth) {
		case FIT_GOLDEN:
			if( m_bFitVrtxLog ) m_strFitLog += "Golden section method\ndifDens dens DRV RMS\n";
			res = Golden(a, b, c, m_dFitVrtxTol, &min);
			break;
		case FIT_BRENT:
			if( m_bFitVrtxLog ) m_strFitLog += "Brent method\ndifDens dens DRV RMS\n";
			res = Brent(a, b, c, m_dFitVrtxTol, &min);
			break;
	}

	// time measure
	finish = clock();
	dDuration = (double)(finish - start) / CLOCKS_PER_SEC;

	MessageBeep( -1 );

	// write fit log
	if( m_bFitVrtxLog ) {
		CStdioFile file;
		str.Format("Fitting elapsed time: %gs\n", dDuration);
		m_strFitLog += str;
		file.Open( "FitLogDens.dat", CFile::modeCreate | CFile::modeWrite | CFile::typeText );
		file.WriteString(m_strFitLog);
		file.Close();
		m_strFitLog.Empty();
	}

	pBody->SetDensity( m_dGrvDensRef + min);
	OnComputeCompute();

	return 0;
}

double CMod3DDoc::FuncFitDens1D(double x)
{
	m_grdInv[m_nFitFld-20].CopyData( &m_fitGrdDens );
	m_fitGrdDens *= x;
	m_fitGrdDens += &m_fitGrdFld;
	m_fitGrdDens -= GetGrid(m_nFitFld+20);
	m_fitGrdDens.SetModifiedFlag();
	m_fitGrdDens.UpdateMinMaxValue();

	double d;
	switch( m_nFitDensChar ) {
		case FIT_DRV:
			d = m_fitGrdDens.GetDrv(); 
			break;
		case FIT_RMS:
			d = m_fitGrdDens.GetRMS(); 
			break;
		//case FIT_DRV_SECOND:
		//	d = ;
		//	break;
		default:
			d = m_fitGrdDens.GetRMS(); 
	}
	
	if( m_bFitVrtxLog ) {
		CString str;
		str.Format("%.6g %.10g %.10g %.10g \n", x, x+m_dGrvDensRef, m_fitGrdDens.GetDrv(), m_fitGrdDens.GetRMS());
		m_strFitLog += str;
	}

	return d;

}

// computes field of one body for pFctLst
// fills pGrdFld with field
// nFld specifies field to compute
int CMod3DDoc::ComputeFieldInv(FacetList* pFctLst)
{
	TRACE("Entering UpdateField() \n");

	if( m_nComputationType != CMP_REALTIME ) return 0;

	if( m_pCompThread != NULL ) {
		AfxMessageBox("Computation is running. Real-time computing is disabled.", MB_OK | MB_ICONINFORMATION);
		return 0;
	}

	if( pFctLst->IsEmpty() ) return 0;

	// show wait cursor
	CWaitCursor wait;
	//ComputeField( pFctLst );

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
	BOOL		bZeroVal = TRUE;

	// grids header !!! the same for all grids
	grdInf = GetObservationGrid()->GetInfo();
	gradInf = GetGradStruct();
	nCols = grdInf.nCol;
	nRows = grdInf.nRow;
	x0 = grdInf.x0;
	y0 = grdInf.y0;
	xSize = grdInf.xSize;
	ySize = grdInf.ySize;

	// grids header !!! the same for all grids
	grdInf	= GetObservationGrid()->GetInfo();
	gradInf = GetGradStruct();
	x0		= grdInf.x0;
	y0		= grdInf.y0;
	xSize	= grdInf.xSize;
	ySize	= grdInf.ySize;

	// model grids data
	gx	= GetGridDataInv( GRDMOD_GX );
	gy	= GetGridDataInv( GRDMOD_GY );
	gz	= GetGridDataInv( GRDMOD_GZ );
	mx	= GetGridDataInv( GRDMOD_MX );
	my	= GetGridDataInv( GRDMOD_MY );
	mz	= GetGridDataInv( GRDMOD_MZ );
	gxx	= GetGridDataInv( GRDMOD_GXX );
	gyy	= GetGridDataInv( GRDMOD_GYY );
	gzz	= GetGridDataInv( GRDMOD_GZZ );
	gxy	= GetGridDataInv( GRDMOD_GXY );
	gxz	= GetGridDataInv( GRDMOD_GXZ );
	gyz	= GetGridDataInv( GRDMOD_GYZ );

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
				else {vgx=NULL;}

				if(gy!=NULL)	{vgy = &gy[i][j];	if(bZeroVal) *vgy = 0.0;	bCompute |= TRUE;}
				else {vgy=NULL;}

				if(gz!=NULL)	{vgz = &gz[i][j];	if(bZeroVal) *vgz = 0.0;	bCompute |= TRUE;}
				else {vgy=NULL;}
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

		} //for(j)
	} //for(i)
	//----------------------------------------------------------------------------------------------------------------

	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	ComputeTotalField();
	ComputeDifferenceField();
	UpdateMinMaxFldValModified();
	SetModifiedFlag();

	cs.Unlock();
	wait.Restore();
	return 1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////




////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//******* 1D minimization **********************************************************************************************************

// Brent parabolic minimization
#define CGOLD 0.3819660
#define ZEPS 1.0e-15
#define SHFT(a,b,c,d) (a)=(b);(b)=(c);(c)=(d);
#define SIGN(a,b) ((b) >= 0.0 ? fabs(a) : -fabs(a))

double CMod3DDoc::Brent(double ax, double bx, double cx, double tol, double *xmin)
{
	int		iter;
	double	a, b, d, etemp, fu, fv, fw, fx, p, q, r, tol1, tol2, u, v, w, x, xm;
	double	e = 0.0;

	a = (ax < cx ? ax : cx);
	b = (ax > cx ? ax : cx);
	x = w = v = bx;
	fw = fv = fx = FuncFitDens1D(x);
	for (iter=1; iter<=m_nFitVrtxMaxIter; iter++) {
		xm = 0.5*(a+b);
		tol2 = 2.0*(tol1=tol*fabs(x)+ZEPS);
		if(fabs(x-xm) <= (tol2-0.5*(b-a))) {
			*xmin=x;
			return fx;
		}
		if(fabs(e) > tol1) {
			r = (x-w)*(fx-fv);
			q = (x-v)*(fx-fw);
			p = (x-v)*q-(x-w)*r;
			q = 2.0*(q-r);
			if(q > 0.0) p = -p;
			q = fabs(q);
			etemp = e;
			e = d;
			if(fabs(p) >= fabs(0.5*q*etemp) || p <= q*(a-x) || p >= q*(b-x))
				d = CGOLD*(e=(x >= xm ? a-x : b-x));
			else {
				d=p/q;
				u=x+d;
				if (u-a < tol2 || b-u < tol2)
					d = SIGN(tol1, xm-x);
			}
		} else {
			d = CGOLD*(e=(x >= xm ? a-x : b-x));
		}
		u = (fabs(d) >= tol1 ? x+d : x+SIGN(tol1, d));
		fu = FuncFitDens1D(u);
		if (fu <= fx) {
			if (u >= x) a=x; else b=x;
			SHFT(v, w, x, u)
			SHFT(fv, fw, fx, fu)
		} else {
			if (u < x) a=u; else b=u;
			if (fu <= fw || w == x) {
				v = w;
				w = u;
				fv = fw;
				fw = fu;
			} 
			else if (fu <= fv || v == x || v == w) {
				v = u;
				fv = fu;
			}
		}
	}

	return 0.0;
}

#undef CGOLD
#undef ZEPS
#undef SHFT
#undef SIGN



// golden section minimization
#define R 0.61803399
#define C (1.0-R)
#define SHFT2(a, b, c) (a)=(b);(b)=(c);
#define SHFT3(a, b, c, d) (a)=(b);(b)=(c);(c)=(d);
// searches for the minimum in the interval <ax, cx>
// tol tollerance
double CMod3DDoc::Golden(double a, double b, double c, double tol,	double *xmin)
{
	CGrid* pGrd = GetGrid(m_nFitFld);
	double f1, f2, x0, x1, x2, x3;

	x0 = a;
	x3 = c;

	if (fabs(c-b) > fabs(b-a)) {
		x1 = b;
		x2 = b + C*(c-b);
	} else {
		x2 = b;
		x1 = b-C*(b-a);
	}

	f1 = FuncFitDens1D(x1);
	f2 = FuncFitDens1D(x2);
	while(fabs(x3-x0) > tol*(fabs(x1)+fabs(x2))) {
		if (f2 < f1) {
			SHFT3(x0, x1, x2, R*x1+C*x3)
			SHFT2(f1, f2, FuncFitDens1D(x2))
		} 
		else {
			SHFT3(x3, x2, x1, R*x2+C*x0)
			SHFT2(f2, f1, FuncFitDens1D(x1))
		}
	}

	if (f1 < f2) {
		*xmin = x1;
		return f1;
	} 
	else {
		*xmin = x2;
		return f2;
	}
}

#undef C
#undef R
#undef SHFT2
#undef SHFT3



#define GOLD 1.618034
#define GLIMIT 10.0
#define TINY 1.0e-15
#define SHFT(a,b,c,d) (a)=(b);(b)=(c);(c)=(d);
#define SIGN(a,b) ((b) >= 0.0 ? fabs(a) : -fabs(a))

// brackets the minimum of a function
// fa, fb, fc function value in ax, bx, cx returned at output
void CMod3DDoc::MNbrak(double *a, double *b, double *c, double *fa, double *fb, double *fc)
{
	double ulim, u, r, q, fu, dum;

	*fa = FuncFitDens1D(*a);
	*fb = FuncFitDens1D(*b);
	if (*fb > *fa) {
		SHFT(dum, *a, *b, dum)
		SHFT(dum, *fb, *fa, dum)
	}
	*c = (*b) + GOLD*(*b-*a);
	*fc = FuncFitDens1D(*c);
	while(*fb > *fc) {
		r = (*b-*a)*(*fb-*fc);
		q = (*b-*c)*(*fb-*fa);
		u = (*b)-((*b-*c)*q-(*b-*a)*r)/(2.0*SIGN(__max(fabs(q-r), TINY), q-r));
		ulim = (*b)+GLIMIT*(*c-*b);
		if( (*b-u)*(u-*c) > 0.0 ) {
			fu = FuncFitDens1D(u);
			if(fu < *fc) {
				*a = (*b);
				*b = u;
				*fa = (*fb);
				*fb = fu;
				return;
			} 
			else if (fu > *fb) {
				*c = u;
				*fc = fu;
				return;
			}
			u = (*c)+GOLD*(*c-*b);
			fu = FuncFitDens1D(u);
		} 
		else if ((*c-u)*(u-ulim) > 0.0) {
			fu = FuncFitDens1D(u);
			if(fu < *fc) {
				SHFT(*b, *c, u, *c+GOLD*(*c-*b))
				SHFT(*fb, *fc, fu, FuncFitDens1D(u))
			}
		} 
		else if((u-ulim)*(ulim-*c) >= 0.0) {
			u = ulim;
			fu = FuncFitDens1D(u);
		} 
		else {
			u = (*c)+GOLD*(*c-*b);
			fu = FuncFitDens1D(u);
		}

		SHFT(*a, *b, *c, u)
		SHFT(*fa, *fb, *fc, fu)
	}
}

#undef GOLD
#undef GLIMIT
#undef TINY
#undef SHFT
#undef SIGN





