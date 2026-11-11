#include "api.h"
__declspec(selectany) int _fltused=0;
int main(void) { T09Packed original={7,3.5}; int result=sysv_packed_small_scalar(1.5f,original,2); if(original.tag!=7 || original.value!=3.5 || result!=13) return 1; return 0; }
