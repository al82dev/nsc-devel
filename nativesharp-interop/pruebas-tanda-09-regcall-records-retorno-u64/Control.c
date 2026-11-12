#include "api.h"
int _fltused=0;
int main(void) { Triple a={{10.0f,20.0f},30.0f}; Other b={{1.0f,2.0f}}; unsigned long long c=combine(a,7,b); unsigned long long d=get_combine()(a,7,b); return !(c==9223372036854775974ULL && d==9223372036854775974ULL && a.pair.x==10.0f && b.pair.x==1.0f); }
