#pragma once
typedef struct T09OnlyResult { int x, y, z; } T09OnlyResult;
typedef T09OnlyResult (__attribute__((sysv_abi)) *T09OnlyResultCallback)(int);
T09OnlyResultCallback t09_get_only_result_callback(void);
