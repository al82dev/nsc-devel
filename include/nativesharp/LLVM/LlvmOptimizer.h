#pragma once

#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/LLVM/LlvmOptimizationObservation.h"

#include <string>
#include <string_view>

namespace nativesharp::llvm_backend
{
class LlvmModule;
class ProjectOopObjectGenerator;
class LlvmTargetMachine;

// O0 is verified at the optimizer boundary; optimized pipelines additionally verify after each
// pass so a transformation failure can still name its last completed pass.
bool llvmVerifyEachPassRequired(
    LlvmOptimizationLevel level, bool detailedPassDiagnostics) noexcept;

enum class AutomaticInliningCostModel
{
    Disabled,
    Conservative,
    Balanced,
    Aggressive
};

struct AutomaticInliningConfiguration final
{
    AutomaticInliningCostModel costModel;
    std::string_view pipeline;

    constexpr bool enabled() const noexcept
    {
        return costModel != AutomaticInliningCostModel::Disabled;
    }
};

AutomaticInliningConfiguration llvmAutomaticInliningConfiguration(
    LlvmOptimizationLevel level) noexcept;

class LlvmOptimizer final
{
public:
    // La entrada de siempre. Equivale EXACTAMENTE a la de abajo con `observation == nullptr`: sin
    // captura del registro de pases, sin cronometro y sin tocar ninguna opcion del PassBuilder que
    // no se tocara ya antes de que este parametro existiera.
    bool optimize(
        LlvmModule& module,
        const LlvmTargetMachine& targetMachine,
        LlvmOptimizationLevel level,
        std::string& error) const;

    // NSC-LIMIT-005. La misma optimizacion, observada. Con `observation != nullptr` se enciende
    // `LLVMPassBuilderOptionsSetDebugLogging` y se captura la salida de error del proceso mientras
    // dura `LLVMRunPasses`, que es lo unico que la API C ofrece para saber por donde iba.
    //
    // OBSERVAR NO PUEDE CAMBIAR EL RESULTADO. El registro solo imprime, y la captura solo desvia un
    // descriptor de fichero; hay una prueba que optimiza el mismo modulo con y sin observacion y
    // compara el IR resultante texto a texto.
    bool optimize(
        LlvmModule& module,
        const LlvmTargetMachine& targetMachine,
        LlvmOptimizationLevel level,
        LlvmOptimizationObservation* observation,
        std::string& error) const;

private:
    friend class ProjectOopObjectGenerator;

    // The project object pipeline has just verified the lowerer's output and configured it with
    // this exact TargetMachine. Keep the public optimizer boundary defensive while allowing that
    // tightly coupled path to avoid repeating two whole-module preparation steps.
    bool optimizePrepared(
        LlvmModule& module,
        const LlvmTargetMachine& targetMachine,
        LlvmOptimizationLevel level,
        LlvmOptimizationObservation* observation,
        std::string& error) const;

    bool optimizeImpl(
        LlvmModule& module,
        const LlvmTargetMachine& targetMachine,
        LlvmOptimizationLevel level,
        LlvmOptimizationObservation* observation,
        bool prepareModule,
        std::string& error) const;
};
}
