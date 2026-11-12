#include "api.h"
extern "C" { int _fltused = 0; }
int main() { unsigned char p=2; short q=3; double r=4; T09HfaCpp::Nested a={{10,20},30}; T09HfaCpp::Array b={{10,20,30}}; T09HfaCpp::Nested c=T09HfaCpp::nested(a,&p,&q,&r,&p), d=T09HfaCpp::fetchNested()(a,&p,&q,&r,&p); T09HfaCpp::Array e=T09HfaCpp::array(b,&p,&q,&r,&p), f=T09HfaCpp::fetchArray()(b,&p,&q,&r,&p); return !(p==6 && q==7 && r==8 && c.pair.x==20 && d.pair.x==23 && e.values[0]==26 && f.values[0]==29); }
