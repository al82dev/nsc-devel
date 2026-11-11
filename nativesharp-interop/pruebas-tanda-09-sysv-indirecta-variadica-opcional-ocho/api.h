#pragma once
typedef struct T09IndirectVarRecord { double x; int y; } T09IndirectVarRecord;
typedef struct T09OtherVarRecord { int a; int b; } T09OtherVarRecord;
typedef int (__attribute__((sysv_abi)) *T09IndirectVarCallback)(T09IndirectVarRecord, ...);
T09IndirectVarCallback t09_get_var_record_callback(void);
