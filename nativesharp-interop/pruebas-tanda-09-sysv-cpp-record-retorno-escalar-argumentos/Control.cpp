#include "api.h"
extern "C" { int _fltused = 0; }
int main() {
 T09Cpp::Small s{10,20,30,40,50,60}; T09Cpp::Other t{100,200,300,400,500};
 auto callback=T09Cpp::fetchCallback(); int a=T09Cpp::most(s,(signed char)-7,(unsigned char)250,(short)-1000,(unsigned short)65000,true,4000000000U,-5000000000LL,9000000000ULL,1.5f,2.25,t); int b=callback(s,(signed char)-7,(unsigned char)250,(short)-1000,(unsigned short)65000,false,4000000000U,-5000000000LL,9000000000ULL,1.5f,2.25,t);
 return callback==&T09Cpp::most && s.x==10 && s.y==20 && s.z==30 && s.w==40 && s.v==50 && s.u==60 &&
 t.x==100 && t.y==200 && t.z==300 && t.w==400 && t.v==500 && a==12 && b==11 ? 0 : 1;
}
