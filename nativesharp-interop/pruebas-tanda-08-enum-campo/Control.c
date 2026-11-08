#include "Api.h"
int main(void) {
    if (h4_anon.level != H4Low || h4_anon.other != H4High || h4_named.kind != H4NamedA) return 1;
    h4_anon.level = H4High; h4_anon.other = H4Low; h4_anon.changing = H4Changed; h4_named.kind = H4NamedB;
    return h4_check() ? 0 : 2;
}
