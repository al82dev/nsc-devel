#include "api.h"
extern "C" { __declspec(selectany) int _fltused = 0; }
int main() { int v=0; t09_p(&v); return t09_f(2.5f)==3.5f && t09_i(-6)==-7 && t09_u(4000000000u)==4000000001u && t09_l(-1099511627775ll)==-1099511627776ll && t09_ul(~0ull-1ull)==~0ull && t09_d(2.25)==3.25 && t09_b(false) && !t09_b(true) && v==27 ? 0 : 1; }
