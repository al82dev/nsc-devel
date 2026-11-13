#include "api.h"
extern "C" { __declspec(selectany) int _fltused = 0; }
int main() { T09Cpp::Small v{3,11,7}; if(T09Cpp::scalar(-3.5f,v)!=28 || v.value!=3 || v.other!=11 || v.third!=7) return 1; auto callback=T09Cpp::fetchCallback(); if(callback(1.25f,v)!=19 || v.value!=3 || v.other!=11 || v.third!=7) return 2; return 0; }
