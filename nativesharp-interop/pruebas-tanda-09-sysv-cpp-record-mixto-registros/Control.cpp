#include "api.h"
extern "C" { int _fltused = 0; }
int main() {
 T09Cpp::Small s{10.0,20}; T09Cpp::Other t{100,200,300,400,500};
 auto callback=T09Cpp::fetchCallback(); int a=T09Cpp::most(s,7,t); int b=callback(s,7,t);
 return callback==&T09Cpp::most && s.x==10.0 && s.y==20 &&
 t.x==100 && t.y==200 && t.z==300 && t.w==400 && t.v==500 && a==417 && b==417 ? 0 : 1;
}