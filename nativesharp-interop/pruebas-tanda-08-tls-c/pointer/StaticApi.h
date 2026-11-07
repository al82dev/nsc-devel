extern __declspec(thread) int* tls_pointer;
int* pointer_target(void);
int pointer_check(void);
int pointer_other_thread(void);
