#include "api.h"
int main() { T09Cpp::Small s{10,20,30,40,50,60}; T09Cpp::Extra out{5,6,7,8,9,10}; auto callback=T09Cpp::fetchCallback(); auto a=T09Cpp::most(s,&out); auto b=callback(s,&out);
return callback==&T09Cpp::most && s.x==10 && s.y==20 && s.z==30 && s.w==40 && s.v==50 && s.u==60 && out.x==25 && out.y==6 && out.z==7 && out.w==8 && out.v==9 && out.u==10 && a.x==10 && a.y==35 && a.z==30 && a.w==40 && a.v==50 && a.u==60 && b.x==10 && b.y==45 && b.z==30 && b.w==40 && b.v==50 && b.u==60 ? 0 : 1; }
