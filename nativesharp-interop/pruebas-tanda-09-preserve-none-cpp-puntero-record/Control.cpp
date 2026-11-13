#include "api.h"
extern "C" { __declspec(selectany) int _fltused = 0; }
int main() { T09Cpp::Small v{3}; if(T09Cpp::scalar(-3.5f,&v)!=10 || v.value!=10) return 1; auto callback=T09Cpp::fetchCallback(); if(callback(1.25f,&v)!=8 || v.value!=8) return 2; return 0; }
