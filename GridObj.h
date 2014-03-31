#pragma once

#include "myobject.h"
#include "grid.h"



class CGridObj : public CMyObject
{
DECLARE_SERIAL(CGridObj)
public:
	enum gridTypes{
		leadingHorizon = 0,
		flyElevation = 1,
	};

public:
	CGridObj(void);
	CGridObj(const char* strID);
	CGridObj(const char* strID, const char* strName);
	~CGridObj(void);

//overrides
	virtual void	Serialize(CArchive& ar);
	virtual int		Properties();

	void SetType(int nType) { m_nType = nType;	}
	int	 GetType()			{ return m_nType;	}
	CGrid*	GetGrid()		{ return &m_grd;	}
	LOGPEN	GetLogPen()		{ return m_lp;		}

protected:
	int		m_nType;	// must be one of gridTypes
	CGrid	m_grd;		// grid itself
	LOGPEN	m_lp;
};
