#pragma once

// Contextual header: include inside namespace nativesharp::llvm_backend after LlvmLowerer dependencies.
class LlvmLowerer::Implementation final
{
public:
    explicit Implementation(std::span<sema::Symbol* const> globalSymbols);

    std::unique_ptr<LlvmModule> lower(
        const passes::OptimizationModule* optimizedModule,
        std::string& error);

private:
#include "LlvmLowererFieldInfo.h"
#include "LlvmLowererDeferredPhi.h"

    void reset();
    void disposeState() noexcept;
    bool initializeModule(std::string& error);
    bool declareStructs(std::string& error);
    bool defineStructBodies(std::string& error);
    bool declareGlobals(std::string& error);
    bool declareFunctions(std::string& error);
    bool declareRuntimeFunctions(std::string& error);
    bool lowerGlobals(std::string& error);
    bool lowerFunctions(std::string& error);
    bool lowerFunction(const mir::MirFunction* function, std::string& error);
    bool lowerInstruction(const mir::MirInstruction* instruction, std::string& error);
    bool lowerInstructionGroup01(
        const mir::MirInstruction* instruction,
        LLVMValueRef& result,
        const std::string& name,
        std::string& error);
    bool lowerInstructionGroup02(
        const mir::MirInstruction* instruction,
        LLVMValueRef& result,
        const std::string& name,
        std::string& error);
    bool lowerInstructionGroup03(
        const mir::MirInstruction* instruction,
        LLVMValueRef& result,
        const std::string& name,
        std::string& error);
    bool lowerInstructionGroup03Remainder(
        const mir::MirInstruction* instruction,
        LLVMValueRef& result,
        const std::string& name,
        std::string& error);
    bool lowerInstructionGroup04(
        const mir::MirInstruction* instruction,
        LLVMValueRef& result,
        const std::string& name,
        std::string& error);
    bool lowerInstructionGroup05(
        const mir::MirInstruction* instruction,
        LLVMValueRef& result,
        const std::string& name,
        std::string& error);
    bool finishInstruction(
        const mir::MirInstruction* instruction,
        LLVMValueRef result,
        std::string& error);
    LLVMValueRef lowerBasicArithmetic(const mir::MirInstruction* instruction, std::string& error);
    LLVMValueRef lowerDivision(const mir::MirInstruction* instruction, std::string& error);
    LLVMValueRef lowerBitwise(const mir::MirInstruction* instruction, std::string& error);
    LLVMValueRef lowerShift(const mir::MirInstruction* instruction, std::string& error);
    LLVMValueRef lowerComparison(const mir::MirInstruction* instruction, std::string& error);
    LLVMValueRef lowerConversion(const mir::MirInstruction* instruction, std::string& error);
    LLVMValueRef lowerFloatingToInteger(
        LLVMValueRef source,
        const sema::Type* sourceType,
        const sema::Type* destinationType,
        const std::string& name,
        std::string& error);
    LLVMValueRef castInteger(
        LLVMValueRef value,
        const sema::Type* sourceType,
        const sema::Type* destinationType,
        bool signExtend,
        const std::string& name,
        std::string& error);
    LLVMValueRef lowerPointerArithmetic(const mir::MirInstruction* instruction, std::string& error);
    LLVMValueRef lowerPointerDifference(const mir::MirInstruction* instruction, std::string& error);
    LLVMValueRef lowerCall(const mir::MirInstruction* instruction, std::string& error);
    bool lowerBoundsCheck(const mir::MirInstruction* instruction, std::string& error);
    LLVMValueRef lowerFieldAddress(const mir::MirInstruction* instruction, std::string& error);
    LLVMValueRef lowerIndexAddress(const mir::MirInstruction* instruction, std::string& error);
    LLVMValueRef lowerExtractField(const mir::MirInstruction* instruction, std::string& error);
    LLVMValueRef lowerCountedAllocation(const mir::MirInstruction* instruction, std::string& error);
    bool guardOrTrap(
        LLVMValueRef validCondition,
        LlvmTrapReason reason,
        const std::string& stem,
        std::string& error);
    bool completePhi(const DeferredPhi& deferred, std::string& error);
    LLVMTypeRef llvmType(const sema::Type* type, std::string& error);
    LLVMTypeRef llvmFunctionType(const sema::Type* type, std::string& error);
    LLVMValueRef llvmConstant(
        const sema::ConstantValue& constant,
        const sema::Type* type,
        std::string& error,
        bool allowStringInstruction);
    std::optional<target::TypeLayout> layoutOf(const sema::Type* type) const;
    LLVMValueRef operand(
        const mir::MirInstruction* instruction,
        std::size_t index,
        std::string& error);
    LLVMValueRef symbolStorage(const sema::Symbol* symbol) const noexcept;
    bool failInstruction(
        const mir::MirInstruction* instruction,
        const std::string& message,
        std::string& error) const;

    std::vector<sema::Symbol*> globalSymbols_;
    target::TargetDataLayout targetLayout_;
    const mir::MirModule* sourceModule_ = nullptr;
    LLVMContextRef context_ = nullptr;
    LLVMModuleRef module_ = nullptr;
    LLVMBuilderRef builder_ = nullptr;
    LLVMValueRef currentFunction_ = nullptr;
    const mir::MirFunction* currentMirFunction_ = nullptr;
    LLVMValueRef trapFunction_ = nullptr;
    LLVMTypeRef trapFunctionType_ = nullptr;
    LLVMValueRef allocationFunction_ = nullptr;
    LLVMTypeRef allocationFunctionType_ = nullptr;
    LLVMValueRef deallocationFunction_ = nullptr;
    LLVMTypeRef deallocationFunctionType_ = nullptr;
    std::unordered_map<const sema::Type*, LLVMTypeRef> typeMap_;
    std::unordered_map<const sema::Type*, sema::Symbol*> structSymbols_;
    std::unordered_map<const sema::Symbol*, FieldInfo> fieldInfo_;
    std::unordered_map<const sema::Symbol*, LLVMValueRef> globalStorage_;
    std::unordered_map<const sema::Symbol*, LLVMValueRef> functionValues_;
    std::unordered_map<const sema::Symbol*, LLVMTypeRef> functionTypes_;
    std::unordered_map<const mir::MirValue*, LLVMValueRef> valueMap_;
    std::unordered_map<const mir::MirBlock*, LLVMBasicBlockRef> blockEntry_;
    std::unordered_map<const mir::MirBlock*, LLVMBasicBlockRef> blockExit_;
    std::unordered_map<const sema::Symbol*, LLVMValueRef> localStorage_;
    std::vector<DeferredPhi> deferredPhis_;
    std::uint64_t trapSequence_ = 0;
    std::uint64_t stringSequence_ = 0;
};
