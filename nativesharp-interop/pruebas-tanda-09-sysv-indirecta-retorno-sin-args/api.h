#pragma once
typedef struct T09NoArgsResult { int x, y, z; } T09NoArgsResult;
typedef T09NoArgsResult (__attribute__((sysv_abi)) *T09NoArgsCallback)(void);
T09NoArgsCallback t09_get_noargs_callback(void);
