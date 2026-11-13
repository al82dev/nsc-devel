#include "api.h"
int main() {
 T09Cpp::Small s{10,20,30,40,50,60}; T09Cpp::Other t{100,200,300,400,500};
 auto callback=T09Cpp::fetchCallback(); int value=7; void* original=&value; void* a=T09Cpp::most(s,original,t); void* b=callback(s,original,t); bool ok=a==original && b==a && value==627;
 return callback==&T09Cpp::most && s.x==10 && s.y==20 && s.z==30 && s.w==40 && s.v==50 && s.u==60 &&
 t.x==100 && t.y==200 && t.z==300 && t.w==400 && t.v==500 && ok ? 0 : 1;
}