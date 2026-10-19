#pragma once

#include <string>

namespace nativesharp::language
{
struct FutureProtocol final
{
    std::string valueType;
    std::string errorType;
    std::string cancellationType;
    // NS304-T05-F02: tipo del Waker (callback de readiness del executor). Vacio = sin Waker
    // (el Future solo se sondea por busqueda activa); el lowering productivo (T05-F03) lo fija.
    std::string wakerType;
    bool hasPoll = true;
    bool hasResume = true;
    bool singleConsumer = true;
    bool staticDispatch = true;
};
}
