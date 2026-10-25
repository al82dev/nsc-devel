#pragma once

#include "nativesharp/LLVM/LlvmOptimizationLevel.h"

#include <atomic>
#include <cstddef>
#include <filesystem>
#include <mutex>
#include <span>
#include <string>
#include <vector>

namespace nativesharp::llvm_backend
{
class LlvmModule;
class LlvmTargetMachine;

/// EN: Intraproject ThinLTO over the code units of one generation (optimization part 2, item 44).
///     Every unit runs the ThinLTO pre-link pipeline and is written as bitcode with its module
///     summary; run() then lets LLVM build the combined index, choose the cross-unit imports and
///     run one post-link backend per unit in parallel, writing one native COFF object per unit.
///     Every defined symbol stays visible to native objects (the runtime, libraries and DLL
///     exports may reference any of them), so nothing is internalized; the gain is cross-unit
///     importing and inlining. Thread safety: addModule() is safe from several threads for
///     distinct ordinals; run() is called once, after every addModule().
/// ES: ThinLTO intraproyecto sobre las unidades de codigo de una generacion (parte 2, punto 44).
///     Cada unidad pasa por el pipeline de pre-enlace de ThinLTO y se escribe como bitcode con su
///     resumen de modulo; run() deja que LLVM arme el indice combinado, elija los imports entre
///     unidades y ejecute un backend de post-enlace por unidad en paralelo, escribiendo un objeto
///     COFF nativo por unidad. Todo simbolo definido sigue visible para los objetos nativos (el
///     runtime, las bibliotecas y los exports de DLL pueden usar cualquiera), asi que nada se
///     internaliza; la ganancia es importar e inlinear entre unidades. Hilos: addModule() es seguro
///     desde varios hilos con ordinales distintos; run() se llama una vez, despues de todos.
class ThinLtoBackend final
{
public:
    explicit ThinLtoBackend(std::size_t moduleCount);

    /// EN: False in a DYNAMIC development host, which links only LLVM-C.dll and therefore has no
    ///     llvm::lto; the generator then keeps the ordinary partitioned build for -flto=thin.
    /// ES: False en un host de desarrollo DYNAMIC, que solo enlaza LLVM-C.dll y por eso no tiene
    ///     llvm::lto; el generador conserva entonces el build particionado normal para -flto=thin.
    static bool available() noexcept;

    /// EN: Runs the pre-link pipeline of `level` on `module` (already configured for
    ///     `targetMachine`) and keeps its summary bitcode for unit `ordinal`. Returns false with
    ///     `error` on failure.
    /// ES: Ejecuta el pipeline de pre-enlace de `level` sobre `module` (ya configurado para
    ///     `targetMachine`) y guarda su bitcode con resumen para la unidad `ordinal`. Devuelve false
    ///     con `error` si falla.
    bool addModule(
        std::size_t ordinal,
        LlvmModule& module,
        const LlvmTargetMachine& targetMachine,
        LlvmOptimizationLevel level,
        std::string& error);

    /// EN: Builds the index, imports across units and writes unit i to `objectFiles[i]`, with at
    ///     most `threads` backends at once and `cacheDirectory` as the backend cache (empty: none).
    /// ES: Arma el indice, importa entre unidades y escribe la unidad i en `objectFiles[i]`, con a
    ///     lo mas `threads` backends a la vez y `cacheDirectory` como cache de backends (vacio:
    ///     ninguno).
    bool run(
        std::span<const std::filesystem::path> objectFiles,
        const LlvmTargetMachine& targetMachine,
        LlvmOptimizationLevel level,
        std::size_t threads,
        const std::filesystem::path& cacheDirectory,
        std::string& error);

    /// EN: Captures the optimized IR of every unit after its post-link pipeline (tests, dumps).
    /// ES: Captura el IR optimizado de cada unidad despues de su pipeline de post-enlace.
    void setCaptureFinalModuleTexts(bool capture) noexcept;
    std::span<const std::string> finalModuleTexts() const noexcept;

    /// EN: Function definitions the backends imported from other units (available_externally).
    /// ES: Definiciones de funcion que los backends importaron de otras unidades.
    std::size_t importedFunctionCount() const noexcept;

    /// EN: Backends whose object came from the backend cache in the last run().
    /// ES: Backends cuyo objeto salio del cache de backends en el ultimo run().
    std::size_t cachedBackendCount() const noexcept;

private:
    std::vector<std::string> bitcode_;
    std::vector<std::string> identifiers_;
    std::vector<std::string> finalModuleTexts_;
    bool captureFinalModuleTexts_ = false;
    std::atomic_size_t importedFunctions_ = 0;
    std::atomic_size_t cachedBackends_ = 0;
    std::atomic_size_t backendRuns_ = 0;
    std::mutex diagnosticsMutex_;
    std::string diagnostics_;
};
}
