#pragma once

#include <iosfwd>
#include <string>
#include <string_view>

namespace nativesharp::llvm_backend
{
class LlvmModule;

// EN: Freeze 3.0 DEC-05. Versioned formats of the stage inspections of a source file. The NativeSharp
//     3.x pipeline has no CFG, dominance, SSA or MIR-pass stage of its own: LLVM performs them, so
//     these inspections observe the LLVM module that the 3.x pipeline generates. The specification
//     lives in docs/3.0/NSC-FORMATOS-INSPECCION-LLVM.md; a change of layout changes the version.
// ES: Congelamiento 3.0 DEC-05. Formatos versionados de las inspecciones de etapa de un archivo
//     fuente. El pipeline 3.x de NativeSharp no tiene etapas propias de CFG, dominancia, SSA ni pases
//     de MIR: las hace LLVM, asi que estas inspecciones observan el modulo LLVM que genera el
//     pipeline 3.x. La especificacion esta en docs/3.0/NSC-FORMATOS-INSPECCION-LLVM.md; un cambio de
//     disposicion cambia la version.
inline constexpr std::string_view kLlvmCfgFormat = "nsc-llvm-cfg/1";
inline constexpr std::string_view kLlvmDominanceFormat = "nsc-llvm-dominance/1";
inline constexpr std::string_view kLlvmSsaFormat = "nsc-llvm-ssa/1";
inline constexpr std::string_view kLlvmOptimizedFormat = "nsc-llvm-opt/1";
inline constexpr std::string_view kLlvmPassPipelineFormat = "nsc-llvm-pass-pipeline/1";

// EN: Read-only views of an LlvmModule. Every function reads the module it receives and never changes
//     it; the SSA form is built on a private copy. Output is deterministic: functions in module order,
//     blocks in layout order, numbered bb0..bbN-1 per function. Each returns false with `error` set
//     when the module cannot be inspected or an invariant does not hold.
// ES: Vistas de solo lectura de un LlvmModule. Cada funcion lee el modulo que recibe y nunca lo
//     cambia; la forma SSA se construye sobre una copia privada. La salida es determinista: funciones
//     en orden del modulo, bloques en orden de disposicion, numerados bb0..bbN-1 por funcion. Cada una
//     devuelve false con `error` cuando el modulo no se puede inspeccionar o no se cumple un
//     invariante.
class LlvmModuleInspector final
{
public:
    // EN: Blocks, predecessors, successors and reverse postorder of every defined function.
    // ES: Bloques, predecesores, sucesores y postorden inverso de cada funcion definida.
    static bool dumpControlFlow(const LlvmModule& module, std::ostream& output, std::string& error);
    // EN: Every block ends in its terminator, every edge stays in its function and the entry block
    //     has no predecessor.
    // ES: Todo bloque termina en su terminador, toda arista queda en su funcion y el bloque de entrada
    //     no tiene predecesores.
    static bool verifyControlFlow(const LlvmModule& module, std::ostream& output, std::string& error);
    // EN: Immediate dominator, depth and dominance frontier of every reachable block.
    // ES: Dominador inmediato, profundidad y frontera de dominancia de cada bloque alcanzable.
    static bool dumpDominance(const LlvmModule& module, std::ostream& output, std::string& error);
    // EN: The dominator tree satisfies its fixpoint: the immediate dominator of each reachable block is
    //     the nearest common dominator of its reachable predecessors.
    // ES: El arbol de dominadores cumple su punto fijo: el dominador inmediato de cada bloque
    //     alcanzable es el dominador comun mas cercano de sus predecesores alcanzables.
    static bool verifyDominance(const LlvmModule& module, std::ostream& output, std::string& error);
    // EN: The module after mem2reg on a copy, preceded by a per-function count of phis and allocas.
    // ES: El modulo despues de mem2reg sobre una copia, precedido de la cuenta de phis y allocas por
    //     funcion.
    static bool dumpSsa(const LlvmModule& module, std::ostream& output, std::string& error);
    // EN: On that copy: the LLVM verifier, phis grouped at the start of their block, one incoming
    //     block per distinct predecessor, and every definition dominating each of its uses. Allocas
    //     that mem2reg cannot promote are counted, not rejected.
    // ES: Sobre esa copia: el verificador de LLVM, los phis agrupados al inicio de su bloque, un
    //     bloque entrante por cada predecesor distinto y cada definicion dominando cada uno de sus
    //     usos. Los allocas que mem2reg no puede promover se cuentan, no se rechazan.
    static bool verifySsa(const LlvmModule& module, std::ostream& output, std::string& error);

private:
    // EN: The LLVMModuleRef of `module`, as an opaque pointer (LlvmModule names this class a friend).
    // ES: El LLVMModuleRef de `module`, como puntero opaco (LlvmModule declara amiga a esta clase).
    static void* moduleOf(const LlvmModule& module) noexcept;
};
}
