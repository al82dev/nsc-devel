#include "nativesharp/Sema/OwnershipKind.h"

namespace nativesharp::sema
{
std::string_view ownershipKindName(OwnershipKind kind) noexcept
{
    switch (kind)
    {
        case OwnershipKind::None: return "none";
        case OwnershipKind::Owned: return "owned";
        case OwnershipKind::Borrowed: return "borrowed";
    }
    return "none";
}
}
