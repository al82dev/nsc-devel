#include "api.h"
extern "C" { __declspec(selectany) int _fltused = 0; }
int main() { T09Cpp::Small a{3}; T09Cpp::Large b{5,7,11}; if(T09Cpp::scalar(-3.5f,a,b)!=131 || a.value!=3 || b.x!=5 || b.y!=7 || b.z!=11) return 1; auto callback=T09Cpp::fetchCallback(); if(callback(1.25f,a,b)!=231 || a.value!=3 || b.x!=5 || b.y!=7 || b.z!=11) return 2; return 0; }
