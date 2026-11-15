#include "../Internal/DriverInternal.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#endif

namespace nativesharp::driver
{
// EN: LKF-263: the case folding of a path key follows the file system, not the bytes of its UTF-8 spelling.
//     NTFS compares names through its upper-case table, which covers non-ASCII letters; the invariant
//     upper-case mapping of Windows is the closest public equivalent. Elsewhere the text is kept as is.
// ES: LKF-263: el plegado de mayúsculas de una clave de ruta sigue al sistema de archivos, no a los bytes de
//     su escritura UTF-8. NTFS compara nombres con su tabla de mayúsculas, que cubre letras no ASCII; el
//     mapeo invariante de Windows es el equivalente público más cercano. En otros sistemas se conserva.
std::wstring upperCasePathText(std::wstring text)
{
#if defined(_WIN32)
    if (text.empty() || text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        return text;
    std::wstring upper(text.size(), L'\0');
    const int length = static_cast<int>(text.size());
    if (LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_UPPERCASE, text.data(), length, upper.data(), length, nullptr,
                      nullptr, 0) == length)
        return upper;
#endif
    return text;
}
} // namespace nativesharp::driver
