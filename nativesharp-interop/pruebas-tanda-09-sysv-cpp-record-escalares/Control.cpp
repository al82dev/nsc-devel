#include "api.h"
int main() {
 T09Cpp::Small s{10,20,30,40,50,60}; int value=5; auto callback=T09Cpp::fetchCallback();
 auto a=T09Cpp::most(s,4000000000U,-5000000000LL,5000000000ULL,&value);
 auto b=callback(s,4000000000U,-5000000000LL,5000000000ULL,&value);
 return callback==&T09Cpp::most && value==13 && s.x==10 && s.y==20 && s.z==30 && s.w==40 && s.v==50 && s.u==60 &&
 a.x==11 && a.y==22 && a.z==33 && a.w==40 && a.v==50 && a.u==60 &&
 b.x==11 && b.y==22 && b.z==33 && b.w==40 && b.v==50 && b.u==60 ? 0 : 1;
}