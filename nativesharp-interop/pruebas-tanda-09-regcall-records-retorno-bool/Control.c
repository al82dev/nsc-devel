#include "api.h"
int _fltused=0;
int main(void) { Triple a={{10.0f,20.0f},30.0f}; Other b={{1.0f,2.0f}}; _Bool c=combine(a,7,b); _Bool d=get_combine()(a,7,b); return !(c==1 && d==1 && !combine(a,8,b) && !get_combine()(a,8,b) && a.pair.x==10.0f && b.pair.x==1.0f); }
