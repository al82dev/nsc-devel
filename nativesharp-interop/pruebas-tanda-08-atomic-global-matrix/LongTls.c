#include "LongTls.h"
_Thread_local _Atomic(long) tls_clong = -4;
_Thread_local _Atomic(unsigned long) tls_culong = 5;
int verify_long_tls(void) { return tls_clong == -40 && tls_culong == 41; }
