#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
API_STORAGE __attribute__((sysv_abi)) int sysv_varargs(int count, ...);
API_STORAGE __attribute__((sysv_abi)) int sysv_many(int count, ...);
typedef struct T09VarRecord { int x; int y; } T09VarRecord;
typedef struct T09VarMixed { double x; int y; } T09VarMixed;
typedef struct T09VarLarge { long long x; long long y; long long z; } T09VarLarge;
typedef struct __attribute__((packed)) T09VarPacked { double x; int y; } T09VarPacked;
typedef struct __attribute__((packed, aligned(2))) T09VarUnobserved { double x; int y; } T09VarUnobserved;
typedef struct __attribute__((packed)) T09VarPackedValue { signed char tag; double value; } T09VarPackedValue;
typedef struct T09VarArray { double values[2]; } T09VarArray;
typedef struct T09VarNestLeaf { int x; int y; } T09VarNestLeaf;
typedef struct T09VarNested { double value; struct T09VarNestLeaf leaf; } T09VarNested;
typedef double T09ScalarAlias;
typedef T09ScalarAlias T09ScalarAlias2;
typedef struct T09VarNestLeaf T09LeafAlias;
typedef struct T09VarAliasFields { T09ScalarAlias2 value; T09LeafAlias leaf; } T09VarAliasFields;
typedef int T09AlignedInt __attribute__((aligned(8)));
typedef struct T09BadAliasFields { T09AlignedInt value; } T09BadAliasFields;
typedef struct T09VarRecordArray { struct T09VarNestLeaf leaves[2]; } T09VarRecordArray;
typedef struct T09VarRecordArrayLarge { struct T09VarNestLeaf leaves[3]; } T09VarRecordArrayLarge;
typedef union T09VarChoice { double real; long long integer; } T09VarChoice;
typedef struct T09VarUnionOuter { double value; union T09VarChoice choice; } T09VarUnionOuter;
typedef struct T09VarBits { unsigned int low : 3; unsigned int high : 5; unsigned int middle : 8; unsigned int upper : 16; } T09VarBits;
typedef struct __attribute__((packed, aligned(2))) T09BadInner { int x; } T09BadInner;
typedef struct T09BadOuter { double value; struct T09BadInner inner; } T09BadOuter;
API_STORAGE __attribute__((sysv_abi)) int sysv_ptrs(int count, ...);
API_STORAGE __attribute__((sysv_abi)) int sysv_empty(int count, ...);
API_STORAGE __attribute__((sysv_abi)) int sysv_floatspill(int count, ...);
API_STORAGE __attribute__((sysv_abi)) int sysv_fixed_ptrs(T09VarRecord *record, short *value, ...);
API_STORAGE __attribute__((sysv_abi)) T09VarRecord *sysv_return_ptr(T09VarRecord *record, ...);
API_STORAGE __attribute__((sysv_abi)) int sysv_record_small(int count, ...);
API_STORAGE __attribute__((sysv_abi)) int sysv_record_mixed(int count, ...);
API_STORAGE __attribute__((sysv_abi)) int sysv_record_two(int count, ...);
API_STORAGE __attribute__((sysv_abi)) int sysv_record_pressure(int count, ...);
API_STORAGE __attribute__((sysv_abi)) int sysv_record_sse_pressure(int count, ...);
API_STORAGE __attribute__((sysv_abi)) int sysv_record_large(int count, ...);
API_STORAGE __attribute__((sysv_abi)) int sysv_record_ptrs(int *fixed, ...);
API_STORAGE __attribute__((sysv_abi)) int *sysv_record_return_ptr(int *fixed, ...);
API_STORAGE __attribute__((sysv_abi)) int sysv_record_rptr(int count, ...);
API_STORAGE __attribute__((sysv_abi)) T09VarRecord *sysv_record_fixed_rptr(T09VarRecord *fixed, ...);
API_STORAGE __attribute__((sysv_abi)) int sysv_record_packed(int count, ...);
API_STORAGE __attribute__((sysv_abi)) int sysv_record_packed_ptr(int count, ...);
API_STORAGE __attribute__((sysv_abi)) int sysv_record_array(int count, ...);
API_STORAGE __attribute__((sysv_abi)) int sysv_record_nested(int count, ...);
API_STORAGE __attribute__((sysv_abi)) int sysv_record_record_array(int count, ...);
API_STORAGE __attribute__((sysv_abi)) int sysv_record_record_array_large(int count, ...);
API_STORAGE __attribute__((sysv_abi)) int sysv_record_union_outer(int count, ...);
API_STORAGE __attribute__((sysv_abi)) int sysv_record_bits(int count, ...);
