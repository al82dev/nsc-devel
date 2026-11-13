#include "api.h"
extern "C" { int _fltused = 0; }
int main() {
 T09Cpp::Small s{10,20,30,40,50,60}; T09Cpp::Other t{100,200,300,400,500};
 auto callback=T09Cpp::fetchCallback(); short x=0,y=0; double* a=T09Cpp::most(s,&x,t); double* b=callback(s,&y,t); bool ok=x==310 && y==310 && a==b && *a==317.5; *a=55.25; ok=ok && *b==55.25;
 return callback==&T09Cpp::most && s.x==10 && s.y==20 && s.z==30 && s.w==40 && s.v==50 && s.u==60 &&
 t.x==100 && t.y==200 && t.z==300 && t.w==400 && t.v==500 && ok ? 0 : 1;
}