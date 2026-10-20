#pragma once

#include <string>

namespace nativesharp::llvm_backend
{
// NSC-LIMIT-005 -- CAPTURAR LA SALIDA DE ERROR DEL PROCESO MIENTRAS CORRE `LLVMRunPasses`.
//
// POR QUE ASI Y NO DE OTRA FORMA. `LLVMPassBuilderOptionsSetDebugLogging` hace que LLVM escriba
// que pase ejecuta y sobre que, pero lo escribe por `llvm::dbgs()`, que es la salida de error del
// PROCESO. La API C no acepta un sink. De modo que o se desvia el descriptor 2 durante la llamada,
// o se entra en la API C++ -- y eso ultimo es una decision de arquitectura, no un detalle de
// implementacion.
//
// ESTA CLASE ES SOLO PARA POSIX. En Windows el producto enlaza LLVM-C.dll, que lleva su propio CRT:
// `_dup2` en nsc cambia el descriptor de nsc, no el de la DLL. Alli la captura se establece al
// CREAR el proceso hijo; ver LlvmProcessTrace.cpp y src/nsc/main.cpp.
#if !defined(_WIN32)
class StandardErrorCapture final
{
public:
    explicit StandardErrorCapture(std::string path);

    StandardErrorCapture(const StandardErrorCapture&) = delete;
    StandardErrorCapture& operator=(const StandardErrorCapture&) = delete;

    ~StandardErrorCapture();

    bool active() const noexcept;

    void restore();

    std::string text() const;

    void remove() const;

private:
    std::string path_;
    int saved_ = -1;
    bool active_ = false;
};
#endif
}
