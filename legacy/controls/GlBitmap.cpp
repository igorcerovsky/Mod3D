#include "stdafx.h"
#include "glbitmap.h"
#include "math.h"

CGlBitmap::CGlBitmap(void)
{
	m_nType = glBmp_Vert;
	m_nDispLst = 0;		// no display list at present
}

CGlBitmap::~CGlBitmap(void)
{
	glDeleteTextures(1, &m_nIdTex);
	glDeleteLists(m_nDispLst, 1);
	delete	m_tex;
}

// loads bitmap to GL call list
// GL rendering context must be active
BOOL CGlBitmap::CompileDisplayList(CImgReferenced& img)
{
	ASSERT(wglGetCurrentContext() != NULL);

	//CWaitCursor wait;
	BYTE* pPix;
	img.GetGlDimensionsPow2(m_w, m_h);
	m_nFormat = 4;
	pPix = new BYTE[m_w*m_h*m_nFormat];
	img.GetPixels(pPix, m_w, m_h, m_nFormat);

	// delete previous display list
	if( glIsList(m_nDispLst) ) {
		glDeleteLists(m_nDispLst, 1);
	}
	if( glIsTexture(m_nIdTex) ) {
		glDeleteTextures(1, &m_nIdTex);
	}

	// create display list
	m_nDispLst = glGenLists(1);
	glGenTextures(1, &m_nIdTex);
	glEnable(GL_TEXTURE_2D);
	glBindTexture( GL_TEXTURE_2D, m_nIdTex );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexImage2D( GL_TEXTURE_2D, 0, 4, m_w, m_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pPix);
	glDisable(GL_TEXTURE_2D);

	glNewList(m_nDispLst, GL_COMPILE);
		glEnable(GL_TEXTURE_2D);
		glBindTexture( GL_TEXTURE_2D, m_nIdTex );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		glEnable(GL_DEPTH_TEST);

		glBegin( GL_QUADS );
			glColor4f(1.0f, 1.0f, 1.0f, 0.7f);
			glTexCoord2f( 0.0f, 0.0f);		
			glVertex3f( m_xt, m_yt, m_zt);

			glTexCoord2f( 0.0f, 1);			
			glVertex3f( m_xb, m_yt, m_zb);

			glTexCoord2f( 1, 1);			
			glVertex3f( m_xb, m_yb, m_zb);

			glTexCoord2f( 1, 0.0f);			
			glVertex3f( m_xt, m_yb, m_zt);

		glEnd();
		glDisable(GL_TEXTURE_2D);
	glEndList();


	// delete texture array
	delete pPix;
	//wait.Restore();

	return	TRUE;
}

void CGlBitmap::DrawGL()
{
	if(m_bVisible) {
		glCallList(m_nDispLst);
	}
}

void CGlBitmap::DrawGL(float fAlpha)
{
	if(m_bVisible) {
		glCallList(m_nDispLst);
	}
}

void CGlBitmap::SetBoundRect(float xt, float yt, float zt, float xb, float yb, float zb)
{
	m_xt = xt;
	m_yt = yt;
	m_zt = zt;
	m_xb = xb;
	m_yb = yb;
	m_zb = zb;
}

// 2D texture
void CGlBitmap::CreateTexture2D(CImgReferenced& img)
{
	if( glIsTexture(m_nIdTex) ) {
		glDeleteTextures(1, &m_nIdTex);
	}

	BYTE* pPix;
	img.GetGlDimensionsPow2(m_w, m_h);
	m_nFormat = 4;
	pPix = new BYTE[m_w*m_h*m_nFormat];
	img.GetPixels(pPix, m_w, m_h, m_nFormat);

	img.GetTopLeft(aN, aW);
	img.GetBotRight(aS, aE);
	const int w = m_w;
	const int h = m_h;
	const int nFormat = m_nFormat;
	const BYTE* pPixels = pPix;
	glEnable(GL_TEXTURE_2D);
	glGenTextures(1, &m_nIdTex);
	glBindTexture(GL_TEXTURE_2D, m_nIdTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, nFormat, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pPixels);
	glDisable(GL_TEXTURE_2D);
	delete pPix;

}

// 2D texture
BOOL CGlBitmap::DrawOnrelief(CGrid* pRel, double dAlpha, double z)
{
	if(!m_bVisible) return 0;

	ASSERT( glIsTexture(m_nIdTex) );

	if( pRel->IsPlanar() ) pRel = NULL;

	if( pRel==NULL ) {
		glEnable(GL_TEXTURE_2D);
		glBindTexture( GL_TEXTURE_2D, m_nIdTex );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE/*GL_DECAL*/ );

		glDisable(GL_LIGHTING);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDepthFunc(GL_LEQUAL);		

		float matEm[] = {0.0f, 0.0f, 0.0f, 0.0f};
		glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, matEm);
		float matAb[] = {0.2f, 0.2f, 0.2f, 0.0f};
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, matAb);
		float matDf[] = {0.8f, 0.8f, 0.8f, 0.0f};
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, matDf);

		glBegin( GL_QUADS );
			glColor4f(1.0f, 1.0f, 1.0f, 0.8f);
			glTexCoord2d( 1.0f, 0.0f );			glVertex3d( aW, z, -aS );
			glTexCoord2d( 1.0f, 1.0f );			glVertex3d( aE, z, -aS );
			glTexCoord2d( 0.0f, 1.0f );			glVertex3d( aE, z, -aN );
			glTexCoord2d( 0.0f, 0.0f );			glVertex3d( aW, z, -aN );
			

		glEnd();
		glDisable(GL_TEXTURE_2D);
		glDepthFunc(GL_LESS);		
		//glDisable(GL_BLEND);
	}
	// map bitmap to relief
	else {
		// GL settings
		glPolygonMode(GL_FRONT, GL_FILL);
		glEnable(GL_DEPTH_TEST);
		//!!!
		glDepthFunc(GL_LEQUAL);		
		glEnable(GL_TEXTURE_2D);
		glBindTexture( GL_TEXTURE_2D, m_nIdTex );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE/*GL_DECAL*/ );

		glDisable(GL_LIGHTING);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		float matEm[] = {0.0f, 0.0f, 0.0f, 0.0f};
		glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, matEm);
		float matAb[] = {0.2f, 0.2f, 0.2f, 0.0f};
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, matAb);
		float matDf[] = {0.8f, 0.8f, 0.8f, 0.0f};
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, matDf);

		// image dimensions
		double	iE, iW, iS, iN;
		int rows, cols;
		double	x0, y0;
		double	csx, csy;
		double** pData;
		rows = pRel->GetRows();
		cols = pRel->GetCols();
		x0 = pRel->GetX0();
		y0 = pRel->GetY0();
		csx = pRel->GetXSize();
		csy = pRel->GetYSize();
		pData = pRel->GetData();

		double s, t;
		double ds, dt;
		iE = aE;
		iW = aW;
		iS = aS;
		iN = aN;
		ds = iE-iW;
		dt = iN-iS;

		double xM, yM, zM;
		double xE, xW, yN, yS;
		double zWS, zWN, zEN, zES;
		glColor4d(1.0, 1.0, 1.0, dAlpha);
		for(int i=0; i<rows-1; i ++) {
			for(int j=0; j<cols-1; j++) {
				xW = x0 + j*csx;
				xE = x0 + (j+1)*csx;
				yS = y0 + i*csy;
				yN = y0 + (i+1)*csy;
				xM = (xW+xE)/2.0;
				yM = (yS+yN)/2.0;

				zWS = pData[i][j];
				zES = pData[i][j+1];
				zEN = pData[i+1][j+1];
				zWN = pData[i+1][j];
				zM = (zWS+zES+zEN+zWN)/4.0;
				//  must be withing texture
				if( xW>=iW && xW<=iE &&	yS>=iS && yN<=iN )	{
					glBegin( GL_TRIANGLES );
						glColor4d(1.0, 1.0, 1.0, dAlpha);

						// first tiangle
						s = (xW-iW)/ds;
						t = 1-(yS-iS)/dt;
						glTexCoord2d( t, s );
						glVertex3d( xW, zWS, -yS );

						s = (xE-iW)/ds;
						t = 1-(yS-iS)/dt;
						glTexCoord2d( t, s );
						glVertex3d( xE, zES, -yS );

						s = (xM-iW)/ds;
						t = 1-(yM-iS)/dt;
						glTexCoord2d( t, s );
						glVertex3d( xM, zM, -yM );

						// second tiangle
						s = (xE-iW)/ds;
						t = 1-(yS-iS)/dt;
						glTexCoord2d( t, s );
						glVertex3d( xE, zES, -yS );

						s = (xE-iW)/ds;
						t = 1-(yN-iS)/dt;
						glTexCoord2d( t, s );
						glVertex3d( xE, zEN, -yN );

						s = (xM-iW)/ds;
						t = 1-(yM-iS)/dt;
						glTexCoord2d( t, s );
						glVertex3d( xM, zM, -yM );

						// 3. tiangle
						s = (xE-iW)/ds;
						t = 1-(yN-iS)/dt;
						glTexCoord2d( t, s );
						glVertex3d( xE, zEN, -yN );

						s = (xW-iW)/ds;
						t = 1-(yN-iS)/dt;
						glTexCoord2d( t, s );
						glVertex3d( xW, zWN, -yN );

						s = (xM-iW)/ds;
						t = 1-(yM-iS)/dt;
						glTexCoord2d( t, s );
						glVertex3d( xM, zM, -yM );

						// 4. tiangle
						s = (xW-iW)/ds;
						t = 1-(yN-iS)/dt;
						glTexCoord2d( t, s );	
						glVertex3d( xW, zWN, -yN );

						s = (xW-iW)/ds;
						t = 1-(yS-iS)/dt;
						glTexCoord2d( t, s );	
						glVertex3d( xW, zWS, -yS );

						s = (xM-iW)/ds;
						t = 1-(yM-iS)/dt;
						glTexCoord2d( t, s );	
						glVertex3d( xM, zM, -yM );
					glEnd();
				}
			}
		}
		glDisable(GL_TEXTURE_2D);
		glDepthFunc(GL_LESS);

	}

	return	TRUE;
}
