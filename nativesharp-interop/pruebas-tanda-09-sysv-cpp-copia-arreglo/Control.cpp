extern "C" { __declspec(selectany) int _fltused = 0; }
#include "api.h"
int main() { T09Cpp::Other s{{10,20,30},40}; auto callback=T09Cpp::fetchCallback(); auto a=T09Cpp::most(s); auto b=callback(s); return callback==&T09Cpp::most && s.x[0]==10 && s.x[1]==20 && s.x[2]==30 && s.y==40 && a.x==11 && a.y==20 && a.z==33 && b.x==11 && b.y==20 && b.z==33 ? 0 : 1; }
