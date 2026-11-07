extern __declspec(thread) int tls_array[2];
extern __declspec(thread) int tls_three[3];
extern __declspec(thread) int tls_matrix[2][3];
int array_check(void);
int array_other_thread(void);
