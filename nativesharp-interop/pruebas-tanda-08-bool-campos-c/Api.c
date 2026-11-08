#include "Api.h"
static BoolFields fields;
static BoolPacked packed;
static BoolBytes bytes;
BoolFields* bool_fields(void) { return &fields; }
BoolPacked* bool_packed(void) { return &packed; }
BoolBytes* bool_bytes(void) { return &bytes; }
long long bool_layout(int id)
{
    switch (id)
    {
    case 1: return (long long)sizeof(fields.flag);
    case 2: return (long long)sizeof(BoolFields);
    case 3: return (long long)__builtin_offsetof(BoolFields, flag);
    case 4: return (long long)__builtin_offsetof(BoolFields, values);
    case 5: return (long long)sizeof(BoolPacked);
    case 6: return (long long)__builtin_offsetof(BoolPacked, last);
    case 7: return (long long)sizeof(BoolBytes);
    default: return -1;
    }
}
void bool_reset(void)
{
    fields.tag = 0x5A; fields.flag = 0; fields.values[0] = 0; fields.values[1] = 0; fields.values[2] = 0;
    fields.after = 0x12345678;
    packed.tag = 0x5A; packed.value = 0x0BADF00D; packed.last = 0;
    bytes.raw = 0;
}
/* Raw byte as stored in memory, read through unsigned char (object representation). */
int bool_raw(int id)
{
    switch (id)
    {
    case 1: return *(const unsigned char*)&fields.flag;
    case 2: return *(const unsigned char*)&fields.values[0];
    case 3: return *(const unsigned char*)&fields.values[1];
    case 4: return *(const unsigned char*)&fields.values[2];
    case 5: return *(const unsigned char*)&packed.last;
    case 6: return bytes.raw;
    default: return -1;
    }
}
int bool_neighbors_intact(void)
{
    return fields.tag == 0x5A && fields.after == 0x12345678 && packed.tag == 0x5A && packed.value == 0x0BADF00D;
}
void bool_c_store(int id, _Bool value)
{
    switch (id)
    {
    case 1: fields.flag = value; break;
    case 2: fields.values[0] = value; break;
    case 3: fields.values[1] = value; break;
    case 4: fields.values[2] = value; break;
    case 5: packed.last = value; break;
    case 6: bytes.flag = value; break;
    default: break;
    }
}
int bool_c_load(int id)
{
    switch (id)
    {
    case 1: return fields.flag ? 1 : 0;
    case 2: return fields.values[0] ? 1 : 0;
    case 3: return fields.values[1] ? 1 : 0;
    case 4: return fields.values[2] ? 1 : 0;
    case 5: return packed.last ? 1 : 0;
    case 6: return bytes.flag ? 1 : 0;
    default: return -1;
    }
}
_Bool acc_get_flag(const BoolFields* p) { return p->flag; }
void acc_set_flag(BoolFields* p, _Bool value) { p->flag = value; }
_Bool acc_get_value(const BoolFields* p, int index) { return p->values[index]; }
void acc_set_value(BoolFields* p, int index, _Bool value) { p->values[index] = value; }
_Bool acc_get_packed(const BoolPacked* p) { return p->last; }
void acc_set_packed(BoolPacked* p, _Bool value) { p->last = value; }
_Bool acc_get_union(const BoolBytes* p) { return p->flag; }
void acc_set_union(BoolBytes* p, _Bool value) { p->flag = value; }
_Bool bool_global;
_Bool bool_global_array[2];
static BoolVolatile volatile_fields;
BoolVolatile* bool_volatile(void) { return &volatile_fields; }
_Bool acc_get_volatile(BoolVolatile* p) { return p->flag; }
void acc_set_volatile(BoolVolatile* p, _Bool value) { p->flag = value; }
