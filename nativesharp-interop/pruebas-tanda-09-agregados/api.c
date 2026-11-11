#include "api.h"
R1 pass1(R1 value) { value.x += 10; return value; }
R2 pass2(R2 value) { value.x += 10; return value; }
U4 pass_union(U4 value) { value.x += 10; return value; }
R3 pass3(R3 value) { value.x += 10; return value; }
R4 pass4(R4 value) { value.x += 10; return value; }
R8 pass8(R8 value) { value.x += 10; return value; }
R24 pass24(R24 value) { value.a += 10; return value; }
