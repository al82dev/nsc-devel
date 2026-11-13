#include "api.h"
__declspec(selectany) int _fltused = 0;
int main(void) { T09MostCallback m=t09_get_most(); T09AllCallback a=t09_get_all(); int value=7;
return m==t09_most && a==t09_all && m(2.25,&value,-7,1)==9.5 && value==19 && a(-5000000000LL,4000000000u,3.5f)==-4999999990LL ? 0 : 1; }
