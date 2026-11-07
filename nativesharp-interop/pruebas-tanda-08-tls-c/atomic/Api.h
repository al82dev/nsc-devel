extern _Thread_local _Atomic(int) atomic_tls;
int atomic_tls_read(void);
int atomic_start_worker(void);
int atomic_wait_worker(void);
