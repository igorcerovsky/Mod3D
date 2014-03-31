#pragma once

#include <afxtempl.h>
#include "afxwin.h"
// CMyObject command target

class CMyObject : public CObject
{
DECLARE_SERIAL( CMyObject )
public:
	CMyObject()												{ m_strID = ""; m_strName = ""; m_bVisible = TRUE; }
	CMyObject(const char* strID)							{ m_strID = strID; m_strName = ""; m_bVisible = TRUE; }
	CMyObject(const char* strID, const char* strName)		{ m_strID = strID; m_strName = strName; m_bVisible = TRUE; }
	virtual ~CMyObject();
	virtual void	Serialize(CArchive& ar);
	virtual void	Draw(CDC* pDC, double scx, double offx, double scy, double offy);
	virtual int		Properties()	{ AfxMessageBox("No Properties for this object.", MB_OK | MB_ICONINFORMATION); return 0; }
	// virtual	BOOL	IsClicked()		{ return FALSE;}

	CString GetID()						{ return m_strID; }
	void	SetID( CString strID)		{ m_strID = strID; }

	CString GetName()					{ return m_strName; }
	void	SetName( CString strName )	{ m_strName = strName; }

	CString	GetIdName()					{ 
		return m_strID + " - " + m_strName; }

	void	Show( BOOL bVisible=TRUE)	{ m_bVisible = bVisible;}
	BOOL	IsVisible()					{ return m_bVisible;}

public:
	CString	m_strID;
	CString m_strName;
	BOOL	m_bVisible;
};

typedef CTypedPtrArray <CObArray, CMyObject*> MyObPtrArray;
