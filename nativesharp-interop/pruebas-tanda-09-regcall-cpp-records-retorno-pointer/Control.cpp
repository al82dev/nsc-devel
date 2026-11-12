#include "api.h"
extern "C" { int _fltused=0; }
int main(){ T09HfaCpp::Nested a{{10.0f,20.0f},30.0f}; T09HfaCpp::OtherNested n{{1.0f,2.0f}}; T09HfaCpp::Array b{{10.0f,20.0f,30.0f}}; T09HfaCpp::OtherArray q{{1.0f,2.0f}}; int c=0,d=0,e=0,f=0; int* pc=T09HfaCpp::nested(a,7,n,&c); int* pd=T09HfaCpp::fetchNested()(a,7,n,&d); int* pe=T09HfaCpp::array(b,7,q,&e); int* pf=T09HfaCpp::fetchArray()(b,7,q,&f); return !(pc==&c && pd==&d && pe==&e && pf==&f && c==166 && d==166 && e==166 && f==166 && a.pair.x==10 && b.values[0]==10); }
