#include "api.h"
int _fltused = 0;
int main(void)
{
    T09Nested a={{10.0f,20.0f},30.0f}; T09Array b={{10.0f,20.0f,30.0f}};
    T09Nested c=t09_nested(a), d=t09_get_nested()(a);
    T09Array e=t09_array(b), f=t09_get_array()(b);
    return !(a.pair.x==10.0f && a.z==30.0f && b.values[0]==10.0f && b.values[2]==30.0f &&
             c.pair.x==11.0f && c.pair.y==22.0f && c.z==33.0f && d.pair.x==11.0f && d.pair.y==22.0f && d.z==33.0f &&
             e.values[0]==11.0f && e.values[1]==22.0f && e.values[2]==33.0f &&
             f.values[0]==11.0f && f.values[1]==22.0f && f.values[2]==33.0f);
}
