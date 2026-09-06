#pragma once

#include <string>
#include <string_view>

namespace nativesharp::llvm_backend
{
// Resultado de volver a ejecutar el ejecutable actual con su salida de error apuntando a un
// fichero DESDE CreateProcess. En Windows esto no es intercambiable con `_dup2`: LLVM-C.dll lleva
// su propio CRT y su descriptor 2 ya existe cuando entra `main`.
struct LlvmTraceChildResult final
{
    unsigned long exitCode = 0;
    std::string standardError;
    std::string launchError;
};

// Agrega `childArgument` a la linea de comandos actual, espera al hijo y recoge stderr. La
// funcion no interpreta el registro de LLVM; sólo establece la frontera de proceso que permite
// capturarlo de forma correcta.
bool runCurrentExecutableWithCapturedStandardError(
    std::string_view childArgument,
    LlvmTraceChildResult& result);

// El padre agrega este argumento privado. El hijo lo retira antes de llamar a Driver y enciende
// la escritura delimitada del registro de pases.
inline constexpr std::string_view kLlvmTraceWorkerArgument =
    "--nsc-internal-llvm-trace-worker";

void setLlvmProcessTraceActive(bool active) noexcept;
bool llvmProcessTraceActive() noexcept;
}
