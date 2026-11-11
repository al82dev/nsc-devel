#include "api.h"
__declspec(selectany) int _fltused=0;
int main(void) { T09SysvAlignedMix first={1,2,3}; T09SysvAlignedMix second={31,32,33}; T09SysvAlignedMix result=sysv_aligned_mix(1.5f,first,7,second); if(first.x!=1 || second.y!=32 || result.x!=39 || result.y!=35 || result.z!=3) return 1; return 0; }
