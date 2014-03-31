#include "stdafx.h"
#include "Mod3D.h"
#include "Mod3DDoc.h"
#include <math.h>
#include "afxmt.h"
#include "DlgFit1D.h"


void CMod3DDoc::OnComputeFit1d()
{
	FitVertexProperties();
}

void CMod3DDoc::FitVertexProperties()
{
	CDlgFit1D dlg;

	GetFldStringArray(dlg.m_strAr, GRDDIF_GX, GRDMES_GX);
	dlg.m_dEpsBrak = m_dFitVrtxEps;
	dlg.m_dFitTol = m_dFitVrtxTol;
	dlg.m_strFld = GetFieldString(m_nFitFld);
	dlg.m_bFitLog = m_bFitVrtxLog;
	dlg.m_nFitType = m_nFitVrtxChar;
	dlg.m_nMethod = m_nFitVrtxMeth;
	dlg.m_nMaxIter = m_nFitVrtxMaxIter;
	dlg.m_bAutoBracket = m_bFitVrtxEpsAuto;
	dlg.m_dAutoBraket = m_dFitVrtxEpsAuto;
	dlg.m_strLogFile = m_strFitVrtxLog;
	
	if( dlg.DoModal() == IDOK ) {
		m_dFitVrtxEps = dlg.m_dEpsBrak;
		m_dFitVrtxTol = dlg.m_dFitTol;
		GetGrid(dlg.m_strFld, m_nFitFld);
		m_bFitVrtxLog = dlg.m_bFitLog;
		m_nFitVrtxChar = dlg.m_nFitType;
		m_nFitVrtxMeth = dlg.m_nMethod;
		m_nFitVrtxMaxIter = dlg.m_nMaxIter;
		m_bFitVrtxEpsAuto = dlg.m_bAutoBracket;
		m_dFitVrtxEpsAuto = dlg.m_dAutoBraket;
		m_strFitVrtxLog = dlg.m_strLogFile;
	}
}

int CMod3DDoc::FitVertex(int nIndex, int nRow, int nCol)
{
	if( m_nFitFld == -1 ) {
		AfxMessageBox("Select fitting field.\nUse command 'Compute/1D Fit Properties'", MB_OK | MB_ICONINFORMATION);
		return -1;
	}
    
	if( !m_grdActiveCompute[ m_nFitFld ] || !m_grdActiveCompute[ m_nFitFld-20 ] ) {
		AfxMessageBox("Requested modeled or difference field is not activated.\nUse command 'Compute/Active Field'", MB_OK | MB_ICONINFORMATION);
		return -1;
	}

	// make sure that field is up to date
	if( GetComputationType() != CMP_REALTIME) {
		AfxMessageBox("Reltime computation mode needed. Fitting canceled.", MB_OK | MB_ICONINFORMATION);
		return -1;
	}

	// make sure no other computation thread is running
	if( m_pCompThread != NULL) {
		int ret = AfxMessageBox("The computation is currently running. Fitting canceled.", MB_OK | MB_ICONINFORMATION);
		return -1;
	}

	CString	str;
	double	a, b, c, fa, fb, fc;
	double	min, res, z, eps;

	// find fitting constrains
	m_model.GetConstrainZ(nRow, nCol, nIndex, m_dFitVrtxZU, m_dFitVrtxZD);
	z = m_model.GetZ(nRow, nCol, nIndex);
	if( m_bFitVrtxEpsAuto ) {
		eps = m_dFitVrtxEpsAuto*min(m_dFitVrtxZU-z,z-m_dFitVrtxZD);
		a = z;
		b = z+eps;
	}
	else {
		a = z; 
		b = a - m_dFitVrtxEps;
	}

	// indicate that this will take a while
	CWaitCursor wait;

	if( m_bFitVrtxLog ) {
		m_strFitLog.Empty();
		m_strFitLog += "Vertex inversion\n\nBracketing minimum ...\ndepth DRV RMS\n";
	}
	
	// measure duration
	clock_t start, finish;
	double	dDuration;
	start = clock();

	// bracket the minima
	MNbrak(&a, &b, &c, &fa, &fb, &fc, nIndex, nRow, nCol);
	if(fabs(fa)<fabs(fb) && fabs(fb)>fabs(fc) ) {
		// go back
		m_model.MoveVertex(nIndex, nRow, nCol, z, BMT_CONSTRAINED);
		UpdateField(FALSE);
		UpdateAllViews( NULL, UPDATE_COMPUTATION_FINISHED );
		wait.Restore();
		AfxMessageBox("The minimama cannot be bracketed. Canceling.", MB_OK | MB_ICONINFORMATION);
		return -1;
	}

	if( m_bFitVrtxLog ) {
		finish = clock();
		dDuration = (double)(finish - start) / CLOCKS_PER_SEC;
		start  = finish;
		str.Format("Bracketing elapsed time: %gs\n\nFitting ...\nFitting characteristic: %d\n", dDuration, m_nFitVrtxChar);
		m_strFitLog += str;
	}

	// now minimize
	switch(m_nFitVrtxMeth) {
		case FIT_GOLDEN:
			if( m_bFitVrtxLog ) m_strFitLog += "Golden section method\nz drv rms drvPt\n";
			res = Golden(a, b, c, m_dFitVrtxTol, &min, nIndex, nRow, nCol);
			break;
		case FIT_BRENT:
			if( m_bFitVrtxLog ) m_strFitLog += "Brent method\nz drv rms drvPt\n";
			res = Brent(a, b, c, m_dFitVrtxTol, &min, nIndex, nRow, nCol);
			break;
	}

	// time measure
	finish = clock();
	dDuration = (double)(finish - start) / CLOCKS_PER_SEC;

	MessageBeep( -1 );
	UpdateAllViews( NULL, UPDATE_COMPUTATION_FINISHED );

	// write fit log
	if( m_bFitVrtxLog ) {
		CStdioFile file;
		str.Format("Fitting elapsed time: %gs\n", dDuration);
		m_strFitLog += str;
		file.Open( "FitLogVrtx.dat", CFile::modeCreate | CFile::modeWrite | CFile::typeText );
		file.WriteString(m_strFitLog);
		file.Close();
		m_strFitLog.Empty();
	}

	wait.Restore();

	return 1;
}

double CMod3DDoc::FuncFit1D(double z, int nIndex, int nRow, int nCol) 
{
	m_model.MoveVertex(nIndex, nRow, nCol, z, BMT_CONSTRAINED);
	UpdateField(FALSE);

	CGrid* pGrd = GetGrid(m_nFitFld);
	ASSERT(pGrd != NULL);

	double d;
	switch( m_nFitVrtxChar ) {
		case FIT_DRV:
			d = pGrd->GetDrv(); 
			break;
		case FIT_RMS:
			d = pGrd->GetRMS(); 
			break;
		case FIT_DRV_LOCAL:
			d = pGrd->GetDrv(nRow, nCol);
			break;
		//case FIT_DRV_SECOND:
		//	d = pGrd->GetDrv(nRow, nCol);
		//	break;
		default:
			d = pGrd->GetRMS(); 
	}
	
	if( m_bFitVrtxLog ) {
		CString str;
		str.Format("%.6g %.10g %.10g %.10g \n", z, pGrd->GetDrv(), pGrd->GetDrv(nRow, nCol), pGrd->GetRMS());
		m_strFitLog += str;
	}

	return d;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////




////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//******* 1D minimization **********************************************************************************************************

// Brent parabolic minimization
#define CGOLD 0.3819660
#define ZEPS 1.0e-15
#define SHFT(a,b,c,d) (a)=(b);(b)=(c);(c)=(d);
#define SIGN(a,b) ((b) >= 0.0 ? fabs(a) : -fabs(a))

double CMod3DDoc::Brent(double ax, double bx, double cx, double tol, double *xmin, int nIndex, int nRow, int nCol)
{
	int		iter;
	double	a, b, d, etemp, fu, fv, fw, fx, p, q, r, tol1, tol2, u, v, w, x, xm;
	double	e = 0.0;

	a = (ax < cx ? ax : cx);
	b = (ax > cx ? ax : cx);
	x = w = v = bx;
	fw = fv = fx = FuncFit1D(x, nIndex, nRow, nCol);
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
		fu = FuncFit1D(u, nIndex, nRow, nCol);
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
double CMod3DDoc::Golden(double a, double b, double c, double tol,	double *xmin, int nIndex, int nRow, int nCol)
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

	f1 = FuncFit1D(x1, nIndex, nRow, nCol);
	f2 = FuncFit1D(x2, nIndex, nRow, nCol);
	while(fabs(x3-x0) > tol*(fabs(x1)+fabs(x2))) {
		if (f2 < f1) {
			SHFT3(x0, x1, x2, R*x1+C*x3)
			SHFT2(f1, f2, FuncFit1D(x2, nIndex, nRow, nCol))
		} 
		else {
			SHFT3(x3, x2, x1, R*x2+C*x0)
			SHFT2(f2, f1, FuncFit1D(x1, nIndex, nRow, nCol))
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
void CMod3DDoc::MNbrak(double *a, double *b, double *c, double *fa, double *fb, double *fc, int nIndex, int nRow, int nCol)
{
	double ulim, u, r, q, fu, dum;

	*fa = FuncFit1D(*a, nIndex, nRow, nCol);
	*fb = FuncFit1D(*b, nIndex, nRow, nCol);
	if (*fb > *fa) {
		SHFT(dum, *a, *b, dum)
		SHFT(dum, *fb, *fa, dum)
	}
	*c = (*b) + GOLD*(*b-*a);
	*fc = FuncFit1D(*c, nIndex, nRow, nCol);
	while(*fb > *fc) {
		r = (*b-*a)*(*fb-*fc);
		q = (*b-*c)*(*fb-*fa);
		u = (*b)-((*b-*c)*q-(*b-*a)*r)/(2.0*SIGN(__max(fabs(q-r), TINY), q-r));
		ulim = (*b)+GLIMIT*(*c-*b);
		if( (*b-u)*(u-*c) > 0.0 ) {
			fu = FuncFit1D(u, nIndex, nRow, nCol);
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
			fu = FuncFit1D(u, nIndex, nRow, nCol);
		} 
		else if ((*c-u)*(u-ulim) > 0.0) {
			fu = FuncFit1D(u, nIndex, nRow, nCol);
			if(fu < *fc) {
				SHFT(*b, *c, u, *c+GOLD*(*c-*b))
				SHFT(*fb, *fc, fu, FuncFit1D(u, nIndex, nRow, nCol))
			}
		} 
		else if((u-ulim)*(ulim-*c) >= 0.0) {
			u = ulim;
			fu = FuncFit1D(u, nIndex, nRow, nCol);
		} 
		else {
			u = (*c)+GOLD*(*c-*b);
			fu = FuncFit1D(u, nIndex, nRow, nCol);
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




