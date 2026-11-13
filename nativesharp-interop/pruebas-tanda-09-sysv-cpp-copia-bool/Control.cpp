extern "C" { __declspec(selectany) int _fltused = 0; }
#include "api.h"
int main() { T09Cpp::Other s{true,20,30}; auto callback=T09Cpp::fetchCallback(); auto a=T09Cpp::most(s); auto b=callback(s); bool kept=s.x; s.x=false; auto c=T09Cpp::most(s); auto d=callback(s); return callback==&T09Cpp::most && kept && !s.x && s.y==20 && s.z==30 && a.x==11 && b.x==11 && c.x==12 && d.x==12 && a.y==20 && b.y==20 && c.y==20 && d.y==20 && a.z==33 && b.z==33 && c.z==33 && d.z==33 ? 0 : 1; }
