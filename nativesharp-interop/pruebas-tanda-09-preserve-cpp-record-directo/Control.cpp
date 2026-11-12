#include "api.h"
int main() { T09Cpp::Small s{10,20}; auto a=T09Cpp::most(s);
return s.x==10 && s.y==20 && a.x==11 && a.y==20 ? 0 : 1; }
