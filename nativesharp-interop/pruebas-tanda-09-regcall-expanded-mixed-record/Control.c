#include "api.h"
int _fltused=0;
int main(void) { Triple a={{10.0f,20.0f},30.0f}; Other b={1.0f,2}; int c=combine(a,7,b); int d=get_combine()(a,7,b); return !(c==166 && d==166 && a.pair.x==10.0f && b.x==1.0f && b.y==2); }
