extern "C" { __declspec(selectany) int _fltused = 0; }
#include "api.h"
int main() { T09Cpp::Small s{10,20,30}; T09Cpp::Small q{2,3,4}; auto callback=T09Cpp::fetchCallback(); auto a=T09Cpp::most(s,5,q); auto b=callback(s,5,q);
return callback==&T09Cpp::most && s.x==10 && s.y==20 && s.z==30 && q.x==2 && q.y==3 && q.z==4 && a.x==12 && a.y==20 && a.z==39 && b.x==12 && b.y==20 && b.z==39 ? 0 : 1; }
