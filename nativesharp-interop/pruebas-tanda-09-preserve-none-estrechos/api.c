#include "api.h"
signed char __attribute__((preserve_none)) t09_regcall(signed char x) { return x + 1; }
unsigned char __attribute__((preserve_none)) t09_uc(unsigned char x) { return x + 1; }
short __attribute__((preserve_none)) t09_sh(short x) { return x + 1; }
unsigned short __attribute__((preserve_none)) t09_us(unsigned short x) { return x + 1; }
T09SC t09_get_sc(void) { return t09_regcall; }
T09UC t09_get_uc(void) { return t09_uc; }
T09SH t09_get_sh(void) { return t09_sh; }
T09US t09_get_us(void) { return t09_us; }
