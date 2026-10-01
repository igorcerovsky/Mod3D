#include <afxtempl.h>
#include <afxwin.h>

typedef CArray<double, double>		DoubleArray;
typedef CArray<float, float>		FloatArray;
typedef CArray<int, int>			IntArray;
typedef CArray<BYTE, BYTE>			ByteArray;

typedef CArray<DoubleArray, DoubleArray&>	DDArray;
typedef CArray<FloatArray, FloatArray&>		FFArray;

typedef CArray<CPen, CPen&>			PenArray;
typedef CArray<CBrush, CBrush&>		BrushArray;
