#include "api.h"
extern "C" { int _fltused = 0; }
int main() {
    T09HfaCpp::Nested a{{10.0f,20.0f},30.0f}; T09HfaCpp::Array b{{10.0f,20.0f,30.0f}};
    T09HfaCpp::OtherNested n{{1.0f,2.0f}}; T09HfaCpp::OtherArray q{{1.0f,2.0f}};
    auto c=T09HfaCpp::nested(a,n,7); auto d=T09HfaCpp::fetchNested()(a,n,7);
    auto e=T09HfaCpp::array(b,q,7); auto f=T09HfaCpp::fetchArray()(b,q,7);
    return !(a.pair.x==10.0f && a.z==30.0f && b.values[0]==10.0f && b.values[2]==30.0f &&
             c.pair.x==18.0f && c.pair.y==22.0f && c.z==31.0f && d.pair.x==18.0f && d.pair.y==22.0f && d.z==31.0f &&
             e.values[0]==18.0f && e.values[1]==22.0f && e.values[2]==31.0f &&
             f.values[0]==18.0f && f.values[1]==22.0f && f.values[2]==31.0f);
}
