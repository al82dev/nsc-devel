#pragma once

#include "../LlvmCApi.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace nativesharp::frontend
{
struct ProjectOopMirNode;
}

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

// EN: Freeze 3.0 C-02: the resume machinery of one poll or MoveNext definition. Its locals live
//     in the heap frame, the entry dispatch jumps to the statement that suspended, and every
//     suspension point owns a distinct state id (1..N; 0 is a fresh start, -1/-2 are terminal).
// ES: Congelamiento 3.0 C-02: la maquinaria de reanudacion de una definicion poll o MoveNext. Sus
//     locales viven en el frame del heap, el despacho de entrada salta a la sentencia que se
//     suspendio y cada punto de suspension tiene un id de estado propio (1..N; 0 es un inicio
//     nuevo, -1/-2 son terminales).
struct OopLlvmCoroutineState final
{
    // EN: `Gen$drop` asks a suspended generator to unwind by storing kCancelStateBase - id; the
    //     MoveNext dispatch sends that state to the cleanup path of yield `id`.
    // ES: `Gen$drop` pide a un generador suspendido que se desenrede guardando
    //     kCancelStateBase - id; el despacho de MoveNext manda ese estado a la limpieza del
    //     yield `id`.
    static constexpr std::int64_t kCancelStateBase = -1000;
    bool active = false;
    // EN: The future (poll) or iterator (MoveNext) pointer, and its frame (offset 8).
    // ES: El puntero al future (poll) o al iterador (MoveNext), y su frame (offset 8).
    LLVMValueRef self = nullptr;
    LLVMValueRef frame = nullptr;
    // EN: i32 slot holding the state seen at entry; a suspension point that resumes clears it.
    // ES: Slot i32 con el estado visto al entrar; el punto de suspension que reanuda lo limpia.
    LLVMValueRef resumeState = nullptr;
    LLVMValueRef dispatch = nullptr;
    std::unordered_map<const frontend::ProjectOopMirNode*, std::int64_t> suspensionIds;
    // EN: Every node whose subtree holds suspension points, with their ids.
    // ES: Todo nodo cuyo subarbol contiene puntos de suspension, con sus ids.
    std::unordered_map<const frontend::ProjectOopMirNode*, std::vector<std::int64_t>> subtreeIds;
    // EN: The start of the innermost statement around each id; the dispatch jumps there.
    // ES: El inicio de la sentencia mas interna alrededor de cada id; ahi salta el despacho.
    std::unordered_map<std::int64_t, LLVMBasicBlockRef> resumeBlocks;
    // EN: Per yield id, the block that runs the cleanups active at that yield and ends.
    // ES: Por id de yield, el bloque que corre las limpiezas activas en ese yield y termina.
    std::unordered_map<std::int64_t, LLVMBasicBlockRef> cancelBlocks;
};
}
