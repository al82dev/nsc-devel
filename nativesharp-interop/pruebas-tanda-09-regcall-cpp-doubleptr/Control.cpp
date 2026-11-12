#include "api.h"
extern "C" { int _fltused = 0; }
int main() { unsigned char p=2; short q=3; double r=4; unsigned char* pp=&p; short* qq=&q; double* rr=&r; T09HfaCpp::Nested a={{10,20},30}; T09HfaCpp::Array b={{10,20,30}}; T09HfaCpp::Nested c=T09HfaCpp::nested(a,&pp,&qq,&rr), d=T09HfaCpp::fetchNested()(a,&pp,&qq,&rr); T09HfaCpp::Array e=T09HfaCpp::array(b,&pp,&qq,&rr), f=T09HfaCpp::fetchArray()(b,&pp,&qq,&rr); return !(pp==&p && qq==&q && rr==&r && p==6 && q==7 && r==8 && c.pair.x==20 && d.pair.x==23 && e.values[0]==26 && f.values[0]==29); }
