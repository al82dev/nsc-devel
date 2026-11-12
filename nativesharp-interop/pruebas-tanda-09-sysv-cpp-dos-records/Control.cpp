#include "api.h"
int main() {
 T09Cpp::Small s{10,20,30,40,50,60}, t{100,200,300,400,500,600};
 auto callback=T09Cpp::fetchCallback();
 auto a=T09Cpp::most(s,7,t); auto b=callback(s,7,t); auto c=callback(s,7,s);
 return callback==&T09Cpp::most && s.x==10 && s.y==20 && s.z==30 && s.w==40 && s.v==50 && s.u==60 &&
 t.x==100 && t.y==200 && t.z==300 && t.w==400 && t.v==500 && t.u==600 &&
 a.x==110 && a.y==27 && a.z==330 && a.w==44 && a.v==55 && a.u==66 &&
 b.x==110 && b.y==27 && b.z==330 && b.w==44 && b.v==55 && b.u==66 &&
 c.x==20 && c.y==27 && c.z==60 && c.w==44 && c.v==55 && c.u==66 ? 0 : 1;
}