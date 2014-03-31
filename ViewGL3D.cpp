// Mod3DView_3D.cpp : implementation file
//

#include <math.h>
#include "stdafx.h"
#include "ViewGL3D.h"
#include "Mod3D.h"
#include "Model.h"
#include "ViewProf.h"
#include "GridObj.h"
#include "Routines.h"
#include "well.h"

#include "View3dSheet.h"
#include "DlgPp3DAxes.h"
#include "DlgPp3dField.h"
#include "DlgPp3dModel.h"
#include "DlgPp3dSetings.h"
#include "DlgPp3dDraw.h"
#include "DlgPp3dSaveLoad.h"


extern double g_x, g_y, g_z;

// CViewGL3D

IMPLEMENT_DYNCREATE(CViewGL3D, CView)

CViewGL3D::CViewGL3D()
{
	m_pDC = NULL;
	m_pDoc = NULL;
	m_mod = NULL;
	m_pSheetProperties = NULL;

	m_dScX = 1.0;
	m_dScY = 1.0;
	m_dScZ = 1.0;

    m_fRotX = 0;
	m_fRotY = 180.0f;
	m_fRotZ = 0;

	m_dProjAngle = 45.0;
	m_dCameraX = 0;
	m_dCameraY = 0;
	m_dCameraZ = 0;

	m_mX = 0.01f;
	m_mY = 0.01f;
	m_mZ = 0.01f;

	m_rX = 2.0f;
	m_rY = 2.0f;
	m_rZ = 2.0f;

	m_bLightsEnabled = TRUE;

	m_nFld = -1;
	m_fAlphaFld = 0.8f;
	m_bFldOnRel = TRUE;

	m_bBodySolid = TRUE;
	m_bTransparentBodies = FALSE;
	m_fAlphaBody = 0.5f;

	m_bDrawBitmaps = TRUE;
	m_fAlphaBmp = 0.9f;

	m_bProfilePlanes = TRUE;
	m_bProfilePlanesSolid = TRUE;
	m_fAlphaProf = 0.2f;

	m_bOutFct = FALSE;
	m_bVrtFct = TRUE;
	m_bCullFacet = FALSE;
	m_nCullFacet = GL_BACK;

	// font
	memset(&m_lf, 0, sizeof(LOGFONT));
	m_lf.lfHeight=12;	
	m_lf.lfWidth=0;
	m_lf.lfEscapement=m_lf.lfOrientation=0;
	m_lf.lfItalic=0;
	m_lf.lfWeight=FW_NORMAL;
	m_lf.lfUnderline=FALSE;
	m_lf.lfStrikeOut=FALSE;
	m_lf.lfCharSet=ANSI_CHARSET;
	m_lf.lfOutPrecision=OUT_DEFAULT_PRECIS;
	m_lf.lfClipPrecision=CLIP_DEFAULT_PRECIS; 
	m_lf.lfQuality=DEFAULT_QUALITY;
	m_lf.lfPitchAndFamily=DEFAULT_PITCH |FF_DONTCARE;
	strcpy(m_lf.lfFaceName, "Arial");        

	m_charsetDListBase2D = 0;
	m_charsetDListBase3D = 0;
	m_nChars = 255;

	m_bSetupLights = TRUE;
	m_gmfvector = NULL;
	m_nTextType = GLTXT_2D;

	m_bGrdLineUpX = FALSE;
	m_bGrdLineUpZ = FALSE;
	m_bGrdLineDwnX = TRUE;
	m_bGrdLineDwnZ = TRUE;
	m_bGrdLineVrt = FALSE;

	m_bHorizon = TRUE;
	m_bHorizonSolid = TRUE;
	m_fHorizonAlpha = 0.8f;

	m_nMode = GL_RENDER;
	m_bSelecting = FALSE;
	m_fSelSensitivity = 1.0;

	m_bDarkenInside = TRUE;
	m_fDarkenInside = 0.6f;
	m_bMatrix = FALSE;

	m_bPlaneSimulator = FALSE;
	m_bUpdateBitmaps = TRUE;

	m_bBottom = FALSE;
	m_nProjection = PRJ_PERSPECTIVE;
	//m_nProjection = PRJ_ORTHO;
	m_dOrthoScale = 1.0;
}

BEGIN_MESSAGE_MAP(CViewGL3D, CView)
	ON_MESSAGE(WM_VIEW_3D_PROPERTIES, OnPropertiesChange)
	ON_MESSAGE(WM_VIEW_3D_SAVE_SETTINGS, OnSaveSettings)
	ON_MESSAGE(WM_VIEW_3D_LOAD_SETTINGS, OnLoadSettings)

	ON_WM_KEYDOWN()
	ON_WM_RBUTTONDOWN()
	//ON_COMMAND(ID_VIEW_REDRAW, OnViewRedraw)
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	ON_WM_LBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_WM_CREATE()
	ON_WM_MOUSEWHEEL()

	// field toolbar
	ON_COMMAND(ID_VIEW_FIELD_MODEL_G, OnViewFieldModelG)
	ON_UPDATE_COMMAND_UI(ID_VIEW_FIELD_MODEL_G, OnUpdateViewFieldModelG)
	ON_COMMAND(ID_VIEW_FIELD_MODEL_GX, OnViewFieldModelGx)
	ON_UPDATE_COMMAND_UI(ID_VIEW_FIELD_MODEL_GX, OnUpdateViewFieldModelGx)
	ON_COMMAND(ID_VIEW_FIELD_MODEL_GY, OnViewFieldModelGy)
	ON_UPDATE_COMMAND_UI(ID_VIEW_FIELD_MODEL_GY, OnUpdateViewFieldModelGy)
	ON_COMMAND(ID_VIEW_FIELD_MODEL_GZ, OnViewFieldModelGz)
	ON_UPDATE_COMMAND_UI(ID_VIEW_FIELD_MODEL_GZ, OnUpdateViewFieldModelGz)
	ON_COMMAND(ID_VIEW_FIELD_MODEL_M, OnViewFieldModelM)
	ON_UPDATE_COMMAND_UI(ID_VIEW_FIELD_MODEL_M, OnUpdateViewFieldModelM)
	ON_COMMAND(ID_VIEW_FIELD_MODEL_MX, OnViewFieldModelMx)
	ON_COMMAND(ID_VIEW_FIELD_MODEL_MY, OnViewFieldModelMy)
	ON_UPDATE_COMMAND_UI(ID_VIEW_FIELD_MODEL_MX, OnUpdateViewFieldModelMx)
	ON_UPDATE_COMMAND_UI(ID_VIEW_FIELD_MODEL_MY, OnUpdateViewFieldModelMy)
	ON_COMMAND(ID_VIEW_FIELD_MODEL_MZ, OnViewFieldModelMz)
	ON_UPDATE_COMMAND_UI(ID_VIEW_FIELD_MODEL_MZ, OnUpdateViewFieldModelMz)
	ON_COMMAND(ID_MODEL_TXX, OnModelTxx)
	ON_UPDATE_COMMAND_UI(ID_MODEL_TXX, OnUpdateModelTxx)
	ON_COMMAND(ID_MODEL_TXY, OnModelTxy)
	ON_UPDATE_COMMAND_UI(ID_MODEL_TXY, OnUpdateModelTxy)
	ON_COMMAND(ID_MODEL_TXZ, OnModelTxz)
	ON_UPDATE_COMMAND_UI(ID_MODEL_TXZ, OnUpdateModelTxz)
	ON_COMMAND(ID_MODEL_TYY, OnModelTyy)
	ON_UPDATE_COMMAND_UI(ID_MODEL_TYY, OnUpdateModelTyy)
	ON_COMMAND(ID_MODEL_TYZ, OnModelTyz)
	ON_UPDATE_COMMAND_UI(ID_MODEL_TYZ, OnUpdateModelTyz)
	ON_COMMAND(ID_MODEL_TZZ, OnModelTzz)
	ON_UPDATE_COMMAND_UI(ID_MODEL_TZZ, OnUpdateModelTzz)
	ON_COMMAND(ID_VIEW_MEASURED_FIELD, OnViewMeasuredField)
	ON_UPDATE_COMMAND_UI(ID_VIEW_MEASURED_FIELD, OnUpdateViewMeasuredField)
	ON_COMMAND(ID_VIEW_MODELED_FIELD, OnViewModeledField)
	ON_UPDATE_COMMAND_UI(ID_VIEW_MODELED_FIELD, OnUpdateViewModeledField)
	ON_COMMAND(ID_VIEW_DIFFERENCE_FIELD, OnViewDifferenceField)
	ON_UPDATE_COMMAND_UI(ID_VIEW_DIFFERENCE_FIELD, OnUpdateViewDifferenceField)

	ON_WM_LBUTTONUP()
	ON_WM_SETCURSOR()
	ON_COMMAND(ID_VIEW_PLANESIMULATOR, OnViewPlanesimulator)
	ON_UPDATE_COMMAND_UI(ID_VIEW_PLANESIMULATOR, OnUpdateViewPlanesimulator)
END_MESSAGE_MAP()


CViewGL3D::~CViewGL3D()
{
}

int CViewGL3D::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	TRACE(" Entering CViewGL3D::OnCreate(LPCREATESTRUCT lpCreateStruct\n)");
	if (CView::OnCreate(lpCreateStruct) == -1)
		return -1;

	// OpenGL rendering context creation
	PIXELFORMATDESCRIPTOR pfd;

	// initialize the private member
	m_pDC= new CClientDC(this);

	// choose the requested video mode
	 if (!bSetupPixelFormat()) return 0;
	
	// ask the system if the video mode is supported
	   int n=::GetPixelFormat(m_pDC->GetSafeHdc());
	   ::DescribePixelFormat(m_pDC->GetSafeHdc(), n, sizeof(pfd), &pfd);

	//----------------------------
	//// get the best available match of pixel format for the device context  
	//int iPixelFormat = ChoosePixelFormat(m_pDC->GetSafeHdc(), &pfd); 
	//	
	//// make that the pixel format of the device context 
	//SetPixelFormat(m_pDC->GetSafeHdc(), iPixelFormat, &pfd); 
	//----------------------------

	// link the Win Device Context with the OGL Rendering Context
    m_hRC = wglCreateContext( m_pDC->GetSafeHdc() );
	if( m_hRC == NULL ) {
		AfxMessageBox("Cannot create Open GL rendering context. Closing.", MB_OK | MB_ICONSTOP);
		CView::SendMessage( WM_CLOSE );
		return -1;
	}

	// specify the target DeviceContext (window) of the subsequent OGL calls
    wglMakeCurrent(m_pDC->GetSafeHdc(), m_hRC);

	// performs default setting of rendering mode,etc..
	OnCreateGL();

	// free the target DeviceContext (window)
    wglMakeCurrent(NULL, NULL);
	
	return 0;
}

void CViewGL3D::OnCreateGL()
{
	// perform hidden line/surface removal (enabling Z-Buffer)
	glEnable(GL_DEPTH_TEST);

	// set background color to white
	glClearColor(1.f, 1.f, 1.f, 1.0f );

	// set clear Z-Buffer value
	glClearDepth(1.0f);

	// Hints
	//glEnable(GL_POLYGON_SMOOTH);			// very slow
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	// fonts
	PrepareCharset2D(&m_lf);
	PrepareCharset3D(&m_lf, 10.0f, FALSE);

	// common display lists
	m_nDispLstCoord = glGenLists(1);
	//glNewList(m_nDispLstCoord, GL_COMPILE);
	//	DrawCoordSys();
	//glEndList();
}


void CViewGL3D::OnDestroy()
{
	// specify the target DeviceContext (window) of the subsequent OGL calls
    wglMakeCurrent(m_pDC->GetSafeHdc(), m_hRC);

	if(m_charsetDListBase2D>0) 
		glDeleteLists(m_charsetDListBase2D, m_nChars);	
	if(m_charsetDListBase3D>0) 
		glDeleteLists(m_charsetDListBase3D, m_nChars);	
	if(m_gmfvector!=NULL) delete[] m_gmfvector;

	// remove all display lists
	//for (int c=0;c<MAX_LISTS;c++) if(m_DispListVector[c]) glDeleteLists(m_DispListVector[c],1);

	// release definitely OGL Rendering Context
	if (m_hRC!=NULL) ::wglDeleteContext(m_hRC);


	// destroy Win Device Context
	if(m_pDC) delete m_pDC;

	// finally call the base function
	CView::OnDestroy();	
}

void CViewGL3D::OnSize(UINT nType, int cx, int cy)
{
	CView::OnSize(nType, cx, cy);
	// when called with a nonzero window:
	if ( 0 < cx && 0 < cy )
	{
		// specify the target DeviceContext of the OGL calls below
		wglMakeCurrent(m_pDC->GetSafeHdc(), m_hRC);

		// call the virtual sizing procedure (to be overridden by user)
		if (m_mod!=NULL)
			SetProjectionMatrix(cx,cy); // call instead OnSizeGL(cx,cy);

		// free the target DeviceContext (window)
		wglMakeCurrent(NULL,NULL);

		// force redraw
		Invalidate(FALSE);
	};
}

BOOL CViewGL3D::bSetupPixelFormat(void)
{
	static PIXELFORMATDESCRIPTOR pfd =
	{
		sizeof(PIXELFORMATDESCRIPTOR),  // size of this pfd
		1,                              // version number
		PFD_DRAW_TO_WINDOW |            // support window
		  PFD_SUPPORT_OPENGL |          // support OpenGL
		  PFD_DOUBLEBUFFER,             // double buffered
		PFD_TYPE_RGBA,                  // RGBA type
		24,                             // 24-bit color depth
		0, 0, 0, 0, 0, 0,               // color bits ignored
		0,                              // no alpha buffer
		0,                              // shift bit ignored
		0,                              // no accumulation buffer
		0, 0, 0, 0,                     // accum bits ignored
		32,                             // 32-bit z-buffer
		0,                              // no stencil buffer
		0,                              // no auxiliary buffer
		PFD_MAIN_PLANE,                 // main layer
		0,                              // reserved
		0, 0, 0                         // layer masks ignored
	};
	int pixelformat;

	if ( (pixelformat = ChoosePixelFormat(m_pDC->GetSafeHdc(), &pfd)) == 0 )
	{
		MessageBox("ChoosePixelFormat failed");
		return FALSE;
	}

	if (SetPixelFormat(m_pDC->GetSafeHdc(), pixelformat, &pfd) == FALSE)
	{
		MessageBox("SetPixelFormat failed");
		return FALSE;
	}

	return TRUE;
}


BOOL CViewGL3D::OnEraseBkgnd(CDC* pDC)
{
	//return CView::OnEraseBkgnd(pDC);
	return TRUE;
}

void CViewGL3D::OnDraw(CDC* pDC)
{
	if( m_mod == NULL ) return;

	// prepare a semaphore
	static BOOL 	bBusy = FALSE;
	// use the semaphore to enter this critic section
	if(bBusy) return;
	bBusy = TRUE;
	// specify the target DeviceContext of the subsequent OGL calls
	wglMakeCurrent(m_pDC->GetSafeHdc(), m_hRC);

	// update lights if needed
	SetupLights(m_bLightsEnabled);

	// clear background
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// call the virtual drawing procedure (to be overridden by user)
	DrawScene();

	// execute OGL commands (flush the OGL graphical pipeline)
	//	glFinish(); // useless cause swapbuffers issues an implicit glFinish

	// if double buffering is used it's time to swap the buffers
	SwapBuffers(m_pDC->GetSafeHdc());

	// check for errors
	m_glErrorCode=glGetError();
	if(m_glErrorCode != GL_NO_ERROR)
	{
		const GLubyte *estring;
		CString mexstr;
		// get the error descritption from OGL
		estring = gluErrorString(m_glErrorCode);
		// prepare and show a message box
		mexstr.Format("GLEnabledView:\n\tAn OpenGL error occurred: %s\n", estring);
		AfxMessageBox(mexstr,MB_OK | MB_ICONEXCLAMATION);
		// replicate mex to debug trace
		TRACE0(mexstr);
		// free the target DeviceContext (window)
		wglMakeCurrent(NULL,NULL);
		bBusy=FALSE;
	}
	else
	{
		// turn the semaphore "green"
		bBusy = FALSE;
	}

	// free the target DeviceContext (window)
    wglMakeCurrent(NULL,NULL);
}

// CViewGL3D diagnostics

#ifdef _DEBUG
void CViewGL3D::AssertValid() const
{
	CView::AssertValid();
}

void CViewGL3D::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}

CMod3DDoc* CViewGL3D::GetDocument() // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf( RUNTIME_CLASS(CMod3DDoc) ) );
	return (CMod3DDoc*)m_pDocument;
}
#endif //_DEBUG

void CViewGL3D::SetModelDimensions(void)
{
	// get document
	CMod3DDoc* pDoc = GetDocument();


	// model extent
	m_minX = (float) m_mod->GetX0();
	m_minY = (float) m_mod->GetZ0();
	m_maxZ = (float) -m_mod->GetY0();
	m_maxX = (float) (m_mod->GetX0() + m_mod->GetLX()); 
	m_maxY = (float) (m_mod->GetZ0() + m_mod->GetLZ()); 
	m_minZ = (float) -(m_mod->GetY0() + m_mod->GetLY()); 
	m_xSize = m_pDoc->GetModel()->GetSizeX();
	m_ySize = m_pDoc->GetModel()->GetSizeY();

	m_midX = (m_maxX+m_minX)/2;
	m_midY = (m_maxY+m_minY)/2;
	m_midZ = (m_maxZ+m_minZ)/2;

	m_lenX = m_maxX-m_minX;
	m_lenY = m_maxY-m_minY;
	m_lenZ = m_maxZ-m_minZ;

	m_dFldMinY = m_maxY;
	m_dFldMaxY = m_maxY + m_lenY*0.7;

	m_diag = (float) sqrt(m_lenX*m_lenX + m_lenY*m_lenY + m_lenZ*m_lenZ );

	float m = (float) m_diag/100;
	m_mX = m;
	m_mY = m;
	m_mZ = m;

	m_dCameraZ = 0.0;	//(m_diag/2.0)/tan(m_dProjAngle*3.141592/360.0);

	m_plX = m_midX;
	m_plY = m_maxY;
	m_plZ = m_midZ;

	m_pld = m_diag/500;

	m_plRotX = 0.0;
	m_plRotY = 0.0;
	m_plRotZ = 0.0;
	m_plRotV = 0.0;

	m_plRotdX = 1.0;
	m_plRotdY = 1.0;
	m_plRotdZ = 1.0;
	m_plRotdV = 1.0;

	m_fSelSensitivity = (float) min(m_xSize, m_ySize)/50;
}

void CViewGL3D::OnInitialUpdate()
{
	GetParent()->SetWindowText(GetDocument()->GetTitle()+ ": 3D View");

	CView::OnInitialUpdate();

	m_pDoc = GetDocument();
	m_mod = GetDocument()->GetModel();
	m_nRowStart = 0;
	m_nRowEnd = m_mod->GetRows()-1;
	m_nColStart = 0;
	m_nColEnd = m_mod->GetCols()-1;
	m_nRowMax = m_mod->GetRows()-1;
	m_nColMax = m_mod->GetCols()-1;
	int cols = m_nColMax+1;
	int rows = m_nRowMax+1;

	m_nFldTbl = FLDTLB_MODELED;
	
	SetModelDimensions();
	//// model extent
	//m_minX = (float) m_mod->GetX0();
	//m_minY = (float) m_mod->GetZ0();
	//m_maxZ = (float) -m_mod->GetY0();
	//m_maxX = (float) (m_mod->GetX0() + m_mod->GetLX()); 
	//m_maxY = (float) (m_mod->GetZ0() + m_mod->GetLZ()); 
	//m_minZ = (float) -(m_mod->GetY0() + m_mod->GetLY()); 

	//m_midX = (m_maxX+m_minX)/2;
	//m_midY = (m_maxY+m_minY)/2;
	//m_midZ = (m_maxZ+m_minZ)/2;

	//m_lenX = m_maxX-m_minX;
	//m_lenY = m_maxY-m_minY;
	//m_lenZ = m_maxZ-m_minZ;

	//m_dFldMinY = m_maxY;
	//m_dFldMaxY = m_maxY + m_lenY*0.7;

	//m_diag = (float) sqrt(m_lenX*m_lenX + m_lenY*m_lenY + m_lenZ*m_lenZ );

	//float m = (float) m_diag/100;
	//m_mX = m;
	//m_mY = m;
	//m_mZ = m;

	//m_dCameraZ = 0.0;	//(m_diag/2.0)/tan(m_dProjAngle*3.141592/360.0);

	GetClientRect(&m_oldRect);

	CWaitCursor wait;
	if( !m_pDoc->GetView3dSettingsArray()->IsEmpty() ) {
		OnLoadSettings((WPARAM) 0, (LPARAM)0);
	}

	// GL initializations
	wglMakeCurrent(m_pDC->GetSafeHdc(), m_hRC);
	SetProjectionMatrix(m_oldRect.Width(), m_oldRect.Height());

	// Setup Lights
	SetupLights();

	LoadTextures();

	// generate textures IDs
	glGenTextures(1, &m_srfRel.m_nTex);
	glGenTextures(1, &m_srfFld.m_nTex);

	// get grid surface
	CGrid* pGrd =  m_pDoc->GetReliefGrid();
	if(pGrd) {
		pGrd->SetColorTable(CTBL_REL);
		LoadGridSurface(pGrd, &m_srfRel, CTBL_REL);
	}

	m_srfFld.SetPlacement(CGlSurface::plcFltFlat);
	m_srfFld.SetLevels(m_dFldMinY, m_dFldMaxY);
	pGrd =  m_pDoc->GetGrid(m_nFld);
	if(pGrd) {
		pGrd->SetColorTable(CTBL_FLD);
		LoadGridSurface(pGrd, &m_srfFld, CTBL_FLD, m_dFldMinY, m_dFldMaxY, m_bFldOnRel);
	}

	// load leading horizons
	LoadLeadingHorizons();

	// compute axis labels
	InitTextLabels();
	wglMakeCurrent(NULL, NULL);

	wait.Restore();

}

void CViewGL3D::SetTransformations()
{
	if( !m_bPlaneSimulator ) {
		if( m_nProjection == PRJ_PERSPECTIVE ) {
			// user camera position
			glTranslated(m_dCameraX, m_dCameraY, m_dCameraZ);

			// User rotation
			double theta;
			theta = m_dProjAngle/2;
			m_dCameraEyeZ = (m_diag*2/3)/tan(theta*3.141592/180.0);

			glTranslated(0.0f, 0.0f, -m_dCameraEyeZ );
			glRotatef(m_fRotX, 1.0f, 0.0f, 0.0f);
			glRotatef(m_fRotY, 0.0f, 1.0f, 0.0f);
			glRotatef(m_fRotZ, 0.0f, 0.0f, 1.0f);
			glTranslated(0.0f, 0.0f, m_dCameraEyeZ );

			glScaled(m_dScX, m_dScY, m_dScZ);

			// Sets camera position.
			gluLookAt( m_midX, m_midY, m_midZ-m_dCameraEyeZ,
					m_midX, m_midY, m_midZ,
					0, 1, 0);
		}
		if( m_nProjection == PRJ_ORTHO ) {
			glRotatef(m_fRotX, 1.0f, 0.0f, 0.0f);
			glRotatef(m_fRotY+180, 0.0f, 1.0f, 0.0f);
			glRotatef(m_fRotZ, 0.0f, 0.0f, 1.0f);
			glTranslated(-m_midX, -m_midY, -m_midZ );
		}
	}
	else {
		glRotated(m_plRotX, 1.0, 0.0, 0.0);		// heading
		glRotated(m_plRotY, 0.0, 1.0, 0.0);		// pitch
		glRotated(m_plRotZ, 0.0, 0.0, 1.0);		// rool

		glScaled(m_dScX, m_dScY, m_dScZ);

		glTranslated(-m_plX, -m_plY, -m_plZ);
	}

	// get matrices for inverse transformation
	glGetIntegerv(GL_VIEWPORT, m_viewport);
	glGetDoublev(GL_MODELVIEW_MATRIX, m_modelMatrix);
	glGetDoublev(GL_PROJECTION_MATRIX, m_projMatrix);
}

void CViewGL3D::DrawScene()
{
	GetClientRect(&m_oldRect);
	SetProjectionMatrix(m_oldRect.Width(), m_oldRect.Height() );

	glPushMatrix();
		SetTransformations();

		glDepthMask(GL_TRUE);
		// coordinate system
		DrawCoordinates();

		// grid lines
		DrawGridLines();

		// draw selection points
		if( m_nMode == GL_SELECT )
			DrawSelectionPoints();

		// wells
		DrawWell();

		// bodies
		DrawBodies();

		// Draw Surfaces
		m_srfRel.DrawGL(FALSE);
		m_srfFld.DrawGL(FALSE);

		// leading horizons surfaces
		DrawLeadingHorizons();

		glDepthMask(GL_FALSE);
			// draw imported bitmaps
			DrawImportedBitmaps();
			// transparent profile planes
			DrawProfilePlanes();
		glDepthMask(GL_TRUE);

		// axis labels
		DrawAxisLabels();

		DrawCoordSys();

		// draw orientation overview in plane mode
		if( m_bPlaneSimulator )
			DrawOrientation();
	glPopMatrix();


	DrawLegend();

};

void CViewGL3D::DrawWell(void)
{
	MyObPtrArray* pObs = m_pDoc->GetObjArray();
	CMyObject*	pOb;
	CWell* pW;
	CData3D* pD;

	for(int i=0; i< pObs->GetSize(); i++) {
		pOb = pObs->GetAt(i);
		if( pOb->IsKindOf( RUNTIME_CLASS(CWell) ) ) {
			pW = (CWell*) pOb;
			pW->DrawGL();
		}
		if( pOb->IsKindOf( RUNTIME_CLASS(CData3D) ) ) {
			pD = (CData3D*) pOb;
			pD->DrawGL();
		}
	}
}

void CViewGL3D::DrawBodies(void)
{
	if( m_mod->IsBodyArrayEmpty() ) return;

	// SHADE MODEL, LIGHTS & MATERIAL TEST
	if(m_bBodySolid)		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	else	{
		glLineWidth(1.0f);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}

	glEnable(GL_LIGHTING);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	if(m_bTransparentBodies) {
		glDepthMask(GL_FALSE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else {
		glDisable(GL_BLEND);
	}
	if(m_bCullFacet) {
		glEnable(GL_CULL_FACE);
		glCullFace(m_nCullFacet);
	}

	float colorFront[4];	// front facet as seen from outside the body
	float colorBack[4];		// back facet as ssen from the inside the body
	float matSpec[4] = {0.5f,0.5f,0.5f,0.1f};		// back facet as ssen from the inside the body

	// data model
	DDArray*	pFFA;
	DoubleArray*	pFA;
	double*		pF;
	double		dBot;
	int			rows, cols;
	int			rowMin, rowMax, colMin, colMax;
	int			nIndex;

	pFFA = m_mod->GetGLFacets();

	double rem;
	rem = fmod(m_fRotY, 360.0);
	if(rem < 0.0) 
		rem += 360.0;


	rows = m_mod->GetRows();
	cols = m_mod->GetCols();
	dBot = m_mod->GetHell();
	rowMin = m_nRowStart;
	rowMax = m_nRowEnd;
	colMin = m_nColStart;
	colMax = m_nColEnd;
	if( !m_bOutFct ) {
		if( rowMin == 0 ) rowMin=1;
		if( rowMax == rows-1 ) rowMax-=2;
		if( colMin == 0 ) colMin=1;
		if( colMax == cols-1 ) colMax-=2;
	}

	for(int k=rowMin; k<=rowMax; k++) {
		for(int l=colMin; l<=colMax; l++) {
			nIndex = cols*k + l;

			pFA = &pFFA->GetAt(nIndex);
			int j = pFA->GetSize()/20;
			pF = pFA->GetData();
			for(int i=0; i<j; i++) {
				if(m_bLightsEnabled) {
					colorFront[0] = (float) (pF[20*i + 12]/3);
					colorFront[1] = (float) (pF[20*i + 13]/3);
					colorFront[2] = (float) (pF[20*i + 14]/3);
					colorFront[3] = (float) (pF[20*i + 15]);
					colorBack[0] =	(float) (pF[20*i + 16]/3);
					colorBack[1] =	(float) (pF[20*i + 17]/3);
					colorBack[2] =	(float) (pF[20*i + 18]/3);
					colorBack[3] =	(float) (pF[20*i + 19]);
				} 
				else {
					colorFront[0] = (float) (pF[20*i + 12]);
					colorFront[1] = (float) (pF[20*i + 13]);
					colorFront[2] = (float) (pF[20*i + 14]);
					colorFront[3] = (float) (pF[20*i + 15]);
					colorBack[0] =	(float) (pF[20*i + 16]);
					colorBack[1] =	(float) (pF[20*i + 17]);
					colorBack[2] =	(float) (pF[20*i + 18]);
					colorBack[3] =	(float) (pF[20*i + 19]);
				};	// if(m_bLightsEnabled)

				if( m_bDarkenInside ) {
					if(colorFront[0]!=colorBack[0] || colorFront[1]!=colorBack[1] || colorFront[2]!=colorBack[2] ) {
						colorFront[0] *= m_fDarkenInside;
						colorFront[1] *= m_fDarkenInside;
						colorFront[2] *= m_fDarkenInside;
					}
					colorBack[0] *= m_fDarkenInside;
					colorBack[1] *= m_fDarkenInside;
					colorBack[2] *= m_fDarkenInside;
				}

				if(colorBack[3]==0.0f && colorFront[3]!=0.0f) {
					colorBack[0] = colorFront[0];
					colorBack[1] = colorFront[1];
					colorBack[2] = colorFront[2];
					colorBack[3] = colorFront[3];
				}
				if(colorBack[3]!=0.0f && colorFront[3]==0.0f) {
					colorFront[0] = colorBack[0];
					colorFront[1] = colorBack[1];
					colorFront[2] = colorBack[2];
					colorFront[3] = colorBack[3];
				}

				if( (m_bVrtFct || (pF[20*i + 10] != 0.0 )) && (colorBack[3]!=0.0f || colorFront[3]!=0.0f) &&
					(!m_bBottom && pF[20*i + 1]!=dBot && pF[20*i + 4]!=dBot && pF[20*i + 7]!= dBot) ) {			// do not show side facets
					glBegin(GL_TRIANGLES);
						glMaterialfv(GL_FRONT, GL_EMISSION, colorFront);
						glMaterialfv(GL_FRONT, GL_DIFFUSE, colorFront);
						glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, colorFront);
						glMaterialfv(GL_BACK, GL_EMISSION, colorBack);
						glMaterialfv(GL_BACK, GL_DIFFUSE, colorBack);
						glMaterialfv(GL_BACK, GL_AMBIENT, colorBack);
						//glMaterialf(GL_FRONT, GL_SHININESS, 120.0f);
						//glMaterialfv(GL_FRONT, GL_SPECULAR, matSpec);
						glNormal3d(pF[20*i + 9], pF[20*i + 10], pF[20*i + 11]);
						glVertex3d(pF[20*i + 0], pF[20*i + 1], pF[20*i + 2]);

						glNormal3d(pF[20*i + 9], pF[20*i + 10], pF[20*i + 11]);
						glVertex3d(pF[20*i + 3], pF[20*i + 4], pF[20*i + 5]);

						glNormal3d(pF[20*i + 9], pF[20*i + 10], pF[20*i + 11]);
						glVertex3d(pF[20*i + 6], pF[20*i + 7], pF[20*i + 8]);
					glEnd();
				} // if( m_bVrtFct || ( m_ptsNormal[9*i+1] != 0.0) )

			}//for(l)
		}// for(k)
	}

	glDepthMask(GL_TRUE);
	glDisable(GL_CULL_FACE);
}

//  GL selection points
// one selection point is stored as follows
// {x,y,z, bodyID,row,col,index,indexZ,indexGL, r,g,b,a }
// where x,y,z:					point coordinates
//		bodyID,row,col,index, i:	integerbodyID, ..., index: in 'm_data' array, indexZ:	"depth" index
//									indexGL index in 'm_glSelPt' array
//		r,g,b,a:				color of upper body point 
//		13 entries per point => totalSize = 11*numberOfPoints
// {x=0,y=1,z=2, bodyID=3,row=4,col=5,index=6,indexZ=7,indexGL=8, r=9,g=10,b=11,a=12 }
void CViewGL3D::DrawSelectionPoints(int nMode)
{
	BOOL bLighting = glIsEnabled(GL_LIGHTING);
	//glDisable(GL_LIGHTING);
	//glPointSize(m_fSelSensitivity);
	CheckGlError();

	DDArray*		pDDA;
	DoubleArray*	pDA;
	double*			pD;
	int				nIndex;
	int				n;
	int				rows, cols;
	int				rowMin, rowMax, colMin, colMax;

	pDDA = m_mod->GetGLSelPt();

	rows = m_mod->GetRows();
	cols = m_mod->GetCols();
	rowMin = m_nRowStart;
	rowMax = m_nRowEnd;
	colMin = m_nColStart;
	colMax = m_nColEnd;

 	glEnable(GL_LIGHTING);
	glEnable(GL_BLEND);
	GLUquadricObj*	pQuad;
	float color[4]={0.8f,0.0f,0.0f,1.0f};
	float matSpec[4] = {0.5f,0.5f,0.5f,0.1f};
	float matEm[4] = {0.0f,0.0f,0.0f,0.1f};
 	double r = m_xSize/50;
	pQuad = gluNewQuadric();
	gluQuadricNormals(pQuad, GLU_SMOOTH);
	
	glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, matEm);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, color);
	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, color);
	glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 120.0f);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, matSpec);
	//glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, color);
	//glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, color);

	for(int k=rowMin; k<rowMax; k++) {
		for(int l=colMin; l < colMax; l++) {
			nIndex = cols*k + l;
			pDA = &pDDA->GetAt(nIndex);
			pD = pDA->GetData();
			n = pDA->GetSize()/13;

			for(int i=0; i<n; i++) {
				// {x=0,y=1,z=2, bodyID=3,row=4,col=5,index=6,indexZ=7,indexGL=8, r=9,g=10,b=11,a=12 }
				if(nMode == GL_SELECT) {
					glLoadName( (GLuint) pD[13*i + 3] );
					glPushName( (GLuint) pD[13*i + 4] );
					glPushName( (GLuint) pD[13*i + 5] );
					glPushName( (GLuint) pD[13*i + 6] );
					glPushName( (GLuint) pD[13*i + 7] );
					glPushName( (GLuint) pD[13*i + 8] );
				}
				//BOOL bError = CheckGlError();

				glPushMatrix(); 
					color[0]=(float)pD[13*i+9];	color[1]=(float)pD[13*i+10];	color[2]=(float)pD[13*i+11];
					glMaterialfv( GL_FRONT_AND_BACK, GL_DIFFUSE, color );
					glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, color);
					glTranslated( pD[13*i + 0], pD[13*i + 1] , pD[13*i + 2]);
					gluSphere( pQuad, m_fSelSensitivity, 5, 5);
				glPopMatrix();

				if(nMode == GL_SELECT) {
					glPopName();
					glPopName();
					glPopName();
					glPopName();
					glPopName();
				}
			}
		}
	}

	
	if(bLighting) {
		glEnable(GL_LIGHTING);
	}
}

void CViewGL3D::DrawImportedBitmaps(void)
{
	if(!m_bDrawBitmaps) return;
	if(m_bUpdateBitmaps) 
		LoadTextures();

	glDisable(GL_LIGHTING);
	glEnable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthFunc(GL_LESS);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	if( !m_objArray.IsEmpty() ) {
		for(int i=0; i<m_objArray.GetCount(); i++) {
			CGlBitmap* pGlBmp;
			pGlBmp = (CGlBitmap*) m_objArray.GetAt(i);
			if( pGlBmp->GetType() == CGlBitmap::glBmp_Map ) {
				pGlBmp->DrawOnrelief(m_pDoc->GetReliefGrid(), 0.4, m_mod->GetHeaven());
			}
			else {
				pGlBmp->DrawGL(m_fAlphaBmp);
			}
		}
	}
}

void CViewGL3D::DrawProfilePlanes(void)
{
	if(!m_bProfilePlanes) return;

	// GL hints
	glDisable(GL_LIGHTING);
	//glDisable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	if(m_bProfilePlanesSolid)	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	else						glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	POSITION pos = m_pDoc->GetFirstViewPosition();
	while (pos != NULL) {
		CView* pView = m_pDoc->GetNextView(pos);
		if( pView->IsKindOf(RUNTIME_CLASS( CViewProf )) ) {
			double	xt, yt, zt, xb, yb, zb;
			((CViewProf*) pView)->GetProfRect(xt, yt, zt, xb, yb, zb);
			zt = m_dFldMinY;	// default
			if( m_nFld != -1 && !m_srfFld.IsEmpty() ) {
				switch( m_srfFld.GetPlacement() ) {
					case CGlSurface::plcOnRelief:
						zt = m_dFldMinY;
						break;
					case CGlSurface::plcFltSrf:
						zt = m_dFldMaxY;
						break;
					case CGlSurface::plcFltFlat:
						zt = (m_dFldMinY+m_dFldMaxY)/2.0 + (m_dFldMaxY-m_dFldMinY)/10;
						break;
				}
			}

 			glBegin(GL_QUADS);
				if( ((CViewProf*) pView)->GetProfType() == PROF_X)
					glColor4f(0.6f, 0.6f, 0.8f, m_fAlphaProf);
				if( ((CViewProf*) pView)->GetProfType() == PROF_Y)
					glColor4f(0.6f, 0.8f, 0.6f, m_fAlphaProf);
				glVertex3d( xt, zt, -yt);
				glVertex3d( xb, zt, -yb);
				glVertex3d( xb, zb, -yb);
				glVertex3d( xt, zb, -yt);
			glEnd();
		}
	}   
	glDisable(GL_BLEND);
	SetupLights(m_bLightsEnabled);
}

void CViewGL3D::DrawCoordinates(void)
{
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);;
	glDisable(GL_LIGHTING);
	glPointSize(2);
	glColor3f(0.5f, 0.5f, 0.5f);

	glBegin(GL_POINTS);
		glVertex3d(m_minX, m_maxY, m_maxZ);
		glVertex3d(m_minX, m_minY, m_maxZ);
		glVertex3d(m_maxX, m_maxY, m_maxZ);
		glVertex3d(m_maxX, m_minY, m_maxZ);
		glVertex3d(m_maxX, m_maxY, m_minZ);
		glVertex3d(m_maxX, m_minY, m_minZ);
		glVertex3d(m_minX, m_maxY, m_minZ);
		glVertex3d(m_minX, m_minY, m_minZ);
	glEnd();

	glLineWidth( 1.0 );
	glColor3f(0.5f, 0.5f, 0.5f);
	GLdouble a = m_diag/60.0f, b = m_diag/80.0f, c = m_diag/180.0f;
	glBegin(GL_LINES);
		glVertex3d(m_maxX, m_maxY, m_maxZ);
		glVertex3d(m_maxX, m_minY, m_maxZ);

		glVertex3d(m_maxX, m_maxY, m_minZ);
		glVertex3d(m_maxX, m_minY, m_minZ);

		glVertex3d(m_minX, m_maxY, m_minZ);
		glVertex3d(m_minX, m_minY, m_minZ);

		// x gl-axis
		glColor3f(0.0f, 1.0f, 0.0f);
		glVertex3d(m_minX, m_minY, m_maxZ);
		glVertex3d(m_maxX+a, m_minY, m_maxZ);

		// y gl-axis
		glColor3f(0.0f, 0.0f, 1.0f);
		glVertex3d(m_minX, m_minY, m_maxZ);
		glVertex3d(m_minX, m_minY, m_minZ-a);

		// z gl-axis
		glColor3f(1.0f, 0.0f, 0.0f);
		glVertex3d(m_minX, m_minY, m_maxZ);
		glVertex3d(m_minX, m_maxY+a, m_maxZ);
	glEnd();

	glEnable(GL_LIGHTING);
	GLUquadricObj*	pCone;
	float *pColor = new float[4];
	pCone = gluNewQuadric();

	// Cone for z-gl-axis
	pColor[0] = 0.0f;
	pColor[1] = 0.0f;
	pColor[2] = 1.0f;
	pColor[3] = 1.0f;
	glColor4fv(pColor);
	glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, pColor);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, pColor);
	glPushMatrix(); 
		glTranslated(m_minX, m_minY , m_minZ - a + b);
		glRotatef(180.0f ,0.0f, 1.0f, 0.0f ); 
		gluCylinder(pCone, c, 0.0f, b, 8, 8);
		gluQuadricNormals(pCone, GLU_SMOOTH);
	glPopMatrix();

	// Cone for y-gl-axis
	pColor[0] = 1.0f;
	pColor[1] = 0.0f;
	pColor[2] = 0.0f;
	pColor[3] = 1.0f;
	glColor4fv(pColor);
	glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, pColor);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, pColor);
	glPushMatrix(); 
		glTranslated(m_minX, m_maxY+a-b, m_maxZ );
		glRotatef(-90.0f ,1.0f, 0.0f, 0.0f ); 
		gluCylinder(pCone, c, 0.0f, b, 8, 8);
		gluQuadricNormals(pCone, GLU_SMOOTH);
	glPopMatrix();

	// Cone for x-gl-axis
	pColor[0] = 0.0f;
	pColor[1] = 1.0f;
	pColor[2] = 0.0f;
	pColor[3] = 1.0f;
	glColor4fv(pColor);
	glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, pColor);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, pColor);
	glPushMatrix(); 
		glTranslated(m_maxX+a-b, m_minY, m_maxZ );
		glRotatef(90.0f ,0.0f, 1.0f, 0.0f ); 
		gluCylinder(pCone, c, 0.0f, b, 8, 8);
		gluQuadricNormals(pCone, GLU_SMOOTH);
	glPopMatrix();

	delete pColor;
	glColor3f(1.0f, 1.0f, 1.0f);
}

void CViewGL3D::SetProjectionMatrix(int w, int h)
{
	GLdouble aspect;

	// Prevent a divide by zero, when window is too short
	// (you can't make a window of zero width)
	if(h == 0)
		h = 1;

	if(w == 0)
		w = 1;

    // Set the viewport to be the entire window
    glViewport(0, 0, w, h);

    // Set the clipping volume
	aspect = (float) w/h;

	// Set projection matrix
 	glPushMatrix();
		glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			//gluPerspective(m_dProjAngle, aspect, 0.001*m_diag, 1.0e8*m_diag );
			//--------------------------------------------------------------------
			// set up ortographic projection
			if( m_nProjection == PRJ_ORTHO ) {
				double wm, hm;			// model width
				wm = hm = m_diag*0.6;	
				wm/hm < aspect ? wm=aspect*hm : hm=wm/aspect;
				wm *= m_dOrthoScale;
				hm *= m_dOrthoScale;
				glOrtho(-wm, +wm, -hm, +hm, -m_diag, m_diag);
			}

 			// Set perspective projection matrix
			if( m_nProjection == PRJ_PERSPECTIVE ) {
				// Set the clipping volume
				gluPerspective(m_dProjAngle, aspect, 0.001*m_diag, 1.0e8*m_diag );
			}
			//--------------------------------------------------------------------
		glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
}

int CViewGL3D::SetupLights(BOOL bEnable)
{
	if (bEnable) {
		glEnable(GL_LIGHTING);
		glLightModelf(GL_LIGHT_MODEL_TWO_SIDE, 1.0f);

		glFrontFace(GL_CCW);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		glShadeModel(GL_FLAT);

		// We`ll white light, so Ambient + Diffuse = White (1.0f, 1.0f, 1.0f, 1.0f)
		GLfloat ambientIntensity[4] = {0.3f, 0.3f, 0.3f, 1.0f};
		GLfloat diffuseIntensity[4] = {0.7f, 0.7f, 0.7f, 1.0f};

		GLfloat position0[4] = {(float) (m_midX),		(float) (m_midY+m_lenY),	(float) (m_midZ, 0.0f)};
		GLfloat position1[4] = {(float) (m_midX),		(float) (m_midY),			(float) (m_midZ+m_lenZ), 0.0f};
		GLfloat position2[4] = {(float) (m_midX+m_lenX), (float) (m_midY),			(float) (m_midZ, 0.0f)};

		// Light from UP
		glEnable(GL_LIGHT0);
		glLightfv(GL_LIGHT0, GL_AMBIENT, ambientIntensity);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseIntensity);
		glLightfv(GL_LIGHT0, GL_POSITION, position0);

		// Light from viewer
		glEnable(GL_LIGHT1);
		glLightfv(GL_LIGHT1, GL_AMBIENT, ambientIntensity);
		glLightfv(GL_LIGHT1, GL_DIFFUSE, diffuseIntensity);
		glLightfv(GL_LIGHT1, GL_POSITION, position1);

		// Light from left
		glEnable(GL_LIGHT2);
		glLightfv(GL_LIGHT2, GL_AMBIENT, ambientIntensity);
		glLightfv(GL_LIGHT2, GL_DIFFUSE, diffuseIntensity);
		glLightfv(GL_LIGHT2, GL_POSITION, position2);
		m_bSetupLights = FALSE;

	} 
	else {
		glDisable(GL_LIGHT0);
		glDisable(GL_LIGHT1);
		glDisable(GL_LIGHT2);
		glDisable(GL_LIGHTING);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	};

	return 0;
}

// CViewGL3D message handlers

void CViewGL3D::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	double l;
	switch(nChar) {
		case VK_UP:
			if(!m_bPlaneSimulator) {
				m_dCameraZ -= m_mZ;
			}
			else {
				l = m_pld*cos(DegToRad(m_plRotV));
				m_plX += l*sin(DegToRad(m_plRotY));
				m_plY -= m_pld*sin(DegToRad(m_plRotV));
				m_plZ -= l*cos(DegToRad(m_plRotY));
				//m_plX += sin(DegToRad(m_plRotY)) * m_pld;
				//m_plY -= sin(DegToRad(m_plRotX)) * m_pld;
				//m_plZ -= cos(DegToRad(m_plRotY)) * m_pld;
			}
			Invalidate(FALSE);
			break;
		case VK_DOWN:
			if(!m_bPlaneSimulator) {
				m_dCameraZ += m_mZ;
			}
			else {
				l = m_pld*cos(DegToRad(m_plRotV));
				m_plX -= l*sin(DegToRad(m_plRotY));
				m_plY += m_pld*sin(DegToRad(m_plRotV));
				m_plZ += l*cos(DegToRad(m_plRotY));
			}
			Invalidate(FALSE);
			break;
		case VK_RIGHT:
			if(!m_bPlaneSimulator) {
				m_fRotY += (int) (m_rY);
			}
			else {
				m_plRotY = AngleDeg(m_plRotY+m_plRotdY);
			}
			Invalidate(FALSE);
			break;
		case VK_LEFT:
			if(!m_bPlaneSimulator) {
				m_fRotY -= (int) (m_rY);
			}
			else {
				m_plRotY = AngleDeg(m_plRotY-m_plRotdY);
			}
			Invalidate(FALSE);
			break;
		case VK_NEXT:	// page down
			if(!m_bPlaneSimulator) {
				m_dCameraY -= m_mY;
			}
			else {
				m_plY -= m_pld;
			}
			Invalidate(FALSE);
			break;
		case VK_PRIOR:	// [page up
			if(!m_bPlaneSimulator) {
				m_dCameraY += m_mY;
			}
			else {
				m_plY += m_pld;
			}
			Invalidate(FALSE);
			break;
		case 69:	// E
			if(!m_bPlaneSimulator) {
				m_fRotX = 0.0f;
				m_fRotY = 90.0f;
				m_fRotZ = 0.0f;
				Invalidate(FALSE);
			}
			break;
		case 87:	// W
			if(!m_bPlaneSimulator) {
				m_fRotX = 0.0f;
				m_fRotY = 270.0f;
				m_fRotZ = 0.0f;
				Invalidate(FALSE);
			}
			break;
		case 78:	// N
			if(!m_bPlaneSimulator) {
				m_fRotX = 0;
				m_fRotY = 0.0f;
				m_fRotZ = 0;
				Invalidate(FALSE);
			}
			break;
		case 83:	// S
			if(!m_bPlaneSimulator) {
				m_fRotX = 0;
				m_fRotY = 180.0f;
				m_fRotZ = 0;
				Invalidate(FALSE);
			}
			break;
		case 77:	// M
			if(!m_bPlaneSimulator) {
				m_fRotX = 90.0f;
				m_fRotY = 180.0f;
				m_fRotZ = 0.0f;
				Invalidate(FALSE);
			}
			break;
		case VK_ESCAPE:
			break;
	}

	
	//CView::OnKeyDown(nChar, nRepCnt, nFlags);
}

void CViewGL3D::OnLButtonDown(UINT nFlags, CPoint point)
{
	if(nFlags==MK_LBUTTON && m_nMode == GL_SELECT) {
		SelectVertex(nFlags, point.x, point.y);
		return;
	}

	m_ptOld = point;
	//CView::OnLButtonDown(nFlags, point);	// default
}

void CViewGL3D::OnLButtonUp(UINT nFlags, CPoint point)
{
	if( GetCapture() == this ) 
		ReleaseCapture();

	CView::OnLButtonUp(nFlags, point);
}

void CViewGL3D::OnMouseMove(UINT nFlags, CPoint point)
{
	if( GetCapture() == this ) {
		DDArray*		pDDA;
		DoubleArray*	pDA;
		double*			pD;
		double gx, gy, gz;

		pDDA = m_mod->GetGLSelPt();
		pDA = &pDDA->GetAt(m_nSelIndex);
		pD = pDA->GetData();
		gx = pD[13*m_nSelIndexGL + 0];
		gy = pD[13*m_nSelIndexGL + 1];
		gz = pD[13*m_nSelIndexGL + 2];
		DPtoGL(point.x, point.y, m_dSelZ, gx, gy, gz, TRC_INVSP);
		g_x = gx;
		g_z = gy;
		g_y = -gz;
		if( m_mod->MoveVertex(m_nSelIndexZ, m_nSelRow, m_nSelCol, gy, nFlags) == -1 ) {
			ReleaseCapture();
		}

		// Update field if needed
		m_pDoc->SetModifiedFlag();
		CVrtxMove update;
		update.nBodyID = -1;
		update.nRow = m_nSelRow; 
		update.nCol = m_nSelCol;
		update.nIndex = m_nSelIndexZ;
		update.dNewZ = DUMMY;
		Invalidate(FALSE);
		m_pDoc->UpdateAllViews(this, UPDATE_MOVE_VERTEX, &update);
		m_pDoc->UpdateField();

		return;
	}

	if(m_nMode == GL_SELECT && nFlags==MK_LBUTTON) {
		return;
	}

	int xDif, yDif;

	xDif = point.x - m_ptOld.x;  if(xDif!=0) (xDif<0 ? xDif=-1+xDif/10 : xDif=1+xDif/10);
	yDif = point.y - m_ptOld.y;  if(yDif!=0) (yDif<0 ? yDif=-1+yDif/10 : yDif=1+yDif/10);

	CRect rcClient;
	GetClientRect( &rcClient );
	double drx, dry;
	// rotation in degrees relative to client rectangle
	dry = 360.0 * double(point.x - m_ptOld.x)/rcClient.Width();
	drx = 360.0 * double(point.y - m_ptOld.y)/rcClient.Height();

	if( m_nProjection == PRJ_PERSPECTIVE ) {
		switch( nFlags ) {
		case MK_LBUTTON:
			{
			if(!m_bPlaneSimulator) {
				m_fRotX = (float)AngleDeg( m_fRotX+drx);
				m_fRotY = (float)AngleDeg( m_fRotY+dry);
			}
			// plane mode
			else {
				m_plRotY = AngleDeg(m_plRotY+dry);
				m_plRotV+=drx;
				if(m_plRotV>90.0) m_plRotV=90.0;
				if(m_plRotV<-90.0) m_plRotV=-90.0;
				m_plRotX = m_plRotV;
			}

			Invalidate(FALSE);
			}
			break;
		case MK_LBUTTON | MK_CONTROL | MK_SHIFT:
			{
			if(!m_bPlaneSimulator) {
				m_fRotX = (float)AngleDeg( m_fRotX+drx);
				m_fRotY = (float)AngleDeg( m_fRotY+dry);
			}
			// plane mode
			else {
				m_plRotY = AngleDeg(m_plRotY+dry);
				m_plRotV+=drx;
				if(m_plRotV>90.0) m_plRotV=90.0;
				if(m_plRotV<-90.0) m_plRotV=-90.0;
				m_plRotX = m_plRotV;
			}

			Invalidate(FALSE);
			}
			break;
		case MK_LBUTTON | MK_CONTROL:
			{
			if(!m_bPlaneSimulator) {
				m_dCameraZ += yDif * m_mZ;
			}
			// plane mode
			else {
				m_plRotY = AngleDeg(m_plRotY+dry);
				m_plRotV+=drx;
				if(m_plRotV>90.0) m_plRotV=90.0;
				if(m_plRotV<-90.0) m_plRotV=-90.0;
				m_plRotX = m_plRotV;
			}

			Invalidate( FALSE );
			}
			break;
		case MK_LBUTTON | MK_SHIFT:
			{
			if(!m_bPlaneSimulator) {
				m_dCameraY -= yDif * m_mY;
				m_dCameraX += xDif * m_mX;
			}
			else {
				m_plY = m_plY+m_pld;
			}

			Invalidate( FALSE );
			}
			break;
		}
	}
		
	if( m_nProjection == PRJ_ORTHO ) {
		switch( nFlags ) {
		case MK_LBUTTON:
			{
			m_fRotX = (float)AngleDeg( m_fRotX+drx);
			m_fRotY = (float)AngleDeg( m_fRotY+dry);
			Invalidate(FALSE);
			}
			break;
		case MK_LBUTTON | MK_CONTROL | MK_SHIFT:
			{
			m_fRotX = (float)AngleDeg(m_fRotX+drx);
			m_fRotY = (float)AngleDeg( m_fRotY+dry);
			Invalidate(FALSE);
			}
			break;
		case MK_LBUTTON | MK_CONTROL:
			{
			if(yDif!=0.0)
				m_dOrthoScale *= 1 - 0.01*fabs(double(yDif))/yDif;
			Invalidate( FALSE );
			}
			break;
		case MK_LBUTTON | MK_SHIFT:
			{
			//Invalidate( FALSE );
			}
			break;
		}
	}
	
	m_ptOld = point;

	//CView::OnMouseMove(nFlags, point);		// default
}

void CViewGL3D::OnRButtonDown(UINT nFlags, CPoint point)
{
	Properties();
}

// 3D View properties
void CViewGL3D::Properties(BOOL bInit)
{
	CView3dSheet	sheet("3D View Properties");

	CDlgPp3DAxes    pgAx;
	CDlgPp3dField	pgFld;
	CDlgPp3dModel	pgMod;
	CDlgPp3dSetings pgSet;
	CDlgPp3dDraw	pgDrw;
	CDlgPp3dSaveLoad	pgSvl;

	pgSvl.m_pView3DSet = m_pDoc->GetView3dSettingsArray();

	sheet.AddPage(&pgSet);
	sheet.AddPage(&pgMod);
	sheet.AddPage(&pgDrw);
	sheet.AddPage(&pgFld);
	sheet.AddPage(&pgAx);
	sheet.AddPage(&pgSvl);

	pgSet.m_dProjAngle = m_dProjAngle;
	pgSet.m_mX = m_mX;
	pgSet.m_mY = m_mZ;
	pgSet.m_mZ = m_mY;
	pgSet.m_rX = m_rX;
	pgSet.m_rY = m_rZ;
	pgSet.m_rZ = m_rY;
	pgSet.m_fRotX = m_fRotX;
	pgSet.m_fRotY = m_fRotY;
	pgSet.m_fRotZ = m_fRotZ;
	pgSet.m_fSelSensitivity = m_fSelSensitivity;
	pgSet.m_dScX = m_dScX;
	pgSet.m_dScY = m_dScZ;
	pgSet.m_dScZ = m_dScY;
	switch( m_nMode) {
		case GL_RENDER:
			pgSet.m_nMode = 0;
			break;
		case GL_SELECT:
			pgSet.m_nMode = 1;
			break;
		default:
			pgSet.m_nMode = 0;
	}

	pgMod.m_bCullFace = m_bCullFacet;
	switch( m_nCullFacet) {
		case GL_FRONT:
			pgMod.m_nCullFacet = 0;
			break;
		case GL_BACK:
			pgMod.m_nCullFacet = 1;
			break;
		default:
			pgMod.m_nCullFacet = 0;
	}
	pgMod.m_bModelOutFct = m_bOutFct;
	pgMod.m_bModelSolid = m_bBodySolid;
	pgMod.m_bModelVertFct = m_bVrtFct;
	pgMod.m_bModelTransparent = m_bTransparentBodies;
	pgMod.m_nColMax = m_nColMax;
	pgMod.m_nRowMax = m_nRowMax;
	pgMod.m_nRowStart = m_nRowStart;
	pgMod.m_nRowEnd = m_nRowEnd;
	pgMod.m_nColStart = m_nColStart;
	pgMod.m_nColEnd = m_nColEnd;
	pgMod.m_fDarkenInside = m_fDarkenInside;
	pgMod.m_bDarkenInside = m_bDarkenInside;
	pgMod.m_bBottom = m_bBottom;

	// grid lines
	pgAx.m_bGrdLineDwnX = m_bGrdLineDwnX;
	pgAx.m_bGrdLineDwnY = m_bGrdLineDwnZ;
	pgAx.m_bGrdLineUpX = m_bGrdLineUpX;
	pgAx.m_bGrdLineUpY = m_bGrdLineUpZ;
	pgAx.m_bGrdLineVrt = m_bGrdLineVrt;
	pgAx.m_nAxLabels = m_nTextType;

	// fill combo box
	m_pDoc->GetGridsNonEmptyString(pgFld.m_strFld);
	pgFld.m_strFldSel = m_pDoc->GetFieldString(m_nFld);
	pgFld.m_fFldAlpha = m_srfFld.GetAlpha();
	pgFld.m_bFldSolid = m_srfFld.IsSolid();
	pgFld.m_bFldShow = m_srfFld.IsVisible();
	pgFld.m_nFldPlacement = m_srfFld.GetPlacement()-1;

	pgDrw.m_bBmpShow = m_bDrawBitmaps;
	pgDrw.m_bLeadHrzShow = m_bHorizon;
	pgDrw.m_bLeadHrzSolid = m_bHorizonSolid;
	pgDrw.m_fAlphaLeadHrz = m_fHorizonAlpha;
	pgDrw.m_bPrfShow = m_bProfilePlanes;
	pgDrw.m_bPrfSolid = m_bProfilePlanesSolid;
	pgDrw.m_fAlphaPrf = m_fAlphaProf;
	pgDrw.m_bRelShow = m_srfRel.IsVisible();
	pgDrw.m_bRelSolid = m_srfRel.IsSolid();
	pgDrw.m_fAlphaRel = m_srfRel.GetAlpha();

	m_pSheetProperties = &sheet;
	if(sheet.DoModal() == IDOK) {
		PropertiesChange(&sheet);
	}
	m_pSheetProperties = NULL;
	m_pDoc->SetModifiedFlag();
}

void CViewGL3D::PropertiesInit(CView3dSheet* pSheet)
{
	ASSERT(pSheet != NULL);
	if( pSheet == NULL) return;

	CDlgPp3DAxes*    pgAx;
	CDlgPp3dField*	pgFld;
	CDlgPp3dModel*	pgMod;
	CDlgPp3dSetings* pgSet;
	CDlgPp3dDraw*	pgDrw;
	//CDlgPp3dSaveLoad*	pgSvl;

	pgSet	= DYNAMIC_DOWNCAST( CDlgPp3dSetings, pSheet->GetPage(0) );
	pgMod	= DYNAMIC_DOWNCAST( CDlgPp3dModel ,pSheet->GetPage(1) );
	pgDrw	= DYNAMIC_DOWNCAST( CDlgPp3dDraw ,pSheet->GetPage(2) );
	pgFld	= DYNAMIC_DOWNCAST( CDlgPp3dField ,pSheet->GetPage(3) );
	pgAx	= DYNAMIC_DOWNCAST( CDlgPp3DAxes ,pSheet->GetPage(4) );

	pgSet->m_dProjAngle = m_dProjAngle;
	pgSet->m_mX = m_mX;
	pgSet->m_mY = m_mZ;
	pgSet->m_mZ = m_mY;
	pgSet->m_rX = m_rX;
	pgSet->m_rY = m_rZ;
	pgSet->m_rZ = m_rY;
	pgSet->m_fRotX = m_fRotX;
	pgSet->m_fRotY = m_fRotY;
	pgSet->m_fRotZ = m_fRotZ;
	pgSet->m_fSelSensitivity = m_fSelSensitivity;
	switch( m_nMode) {
		case GL_RENDER:
			pgSet->m_nMode = 0;
			break;
		case GL_SELECT:
			pgSet->m_nMode = 1;
			break;
		default:
			pgSet->m_nMode = 0;
	}

	pgMod->m_bCullFace = m_bCullFacet;
	switch( m_nCullFacet) {
		case GL_FRONT:
			pgMod->m_nCullFacet = 0;
			break;
		case GL_BACK:
			pgMod->m_nCullFacet = 1;
			break;
		default:
			pgMod->m_nCullFacet = 0;
	}
	pgMod->m_bModelOutFct = m_bOutFct;
	pgMod->m_bModelSolid = m_bBodySolid;
	pgMod->m_bModelVertFct = m_bVrtFct;
	pgMod->m_bModelTransparent = m_bTransparentBodies;
	pgMod->m_nColMax = m_nColMax;
	pgMod->m_nRowMax = m_nRowMax;
	pgMod->m_nRowStart = m_nRowStart;
	pgMod->m_nRowEnd = m_nRowEnd;
	pgMod->m_nColStart = m_nColStart;
	pgMod->m_nColEnd = m_nColEnd;
	pgMod->m_fDarkenInside = m_fDarkenInside;
	pgMod->m_bDarkenInside = m_bDarkenInside;
	pgMod->m_bBottom = m_bBottom;

	// grid lines
	pgAx->m_bGrdLineDwnX = m_bGrdLineDwnX;
	pgAx->m_bGrdLineDwnY = m_bGrdLineDwnZ;
	pgAx->m_bGrdLineUpX = m_bGrdLineUpX;
	pgAx->m_bGrdLineUpY = m_bGrdLineUpZ;
	pgAx->m_bGrdLineVrt = m_bGrdLineVrt;
	pgAx->m_nAxLabels = m_nTextType;

	// fill combo box
	m_pDoc->GetGridsNonEmptyString(pgFld->m_strFld);
	pgFld->m_strFldSel = m_pDoc->GetFieldString(m_nFld);
	pgFld->m_fFldAlpha = m_srfFld.GetAlpha();
	pgFld->m_bFldSolid = m_srfFld.IsSolid();
	pgFld->m_bFldShow = m_srfFld.IsVisible();

	pgDrw->m_bBmpShow = m_bDrawBitmaps;
	pgDrw->m_bLeadHrzShow = m_bHorizon;
	pgDrw->m_bLeadHrzSolid = m_bHorizonSolid;
	pgDrw->m_fAlphaLeadHrz = m_fHorizonAlpha;
	pgDrw->m_bPrfShow = m_bProfilePlanes;
	pgDrw->m_bPrfSolid = m_bProfilePlanesSolid;
	pgDrw->m_fAlphaPrf = m_fAlphaProf;
	pgDrw->m_bRelShow = m_srfRel.IsVisible();
	pgDrw->m_bRelSolid = m_srfRel.IsSolid();
	pgDrw->m_fAlphaRel = m_srfRel.GetAlpha();

}

// called from inside of property shhet
LRESULT CViewGL3D::OnPropertiesChange(WPARAM wParam, LPARAM lParam)
{
	if( m_pSheetProperties == NULL ) return 0;

	PropertiesChange( m_pSheetProperties );

	return 0;
}

// applies changes in properties
void CViewGL3D::PropertiesChange(CView3dSheet* pSheet)
{
	if( pSheet == NULL ) return;

	BOOL			b=FALSE;					// we shoul call Invalidate
	BOOL			bTmp;
	BOOL			bUpdateFld=FALSE;
	BOOL			bUpdateRel=FALSE;
	CGrid			grdTmp;
	CDlgPp3DAxes*	pgAx;
	CDlgPp3dField*	pgFld;
	CDlgPp3dModel*	pgMod;
	CDlgPp3dSetings* pgSet;
	CDlgPp3dDraw*	pgDrw;

	pgSet	= DYNAMIC_DOWNCAST( CDlgPp3dSetings, pSheet->GetPage(0) );
	pgMod	= DYNAMIC_DOWNCAST( CDlgPp3dModel ,pSheet->GetPage(1) );
	pgDrw	= DYNAMIC_DOWNCAST( CDlgPp3dDraw ,pSheet->GetPage(2) );
	pgFld	= DYNAMIC_DOWNCAST( CDlgPp3dField ,pSheet->GetPage(3) );
	pgAx	= DYNAMIC_DOWNCAST( CDlgPp3DAxes ,pSheet->GetPage(4) );

	// settings tab
	b |= SetChange( m_dProjAngle, pgSet->m_dProjAngle );
	if(b)	m_dCameraZ = 0.0/*(m_diag/2.0)/tan(m_dProjAngle*3.141592/360.0)*/;
	b |= SetChange( m_mX , pgSet->m_mX );
	b |= SetChange( m_mZ , pgSet->m_mY );
	b |= SetChange( m_mY , pgSet->m_mZ );
	b |= SetChange( m_rX , pgSet->m_rX );
	b |= SetChange( m_rZ , pgSet->m_rY );
	b |= SetChange( m_rY , pgSet->m_rZ );
	b |= SetChange( m_fRotX, pgSet->m_fRotX );
	b |= SetChange( m_fRotY, pgSet->m_fRotY );
	b |= SetChange( m_fRotZ, pgSet->m_fRotZ );
	b |= SetChange( m_fSelSensitivity, pgSet->m_fSelSensitivity );
	b |= SetChange( m_dScX, pgSet->m_dScX );
	b |= SetChange( m_dScZ, pgSet->m_dScY );
	b |= SetChange( m_dScY, pgSet->m_dScZ );

	int nMode;
	switch( pgSet->m_nMode ) {
		case 0:
			nMode = GL_RENDER;
			break;
		case 1:
			nMode = GL_SELECT;
			break;
		default:
			nMode = GL_RENDER;
	}
	if(m_nMode != nMode) b = TRUE;
	m_nMode = nMode;

	b |= SetChange( m_bCullFacet , pgMod->m_bCullFace );
	switch( pgMod->m_nCullFacet ) {
		case 0:
			nMode = GL_FRONT;
			break;
		case 1:
			nMode = GL_BACK;
			break;
		default:
			nMode = GL_FRONT;
	}
	if(m_nCullFacet != nMode) b = TRUE;
	m_nCullFacet = nMode;
	b |= SetChange( m_bOutFct , pgMod->m_bModelOutFct );
	b |= SetChange( m_bBodySolid , pgMod->m_bModelSolid );
	b |= SetChange( m_bVrtFct , pgMod->m_bModelVertFct );
	b |= SetChange( m_bTransparentBodies , pgMod->m_bModelTransparent );
	b |= SetChange( m_fDarkenInside, pgMod->m_fDarkenInside );
	b |= SetChange( m_bDarkenInside, pgMod->m_bDarkenInside );
	b |= SetChange( m_bBottom, pgMod->m_bBottom );

	// grid lines
	b |= SetChange( m_bGrdLineDwnX , pgAx->m_bGrdLineDwnX );
	b |= SetChange( m_bGrdLineDwnZ , pgAx->m_bGrdLineDwnY );
	b |= SetChange( m_bGrdLineUpX , pgAx->m_bGrdLineUpX );
	b |= SetChange( m_bGrdLineUpZ , pgAx->m_bGrdLineUpY );
	b |= SetChange( m_bGrdLineVrt , pgAx->m_bGrdLineVrt );
	b |= SetChange( m_nTextType, pgAx->m_nAxLabels );


	b |= SetChange( m_nColMax , pgMod->m_nColMax );
	b |= SetChange( m_nRowMax , pgMod->m_nRowMax );
	b |= SetChange( m_nRowStart , pgMod->m_nRowStart );
	b |= SetChange( m_nRowEnd , pgMod->m_nRowEnd );
	b |= SetChange( m_nColStart , pgMod->m_nColStart );
	b |= SetChange( m_nColEnd , pgMod->m_nColEnd );

	b |= SetChange( m_bDrawBitmaps , pgDrw->m_bBmpShow );
	b |= SetChange( m_bHorizon , pgDrw->m_bLeadHrzShow );
	b |= SetChange( m_bHorizonSolid , pgDrw->m_bLeadHrzSolid );
	b |= SetChange( m_fHorizonAlpha , pgDrw->m_fAlphaLeadHrz );
	b |= SetChange( m_bProfilePlanes , pgDrw->m_bPrfShow );
	b |= SetChange( m_bProfilePlanesSolid , pgDrw->m_bPrfSolid );
	b |= SetChange( m_fAlphaProf , pgDrw->m_fAlphaPrf );
	bTmp = m_srfRel.IsSolid();
	if( pgDrw->m_bRelSolid != bTmp )	b = TRUE;
	m_srfRel.SetSolid(pgDrw->m_bRelSolid);
	bTmp = m_srfRel.IsVisible();
	if( pgDrw->m_bRelShow != bTmp )	b = TRUE;
	m_srfRel.Show(pgDrw->m_bRelShow);
	if( m_srfRel.m_fAlpha != pgDrw->m_fAlphaRel )  {
		m_srfRel.SetAlpha(pgDrw->m_fAlphaRel);
		// get grid surface
		CGrid* pGrdRel =  m_pDoc->GetReliefGrid();
		if(pGrdRel) {
			//pGrdRel->CreateColorTableGL(m_srfRel.m_tex1D, m_srfRel.m_nTexWidth, m_srfRel.m_nTexFormat, CTBL_REL, m_srfRel.GetAlpha());
			wglMakeCurrent(m_pDC->GetSafeHdc(), m_hRC);
			LoadGridSurface(pGrdRel, &m_srfRel, CTBL_REL);
			wglMakeCurrent(NULL, NULL);
		}
		b = TRUE;
	}

	bTmp = m_srfFld.IsSolid();
	if( pgFld->m_bFldSolid != bTmp )	b = TRUE;
	m_srfFld.SetSolid(pgFld->m_bFldSolid);
	bTmp = m_srfFld.IsVisible();
	if( pgFld->m_bFldShow != bTmp )	b = TRUE;
	m_srfFld.Show(pgFld->m_bFldShow);
	int nFld;
	CString strFldSel = pgFld->m_strFldSel;
	m_pDoc->GetGrid(_T(strFldSel), nFld);
	if( nFld != m_nFld )	b = TRUE;
	m_nFld = nFld;
	if( pgFld->m_fFldAlpha != m_srfFld.GetAlpha()) b=TRUE;
	if( (pgFld->m_nFldPlacement+1) != m_srfFld.GetPlacement() ) {
		m_srfFld.SetPlacement(pgFld->m_nFldPlacement+1); 
		b=TRUE;
	}
	if( b/*m_srfFld.GetAlpha() != pgFld->m_fFldAlpha */)  {
		m_srfFld.SetAlpha(pgFld->m_fFldAlpha);
		// get field
		CGrid* pGrd =  m_pDoc->GetGrid(m_nFld);
		if(pGrd || bUpdateFld) {
			//pGrd->CreateColorTableGL(m_srfFld.m_tex1D, m_srfFld.m_nTexWidth, m_srfFld.m_nTexFormat, CTBL_REL, m_srfFld.GetAlpha());
			int nClr = CTBL_FLD;
			if( (m_nFld >= m_pDoc->GetComponentsMod()) && (m_nFld < m_pDoc->GetComponentsDif()) ) {
				nClr = CTBL_DIF;
			}
			wglMakeCurrent(m_pDC->GetSafeHdc(), m_hRC);
			LoadGridSurface(pGrd, &m_srfFld, nClr, m_dFldMinY, m_dFldMaxY, m_bFldOnRel);
			wglMakeCurrent(NULL, NULL);
			b = TRUE;
		}
		bUpdateFld = TRUE;
		b = TRUE;
	}


	if(b) Invalidate( FALSE );
}

void CViewGL3D::OnUpdate(CView* /*pSender*/, LPARAM lHint, CObject* /*pHint*/)
{
	if( !m_mod ) return;

	switch( lHint ) {
		case UPDATE_MOVE_PROFILE:
			Invalidate(FALSE);
			break;
		case UPDATE_PROPERTIES_REDRAW:
		//	break;
		case	UPDATE_BITMAP_DELETE:
			m_bUpdateBitmaps = TRUE;
			Invalidate(FALSE);
			break;
		case	UPDATE_BITMAP_NEW:
			m_bUpdateBitmaps = TRUE;
			Invalidate(FALSE);
			break;
		case	UPDATE_BITMAP_CAHNGED:
			m_bUpdateBitmaps = TRUE;
			Invalidate(FALSE);
			break;
		case UPDATE_MOVE_EDGE: 
		//	break;								  
		case UPDATE_LEADHRZ_DELETE: 
			wglMakeCurrent(m_pDC->GetSafeHdc(), m_hRC);
			LoadLeadingHorizons();
			wglMakeCurrent(NULL, NULL);
			Invalidate(FALSE);
			break;								  
		case UPDATE_LEADHRZ_NEW: 
			wglMakeCurrent(m_pDC->GetSafeHdc(), m_hRC);
			LoadLeadingHorizons();
			wglMakeCurrent(NULL, NULL);
			Invalidate(FALSE);
			break;								  
		case UPDATE_LEADHRZ_CAHNGED: 
			wglMakeCurrent(m_pDC->GetSafeHdc(), m_hRC);
			LoadLeadingHorizons();
			wglMakeCurrent(NULL, NULL);
			Invalidate(FALSE);
			break;								  
		case UPDATE_OBJECT_MANAGER_SHOW: 
			UpdateTexturesShow();
			Invalidate();
			break;								  
		case UPDATE_NEW_MODEL: 
			OnInitialUpdate();
			break;
		case UPDATE_COMPUTATION_FINISHED:
			{
				wglMakeCurrent(m_pDC->GetSafeHdc(), m_hRC);
					// get field
					CGrid* pGrd =  m_pDoc->GetGrid(m_nFld);
					if( pGrd ) {
						double min, max;
						pGrd->GetHistogramMinMax(min, max);
						m_srfFld.SetDataRange(min, max);
						pGrd->CreateColorTableGL(m_srfFld.m_tex1D, m_srfFld.m_nTexWidth, m_srfFld.m_nTexFormat, CTBL_FLD);
						m_srfFld.UpdateTexture();
					}
				wglMakeCurrent(NULL, NULL);
				Invalidate(FALSE);
			}
			break;
		case UPDATE_PROPERTIES_GRID_COLOR: 
			{
				wglMakeCurrent(m_pDC->GetSafeHdc(), m_hRC);
					// get field
					CGrid* pGrd =  m_pDoc->GetGrid(m_nFld);
					if( pGrd ) {
						double min, max;
						pGrd->GetHistogramMinMax(min, max);
						m_srfFld.SetDataRange(min, max);
						pGrd->CreateColorTableGL(m_srfFld.m_tex1D, m_srfFld.m_nTexWidth, m_srfFld.m_nTexFormat, CTBL_FLD);
						m_srfFld.UpdateTexture();
					}

					pGrd =  m_pDoc->GetReliefGrid();
					if( pGrd ) {
						if( !pGrd->IsPlanar() ) {
							int nClr = CTBL_REL;
							double min, max;
							pGrd->GetHistogramMinMax(min, max);
							m_srfRel.SetDataRange(min, max);
							pGrd->CreateColorTableGL(m_srfRel.m_tex1D, m_srfRel.m_nTexWidth, m_srfRel.m_nTexFormat, CTBL_REL);
							m_srfRel.UpdateTexture();
						}
					}

				wglMakeCurrent(NULL, NULL);
				Invalidate(FALSE);
			}
			break;								  
		case UPDATE_PROPERTIES_BODY_REDRAW:
			Invalidate(FALSE);
		default: 
			Invalidate( FALSE );
	}
}

BOOL CViewGL3D::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	if( m_nProjection == PRJ_ORTHO ) {
		if( zDelta > 0 ) m_dCameraZ -= m_mZ;
		if( zDelta < 0 ) m_dCameraZ += m_mZ;
	}
	if( m_nProjection == PRJ_ORTHO ) {
		m_dOrthoScale *= 1 + 0.01*abs(zDelta)/zDelta;
	}

	Invalidate( FALSE );
	return TRUE;
	//return CView::OnMouseWheel(nFlags, zDelta, pt);
}

void CViewGL3D::GetScaleShift(float& tx, float& ty, float& tz, float& sc)
{
	double lx, ly, lz;

	// Center transformation; coordinates in GL coordinate system !!!
	lx =  m_mod->GetLX();
	ly =  m_mod->GetLZ();
	lz = -m_mod->GetLY();
	tx = (float) (-m_mod->GetX0() - (lx/2) );
	ty = (float) (-m_mod->GetZ0() - (ly/2) );
	tz = (float) (m_mod->GetY0() - (lz/2) );
	// scaling to canonical volume
	sc = (float) (1.0/max( max(lx, ly), lz) );
}



void CViewGL3D::DrawTest(void)
{
	glDisable(GL_LIGHTING);
	glDisable(GL_BLEND);
	glPointSize(2.0f);
	glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
		
	float colorText[4] = {0.0f, 0.0f, 0.0f, 1.0f};		// front facet as seen from outside the body
	double x = m_minXl;
	CString str;
	while(x < m_maxX) {
		glColor3f(0.0f, 0.0f, 0.0f);
		glBegin(GL_POINT);
			glVertex3d(x, m_minY, m_maxZ);
		glEnd();
		glPushMatrix();
			glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, colorText);
			glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, colorText);
			glTranslated(x, m_minY, m_maxZ);
			glRotated(-60.0f, 0.0f, 1.0f, 0.0f);
			glRotated(-60.0f, 1.0f, 0.0f, 0.0f);
			glScaled((m_maxX-m_minX)/20, (m_maxY-m_minY)/10, 0.0f);
			CString strFormat;
			CString strDec;
			
			strDec.Format("%d", m_decXl);
			strFormat = "%." + strDec + "f";
			str.Format(strFormat, x);
			Text3D(str);								// Print GL Text To The Screen
		glPopMatrix();
		// 2D text
		glRasterPos3d(x, m_minY, m_maxZ);
		Text2D(str);							// Print GL Text To The Screen

		x +=m_stepXl;
	}

	double y = m_minYl;
	while(y < m_maxY) {
		glColor3f(0.0f, 0.0f, 0.0f);
		glBegin(GL_POINT);
			glVertex3d(m_minX, y, m_maxZ);
		glEnd();
		glPushMatrix();
			glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, colorText);
			glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, colorText);
			glTranslated(m_minX, y, m_maxZ);
			glRotated(-135.0f, 0.0f, 1.0f, 0.0f);
			glRotated(0.0f, 1.0f, 0.0f, 0.0f);
			glScaled((m_maxX-m_minX)/20, (m_maxY-m_minY)/10, 0.0f);
			CString strFormat;
			CString strDec;
			
			strDec.Format("%d", m_decXl);
			strFormat = "%." + strDec + "f";
			str.Format(strFormat, y);
			Text3D(str);								// Print GL Text To The Screen
		glPopMatrix();
		// 2D text
		glRasterPos3d(m_minX, y, m_maxZ);
		Text2D(str);							// Print GL Text To The Screen

		y +=m_stepYl;
	}

	double z = m_minZl;
	while(z < m_maxZ) {
		glColor3f(0.0f, 0.0f, 0.0f);
		glBegin(GL_POINT);
			glVertex3d(m_minX, m_minY, z);
		glEnd();
		glPushMatrix();
			glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, colorText);
			glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, colorText);
			glTranslated(m_minX, m_minY, z);
			glRotated(-60.0f, 0.0f, 1.0f, 0.0f);
			glRotated(-60.0f, 1.0f, 0.0f, 0.0f);
			glScaled((m_maxX-m_minX)/20, (m_maxY-m_minY)/10, 0.0f);
			CString strFormat;
			CString strDec;
			
			strDec.Format("%d", m_decZl);
			strFormat = "%." + strDec + "f";
			str.Format(strFormat, z);
			Text3D(str);								// Print GL Text To The Screen
		glPopMatrix();
		// 2D text
		glRasterPos3d(m_minX, m_minY, z);
		Text2D(str);							// Print GL Text To The Screen

		z +=m_stepZl;
	}
}

// loads images from document gl display list
void CViewGL3D::LoadTextures(void)
{
	BOOL bNullGL=FALSE;
	if( wglGetCurrentContext()==NULL) {
		wglMakeCurrent(m_pDC->GetSafeHdc(), m_hRC);
		SetupLights();
		bNullGL = TRUE;
	}

	ASSERT(wglGetCurrentContext() != NULL);

	CMyObject*		pObj;
	MyObPtrArray*	pObjAr;
	pObjAr = m_pDoc->GetObjArray();

	for(int i=0; i<m_objArray.GetCount(); i++) {
		CMyObject* pO = m_objArray.GetAt(i);
		m_objArray.RemoveAt(i);
	}
	if(pObjAr) {
		for(int i=0; i<pObjAr->GetSize(); i++) {
			pObj = m_pDoc->GetObjArray()->GetAt( i );
			if( pObj->IsKindOf( RUNTIME_CLASS( CImgReferenced ) ) ) {
				CImgReferenced* pIm = (CImgReferenced*) pObj;
				if( pIm->GetType()==IMG_PRF && !pIm->m_img.IsNull() ) {
					CGlBitmap*	pGlBmp;
					pGlBmp = new CGlBitmap;
					pGlBmp->Show(pIm->IsVisible());
					pGlBmp->SetSourceID(pIm->GetID());
					m_objArray.Add(pGlBmp);

					// get bounding rectangle
					pGlBmp->SetBoundRect((float) (pIm->m_xs), (float) (pIm->m_zs), (float) (-pIm->m_ys), (float) (pIm->m_xe), (float) (pIm->m_ze), (float) (-pIm->m_ye) );
					pGlBmp->CompileDisplayList(*pIm);
				}
				if( pIm->GetType()==IMG_MAP && !pIm->m_img.IsNull() ) {
					CGlBitmap*	pGlBmp;
					pGlBmp = new CGlBitmap;
					pGlBmp->Show(pIm->IsVisible());
					pGlBmp->SetSourceID(pIm->GetID());
					pGlBmp->SetType(CGlBitmap::glBmp_Map);
					pGlBmp->CreateTexture2D(*pIm);
					m_objArray.Add(pGlBmp);
				}
			}
		}
	}

	m_bUpdateBitmaps = FALSE;
	if(bNullGL) wglMakeCurrent(NULL, NULL);
}

void CViewGL3D::UpdateTexturesShow(void)
{
	CMyObject*		pObj;
	MyObPtrArray*	pObjAr;
	pObjAr = m_pDoc->GetObjArray();
	if(pObjAr) {
		for(int i=0; i<pObjAr->GetSize(); i++) {
			pObj = m_pDoc->GetObjArray()->GetAt( i );
			if( pObj->IsKindOf( RUNTIME_CLASS( CImgReferenced ) )  ) {
				CImgReferenced* pIm = (CImgReferenced*) pObj;
				CString strId = pIm->GetID();
				if( !m_objArray.IsEmpty() ) {
					for(int i=0; i<m_objArray.GetCount(); i++) {
						CGlBitmap* pGlBmp;
						pGlBmp = (CGlBitmap*) m_objArray.GetAt(i);
						if(pGlBmp->GetSourceID() == strId) {
							pGlBmp->Show( pIm->IsVisible() );
						}
					}
				}
			}
			if( pObj->IsKindOf( RUNTIME_CLASS( CGridObj ) ) ) {
				CGridObj* pGo = (CGridObj*) pObj;
				CString strId = pGo->GetID();
				if( !m_objHrz.IsEmpty() ) {
					for(int i=0; i<m_objHrz.GetCount(); i++) {
						CGlSurface* pSrf;
						pSrf = (CGlSurface*) m_objHrz.GetAt(i);
						if(pSrf->GetSourceID() == strId) {
							pSrf->Show( pGo->IsVisible() );
						}
					}
				}
			}
		}
	}
}

// // loads grid from document to this view structures
void CViewGL3D::LoadGridSurface(CGrid* pGrd, CGlSurface* pSrf, int nClrType, double min, double max, BOOL bRelief)
{
	ASSERT(wglGetCurrentContext() != NULL);

	if( pGrd->IsEmpty() || pGrd->IsPlanar() ) 	return;

	pSrf->SetSize(pGrd->GetRows(), pGrd->GetCols());
	pGrd->CreateColorTableGL(pSrf->m_tex1D, pSrf->m_nTexWidth, pSrf->m_nTexFormat, nClrType);
	pSrf->UpdateTexture();
	BOOL bOK=FALSE;
	if( bRelief ) {
		CGrid* pRel=m_pDoc->GetReliefGrid();
		bOK = pGrd->GetGlRel(&pSrf->m_srfData, pSrf->m_nSrfFormat, min, max, pRel);
	}
	if(!bRelief || !bOK) {
		pGrd->GetGL(&pSrf->m_srfData, pSrf->m_nSrfFormat, min, max);
	}

	//double minH, maxH,
	pGrd->GetHistogramMinMax(min, max);
	pSrf->SetDataRange(min, max);
	//pSrf->CompileDisplayList(min, max, bRelief);

}




/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//****** GL Text ************************************************************************************************************************

GLvoid	CViewGL3D::PrepareCharset2D(const LOGFONT* pLf)
{
	// debug checks
	ASSERT(pLf!=NULL);

	// ask for the font
	CFont	font;
	CFont*	pDefFont;
	if(!font.CreateFontIndirect(pLf))
	{
		TRACE("CGLEnabledView::PrepareCharset2D:\n\tUnable to create a font from the passed logical font descriptor.\n");
		return;
	}
	//select the font into the DC
	pDefFont = m_pDC->SelectObject(&font);
	// create or recreate the display lists
	if(m_charsetDListBase2D>0) 
		glDeleteLists(m_charsetDListBase2D, m_nChars);	
	if (0 == (m_charsetDListBase2D=glGenLists(m_nChars)))
	{
		TRACE("CGLEnabledView::PrepareCharset2D:\n\tUnable to create the charset (no memory for display lists).\n");
		goto end;
	};
	// build the charset display lists
	if(!wglUseFontBitmaps(m_pDC->GetSafeHdc(), 0, m_nChars-1, m_charsetDListBase2D))
	{
		TRACE("CGLEnabledView::PrepareCharset2D:\n\tUnable to create the charset (internal error).\n");
		glDeleteLists(m_charsetDListBase2D, m_nChars);
		m_charsetDListBase2D=0;
	};
	// deselect and dispose of the GDI font
end:
	m_pDC->SelectObject(pDefFont);
	font.DeleteObject();
};

GLvoid	CViewGL3D::Text2D(CString str)
{
	int textlen=0;

	if(m_charsetDListBase2D!=0)
	{
		if( (textlen=str.GetLength()) >0)
		{
			// output the outlines corresponding to the requested text srting
			glListBase(m_charsetDListBase2D);
			glCallLists(textlen, GL_UNSIGNED_BYTE, LPCTSTR(str));
		}
	}
	else TRACE("CGLEnabledView::Text2D:\n\tNo charset available. Use PrepareCharset2D routines first.\n");

};

void CViewGL3D::PrepareCharset3D(const LOGFONT* pLF, float extrusion, BOOL uselines, float precision)
{
	// debug checks
	ASSERT(extrusion>=0.f && precision>=0.f);
	ASSERT(pLF!=NULL);
	// variables initialization
	int mode=uselines ? WGL_FONT_LINES : WGL_FONT_POLYGONS;
	if(m_gmfvector==NULL) 
		m_gmfvector=new GLYPHMETRICSFLOAT[m_nChars];
	// ask for the font
	CFont fnt;
	if(!fnt.CreateFontIndirect(pLF))
	{
		TRACE("CGLEnabledView::PrepareCharset3D:\n\tUnable to create a font from the passed logical font descriptor.\n");
		return;
	}
	//select the font into the DC
	CFont* pDefFont = m_pDC->SelectObject(&fnt);
	// create or recreate the display lists
	if(m_charsetDListBase3D>0) 
		glDeleteLists(m_charsetDListBase3D, m_nChars);	
	if (0 == (m_charsetDListBase3D=glGenLists(m_nChars)))
	{
		TRACE("CGLEnabledView::PrepareCharset3D:\n\tUnable to create the charset (no memory for display lists).\n");
		goto end;
	};
	// build the charset display lists
	if(!wglUseFontOutlines(m_pDC->GetSafeHdc(), 
		0, 
		m_nChars-1,					// number of display lists
		m_charsetDListBase3D,		// display lists base
		precision,					// deviation from true outlines
		extrusion,					// font thickness in z direection
		mode,						// mode - polygons or lines
		m_gmfvector))				// buffer to recieve data
	{
		TRACE("CGLEnabledView::PrepareCharset3D:\n\tUnable to create the charset (internal error).\n");
		glDeleteLists(m_charsetDListBase3D, m_nChars);
		m_charsetDListBase3D=0;
	};
	// deselect and dispose of the GDI font
end:
	m_pDC->SelectObject(pDefFont);
	fnt.DeleteObject();
}

// dir is direstion write text from begining to end dir == 1
//	reverse text dir == -1
float CViewGL3D::Text3D(CString str)
{
	float	retLen=0.0f;
	int		txtLen=0;

	if(m_charsetDListBase3D!=0)
	{
		if( (txtLen=str.GetLength()) >0) {	
			// output the outlines corresponding to the requested text srting
			glListBase(m_charsetDListBase3D);
			glCallLists(txtLen, GL_UNSIGNED_BYTE, LPCTSTR(str));
			// calculate and return the length of the produced outlines
			for(int c=0; c<txtLen; c++) {
				retLen += m_gmfvector[char(str[c])].gmfCellIncX;
			}
		}
	}
	else TRACE("CGLEnabledView::Text3D:\n\tNo charset available. Use PrepareCharset3D routines first.\n");
	return retLen;
}

void FindAxisMinMaxStep(double& min, double& max, double& step, double& start, int& digM, int& digN, int n)
{
	// quit if automatic if dasabled
	double a, b, end, dec;

	a = max - min;
	if(a == 0) return;
	b = 1.0;
	if(a > 1) {
		b = 1.0;
		while(a > 10) {
			a /= 10;
			b *= 10;
		}
	}
	else {
		b = 1.0;
		while(a <= 1) {
			a *= 10;
			b /= 10;
		}
	}
	start = floor(min/b) * b;
	end = ceil(max/b) * b;
	a = (end - start) / n;
	b = 1.0;
	if(a > 1) {
		while(a >= 10) {
			a /= 10;
			b *= 10;
		}
	}
	else {
		while(a <= 1) {
			a *= 10;
			b /= 10;
		}
	}
	step = floor(((end - start) /n)/b) * b;

	dec = log10(step/20);
	digM = 0;
	digN = 0;
	if( dec < 0 ) {
		dec = ceil( fabs(dec) )-1;
		digM = (int)dec;
		digN = (int)dec+1;
	}
}

void CViewGL3D::DrawLegend(int x, int y, int w, int h, double minData, double maxData, int nTex, CString strName)
{
	if(!glIsTexture(nTex)) return;

 	glFlush();
	
	glViewport(x, y, w, h);
	glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0, 1, 0, 1,-5, 5);
	glMatrixMode(GL_MODELVIEW);

	glPushMatrix();
		glLoadIdentity();

		// texture is OK continue drawing
		glDepthFunc(GL_LEQUAL);
		glEnable(GL_TEXTURE_1D);
		glBindTexture(GL_TEXTURE_1D, nTex);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);

		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);;
		glDisable(GL_LIGHTING);
		glPointSize(1);
		glLineWidth( 1.0 );
		glBegin(GL_QUADS);
			glColor4d(1.0, 1.0, 1.0, 1.0);
			glTexCoord1d(0);
			glVertex3d(0, 0.2, 0);

			glTexCoord1d(0);
			glVertex3d(0.1, 0.2, 0);

			glTexCoord1d(1);
			glVertex3d(0.1, 1, 0);

			glTexCoord1d(1);
			glVertex3d(0, 1, 0);
		glEnd();
		glDisable(GL_TEXTURE_1D);

		glColor4d(0.0, 0.0, 0.1, 1.0);
		glRasterPos3d(0.01, 0.1, 0);
		Text2D(strName);	

		double min, max, maxFld, step, start, sc, of;
		int nTicks = 6;
		int decM, decN;
		
		min = minData;
		
		max = maxData;
		maxFld = max;
		sc = 0.8/(max-min);
		of = 0.2-sc*min;
		FindAxisMinMaxStep(min, max, step, start, decM, decN, 4);


		glDisable(GL_LIGHTING);
		glDisable(GL_BLEND);
		glPointSize(2.0f);
		glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
			
		CString str;
		CString strFormat;
		CString strDec;
		float colorText[4] = {0.0f, 0.0f, 0.0f, 1.0f};		// front facet as seen from outside the body

		double yv, yn, ytxt;
		yv = start;
		yn = start;
		while(yv < max) {
			if(yv>=min) {
				strDec.Format("%d", decM);
				strFormat = " %." + strDec + "f";
				str.Format(strFormat, yv);
				ytxt = of+sc*yv;
				glBegin(GL_LINES);
					glColor4d(0.0, 0.0, 0.1, 1.0);
					glVertex3d(0.1, ytxt, 0.1);
					glVertex3d(0.13, ytxt, 0.1);
				glEnd();

				// 2D text
				glRasterPos3d(0.14, ytxt, 0);
				Text2D(str);	
			}
			yn = yv+step/2.0;
			if(yn>=min) {
				strDec.Format("%d", decN);
				strFormat = " %." + strDec + "f";
				str.Format(strFormat, yn);
				ytxt = of+sc*yn;
				glBegin(GL_LINES);
					glColor4d(0.0, 0.0, 0.1, 1.0);
					glVertex3d(0.1, ytxt, 0.1);
					glVertex3d(0.11, ytxt, 0.1);
				glEnd();

				// 2D text
				glRasterPos3d(0.12, ytxt, 0);
				Text2D(str);	
			}
			yv += step;
		}
	glPopMatrix();
	glDepthFunc(GL_LESS);

 	glFlush();
}

void CViewGL3D::DrawLegend(void)
{
	CRect clRc;
	GetClientRect(&clRc);
	int h = (clRc.Height()-60)/2;
	if( h<100 || clRc.Width()<121) return;
	if(h>200) h=200;
 	
	double minDt, maxDt;
	if( m_srfFld.IsVisible() ) {
		m_srfFld.SetName(m_pDoc->GetFieldString(m_nFld));
		m_srfFld.GetDataRange(minDt, maxDt);
		DrawLegend(0, clRc.Height()-h, 120, h, minDt, maxDt, m_srfFld.GetTexture(), m_srfFld.GetName());
	}

	if( m_srfRel.IsVisible() ) {
		m_srfRel.GetDataRange(minDt, maxDt);
		DrawLegend(0, clRc.Height()-2*h, 120, h, minDt, maxDt, m_srfRel.GetTexture(), "h [m]");
	}


	return;
}

	// draw orientation overview in plane simulator mode
void CViewGL3D::DrawOrientation(void)
{
	CRect clRc;
	GetClientRect(&clRc);
 	glFlush();

	glViewport(clRc.right-100, 0, 100, 100);
	glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		GLdouble aspect = 1.0f;
		double wm, hm;			// model width
		wm = hm = m_diag*0.6;	
		wm/hm < aspect ? wm=aspect*hm : hm=wm/aspect;
		wm *= m_dOrthoScale;
		hm *= m_dOrthoScale;
		glOrtho(-wm, +wm, -hm, +hm, -m_diag, m_diag);
	glMatrixMode(GL_MODELVIEW);

	glPushMatrix();
		glLoadIdentity();
		glRotatef(90, 1.0f, 0.0f, 0.0f);
		glRotatef(0, 0.0f, 1.0f, 0.0f);
		glRotatef(0, 0.0f, 0.0f, 1.0f);
		glTranslated(-m_midX, -m_midY, -m_midZ );
		glDisable(GL_LIGHTING);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glColor3d(0.8, 0.8, 0.8);
		glBegin(GL_QUADS);
			glVertex3d(m_minX, m_midY, m_minZ);
			glVertex3d(m_maxX, m_midY, m_minZ);
			glVertex3d(m_maxX, m_midY, m_maxZ);
			glVertex3d(m_minX, m_midY, m_maxZ);
		glEnd();
		glPointSize(4);
		glColor3d(0.8, 0, 0);
		glBegin(GL_POINTS);
			glVertex3d(m_plX, m_plY, m_plZ);
		glEnd();
		glColor3d(0, 0.6, 0);
		glBegin(GL_LINES);
			glVertex3d(m_plX, m_plY, m_plZ);
			double plX = m_plX+sin(DegToRad(m_plRotY)) * m_diag/5*cos(DegToRad(m_plRotV));
			double plY = m_plY-cos(DegToRad(m_plRotX)) * m_diag/5;
			double plZ = m_plZ-cos(DegToRad(m_plRotY)) * m_diag/5*cos(DegToRad(m_plRotV));
			glVertex3d(plX, plY, plZ);
		glEnd();
	glPopMatrix();
}

// draws small coordinatte sysem
void CViewGL3D::DrawCoordSys(void)
{
	CRect rc;
	GetClientRect( &rc);

	if( rc.Height()<100 || rc.Width()<100 )
		return;

 	glFlush();
	// coord. sys
	glViewport(0, 0, 60, 60);
	glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(-1.74, 1.74, -1.74, 1.74, 0.01, 5);
	glMatrixMode(GL_MODELVIEW);

	glPushMatrix();
		glLoadIdentity();
		
		glTranslatef(0.0f, 0.0f, -2.2f);
		if(m_bPlaneSimulator) {
			glRotated(m_plRotX, 1.0f, 0.0f, 0.0f);
			glRotated(m_plRotY+180, 0.0f, 1.0f, 0.0f);
			glRotated(m_plRotZ, 0.0f, 0.0f, 1.0f);
		}
		else {
			glRotatef(m_fRotX, 1.0f, 0.0f, 0.0f);
			glRotatef(m_fRotY, 0.0f, 1.0f, 0.0f);
			glRotatef(m_fRotZ, 0.0f, 0.0f, 1.0f);
		}
		GLdouble minX=-0.6f, maxX=0.6f, 
				minY=-0.6f, maxY=0.6f, 
				minZ=-0.6f, maxZ=0.6f;

		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);;
		glDisable(GL_LIGHTING);
		glPointSize(2);
		glColor3f(0.5f, 0.5f, 0.5f);

		glLineWidth( 1.0 );
		glColor3f(0.5f, 0.5f, 0.5f);
		GLdouble a = 0.4f, c = 0.15f;
 		glBegin(GL_LINES);
			// x gl-axis
 			glColor3f(0.0f, 1.0f, 0.0f);
			glVertex3d(minX-a, minY, minZ);
			glVertex3d(maxX, minY, minZ);

			// y gl-axis
			glColor3f(0.0f, 0.0f, 1.0f);
			glVertex3d(maxX, minY, minZ);
			glVertex3d(maxX, minY, maxZ+a);

			// z gl-axis
			glColor3f(1.0f, 0.0f, 0.0f);
			glVertex3d(maxX, minY, minZ);
			glVertex3d(maxX, maxY+a, minZ);
		glEnd();
		glEnable(GL_LIGHTING);

		GLUquadricObj*	pCone;
		float *pColor = new float[4];
		pCone = gluNewQuadric();

		// Cone for z-gl-axis
		pColor[0] = 0.0f;
		pColor[1] = 0.0f;
		pColor[2] = 1.0f;
		pColor[3] = 1.0f;
		
		glColor4fv(pColor);
		glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, pColor);
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, pColor);
		glPushMatrix(); 
			glTranslated(maxX, minY , maxZ + a);
			gluCylinder(pCone, c, 0.0f, a, 6, 4);
			gluQuadricNormals(pCone, GLU_SMOOTH);
		glPopMatrix();

		// Cone for y-gl-axis
		pColor[0] = 1.0f;
		pColor[1] = 0.0f;
		pColor[2] = 0.0f;
		pColor[3] = 1.0f;
		glColor4fv(pColor);
		glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, pColor);
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, pColor);
		glPushMatrix(); 
			glTranslated(maxX, maxY+a, minZ );
			glRotated(-90.0f ,1.0f, 0.0f, 0.0f ); 
			gluCylinder(pCone, c, 0.0f, a, 8, 8);
			gluQuadricNormals(pCone, GLU_SMOOTH);
		glPopMatrix();

		// Cone for x-gl-axis
		pColor[0] = 0.0f;
		pColor[1] = 1.0f;
		pColor[2] = 0.0f;
		pColor[3] = 1.0f;
		glColor4fv(pColor);
		glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, pColor);
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, pColor);
		glPushMatrix(); 
			glTranslated(minX-a, minY, minZ );
			glRotated(-90.0f ,0.0f, 1.0f, 0.0f ); 
			gluCylinder(pCone, c, 0.0f, a, 8, 8);
			gluQuadricNormals(pCone, GLU_SMOOTH);
		glPopMatrix();

		delete pColor;
		glColor3f(1.0f, 1.0f, 1.0f);
	glPopMatrix();

	glFlush();
}

void CViewGL3D::InitTextLabels(void)
{
	m_minXl = m_minX;
	m_minYl = m_minY;
	m_minZl = m_minZ;
	FindLabels(m_minXl, m_maxX, m_stepXl, m_decXl);
	FindLabels(m_minYl, m_maxY, m_stepYl, m_decYl);
	FindLabels(m_minZl, m_maxZ, m_stepZl, m_decZl);
}

void CViewGL3D::FindLabels(GLdouble& min, GLdouble max, GLdouble& step, GLuint& dec)
{
	int		n=6;
	double	a, b, s;
	double	minT, maxT;
	dec = 0;

	minT = min;
	maxT = max;

	s = a = (max - min)/n;
	if(a == 0) return;
	b = 1.0;
	if(a > 1) {
		while(a > 10) {
			a /= 10;
			b *= 10;
		}
	}
	else {
		while(a <= 1) {
			a *= 10;
			b /= 10;
		}
	}
	step = s = floor(s/b) * b;
	min = ceil(minT/s)*s;
	max = floor(maxT/s)*s;

	double d = log10(step/20);
	if( d < 0 ) {
		dec = (int) ceil( fabs(d) )-1;
	}
}

// draws axis labels 2D or 3D text
void CViewGL3D::DrawAxisLabels(void)
{
	if( m_nTextType == GLTXT_NONE ) return;

	glDisable(GL_LIGHTING);
	glDisable(GL_BLEND);
	glPointSize(2.0f);
	glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
		
	CString str;
	CString strFormat;
	CString strDec;
	float colorText[4] = {0.0f, 0.0f, 0.0f, 1.0f};		// front facet as seen from outside the body

	double x, y, z;
	x = m_minXl;
	while(x < m_maxX) {
		strDec.Format("%d", m_decXl);
		strFormat = " %." + strDec + "f";
		str.Format(strFormat, x);
		glColor3f(0.0f, 0.0f, 0.0f);
		glBegin(GL_POINTS);
			glVertex3d(x, m_minY, m_maxZ);
		glEnd();

		// 3D text
		if( m_nTextType == GLTXT_3D ) {
			glPushMatrix();
				glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, colorText);
				glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, colorText);
				glTranslated(x, m_minY, m_maxZ);
				glRotated(-90.0f, 0.0f, 1.0f, 0.0f);
				glRotated(-45.0f, 1.0f, 0.0f, 0.0f);
				glScaled((m_maxX-m_minX)/20, (m_maxY-m_minY)/10, 0.0f);
				Text3D(str);								// Print GL Text To The Screen
			glPopMatrix();
		}

		// 2D text
		if( m_nTextType == GLTXT_2D ) {
			glRasterPos3d(x, m_minY, m_maxZ);
			Text2D(str);	
		}// Print GL Text To The Screen

		x += m_stepXl;
	}

	// y GL coordinate
	y = m_minYl;
	while(y < m_maxY) {
		strDec.Format("%d", m_decXl);
		strFormat = " %." + strDec + "f";
		str.Format(strFormat, y);
		glColor3f(0.0f, 0.0f, 0.0f);
		glBegin(GL_POINTS);
			glVertex3d(m_minX, y, m_maxZ);
		glEnd();
		// 3D text
		if( m_nTextType == GLTXT_3D ) {
			glPushMatrix();
				glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, colorText);
				glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, colorText);
				glTranslated(m_minX, y, m_maxZ);
				glRotated(-135.0f, 0.0f, 1.0f, 0.0f);
				glRotated(0.0f, 1.0f, 0.0f, 0.0f);
				glScaled((m_maxX-m_minX)/20, (m_maxY-m_minY)/10, 0.0f);
				Text3D(str);								// Print GL Text To The Screen
			glPopMatrix();
		}

		// 2D text
		if( m_nTextType == GLTXT_2D ) {
			glRasterPos3d(m_minX, y, m_maxZ);
			Text2D(str);							// Print GL Text To The Screen
		}

		y +=m_stepYl;
	}

	// z GL coordinate
	z = m_minZl;
	while(z < m_maxZ) {
		strDec.Format("%d", m_decZl);
		strFormat = " %." + strDec + "f"; 
		str.Format(strFormat, -z);
		glColor3f(0.0f, 0.0f, 0.0f);
		glBegin(GL_POINTS);
			glVertex3d(m_minX, m_minY, z);
		glEnd();

		// 3D text
		if( m_nTextType == GLTXT_3D ) {
			float fLen=0.0f;
			int txtLen;
			if( (txtLen=str.GetLength()) >0) {
				// calculate and return the length of the produced outlines
				for(int c=0; c<txtLen; c++) {
					fLen += m_gmfvector[char(str[c])].gmfCellIncX;
				}
			}
			glPushMatrix();
				glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, colorText);
				glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, colorText);
				glTranslated(m_minX-fLen*(m_maxX-m_minX)/20, m_minY, z);
				glRotated(0.0f, 0.0f, 1.0f, 0.0f);
				glRotated(-45.0f, 1.0f, 0.0f, 0.0f);
				glScaled((m_maxX-m_minX)/20, (m_maxY-m_minY)/10, 0.0f);
				Text3D(str);								// Print GL Text To The Screen
			glPopMatrix();
		}

		// 2D text
		if( m_nTextType == GLTXT_2D ) {
			glRasterPos3d(m_minX, m_minY, z);
			Text2D(str);							// Print GL Text To The Screen
		}
		z +=m_stepZl;
	}
}

void CViewGL3D::DrawGridLines(void)
{
	glDisable(GL_LIGHTING);
	glDisable(GL_BLEND);
	glDepthFunc(GL_LEQUAL);
	glColor3f(0.8f, 0.8f, 0.8f);

	double x, z;
		
	if(m_bGrdLineDwnX || m_bGrdLineUpX) {
		x = m_minXl;
		if( x== m_minX) x += m_stepXl;
		while(x <= m_maxX) {
			if(m_bGrdLineDwnX) {
				glBegin(GL_LINES);
					glVertex3d(x, m_minY, m_minZ);
					glVertex3d(x, m_minY, m_maxZ);
				glEnd();
			}
			if(m_bGrdLineUpX) {
				glBegin(GL_LINES);
					glVertex3d(x, m_maxY, m_minZ);
					glVertex3d(x, m_maxY, m_maxZ);
				glEnd();
			}
			x += m_stepXl;
		}
	}

	if(m_bGrdLineDwnZ || m_bGrdLineUpZ) {
		z = m_minZl;
		while(z <= m_maxZ) {
			if(m_bGrdLineDwnZ) {
				glBegin(GL_LINES);
					glVertex3d(m_minX, m_minY, z);
					glVertex3d(m_maxX, m_minY, z);
				glEnd();
			}
			if(m_bGrdLineUpZ) {
				glBegin(GL_LINES);
					glVertex3d(m_minX, m_maxY, z);
					glVertex3d(m_maxX, m_maxY, z);
				glEnd();
			}
			z += m_stepZl;
		}
	}

	if(m_bGrdLineVrt) {
		x = m_minXl;
		while(x <= m_maxX) {
			z = m_minZl;
			while(z <= m_maxZ) {
				glBegin(GL_LINES);
					glVertex3d(x, m_minY, z);
					glVertex3d(x, m_maxY, z);
				glEnd();
				z += m_stepZl;
			}
			x += m_stepXl;
		}
		
	}
}

// loads leading horizons from document to GLSurface
void CViewGL3D::LoadLeadingHorizons(void)
{
	ASSERT(wglGetCurrentContext() != NULL);

	CMyObject*		pObj;
	MyObPtrArray*	pObjAr;
	m_objHrz.RemoveAll();
	pObjAr = m_pDoc->GetObjArray();
	if(pObjAr) {
		for(int i=0; i<pObjAr->GetSize(); i++) {
			pObj = m_pDoc->GetObjArray()->GetAt( i );
			if( pObj->IsKindOf( RUNTIME_CLASS( CGridObj ) ) ) {
				CGridObj* pGo = (CGridObj*) pObj;
				CGlSurface*	pSrf;
				pSrf = new CGlSurface;
				pSrf->Show(pGo->IsVisible());
				pSrf->SetSourceID(pGo->GetID());
				m_objHrz.Add(pSrf);

				// generate GL surface
				CGrid* pGrd = pGo->GetGrid();
				if( pGrd->IsEmpty() || pGrd->IsPlanar() ) 	{
					delete pSrf;
					m_objHrz.RemoveAt(m_objHrz.GetUpperBound());
				}
				else {
					pGrd->SetColorTable(CTBL_BW);
					LoadGridSurface(pGrd, pSrf, CTBL_BW);
					//pSrf->SetSize( pGrd->GetRows(), pGrd->GetCols() );
					//pGrd->CreateColorTableGL(pSrf->m_tex1D, pSrf->m_nTexWidth, pSrf->m_nTexFormat, CTBL_BW);
					//pGrd->GetGL(&pSrf->m_srfData, pSrf->m_nSrfFormat);

					//pSrf->CompileDisplayList();
				}
			}
		}
	}
}

void CViewGL3D::DrawLeadingHorizons(void)
{
	if(!m_bHorizon) return;

	glDisable(GL_LIGHTING);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthFunc(GL_LESS);
	if( !m_objHrz.IsEmpty() ) {
		for(int i=0; i<m_objHrz.GetCount(); i++) {
			CGlSurface* pSrf;
			pSrf = (CGlSurface*) m_objHrz.GetAt(i);
			pSrf->SetPlacement(CGlSurface::plcNone);
			pSrf->SetSolid(m_bHorizonSolid);
			pSrf->DrawGL(FALSE);
		}
	}
}




//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//******* field toolbar **********************************************************************************************
void CViewGL3D::OnViewFieldModelGx()						{ CheckModField(GRDMOD_GX);	}
void CViewGL3D::OnUpdateViewFieldModelGx(CCmdUI* pCmdUI)	{ UpdateViewFieldMod(pCmdUI, GRDMOD_GX);}

void CViewGL3D::OnViewFieldModelGy()						{ CheckModField(GRDMOD_GY);	}
void CViewGL3D::OnUpdateViewFieldModelGy(CCmdUI* pCmdUI)	{ UpdateViewFieldMod(pCmdUI, GRDMOD_GY); }

void CViewGL3D::OnViewFieldModelGz()						{ CheckModField(GRDMOD_GZ);	}
void CViewGL3D::OnUpdateViewFieldModelGz(CCmdUI* pCmdUI)	{ UpdateViewFieldMod(pCmdUI, GRDMOD_GZ); }

void CViewGL3D::OnViewFieldModelG()							{ CheckModField(GRDMOD_G);	} 
void CViewGL3D::OnUpdateViewFieldModelG(CCmdUI* pCmdUI)		{ UpdateViewFieldMod(pCmdUI, GRDMOD_G); }

void CViewGL3D::OnViewFieldModelMx()						{ CheckModField(GRDMOD_MX);	} 
void CViewGL3D::OnUpdateViewFieldModelMx(CCmdUI* pCmdUI)	{ UpdateViewFieldMod(pCmdUI, GRDMOD_MX); }

void CViewGL3D::OnViewFieldModelMy()						{ CheckModField(GRDMOD_MY);	} 
void CViewGL3D::OnUpdateViewFieldModelMy(CCmdUI* pCmdUI)	{ UpdateViewFieldMod(pCmdUI, GRDMOD_MY); }

void CViewGL3D::OnViewFieldModelMz()						{ CheckModField(GRDMOD_MZ);	} 
void CViewGL3D::OnUpdateViewFieldModelMz(CCmdUI* pCmdUI)	{ UpdateViewFieldMod(pCmdUI, GRDMOD_MZ); }

void CViewGL3D::OnViewFieldModelM()							{ CheckModField(GRDMOD_M);	} 
void CViewGL3D::OnUpdateViewFieldModelM(CCmdUI* pCmdUI)		{ UpdateViewFieldMod(pCmdUI, GRDMOD_M); }

// gradients
void CViewGL3D::OnModelTxx()								{ CheckModField(GRDMOD_GXX);	} 
void CViewGL3D::OnUpdateModelTxx(CCmdUI *pCmdUI)			{ UpdateViewFieldMod(pCmdUI, GRDMOD_GXX); }

void CViewGL3D::OnModelTxy()								{ CheckModField(GRDMOD_GXY);	} 
void CViewGL3D::OnUpdateModelTxy(CCmdUI *pCmdUI)			{ UpdateViewFieldMod(pCmdUI, GRDMOD_GXY); }

void CViewGL3D::OnModelTxz()								{ CheckModField(GRDMOD_GXZ);	} 
void CViewGL3D::OnUpdateModelTxz(CCmdUI *pCmdUI)			{ UpdateViewFieldMod(pCmdUI, GRDMOD_GXZ); }

void CViewGL3D::OnModelTyy()								{ CheckModField(GRDMOD_GYY);	} 
void CViewGL3D::OnUpdateModelTyy(CCmdUI *pCmdUI)			{ UpdateViewFieldMod(pCmdUI, GRDMOD_GYY); }

void CViewGL3D::OnModelTyz()								{ CheckModField(GRDMOD_GYZ);	} 
void CViewGL3D::OnUpdateModelTyz(CCmdUI *pCmdUI)			{ UpdateViewFieldMod(pCmdUI, GRDMOD_GYZ); }

void CViewGL3D::OnModelTzz()								{ CheckModField(GRDMOD_GZZ);	} 
void CViewGL3D::OnUpdateModelTzz(CCmdUI *pCmdUI)			{ UpdateViewFieldMod(pCmdUI, GRDMOD_GZZ); }

void CViewGL3D::FindFieldIndex(int &n) 
{
	switch( m_nFldTbl ) {
		case FLDTLB_MODELED:				return;
		case FLDTLB_DIFFERENCE: n += 20;	return;
		case FLDTLB_MEASURED:	n += 40;	return;
	}
}

void CViewGL3D::CheckModField(int n)
{
	FindFieldIndex( n );
	ASSERT(n!=-1);

	if( m_pDoc->GetGrid(n)->IsPlanar() ) {
		AfxMessageBox("The grid is planar. Cannot be selected.", MB_OK | MB_ICONINFORMATION);
		return;
	}

	m_nFld = n;
	CGrid* pGrd = m_pDoc->GetGrid(m_nFld);
	if( pGrd ) {
		int nClr = CTBL_FLD;
		if( (m_nFld >= m_pDoc->GetComponentsMod()) && (m_nFld < m_pDoc->GetComponentsDif()) ) {
			nClr = CTBL_DIF;
		}
		wglMakeCurrent(m_pDC->GetSafeHdc(), m_hRC);
			m_srfFld.SetName(m_pDoc->GetFieldString(m_nFld));
			LoadGridSurface(pGrd, &m_srfFld, nClr, m_dFldMinY, m_dFldMaxY, m_bFldOnRel);
		wglMakeCurrent(NULL, NULL);
		Invalidate();
	}
}

void CViewGL3D::UpdateViewFieldMod(CCmdUI* pCmdUI, int n)
{
	FindFieldIndex( n );
	ASSERT(n!=-1);

	CGrid*	pGrd = m_pDoc->GetGrid(n);
	if( pGrd != NULL ) {
		if( pGrd->IsEmpty() || pGrd->IsPlanar() )		{ pCmdUI->Enable(0);}
		else											{ pCmdUI->Enable(1);}

		if( n == m_nFld )								{ pCmdUI->SetCheck(1);}
		else											{ pCmdUI->SetCheck(0);}
	}
	else {
		pCmdUI->Enable(0);
		pCmdUI->SetCheck(0);
	}
}


void CViewGL3D::OnViewMeasuredField()	{	m_nFldTbl = FLDTLB_MEASURED;	}
void CViewGL3D::OnUpdateViewMeasuredField(CCmdUI *pCmdUI)
{
	if( m_nFldTbl == FLDTLB_MEASURED )	pCmdUI->SetCheck(1);
	else								pCmdUI->SetCheck(0);
}

void CViewGL3D::OnViewModeledField()	{	m_nFldTbl = FLDTLB_MODELED;		}
void CViewGL3D::OnUpdateViewModeledField(CCmdUI *pCmdUI)
{
	if( m_nFldTbl == FLDTLB_MODELED )	pCmdUI->SetCheck(1);
	else								pCmdUI->SetCheck(0);
}

void CViewGL3D::OnViewDifferenceField(){	m_nFldTbl = FLDTLB_DIFFERENCE;	}
void CViewGL3D::OnUpdateViewDifferenceField(CCmdUI *pCmdUI)
{
	if( m_nFldTbl == FLDTLB_DIFFERENCE )	pCmdUI->SetCheck(1);
	else									pCmdUI->SetCheck(0);
}
//******* field toolbar **********************************************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////




//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//**** selection *****************************************************************************************************
void CViewGL3D::ProcessHits (GLint hits, GLuint buffer[])
{
	if( hits < 0 ) return;
	CString		str, s;
	unsigned int i, j, k;
	GLuint		 n, names, *ptr;
	//GLuint		row, col, names, *ptr;
	//GLuint		n, id, index, indexZ;
	GLfloat		z1, z2, zOld;					// GL depth values

	TRACE ("hits = %d\n", hits);
	ptr = (GLuint *) buffer;
	for (i = 0; i < (UINT)hits; i++) { /* for each hit */
		names = *ptr;
		//TRACE(" number of names for this hit = %d\n", names);
		ptr++;
		m_dSelZ = z1 = (float) *ptr/0x7fffffff;
		ptr++;
		z2 = (float) *ptr/0x7fffffff;
		ptr++;
		s.Format("%.6f, %.6f; ", z1, z2);
		str += s;
		zOld = z1;
		//TRACE(" z1 is %.6g;", z1); 
		//TRACE(" z2 is %.6g\n", z2);
		//TRACE (" names are ");
		if(i == 0 || z1 > zOld) {
			for (j = 0; j < names; j++) { /* for each name */
				k = j % 6;
				n = *ptr;
				switch(k) {
					case 0:
						m_nSelId = n;
						break;
					case 1:
						m_nSelRow = n;
						break;
					case 2:
						m_nSelCol = n;
						break;
					case 3:
						m_nSelIndex = n;
						break;
					case 4:
						m_nSelIndexZ = n;
						break;
					case 5:
						m_nSelIndexGL = n;
						break;
				}
				//TRACE("%d ", *ptr);
				s.Format("%d ", n);
				str += s;
				ptr++;
			}
		}
		str+= "->";
		//TRACE ("\n");
		//AfxMessageBox(str, MB_OK | MB_ICONINFORMATION);
		SetCapture();
	}
}

void CViewGL3D::SelectVertex(int nFlags, int x, int y)
{
	if( !m_pDoc->IsEnabled(MODF_MODEL) ) {
		ReleaseCapture();
		AfxMessageBox("Model modification is disabled, computation is running.", MB_OK | MB_ICONINFORMATION);
		return;
	}

	GLuint	selBuf[1024];
	GLint	hits;
	GLint	viewport[4];		// The Size Of The Viewport. [0] Is <x>, [1] Is <y>, [2] Is <length>, [3] Is <width>
	GLdouble aspect;
	CRect rc;
	int w, h;

	if (nFlags != MK_LBUTTON)
		return;

	m_bSelecting = TRUE;

	// make GL device context current
	if( !wglMakeCurrent(m_pDC->GetSafeHdc(), m_hRC) ) {
		AfxMessageBox("Failed to initialize GL.", MB_OK | MB_ICONINFORMATION);
		return;
	}

	GetClientRect(&rc);
	w = rc.Width();
	h = rc.Height();
	aspect =  (double) w/h;
	glViewport(0, 0, w, h);

	// This Sets The Array <viewport> To The Size And Location Of The Screen Relative To The Window
	glGetIntegerv(GL_VIEWPORT, viewport);
	glSelectBuffer(512, selBuf);									// Tell OpenGL To Use Our Array For Selection

	// Puts OpenGL In Selection Mode. Nothing Will Be Drawn.  Object ID's and Extents Are Stored In The Buffer.
	(void) glRenderMode(GL_SELECT);
	
	glInitNames();													// Initializes The Name Stack
	glPushName(0);													// Push 0 (At Least One Entry) Onto The Stack

	glPushMatrix();													// Push The Projection Matrix
		glMatrixMode(GL_PROJECTION);								// Selects The Projection Matrix
		glLoadIdentity();											// Resets The Matrix
		gluPickMatrix((GLdouble) x, (GLdouble) (viewport[3]-y), m_fSelSensitivity, m_fSelSensitivity, viewport);
		gluPerspective(m_dProjAngle, aspect, 0.001*m_diag, 10000*m_diag );
		glMatrixMode(GL_MODELVIEW);									// Select The Modelview Matrix
	glPopMatrix();

	glPushMatrix();
		SetTransformations();

		DrawSelectionPoints(GL_SELECT);							// Render The Targets To The Selection Buffer
	glPopMatrix();

	hits=glRenderMode(GL_RENDER);								// Switch To Render Mode, Find Out How Many

	SwapBuffers(m_pDC->GetSafeHdc());

	wglMakeCurrent(NULL, NULL);

	ProcessHits(hits, selBuf);									// Objects Were Drawn Where The Mouse Was

	m_bSelecting = FALSE;
}
//**** end selection *************************************************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


BOOL CViewGL3D::CheckGlError(void)
{
	// check for errors
	m_glErrorCode=glGetError();
	if(m_glErrorCode != GL_NO_ERROR)
	{
		const GLubyte *estring;
		CString mexstr;
		// get the error descritption from OGL
		estring = gluErrorString(m_glErrorCode);
		// prepare and show a message box
		mexstr.Format("GLEnabledView:\n\tAn OpenGL error occurred: %s\n", estring);
		AfxMessageBox(mexstr,MB_OK | MB_ICONEXCLAMATION);
		// replicate mex to debug trace
		TRACE0(mexstr);

		return 0;
	}

	return 1;
}

// x,y, z coordinates will be transformed to Return coordinates rx, ry, rz according to tag value
void CViewGL3D::DPtoGL(double x, double y, double z, double& rx, double& ry, double& rz, int tag)
{
	double	xu, yu, zu;

	switch(tag) {
		case TRC_FRW:
			gluProject(x, y, z, m_modelMatrix, m_projMatrix, m_viewport, &rx, &ry, &rz);
			break;
		case TRC_INV:
			gluUnProject(x, m_oldRect.bottom-y, z, m_modelMatrix, m_projMatrix, m_viewport, &rx, &ry, &rz);
			break;
		// secial transformation uses calibrated depth value
		case TRC_INVSP:
			gluProject(rx, ry, rz, m_modelMatrix, m_projMatrix, m_viewport, &xu, &yu, &zu);
			gluUnProject(x, m_oldRect.bottom-y, zu, m_modelMatrix, m_projMatrix, m_viewport, &rx, &ry, &rz);
			break;
	}
}


LRESULT CViewGL3D::OnSaveSettings(WPARAM wParam, LPARAM lParam)
{
	View3dSet*	pSet;
	VIEW3DSET	set;
	CString		str;

	str = *((CString*) wParam);
	pSet = m_pDoc->GetView3dSettingsArray();
	ASSERT(pSet != NULL);

	set.fAlphaRel = m_srfRel.GetAlpha();
	set.bRelief = m_srfRel.IsVisible();
	set.bRelSolid = m_srfRel.IsSolid();

	set.bField = m_srfFld.IsVisible();
	set.bFldSolid = m_srfFld.IsSolid();
	set.fAlphaSrf = m_srfFld.GetAlpha();

	set.bBodySolid = m_bBodySolid;
	set.bCullFacet = m_bCullFacet;
	set.bDarkenInside = m_bDarkenInside;
	set.bDrawBitmaps = m_bDrawBitmaps;
	set.bGrdLineDwnX = m_bGrdLineDwnX;
	set.bGrdLineDwnZ = m_bGrdLineDwnZ;
	set.bGrdLineUpX = m_bGrdLineUpX;
	set.bGrdLineUpZ = m_bGrdLineUpZ;
	set.bGrdLineVrt = m_bGrdLineVrt;
	set.bHorizon = m_bHorizon;
	set.bHorizonSolid = m_bHorizonSolid;
	set.bLightsEnabled = m_bLightsEnabled;
	set.bOutFct = m_bOutFct;
	set.bProfilePlanes = m_bProfilePlanes;
	set.bProfilePlanesSolid = m_bProfilePlanesSolid;
	set.bTransparentBodies = m_bTransparentBodies;
	set.bVrtFct = m_bVrtFct;
	set.dCameraEyeZ = m_dCameraEyeZ;
	set.dCameraX = m_dCameraX;
	set.dCameraY = m_dCameraY;
	set.dCameraZ = m_dCameraZ;
	set.decXl = m_decXl;
	set.decYl = m_decYl;
	set.decZl = m_decZl;
	set.dProjAngle = m_dProjAngle;
	set.fAlphaBmp = m_fAlphaBmp;
	set.fAlphaBody = m_fAlphaBody;
	set.fAlphaFld = m_fAlphaFld;
	set.fAlphaProf = m_fAlphaProf;
	set.fDarkenInside = m_fDarkenInside;
	set.fHorizonAlpha = m_fHorizonAlpha;
	set.fRotX = m_fRotX;
	set.fRotY = m_fRotY;
	set.fRotY = m_fRotZ;
	set.fSelSensitivity = m_fSelSensitivity;
	set.minXl = m_minXl;
	set.minYl = m_minYl;
	set.minZl = m_minZl;
	set.mX = m_mX;
	set.mY = m_mY;
	set.mZ = m_mZ;
	set.nColEnd = m_nColEnd;
	set.nColStart = m_nColStart;
	set.nCullFacet = m_nCullFacet;
	set.nFld = m_nFld;
	set.nFldTbl = m_nFldTbl;
	set.nMode = m_nMode;
	set.nRowEnd = m_nRowEnd;
	set.nRowStart = m_nRowStart;
	set.nTextType = m_nTextType;
	set.rX = m_rX;
	set.rY = m_rY;
	set.rZ = m_rZ;
	set.stepXl = m_stepXl;
	set.stepYl = m_stepYl;
	set.stepZl = m_stepZl;
	strcpy(set.strSettingsName, str);

	pSet->Add(set);

	return 0;
}

LRESULT CViewGL3D::OnLoadSettings(WPARAM wParam, LPARAM lParam)
{
	int nIndex;
	View3dSet*	pSet;
	VIEW3DSET	set;

	nIndex = (int) wParam;
	pSet = m_pDoc->GetView3dSettingsArray();
	ASSERT(pSet != NULL);

	if( nIndex > pSet->GetUpperBound() ) {
		AfxMessageBox("Loading failed.", MB_OK | MB_ICONINFORMATION);
		return -1;
	}

	set = pSet->GetAt(nIndex);

	m_srfFld.Show(set.bField);
	m_srfFld.SetSolid(set.bFldSolid);
	m_srfFld.SetAlpha(set.fAlphaFld);

	m_srfRel.SetAlpha(set.fAlphaRel);
	m_srfRel.Show(set.bRelief);
	m_srfRel.SetSolid(set.bRelSolid);

	m_bBodySolid = set.bBodySolid;
	m_bCullFacet = set.bCullFacet;
	m_bDarkenInside = set.bDarkenInside;
	m_bDrawBitmaps = set.bDrawBitmaps;
	m_bGrdLineDwnX = set.bGrdLineDwnX;
	m_bGrdLineDwnZ = set.bGrdLineDwnZ;
	m_bGrdLineUpX = set.bGrdLineUpX;
	m_bGrdLineUpZ = set.bGrdLineUpZ;
	m_bGrdLineVrt = set.bGrdLineVrt;
	m_bHorizon = set.bHorizon;
	m_bHorizonSolid = set.bHorizonSolid;
	m_bLightsEnabled = set.bLightsEnabled;
	m_bOutFct = set.bOutFct;
	m_bProfilePlanes = set.bProfilePlanes;
	m_bProfilePlanesSolid = set.bProfilePlanesSolid;
	m_bTransparentBodies = set.bTransparentBodies;
	m_bVrtFct = set.bVrtFct;
	m_dCameraEyeZ = set.dCameraEyeZ;
	m_dCameraX = set.dCameraX;
	m_dCameraY = set.dCameraY;
	m_dCameraZ = set.dCameraZ;
	m_decXl = set.decXl;
	m_decYl = set.decYl;
	m_decZl = set.decZl;
	m_dProjAngle = set.dProjAngle;
	m_fAlphaBmp = set.fAlphaBmp;
	m_fAlphaBody = set.fAlphaBody;
	m_fAlphaFld = set.fAlphaFld;
	m_fAlphaProf = set.fAlphaProf;
	m_fDarkenInside = set.fDarkenInside;
	m_fHorizonAlpha = set.fHorizonAlpha;
	m_fRotX = set.fRotX;
	m_fRotY = set.fRotY;
	m_fRotZ = set.fRotY;
	m_fSelSensitivity = set.fSelSensitivity;
	m_minXl = set.minXl;
	m_minYl = set.minYl;
	m_minZl = set.minZl;
	m_mX = set.mX;
	m_mY = set.mY;
	m_mZ = set.mZ;
	m_nColEnd = set.nColEnd;
	m_nColStart = set.nColStart;
	m_nCullFacet = set.nCullFacet;
	m_nFld = set.nFld;
	m_nFldTbl = set.nFldTbl;
	m_nMode = set.nMode;
	m_nRowEnd = set.nRowEnd;
	m_nRowStart = set.nRowStart;
	m_nTextType = set.nTextType;
	m_rX = set.rX;
	m_rY = set.rY;
	m_rZ = set.rZ;
	m_stepXl = set.stepXl;
	m_stepYl = set.stepYl;
	m_stepZl = set.stepZl;

	if( m_pSheetProperties != NULL )  {
		PropertiesInit(m_pSheetProperties);
		Invalidate(FALSE);
	}

	return 0;
}



BOOL CViewGL3D::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
	HCURSOR hCursor;

	hCursor = AfxGetApp()->LoadCursor(IDC_CURSOR_3D);
	::SetCursor(hCursor);

	return 0;
	//return CView::OnSetCursor(pWnd, nHitTest, message);
}

void CViewGL3D::OnViewPlanesimulator()
{
	if( m_nProjection == PRJ_ORTHO ) {
		int ret = AfxMessageBox("Plane simulator mode is supported only for perpective projection. Switch to perspective projection?", MB_YESNO | MB_ICONQUESTION );
		if( ret != IDYES ) return;
	}
	m_bPlaneSimulator = !m_bPlaneSimulator;
	Invalidate(FALSE);
}
void CViewGL3D::OnUpdateViewPlanesimulator(CCmdUI *pCmdUI)
{
	if( m_bPlaneSimulator )		pCmdUI->SetCheck(1);
	else						pCmdUI->SetCheck(0);
}

