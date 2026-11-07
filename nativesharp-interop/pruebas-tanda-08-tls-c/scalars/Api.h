extern __declspec(thread) unsigned int tls_unsigned;
extern __declspec(thread) short tls_short;
extern __declspec(thread) double tls_double;
extern __declspec(thread) _Bool tls_bool;
extern __declspec(thread) const int tls_const;
extern __declspec(thread) volatile int tls_volatile;
int scalar_check(void);
int scalar_thread_check(void);
