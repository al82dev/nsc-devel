#include "../Internal/DriverInternal.h"
#include "../../Frontend/Cache/SyntaxProductCacheCodec.h"
#include "nativesharp/Compilation/PrecompiledProductSidecarCodec.h"
#include "nativesharp/Frontend/PrecompiledTemplateSources.h"

namespace nativesharp::driver
{
using target::BuiltinLayoutKind;

bool buildPrecompiledPublicSource(const interop::LibraryMetadata &metadata, std::string &source, std::string &error)
{
    if (!PublicSurfaceEmitter::build(metadata, source, error)) return false;
    for (const std::string& name : metadata.sourceNamespaces)
    {
        // The producer's own declarations define namespace existence even if every contained
        // type is internal and omitted from the public API. A shell exposes no such type.
        bool atSegmentStart = true;
        for (const char c : name)
        {
            const bool letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
            if (c == '.')
            {
                if (atSegmentStart) { error = "invalid metadata source namespace"; return false; }
                atSegmentStart = true;
            }
            else if (letter || (!atSegmentStart && c >= '0' && c <= '9')) atSegmentStart = false;
            else { error = "invalid metadata source namespace"; return false; }
        }
        if (atSegmentStart) { error = "invalid metadata source namespace"; return false; }
        source += "\nnamespace " + name + " { }\n";
    }
    return true;
}

bool verifyPrecompiledPublicSource(const interop::LibraryMetadata &metadata, std::string &error)
{
    std::string source;
    return buildPrecompiledPublicSource(metadata, source, error) &&
        PublicSurfaceEmitter::verify(metadata, std::move(source), error);
}

namespace
{
bool renderTemplateImport(const interop::LibraryMetadataGenericTemplate::SourceImport& directive,
                          std::string& text, std::string& error)
{
    text.clear();
    if (directive.reexport) text += "export ";
    switch (directive.importKind)
    {
    case 0:
        text += "using ";
        if (!directive.alias.empty()) text += directive.alias + " = ";
        text += directive.target;
        break;
    case 1: text += "import module " + directive.target; break;
    case 2: text += "import symbol " + directive.target; break;
    case 3: text += "import trait " + directive.target; break;
    case 4: text += "import extensions " + directive.target; break;
    case 5: text += "import " + directive.target + ".*"; break;
    default: error = "invalid template import kind"; return false;
    }
    if (directive.importKind != 0U && !directive.alias.empty())
        text += " as " + directive.alias;
    text += ";\n";

    // The metadata may be untrusted. Parse the isolated directive and require exactly one
    // directive of the claimed kind before placing it beside a reusable declaration.
    source::SourceManager sources;
    diagnostics::DiagnosticEngine diagnostics;
    const FileId fileId = sources.addBuffer("template-import.ns", text);
    const source::SourceFile* file = sources.getFile(fileId);
    frontend::ProjectSyntaxUnit unit(0, 0, "template-import", fileId, false);
    if (file == nullptr || !unit.lex(*file, diagnostics) ||
        !unit.parse(diagnostics, std::span<const std::string>{}) ||
        unit.syntaxTree() == nullptr || unit.syntaxTree()->children().size() != 1U)
    {
        error = "invalid template source import syntax";
        return false;
    }
    const ast::AstNode* node = unit.syntaxTree()->children().front();
    const bool expectAlias = !directive.alias.empty() || directive.importKind == 2U ||
        directive.importKind == 3U;
    if (node == nullptr ||
        node->kind() != (expectAlias ? ast::AstNodeKind::UsingAliasDirective
                                     : ast::AstNodeKind::UsingNamespaceDirective) ||
        node->integerValue() != directive.importKind || node->flag() != directive.reexport)
    {
        error = "template source import does not match its declared kind";
        return false;
    }
    return true;
}

bool appendTemplateImports(std::ostringstream& generated,
                           const interop::LibraryMetadataGenericTemplate& generic,
                           std::string_view namespaceName, std::string& error)
{
    for (const auto& directive : generic.sourceImports)
    {
        if (directive.namespaceName != namespaceName) continue;
        std::string text;
        if (!renderTemplateImport(directive, text, error)) return false;
        generated << text;
    }
    return true;
}
}

bool buildPrecompiledGenericTemplateSource(const interop::LibraryMetadata &metadata, std::string &source,
                                           std::string &error)
{
    constexpr std::uint8_t kGenericMethodKind = 3U;
    const bool isolated = std::ranges::any_of(metadata.genericTemplates,
        [](const auto& generic) { return !generic.sourceImports.empty(); });
    std::vector<std::string> units;
    for (const interop::LibraryMetadataGenericTemplate &generic : metadata.genericTemplates)
    {
        if (generic.sourceText.empty())
        {
            // NSMETA <= 1.6 knew the identity but did not promise a materializable
            // body.
            continue;
        }
        std::ostringstream generated;
        if (!appendTemplateImports(generated, generic, {}, error)) return false;
        std::size_t namespaceCount = 0;
        std::string namespacePrefix;
        std::size_t namespaceBegin = 0;
        while (namespaceBegin < generic.namespaceName.size())
        {
            const std::size_t dot = generic.namespaceName.find('.', namespaceBegin);
            const std::size_t stop = dot == std::string::npos ? generic.namespaceName.size() : dot;
            const std::string segment = generic.namespaceName.substr(namespaceBegin, stop - namespaceBegin);
            if (segment.empty()) { error = "empty template namespace segment"; return false; }
            if (!namespacePrefix.empty()) namespacePrefix.push_back('.');
            namespacePrefix += segment;
            generated << "namespace " << segment << "\n{\n";
            ++namespaceCount;
            if (!appendTemplateImports(generated, generic, namespacePrefix, error)) return false;
            if (dot == std::string::npos) break;
            namespaceBegin = dot + 1U;
        }
        if (generic.kind == kGenericMethodKind)
        {
            if (generic.ownerName.empty())
            {
                generated << generic.sourceText << std::endl;
                while (namespaceCount > 0U) { generated << "}\n"; --namespaceCount; }
                units.push_back(generated.str());
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
        while (namespaceCount > 0U) { generated << "}\n"; --namespaceCount; }
        units.push_back(generated.str());
    }
    if (isolated) source = frontend::encodePrecompiledTemplateSources(units);
    else
    {
        source.clear();
        for (const std::string& unit : units) source += unit;
    }
    return true;
}

namespace
{
bool verifyPrecompiledGenericTemplateSourceText(
    const interop::LibraryMetadata& metadata,
    std::string generated,
    std::string* syntaxProduct,
    std::string& error)
{
    if (syntaxProduct != nullptr) syntaxProduct->clear();
    if (generated.empty())
        return true;
    if (frontend::isPrecompiledTemplateFrame(generated))
    {
        std::vector<std::string> units;
        if (!frontend::decodePrecompiledTemplateSources(generated, units, error)) return false;
        for (std::string& source : units)
            if (!verifyPrecompiledGenericTemplateSourceText(metadata, std::move(source), nullptr, error))
                return false;
        return true;
    }

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
    {
        if (syntaxProduct != nullptr)
            *syntaxProduct = frontend::internal::serializeSyntaxProductUnit(unit);
        return true;
    }

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
}

bool verifyPrecompiledGenericTemplateSource(const interop::LibraryMetadata &metadata, std::string &error)
{
    std::string generated;
    if (!buildPrecompiledGenericTemplateSource(metadata, generated, error))
        return false;
    return verifyPrecompiledGenericTemplateSourceText(
        metadata, std::move(generated), nullptr, error);
}

bool writeVerifiedLibraryMetadata(const frontend::ProjectCompilation &compilation,
                                  const std::filesystem::path &destination, std::string &error,
                                  interop::LibraryMetadata *writtenModel)
{
    interop::LibraryMetadataCodec codec;
    interop::LibraryMetadata written;
    std::vector<std::uint8_t> metadataBytes;
    compilation::PrecompiledProductSidecar sidecar;
    if (codec.write(compilation, destination, written, metadataBytes, error) &&
        codec.verifyCurrentCompiler(written, error) &&
        buildPrecompiledPublicSource(written, sidecar.publicApiSource, error) &&
        buildPrecompiledGenericTemplateSource(written, sidecar.publicTemplateSource, error) &&
        PublicSurfaceEmitter::verify(
            written, sidecar.publicApiSource, sidecar.publicApiSyntax, error) &&
        verifyPrecompiledGenericTemplateSourceText(
            written,
            sidecar.publicTemplateSource,
            &sidecar.publicTemplateSyntax,
            error) &&
        compilation::PrecompiledProductSidecarCodec::write(
            destination, sidecar, metadataBytes, error))
    {
        if (writtenModel != nullptr)
            *writtenModel = std::move(written);
        return true;
    }

    std::error_code ignored;
    std::filesystem::remove(destination, ignored);
    std::filesystem::remove(
        compilation::PrecompiledProductSidecarCodec::pathForMetadata(destination), ignored);
    error = "metadata producer read-back verification failed: " + error;
    return false;
}
} // namespace nativesharp::driver
