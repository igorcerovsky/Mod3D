#pragma once
#include "myobject.h"

#include "stdafx.h"

#include "gl/gl.h"
#include "gl/glu.h"


#include "ClrGrad\gradient.h"

class CData3D :
	public CMyObject
{
DECLARE_SERIAL( CData3D )

public:
	CData3D(void);
	~CData3D(void);

	virtual void	Serialize(CArchive& ar);
	virtual int		Properties();

	BOOL	IsEmpty()	{ return m_pData==NULL;}

	void	SetFormat(int nFormat)	{ m_nFormat = nFormat;}
	int		GetFormat()				{ return m_nFormat;}

	void	SetItemCount(int n)		{ m_nCount=n;}
	int		GetItemCount()			{ return m_nCount;}

	void	SetData(double* pNew, int nCount, int nFormat=4);
	void	FreeData();

	void	DrawGL();
	int		DrawProfile(CDC* pDC, double xSc, double xOff, double ySc, double yOff, int nProfType, double xy);
	void	DrawMap(CDC* pDC, double scx, double offx, double scy, double offy );
	void	SetProfDis(double dis)	{m_dProfDis=dis;}
	void	SetDrawRadius(double r)	{m_dRadiusDraw=r;}

	//void	SetFilePathA(CString strFile) {m_strFile = strFile;	}
	//void	GetFilePathA(CString &strFile) {strFile = m_strFile;}

	void	UpdateMimMax();
private:
	int		m_nVersion;

protected:
	int		m_nCount;
	int		m_nFormat;
	double* m_pData;

	// prifile drawing
	double	m_dProfDis;

	BOOL	m_bPoints;
	int		m_nStacks;
	int		m_nSlices;
	BOOL	m_bConnectPts;;
	BOOL	m_bColorDstrb;
	BOOL	m_bCustomRange;
	BOOL	m_bUpdateMinMax;
	double	m_dMinDataCust;
	double	m_dMaxDataCust;
	double	m_dMinData;
	double	m_dMaxData;
	double	m_dRadiusDraw;

	CGradient	m_clrGrad;
	LOGPEN	m_logPen;
	double	m_dAlpha;

};
