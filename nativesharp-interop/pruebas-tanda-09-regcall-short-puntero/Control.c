#include "api.h"
int main(void) { short value=7; T09PtrCallback p=t09_get_callback(); T09VoidCallback v=t09_get_void(); short* first=t09_regcall(&value,1); short* result=p(first,3); v(result,4u); return first==&value && result==&value && value==15 ? 0 : 1; }
