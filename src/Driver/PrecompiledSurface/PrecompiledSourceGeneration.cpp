#include "../Internal/DriverInternal.h"

namespace nativesharp::driver
{
using target::BuiltinLayoutKind;

bool buildPrecompiledPublicSource(const interop::LibraryMetadata &metadata, std::string &source, std::string &error)
{
    return PublicSurfaceEmitter::build(metadata, source, error);
}

bool verifyPrecompiledPublicSource(const interop::LibraryMetadata &metadata, std::string &error)
{
    return PublicSurfaceEmitter::verify(metadata, error);
}

bool buildPrecompiledGenericTemplateSource(const interop::LibraryMetadata &metadata, std::string &source,
                                           std::string &error)
{
    constexpr std::uint8_t kGenericMethodKind = 3U;
    std::ostringstream generated;
    for (const interop::LibraryMetadataGenericTemplate &generic : metadata.genericTemplates)
    {
        if (generic.sourceText.empty())
        {
            // NSMETA <= 1.6 knew the identity but did not promise a materializable
            // body.
            continue;
        }
        if (!generic.namespaceName.empty())
            generated << "namespace " << generic.namespaceName << "\n{\n";
        if (generic.kind == kGenericMethodKind)
        {
            if (generic.ownerName.empty())
            {
                generated << generic.sourceText << std::endl;
                if (!generic.namespaceName.empty())
                    generated << static_cast<char>(125) << std::endl;
                continue;
            }

            std::size_t openedOwners = 0;
            std::string ownerPrefix;
            std::size_t begin = 0;
            while (begin <= generic.ownerName.size())
            {
                const std::size_t separator = generic.ownerName.find('+', begin);
                const std::size_t stop = separator == std::string::npos ? generic.ownerName.size() : separator;
                if (stop == begin)
                {
                    error =
                        "metadata generic method '" + generic.canonicalKey + "' has an empty declaring-type segment";
                    return false;
                }
                const std::string segment = generic.ownerName.substr(begin, stop - begin);
                if (!ownerPrefix.empty())
                    ownerPrefix.push_back('+');
                ownerPrefix += segment;
                const std::string fullyQualifiedOwner =
                    generic.namespaceName.empty() ? ownerPrefix : generic.namespaceName + "." + ownerPrefix;
                const interop::LibraryMetadataType *owner =
                    metadataTypeForKey(metadata, "type:" + metadata.moduleName + "::" + fullyQualifiedOwner);
                if (owner == nullptr || owner->kind != interop::LibraryMetadataTypeKind::Class)
                {
                    error = "metadata generic method '" + generic.canonicalKey +
                            "' does not have a materializable class owner '" + fullyQualifiedOwner + "'";
                    return false;
                }
                generated << sourceAccessibility(owner->accessibility) << " partial ";
                if ((owner->flags & (1U << 1U)) != 0U)
                    generated << "abstract ";
                if ((owner->flags & (1U << 2U)) != 0U)
                    generated << "sealed ";
                if ((owner->flags & (1U << 3U)) != 0U)
                    generated << "static ";
                generated << "class " << segment << "\n{\n";
                ++openedOwners;
                if (separator == std::string::npos)
                    break;
                begin = separator + 1U;
            }
            generated << generic.sourceText << '\n';
            while (openedOwners-- > 0U)
                generated << "}\n";
        }
        else
        {
            generated << generic.sourceText << '\n';
        }
        if (!generic.namespaceName.empty())
            generated << "}\n";
    }
    source = generated.str();
    return true;
}

bool verifyPrecompiledGenericTemplateSource(const interop::LibraryMetadata &metadata, std::string &error)
{
    std::string generated;
    if (!buildPrecompiledGenericTemplateSource(metadata, generated, error))
        return false;
    if (generated.empty())
        return true;

    source::SourceManager sources;
    diagnostics::DiagnosticEngine diagnostics;
    std::filesystem::path displayPath = metadata.moduleName;
    displayPath += ".nsmeta.public-templates.ns";
    const FileId fileId = sources.addBuffer(displayPath, std::move(generated));
    const source::SourceFile *file = sources.getFile(fileId);
    if (file == nullptr)
    {
        error = "generated precompiled template surface has no source buffer";
        return false;
    }

    std::vector<std::string> knownTypeNames;
    const auto appendKnownType = [&](const std::string &typeKey)
    { appendParserKnownTypeNames(metadata, typeKey, knownTypeNames); };
    for (const interop::LibraryMetadataType &type : metadata.types)
        appendKnownType("type:" + type.identityKey);
    constexpr std::uint8_t kGenericMethodKind = 3U;
    for (const interop::LibraryMetadataGenericTemplate &generic : metadata.genericTemplates)
        if (generic.kind != kGenericMethodKind)
            appendKnownType("type:" + generic.canonicalKey);
    for (const std::string &typeKey : metadata.externalTypeKeys)
        appendKnownType(typeKey);
    std::sort(knownTypeNames.begin(), knownTypeNames.end());
    knownTypeNames.erase(std::unique(knownTypeNames.begin(), knownTypeNames.end()), knownTypeNames.end());

    frontend::ProjectSyntaxUnit unit(0, 0, metadata.moduleName, fileId, false);
    if (unit.lex(*file, diagnostics) && unit.parse(diagnostics, knownTypeNames))
        return true;

    error = "generated precompiled public template surface is not valid NativeSharp source";
    if (!diagnostics.diagnostics().empty())
    {
        std::ostringstream rendered;
        diagnostics::DiagnosticRenderer renderer(sources);
        renderer.renderAll(diagnostics, rendered);
        error += ":\n" + rendered.str();
    }
    return false;
}

bool writeVerifiedLibraryMetadata(const frontend::ProjectCompilation &compilation,
                                  const std::filesystem::path &destination, std::string &error,
                                  interop::LibraryMetadata *writtenModel)
{
    interop::LibraryMetadataCodec codec;
    interop::LibraryMetadata written;
    if (codec.write(compilation, destination, written, error) && codec.verifyCurrentCompiler(written, error) &&
        verifyPrecompiledPublicSource(written, error) && verifyPrecompiledGenericTemplateSource(written, error))
    {
        if (writtenModel != nullptr)
            *writtenModel = std::move(written);
        return true;
    }

    std::error_code ignored;
    std::filesystem::remove(destination, ignored);
    error = "metadata producer read-back verification failed: " + error;
    return false;
}
} // namespace nativesharp::driver
