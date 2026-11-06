typedef struct VolatileBits {
    volatile unsigned int first : 3;
    volatile signed int delta : 5;
} VolatileBits;
#pragma pack(push, 1)
typedef struct PackedVolatileBits {
    char prefix;
    volatile unsigned int first : 4;
    volatile signed int delta : 4;
} PackedVolatileBits;
#pragma pack(pop)
VolatileBits* get_volatile_bits(void);
PackedVolatileBits* get_packed_volatile_bits(void);
void init_volatile_bits(VolatileBits* value, PackedVolatileBits* packed);
int check_volatile_bits(VolatileBits* value, PackedVolatileBits* packed);
int read_volatile_first(VolatileBits* value);
void write_volatile_first(VolatileBits* value, unsigned int number);
int read_packed_volatile_first(PackedVolatileBits* value);
void write_packed_volatile_first(PackedVolatileBits* value, unsigned int number);
