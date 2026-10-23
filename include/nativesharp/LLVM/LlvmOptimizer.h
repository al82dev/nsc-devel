#pragma once

#include "nativesharp/LLVM/AutomaticInliningConfiguration.h"
#include "nativesharp/LLVM/AutomaticInliningCostModel.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/LLVM/LlvmOptimizationObservation.h"

#include <memory>
#include <string>
#include <string_view>

namespace nativesharp::llvm_backend
{
struct CodeUnitTelemetry;
class LlvmModule;
class ProjectOopObjectGenerator;
class LlvmTargetMachine;

// O0 is verified at the optimizer boundary; optimized pipelines additionally verify after each
// pass so a transformation failure can still name its last completed pass.
bool llvmVerifyEachPassRequired(
    LlvmOptimizationLevel level, bool detailedPassDiagnostics) noexcept;

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
    friend bool optimizeConfiguredCodeUnit(
        LlvmModule& module,
        const LlvmTargetMachine& targetMachine,
        LlvmOptimizationLevel optimizationLevel,
        bool requiresWholeModuleRequiredInlining,
        std::string& moduleError,
        CodeUnitTelemetry& telemetry);

    // The project object pipeline has just verified the lowerer's output and configured it with
    // this exact TargetMachine. Keep the public optimizer boundary defensive while allowing that
    // tightly coupled path to avoid repeating two whole-module preparation steps.
    bool optimizePrepared(
        LlvmModule& module,
        const LlvmTargetMachine& targetMachine,
        LlvmOptimizationLevel level,
        bool mandatoryInliningRequired,
        LlvmOptimizationObservation* observation,
        std::string& error) const;

    bool optimizeImpl(
        LlvmModule& module,
        const LlvmTargetMachine& targetMachine,
        LlvmOptimizationLevel level,
        LlvmOptimizationObservation* observation,
        bool prepareModule,
        bool mandatoryInliningRequired,
        std::string& error) const;

    bool prepareOptimizationModule(
        LlvmModule& module,
        const LlvmTargetMachine& targetMachine,
        bool prepareModule,
        LlvmOptimizationObservation* observation,
        std::string& error) const;

    bool verifyOptimizedModule(
        LlvmModule& module,
        LlvmOptimizationObservation* observation,
        std::string& error) const;
};
}
