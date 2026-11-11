#include "api.h"
_Static_assert((T09Color)-1 > (T09Color)0, "expected unsigned enum representation");
_Static_assert(sizeof(T09Color)==4, "expected 32-bit enum representation");
__declspec(selectany) int _fltused=0;
T09Color __attribute__((sysv_abi)) sysv_enum_unsigned_record(float factor, T09Byte first, T09Wide wide, T09Color code, int n) { if (!(factor==1.5f && first.x==11 && wide.x==1.5f && wide.y==2.5f && n==7)) return T09B; first.x=0; wide.x=0; return code; }
