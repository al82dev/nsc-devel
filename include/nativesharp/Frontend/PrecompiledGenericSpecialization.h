#pragma once

#include "nativesharp/Frontend/GenericTemplateKind.h"

#include <string>
#include <vector>

namespace nativesharp::frontend
{
// Closed code that is already present in a referenced binary. Seeding the generic cache with
// these identities makes an equal consumer request bind to the producer implementation instead
// of cloning a second specialization into the consumer.
struct PrecompiledGenericSpecialization final
{
    GenericTemplateKind kind = GenericTemplateKind::Class;
    std::string templateKey;
    std::string specializationKey;
    std::string specializedName;
    std::vector<std::string> typeArguments;
    // True when the importing module's ordinary public-API source already declares the concrete
    // generated name.  A cross-module specialization used only through a published signature has
    // no such declaration: it must be materialized from its open template instead of merely
    // seeding the cache with a name that name lookup can never resolve.
    bool hasConcreteDeclaration = false;
};
}
