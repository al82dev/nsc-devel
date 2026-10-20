#pragma once

namespace nativesharp::llvm_backend
{
enum class FloatingPointDenormalMode
{
    Preserve,
    FlushToZero,
    DenormalsAreZero,
    FlushToZeroAndDenormalsAreZero
};
}
