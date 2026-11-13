#include "api.h"
int main() { T09Cpp::Small s{10,20,30,40}; auto callback=T09Cpp::fetchCallback(); auto a=T09Cpp::most(s); auto b=callback(s);
return callback==&T09Cpp::most && s.x==10 && s.y==20 && s.z==30 && s.w==40 && a.x==11 && a.y==20 && a.z==30 && a.w==42 && b.x==11 && b.y==20 && b.z==30 && b.w==42 ? 0 : 1; }
