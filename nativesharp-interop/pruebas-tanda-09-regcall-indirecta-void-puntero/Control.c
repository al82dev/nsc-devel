#include "api.h"
int main(void) { int value=7; T09PtrCallback p=t09_get_callback(); T09VoidCallback v=t09_get_void(); int* result=p(&value,3); v(result,4u); return result==&value && value==14 ? 0 : 1; }
