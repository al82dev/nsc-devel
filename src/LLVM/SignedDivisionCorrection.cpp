#include "SignedDivisionCorrection.h"

namespace nativesharp::llvm_backend
{
LLVMValueRef correctedSignedDivisor(
    LLVMBuilderRef builder,
    LLVMTypeRef integerType,
    LLVMValueRef dividend,
    LLVMValueRef divisor,
    unsigned width,
    std::string_view stem)
{
    if (builder == nullptr || integerType == nullptr || dividend == nullptr || divisor == nullptr)
    {
        return divisor;
    }

    const std::uint64_t minimumBits = width >= 64U
        ? (std::uint64_t{1} << 63)
        : (std::uint64_t{1} << (width - 1U));
    const std::uint64_t minusOneBits = width >= 64U
        ? ~std::uint64_t{0}
        : ((std::uint64_t{1} << width) - 1U);

    LLVMValueRef minimum = LLVMConstInt(integerType, minimumBits, 0);
    LLVMValueRef minusOne = LLVMConstInt(integerType, minusOneBits, 0);
    LLVMValueRef one = LLVMConstInt(integerType, 1, 0);

    const std::string base(stem);
    LLVMValueRef isMinimum =
        LLVMBuildICmp(builder, LLVMIntEQ, dividend, minimum, (base + ".is_min").c_str());
    LLVMValueRef isMinusOne =
        LLVMBuildICmp(builder, LLVMIntEQ, divisor, minusOne, (base + ".is_minus_one").c_str());
    LLVMValueRef overflow =
        LLVMBuildAnd(builder, isMinimum, isMinusOne, (base + ".overflow").c_str());
    return LLVMBuildSelect(builder, overflow, one, divisor, (base + ".divisor").c_str());
}
}
