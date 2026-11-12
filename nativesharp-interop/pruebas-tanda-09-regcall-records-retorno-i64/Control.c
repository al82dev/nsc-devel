#include "api.h"
int _fltused=0;
int main(void) { Triple a={{10.0f,20.0f},30.0f}; Other b={{1.0f,2.0f}}; long long c=combine(a,7,b); long long d=get_combine()(a,7,b); return !(c==4294967462LL && d==4294967462LL && a.pair.x==10.0f && b.pair.x==1.0f); }
