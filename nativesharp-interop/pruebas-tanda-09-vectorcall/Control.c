#include "api.h"
#ifdef API_DLL_IMPORT
__declspec(selectany) int _fltused = 0;
#endif
int main(void) {
    H2 value; value.x = 1.5f; value.y = 3.0f;
    H2 result = transform_h2(value, 2);
    if (value.x != 1.5f || value.y != 3.0f) return 1;
    if (result.x != 3.0f || result.y != 5.0f) return 2;
    return 0;
}
