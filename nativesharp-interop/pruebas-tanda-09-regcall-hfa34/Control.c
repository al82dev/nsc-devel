#include "api.h"
int _fltused = 0;
int main(void)
{
    T09Three a = { 10.0f, 20.0f, 30.0f }; T09Four b = { 10.0f, 20.0f, 30.0f, 40.0f };
    T09Three c=t09_three(a), d=t09_get_three()(a);
    T09Four e=t09_four(b), f=t09_get_four()(b);
    return !(a.x==10.0f && a.z==30.0f && b.x==10.0f && b.w==40.0f &&
             c.x==11.0f && c.y==22.0f && c.z==33.0f && d.x==11.0f && d.y==22.0f && d.z==33.0f &&
             e.x==11.0f && e.y==22.0f && e.z==33.0f && e.w==44.0f &&
             f.x==11.0f && f.y==22.0f && f.z==33.0f && f.w==44.0f);
}
