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

// Build cumulative content profiles while retaining feature flags for validation.
inline void projectFixtureMetadata(
    interop::LibraryMetadata& metadata,
    std::uint16_t profile)
{
    if (profile < 1) clearHistoricalLifetimeContracts(metadata);
    if (profile < 2)
    {
        for (auto& member : metadata.members) member.parameterEscapeSummary.clear();
        for (auto& function : metadata.namespaceFunctions)
            function.parameterEscapeSummary.clear();
    }
    if (profile < 3)
        for (auto& type : metadata.types) type.dropGlueEntryName.clear();
    if (profile < 4)
    {
        for (auto& type : metadata.types) type.delegateErrorTypeKey.clear();
        for (auto& member : metadata.members) member.declaredErrorTypeKey.clear();
        for (auto& function : metadata.namespaceFunctions)
            function.declaredErrorTypeKey.clear();
    }
    if (profile < 5)
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
