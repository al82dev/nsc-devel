extern "C" { __declspec(selectany) int _fltused = 0; }
#include "api.h"
int main() { int value=30; T09Cpp::Other s{10,20,&value}; auto callback=T09Cpp::fetchCallback(); auto a=T09Cpp::most(s); auto b=callback(s); return callback==&T09Cpp::most && s.x==10 && s.y==20 && s.z==&value && value==32 && a.x==11 && a.y==20 && a.z==33 && b.x==11 && b.y==20 && b.z==34 ? 0 : 1; }
