// GradientCtrlImpl.h: interface for the CGradientCtrlImpl class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "Gradient.h"

class CGradientCtrl;

class CGradientCtrlImpl  
{
public:
	CGradientCtrlImpl(CGradientCtrl *owner);
protected:
	virtual ~CGradientCtrlImpl();

	void Draw(CDC *dc);
	void DrawGradient(CDC *dc);
	void DrawPegs(CDC *dc);
	void DrawSelPeg(CDC *dc, int peg);
	void DrawSelPeg(CDC *dc, CPoint point, int direction);
	void DrawPeg(CDC *dc, CPoint point, COLORREF colour, int direction);
	void DrawEndPegs(CDC *dc);
	void DrawHistogram(CDC* pDC, int left, int top, int width, int height);
	
	int PointFromPos(float pos);
	float PosFromPoint(int point);
	int GetPegIndent(int index);
	int PtInPeg(CPoint point);

	void GetPegRect(int index, CRect *rect, bool right);
	
	void ParseToolTipLine(CString &tiptext, CPeg peg);
	void ShowTooltip(CPoint point, CString text);
	CString ExtractLine(CString source, int line);
	void SetTooltipText(CString text);
	void DestroyTooltip();
	void SynchronizeTooltips();

	bool IsVertical();
	int GetDrawWidth();

	HWND m_wndToolTip;
	TOOLINFO m_ToolInfo;
	CGradientCtrl *m_Owner;
	CToolTipCtrl m_ToolTipCtrl;
	int m_RectCount;
	BOOL m_LeftDownSide, m_RightUpSide;

	CPeg m_Null;

	friend class CGradientCtrl;

	CRect GetColorRect();
};

