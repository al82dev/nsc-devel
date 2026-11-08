typedef struct BoolFields { char tag; _Bool flag; _Bool values[3]; int after; } BoolFields;
#pragma pack(push, 1)
typedef struct BoolPacked { char tag; int value; _Bool last; } BoolPacked;
#pragma pack(pop)
typedef union BoolBytes { _Bool flag; unsigned char raw; } BoolBytes;
BoolFields* bool_fields(void);
BoolPacked* bool_packed(void);
BoolBytes* bool_bytes(void);
long long bool_layout(int id);
void bool_reset(void);
int bool_raw(int id);
int bool_neighbors_intact(void);
void bool_c_store(int id, _Bool value);
int bool_c_load(int id);
_Bool acc_get_flag(const BoolFields* p);
void acc_set_flag(BoolFields* p, _Bool value);
_Bool acc_get_value(const BoolFields* p, int index);
void acc_set_value(BoolFields* p, int index, _Bool value);
_Bool acc_get_packed(const BoolPacked* p);
void acc_set_packed(BoolPacked* p, _Bool value);
_Bool acc_get_union(const BoolBytes* p);
void acc_set_union(BoolBytes* p, _Bool value);
/* Probe only: non-record C _Bool globals (outside T08-006 record fields). */
extern _Bool bool_global;
extern _Bool bool_global_array[2];
/* Probe only: volatile _Bool record field (outside the T08-006 patch). */
typedef struct BoolVolatile { char tag; volatile _Bool flag; } BoolVolatile;
BoolVolatile* bool_volatile(void);
_Bool acc_get_volatile(BoolVolatile* p);
void acc_set_volatile(BoolVolatile* p, _Bool value);
