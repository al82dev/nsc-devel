#include "api.h"
int main() { T09Cpp::Small s{10,20,30,40,50,60,70,80}; auto callback=T09Cpp::fetchCallback(); auto a=T09Cpp::most(s); auto b=callback(s);
return callback==&T09Cpp::most && s.x==10 && s.y==20 && s.z==30 && s.w==40 && s.u==50 && s.v==60 && s.q==70 && s.r==80 && a.x==11 && a.y==20 && a.z==30 && a.w==40 && a.u==50 && a.v==60 && a.q==70 && a.r==82 && b.x==11 && b.y==20 && b.z==30 && b.w==40 && b.u==50 && b.v==60 && b.q==70 && b.r==82 ? 0 : 1; }
