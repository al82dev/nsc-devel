#include "api.h"
extern "C" { __declspec(selectany) int _fltused = 0; }
int main() { int v=3; if(T09Cpp::scalar(-3.5f,&v)!=10 || v!=10) return 1; auto callback=T09Cpp::fetchCallback(); if(callback(1.25f,&v)!=8 || v!=8) return 2; return 0; }
