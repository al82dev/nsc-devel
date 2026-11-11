#include "api.h"
__declspec(selectany) int _fltused = 0;
int main(void) { T09SC a=t09_get_sc(); T09UC b=t09_get_uc(); T09SH c=t09_get_sh(); T09US d=t09_get_us();
return t09_regcall(2.5f)==3.5f && a(2.5f)==3.5f && t09_uc(4000000000u)==4000000001u && b(4000000000u)==4000000001u && t09_sh(18446744073709551614ULL)==18446744073709551615ULL && c(18446744073709551614ULL)==18446744073709551615ULL && t09_us(2.25)==3.25 && d(2.25)==3.25 ? 0 : 1; }
