#include "api.h"
extern "C" { int _fltused = 0; }
int main() {
    T09HfaCpp::Box box{2};
    T09HfaCpp::Nested a{{10.0f,20.0f},30.0f}; T09HfaCpp::Array b{{10.0f,20.0f,30.0f}};
    auto c=T09HfaCpp::nested(a,&box); auto d=T09HfaCpp::fetchNested()(a,&box);
    auto e=T09HfaCpp::array(b,&box); auto f=T09HfaCpp::fetchArray()(b,&box);
    return !(box.x==6 && a.pair.x==10.0f && a.z==30.0f && b.values[0]==10.0f && b.values[2]==30.0f &&
             c.pair.x==12.0f && c.pair.y==22.0f && c.z==33.0f && d.pair.x==13.0f && d.pair.y==22.0f && d.z==33.0f &&
             e.values[0]==14.0f && e.values[1]==22.0f && e.values[2]==33.0f &&
             f.values[0]==15.0f && f.values[1]==22.0f && f.values[2]==33.0f);
}
