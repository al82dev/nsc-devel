#pragma once

// NSC305-L03. Module-level metadata + LLVM intrinsic names moved out of ProjectOopLlvmLowerer
// (pure move).
namespace nativesharp::llvm_backend
{
inline constexpr const char* kTargetTriple = "x86_64-pc-windows-msvc";
inline constexpr const char* kMemsetIntrinsicName = "llvm.memset.p0.i64";
inline constexpr const char* kMemcpyIntrinsicName = "llvm.memcpy.p0.p0.i64";
inline constexpr const char* kAssumeIntrinsicName = "llvm.assume";
} // namespace nativesharp::llvm_backend
