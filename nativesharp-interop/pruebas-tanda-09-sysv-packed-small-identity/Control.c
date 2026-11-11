#include "api.h"
__declspec(selectany) int _fltused=0;
int main(void) { T09Packed original={7,3.5}; T09Packed result=sysv_packed_small_identity(original); if(original.tag!=7 || original.value!=3.5 || result.tag!=8 || result.value!=4.0) return 1; return 0; }
