typedef struct FlexConst { int n; const int data[]; } FlexConst;
typedef struct FlexVolatile { int n; volatile int data[]; } FlexVolatile;
#pragma pack(push, 1)
typedef struct FlexPacked { char tag; int data[]; } FlexPacked;
#pragma pack(pop)
FlexConst* flex_const(void);
FlexVolatile* flex_volatile(void);
int flex_volatile_get(const FlexVolatile* value, int index);
void flex_volatile_set(FlexVolatile* value, int index, int element);
int flex_volatile_pointer(volatile int* value);
FlexPacked* flex_packed(void);
FlexPacked flex_packed_value(void);
int flex_packed_take(FlexPacked value);
int flex_packed_get(const FlexPacked* value, int index);
void flex_packed_set(FlexPacked* value, int index, int element);
