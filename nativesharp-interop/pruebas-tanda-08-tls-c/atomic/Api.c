#include "Api.h"
_Thread_local _Atomic(int) atomic_tls = 3;
int atomic_tls_read(void) { return atomic_tls; }
