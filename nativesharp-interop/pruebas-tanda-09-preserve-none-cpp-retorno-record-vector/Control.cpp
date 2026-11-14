#include "api.h"
extern "C" { __declspec(selectany) int _fltused = 0; }
int main() { T09Cpp::Small v; t09_init(&v); auto a=T09Cpp::scalar(-3.5f,v); if(!t09_check(&a,10) || !t09_check(&v,3)) return 1; auto callback=T09Cpp::fetchCallback(); auto b=callback(1.25f,v); if(!t09_check(&b,1) || !t09_check(&v,3)) return 2; return 0; }
