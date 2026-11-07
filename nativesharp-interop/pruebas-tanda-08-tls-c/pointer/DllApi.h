extern __declspec(thread) int* tls_pointer;
__declspec(dllimport) int* pointer_target(void);
__declspec(dllimport) int pointer_check(void);
__declspec(dllimport) int pointer_other_thread(void);
