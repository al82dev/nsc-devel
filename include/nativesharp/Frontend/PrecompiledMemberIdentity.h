#pragma once

#include "nativesharp/Frontend/ClassMemberKind.h"

#include "nativesharp/Language/OwnershipSemantics.h"

#include <cstdint>
#include <string>
#include <vector>

namespace nativesharp::frontend
{
struct PrecompiledMemberIdentity final
{
    ClassMemberKind kind = ClassMemberKind::Field;
    std::string declaringTypeKey;
    std::string name;
    std::string signatureKey;
    std::string mangledName;
    std::vector<language::EscapeClass> parameterEscapeSummary;
    bool operatorInOperandsNoEscape = false;
    bool receiverNoReferenceFieldEffects = false;
    bool verifiedNoEscapeOut = false;
    std::vector<std::vector<std::uint32_t>> pointerOutputObjectOrigins{};
};
}
