#pragma once

#include "nativesharp/Basic/SourceRange.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Frontend/GenericConstraintViolation.h"

#include <string>

namespace nativesharp::frontend::internal
{
struct PendingDiagnostic final
{
    diagnostics::DiagnosticId id = diagnostics::DiagnosticId::InvalidType;
    SourceRange range;
    std::string message;
    GenericConstraintViolation violation;
};
}
