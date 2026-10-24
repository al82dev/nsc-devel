#include "nativesharp/Frontend/ProjectCompilation.h"
#include "Cache/CachePayloadReader.h"
#include "Cache/CachePayloadWriter.h"
#include "Cache/SyntaxProductCacheCodec.h"
#include "Internal/ProjectPhaseTimer.h"
#include "Internal/IndexShadowCheckSwitch.h"
#include "Internal/IndexShadowMismatch.h"
#include "nativesharp/Frontend/ComptimeEngine.h"
#include "nativesharp/Frontend/TemplateParseCache.h"

#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Basic/Version.h"
#include "nativesharp/Compilation/ProjectGraphLoader.h"
#include "nativesharp/Driver/CompilationWorkerPool.h"
#include "nativesharp/Lexer/TokenKind.h"
#include "nativesharp/Source/SourceFile.h"
#include "nativesharp/Source/SourcePosition.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <set>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
void ProjectCompilation::setLifetime(ProjectCompilationLifetime& lifetime) noexcept
{
    lifetime_ = &lifetime;
    lifetime_->setWorkerCount(compilationJobs_);
}

void ProjectCompilation::setCompilationJobs(std::size_t jobs) noexcept
{
    compilationJobs_ = std::max<std::size_t>(1, jobs);
    lifetime_->setWorkerCount(compilationJobs_);
}

void ProjectCompilation::setProjectLoadOptions(compilation::ProjectLoadOptions options)
{
    projectLoadOptions_ = std::move(options);
}

void ProjectCompilation::setTemplateCacheRoot(std::filesystem::path root)
{
    templateCacheRoot_ = std::move(root);
    precompiledProductCacheRoot_ = templateCacheRoot_.empty()
        ? std::filesystem::path{}
        : templateCacheRoot_ / "precompiled-products";
    lifetime_->metadataRepository().setCacheRoot(
        templateCacheRoot_.empty()
            ? std::filesystem::path{}
            : templateCacheRoot_ / "verified-nsmeta-models");
}

const std::filesystem::path& ProjectCompilation::precompiledProductCacheRoot() const noexcept
{
    return precompiledProductCacheRoot_;
}

void ProjectCompilation::setCancellationCheck(std::function<bool()> check)
{
    cancellationCheck_ = std::move(check);
}

void ProjectCompilation::setSourceBufferProvider(SourceBufferProvider provider)
{
    sourceBufferProvider_ = std::move(provider);
}

void ProjectCompilation::setPhaseTimingSink(
    std::function<void(std::string_view, std::chrono::nanoseconds)> sink)
{
    phaseTimingSink_ = std::move(sink);
}

void ProjectCompilation::setProgressSink(
    std::function<void(const basic::CompilationEvent&)> sink)
{
    progressSink_ = std::move(sink);
}

#if 0 // INSTRUMENTATION CODE: detailed generics trace support.
void ProjectCompilation::setDetailedProgressSink(
    std::function<void(const basic::CompilationEvent&)> sink)
{
    detailedProgressSink_ = std::move(sink);
}
#endif // INSTRUMENTATION CODE

bool ProjectCompilation::cancellationRequested(std::string& loadError, std::string_view nextPhase) const
{
    if (!cancellationCheck_ || !cancellationCheck_()) return false;
    loadError = "compilation cancelled before phase '" + std::string(nextPhase) + "'";
    return true;
}
namespace
{
using lexer::Token;
using lexer::TokenKind;

using internal::ProjectPhaseTimer;

constexpr std::string_view templateParseProductSchema = "nsc-template-parse-product-v1";

// Appends one length-delimited field to the cache identity. Length framing prevents different
// sequences of module names, ordinals or digests from producing the same unhashed material.
void appendTemplateParseCacheKeyField(std::string& material, std::string_view value)
{
    material += std::to_string(value.size());
    material.push_back(':');
    material.append(value);
    material.push_back(';');
}

// Hashes one source buffer without copying it into the aggregate key material. The returned text
// has a fixed width, so the aggregate identity remains small even for large precompiled sources.
std::string templateParseSourceDigest(std::string_view source)
{
    const std::span<const std::uint8_t> bytes{
        reinterpret_cast<const std::uint8_t*>(source.data()), source.size()};
    return basic::sha256Hex(basic::sha256(bytes));
}

std::string precompiledSyntaxCacheKey(
    std::string_view productCacheKey,
    std::size_t sourceOrdinal)
{
    if (productCacheKey.empty()) return {};
    std::string material;
    // EN: v2 retires every v1 entry: v1 published trees already rewritten by the generic pass.
    // ES: v2 retira toda entrada v1: v1 publicaba árboles ya reescritos por el pase genérico.
    appendTemplateParseCacheKeyField(material, "nsc-precompiled-syntax-v2");
    appendTemplateParseCacheKeyField(material, productCacheKey);
    appendTemplateParseCacheKeyField(material, std::to_string(sourceOrdinal));
    return templateParseSourceDigest(material);
}

// Reads and validates the stable identity of one syntax unit. A mismatch is a cache miss because
// installing the tree into a different ordinal or FileId would change lookup and diagnostics.
bool readTemplateParseUnitIdentity(
    internal::CachePayloadReader& reader,
    const ProjectSyntaxUnit& unit)
{
    std::uint64_t projectOrdinal = 0;
    std::uint64_t sourceOrdinal = 0;
    std::string moduleName;
    std::uint32_t fileId = 0;
    std::uint8_t precompiled = 0;
    return reader.u64(projectOrdinal) && reader.u64(sourceOrdinal) &&
        reader.string(moduleName) && reader.u32(fileId) && reader.byte(precompiled) &&
        projectOrdinal == unit.projectOrdinal() &&
        sourceOrdinal == unit.sourceOrdinal() &&
        moduleName == unit.moduleName() && fileId == unit.fileId().value() &&
        (precompiled != 0U) == unit.isPrecompiled();
}

void reportSourceProgress(
    const std::function<void(const basic::CompilationEvent&)>& sink,
    std::string_view phase,
    const std::filesystem::path& path,
    std::size_t completed,
    std::size_t total)
{
    if (!sink) return;
    const std::string detail = path.generic_string();
    sink({basic::CompilationEventKind::WorkFinished,
        phase, detail, completed, total, {}});
}

bool isMetadataBackedVirtualSource(const std::filesystem::path& path)
{
    const std::string sourcePath = path.generic_string();
    return sourcePath.ends_with(".nsmeta.public-api.ns") ||
        sourcePath.ends_with(".nsmeta.public-templates.ns");
}

bool sourceMayRequireComptime(std::string_view source) noexcept
{
    return source.find('#') != std::string_view::npos ||
        source.find("comptime") != std::string_view::npos ||
        source.find("__nsc_macro_") != std::string_view::npos;
}

ComptimeResult makePassthroughComptimeResult(std::string_view source)
{
    ComptimeResult result;
    const ComptimeLimits limits;
    if (source.size() > limits.maximumSourceBytes)
    {
        result.error = "comptime source byte limit exceeded";
        return result;
    }
    if (source.size() > limits.maximumMemoryBytes)
    {
        result.error = "comptime memory budget exceeded by source buffer";
        return result;
    }

    constexpr std::string_view DigestPrefix = "nativesharp-comptime-result-v1\n";
    std::string digestMaterial;
    digestMaterial.reserve(DigestPrefix.size() + source.size() + 1U);
    digestMaterial.append(DigestPrefix);
    digestMaterial.append(source);
    digestMaterial.push_back('\n');
    const std::span<const std::uint8_t> bytes{
        reinterpret_cast<const std::uint8_t*>(digestMaterial.data()), digestMaterial.size()};
    result.reproducibilityDigest = basic::sha256Hex(basic::sha256(bytes));
    result.memoryUsed = source.size();
    result.success = true;
    return result;
}

std::size_t visibleSourceCount(
    const std::vector<std::unique_ptr<ProjectSyntaxUnit>>& units,
    const source::SourceManager& sourceManager)
{
    std::size_t count = 0;
    for (const std::unique_ptr<ProjectSyntaxUnit>& unit : units)
    {
        const std::filesystem::path path(sourceManager.getPath(
            SourceLocation(unit->fileId(), 0U)));
        if (!isMetadataBackedVirtualSource(path)) ++count;
    }
    return count;
}

bool waitForSyntaxWorkers(
    driver::CompilationWorkerPool& pool,
    std::string_view phase,
    std::string& error)
{
    try
    {
        pool.wait();
        return true;
    }
    catch (const std::exception& exception)
    {
        error = "parallel " + std::string(phase) + " failed: " + exception.what();
    }
    catch (...)
    {
        error = "parallel " + std::string(phase) + " failed with a non-standard exception";
    }
    return false;
}

std::string joinName(std::span<const std::string> prefix, std::string_view name)
{
    std::string result;
    for (const std::string& segment : prefix)
    {
        if (!result.empty())
        {
            result.push_back('.');
        }
        result += segment;
    }
    if (!result.empty() && !name.empty())
    {
        result.push_back('.');
    }
    result += name;
    return result;
}

bool readQualifiedName(
    std::span<const Token> tokens,
    std::size_t& index,
    std::vector<std::string>& segments)
{
    segments.clear();
    if (index >= tokens.size() || tokens[index].kind() != TokenKind::Identifier)
    {
        return false;
    }

    segments.emplace_back(tokens[index].lexeme());
    ++index;
    while (index + 1 < tokens.size() &&
           tokens[index].kind() == TokenKind::Dot &&
           tokens[index + 1].kind() == TokenKind::Identifier)
    {
        segments.emplace_back(tokens[index + 1].lexeme());
        index += 2;
    }
    return true;
}

void skipBalancedBlock(std::span<const Token> tokens, std::size_t& index)
{
    if (index >= tokens.size() || tokens[index].kind() != TokenKind::LeftBrace)
    {
        return;
    }

    int depth = 0;
    while (index < tokens.size())
    {
        if (tokens[index].kind() == TokenKind::LeftBrace)
        {
            ++depth;
        }
        else if (tokens[index].kind() == TokenKind::RightBrace)
        {
            --depth;
            ++index;
            if (depth == 0)
            {
                return;
            }
            continue;
        }
        else if (tokens[index].kind() == TokenKind::EndOfFile)
        {
            return;
        }
        ++index;
    }
}

// NSC-LIMIT-009. Las formas bajo las que un tipo puede escribirse, sembradas todas. Para un tipo
// de namespace son las dos de siempre --`Item` y `ns.Item`--; para uno ANIDADO son ademas las
// intermedias, `Outer.Item` y `ns.Outer.Item`, porque el nombre fuente de un anidado lleva punto y
// el parser tiene que reconocerlo escrito desde dentro del contenedor y desde fuera.
//
// Aqui NO hay identidad canonica: esto es la lista de nombres con la que el parser desambigua
// `Foo x;` de una expresion, y nada mas. La identidad --con `+`-- la construye TypeIdentity, en
// otra fase y a partir del arbol, no de esta tabla.
void insertTypeSeeds(
    std::set<std::string>& names,
    const std::vector<std::string>& namespacePrefix,
    const std::vector<std::string>& typeChain)
{
    if (typeChain.empty())
    {
        return;
    }
    for (std::size_t start = 0; start < typeChain.size(); ++start)
    {
        std::string spelling;
        for (std::size_t index = start; index < typeChain.size(); ++index)
        {
            if (!spelling.empty()) spelling.push_back('.');
            spelling += typeChain[index];
        }
        names.insert(spelling);
        names.insert(joinName(namespacePrefix, spelling));
    }
}

void collectTypeSeedsInScope(
    std::span<const Token> tokens,
    std::size_t& index,
    const std::vector<std::string>& namespacePrefix,
    const std::vector<std::string>& containingTypes,
    TokenKind terminator,
    std::set<std::string>& names);

bool collectNamespaceTypeSeeds(
    std::span<const Token> tokens,
    std::size_t& index,
    const std::vector<std::string>& namespacePrefix,
    const std::vector<std::string>& containingTypes,
    TokenKind terminator,
    std::set<std::string>& names,
    bool& terminateScope)
{
    if (tokens[index].kind() != TokenKind::KwNamespace) return false;
    ++index;
    std::vector<std::string> declaredSegments;
    if (!readQualifiedName(tokens, index, declaredSegments)) return true;

    std::vector<std::string> nested = namespacePrefix;
    nested.insert(nested.end(), declaredSegments.begin(), declaredSegments.end());
    if (index < tokens.size() && tokens[index].kind() == TokenKind::Semicolon)
    {
        ++index;
        collectTypeSeedsInScope(tokens, index, nested, containingTypes, terminator, names);
        terminateScope = true;
        return true;
    }
    if (index < tokens.size() && tokens[index].kind() == TokenKind::LeftBrace)
    {
        ++index;
        collectTypeSeedsInScope(
            tokens, index, nested, containingTypes, TokenKind::RightBrace, names);
        if (index < tokens.size() && tokens[index].kind() == TokenKind::RightBrace) ++index;
    }
    return true;
}

bool collectDeclaredTypeSeed(
    std::span<const Token> tokens,
    std::size_t& index,
    const std::vector<std::string>& namespacePrefix,
    const std::vector<std::string>& containingTypes,
    std::set<std::string>& names)
{
    const bool opaqueType = tokens[index].kind() == TokenKind::Identifier &&
        tokens[index].lexeme() == "opaque" && index + 1U < tokens.size() &&
        tokens[index + 1U].kind() == TokenKind::Identifier &&
        tokens[index + 1U].lexeme() == "type";
    const bool traitType = tokens[index].kind() == TokenKind::Identifier &&
        tokens[index].lexeme() == "trait" && index + 1U < tokens.size() &&
        tokens[index + 1U].kind() == TokenKind::Identifier;
    const bool variantType = tokens[index].kind() == TokenKind::Identifier &&
        tokens[index].lexeme() == "variant" && index + 1U < tokens.size() &&
        tokens[index + 1U].kind() == TokenKind::Identifier;
    const bool typeDeclaration = tokens[index].kind() == TokenKind::KwStruct ||
        (tokens[index].kind() == TokenKind::Identifier && tokens[index].lexeme() == "union") ||
        traitType || variantType || opaqueType ||
        tokens[index].kind() == TokenKind::KwClass ||
        tokens[index].kind() == TokenKind::KwInterface ||
        tokens[index].kind() == TokenKind::KwEnum;
    if (!typeDeclaration) return false;

    index += opaqueType ? 2U : 1U;
    std::vector<std::string> chain = containingTypes;
    if (index < tokens.size() && tokens[index].kind() == TokenKind::Identifier)
    {
        chain.emplace_back(tokens[index].lexeme());
        insertTypeSeeds(names, namespacePrefix, chain);
        ++index;
    }
    // NSC-LIMIT-009. Avanzar HASTA el cuerpo, y despues BAJAR a el.
    //
    // Las dos mitades importan. La primera porque entre el nombre y el `{` cabe una lista
    // de parametros genericos, una lista de bases, el tipo subyacente de un enum y una
    // clausula `where`: la version anterior solo miraba si el token siguiente al nombre
    // era `{`, de modo que en `class C<T> : Base { ... }` no reconocia el cuerpo y seguia
    // leyendo sus tokens en linea recta, como si fueran declaraciones del namespace.
    //
    // Y la segunda porque un cuerpo de tipo YA NO se salta: dentro puede haber otro tipo.
    // Bajar es lo unico que hace que `Outer.Item` exista para el parser. Los cuerpos de
    // metodo de dentro se saltan solos: los cierra la rama `LeftBrace` de mas abajo.
    while (index < tokens.size() && tokens[index].kind() != TokenKind::LeftBrace &&
           tokens[index].kind() != TokenKind::Semicolon &&
           tokens[index].kind() != TokenKind::RightBrace &&
           tokens[index].kind() != TokenKind::EndOfFile)
    {
        ++index;
    }
    if (index < tokens.size() && tokens[index].kind() == TokenKind::LeftBrace)
    {
        ++index;
        collectTypeSeedsInScope(
            tokens, index, namespacePrefix, chain, TokenKind::RightBrace, names);
        if (index < tokens.size() && tokens[index].kind() == TokenKind::RightBrace) ++index;
    }
    return true;
}

bool collectDelegateTypeSeed(
    std::span<const Token> tokens,
    std::size_t& index,
    const std::vector<std::string>& namespacePrefix,
    const std::vector<std::string>& containingTypes,
    std::set<std::string>& names)
{
    if (tokens[index].kind() != TokenKind::KwDelegate) return false;
    // Stage 52 fix: this scanner previously only seeded class/struct/interface/enum
    // names, never delegate names. That is invisible within a single file, because the
    // real parser (Parser::parseDelegateDeclaration) inserts the delegate's name into
    // its OWN incrementally-built knownTypeNames_ the moment it parses the declaration,
    // so a later use in the SAME translation unit already sees it. But a project that
    // only *references* another project's delegate (e.g. this package's own
    // `HashFunc<T>`/`Comparison<T>`/`Predicate<T>`/`Action<T>` from Delegates.ns, used by
    // a consuming project via <ProjectReference>) never physically parses that
    // declaration itself -- it relies entirely on this cross-file seed list, which was
    // missing the name. The concrete, reliably reproducible symptom: `Predicate<int> p =
    // someMethod;` in a project referencing this one failed to parse ("expected
    // expression") for a builtin type argument specifically (a class type argument
    // happened to disambiguate through a different path and did not expose the gap).
    //
    // A delegate declaration has the shape `delegate ReturnType Name<TParams>(params);`
    // (or `delegate ReturnType Name(params);` for a non-generic delegate) -- ReturnType
    // can itself be an arbitrary type, including a generic one, so the delegate's own
    // name is identified here as the first identifier directly followed by '<' or '('
    // (its generic-parameter list or its parameter list), skipping over any balanced
    // '<...>' the return type itself contributes first.
    ++index;
    while (index < tokens.size() && tokens[index].kind() != TokenKind::Semicolon &&
           tokens[index].kind() != TokenKind::LeftBrace)
    {
        if (tokens[index].kind() == TokenKind::Identifier && index + 1 < tokens.size() &&
            (tokens[index + 1].kind() == TokenKind::Less ||
             tokens[index + 1].kind() == TokenKind::LeftParen))
        {
            std::vector<std::string> chain = containingTypes;
            chain.emplace_back(tokens[index].lexeme());
            insertTypeSeeds(names, namespacePrefix, chain);
            ++index;
            break;
        }
        if (tokens[index].kind() == TokenKind::Less)
        {
            int depth = 1;
            ++index;
            while (index < tokens.size() && depth > 0 &&
                   tokens[index].kind() != TokenKind::Semicolon &&
                   tokens[index].kind() != TokenKind::LeftBrace)
            {
                if (tokens[index].kind() == TokenKind::Less) ++depth;
                else if (tokens[index].kind() == TokenKind::Greater) --depth;
                ++index;
            }
            continue;
        }
        ++index;
    }
    while (index < tokens.size() && tokens[index].kind() != TokenKind::Semicolon &&
           tokens[index].kind() != TokenKind::LeftBrace)
    {
        ++index;
    }
    if (index < tokens.size() && tokens[index].kind() == TokenKind::Semicolon) ++index;
    return true;
}

void collectTypeSeedsInScope(
    std::span<const Token> tokens,
    std::size_t& index,
    const std::vector<std::string>& namespacePrefix,
    const std::vector<std::string>& containingTypes,
    TokenKind terminator,
    std::set<std::string>& names)
{
    while (index < tokens.size() && tokens[index].kind() != TokenKind::EndOfFile)
    {
        if (terminator != TokenKind::Invalid && tokens[index].kind() == terminator)
        {
            return;
        }

        bool terminateScope = false;
        if (collectNamespaceTypeSeeds(
                tokens, index, namespacePrefix, containingTypes,
                terminator, names, terminateScope))
        {
            if (terminateScope) return;
            continue;
        }
        if (collectDeclaredTypeSeed(
                tokens, index, namespacePrefix, containingTypes, names)) continue;
        if (collectDelegateTypeSeed(
                tokens, index, namespacePrefix, containingTypes, names)) continue;

        if (tokens[index].kind() == TokenKind::LeftBrace)
        {
            skipBalancedBlock(tokens, index);
            continue;
        }
        ++index;
    }
}

void collectUsingAliases(
    std::span<const Token> tokens,
    std::vector<std::pair<std::string, std::string>>& aliases)
{
    std::size_t index = 0;
    while (index + 3 < tokens.size())
    {
        if (tokens[index].kind() == TokenKind::KwUsing &&
            tokens[index + 1].kind() == TokenKind::Identifier &&
            tokens[index + 2].kind() == TokenKind::Equal)
        {
            const std::string alias(tokens[index + 1].lexeme());
            index += 3;
            std::vector<std::string> targetSegments;
            if (readQualifiedName(tokens, index, targetSegments))
            {
                aliases.emplace_back(alias, joinName(targetSegments, ""));
            }
            continue;
        }
        ++index;
    }
}

std::vector<std::string> collectParserKnownTypeNames(
    std::span<const std::unique_ptr<ProjectSyntaxUnit>> units)
{
    std::set<std::string> names;
    std::vector<std::pair<std::string, std::string>> aliases;
    for (const std::unique_ptr<ProjectSyntaxUnit>& unit : units)
    {
        if (unit == nullptr || !unit->isLexicallyValid())
        {
            continue;
        }
        std::size_t index = 0;
        collectTypeSeedsInScope(unit->tokens(), index, {}, {}, TokenKind::Invalid, names);
        collectUsingAliases(unit->tokens(), aliases);
    }

    // baseNames comes from the std::set, so it is sorted and unique: the target itself is a binary
    // search and the names that start with "target." are one contiguous run beginning at the
    // lower bound of that prefix. The same names are inserted as by comparing every alias with
    // every name, and the result is the same sorted set.
    const std::vector<std::string> baseNames(names.begin(), names.end());
    for (const auto& [alias, target] : aliases)
    {
        if (std::binary_search(baseNames.begin(), baseNames.end(), target))
        {
            names.insert(alias);
        }
        const std::string prefix = target + ".";
        for (auto known = std::lower_bound(baseNames.begin(), baseNames.end(), prefix);
             known != baseNames.end() && known->starts_with(prefix); ++known)
        {
            names.insert(alias + known->substr(target.size()));
        }
    }
    return std::vector<std::string>(names.begin(), names.end());
}
}

bool ProjectCompilation::setPrecompiledModules(
    std::vector<PrecompiledSyntaxModule> modules,
    std::string& error)
{
    error.clear();
    if (projectGraph_.has_value() || !units_.empty())
    {
        error = "precompiled modules must be configured before project compilation starts";
        return false;
    }
    precompiledModules_ = std::move(modules);
    return true;
}

bool ProjectCompilation::setPreloadedProjectGraph(
    compilation::ProjectGraph graph,
    std::string& error)
{
    error.clear();
    if (projectGraph_.has_value() || !units_.empty() || genericModel_ != nullptr ||
        nameLookup_ != nullptr || memberModel_ != nullptr || namespaceFunctionModel_ != nullptr)
    {
        error = "project graph must be configured before project compilation starts";
        return false;
    }
    if (graph.rootProject() == nullptr)
    {
        error = "preloaded project graph has no root project";
        return false;
    }
    projectGraph_ = std::move(graph);
    return true;
}

bool ProjectCompilation::compileProjectSyntax(
    const std::filesystem::path& projectFile,
    std::string& loadError)
{
    return compileSyntaxCore(projectFile, loadError);
}

bool ProjectCompilation::compileProjectNames(
    const std::filesystem::path& projectFile,
    std::string& loadError)
{
    if (!compileProjectGenerics(projectFile, loadError))
    {
        return false;
    }
    if (cancellationRequested(loadError, "names")) return false;

    const ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "names");
    nameLookup_ = std::make_unique<ProjectNameLookup>(lifetime_->indexes());
    if (!nameLookup_->build(
            *projectGraph_, units_, diagnostics_, compilationJobs_,
            &lifetime_->workerPool()))
    {
        return false;
    }
    return enterStage(CompilationStage::Binding, loadError);
}

bool ProjectCompilation::compileProjectGenerics(
    const std::filesystem::path& projectFile,
    std::string& loadError)
{
    if (!compileSyntaxCore(projectFile, loadError))
    {
        return false;
    }
    if (cancellationRequested(loadError, "generics")) return false;

    const ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "generics");
    genericModel_ = std::make_unique<ProjectGenericModel>(
        lifetime_->indexes(),
        projectLoadOptions_.genericInstantiationDepth.value_or(
            ProjectGenericModel::DefaultMaximumInstantiationDepth),
        projectLoadOptions_.genericInstantiationCount.value_or(
            ProjectGenericModel::DefaultMaximumInstantiationCount));
    // EN: The generic model's C1 record cache and C3 clone cache stay disconnected, exactly as
    //     before 3.0.6. C3 persists a clone after its nested references were rewritten, keys it only
    //     by the specialization key (self-references inside a restored clone keep the producer's
    //     spelling), and its hit path in executeTypeDemand returns without rewriting the requesting
    //     syntax; resolved argument identities are not persisted either. C1 rebinds every field on
    //     a hit but keeps dependency lists computed by another compilation. Both therefore change
    //     the specializations and metadata that a cold build produces.
    // ES: La caché C1 de registros y la caché C3 de clones del modelo genérico siguen
    //     desconectadas, igual que antes de 3.0.6. C3 persiste un clon después de reescribir sus
    //     referencias anidadas, lo indexa sólo por la clave de especialización (las autorreferencias
    //     del clon restaurado conservan la escritura del productor) y su ruta de hit en
    //     executeTypeDemand regresa sin reescribir la sintaxis que lo pidió; tampoco persiste las
    //     identidades resueltas de los argumentos. C1 reenlaza todos los campos en un hit, pero
    //     conserva listas de dependencias calculadas por otra compilación. Por eso ambas cambian las
    //     especializaciones y la metadata que produce una compilación en frío.
    // NSC305-P01. The generic telemetry is a no-op unless a phase timing sink is wired (which the
    // driver only does for `--time` / `--time-trace`). It forwards sub-phase and per-template
    // timings through the same channel as the parent `generics` timer and never touches
    // diagnostics, dumps or emitted artifacts.
    GenericTelemetry& telemetry = genericModel_->telemetry();
    telemetry.timingSink = phaseTimingSink_;
#if 0 // INSTRUMENTATION CODE
    telemetry.progressSink = detailedProgressSink_;
#endif // INSTRUMENTATION CODE
    telemetry.enabled = static_cast<bool>(phaseTimingSink_);
    telemetry.detailed = telemetry.enabled;
    telemetry.ownUnits = units_.size();
    std::vector<PrecompiledGenericSpecialization> precompiledSpecializations;
    for (const PrecompiledSyntaxModule& module : precompiledModules_)
    {
        telemetry.templateSourceBytes += module.templateSourceText.size();
        precompiledSpecializations.insert(
            precompiledSpecializations.end(),
            module.genericSpecializations.begin(),
            module.genericSpecializations.end());
    }
    if (!genericModel_->build(
            *projectGraph_, units_, precompiledSpecializations, diagnostics_))
    {
        return false;
    }
    publishPrecompiledSyntaxProducts();
    publishTemplateParseProduct();
    releasePristineSyntaxProducts();
    return true;
}

bool ProjectCompilation::compileProjectMembers(
    const std::filesystem::path& projectFile,
    std::string& loadError)
{
    if (!compileProjectNames(projectFile, loadError))
    {
        return false;
    }
    if (cancellationRequested(loadError, "members")) return false;

    const ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "members");
    memberModel_ = std::make_unique<ProjectMemberModel>(lifetime_->indexes());
    if (!memberModel_->build(*nameLookup_, units_, diagnostics_, genericModel_.get())) return false;
    if (!memberModel_->buildModuleInitializationPlan(*projectGraph_, loadError)) return false;

    std::vector<PrecompiledMemberIdentity> precompiledMembers;
    std::vector<std::pair<std::string, std::string>> specializationTypeAliases;
    std::unordered_map<std::string_view, const GenericSpecializationRecord*>
        localSpecializationsByKey;
    localSpecializationsByKey.reserve(genericModel_->specializations().size());
    for (const GenericSpecializationRecord& specialization : genericModel_->specializations())
        localSpecializationsByKey.emplace(
            specialization.specializationKey, &specialization);
    for (const PrecompiledSyntaxModule& module : precompiledModules_)
    {
        precompiledMembers.insert(precompiledMembers.end(), module.members.begin(), module.members.end());
        for (const PrecompiledGenericSpecialization& imported : module.genericSpecializations)
        {
            if (imported.kind == GenericTemplateKind::Method) continue;
            const auto local = localSpecializationsByKey.find(imported.specializationKey);
            if (local == localSpecializationsByKey.end()) continue;

            const auto typeKey = [](std::string_view templateKey, std::string_view specializedName) {
                const std::size_t moduleSeparator = templateKey.find("::");
                if (moduleSeparator == std::string_view::npos) return std::string{};
                const std::size_t terminal = templateKey.find_last_of(".+");
                const std::size_t terminalBegin = terminal == std::string_view::npos ||
                        terminal < moduleSeparator + 2U
                    ? moduleSeparator + 2U
                    : terminal + 1U;
                if (templateKey.find('`', terminalBegin) == std::string_view::npos) return std::string{};
                return "type:" + std::string(templateKey.substr(0, terminalBegin)) +
                    std::string(specializedName);
            };
            const std::string localTypeKey =
                typeKey(local->second->templateKey, local->second->specializedName);
            const std::string publishedTypeKey = typeKey(imported.templateKey, imported.specializedName);
            if (!localTypeKey.empty() && !publishedTypeKey.empty())
                specializationTypeAliases.emplace_back(localTypeKey, publishedTypeKey);
        }
    }
    if (!memberModel_->bindPrecompiledPhysicalNames(
            precompiledMembers, specializationTypeAliases, loadError))
    {
        return false;
    }

    namespaceFunctionModel_ = std::make_unique<ProjectNamespaceFunctionModel>();
    if (!namespaceFunctionModel_->build(
            *nameLookup_, *memberModel_, units_, diagnostics_, genericModel_.get()))
    {
        return false;
    }
    std::vector<PrecompiledNamespaceFunctionIdentity> precompiledNamespaceFunctions;
    for (const PrecompiledSyntaxModule& module : precompiledModules_)
    {
        precompiledNamespaceFunctions.insert(
            precompiledNamespaceFunctions.end(),
            module.namespaceFunctions.begin(), module.namespaceFunctions.end());
    }
    return namespaceFunctionModel_->bindPrecompiledEscapeSummaries(
        precompiledNamespaceFunctions, loadError);
}


bool ProjectCompilation::compileProjectInheritance(
    const std::filesystem::path& projectFile,
    std::string& loadError)
{
    if (!compileProjectMembers(projectFile, loadError))
    {
        return false;
    }
    if (cancellationRequested(loadError, "inheritance")) return false;

    const ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "inheritance");
    inheritanceModel_ = std::make_unique<ProjectInheritanceModel>();
    return inheritanceModel_->build(*nameLookup_, diagnostics_);
}

// NSC-LIMIT-007. LOS CONSTRAINTS SE VALIDAN AQUI, y este es el unico punto posible de toda la
// cadena. La materializacion generica corre en la fase 2 --antes que nombres, miembros, herencia
// e interfaces-- asi que no puede validar; y si se dejara para despues, los cuerpos ya se habrian
// analizado sobre una especializacion invalida y el error saldria de DENTRO del cuerpo sustituido,
// apuntando a las tripas de la biblioteca en vez de al sitio de la llamada.
//
// Es una FASE, no un trozo de la de cuerpos, y la diferencia es medible: `--check-project-generic-
// constraints` tiene que poder fallar por un `where` y solo por un `where`. Cuando la validacion
// vivia dentro de `compileProjectMethodBodies`, esa entrada analizaba ademas cada cuerpo del
// proyecto y devolvia rojo por errores ajenos al ID.
bool ProjectCompilation::buildGenericConstraintModel()
{
    const ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "generic-constraints");
    genericConstraintModel_ = std::make_unique<ProjectGenericConstraintModel>();
    return genericConstraintModel_->build(
        *genericModel_, *nameLookup_, *memberModel_, *inheritanceModel_, *interfaceModel_,
        diagnostics_);
}

bool ProjectCompilation::compileProjectGenericConstraints(
    const std::filesystem::path& projectFile,
    std::string& loadError)
{
    if (!compileProjectInterfaces(projectFile, loadError))
    {
        return false;
    }
    if (cancellationRequested(loadError, "generic-constraints")) return false;
    return buildGenericConstraintModel();
}

bool ProjectCompilation::compileProjectMethodBodies(
    const std::filesystem::path& projectFile,
    std::string& loadError)
{
    // LA CADENA SE ENCADENA POR `compileProjectInterfaces`, NO POR LA FASE DE CONSTRAINTS, y la
    // distincion no es de estilo: la fase de constraints devuelve `false` tambien cuando el
    // programa es valido en todo salvo en un `where`, y eso NO es motivo para parar --se sigue
    // analizando el resto del fichero, o un solo `Caja<int>` mal escrito ocultaria todos los
    // demas diagnosticos--. Encadenar por su valor de retorno confundia las dos cosas: cuando
    // una fase ANTERIOR fallaba sin dejar `loadError` --un error de diagnostico, no de carga--
    // se seguia adelante con los modelos sin construir, y el modelo de cuerpos deferenciaba un
    // puntero nulo. Medido: segfault en ProjectMemberModel::isBuilt.
    if (!compileProjectInterfaces(projectFile, loadError))
    {
        return false;
    }
    if (cancellationRequested(loadError, "method-bodies")) return false;
    buildGenericConstraintModel();

    const ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "method-bodies");
    methodBodyModel_ = std::make_unique<ProjectMethodBodyModel>();
    std::optional<std::size_t> publicSurfaceProjectOrdinal;
    if (projectGraph_.has_value() && projectGraph_->rootProject() != nullptr &&
        projectGraph_->rootProject()->outputKind() != compilation::ProjectOutputKind::Executable)
    {
        publicSurfaceProjectOrdinal = projectGraph_->projects().size() - 1U;
    }
    ProjectMethodBodyBuildOptions methodBodyOptions;
    methodBodyOptions.workerCount = compilationJobs_;
    methodBodyOptions.workerPool = &lifetime_->workerPool();
    methodBodyOptions.cancellationCheck = cancellationCheck_;
    methodBodyOptions.phaseTimingSink = phaseTimingSink_;
    methodBodyOptions.executionError = &loadError;
    if (!methodBodyModel_->build(
        *nameLookup_, *memberModel_, *namespaceFunctionModel_, *inheritanceModel_, *interfaceModel_,
        genericModel_.get(), diagnostics_,
        genericConstraintModel_.get(), publicSurfaceProjectOrdinal,
        methodBodyOptions)) return false;
    if (!enterStage(CompilationStage::TypeChecking, loadError)) return false;
    return enterStage(CompilationStage::Comptime, loadError);
}

bool ProjectCompilation::compileProjectInterfaces(
    const std::filesystem::path& projectFile,
    std::string& loadError)
{
    if (!compileProjectInheritance(projectFile, loadError))
    {
        return false;
    }
    if (cancellationRequested(loadError, "interfaces")) return false;

    const ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "interfaces");
    interfaceModel_ = std::make_unique<ProjectInterfaceModel>();
    return interfaceModel_->build(*nameLookup_, *memberModel_, *inheritanceModel_, diagnostics_);
}

bool ProjectCompilation::compileProjectVirtualDispatch(
    const std::filesystem::path& projectFile,
    std::string& loadError)
{
    if (!compileProjectMethodBodies(projectFile, loadError))
    {
        return false;
    }
    if (cancellationRequested(loadError, "virtual-dispatch")) return false;

    const ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "virtual-dispatch");
    virtualDispatchModel_ =
        std::make_unique<ProjectVirtualDispatchModel>(lifetime_->indexes());
    return virtualDispatchModel_->build(
        *nameLookup_, *memberModel_, *inheritanceModel_, diagnostics_);
}

bool ProjectCompilation::compileProjectDestruction(
    const std::filesystem::path& projectFile,
    std::string& loadError)
{
    if (!compileProjectVirtualDispatch(projectFile, loadError))
    {
        return false;
    }
    if (cancellationRequested(loadError, "destruction")) return false;

    const ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "destruction");
    destructionModel_ = std::make_unique<ProjectDestructionModel>();
    return destructionModel_->build(
        *nameLookup_, *memberModel_, *inheritanceModel_, diagnostics_);
}

bool ProjectCompilation::compileProjectConstructions(
    const std::filesystem::path& projectFile,
    std::string& loadError)
{
    if (!compileProjectDestruction(projectFile, loadError))
    {
        return false;
    }
    if (cancellationRequested(loadError, "construction")) return false;

    const ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "construction");
    constructionModel_ = std::make_unique<ProjectConstructionModel>();
    return constructionModel_->build(
        *nameLookup_, *memberModel_, *inheritanceModel_, *interfaceModel_, *methodBodyModel_,
        *namespaceFunctionModel_, units_, diagnostics_);
}

bool ProjectCompilation::compileProjectObjectLayouts(
    const std::filesystem::path& projectFile,
    std::string& loadError)
{
    if (!compileProjectConstructions(projectFile, loadError))
    {
        return false;
    }
    if (cancellationRequested(loadError, "layout")) return false;

    const ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "layout");
    objectLayoutModel_ =
        std::make_unique<ProjectObjectLayoutModel>(lifetime_->indexes());
    if (!objectLayoutModel_->build(
            *nameLookup_, *memberModel_, *inheritanceModel_, *constructionModel_, diagnostics_))
        return false;
    metaprogrammingModel_ = std::make_unique<ProjectMetaprogrammingModel>();
    if (!metaprogrammingModel_->build(
            *nameLookup_, *memberModel_, *methodBodyModel_,
            *objectLayoutModel_, comptimeRecords_, diagnostics_))
    {
        loadError = metaprogrammingModel_->error();
        return false;
    }
    return true;
}

bool ProjectCompilation::compileProjectOopHir(
    const std::filesystem::path& projectFile,
    std::string& loadError)
{
    if (!compileProjectObjectLayouts(projectFile, loadError))
    {
        return false;
    }
    if (cancellationRequested(loadError, "oop-hir")) return false;

    const ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "oop-hir");
    oopHirModel_ = std::make_unique<ProjectOopHirModel>(lifetime_->indexes());
    if (oopHirModel_->build(
            *nameLookup_, *memberModel_, *inheritanceModel_, *interfaceModel_, *methodBodyModel_,
            *constructionModel_, *objectLayoutModel_, loadError, genericConstraintModel_.get(),
            compilationJobs_, namespaceFunctionModel_.get(),
            metaprogrammingModel_.get(), &lifetime_->workerPool()))
    {
        return true;
    }

    // EL SITIO DONDE FALLO, delante del mensaje. Es la otra mitad del envoltorio de
    // `lowerExpression` (doc 112 §A.4): aquel apunta el rango del nodo MAS INTERNO que no se pudo
    // bajar, y hasta hoy NADIE LO CONSUMIA -- se guardaba y se tiraba.
    //
    // POR QUE AQUI Y NO EN EL MODELO. `ProjectOopHirModel` no tiene `SourceManager` y no le hace
    // falta para recordar un rango: recordar es barato, traducir necesita los ficheros. Esta
    // clase ya tiene los dos. Enhebrar el `SourceManager` hasta el modelo habria sido pasarlo por
    // seis constructores para hacer aqui mismo lo que ya se puede hacer.
    //
    // El formato es `fichero:linea:columna: mensaje`, el de siempre en compiladores de C, para que
    // un editor pueda saltar al sitio.
    const SourceRange range = oopHirModel_->lastErrorRange();
    if (range.isValid() && !loadError.empty())
    {
        const source::SourcePosition position = sourceManager_.getPosition(range.begin());
        const std::string_view path = sourceManager_.getPath(range.begin());
        if (!path.empty())
        {
            loadError = std::string(path) + ':' + std::to_string(position.line) + ':' +
                std::to_string(position.column) + ": " + loadError;
        }
    }
    return false;
}

bool ProjectCompilation::compileProjectOopMir(
    const std::filesystem::path& projectFile,
    std::string& loadError)
{
    if (!compileProjectOopHir(projectFile, loadError))
    {
        return false;
    }
    if (cancellationRequested(loadError, "oop-mir")) return false;

    const ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "oop-mir");
    oopMirModel_ = std::make_unique<ProjectOopMirModel>(lifetime_->indexes());
    // EN: The MIR cache stays disconnected, as before 3.0.6. Its compilation identity embeds the
    //     complete textual HIR dump plus six semantic-model dumps, so every edit invalidates every
    //     slot, while each build still pays those dumps, one SHA-256 per callable, one lookup per
    //     callable and, on a miss, two write-through file publications per callable. Unchanged
    //     inputs are already served by the compilation bundle cache before the frontend runs.
    // ES: La caché MIR sigue desconectada, como antes de 3.0.6. Su identidad de compilación incluye
    //     el volcado textual completo del HIR y seis volcados de modelos semánticos, de modo que
    //     cualquier edición invalida todas las entradas, mientras cada compilación paga esos
    //     volcados, un SHA-256 por callable, una consulta por callable y, en un miss, dos
    //     publicaciones de archivo write-through por callable. Las entradas sin cambios ya las
    //     atiende la caché de bundles de compilación antes de ejecutar el frontend.
    if (projectGraph_.has_value() && projectGraph_->rootProject() != nullptr)
    {
        oopMirModel_->setPanicProfile(projectGraph_->rootProject()->panicProfile());
        oopMirModel_->setPanicHandler(std::string(projectGraph_->rootProject()->panicHandler()));
    }
    if (!oopMirModel_->build(
        *nameLookup_, *memberModel_, *objectLayoutModel_, *virtualDispatchModel_, *interfaceModel_,
        *destructionModel_, *oopHirModel_, *namespaceFunctionModel_, loadError,
        compilationJobs_, &lifetime_->workerPool()))
        return false;
    if (!enterStage(CompilationStage::Mir, loadError)) return false;
    return enterStage(CompilationStage::Monomorphization, loadError);
}

bool ProjectCompilation::isSyntaxCompilationFresh(std::string& loadError) const
{
    if (units_.empty() && genericModel_ == nullptr && nameLookup_ == nullptr &&
        memberModel_ == nullptr && inheritanceModel_ == nullptr && interfaceModel_ == nullptr &&
        methodBodyModel_ == nullptr && virtualDispatchModel_ == nullptr &&
        destructionModel_ == nullptr && constructionModel_ == nullptr &&
        objectLayoutModel_ == nullptr && metaprogrammingModel_ == nullptr &&
        oopHirModel_ == nullptr && oopMirModel_ == nullptr)
    {
        return true;
    }
    loadError = "ProjectCompilation instances are single-use";
    return false;
}

bool ProjectCompilation::ensureProjectGraph(
    const std::filesystem::path& projectFile,
    std::string& loadError)
{
    if (projectGraph_.has_value())
    {
        return true;
    }
    compilation::ProjectGraphLoader loader(compilationJobs_, {}, projectLoadOptions_);
    std::optional<compilation::ProjectGraph> graph;
    {
        const ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "project-graph");
        graph = loader.load(projectFile, loadError);
    }
    if (!graph.has_value())
    {
        return false;
    }
    projectGraph_ = std::move(*graph);
    return true;
}

bool ProjectCompilation::addPrecompiledProjects(std::string& loadError)
{
    std::vector<compilation::PrecompiledProjectDescriptor> descriptors;
    descriptors.reserve(precompiledModules_.size());
    for (const PrecompiledSyntaxModule& module : precompiledModules_)
    {
        descriptors.push_back({
            module.ownerProjectFile,
            module.metadataPath,
            module.moduleName});
    }
    return projectGraph_->addPrecompiledProjects(descriptors, loadError);
}

std::vector<ProjectSyntaxSourceRead> ProjectCompilation::collectSyntaxSourceReads() const
{
    std::vector<ProjectSyntaxSourceRead> sourceReads;
    for (std::size_t projectOrdinal = 0;
         projectOrdinal < projectGraph_->projects().size();
         ++projectOrdinal)
    {
        const compilation::Project& project = projectGraph_->projects()[projectOrdinal];
        for (const compilation::ProjectSource& source : project.sourceSet().sources())
        {
            sourceReads.push_back({
                projectOrdinal,
                source.ordinal,
                project.moduleName(),
                source.absolutePath,
                std::nullopt,
                std::nullopt,
                {}});
        }
    }
    return sourceReads;
}

bool ProjectCompilation::readSyntaxSources(
    std::vector<ProjectSyntaxSourceRead>& sourceReads,
    driver::CompilationWorkerPool& syntaxWorkers,
    std::string& loadError)
{
    const ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "syntax-read");
    std::atomic_size_t completed = 0;
    for (std::size_t index = 0; index < sourceReads.size(); ++index)
    {
        syntaxWorkers.submit([&, index](std::size_t)
        {
            if (sourceBufferProvider_)
            {
                sourceReads[index].buffer = sourceBufferProvider_(sourceReads[index].path);
            }
            if (!sourceReads[index].buffer.has_value())
            {
                sourceReads[index].buffer = source::SourceManager::readFileBuffer(
                    sourceReads[index].path, sourceReads[index].error);
            }
            if (sourceReads[index].buffer.has_value())
            {
                sourceReads[index].requiresComptime =
                    sourceMayRequireComptime(*sourceReads[index].buffer);
            }
            reportSourceProgress(
                progressSink_,
                "syntax-read",
                sourceReads[index].path,
                completed.fetch_add(1, std::memory_order_relaxed) + 1U,
                sourceReads.size());
        });
    }
    if (!waitForSyntaxWorkers(syntaxWorkers, "source reading", loadError))
    {
        return false;
    }
    for (const ProjectSyntaxSourceRead& read : sourceReads)
    {
        if (!read.buffer.has_value())
        {
            loadError = read.error;
            return false;
        }
    }
    return true;
}

void ProjectCompilation::appendComptimeRecord(
    const ProjectSyntaxSourceRead& read,
    const std::filesystem::path& relativePath)
{
    comptimeRecords_.push_back({
        read.projectOrdinal,
        read.sourceOrdinal,
        read.moduleName,
        relativePath.generic_string(),
        read.comptime->evaluations.size(),
        read.comptime->executedSteps,
        read.comptime->cacheHits,
        read.comptime->cacheMisses,
        read.comptime->reproducibilityDigest,
        read.comptime->dependencies,
        read.comptime->attributes,
        read.comptime->macroExpansions,
        read.comptime->authoredDiagnostics,
        read.comptime->declaredCapabilities});
}

void ProjectCompilation::reportComptimeDiagnostics(
    const ProjectSyntaxSourceRead& read,
    FileId fileId)
{
    for (const ComptimeAuthoredDiagnostic& diagnostic : read.comptime->authoredDiagnostics)
    {
        const auto severity = diagnostic.severity == ComptimeDiagnosticSeverity::Error
            ? diagnostics::DiagnosticSeverity::Error
            : diagnostic.severity == ComptimeDiagnosticSeverity::Warning
                ? diagnostics::DiagnosticSeverity::Warning
                : diagnostics::DiagnosticSeverity::Note;
        diagnostics_.reportStructured(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            severity,
            SourceRange(
                SourceLocation(fileId, static_cast<std::uint32_t>(diagnostic.offset)),
                SourceLocation(fileId, static_cast<std::uint32_t>(
                    diagnostic.offset + diagnostic.length))),
            "[" + diagnostic.code + "] " + diagnostic.message,
            {},
            diagnostic.notes);
    }
}

bool ProjectCompilation::materializeComptimeSource(
    ProjectSyntaxSourceRead& read,
    std::string& loadError)
{
    if (!read.comptime.has_value() || !read.comptime->success)
    {
        loadError = read.path.generic_string() + ": " +
            (read.comptime.has_value() ? read.comptime->error : "comptime produced no result");
        return false;
    }
    const compilation::Project& owner = projectGraph_->projects()[read.projectOrdinal];
    std::error_code relativeError;
    std::filesystem::path relativePath = std::filesystem::relative(
        read.path, owner.projectFile().parent_path(), relativeError);
    if (relativeError)
    {
        relativePath = read.path.filename();
    }
    appendComptimeRecord(read, relativePath);
    std::string source = read.requiresComptime
        ? std::move(read.comptime->source)
        : std::move(*read.buffer);
    const FileId fileId = sourceManager_.addBuffer(read.path, std::move(source));
    reportComptimeDiagnostics(read, fileId);
    units_.push_back(std::make_unique<ProjectSyntaxUnit>(
        read.projectOrdinal,
        read.sourceOrdinal,
        std::move(read.moduleName),
        fileId,
        false));
    return true;
}

bool ProjectCompilation::executeSyntaxComptime(
    std::vector<ProjectSyntaxSourceRead>& sourceReads,
    driver::CompilationWorkerPool& syntaxWorkers,
    std::string& loadError)
{
    const ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "syntax-comptime");
    std::shared_ptr<ComptimeCache> comptimeCache;
    for (const ProjectSyntaxSourceRead& read : sourceReads)
    {
        if (!read.requiresComptime) continue;
        comptimeCache = std::make_shared<ComptimeCache>();
        break;
    }
    std::atomic_size_t completed = 0;
    for (std::size_t index = 0; index < sourceReads.size(); ++index)
    {
        syntaxWorkers.submit([&, index](std::size_t)
        {
            if (sourceReads[index].requiresComptime)
            {
                sourceReads[index].comptime = ComptimeEngine(
                    {},
                    comptimeCache,
                    ComptimeExecutionContext{sourceReads[index].path.parent_path(), true, {}})
                    .execute(*sourceReads[index].buffer);
            }
            else
            {
                sourceReads[index].comptime =
                    makePassthroughComptimeResult(*sourceReads[index].buffer);
            }
            reportSourceProgress(
                progressSink_,
                "syntax-comptime",
                sourceReads[index].path,
                completed.fetch_add(1, std::memory_order_relaxed) + 1U,
                sourceReads.size());
        });
    }
    if (!waitForSyntaxWorkers(syntaxWorkers, "comptime", loadError))
    {
        return false;
    }
    for (ProjectSyntaxSourceRead& read : sourceReads)
    {
        if (!materializeComptimeSource(read, loadError))
        {
            return false;
        }
    }
    return true;
}

std::optional<std::size_t> ProjectCompilation::precompiledProjectOrdinal(
    const std::filesystem::path& canonicalMetadataPath) const
{
    const auto found = precompiledProjectOrdinals_.find(canonicalMetadataPath);
    if constexpr (internal::kIndexShadowCheckEnabled)
    {
        std::optional<std::size_t> scanned;
        for (std::size_t ordinal = 0; !scanned && ordinal < projectGraph_->projects().size(); ++ordinal)
        {
            std::error_code projectError;
            const std::filesystem::path canonicalProject = std::filesystem::weakly_canonical(
                projectGraph_->projects()[ordinal].projectFile(), projectError);
            if (!projectError && canonicalProject == canonicalMetadataPath) scanned = ordinal;
        }
        const std::optional<std::size_t> indexed = found == precompiledProjectOrdinals_.end()
            ? std::optional<std::size_t>()
            : std::optional<std::size_t>(found->second);
        if (scanned != indexed)
        {
            internal::reportIndexShadowMismatch(
                "ProjectCompilation::precompiledProjectOrdinal", canonicalMetadataPath.generic_string());
        }
    }
    if (found == precompiledProjectOrdinals_.end()) return std::nullopt;
    return found->second;
}

bool ProjectCompilation::materializePrecompiledTemplateUnit(
    const PrecompiledSyntaxModule& module,
    std::size_t projectOrdinal,
    std::string& loadError)
{
    if (module.templateSourceText.empty()) return true;
    return materializePrecompiledUnit(
        module, projectOrdinal, 1U, ".public-templates.ns",
        module.templateSourceText, module.publicTemplateSyntaxProduct,
        false, "templates", loadError);
}

bool ProjectCompilation::materializePrecompiledUnit(
    const PrecompiledSyntaxModule& module,
    std::size_t projectOrdinal,
    std::size_t sourceOrdinal,
    std::string_view displaySuffix,
    const std::string& sourceText,
    std::string_view publishedSyntaxProduct,
    bool isPrecompiled,
    std::string_view productName,
    std::string& loadError)
{
    std::filesystem::path displayPath = module.metadataPath;
    displayPath += displaySuffix;
    const FileId fileId = sourceManager_.addBuffer(displayPath, sourceText);
    const source::SourceFile* sourceFile = sourceManager_.getFile(fileId);
    if (sourceFile == nullptr)
    {
        loadError = "internal error: generated precompiled " + std::string(productName) +
            " have no SourceFile slot";
        return false;
    }
    units_.push_back(std::make_unique<ProjectSyntaxUnit>(
        projectOrdinal,
        sourceOrdinal,
        module.moduleName,
        fileId,
        isPrecompiled));
    precompiledSyntaxCacheKeysByUnit_.resize(units_.size());
    precompiledSyntaxCacheHitsByUnit_.resize(units_.size());
    std::string& syntaxCacheKey = precompiledSyntaxCacheKeysByUnit_.back();
    syntaxCacheKey = precompiledSyntaxCacheKey(module.productCacheKey, sourceOrdinal);
    const bool hasPublishedSyntax = !module.productSidecarPath.empty();
    if (hasPublishedSyntax && !restorePublishedPrecompiledSyntaxUnit(
            *units_.back(), publishedSyntaxProduct, productName, loadError))
    {
        return false;
    }
    precompiledSyntaxCacheHitsByUnit_.back() = hasPublishedSyntax ||
        restorePrecompiledSyntaxUnit(*units_.back(), syntaxCacheKey);
    return true;
}

// Installs a producer-owned syntax product only after all ranges bind to this compilation's file.
bool ProjectCompilation::restorePublishedPrecompiledSyntaxUnit(
    ProjectSyntaxUnit& unit,
    std::string_view payload,
    std::string_view productName,
    std::string& loadError)
{
    const source::SourceFile* sourceFile = sourceManager_.getFile(unit.fileId());
    std::vector<lexer::Token> tokens;
    std::unique_ptr<ast::AstArena> arena;
    ast::AstNode* tree = nullptr;
    if (sourceFile == nullptr ||
        !internal::deserializeRelocatableSyntaxProductUnit(
            *sourceFile, payload, tokens, arena, tree))
    {
        loadError = "cannot restore precompiled " + std::string(productName) +
            " syntax product for module '" + std::string(unit.moduleName()) + "'";
        return false;
    }
    unit.tokens_ = std::move(tokens);
    unit.arena_ = std::move(arena);
    unit.syntaxTree_ = tree;
    unit.lexicallyValid_ = true;
    unit.syntaxValid_ = true;
    return true;
}

bool ProjectCompilation::restorePrecompiledSyntaxUnit(
    ProjectSyntaxUnit& unit,
    std::string_view cacheKey)
{
    if (precompiledProductCacheRoot_.empty() || cacheKey.empty()) return false;
    TemplateParseCache& cache = lifetime_->precompiledSyntaxCache();
    cache.setRoot(precompiledProductCacheRoot_ / "syntax");
    std::string payload;
    bool hit = false;
    std::string ignoredError;
    if (!cache.restore(cacheKey, payload, hit, ignoredError) || !hit) return false;
    const source::SourceFile* sourceFile = sourceManager_.getFile(unit.fileId());
    std::vector<lexer::Token> tokens;
    std::unique_ptr<ast::AstArena> arena;
    ast::AstNode* tree = nullptr;
    if (sourceFile == nullptr || !internal::deserializeSyntaxProductUnit(
            *sourceFile, payload, tokens, arena, tree))
    {
        cache.quarantine(cacheKey);
        return false;
    }
    unit.tokens_ = std::move(tokens);
    unit.arena_ = std::move(arena);
    unit.syntaxTree_ = tree;
    unit.lexicallyValid_ = true;
    unit.syntaxValid_ = true;
    return true;
}

bool ProjectCompilation::materializePrecompiledModule(
    const PrecompiledSyntaxModule& module,
    std::set<std::string>& materializedMetadata,
    std::string& loadError)
{
    std::error_code metadataKeyError;
    const std::filesystem::path canonicalMetadata =
        std::filesystem::weakly_canonical(module.metadataPath, metadataKeyError);
    std::string metadataKey = canonicalMetadata.generic_string();
    if (metadataKeyError)
    {
        metadataKey = std::filesystem::absolute(
            module.metadataPath).lexically_normal().generic_string();
    }
    if (!materializedMetadata.insert(metadataKey).second)
    {
        return true;
    }
    // The canonical path of the key above is reused; a path that could not be canonicalized
    // has no project-graph node, as before.
    const std::optional<std::size_t> projectOrdinal = metadataKeyError
        ? std::optional<std::size_t>()
        : precompiledProjectOrdinal(canonicalMetadata);
    if (!projectOrdinal.has_value())
    {
        loadError = "internal error: precompiled module '" + module.moduleName +
            "' has no project-graph node";
        return false;
    }
    if (!materializePrecompiledUnit(
            module, *projectOrdinal, 0U, ".public-api.ns", module.sourceText,
            module.publicApiSyntaxProduct, true, "API", loadError))
    {
        return false;
    }
    // Deliberately NOT precompiled: these public bodies are input to the consumer's ordinary
    // generic pass, which specializes only what that consumer requests.
    return materializePrecompiledTemplateUnit(module, *projectOrdinal, loadError);
}

bool ProjectCompilation::materializePrecompiledSyntax(std::string& loadError)
{
    const ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "precompiled-syntax");
    std::set<std::string> materializedMetadata;
    // Each project file is canonicalized once here instead of once per precompiled module. The
    // map compares paths like path::operator== and keeps the first ordinal of equal paths, which
    // is the ordinal the former linear search returned; a failed canonicalization never matched.
    precompiledProjectOrdinals_.clear();
    if (!precompiledModules_.empty())
    {
        for (std::size_t ordinal = 0; ordinal < projectGraph_->projects().size(); ++ordinal)
        {
            std::error_code projectError;
            const std::filesystem::path canonicalProject = std::filesystem::weakly_canonical(
                projectGraph_->projects()[ordinal].projectFile(), projectError);
            if (!projectError) precompiledProjectOrdinals_.try_emplace(canonicalProject, ordinal);
        }
    }
    for (const PrecompiledSyntaxModule& module : precompiledModules_)
    {
        if (!materializePrecompiledModule(module, materializedMetadata, loadError))
        {
            return false;
        }
    }
    return true;
}

// Computes the whole syntax-corpus identity while every final source buffer is resident but before
// lexer or parser work starts. The identity deliberately excludes physical paths so equivalent IDE
// temporary directories can share a hit; unit order, logical ownership, source bytes and compiler
// build identity still make restored FileIds and parser-wide known-type inputs deterministic.
bool ProjectCompilation::calculateTemplateParseCacheKey(std::string& loadError)
{
    templateParseCacheKey_.clear();
    if (templateCacheRoot_.empty()) return true;

    std::string material;
    // EN: v2 retires every v1 corpus: v1 published trees already rewritten by the generic pass.
    // ES: v2 retira todo corpus v1: v1 publicaba árboles ya reescritos por el pase genérico.
    appendTemplateParseCacheKeyField(material, "nsc-template-parse-input-v2");
    appendTemplateParseCacheKeyField(material, kCompilerBuildId);
    appendTemplateParseCacheKeyField(material, std::to_string(units_.size()));
    for (std::size_t index = 0; index < units_.size(); ++index)
    {
        const ProjectSyntaxUnit& unit = *units_[index];
        const source::SourceFile* sourceFile = sourceManager_.getFile(unit.fileId());
        if (sourceFile == nullptr)
        {
            loadError = "internal error: syntax cache key unit has no SourceFile slot";
            return false;
        }
        appendTemplateParseCacheKeyField(material, std::to_string(index));
        appendTemplateParseCacheKeyField(material, std::to_string(unit.projectOrdinal()));
        appendTemplateParseCacheKeyField(material, std::to_string(unit.sourceOrdinal()));
        appendTemplateParseCacheKeyField(material, unit.moduleName());
        appendTemplateParseCacheKeyField(material, unit.isPrecompiled() ? "1" : "0");
        appendTemplateParseCacheKeyField(
            material,
            templateParseSourceDigest(sourceFile->buffer()));
    }
    const std::span<const std::uint8_t> bytes{
        reinterpret_cast<const std::uint8_t*>(material.data()), material.size()};
    templateParseCacheKey_ = basic::sha256Hex(basic::sha256(bytes));
    return true;
}

// Restores a complete parsed corpus only after validating every unit and decoding all products into
// detached arenas. A committed hit supplies the token stream used by the template-record cache, so
// the generic pass can restore records instead of reconstructing them from each declaration.
bool ProjectCompilation::restoreTemplateParseProduct(std::string& loadError)
{
    templateParseCacheHit_ = false;
    if (templateCacheRoot_.empty() || templateParseCacheKey_.empty()) return true;
    TemplateParseCache& cache = lifetime_->templateParseCache();
    cache.setRoot(templateCacheRoot_);
    std::string payload;
    bool hit = false;
    TemplateParseCacheLookupReason lookupReason = TemplateParseCacheLookupReason::Cold;
    if (!cache.restore(
            templateParseCacheKey_, payload, hit, lookupReason, loadError) ||
        lookupReason != TemplateParseCacheLookupReason::Hit)
    {
        return true;
    }
    internal::CachePayloadReader reader(payload);
    std::string schema;
    std::uint64_t unitCount = 0;
    if (!reader.string(schema) || schema != templateParseProductSchema ||
        !reader.u64(unitCount) || unitCount != units_.size())
    {
        cache.quarantine(templateParseCacheKey_);
        return true;
    }
    std::vector<std::string> unitPayloads;
    unitPayloads.reserve(units_.size());
    for (const std::unique_ptr<ProjectSyntaxUnit>& unit : units_)
    {
        std::string unitPayload;
        if (!readTemplateParseUnitIdentity(reader, *unit) ||
            !reader.string(unitPayload))
        {
            cache.quarantine(templateParseCacheKey_);
            return true;
        }
        unitPayloads.push_back(std::move(unitPayload));
    }
    if (!reader.remaining().empty())
    {
        cache.quarantine(templateParseCacheKey_);
        return true;
    }
    std::vector<std::vector<lexer::Token>> tokens(units_.size());
    std::vector<std::unique_ptr<ast::AstArena>> arenas(units_.size());
    std::vector<ast::AstNode*> trees(units_.size(), nullptr);
    for (std::size_t index = 0; index < units_.size(); ++index)
    {
        const source::SourceFile* sourceFile = sourceManager_.getFile(units_[index]->fileId());
        if (sourceFile == nullptr || !internal::deserializeSyntaxProductUnit(
                *sourceFile, unitPayloads[index], tokens[index], arenas[index], trees[index]))
        {
            cache.quarantine(templateParseCacheKey_);
            return true;
        }
    }
    for (std::size_t index = 0; index < units_.size(); ++index)
    {
        units_[index]->tokens_ = std::move(tokens[index]);
        units_[index]->arena_ = std::move(arenas[index]);
        units_[index]->syntaxTree_ = trees[index];
        units_[index]->lexicallyValid_ = true;
        units_[index]->syntaxValid_ = true;
    }
    templateParseCacheHit_ = true;
    return true;
}

// Publishes the syntax corpus only after the generic pass has completed successfully. At that point
// every unit is valid and the per-template records consumed by a warm hit have also been stored.
// Cache I/O is opportunistic and never changes compiler success or diagnostics.
// EN: The published unit payloads are the pristine snapshots taken right after parsing; the live trees
//     were already rewritten by the generic pass and must never be published.
// ES: Los payloads publicados son las fotografías prístinas tomadas justo después del parser; los árboles
//     vivos ya fueron reescritos por el pase genérico y nunca deben publicarse.
void ProjectCompilation::publishTemplateParseProduct()
{
    if (templateParseCacheHit_ || templateCacheRoot_.empty() ||
        templateParseCacheKey_.empty())
    {
        return;
    }
    for (std::size_t index = 0; index < units_.size(); ++index)
    {
        const std::unique_ptr<ProjectSyntaxUnit>& unit = units_[index];
        if (unit == nullptr || !unit->isLexicallyValid() ||
            !unit->isSyntaxValid() || unit->syntaxTree() == nullptr ||
            pristineSyntaxProduct(index).empty())
        {
            return;
        }
    }
    std::string payload;
    internal::CachePayloadWriter::string(payload, templateParseProductSchema);
    internal::CachePayloadWriter::u64(payload, units_.size());
    for (std::size_t index = 0; index < units_.size(); ++index)
    {
        const std::unique_ptr<ProjectSyntaxUnit>& unit = units_[index];
        internal::CachePayloadWriter::u64(payload, unit->projectOrdinal());
        internal::CachePayloadWriter::u64(payload, unit->sourceOrdinal());
        internal::CachePayloadWriter::string(payload, unit->moduleName());
        internal::CachePayloadWriter::u32(payload, unit->fileId().value());
        internal::CachePayloadWriter::byte(payload, unit->isPrecompiled() ? 1U : 0U);
        internal::CachePayloadWriter::string(payload, pristineSyntaxProduct(index));
    }
    TemplateParseCache& cache = lifetime_->templateParseCache();
    cache.setRoot(templateCacheRoot_);
    std::string ignoredError;
    (void)cache.store(templateParseCacheKey_, payload, ignoredError);
}

// EN: Publishes the pristine parse product of every precompiled unit that missed the local syntax cache.
//     The payload is the snapshot taken before the generic pass; a missing snapshot skips publication.
// ES: Publica el producto prístino del parser de cada unidad precompilada que falló la caché sintáctica
//     local. El payload es la fotografía tomada antes del pase genérico; sin fotografía no se publica.
void ProjectCompilation::publishPrecompiledSyntaxProducts()
{
    if (precompiledProductCacheRoot_.empty()) return;
    TemplateParseCache& cache = lifetime_->precompiledSyntaxCache();
    cache.setRoot(precompiledProductCacheRoot_ / "syntax");
    const std::size_t count = std::min(
        units_.size(), precompiledSyntaxCacheKeysByUnit_.size());
    for (std::size_t index = 0; index < count; ++index)
    {
        const std::string& key = precompiledSyntaxCacheKeysByUnit_[index];
        if (key.empty() || (index < precompiledSyntaxCacheHitsByUnit_.size() &&
            precompiledSyntaxCacheHitsByUnit_[index]))
        {
            continue;
        }
        const ProjectSyntaxUnit& unit = *units_[index];
        if (!unit.isLexicallyValid() || !unit.isSyntaxValid() ||
            unit.syntaxTree() == nullptr)
        {
            continue;
        }
        const std::string_view payload = pristineSyntaxProduct(index);
        if (payload.empty())
        {
            continue;
        }
        std::string ignoredError;
        (void)cache.store(key, payload, ignoredError);
    }
}

// EN: Tells whether the precompiled unit at `index` missed the local syntax cache and therefore will
//     be published after a successful generic pass.
// ES: Indica si la unidad precompilada en `index` falló la caché sintáctica local y por tanto se
//     publicará después de un pase genérico exitoso.
bool ProjectCompilation::precompiledSyntaxProductPending(std::size_t index) const noexcept
{
    if (precompiledProductCacheRoot_.empty() || index >= precompiledSyntaxCacheKeysByUnit_.size() ||
        precompiledSyntaxCacheKeysByUnit_[index].empty())
    {
        return false;
    }
    return index >= precompiledSyntaxCacheHitsByUnit_.size() ||
        !precompiledSyntaxCacheHitsByUnit_[index];
}

// EN: Serializes, before the generic pass rewrites closed generic references and appends specialized
//     clones, every syntax product that a later cache publication needs. Publishing a post-generics
//     tree makes a later hit skip specialization discovery, which drops the closed-type aliases that
//     bind precompiled callables to their NSMETA physical identities.
// ES: Serializa, antes de que el pase genérico reescriba referencias genéricas cerradas y agregue clones
//     especializados, cada producto sintáctico que una publicación posterior necesita. Publicar un árbol
//     posterior a genéricos hace que un hit posterior omita el descubrimiento de especializaciones, lo
//     que pierde los alias de tipos cerrados que enlazan callables precompilados con su identidad
//     física en NSMETA.
// EN: The units are serialized on the syntax workers. Each task reads one unit, which no other task
//     touches, and writes only its own slot of pristineSyntaxProductsByUnit_, which was sized before
//     the first submission; the slots are read only after wait(). The snapshots are byte-identical to
//     the sequential ones. wait() rethrows a task failure, as the sequential loop let it propagate.
// ES: Las unidades se serializan en los workers sintácticos. Cada tarea lee una unidad que ninguna otra
//     toca y escribe solo su propia ranura de pristineSyntaxProductsByUnit_, dimensionada antes de la
//     primera tarea; las ranuras se leen solo después de wait(). Las fotografías son idénticas byte a
//     byte a las secuenciales. wait() relanza el fallo de una tarea, como lo propagaba el bucle.
void ProjectCompilation::capturePristineSyntaxProducts(driver::CompilationWorkerPool& syntaxWorkers)
{
    pristineSyntaxProductsByUnit_.clear();
    const bool publishesCorpus = !templateParseCacheHit_ && !templateCacheRoot_.empty() &&
        !templateParseCacheKey_.empty();
    if (!publishesCorpus && precompiledProductCacheRoot_.empty())
    {
        return;
    }
    pristineSyntaxProductsByUnit_.resize(units_.size());
    for (std::size_t index = 0; index < units_.size(); ++index)
    {
        const ProjectSyntaxUnit* unit = units_[index].get();
        if (unit == nullptr || !unit->isLexicallyValid() || !unit->isSyntaxValid() ||
            unit->syntaxTree() == nullptr)
        {
            continue;
        }
        if (publishesCorpus || precompiledSyntaxProductPending(index))
        {
            syntaxWorkers.submit([this, unit, index](std::size_t)
            {
                pristineSyntaxProductsByUnit_[index] = internal::serializeSyntaxProductUnit(*unit);
            });
        }
    }
    syntaxWorkers.wait();
}

// EN: Returns the pristine snapshot of the unit at `index`, or an empty view when none was taken.
// ES: Devuelve la fotografía prístina de la unidad en `index`, o una vista vacía si no se tomó.
std::string_view ProjectCompilation::pristineSyntaxProduct(std::size_t index) const noexcept
{
    return index < pristineSyntaxProductsByUnit_.size()
        ? std::string_view(pristineSyntaxProductsByUnit_[index])
        : std::string_view{};
}

// EN: Releases the pristine snapshots once both cache publications have consumed them.
// ES: Libera las fotografías prístinas una vez que ambas publicaciones de caché las consumieron.
void ProjectCompilation::releasePristineSyntaxProducts() noexcept
{
    std::vector<std::string>().swap(pristineSyntaxProductsByUnit_);
}

// EN: Produces the parsed corpus from the template-parse cache or from the lexer and parser, and then
//     snapshots the pristine syntax products before any later phase mutates the trees.
// ES: Obtiene el corpus analizado desde la caché de parseo o desde lexer y parser, y después fotografía
//     los productos sintácticos prístinos antes de que una fase posterior mute los árboles.
bool ProjectCompilation::produceSyntaxCorpus(
    driver::CompilationWorkerPool& syntaxWorkers,
    std::string& loadError)
{
    if (!restoreTemplateParseProduct(loadError))
    {
        return false;
    }
    if (templateParseCacheHit_)
    {
        if (!enterStage(CompilationStage::Parse, loadError))
        {
            return false;
        }
        capturePristineSyntaxProducts(syntaxWorkers);
        return true;
    }

    bool lexicalOk = true;
    if (!lexSyntaxUnits(syntaxWorkers, lexicalOk, loadError))
    {
        return false;
    }
    if (!parseSyntaxUnits(syntaxWorkers, lexicalOk, loadError))
    {
        return false;
    }
    capturePristineSyntaxProducts(syntaxWorkers);
    return true;
}

bool ProjectCompilation::lexSyntaxUnits(
    driver::CompilationWorkerPool& syntaxWorkers,
    bool& lexicalOk,
    std::string& loadError)
{
    const ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "syntax-lex");
    const std::size_t progressTotal = visibleSourceCount(units_, sourceManager_);
    std::atomic_size_t completed = 0;
    std::vector<diagnostics::DiagnosticEngine> localDiagnostics;
    localDiagnostics.reserve(units_.size());
    for (std::size_t index = 0; index < units_.size(); ++index)
    {
        localDiagnostics.emplace_back(diagnostics_.errorLimit());
        if (units_[index]->isLexicallyValid()) continue;
        syntaxWorkers.submit([&, index](std::size_t)
        {
            const source::SourceFile* sourceFile = sourceManager_.getFile(units_[index]->fileId());
            if (sourceFile == nullptr)
            {
                throw std::logic_error("syntax unit has no SourceFile slot");
            }
            units_[index]->lex(*sourceFile, localDiagnostics[index]);
            if (!isMetadataBackedVirtualSource(sourceFile->path()))
            {
                reportSourceProgress(
                    progressSink_,
                    "syntax-lex",
                    sourceFile->path(),
                    completed.fetch_add(1, std::memory_order_relaxed) + 1U,
                    progressTotal);
            }
        });
    }
    if (!waitForSyntaxWorkers(syntaxWorkers, "lexing", loadError))
    {
        return false;
    }
    lexicalOk = true;
    for (std::size_t index = 0; index < units_.size(); ++index)
    {
        lexicalOk = units_[index]->isLexicallyValid() && lexicalOk;
    }
    diagnostics_.mergeDeterministically(localDiagnostics);
    return true;
}

bool ProjectCompilation::parseSyntaxUnits(
    driver::CompilationWorkerPool& syntaxWorkers,
    bool lexicalOk,
    std::string& loadError)
{
    const ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "syntax-parse");
    const std::vector<std::string> knownTypeNames = collectParserKnownTypeNames(units_);
    const parser::KnownTypeNameSet knownTypeNameIndex =
        parser::buildKnownTypeNameSet(knownTypeNames);
    const std::size_t progressTotal = visibleSourceCount(units_, sourceManager_);
    std::atomic_size_t completed = 0;
    bool syntaxOk = lexicalOk;
    std::vector<diagnostics::DiagnosticEngine> localDiagnostics;
    localDiagnostics.reserve(units_.size());
    for (std::size_t index = 0; index < units_.size(); ++index)
    {
        localDiagnostics.emplace_back(diagnostics_.errorLimit());
        if (!units_[index]->isLexicallyValid() || units_[index]->isSyntaxValid()) continue;
        syntaxWorkers.submit([&, index](std::size_t)
        {
            units_[index]->parse(localDiagnostics[index], knownTypeNameIndex);
            const std::filesystem::path sourcePath(sourceManager_.getPath(
                SourceLocation(units_[index]->fileId(), 0U)));
            if (!isMetadataBackedVirtualSource(sourcePath))
            {
                reportSourceProgress(
                    progressSink_,
                    "syntax-parse",
                    sourcePath,
                    completed.fetch_add(1, std::memory_order_relaxed) + 1U,
                    progressTotal);
            }
        });
    }
    if (!waitForSyntaxWorkers(syntaxWorkers, "parsing", loadError))
    {
        return false;
    }
    for (std::size_t index = 0; index < units_.size(); ++index)
    {
        syntaxOk = units_[index]->isSyntaxValid() && syntaxOk;
    }
    diagnostics_.mergeDeterministically(localDiagnostics);
    if (!syntaxOk || diagnostics_.hasErrors())
    {
        return false;
    }
    return enterStage(CompilationStage::Parse, loadError);
}

bool ProjectCompilation::compileSyntaxCore(
    const std::filesystem::path& projectFile,
    std::string& loadError)
{
    loadError.clear();
    if (!isSyntaxCompilationFresh(loadError))
    {
        return false;
    }
    lifetime_->indexes().clear();
    if (!ensureProjectGraph(projectFile, loadError))
    {
        return false;
    }
    if (cancellationRequested(loadError, "syntax")) return false;
    if (!addPrecompiledProjects(loadError))
    {
        return false;
    }
    comptimeRecords_.clear();
    driver::CompilationWorkerPool& syntaxWorkers = lifetime_->workerPool();
    std::vector<ProjectSyntaxSourceRead> sourceReads = collectSyntaxSourceReads();

    if (!readSyntaxSources(sourceReads, syntaxWorkers, loadError))
    {
        return false;
    }

    if (!executeSyntaxComptime(sourceReads, syntaxWorkers, loadError))
    {
        return false;
    }

    if (!materializePrecompiledSyntax(loadError))
    {
        return false;
    }

    if (!calculateTemplateParseCacheKey(loadError))
    {
        return false;
    }

    return produceSyntaxCorpus(syntaxWorkers, loadError);
}

bool ProjectCompilation::enterStage(CompilationStage stage, std::string& error)
{
    return stageTracker_.enter(stage, error);
}

bool ProjectCompilation::completeCodeGeneration(std::string& error)
{
    return enterStage(CompilationStage::Codegen, error);
}

std::span<const CompilationStage> ProjectCompilation::completedStages() const noexcept
{
    return stageTracker_.completed();
}

const compilation::ProjectGraph* ProjectCompilation::projectGraph() const noexcept
{
    return projectGraph_.has_value() ? &*projectGraph_ : nullptr;
}

compilation::LibraryMetadataRepository& ProjectCompilation::libraryMetadataRepository() noexcept
{
    return lifetime_->metadataRepository();
}

const compilation::LibraryMetadataRepository& ProjectCompilation::libraryMetadataRepository() const noexcept
{
    return lifetime_->metadataRepository();
}

ProjectCompilationLifetime& ProjectCompilation::lifetime() noexcept
{
    return *lifetime_;
}

const ProjectCompilationLifetime& ProjectCompilation::lifetime() const noexcept
{
    return *lifetime_;
}

ProjectCompilationIndexes& ProjectCompilation::indexes() noexcept
{
    return lifetime_->indexes();
}

const ProjectCompilationIndexes& ProjectCompilation::indexes() const noexcept
{
    return lifetime_->indexes();
}

const source::SourceManager& ProjectCompilation::sourceManager() const noexcept
{
    return sourceManager_;
}

diagnostics::DiagnosticEngine& ProjectCompilation::diagnostics() noexcept
{
    return diagnostics_;
}

const diagnostics::DiagnosticEngine& ProjectCompilation::diagnostics() const noexcept
{
    return diagnostics_;
}

std::span<const std::unique_ptr<ProjectSyntaxUnit>> ProjectCompilation::units() const noexcept
{
    return units_;
}

std::span<const ProjectComptimeRecord> ProjectCompilation::comptimeRecords() const noexcept
{
    return comptimeRecords_;
}

const ProjectNameLookup* ProjectCompilation::nameLookup() const noexcept
{
    return nameLookup_.get();
}

const ProjectGenericModel* ProjectCompilation::genericModel() const noexcept
{
    return genericModel_.get();
}

const ProjectGenericConstraintModel* ProjectCompilation::genericConstraintModel() const noexcept
{
    return genericConstraintModel_.get();
}

const ProjectMemberModel* ProjectCompilation::memberModel() const noexcept
{
    return memberModel_.get();
}

const ProjectNamespaceFunctionModel* ProjectCompilation::namespaceFunctionModel() const noexcept
{
    return namespaceFunctionModel_.get();
}

const ProjectInheritanceModel* ProjectCompilation::inheritanceModel() const noexcept
{
    return inheritanceModel_.get();
}

const ProjectInterfaceModel* ProjectCompilation::interfaceModel() const noexcept
{
    return interfaceModel_.get();
}

const ProjectMethodBodyModel* ProjectCompilation::methodBodyModel() const noexcept
{
    return methodBodyModel_.get();
}

const ProjectVirtualDispatchModel* ProjectCompilation::virtualDispatchModel() const noexcept
{
    return virtualDispatchModel_.get();
}

const ProjectDestructionModel* ProjectCompilation::destructionModel() const noexcept
{
    return destructionModel_.get();
}

const ProjectConstructionModel* ProjectCompilation::constructionModel() const noexcept
{
    return constructionModel_.get();
}

const ProjectObjectLayoutModel* ProjectCompilation::objectLayoutModel() const noexcept
{
    return objectLayoutModel_.get();
}

const ProjectMetaprogrammingModel* ProjectCompilation::metaprogrammingModel() const noexcept
{
    return metaprogrammingModel_.get();
}

const ProjectOopHirModel* ProjectCompilation::oopHirModel() const noexcept
{
    return oopHirModel_.get();
}

const ProjectOopMirModel* ProjectCompilation::oopMirModel() const noexcept
{
    return oopMirModel_.get();
}
}
