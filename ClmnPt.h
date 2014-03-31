#pragma once

#include <afxtempl.h>
#include "Point3d.h"

// CClmnPt command target

class CClmnPt : public CObject
{
	DECLARE_SERIAL(CClmnPt);
public:
	CClmnPt();
	CClmnPt( double z );
	CClmnPt( int nBodyID, double z);
	CClmnPt( int nBodyID, double z, void* pBody );
	CClmnPt(const CClmnPt& mpt);
	virtual ~CClmnPt();
	virtual void Serialize(CArchive& ar);
	BOOL	IsModified()							{return m_bModified;}
	void	SetModified(BOOL bModified = TRUE)		{m_bModified = bModified;}

	const CClmnPt& operator=(const CClmnPt& mpt)
	{
		m_nBodyID = mpt.m_nBodyID;
		m_pBody = mpt.m_pBody;
		m_z = mpt.m_z;
		return *this;
	}

public:
	BOOL	m_bModified;		
	int		m_nBodyID;			// body  ID
	void*	m_pBody;			// pointer to body, temporary!
	
	CPoint3D	m_pt;
	double		m_z;

};

typedef CArray< CClmnPt, CClmnPt&> ClmnPtArray;
typedef CTypedPtrArray< CObArray, CClmnPt*> ClmnPtPtrArray;


