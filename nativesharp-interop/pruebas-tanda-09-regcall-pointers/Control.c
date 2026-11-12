#include "api.h"
int _fltused = 0;
int main() { unsigned char p=2; short q=3; double r=4; T09Nested a={{10,20},30}; T09Array b={{10,20,30}}; T09Nested c=t09_nested(a,&p,&q,&r,&p), d=t09_get_nested()(a,&p,&q,&r,&p); T09Array e=t09_array(b,&p,&q,&r,&p), f=t09_get_array()(b,&p,&q,&r,&p); return !(p==6 && q==7 && r==8 && c.pair.x==20 && d.pair.x==23 && e.values[0]==26 && f.values[0]==29); }
