#include "api.h"
extern "C" { int _fltused = 0; }
int main() { int input=14; int out=0; T09HfaCpp::Nested a={{10,20},30}; T09HfaCpp::Array b={{10,20,30}}; T09HfaCpp::Nested c=T09HfaCpp::nested(a,&input,&out), d=T09HfaCpp::fetchNested()(a,&input,&out); T09HfaCpp::Array e=T09HfaCpp::array(b,&input,&out), f=T09HfaCpp::fetchArray()(b,&input,&out); return !(input==14 && out==4 && c.pair.x==24 && d.pair.x==24 && e.values[0]==24 && f.values[0]==24); }
