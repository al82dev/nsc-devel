#include "api.h"
int _fltused=0;
int main(void){ Triple a={{10.0f,20.0f},30.0f}; Other b={{1.0f,2.0f}}; int c=0,d=0; int* p=combine(a,7,b,&c); int* q=get_combine()(a,7,b,&d); return !(p==&c && q==&d && c==166 && d==166 && a.pair.x==10.0f && b.pair.x==1.0f); }
