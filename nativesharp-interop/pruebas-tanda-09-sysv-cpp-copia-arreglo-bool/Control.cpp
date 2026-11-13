extern "C" { __declspec(selectany) int _fltused = 0; }
#include "api.h"
int main() { T09Cpp::Other s{{true,false,true},40,30}; auto callback=T09Cpp::fetchCallback(); auto a=T09Cpp::most(s); auto b=callback(s); bool kept=s.x[0]; s.x[0]=false; s.x[1]=true; auto c=T09Cpp::most(s); auto d=callback(s); return callback==&T09Cpp::most && kept && !s.x[0] && s.x[1] && s.x[2] && s.y==40 && s.z==30 && a.x==11 && a.y==20 && a.z==33 && b.x==11 && b.y==20 && b.z==33 && c.x==12 && c.y==21 && c.z==33 && d.x==12 && d.y==21 && d.z==33 ? 0 : 1; }
