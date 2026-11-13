#include "api.h"
extern "C" void* memcpy(void* dest, const void* source, unsigned long long count)
{
    auto* out = static_cast<volatile unsigned char*>(dest);
    auto* in = static_cast<const volatile unsigned char*>(source);
    for (unsigned long long i = 0; i < count; ++i) out[i] = in[i];
    return dest;
}
int main() { T09Cpp::Small s{10,20,30,40,50,60,70,80,90,100,110}; auto callback=T09Cpp::fetchCallback(); auto b=callback(s);
return s.x==10 && s.y==20 && s.z==30 && s.w==40 && s.u==50 && s.v==60 && s.q==70 && s.r==80 && s.t==90 && s.k==100 && s.j==110 && b.x==11 && b.y==20 && b.z==30 && b.w==40 && b.u==50 && b.v==60 && b.q==70 && b.r==80 && b.t==90 && b.k==100 && b.j==112 ? 0 : 1; }
