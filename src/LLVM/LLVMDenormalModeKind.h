#pragma once

namespace nativesharp::llvm_api
{
enum LLVMDenormalModeKind
{
    LLVMDenormalModeKindIEEE = 0,
    LLVMDenormalModeKindPreserveSign = 1,
    LLVMDenormalModeKindPositiveZero = 2,
    LLVMDenormalModeKindDynamic = 3
};
}
