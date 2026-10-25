#pragma once

#include "../LlvmCApi.h"

#include <cstddef>

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

// CC1 EH-001. One entry per `catch` clause body currently being lowered, innermost last.
// Deliberately NOT a field on FuncletContext: since the B-006/B-007 fix, clause bodies are
// ordinary parent blocks and nothing ever pushes a FuncletKind::Catch entry onto funcletStack_
// any more (the only push site builds Cleanup entries), so an obligation hung there would be
// unreachable code that closes the finding on paper and measures nothing.
struct ActiveCatchBody final
{
    LLVMValueRef exception = nullptr;
    std::size_t loopDepthAtEntry = 0;
    // EN: Freeze 3.0 C-01: the catch owns the exception it handles. `ownerSlot` holds it until
    //     the catch releases it or hands it to a rethrow (which stores null). A throw leaves this
    //     body when no try opened inside it is active: exceptionHandlerStack_.size() is still
    //     `handlerDepthAtEntry`.
    // ES: Congelamiento 3.0 C-01: el catch es dueno de la excepcion que maneja. `ownerSlot` la
    //     guarda hasta que el catch la libera o se la entrega a un rethrow (que guarda null). Un
    //     throw sale de este cuerpo cuando no hay activo ningun try abierto dentro de el:
    //     exceptionHandlerStack_.size() sigue valiendo `handlerDepthAtEntry`.
    LLVMValueRef ownerSlot = nullptr;
    std::size_t handlerDepthAtEntry = 0;
};
}
