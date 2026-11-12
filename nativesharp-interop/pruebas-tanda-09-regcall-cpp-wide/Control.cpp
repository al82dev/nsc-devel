#include "api.h"
extern "C" { int _fltused = 0; }
int main() {
    int p1=7,p2=7,p3=7,p4=7;
    T09HfaCpp::Nested a{{10.0f,20.0f},30.0f}; T09HfaCpp::Array b{{10.0f,20.0f,30.0f}};
    auto c=T09HfaCpp::nested(a,4294967308LL,3U,4294967301ULL,&p1,4.0); auto d=T09HfaCpp::fetchNested()(a,4294967308LL,3U,4294967301ULL,&p2,4.0);
    auto e=T09HfaCpp::array(b,4294967308LL,3U,4294967301ULL,&p3,4.0); auto f=T09HfaCpp::fetchArray()(b,4294967308LL,3U,4294967301ULL,&p4,4.0);
    return !(p1==8 && p2==8 && p3==8 && p4==8 && a.pair.x==10.0f && a.z==30.0f && b.values[0]==10.0f && b.values[2]==30.0f &&
             c.pair.x==91.0f && c.pair.y==22.0f && c.z==33.0f && d.pair.x==91.0f && d.pair.y==22.0f && d.z==33.0f &&
             e.values[0]==91.0f && e.values[1]==22.0f && e.values[2]==33.0f &&
             f.values[0]==91.0f && f.values[1]==22.0f && f.values[2]==33.0f);
}
