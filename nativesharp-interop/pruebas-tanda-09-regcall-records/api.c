#include "api.h"
T09Small __attribute__((regcall)) t09_most(T09Small p) { p.x+=1; return p; }
T09Large __attribute__((regcall)) t09_most_void(T09Large p) { p.a+=2; return p; }
T09Small __attribute__((regcall)) t09_all_ptr(T09Small p) { p.x+=3; return p; }
T09Large __attribute__((regcall)) t09_all(T09Large p) { p.a+=4; return p; }
MostPtr t09_get_most(void) { return t09_most; }
MostVoid t09_get_most_void(void) { return t09_most_void; }
AllPtr t09_get_all_ptr(void) { return t09_all_ptr; }
AllVoid t09_get_all(void) { return t09_all; }
