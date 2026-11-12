#include "api.h"
int _fltused=0;
int main(void) {
    PairMixed value={10.25,20.5,3.5}; int direct=0, indirect=0;
    write_pair(value,&direct); get_write()(value,&indirect);
    return !(direct==61 && indirect==61 && value.x==10.25f);
}
