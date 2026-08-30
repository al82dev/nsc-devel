#pragma once

namespace nativesharp
{
enum class ExitCode : int
{
    Success = 0,
    CompilationError = 1,
    CommandLineError = 2,
    InternalError = 70
};
}
