#include <stdarg.h>
#include <stdint.h>

typedef struct Pair { int32_t Left; int32_t Right; } Pair;
typedef int32_t (*BinaryCallback)(int32_t, int32_t);
typedef int32_t (*ComInvoke)(void *, int32_t);
typedef struct ComVTable { ComInvoke Invoke; } ComVTable;
typedef struct ComObject { ComVTable *VTable; int32_t Bias; } ComObject;

int32_t host_pair_sum(Pair value) { return value.Left + value.Right; }
Pair host_make_pair(int32_t left, int32_t right) { Pair value = { left, right }; return value; }
int32_t host_apply(BinaryCallback callback, int32_t left, int32_t right) { return callback(left, right); }
int32_t host_com_call(ComObject *value, int32_t argument) { return value->VTable->Invoke(value, argument); }

int32_t host_vararg_sum(int32_t count, ...)
{
    int32_t total = 0;
    va_list values;
    va_start(values, count);
    for (int32_t index = 0; index < count; ++index) { total += va_arg(values, int32_t); }
    va_end(values);
    return total;
}

int32_t __vectorcall host_vectorcall(int32_t a, int32_t b, int32_t c, int32_t d)
{
    return a + b + c + d;
}
