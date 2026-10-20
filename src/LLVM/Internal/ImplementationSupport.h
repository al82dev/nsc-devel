#pragma once

// Contextual header: include inside namespace nativesharp::llvm_backend after LlvmLowerer dependencies.
namespace llvm_lowerer_detail
{
extern const char* const kTargetTriple;
extern const char* const kTrapFunctionName;
extern const char* const kAllocationFunctionName;
extern const char* const kDeallocationFunctionName;

bool isInteger(const sema::Type* type) noexcept;
bool isSignedInteger(const sema::Type* type) noexcept;
bool isFloating(const sema::Type* type) noexcept;
bool isPointerLike(const sema::Type* type) noexcept;
unsigned integerWidth(const sema::Type* type) noexcept;
double lowerFloatingBoundary(
    long double mathematicalBoundary,
    const sema::Type* sourceType) noexcept;
std::string valueName(const mir::MirValue* value);
std::string blockName(const mir::MirBlock* block);
LLVMIntPredicate integerPredicate(mir::MirOpcode opcode, bool signedOperands);
LLVMRealPredicate floatingPredicate(mir::MirOpcode opcode);
}
