extern __declspec(thread) int tls_array[2];
__declspec(dllimport) int array_check(void);
__declspec(dllimport) int array_other_thread(void);
