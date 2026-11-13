#include "api.h"
int main() {
 T09Cpp::Small s{10,20,30,40,50,60}; auto callback=T09Cpp::fetchCallback();
 auto a=T09Cpp::most(s,-7,250,-1000,65000,true); auto b=callback(s,-7,250,-1000,65000,false);
 return callback==&T09Cpp::most && s.x==10 && s.y==20 && s.z==30 && s.w==40 && s.v==50 && s.u==60 &&
 a.x==11 && a.y==22 && a.z==33 && a.w==44 && a.v==55 && a.u==66 &&
 b.x==11 && b.y==22 && b.z==33 && b.w==44 && b.v==55 && b.u==67 ? 0 : 1;
}