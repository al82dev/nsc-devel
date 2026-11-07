typedef struct FlexConst { int n; const int data[]; } FlexConst;
#pragma pack(push, 1)
typedef struct FlexPacked { char tag; int data[]; } FlexPacked;
#pragma pack(pop)
FlexConst* flex_const(void);
FlexPacked* flex_packed(void);
FlexPacked flex_packed_value(void);
