#include "api.h"
extern "C" { __declspec(selectany) int _fltused = 0; }
int main() { T09Cpp::Small v{3,11}; if(T09Cpp::scalar(-3.5f,v)!=21 || v.value!=3 || v.other!=11) return 1; auto callback=T09Cpp::fetchCallback(); if(callback(1.25f,v)!=12 || v.value!=3 || v.other!=11) return 2; return 0; }
