#include "api.h"
__declspec(selectany) int _fltused = 0;
int main(void) { int value=7; double result=t09_most(2.25,&value,-7,1); return result==9.5 && value==19 && t09_all(-5000000000LL,4000000000u,3.5f)==-4999999990LL ? 0 : 1; }
