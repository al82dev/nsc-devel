#pragma once

#include "nativesharp/Interop/LibraryMetadata.h"

#include <cstdint>

namespace nativesharp::tests
{
inline void clearHistoricalLifetimeContracts(interop::LibraryMetadata& metadata)
{
    for (auto& type : metadata.types)
    {
        type.lifetimeParameters.clear();
        type.lifetimeOutlivesConstraints.clear();
        type.delegateReferenceReturn = false;
        type.delegateReturnLifetimeName.clear();
        type.delegateParameterPassKinds.clear();
        type.delegateParameterLifetimeNames.clear();
        for (auto& field : type.fields) field.lifetimeName.clear();
    }
    const auto clearCallable = [](auto& callable)
    {
        callable.returnLifetimeName.clear();
        callable.lifetimeParameters.clear();
        callable.lifetimeOutlivesConstraints.clear();
        for (auto& parameter : callable.parameters) parameter.lifetimeName.clear();
    };
    for (auto& member : metadata.members) clearCallable(member);
    for (auto& function : metadata.namespaceFunctions) clearCallable(function);
}

// Use only with a fixture whose source deliberately exercises the common historical
// contract. The helper removes fields added after the selected minor, but it leaves
// feature flags such as bit fields, variants, traits, Copy and parameter packs intact
// so LibraryMetadataCodec still rejects an invalid down-level representation.
inline void projectHistoricalFixtureMetadata(
    interop::LibraryMetadata& metadata,
    std::uint16_t minor)
{
    if (minor < 34) clearHistoricalLifetimeContracts(metadata);
    if (minor < 35)
    {
        for (auto& member : metadata.members) member.parameterEscapeSummary.clear();
        for (auto& function : metadata.namespaceFunctions)
            function.parameterEscapeSummary.clear();
    }
    if (minor < 36)
        for (auto& type : metadata.types) type.dropGlueEntryName.clear();
    if (minor < 37)
    {
        for (auto& type : metadata.types) type.delegateErrorTypeKey.clear();
        for (auto& member : metadata.members) member.declaredErrorTypeKey.clear();
        for (auto& function : metadata.namespaceFunctions)
            function.declaredErrorTypeKey.clear();
    }
    if (minor < 38)
    {
        for (auto& generic : metadata.genericTemplates)
        {
            generic.parameterContracts.clear();
            generic.lifetimeParameters.clear();
            generic.lifetimeOutlivesConstraints.clear();
        }
    }
}
}
