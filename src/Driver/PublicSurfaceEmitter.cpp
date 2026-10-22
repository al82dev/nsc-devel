#include "PublicSurfaceEmitter.h"
#include "PublicSurfaceBuilder.h"
#include "PublicSurfaceEmitterSupport.h"
#include "../Frontend/Cache/SyntaxProductCacheCodec.h"

#include "nativesharp/Diagnostics/DiagnosticRenderer.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Source/SourceFile.h"

#include <algorithm>
#include <array>
#include <bit>
#include <filesystem>
#include <functional>
#include <limits>
#include <map>
#include <ranges>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nativesharp::driver
{
using namespace public_surface_support;

bool PublicSurfaceEmitter::build(
    const interop::LibraryMetadata& metadata,
    std::string& source,
    std::string& error)
{
    PublicSurfaceBuilder builder(metadata, source, error);
    return builder.run();
}

bool PublicSurfaceEmitter::verify(const interop::LibraryMetadata &metadata, std::string &error)
{
    std::string generated;
    if (!build(metadata, generated, error)) return false;
    return verifyImpl(metadata, std::move(generated), nullptr, error);
}

bool PublicSurfaceEmitter::verify(
    const interop::LibraryMetadata& metadata,
    std::string generated,
    std::string& error)
{
    return verifyImpl(metadata, std::move(generated), nullptr, error);
}

bool PublicSurfaceEmitter::verify(
    const interop::LibraryMetadata& metadata,
    std::string generated,
    std::string& syntaxProduct,
    std::string& error)
{
    syntaxProduct.clear();
    return verifyImpl(metadata, std::move(generated), &syntaxProduct, error);
}

bool PublicSurfaceEmitter::verifyImpl(
    const interop::LibraryMetadata& metadata,
    std::string generated,
    std::string* syntaxProduct,
    std::string& error)
{
    source::SourceManager sources;
    diagnostics::DiagnosticEngine diagnostics;
    std::filesystem::path displayPath = metadata.moduleName;
    displayPath += ".nsmeta.public-api.ns";
    const FileId fileId = sources.addBuffer(displayPath, std::move(generated));
    const source::SourceFile *file = sources.getFile(fileId);
    if (file == nullptr)
    {
        error = "generated precompiled public API has no source buffer";
        return false;
    }
    frontend::ProjectSyntaxUnit unit(
        0,
        0,
        metadata.moduleName,
        fileId,
        true);
    std::vector<std::string> knownTypeNames;
    const auto appendKnownType = [&](const std::string &typeKey) {
        appendParserKnownTypeNames(metadata, typeKey, knownTypeNames);
    };
    for (const interop::LibraryMetadataType &type : metadata.types)
    {
        appendKnownType("type:" + type.identityKey);
        appendKnownType(type.baseTypeKey);
        for (const std::string &interfaceTypeKey : type.interfaceTypeKeys) appendKnownType(interfaceTypeKey);
        for (const interop::LibraryMetadataField &field : type.fields) appendKnownType(field.typeKey);
        appendKnownType(type.enumUnderlyingTypeKey);
        appendKnownType(type.delegateReturnTypeKey);
        appendKnownType(type.delegateErrorTypeKey);
        for (const std::string &parameterTypeKey : type.delegateParameterTypeKeys) appendKnownType(parameterTypeKey);
    }
    for (const interop::LibraryMetadataMember &member : metadata.members)
    {
        appendKnownType(member.declaringTypeKey);
        appendKnownType(member.declaredTypeKey);
        appendKnownType(member.declaredErrorTypeKey);
        for (const interop::LibraryMetadataParameter &parameter : member.parameters) appendKnownType(parameter.typeKey);
    }
    for (const interop::LibraryMetadataNamespaceFunction &function :
         metadata.namespaceFunctions)
    {
        appendKnownType(function.declaredTypeKey);
        appendKnownType(function.declaredErrorTypeKey);
        for (const interop::LibraryMetadataParameter &parameter :
             function.parameters)
        {
            appendKnownType(parameter.typeKey);
        }
    }
    for (const interop::LibraryMetadataNamespaceState &state :
         metadata.namespaceStates)
    {
        appendKnownType(state.declaredTypeKey);
    }
    constexpr std::uint8_t kGenericMethodKind = 3U;
    for (const interop::LibraryMetadataGenericTemplate &generic : metadata.genericTemplates)
        if (generic.kind != kGenericMethodKind) appendKnownType("type:" + generic.canonicalKey);
    std::sort(knownTypeNames.begin(), knownTypeNames.end());
    knownTypeNames.erase(std::unique(knownTypeNames.begin(), knownTypeNames.end()), knownTypeNames.end());
    if (unit.lex(*file, diagnostics) && unit.parse(diagnostics, knownTypeNames))
    {
        if (syntaxProduct != nullptr)
            *syntaxProduct = frontend::internal::serializeSyntaxProductUnit(unit);
        return true;
    }

    error = "generated precompiled public API is not valid NativeSharp source";
    if (!diagnostics.diagnostics().empty())
    {
        std::ostringstream rendered;
        diagnostics::DiagnosticRenderer renderer(sources);
        renderer.renderAll(diagnostics, rendered);
        error += ":\n" + rendered.str();
    }
    return false;
}

} // namespace nativesharp::driver
