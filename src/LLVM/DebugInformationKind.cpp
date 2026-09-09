#include "nativesharp/LLVM/DebugInformationKind.h"

namespace nativesharp::llvm_backend
{
std::string_view debugInformationKindName(DebugInformationKind kind) noexcept
{
    switch (kind)
    {
        case DebugInformationKind::None:
            return "none";
        case DebugInformationKind::LineTablesOnly:
            return "line-tables-only";
        case DebugInformationKind::Full:
            return "full";
    }
    return {};
}
}
