#include "api.h"
int _fltused=0;
int main(void) { Triple a={{10.0f,20.0f},30.0f}; Other b={{1.0f,2.0f}}; double c=combine(a,7,b); double d=get_combine()(a,7,b); return !(c==166.5 && d==166.5 && a.pair.x==10.0f && b.pair.x==1.0f); }
