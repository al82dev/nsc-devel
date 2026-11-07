extern _Thread_local _Atomic(int) atomic_tls;
extern _Thread_local const _Atomic(int) atomic_readonly;
extern _Thread_local volatile _Atomic(int) atomic_volatile;
int atomic_tls_read(void);
int atomic_volatile_read(void);
int atomic_start_worker(void);
int atomic_wait_worker(void);
