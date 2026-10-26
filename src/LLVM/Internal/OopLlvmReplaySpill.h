#pragma once

#include "../LlvmCApi.h"

namespace nativesharp::frontend
{
struct ProjectOopMirNode;
}

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

// EN: Freeze 3.0 D-01: one operand under evaluation with its replay slot. `loadBlock` is entered
//     when the resume state names a replay of this operand; `computeBlock` evaluates it. Built by
//     OopLlvmImplementation::beginCoroutineReplaySpill and closed by finishCoroutineReplaySpill;
//     it owns nothing (every member belongs to the LLVM function being lowered).
// ES: Congelamiento 3.0 D-01: un operando en evaluacion con su slot de repeticion. A `loadBlock`
//     se entra cuando el estado de reanudacion nombra una repeticion de este operando;
//     `computeBlock` lo evalua. Lo crea OopLlvmImplementation::beginCoroutineReplaySpill y lo
//     cierra finishCoroutineReplaySpill; no posee nada (todo pertenece a la funcion LLVM en curso).
struct OopLlvmReplaySpill final
{
    const frontend::ProjectOopMirNode* node = nullptr;
    LLVMValueRef slot = nullptr;
    LLVMBasicBlockRef loadBlock = nullptr;
    LLVMBasicBlockRef computeBlock = nullptr;
};
}
