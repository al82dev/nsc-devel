#pragma once

#include "nativesharp/Frontend/PrecompiledMemberIdentityMatchKind.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"

namespace nativesharp::frontend
{
struct PrecompiledMemberIdentityMatch final
{
    PrecompiledMemberIdentityMatchKind kind =
        PrecompiledMemberIdentityMatchKind::Missing;
    const PrecompiledMemberIdentity* identity = nullptr;
};
}
