extern "C" { __declspec(selectany) int _fltused = 0; }
#include "api.h"
int main() { T09Cpp::Small s{10,20,30}; auto callback=T09Cpp::fetchCallback(); auto a=T09Cpp::most(s,5,2.0f); auto b=callback(s,5,2.0f);
return callback==&T09Cpp::most && s.x==10 && s.y==20 && s.z==30 && a.x==15 && a.y==20 && a.z==32 && b.x==15 && b.y==20 && b.z==32 ? 0 : 1; }
