// Contextual header: include only in LlvmLowerer::Implementation private scope.
struct DeferredPhi
{
    const mir::MirInstruction* instruction = nullptr;
    LLVMValueRef llvmPhi = nullptr;
};
