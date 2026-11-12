#include "api.h"
int _fltused = 0;
int main() { unsigned char p=2; short q=3; double r=4; unsigned char* pp=&p; short* qq=&q; double* rr=&r; T09Nested a={{10,20},30}; T09Array b={{10,20,30}}; T09Nested c=t09_nested(a,&pp,&qq,&rr), d=t09_get_nested()(a,&pp,&qq,&rr); T09Array e=t09_array(b,&pp,&qq,&rr), f=t09_get_array()(b,&pp,&qq,&rr); return !(pp==&p && qq==&q && rr==&r && p==6 && q==7 && r==8 && c.pair.x==20 && d.pair.x==23 && e.values[0]==26 && f.values[0]==29); }
