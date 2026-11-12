#include "api.h"
int _fltused = 0;
int main() { int input=14; int out=0; T09Nested a={{10,20},30}; T09Array b={{10,20,30}}; T09Nested c=t09_nested(a,&input,&out), d=t09_get_nested()(a,&input,&out); T09Array e=t09_array(b,&input,&out), f=t09_get_array()(b,&input,&out); return !(input==14 && out==4 && c.pair.x==24 && d.pair.x==24 && e.values[0]==24 && f.values[0]==24); }
