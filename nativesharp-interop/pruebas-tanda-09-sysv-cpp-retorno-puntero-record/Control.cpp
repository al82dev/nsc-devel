#include "api.h"
int main() {
 T09Cpp::Small s{10,20,30,40,50,60}; T09Cpp::Other t{100,200,300,400,500};
 T09Cpp::Small out{5,6,7,8,9,10}; auto callback=T09Cpp::fetchCallback(); T09Cpp::Small* a=T09Cpp::most(s,&out,t); T09Cpp::Small* b=callback(s,&out,t);
 return callback==&T09Cpp::most && s.x==10 && s.y==20 && s.z==30 && s.w==40 && s.v==50 && s.u==60 &&
 t.x==100 && t.y==200 && t.z==300 && t.w==400 && t.v==500 && out.x==625 && out.y==6 && out.z==7 && out.w==8 && out.v==9 && out.u==10 && a==&out && b==a && a->x==625 ? 0 : 1;
}