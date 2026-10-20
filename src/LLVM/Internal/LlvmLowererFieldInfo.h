// Contextual header: include only in LlvmLowerer::Implementation private scope.
struct FieldInfo
{
    LLVMTypeRef structType = nullptr;
    unsigned index = 0;
};
