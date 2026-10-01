template< class T > int SetChange( T& newValue, T& oldValue )
{
	BOOL b;
    
	b = oldValue != newValue;
	newValue = oldValue;

	return b;
}

template< class T > int IsPointInPolygon(T* pPlg, int nFormat, int n, T x, T y)
{
	// pt - point
	// plg - polygon in the same coordinates as pt !!!
	// n - number of polygon points
	double x1, x2, y1, y2;
	BOOL	bIn = false;

	// check wheather point is inside polygon
	x1 = pPlg[0];
	y1 = pPlg[1];
	for( int i=1; i<n; i++) {
		x2 = pPlg[i*nFormat + 0];
		y2 = pPlg[i*nFormat + 1];
		if (y1 < y && y2 >= y  ||  y2 < y && y1 >= y) {
			if( x1 + (double)(y-y1)/(double)(y2 - y1)*(x2 - x1) < x ) {
				bIn = !bIn; 
			}
		}
		x1 = x2;
		y1 = y2;
	} //for (over polygon)

	if( bIn ) 	
		return 1;

	return 0; 
}

template<class T> T AngleDeg(T a) { return (T)fmod((double)a, 360.0); }
template<class T> T DegToRad(T a) { return (T)(3.1415926535897932384626433832795*a/180.0); }
template<class T> T DegUnit(T a) { return (T)(a/360.0); }

template<class T> void FindTicks(T& min, T& max, T& step, int& dec, int n=6)
{
	double	a, b, s;
	T	minT, maxT;
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
