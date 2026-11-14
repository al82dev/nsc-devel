#include "WinEhCatchSwitchPhiLegalization.h"

#include "../LlvmCApi.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace nativesharp::llvm_backend
{
namespace
{
using namespace llvm_api;

LLVMValueRef firstNonPhi(LLVMBasicBlockRef block)
{
    LLVMValueRef instruction = LLVMGetFirstInstruction(block);
    while (instruction != nullptr && LLVMIsAPHINode(instruction) != nullptr)
        instruction = LLVMGetNextInstruction(instruction);
    return instruction;
}

// EN: A catchswitch is the only EH pad that is also its block's terminator, so nothing can be
//     inserted in its block. / ES: Un catchswitch es el unico bloque EH cuyo pad es tambien su
//     terminador, asi que en su bloque no se puede insertar nada.
bool isCatchSwitchBlock(LLVMBasicBlockRef block)
{
    const LLVMValueRef pad = firstNonPhi(block);
    return pad != nullptr && LLVMIsACatchSwitchInst(pad) != nullptr;
}

bool isEhPadBlock(LLVMBasicBlockRef block)
{
    const LLVMValueRef pad = firstNonPhi(block);
    return pad != nullptr &&
        (LLVMIsACatchSwitchInst(pad) != nullptr || LLVMIsAFuncletPadInst(pad) != nullptr ||
         LLVMIsALandingPadInst(pad) != nullptr);
}

struct PadPhiEntry
{
    LLVMValueRef phi = nullptr;
    unsigned index = 0;
    LLVMBasicBlockRef incoming = nullptr;
};

std::size_t legalizePhi(
    LLVMValueRef phi,
    LLVMBasicBlockRef catchSwitchBlock,
    LLVMValueRef function,
    LLVMBuilderRef builder)
{
    // EN: The EH-pad phis that take `phi` from a block other than the catchswitch itself; those
    //     are the entries WinEHPrepare turns into `store poison`. An entry whose incoming block is
    //     the catchswitch is handled correctly by WinEHPrepare and stays as it is.
    // ES: Las phis de bloques EH que toman `phi` desde un bloque distinto del propio catchswitch;
    //     son las entradas que WinEHPrepare convierte en `store poison`. Una entrada cuyo bloque de
    //     entrada es el catchswitch la resuelve bien WinEHPrepare y se deja igual.
    std::vector<LLVMValueRef> users;
    for (LLVMUseRef use = LLVMGetFirstUse(phi); use != nullptr; use = LLVMGetNextUse(use))
    {
        const LLVMValueRef user = LLVMGetUser(use);
        if (LLVMIsAPHINode(user) == nullptr) continue;
        if (!isEhPadBlock(LLVMGetInstructionParent(user))) continue;
        if (std::find(users.begin(), users.end(), user) == users.end()) users.push_back(user);
    }
    std::vector<PadPhiEntry> entries;
    for (const LLVMValueRef user : users)
    {
        const unsigned count = LLVMCountIncoming(user);
        for (unsigned index = 0; index < count; ++index)
        {
            if (LLVMGetIncomingValue(user, index) != phi) continue;
            const LLVMBasicBlockRef incoming = LLVMGetIncomingBlock(user, index);
            if (incoming == catchSwitchBlock) continue;
            entries.push_back({user, index, incoming});
        }
    }
    if (entries.empty()) return 0;

    // EN: The slot needs a store at the end of every predecessor of the catchswitch and a load at
    //     the end of every affected incoming block. A block that is itself a catchswitch has no room
    //     for either; that shape is left to LLVM unchanged.
    // ES: La ranura necesita un guardado al final de cada predecesor del catchswitch y una carga al
    //     final de cada bloque de entrada afectado. Un bloque que tambien es catchswitch no tiene
    //     espacio para ninguno de los dos; esa forma se deja a LLVM sin cambios.
    const unsigned incomingCount = LLVMCountIncoming(phi);
    for (unsigned index = 0; index < incomingCount; ++index)
        if (isCatchSwitchBlock(LLVMGetIncomingBlock(phi, index))) return 0;
    for (const PadPhiEntry& entry : entries)
        if (isCatchSwitchBlock(entry.incoming)) return 0;

    const LLVMTypeRef type = LLVMTypeOf(phi);
    LLVMPositionBuilderBefore(builder, LLVMGetFirstInstruction(LLVMGetEntryBasicBlock(function)));
    const LLVMValueRef slot = LLVMBuildAlloca(builder, type, "nsc.wineh.catchswitch.phi.slot");

    // EN: Loads first. When an incoming block is also a predecessor of the catchswitch, its load
    //     must read the value of the previous entry, so it goes before that block's store.
    // ES: Primero las cargas. Si un bloque de entrada tambien es predecesor del catchswitch, su
    //     carga debe leer el valor de la entrada anterior, asi que va antes del guardado del bloque.
    std::vector<std::pair<LLVMBasicBlockRef, LLVMValueRef>> loads;
    for (const PadPhiEntry& entry : entries)
    {
        LLVMValueRef load = nullptr;
        for (const auto& [block, value] : loads)
            if (block == entry.incoming) load = value;
        if (load == nullptr)
        {
            LLVMPositionBuilderBefore(builder, LLVMGetBasicBlockTerminator(entry.incoming));
            load = LLVMBuildLoad2(builder, type, slot, "nsc.wineh.catchswitch.phi.reload");
            loads.emplace_back(entry.incoming, load);
        }
        LLVMSetOperand(entry.phi, entry.index, load);
    }
    for (unsigned index = 0; index < incomingCount; ++index)
    {
        LLVMPositionBuilderBefore(builder, LLVMGetBasicBlockTerminator(LLVMGetIncomingBlock(phi, index)));
        LLVMBuildStore(builder, LLVMGetIncomingValue(phi, index), slot);
    }
    return entries.size();
}
}

std::size_t legalizeWinEhCatchSwitchPhiUses(void* moduleHandle) noexcept
{
    const auto module = static_cast<LLVMModuleRef>(moduleHandle);
    if (module == nullptr) return 0;
    LLVMBuilderRef builder = nullptr;
    std::size_t rewritten = 0;
    for (LLVMValueRef function = LLVMGetFirstFunction(module); function != nullptr;
         function = LLVMGetNextFunction(function))
    {
        if (LLVMIsDeclaration(function) || !LLVMHasPersonalityFn(function)) continue;
        for (LLVMBasicBlockRef block = LLVMGetFirstBasicBlock(function); block != nullptr;
             block = LLVMGetNextBasicBlock(block))
        {
            if (!isCatchSwitchBlock(block)) continue;
            std::vector<LLVMValueRef> phis;
            for (LLVMValueRef instruction = LLVMGetFirstInstruction(block);
                 instruction != nullptr && LLVMIsAPHINode(instruction) != nullptr;
                 instruction = LLVMGetNextInstruction(instruction))
                phis.push_back(instruction);
            for (const LLVMValueRef phi : phis)
            {
                if (builder == nullptr) builder = LLVMCreateBuilderInContext(LLVMGetModuleContext(module));
                rewritten += legalizePhi(phi, block, function, builder);
            }
        }
    }
    if (builder != nullptr) LLVMDisposeBuilder(builder);
    return rewritten;
}
}
