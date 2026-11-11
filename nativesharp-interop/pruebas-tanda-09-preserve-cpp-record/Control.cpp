#include "api.h"
int main() { T09Cpp::Small s{10,20}; auto callback=T09Cpp::fetchCallback(); auto a=T09Cpp::most(s); auto b=callback(s);
return callback==&T09Cpp::most && s.x==10 && s.y==20 && a.x==11 && a.y==20 && b.x==11 && b.y==20 ? 0 : 1; }
