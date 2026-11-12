#include "api.h"
int main() {
 T09Cpp::Small s{10,20,30,40,50,60}; T09Cpp::Other t{4294967396LL,200,300};
 auto callback=T09Cpp::fetchCallback();
 auto a=T09Cpp::most(s,7,t); auto b=callback(s,7,t); auto c=callback(s,7,t);
 return callback==&T09Cpp::most && s.x==10 && s.y==20 && s.z==30 && s.w==40 && s.v==50 && s.u==60 &&
 t.x==4294967396 && t.y==200 && t.z==300 &&
 a.x==110 && a.y==37 && a.z==330 && a.w==44 && a.v==55 && a.u==66 &&
 b.x==110 && b.y==37 && b.z==330 && b.w==44 && b.v==55 && b.u==66 &&
 c.x==110 && c.y==37 && c.z==330 && c.w==44 && c.v==55 && c.u==66 ? 0 : 1;
}