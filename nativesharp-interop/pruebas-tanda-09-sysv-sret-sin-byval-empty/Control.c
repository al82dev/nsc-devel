#include "api.h"
__declspec(selectany) int _fltused=0;
int main(void) {  T09Result result=sysv_sret_empty(); if (result.x!=10 || result.y!=7 || result.z!=15) return 1; return 0; }
