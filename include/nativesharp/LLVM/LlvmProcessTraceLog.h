#pragma once

#include <cstddef>
#include <string>

namespace nativesharp::llvm_backend
{
// Separacion de la salida de error de un hijo observado. `diagnostics` contiene exclusivamente lo
// que se escribio FUERA de las ventanas de LLVM; `passLog` contiene las ventanas sin la sonda.
// De esta manera el supervisor puede ocultar miles de lineas de instrumentacion en una corrida
// sana sin tragarse un diagnostico del compilador.
struct LlvmProcessTraceLog final
{
    bool framingObserved = false;
    bool framingComplete = false;
    bool llvmStandardErrorCaptured = false;
    std::size_t capturedBytes = 0;
    std::string passLog;
    std::string diagnostics;
};
}
