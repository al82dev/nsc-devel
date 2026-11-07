extern __declspec(thread) int* tls_pointer;
extern __declspec(thread) double* tls_double_pointer;
extern __declspec(thread) const int* tls_readonly_pointer;
extern __declspec(thread) volatile int* tls_volatile_pointer;
extern __declspec(thread) int* volatile tls_volatile_slot;
extern __declspec(thread) int* const tls_const_slot;
int* pointer_target(void);
double* pointer_double_target(void);
void pointer_prepare(void);
int pointer_check(void);
int pointer_other_thread(void);
#include "Types.h"
extern __declspec(thread) TlsRecord tls_record;
extern __declspec(thread) TlsRecord* tls_record_pointer;
extern __declspec(thread) TlsRecord tls_records[2];
extern __declspec(thread) TlsMode tls_mode;
extern __declspec(thread) TlsMode* tls_mode_pointer;
extern __declspec(thread) TlsCallback tls_callback;
TlsCallback pointer_callback(void);
