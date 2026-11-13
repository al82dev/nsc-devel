#include "api.h"
__declspec(selectany) int _fltused = 0;
int main(void) { T09SC a=t09_get_sc(); T09UC b=t09_get_uc(); T09SH c=t09_get_sh(); T09US d=t09_get_us();
return t09_regcall(-7)==-1007 && a(-7)==-1007 && t09_uc(250)==4000000250u && b(250)==4000000250u && t09_sh(-6)==-1099511627782LL && c(-6)==-1099511627782LL && t09_us(2.5f)==3.75 && d(2.5f)==3.75 ? 0 : 1; }
