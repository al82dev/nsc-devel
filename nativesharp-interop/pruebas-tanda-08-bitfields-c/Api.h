typedef struct Bits {
    unsigned int first : 3;
    signed int delta : 5;
    unsigned int : 0;
    unsigned int second : 6;
    unsigned int third : 4;
} Bits;
#pragma pack(push, 1)
typedef struct PackedBits {
    char prefix;
    unsigned int small : 4;
    signed int negative : 4;
} PackedBits;
#pragma pack(pop)
Bits* get_bits(void);
PackedBits* get_packed_bits(void);
void init_bits(Bits* value, PackedBits* packed);
int check_bits(Bits* value, PackedBits* packed);
int bits_size(void);
int packed_bits_size(void);
