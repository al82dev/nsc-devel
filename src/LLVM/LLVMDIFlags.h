#pragma once

namespace nativesharp::llvm_api
{
enum LLVMDIFlags
{
    LLVMDIFlagZero = 0,
    LLVMDIFlagPrivate = 1,
    LLVMDIFlagProtected = 2,
    LLVMDIFlagPublic = 3,
    LLVMDIFlagArtificial = 1 << 6,
    LLVMDIFlagObjectPointer = 1 << 10,
    LLVMDIFlagThunk = 1 << 25,
};
}
