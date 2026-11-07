extern __declspec(thread) int tls_zero;
extern __declspec(thread) volatile int tls_volatile;
extern __declspec(thread) const int tls_const;
extern __declspec(thread) unsigned int tls_unsigned;
extern __declspec(thread) double tls_double;
extern __declspec(thread) _Bool tls_bool;
__declspec(dllimport) int tls_scalar_check(void);
__declspec(dllimport) int tls_zero_read(void);
