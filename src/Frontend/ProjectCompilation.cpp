#include "nativesharp/Frontend/ProjectCompilation.h"
#include "Cache/CachePayloadReader.h"
#include "Cache/CachePayloadWriter.h"
#include "Cache/SyntaxProductCacheCodec.h"
#include "Internal/ProjectPhaseTimer.h"
#include "Internal/IndexShadowCheckSwitch.h"
#include "Internal/IndexShadowMismatch.h"
#include "nativesharp/Frontend/ComptimeEngine.h"
#include "nativesharp/Frontend/CppParserProjectInventory.h"
#include "nativesharp/Frontend/CppScopedTypeNames.h"
#include "nativesharp/Frontend/ProjectOopHirNode.h"
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
#include <cctype>
#include <cstdint>
#include <exception>
#include <set>
#include <memory>
#include <span>
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

bool ProjectCompilation::setCppInteropSession(
    std::unique_ptr<interop::CppInteropSession> session, std::string& error)
{
    if (compilationAttempted_)
    {
        error = "C/C++ interoperability session must be set before compilation starts";
        return false;
    }
    if (session && session->state() != interop::CppSessionState::Ready)
    {
        error = "C/C++ interoperability session must be ready";
        return false;
    }
    cppInteropSession_ = std::move(session);
    error.clear();
    return true;
}

interop::CppInteropSession* ProjectCompilation::cppInteropSession() const noexcept
{
    return cppInteropSession_.get();
}

bool ProjectCompilation::setCppImportedFunctions(
    std::vector<NamespaceFunctionSymbol> functions, std::string& error)
{
    if (compilationAttempted_)
    {
        error = "C/C++ functions must be set before compilation starts";
        return false;
    }
    if (cppFunctionProvider_)
    {
        error = "C/C++ functions and a function provider cannot both be configured";
        return false;
    }
    ProjectNamespaceFunctionModel validation;
    if (!validation.setCppImportedFunctions(functions, error)) return false;
    cppImportedFunctions_ = std::move(functions);
    cppImportedFunctionsConfigured_ = true;
    error.clear();
    return true;
}

bool ProjectCompilation::setCppVariadicCallPlanProvider(
    CppVariadicCallPlanProvider provider, std::string& error)
{
    if (compilationAttempted_) { error = "C/C++ variadic plan provider must be set before compilation starts"; return false; }
    cppVariadicCallPlanProvider_ = std::move(provider);
    error.clear();
    return true;
}

bool ProjectCompilation::setCppFunctionPointerPlanProvider(
    CppFunctionPointerPlanProvider provider, std::string& error)
{
    if (compilationAttempted_) { error = "C/C++ function pointer plan provider must be set before compilation starts"; return false; }
    cppFunctionPointerPlanProvider_ = std::move(provider);
    error.clear();
    return true;
}

bool ProjectCompilation::setExperimentalCppImportedStates(
    std::vector<NamespaceStateSymbol> states, std::string& error)
{
    if (compilationAttempted_)
    {
        error = "experimental C/C++ globals must be set before compilation starts";
        return false;
    }
    ProjectNamespaceFunctionModel validation;
    if (!validation.setCppImportedStates(states, error)) return false;
    experimentalCppImportedStates_ = std::move(states);
    return true;
}

bool ProjectCompilation::setExperimentalCppImportedTypes(
    std::vector<CppImportedTypeDeclaration> types, std::string& error)
{
    if (compilationAttempted_)
    {
        error = "experimental C/C++ types must be set before compilation starts";
        return false;
    }
    if (experimentalCppTypeProvider_)
    {
        error = "experimental C/C++ types and a type provider cannot both be configured";
        return false;
    }
    experimentalCppImportedTypes_ = std::move(types);
    experimentalCppImportedTypesConfigured_ = true;
    error.clear();
    return true;
}

bool ProjectCompilation::setCppTypeAliasProvider(CppTypeAliasProvider provider, std::string& error)
{
    if (compilationAttempted_)
    {
        error = "C/C++ type alias provider must be set before compilation starts";
        return false;
    }
    cppTypeAliasProvider_ = std::move(provider);
    error.clear();
    return true;
}

bool ProjectCompilation::setExperimentalCppTypeProvider(ExperimentalCppTypeProvider provider, std::string& error)
{
    if (compilationAttempted_)
    {
        error = "experimental C/C++ type provider must be set before compilation starts";
        return false;
    }
    if (provider && experimentalCppImportedTypesConfigured_)
    {
        error = "experimental C/C++ types and a type provider cannot both be configured";
        return false;
    }
    experimentalCppTypeProvider_ = std::move(provider);
    error.clear();
    return true;
}

bool ProjectCompilation::setExperimentalCppTypeSelector(ExperimentalCppTypeSelector selector, std::string& error)
{
    if (compilationAttempted_)
    {
        error = "experimental C/C++ type selector must be set before compilation starts";
        return false;
    }
    experimentalCppTypeSelector_ = std::move(selector);
    error.clear();
    return true;
}

bool ProjectCompilation::setCppPrecompiledParserTypeProvider(CppPrecompiledParserTypeProvider provider, std::string& error)
{
    if (compilationAttempted_)
    {
        error = "precompiled C parser type provider must be set before compilation starts";
        return false;
    }
    cppPrecompiledParserTypeProvider_ = std::move(provider);
    error.clear();
    return true;
}

bool ProjectCompilation::setExperimentalCppInstanceMethodProvider(
    ExperimentalCppInstanceMethodProvider provider, std::string& error)
{
    if (compilationAttempted_)
    {
        error = "experimental C++ instance method provider must be set before compilation starts";
        return false;
    }
    experimentalCppInstanceMethodProvider_ = std::move(provider);
    error.clear();
    return true;
}

bool ProjectCompilation::setExperimentalCppLifetimeMethodProvider(
    ExperimentalCppLifetimeMethodProvider provider, std::string& error)
{
    if (compilationAttempted_)
    {
        error = "experimental C++ lifetime provider must be set before compilation starts";
        return false;
    }
    experimentalCppLifetimeMethodProvider_ = std::move(provider);
    error.clear();
    return true;
}

bool ProjectCompilation::enableExperimentalCppLifetimeAnalysisForTests(std::string& error)
{
    if (compilationAttempted_)
    {
        error = "experimental C++ lifetime analysis must be enabled before compilation starts";
        return false;
    }
    if (!experimentalCppLifetimeMethodProvider_)
    {
        error = "experimental C++ lifetime analysis requires a lifetime provider";
        return false;
    }
    experimentalCppLifetimeAnalysisForTests_ = true;
    error.clear();
    return true;
}

bool ProjectCompilation::enableExperimentalCppLocalLifetimeAnalysisForTests(std::string& error)
{
    if (!enableExperimentalCppLifetimeAnalysisForTests(error)) return false;
    experimentalCppLocalLifetimeAnalysisForTests_ = true;
    return true;
}

bool ProjectCompilation::enableCppImportedNoexceptLocalLifetime(std::string& error)
{
    if (!enableExperimentalCppLocalLifetimeAnalysisForTests(error)) return false;
    cppNoexceptLocalLifetimeOnly_ = true;
    return true;
}

bool ProjectCompilation::enableExperimentalCppDiscardedLifetimeLoweringForTests(std::string& error)
{
    if (!enableExperimentalCppLifetimeAnalysisForTests(error)) return false;
    experimentalCppDiscardedLifetimeLoweringForTests_ = true;
    return true;
}

bool ProjectCompilation::enableExperimentalCppInstanceMethodLoweringForTests(std::string& error)
{
    return enableCppImportedMethodLowering(error);
}

bool ProjectCompilation::enableCppImportedMethodLowering(std::string& error)
{
    if (compilationAttempted_)
    {
        error = "experimental C++ instance method lowering must be enabled before compilation starts";
        return false;
    }
    experimentalCppInstanceMethodLoweringForTests_ = true;
    error.clear();
    return true;
}

bool ProjectCompilation::setExperimentalCppInstanceFieldProvider(
    ExperimentalCppInstanceFieldProvider provider, std::string& error)
{
    if (compilationAttempted_)
    {
        error = "experimental C++ instance field provider must be set before compilation starts";
        return false;
    }
    experimentalCppInstanceFieldProvider_ = std::move(provider);
    error.clear();
    return true;
}

bool ProjectCompilation::setExperimentalCppStaticMethodProvider(
    ExperimentalCppStaticMethodProvider provider, std::string& error)
{
    if (compilationAttempted_)
    {
        error = "experimental C++ static method provider must be set before compilation starts";
        return false;
    }
    experimentalCppStaticMethodProvider_ = std::move(provider);
    error.clear();
    return true;
}

bool ProjectCompilation::enableExperimentalCppInstanceFieldLoweringForTests(std::string& error)
{
    return enableCppImportedFieldLowering(error);
}

bool ProjectCompilation::enableCppImportedFieldLowering(std::string& error)
{
    if (compilationAttempted_)
    {
        error = "experimental C++ instance field lowering must be enabled before compilation starts";
        return false;
    }
    experimentalCppInstanceFieldLoweringForTests_ = true;
    error.clear();
    return true;
}

bool ProjectCompilation::setExperimentalCppStaticFieldProvider(
    ExperimentalCppStaticFieldProvider provider, std::string& error)
{
    if (compilationAttempted_)
    {
        error = "experimental C++ static field provider must be set before compilation starts";
        return false;
    }
    experimentalCppStaticFieldProvider_ = std::move(provider);
    error.clear();
    return true;
}

bool ProjectCompilation::setCppFunctionProvider(CppFunctionProvider provider, std::string& error)
{
    if (compilationAttempted_)
    {
        error = "C/C++ function provider must be set before compilation starts";
        return false;
    }
    if (provider && cppImportedFunctionsConfigured_)
    {
        error = "C/C++ functions and a function provider cannot both be configured";
        return false;
    }
    cppFunctionProvider_ = std::move(provider);
    error.clear();
    return true;
}

bool ProjectCompilation::setCppNamespaceProvider(CppNamespaceProvider provider, std::string& error)
{
    if (compilationAttempted_)
    {
        error = "C/C++ namespace provider must be set before compilation starts";
        return false;
    }
    cppNamespaceProvider_ = std::move(provider);
    error.clear();
    return true;
}

bool ProjectCompilation::setCppGlobalProvider(CppGlobalProvider provider, std::string& error)
{
    if (compilationAttempted_)
    {
        error = "C/C++ global provider must be set before compilation starts";
        return false;
    }
    cppGlobalProvider_ = std::move(provider);
    error.clear();
    return true;
}

bool ProjectCompilation::setCppRecordObserver(CppRecordObserver observer, std::string& error)
{
    if (compilationAttempted_)
    {
        error = "C/C++ record observer must be set before compilation starts";
        return false;
    }
    cppRecordObserver_ = std::move(observer);
    error.clear();
    return true;
}

bool ProjectCompilation::setCppMetadataCatalogProvider(CppMetadataCatalogProvider provider, std::string& error)
{
    if (compilationAttempted_)
    {
        error = "C metadata catalog provider must be set before compilation starts";
        return false;
    }
    cppMetadataCatalogProvider_ = std::move(provider);
    error.clear();
    return true;
}

bool ProjectCompilation::collectCppMetadataCatalog(const interop::LibraryMetadata& metadata,
                                                   std::vector<std::uint8_t>& output, std::string& error) const
{
    std::vector<std::uint8_t> result;
    if (cppMetadataCatalogProvider_ && !cppMetadataCatalogProvider_(*this, metadata, result, error)) return false;
    output = std::move(result);
    error.clear();
    return true;
}

bool ProjectCompilation::setCppProjectSessions(
    std::vector<interop::CppProjectSessions> sessions, std::string& error)
{
    if (compilationAttempted_)
    {
        error = "C/C++ project sessions must be set before compilation starts";
        return false;
    }
    for (std::size_t index = 0; index < sessions.size(); ++index)
    {
        auto& group = sessions[index];
        std::error_code ec;
        if (!group.projectFile.is_absolute() || !std::filesystem::is_regular_file(group.projectFile, ec) || ec)
        {
            error = "C/C++ session owner must be an existing absolute project path";
            return false;
        }
        for (std::size_t previous = 0; previous < index; ++previous)
        {
            ec.clear();
            const bool same = std::filesystem::equivalent(group.projectFile, sessions[previous].projectFile, ec);
            if (ec || same)
            {
                error = ec ? "cannot compare C/C++ session project paths" : "duplicate C/C++ session project";
                return false;
            }
        }
        for (const auto& session : group.sessions)
            if (!session || session->state() != interop::CppSessionState::Ready)
            {
                error = "C/C++ project sessions must be non-null and ready";
                return false;
            }
    }
    cppProjectSessions_ = std::move(sessions);
    error.clear();
    return true;
}

std::span<const interop::CppProjectSessions> ProjectCompilation::cppProjectSessions() const noexcept
{
    return cppProjectSessions_;
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

void ProjectCompilation::setCppRuntimeIsolationResolver(CppRuntimeIsolationResolver resolver)
{
    cppRuntimeIsolationResolver_ = std::move(resolver);
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
// EN: T08-D26. A precompiled producer publishes its member signatures with its own C type keys; the consumer binds
//     the reconciled ones to the canonical type. Each whole `type:` key (not a prefix of a longer or nested name)
//     is replaced by its canonical key before comparing signatures.
// ES: T08-D26. Un productor precompilado publica las firmas de sus miembros con sus propias claves de tipo C; el
//     consumidor asocia las conciliadas al tipo canonico. Cada clave `type:` completa (no el prefijo de un nombre
//     mas largo o anidado) se sustituye por su clave canonica antes de comparar firmas.
void canonicalizeReconciledTypeKeys(std::string& value, std::span<const std::pair<std::string, std::string>> keys)
{
    for (const auto& [local, canonical] : keys)
    {
        std::size_t cursor = 0;
        while ((cursor = value.find(local, cursor)) != std::string::npos)
        {
            const std::size_t end = cursor + local.size();
            const bool whole = end == value.size() ||
                !(std::isalnum(static_cast<unsigned char>(value[end])) || value[end] == '_' || value[end] == '.' ||
                  value[end] == '`');
            if (!whole)
            {
                cursor = end;
                continue;
            }
            value.replace(cursor, local.size(), canonical);
            cursor += canonical.size();
        }
    }
}

std::string utf8Path(const std::filesystem::path& path)
{
    const auto encoded = path.generic_u8string();
    return std::string(reinterpret_cast<const char*>(encoded.data()), encoded.size());
}

using lexer::Token;
using lexer::TokenKind;

using internal::ProjectPhaseTimer;

constexpr std::string_view templateParseProductSchema = "nsc-template-parse-product-v1";

// EN: The whole-corpus parse cache stays disconnected. Its key hashes every source buffer of the
//     corpus, the precompiled surfaces included, and a miss publishes the complete parsed corpus
//     (tens of MB for a library over NativeSharpCore) with a SHA-256 of it, on every build whose
//     sources changed. It can only hit when every source is identical, and then the compilation
//     bundle cache already answers the whole compilation. The per-module precompiled syntax cache
//     (`precompiledProductCacheRoot_`) is independent and stays on. With the key left empty,
//     restoreTemplateParseProduct, publishTemplateParseProduct and the corpus snapshots are skipped.
// ES: La caché de parseo del corpus completo queda desconectada. Su clave hashea cada buffer fuente
//     del corpus, incluidas las superficies precompiladas, y un fallo publica el corpus analizado
//     completo (decenas de MB para una biblioteca sobre NativeSharpCore) con su SHA-256, en cada build
//     cuyas fuentes cambiaron. Solo puede acertar cuando todas las fuentes son idénticas, y entonces la
//     caché de bundles de compilación ya responde la compilación completa. La caché de sintaxis
//     precompilada por módulo (`precompiledProductCacheRoot_`) es independiente y sigue activa. Con la
//     clave vacía se omiten restoreTemplateParseProduct, publishTemplateParseProduct y las fotografías
//     del corpus.
constexpr bool kTemplateParseCorpusCacheEnabled = false;

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
    std::size_t sourceOrdinal,
    std::span<const CppImportedTypeDeclaration> importedTypes)
{
    if (productCacheKey.empty()) return {};
    // The experimental scoped parser depends on project visibility and unit-specific imports.
    // Its complete cache identity is not defined yet; do not restore or publish stale products.
    if (!importedTypes.empty()) return {};
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
    const std::string detail = utf8Path(path);
    sink({basic::CompilationEventKind::WorkFinished,
        phase, detail, completed, total, {}});
}

bool isMetadataBackedVirtualSource(const std::filesystem::path& path)
{
    const std::string sourcePath = utf8Path(path);
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
        const auto sourcePath = sourceManager.getPath(SourceLocation(unit->fileId(), 0U));
        const std::filesystem::path path(std::u8string(sourcePath.begin(), sourcePath.end()));
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
    std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
    std::span<const CppImportedTypeDeclaration> importedTypes = {})
{
    std::set<std::string> names;
    for (const auto& imported : importedTypes)
    {
        names.insert(imported.identity.name());
        names.insert(imported.identity.fullyQualifiedName());
    }
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

// Imported names can change the grammar of `T * value;`. Only expose a C++ name to a unit
// when its project can see the owning project through the same reference closure as name lookup.
std::vector<std::shared_ptr<const parser::KnownTypeNameSet>> parserTypeIndexesByProject(
    const compilation::ProjectGraph& graph,
    std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
    std::span<const CppImportedTypeDeclaration> importedTypes)
{
    const auto projects = graph.projects();
    const auto nativeNames = collectParserKnownTypeNames(units);
    const auto nativeIndex = std::make_shared<const parser::KnownTypeNameSet>(parser::buildKnownTypeNameSet(nativeNames));
    if (importedTypes.empty()) return std::vector<std::shared_ptr<const parser::KnownTypeNameSet>>(projects.size(), nativeIndex);
    auto pathKey = [](const std::filesystem::path& path) {
        std::error_code error;
        auto canonical = std::filesystem::weakly_canonical(path, error);
        if (error) canonical = std::filesystem::absolute(path).lexically_normal();
        auto key = utf8Path(canonical);
        std::transform(key.begin(), key.end(), key.begin(),
                       [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
        return key;
    };
    std::unordered_map<std::string, std::size_t> projectByPath;
    for (std::size_t index = 0; index < projects.size(); ++index)
        projectByPath.emplace(pathKey(projects[index].projectFile()), index);
    std::vector<std::vector<std::size_t>> dependencies(projects.size());
    for (std::size_t owner = 0; owner < projects.size(); ++owner)
        for (const auto& reference : projects[owner].projectReferences())
            if (const auto target = projectByPath.find(pathKey(reference)); target != projectByPath.end())
                dependencies[owner].push_back(target->second);
    std::vector<std::shared_ptr<const parser::KnownTypeNameSet>> indexes;
    indexes.reserve(projects.size());
    for (std::size_t owner = 0; owner < projects.size(); ++owner)
    {
        std::vector<bool> visible(projects.size(), false);
        std::vector<std::size_t> pending{owner};
        visible[owner] = true;
        while (!pending.empty())
        {
            const auto current = pending.back(); pending.pop_back();
            for (const auto target : dependencies[current])
                if (!visible[target]) { visible[target] = true; pending.push_back(target); }
        }
        std::vector<CppImportedTypeDeclaration> accessible;
        for (const auto& imported : importedTypes)
            if (imported.projectOrdinal < visible.size() && visible[imported.projectOrdinal])
                accessible.push_back(imported);
        if (accessible.empty()) indexes.push_back(nativeIndex);
        else
        {
            const auto names = collectParserKnownTypeNames(units, accessible);
            indexes.push_back(std::make_shared<const parser::KnownTypeNameSet>(parser::buildKnownTypeNameSet(names)));
        }
    }
    return indexes;
}

bool prepareCppScopedParserNames(
    const compilation::ProjectGraph& graph,
    std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
    std::span<const CppImportedTypeDeclaration> types,
    std::span<const std::pair<std::size_t, std::string>> cppNamespaces,
    std::span<const CppImportedTypeAlias> aliases,
    std::vector<std::optional<parser::ScopedKnownTypeNames>>& output,
    std::string& error)
{
    output.clear();
    output.resize(units.size());
    if (types.empty() && aliases.empty()) return true;
    const auto inventories = cppParserProjectInventories(graph, units, types, cppNamespaces, aliases);
    if (!inventories) { error = "cannot prepare C/C++ parser project visibility"; return false; }
    for (std::size_t i = 0; i < units.size(); ++i)
    {
        const auto& unit = units[i];
        if (!unit || !unit->isLexicallyValid()) continue;
        const auto& inventory = inventories->at(unit->projectOrdinal());
        if (inventory.cppTypes.empty()) continue;
        const auto scopes = parser::lexicalNamespaceScopes(unit->tokens());
        auto names = cppScopedTypeNames(scopes, inventory.namespaces, inventory.cppTypes);
        if (!names)
        {
            error = "cannot prepare scoped C/C++ type names for source unit " + std::to_string(i) +
                ": namespace or alias requires unsupported resolution";
            return false;
        }
        output[i] = std::move(*names);
    }
    return true;
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
    if (cppRecordObserver_) cppRecordObserver_(*this);
    nameLookup_ = std::make_unique<ProjectNameLookup>(lifetime_->indexes());
    std::vector<std::pair<std::size_t, std::string>> importedNamespaces;
    for (const auto& function : cppImportedFunctions_)
        if (!function.namespaceName.empty())
        {
            if (function.projectOrdinal >= projectGraph_->projects().size() ||
                projectGraph_->projects()[function.projectOrdinal].moduleName() != function.moduleName)
            {
                loadError = "imported C/C++ namespace has an invalid project owner";
                return false;
            }
            importedNamespaces.emplace_back(function.projectOrdinal, function.namespaceName);
        }
    for (const auto& state : experimentalCppImportedStates_)
    {
        if (state.projectOrdinal >= projectGraph_->projects().size() ||
            projectGraph_->projects()[state.projectOrdinal].moduleName() != state.moduleName)
        {
            loadError = "imported C/C++ global has an invalid project owner";
            return false;
        }
        if (!state.namespaceName.empty())
            importedNamespaces.emplace_back(state.projectOrdinal, state.namespaceName);
    }
    if (cppNamespaceProvider_ &&
        !cppNamespaceProvider_(*projectGraph_, cppProjectSessions_, importedNamespaces, loadError))
        return false;
    if (!nameLookup_->setImportedNamespaces(std::move(importedNamespaces))) return false;
    std::vector<CppImportedTypeDeclaration> selectedCppTypes = experimentalCppImportedTypes_;
    if (experimentalCppTypeSelector_ && !experimentalCppTypeSelector_(*this, selectedCppTypes, loadError))
        return false;
    if (!nameLookup_->setCppImportedTypes(std::move(selectedCppTypes)))
    {
        loadError = "cannot configure experimental C/C++ types";
        return false;
    }
    if (!nameLookup_->setCppImportedTypeAliases(cppImportedTypeAliases_))
    {
        loadError = "cannot configure imported C/C++ type aliases";
        return false;
    }
    nameLookup_->setPrecompiledSnapshots(precompiledSnapshots_);
    if (!experimentalCppLifetimeMethodProvider_ && !experimentalCppInstanceMethodProvider_ && !experimentalCppInstanceFieldProvider_ &&
        !experimentalCppStaticMethodProvider_ && !experimentalCppStaticFieldProvider_ && precompiledSnapshotCapture_.has_value())
        nameLookup_->setPrecompiledSnapshotCaptureUnit(precompiledSnapshotCapture_->unitOrdinal);
    if (!nameLookup_->build(
            *projectGraph_, units_, diagnostics_, compilationJobs_,
            &lifetime_->workerPool()))
    {
        return false;
    }
    if (reportSink_ && !precompiledSnapshots_.empty())
    {
        reportSink_("precompiled-snapshot-install: names=" +
            std::to_string(nameLookup_->installedSnapshotUnitCount()) + " name-declarations=" +
            std::to_string(nameLookup_->installedDeclarationUnitCount()));
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
    genericModel_->setCaptureForeignReferenceCandidates(!cppProjectSessions_.empty());
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
    if (!buildMemberModel(loadError)) return false;
    if (!memberModel_->buildModuleInitializationPlan(*projectGraph_, loadError)) return false;

    std::vector<PrecompiledMemberIdentity> precompiledMembers;
    std::vector<std::pair<std::string, std::string>> specializationTypeAliases;
    std::unordered_map<std::string_view, const GenericSpecializationRecord*>
        localSpecializationsByKey;
    localSpecializationsByKey.reserve(genericModel_->specializations().size());
    for (const GenericSpecializationRecord& specialization : genericModel_->specializations())
        localSpecializationsByKey.emplace(
            specialization.specializationKey, &specialization);
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
    const auto specializationStem = [](std::string_view key) {
        if (!key.starts_with("type:")) return std::string_view{};
        const std::size_t generic = key.rfind("__g");
        const std::size_t hash = key.rfind("__h");
        if (generic == std::string_view::npos || hash == std::string_view::npos ||
            generic >= hash || hash + 3U + 16U != key.size())
        {
            return std::string_view{};
        }
        return key.substr(0, hash);
    };
    for (const PrecompiledSyntaxModule& module : precompiledModules_)
    {
        precompiledMembers.insert(precompiledMembers.end(), module.members.begin(), module.members.end());
        std::error_code metadataPathError;
        const std::filesystem::path canonicalMetadata =
            std::filesystem::weakly_canonical(module.metadataPath, metadataPathError);
        const std::optional<std::size_t> moduleProjectOrdinal = metadataPathError
            ? std::optional<std::size_t>{}
            : precompiledProjectOrdinal(canonicalMetadata);
        const std::string publishedTypePrefix = "type:" + module.moduleName + "::";
        const auto argumentMatches = [&](std::string_view localArgument, std::string_view publishedArgument) {
            if (localArgument == publishedArgument) return true;
            if (!moduleProjectOrdinal.has_value() ||
                !localArgument.starts_with("type:") ||
                !publishedArgument.starts_with(publishedTypePrefix))
            {
                return false;
            }
            const std::size_t localModuleSeparator = localArgument.find("::", 5U);
            if (localModuleSeparator == std::string_view::npos ||
                localArgument.substr(localModuleSeparator + 2U) !=
                    publishedArgument.substr(publishedTypePrefix.size()))
            {
                return false;
            }
            const std::string_view localIdentity = localArgument.substr(5U);
            return std::ranges::any_of(nameLookup_->namedTypes(), [&](const NamedTypeDeclaration& type) {
                return type.isPrecompiled && type.projectOrdinal == *moduleProjectOrdinal &&
                    type.identity.keyView() == localIdentity;
            });
        };
        for (const PrecompiledGenericSpecialization& imported : module.genericSpecializations)
        {
            if (imported.kind == GenericTemplateKind::Method) continue;
            const std::string publishedTypeKey = typeKey(imported.templateKey, imported.specializedName);
            if (publishedTypeKey.empty()) continue;
            const auto appendAlias = [&](const GenericSpecializationRecord& local) {
                const std::string localTypeKey = typeKey(local.templateKey, local.specializedName);
                if (!localTypeKey.empty())
                    specializationTypeAliases.emplace_back(localTypeKey, publishedTypeKey);
            };
            const auto exact = localSpecializationsByKey.find(imported.specializationKey);
            if (exact != localSpecializationsByKey.end()) appendAlias(*exact->second);
            for (const GenericSpecializationRecord& local : genericModel_->specializations())
            {
                if ((exact != localSpecializationsByKey.end() && exact->second == &local) ||
                    local.kind != imported.kind || local.templateKey != imported.templateKey ||
                    local.typeArguments.size() != imported.typeArguments.size())
                {
                    continue;
                }
                bool sameArguments = true;
                for (std::size_t index = 0; index < local.typeArguments.size(); ++index)
                {
                    if (!argumentMatches(local.typeArguments[index], imported.typeArguments[index]))
                    {
                        sameArguments = false;
                        break;
                    }
                }
                if (sameArguments) appendAlias(local);
            }
        }
        // Provider-owned closed generics are recorded as external type keys rather than as
        // generic-specialization records. Match their structural name only when it identifies one
        // local specialization; the producer's suffix then restores the signature published in NSMETA.
        for (const std::string& externalTypeKey : module.externalTypeKeys)
        {
            const std::string_view externalStem = specializationStem(externalTypeKey);
            if (externalStem.empty()) continue;
            std::string localTypeKey;
            bool ambiguous = false;
            for (const GenericSpecializationRecord& local : genericModel_->specializations())
            {
                if (local.kind == GenericTemplateKind::Method) continue;
                const std::string candidate = typeKey(local.templateKey, local.specializedName);
                if (specializationStem(candidate) != externalStem) continue;
                if (!localTypeKey.empty() && localTypeKey != candidate)
                {
                    ambiguous = true;
                    break;
                }
                localTypeKey = candidate;
            }
            if (!ambiguous && !localTypeKey.empty())
                specializationTypeAliases.emplace_back(std::move(localTypeKey), externalTypeKey);
        }
    }
    const auto reconciledTypeKeys = nameLookup_->reconciledCppTypeKeys();
    if (!reconciledTypeKeys.empty())
        for (PrecompiledMemberIdentity& identity : precompiledMembers)
        {
            canonicalizeReconciledTypeKeys(identity.declaringTypeKey, reconciledTypeKeys);
            canonicalizeReconciledTypeKeys(identity.signatureKey, reconciledTypeKeys);
        }
    if (!memberModel_->bindPrecompiledPhysicalNames(
            precompiledMembers, specializationTypeAliases, loadError))
    {
        return false;
    }

    if (!buildNamespaceFunctionModel(loadError)) return false;
    std::vector<PrecompiledNamespaceFunctionIdentity> precompiledNamespaceFunctions;
    for (const PrecompiledSyntaxModule& module : precompiledModules_)
    {
        precompiledNamespaceFunctions.insert(
            precompiledNamespaceFunctions.end(),
            module.namespaceFunctions.begin(), module.namespaceFunctions.end());
    }
    for (PrecompiledNamespaceFunctionIdentity& identity : precompiledNamespaceFunctions)
        canonicalizeReconciledTypeKeys(identity.signatureKey, reconciledTypeKeys);
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
    if (experimentalCppLifetimeMethodProvider_ && !experimentalCppLifetimeAnalysisForTests_)
    {
        loadError = "C++ lifetime members are not yet supported beyond experimental member indexing";
        return false;
    }
    if ((experimentalCppInstanceFieldProvider_ || experimentalCppStaticFieldProvider_) &&
        !experimentalCppInstanceFieldLoweringForTests_ && memberModel_ &&
        std::any_of(memberModel_->members().begin(), memberModel_->members().end(),
                    [](const auto& member) { return member.isCppExternal && member.kind == ClassMemberKind::Field; }))
    {
        loadError = "C++ fields are not yet supported beyond experimental member indexing";
        return false;
    }
    if ((experimentalCppInstanceMethodProvider_ || experimentalCppStaticMethodProvider_) &&
        !experimentalCppInstanceMethodLoweringForTests_ && memberModel_ &&
        std::any_of(memberModel_->members().begin(), memberModel_->members().end(),
                    [](const auto& member) { return member.isCppExternal; }))
    {
        loadError = "C++ instance methods are not yet supported beyond experimental member indexing";
        return false;
    }
    if (cancellationRequested(loadError, "inheritance")) return false;

    const ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "inheritance");
    return buildInheritanceModel();
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
    if (experimentalCppLifetimeAnalysisForTests_ && experimentalCppLifetimeMethodProvider_)
        methodBodyModel_->enableCppLifetimeAnalysisForTests();
    if (experimentalCppLocalLifetimeAnalysisForTests_)
        methodBodyModel_->enableCppLocalLifetimeAnalysisForTests();
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
    // EN: Freeze 3.0 (item 130a): previews constructor selection while the bodies are analyzed.
    // ES: Congelamiento 3.0 (elemento 130a): anticipa la eleccion de constructores mientras se analizan
    //     los cuerpos.
    ProjectConstructionModel constructorDemandResolver;
    constructorDemandResolver.attachDemandResolver(
        *nameLookup_, *memberModel_, *inheritanceModel_, *interfaceModel_, *methodBodyModel_,
        *namespaceFunctionModel_);
    methodBodyOptions.constructorDemandResolver = &constructorDemandResolver;
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
    return buildInterfaceModel();
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
    return buildVirtualDispatchModel();
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
    return buildDestructionModel();
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
    return buildConstructionModel();
}

bool ProjectCompilation::compileProjectObjectLayouts(
    const std::filesystem::path& projectFile,
    std::string& loadError)
{
    if (!compileProjectConstructions(projectFile, loadError))
    {
        return false;
    }
    if (experimentalCppLifetimeAnalysisForTests_ && !experimentalCppDiscardedLifetimeLoweringForTests_ &&
        !experimentalCppLocalLifetimeAnalysisForTests_)
    {
        loadError = "experimental C++ lifetime analysis cannot proceed to layout or code generation";
        return false;
    }
    if (cancellationRequested(loadError, "layout")) return false;

    const ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "layout");
    if (!buildObjectLayoutModel()) return false;
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

    // EN: T08-D13. The volatile mode of C storage, and of every NativeSharp alias of it, is the VolatileMode of
    //     the CppLibraryReference that declares it, never a mode of the consuming project; the project property
    //     is therefore rejected.
    // ES: T08-D13. El modo volatile del almacenamiento C, y de todo alias NativeSharp suyo, es el VolatileMode de
    //     la CppLibraryReference que lo declara, nunca un modo del proyecto consumidor; por eso se rechaza la
    //     propiedad del proyecto.
    for (const auto& project : projectGraph_->projects())
    {
        if (project.cppConsumerVolatileMode().has_value())
        {
            loadError = "CppConsumerVolatileMode is not supported: the volatile mode of C storage and of its "
                "aliases is the VolatileMode of its CppLibraryReference: " + project.projectFile().string();
            return false;
        }
    }

    const ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "oop-hir");
    oopHirModel_ = std::make_unique<ProjectOopHirModel>(lifetime_->indexes());
    oopHirModel_->setCppVariadicCallPlanProvider(cppVariadicCallPlanProvider_);
    oopHirModel_->setCppFunctionPointerPlanProvider(cppFunctionPointerPlanProvider_);
    std::unordered_map<std::uint32_t, compilation::CppConsumerVolatileMode> cppConsumerModes;
    for (const auto& unit : units_)
    {
        if (!unit || !unit->fileId().isValid() || unit->projectOrdinal() >= projectGraph_->projects().size())
            continue;
        const auto mode = projectGraph_->projects()[unit->projectOrdinal()].cppConsumerVolatileMode();
        if (mode) cppConsumerModes.emplace(unit->fileId().value(), *mode);
    }
    oopHirModel_->setCppConsumerVolatileModes(std::move(cppConsumerModes));
    // EN: T08-D05b. DLLs whose TLS aliases must isolate homonymous dependencies, resolved by the driver.
    // ES: T08-D05b. DLL cuyos alias TLS deben aislar dependencias homonimas, resueltas por el driver.
    if (cppRuntimeIsolationResolver_)
    {
        std::unordered_set<std::string> isolated;
        if (!cppRuntimeIsolationResolver_(*this, isolated, loadError)) return false;
        oopHirModel_->setCppIsolatedDllIdentities(std::move(isolated));
    }
    if (oopHirModel_->build(
            *nameLookup_, *memberModel_, *inheritanceModel_, *interfaceModel_, *methodBodyModel_,
            *constructionModel_, *objectLayoutModel_, loadError, genericConstraintModel_.get(),
            compilationJobs_, namespaceFunctionModel_.get(),
            metaprogrammingModel_.get(), &lifetime_->workerPool()))
    {
        if (experimentalCppDiscardedLifetimeLoweringForTests_ || experimentalCppLocalLifetimeAnalysisForTests_)
        {
            using HirNode = ProjectOopHirNode;
            std::vector<std::pair<const HirNode*, const HirNode*>> pending;
            for (const auto& function : oopHirModel_->functions())
                if (function.body) pending.emplace_back(function.body.get(), nullptr);
            while (!pending.empty())
            {
                const auto [node, parent] = pending.back();
                pending.pop_back();
                for (const auto& child : node->children) if (child) pending.emplace_back(child.get(), node);
                const auto valueOrdinal = node->typeKey.starts_with("type:")
                    ? nameLookup_->typeOrdinalForIdentity(std::string_view(node->typeKey).substr(5)) : std::nullopt;
                const bool cppValue = valueOrdinal && nameLookup_->cppImportedType(*valueOrdinal);
                if (experimentalCppLocalLifetimeAnalysisForTests_ && cppValue &&
                    node->kind == ProjectOopHirNodeKind::LocalDeclaration && !node->referenceLocal)
                {
                    if (node->staticLocal || !node->localOrdinal || !node->cleanupRegionOrdinal ||
                        !node->automaticDestruction || node->children.size() != 1 || !node->children[0] ||
                        node->children[0]->kind != ProjectOopHirNodeKind::ValueConstruction ||
                        node->children[0]->targetTypeOrdinal != valueOrdinal)
                    { loadError = "experimental C++ local requires direct automatic construction and cleanup"; return false; }
                }
                else if (experimentalCppLocalLifetimeAnalysisForTests_ && cppValue &&
                    node->kind != ProjectOopHirNodeKind::ValueConstruction &&
                    node->kind != ProjectOopHirNodeKind::LocalAddress &&
                    !(node->kind == ProjectOopHirNodeKind::LocalDeclaration && node->referenceLocal))
                { loadError = "experimental C++ local cannot copy or transfer a C++ value"; return false; }
                const bool construction = node->kind == ProjectOopHirNodeKind::ValueConstruction ||
                    node->kind == ProjectOopHirNodeKind::ObjectConstruction ||
                    node->kind == ProjectOopHirNodeKind::PlacementConstruction;
                if (!construction || !node->targetTypeOrdinal ||
                    !nameLookup_->cppImportedType(*node->targetTypeOrdinal)) continue;
                const bool discarded = experimentalCppDiscardedLifetimeLoweringForTests_ && parent &&
                    parent->kind == ProjectOopHirNodeKind::ExpressionStatement && parent->children.size() == 1;
                const bool local = experimentalCppLocalLifetimeAnalysisForTests_ && parent &&
                    parent->kind == ProjectOopHirNodeKind::LocalDeclaration && !parent->referenceLocal &&
                    !parent->staticLocal && parent->children.size() == 1 && parent->children[0].get() == node &&
                    parent->typeKey == node->typeKey && parent->localOrdinal && parent->cleanupRegionOrdinal &&
                    parent->automaticDestruction;
                if (node->kind != ProjectOopHirNodeKind::ValueConstruction || (!discarded && !local) ||
                    !node->memberOrdinal || *node->memberOrdinal >= memberModel_->members().size())
                { loadError = "experimental C++ lifetime requires direct local or discarded construction"; return false; }
                const auto& constructor = memberModel_->members()[*node->memberOrdinal];
                const auto destructors = memberModel_->findMembers(*node->targetTypeOrdinal, ".dtor");
                const bool scalarArguments = constructor.parameters.size() == node->argumentCount &&
                    std::all_of(constructor.parameters.begin(), constructor.parameters.end(), [](const auto& parameter) {
                        std::string_view type = parameter.typeKey;
                        const bool pointer = type.ends_with('*');
                        while (type.ends_with('*')) type.remove_suffix(1);
                        return parameter.passKind == ParameterPassKind::Value &&
                            ((pointer && type == "builtin:void") || type == "builtin:bool" || type == "builtin:sbyte" || type == "builtin:byte" ||
                             type == "builtin:short" || type == "builtin:ushort" || type == "builtin:int" ||
                             type == "builtin:uint" || type == "builtin:long" || type == "builtin:ulong" ||
                             type == "builtin:float" || type == "builtin:double");
                    });
                if (!constructor.isCppExternal || constructor.kind != ClassMemberKind::Constructor ||
                    !scalarArguments || destructors.size() != 1 ||
                    destructors[0] >= memberModel_->members().size() ||
                    !memberModel_->members()[destructors[0]].isCppExternal ||
                    memberModel_->members()[destructors[0]].kind != ClassMemberKind::Destructor)
                { loadError = "experimental C++ lifetime requires a supported constructor and unique destructor"; return false; }
                if (local && cppNoexceptLocalLifetimeOnly_ &&
                    (!constructor.cppLifetimeNonThrowing ||
                     !memberModel_->members()[destructors[0]].cppLifetimeNonThrowing))
                { loadError = "C++ local lifetime currently requires a non-throwing constructor and destructor"; return false; }
            }
        }
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
        utf8Path(relativePath),
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
        loadError = utf8Path(read.path) + ": " +
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
                "ProjectCompilation::precompiledProjectOrdinal", utf8Path(canonicalMetadataPath));
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
    if (!experimentalCppLifetimeMethodProvider_ && !experimentalCppInstanceMethodProvider_ && !experimentalCppInstanceFieldProvider_ && !experimentalCppStaticMethodProvider_ && !experimentalCppStaticFieldProvider_)
        syntaxCacheKey = precompiledSyntaxCacheKey(module.productCacheKey, sourceOrdinal, experimentalCppImportedTypes_);
    const bool hasPublishedSyntax = !experimentalCppLifetimeMethodProvider_ && !experimentalCppInstanceMethodProvider_ && !experimentalCppInstanceFieldProvider_ && !experimentalCppStaticMethodProvider_ && !experimentalCppStaticFieldProvider_ && experimentalCppImportedTypes_.empty() && !module.productSidecarPath.empty();
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
    std::string metadataKey = utf8Path(canonicalMetadata);
    if (metadataKeyError)
    {
        metadataKey = utf8Path(std::filesystem::absolute(module.metadataPath).lexically_normal());
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
    // EN: The public API unit is the first unit a module adds; a module whose metadata was already
    //     materialized adds none.
    // ES: La unidad de API pública es la primera unidad que agrega un módulo; un módulo cuya
    //     metadata ya se materializó no agrega ninguna.
    precompiledApiUnits_.assign(precompiledModules_.size(), std::nullopt);
    for (std::size_t moduleIndex = 0; moduleIndex < precompiledModules_.size(); ++moduleIndex)
    {
        const std::size_t firstUnit = units_.size();
        if (!materializePrecompiledModule(
                precompiledModules_[moduleIndex], materializedMetadata, loadError))
        {
            return false;
        }
        if (units_.size() > firstUnit) precompiledApiUnits_[moduleIndex] = firstUnit;
    }
    if (!experimentalCppLifetimeMethodProvider_ && !experimentalCppInstanceMethodProvider_ && !experimentalCppInstanceFieldProvider_ && !experimentalCppStaticMethodProvider_ && !experimentalCppStaticFieldProvider_ && experimentalCppImportedTypes_.empty()) loadPrecompiledSnapshots();
    return true;
}

// Computes the whole syntax-corpus identity while every final source buffer is resident but before
// lexer or parser work starts. The identity deliberately excludes physical paths so equivalent IDE
// temporary directories can share a hit; unit order, logical ownership, source bytes and compiler
// build identity still make restored FileIds and parser-wide known-type inputs deterministic.
bool ProjectCompilation::calculateTemplateParseCacheKey(std::string& loadError)
{
    templateParseCacheKey_.clear();
    if (experimentalCppLifetimeMethodProvider_ || experimentalCppInstanceMethodProvider_ || experimentalCppInstanceFieldProvider_ || experimentalCppStaticMethodProvider_ || experimentalCppStaticFieldProvider_ || !experimentalCppImportedTypes_.empty()) return true;
    if (!kTemplateParseCorpusCacheEnabled || templateCacheRoot_.empty()) return true;

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
    const auto knownTypesByProject = parserTypeIndexesByProject(*projectGraph_, units_, {});
    std::vector<std::pair<std::size_t, std::string>> cppNamespaces;
    if (!experimentalCppImportedTypes_.empty() && cppNamespaceProvider_ &&
        !cppNamespaceProvider_(*projectGraph_, cppProjectSessions_, cppNamespaces, loadError)) return false;
    std::vector<CppImportedTypeDeclaration> parserTypes = experimentalCppImportedTypes_;
    if (cppPrecompiledParserTypeProvider_ &&
        !cppPrecompiledParserTypeProvider_(*this, parserTypes, loadError)) return false;
    std::vector<std::optional<parser::ScopedKnownTypeNames>> scopedNames;
    if (!prepareCppScopedParserNames(*projectGraph_, units_, parserTypes,
                                     cppNamespaces, cppImportedTypeAliases_, scopedNames, loadError)) return false;
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
            units_[index]->parse(localDiagnostics[index], *knownTypesByProject.at(units_[index]->projectOrdinal()),
                                 scopedNames[index] ? &*scopedNames[index] : nullptr);
            const auto sourceName = sourceManager_.getPath(SourceLocation(units_[index]->fileId(), 0U));
            const std::filesystem::path sourcePath(std::u8string(sourceName.begin(), sourceName.end()));
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
    compilationAttempted_ = true;
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
    for (const auto& project : projectGraph_->projects())
    {
        if (!project.cppLibraryReferences().empty())
        {
            const interop::CppProjectSessions* group = nullptr;
            for (const auto& candidate : cppProjectSessions_)
            {
                std::error_code ec;
                if (std::filesystem::equivalent(candidate.projectFile, project.projectFile(), ec) && !ec)
                { group = &candidate; break; }
            }
            bool valid = cppNamespaceProvider_ && cppFunctionProvider_ && group != nullptr &&
                group->sessions.size() == project.cppLibraryReferences().size();
            for (std::size_t i = 0; valid && i < project.cppLibraryReferences().size(); ++i)
            {
                const auto& reference = project.cppLibraryReferences()[i];
                const auto options = group->sessions[i]->optionsSnapshot();
                const auto* input = std::get_if<interop::CppFileInput>(&options.input);
                std::error_code ec;
                valid = group->sessions[i]->state() == interop::CppSessionState::Ready && input != nullptr &&
                    std::filesystem::equivalent(input->path, reference.headerPath, ec) && !ec &&
                    options.language == reference.language && options.languageStandard == reference.languageStandard &&
                    options.volatileMode == reference.volatileMode &&
                    options.targetTriple == "x86_64-pc-windows-msvc" &&
                    options.macros.size() == reference.macros.size() &&
                    options.includePaths.size() >= reference.includePaths.size();
                for (std::size_t j = 0; valid && j < reference.macros.size(); ++j)
                    valid = options.macros[j].action == reference.macros[j].action &&
                        options.macros[j].text == reference.macros[j].text;
                for (std::size_t j = 0; valid && j < reference.includePaths.size(); ++j)
                {
                    ec.clear();
                    valid = options.includePaths[j].kind == reference.includePaths[j].kind &&
                        std::filesystem::equivalent(options.includePaths[j].path, reference.includePaths[j].path, ec) && !ec;
                }
            }
            if (!valid)
            {
                loadError = "C/C++ project references are not yet connected to NativeSharp compilation";
                return false;
            }
        }
    }
    if (cancellationRequested(loadError, "syntax")) return false;
    std::vector<std::filesystem::path> cppTypeOwners;
    cppTypeOwners.reserve(experimentalCppImportedTypes_.size());
    for (const auto& type : experimentalCppImportedTypes_)
    {
        if (type.projectOrdinal >= projectGraph_->projects().size() ||
            projectGraph_->projects()[type.projectOrdinal].moduleName() != type.identity.moduleName())
        {
            loadError = "experimental C/C++ type has an invalid project owner before precompiled projects";
            return false;
        }
        cppTypeOwners.push_back(projectGraph_->projects()[type.projectOrdinal].projectFile());
    }
    if (!addPrecompiledProjects(loadError))
    {
        return false;
    }
    for (std::size_t i = 0; i < experimentalCppImportedTypes_.size(); ++i)
    {
        auto& type = experimentalCppImportedTypes_[i];
        std::optional<std::size_t> owner;
        for (std::size_t ordinal = 0; ordinal < projectGraph_->projects().size(); ++ordinal)
        {
            const auto& project = projectGraph_->projects()[ordinal];
            if (project.moduleName() != type.identity.moduleName()) continue;
            std::error_code ec;
            if (!std::filesystem::equivalent(cppTypeOwners[i], project.projectFile(), ec) || ec) continue;
            if (owner) { loadError = "ambiguous C/C++ type project after precompiled projects"; return false; }
            owner = ordinal;
        }
        if (!owner) { loadError = "C/C++ type project disappeared after precompiled projects"; return false; }
        type.projectOrdinal = *owner;
    }
    if (experimentalCppTypeProvider_)
    {
        std::vector<CppImportedTypeDeclaration> types;
        if (!experimentalCppTypeProvider_(*projectGraph_, cppProjectSessions_, types, loadError)) return false;
        for (const auto& type : types)
            if (type.projectOrdinal >= projectGraph_->projects().size() ||
                projectGraph_->projects()[type.projectOrdinal].moduleName() != type.identity.moduleName())
            {
                loadError = "experimental C/C++ type provider returned an invalid project owner";
                return false;
            }
        ProjectNameLookup validation;
        if (!validation.setCppImportedTypes(types))
        {
            loadError = "experimental C/C++ type provider returned invalid declarations";
            return false;
        }
        experimentalCppImportedTypes_ = std::move(types);
    }
    if (cppTypeAliasProvider_)
    {
        std::vector<CppImportedTypeAlias> aliases;
        if (!cppTypeAliasProvider_(*projectGraph_, cppProjectSessions_, aliases, loadError)) return false;
        for (const auto& alias : aliases)
        {
            if (alias.projectOrdinal >= projectGraph_->projects().size())
            {
                loadError = "C/C++ type alias provider returned an invalid project owner";
                return false;
            }
            std::error_code ec;
            if (!std::filesystem::equivalent(alias.projectFile,
                    projectGraph_->projects()[alias.projectOrdinal].projectFile(), ec) || ec)
            {
                loadError = "C/C++ type alias provider returned inconsistent project provenance";
                return false;
            }
        }
        cppImportedTypeAliases_ = std::move(aliases);
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

bool ProjectCompilation::cloneSyntaxForObservation(
    std::vector<std::unique_ptr<ProjectSyntaxUnit>>& output,
    diagnostics::DiagnosticEngine& diagnostics,
    std::string& error) const
{
    error.clear();
    if (!projectGraph_ || units_.empty())
    {
        error = "syntax observation requires a parsed project";
        return false;
    }
    std::vector<std::unique_ptr<ProjectSyntaxUnit>> copied;
    copied.reserve(units_.size());
    const auto knownTypesByProject = parserTypeIndexesByProject(*projectGraph_, units_, {});
    std::vector<std::pair<std::size_t, std::string>> cppNamespaces;
    if (!experimentalCppImportedTypes_.empty() && cppNamespaceProvider_ &&
        !cppNamespaceProvider_(*projectGraph_, cppProjectSessions_, cppNamespaces, error)) return false;
    std::vector<CppImportedTypeDeclaration> parserTypes = experimentalCppImportedTypes_;
    if (cppPrecompiledParserTypeProvider_ &&
        !cppPrecompiledParserTypeProvider_(*this, parserTypes, error)) return false;
    std::vector<std::optional<parser::ScopedKnownTypeNames>> scopedNames;
    if (!prepareCppScopedParserNames(*projectGraph_, units_, parserTypes,
                                     cppNamespaces, cppImportedTypeAliases_, scopedNames, error)) return false;
    for (std::size_t unitIndex = 0; unitIndex < units_.size(); ++unitIndex)
    {
        const auto& unit = units_[unitIndex];
        if (!unit || !unit->isSyntaxValid())
        {
            error = "syntax observation requires valid source units";
            return false;
        }
        const auto* source = sourceManager_.getFile(unit->fileId());
        if (!source)
        {
            error = "syntax observation cannot find a captured source buffer";
            return false;
        }
        auto clone = std::make_unique<ProjectSyntaxUnit>(unit->projectOrdinal(), unit->sourceOrdinal(),
            std::string(unit->moduleName()), unit->fileId(), unit->isPrecompiled());
        if (unit->isPrecompiled())
        {
            const std::string payload = internal::serializeSyntaxProductUnit(*unit);
            if (!internal::deserializeSyntaxProductUnit(
                    *source, payload, clone->tokens_, clone->arena_, clone->syntaxTree_))
            {
                error = "syntax observation could not copy a precompiled syntax product";
                return false;
            }
            clone->lexicallyValid_ = true;
            clone->syntaxValid_ = true;
        }
        else if (!clone->lex(*source, diagnostics) ||
                 !clone->parse(diagnostics, *knownTypesByProject.at(unit->projectOrdinal()),
                               scopedNames[unitIndex] ? &*scopedNames[unitIndex] : nullptr))
        {
            error = "syntax observation could not reparse a captured source buffer";
            return false;
        }
        copied.push_back(std::move(clone));
    }
    output = std::move(copied);
    return true;
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
