#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/PrecompiledTemplateSources.h"
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

#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <exception>
#include <functional>
#include <map>
#include <set>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
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

bool ProjectCompilation::setCppVariadicFunctionPointerPlanProvider(
    CppVariadicFunctionPointerPlanProvider provider, std::string& error)
{
    if (compilationAttempted_) { error = "C/C++ variadic function pointer plan provider must be set before compilation starts"; return false; }
    cppVariadicFunctionPointerPlanProvider_ = std::move(provider);
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

bool ProjectCompilation::appendCppProjectSessions(
    std::vector<interop::CppProjectSessions> sessions, std::string& error)
{
    std::vector<interop::CppProjectSessions> combined;
    combined.reserve(cppProjectSessions_.size() + sessions.size());
    for (auto& group : cppProjectSessions_) combined.push_back(std::move(group));
    for (auto& group : sessions) combined.push_back(std::move(group));
    cppProjectSessions_.clear();
    if (setCppProjectSessions(std::move(combined), error)) return true;
    return false;
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
    nameLookup_->setCppNativeMirrorTypes(cppNativeMirrorTypes_);
    if (!nameLookup_->setCppImportedTypeAliases(cppImportedTypeAliases_))
    {
        loadError = "cannot configure imported C/C++ type aliases";
        return false;
    }
    nameLookup_->setGenericModel(genericModel_.get());
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
    // EN: The generic model's C1 record cache and C3 clone cache stay disconnected. C3 persists a
    //     clone after its nested references were rewritten, keys it only
    //     by the specialization key (self-references inside a restored clone keep the producer's
    //     spelling), and its hit path in executeTypeDemand returns without rewriting the requesting
    //     syntax; resolved argument identities are not persisted either. C1 rebinds every field on
    //     a hit but keeps dependency lists computed by another compilation. Both therefore change
    //     the specializations and metadata that a cold build produces.
    // ES: La caché C1 de registros y la caché C3 de clones del modelo genérico siguen
    //     desconectadas. C3 persiste un clon después de reescribir sus
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
        genericModel_->addPrecompiledFieldCertificates(module.fieldCertificates);
        genericModel_->addPrecompiledTemplateFieldCertificates(module.templateFieldCertificates);
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
        for (const std::string& key : module.trivialCleanupRootTypeKeys)
            memberModel_->addTrivialCleanupRootTypeKey(key);
        for (const std::string& key : module.nonEscapingLifecycleTypeKeys)
            memberModel_->addNonEscapingLifecycleTypeKey(key);
        for (const std::string& key : module.identitySafeLifecycleTypeKeys)
            memberModel_->addIdentitySafeLifecycleTypeKey(key);
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

    return buildNamespaceFunctions(loadError);
}

// T13-005: also run again with the deduced C++ function template specializations.
// T13-005: tambien se repite con las especializaciones deducidas de plantillas de funcion C++.
bool ProjectCompilation::buildNamespaceFunctions(std::string& loadError)
{
    const auto reconciledTypeKeys = nameLookup_->reconciledCppTypeKeys();
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

// EN: T13-005. Each recorded call is offered to the sessions of the project that declares the template, with
//     the C++ type of each argument: a builtin type, a pointer to one, or an imported C/C++ type by its Clang
//     identity. A call whose deduction Clang rejects keeps the rejection only when no other function can be
//     called there.
// ES: T13-005. Cada llamada registrada se ofrece a las sesiones del proyecto que declara la plantilla, con el
//     tipo C++ de cada argumento: un tipo basico, un puntero a uno o un tipo C/C++ importado por su identidad
//     de Clang. Una llamada cuya deduccion rechaza Clang conserva el rechazo solo si no hay otra funcion que
//     llamar ahi.
// EN: T19-040. The text of an argument as the call wrote it: its tokens, with one space where the source
//     separated them, as C writes an argument for `#`.
// ES: T19-040. El texto de un argumento tal como lo escribio la llamada: sus tokens, con un espacio donde la fuente
//     los separaba, como C escribe un argumento para `#`.
bool ProjectCompilation::macroArgumentSideEffectFree(const ast::AstNode* argument) const
{
    if (!argument) return false;
    switch (argument->kind())
    {
        case ast::AstNodeKind::IntegerLiteralExpression:
        case ast::AstNodeKind::FloatingLiteralExpression:
        case ast::AstNodeKind::CharacterLiteralExpression:
        case ast::AstNodeKind::StringLiteralExpression:
        case ast::AstNodeKind::BooleanLiteralExpression:
        case ast::AstNodeKind::NullLiteralExpression:
        case ast::AstNodeKind::SizeofTypeExpression:
        case ast::AstNodeKind::AlignofTypeExpression:
        case ast::AstNodeKind::OffsetofExpression:
            return true;
        case ast::AstNodeKind::IdentifierExpression:
            // A volatile C/C++ global is read once per evaluation in C. / Una global C/C++ volatile se lee una vez
            //     por evaluacion en C.
            if (namespaceFunctionModel_)
                for (const auto& state : namespaceFunctionModel_->states())
                    if (state.isCppExternal && state.name == argument->text() &&
                        (state.isVolatile || state.cppAtomicStorage))
                        return false;
            return true;
        case ast::AstNodeKind::UnaryExpression:
            if (argument->tokenKind() == lexer::TokenKind::PlusPlus ||
                argument->tokenKind() == lexer::TokenKind::MinusMinus ||
                argument->tokenKind() == lexer::TokenKind::Star)
                return false;
            return macroArgumentSideEffectFree(argument->first());
        case ast::AstNodeKind::BinaryExpression:
            return macroArgumentSideEffectFree(argument->first()) && macroArgumentSideEffectFree(argument->second());
        case ast::AstNodeKind::ConditionalExpression:
            return macroArgumentSideEffectFree(argument->first()) && macroArgumentSideEffectFree(argument->second()) &&
                macroArgumentSideEffectFree(argument->third());
        case ast::AstNodeKind::CastExpression:
        case ast::AstNodeKind::MemberExpression:
            return macroArgumentSideEffectFree(argument->first());
        case ast::AstNodeKind::IndexExpression:
            if (!macroArgumentSideEffectFree(argument->first())) return false;
            for (const ast::AstNode* index : argument->children())
                if (!macroArgumentSideEffectFree(index)) return false;
            return true;
        default:
            return false;
    }
}

std::string ProjectCompilation::argumentSpelling(const ast::AstNode* argument) const
{
    if (argument == nullptr || !argument->range().isValid()) return {};
    // EN: T19-042. A type argument is written as its C type: `long` is `long long`, `byte` is `unsigned char`. A
    //     NativeSharp type with no C spelling (`char`, `string`) has none.
    // ES: T19-042. Un argumento de tipo se escribe como su tipo C: `long` es `long long`, `byte` es `unsigned char`.
    //     Un tipo NativeSharp sin escritura C (`char`, `string`) no tiene ninguna.
    if (argument->kind() == ast::AstNodeKind::TypeArgumentExpression)
    {
        std::string text;
        for (const auto& unit : units_)
        {
            if (!unit || !(unit->fileId() == argument->range().begin().fileId())) continue;
            for (const auto& token : unit->tokens())
            {
                if (token.range().begin().offset() < argument->range().begin().offset()) continue;
                if (token.range().end().offset() > argument->range().end().offset()) break;
                using lexer::TokenKind;
                std::string_view spelling;
                switch (token.kind())
                {
                case TokenKind::KwBool: spelling = "_Bool"; break;
                case TokenKind::KwSByte: spelling = "signed char"; break;
                case TokenKind::KwByte: spelling = "unsigned char"; break;
                case TokenKind::KwShort: spelling = "short"; break;
                case TokenKind::KwUShort: spelling = "unsigned short"; break;
                case TokenKind::KwInt: spelling = "int"; break;
                case TokenKind::KwUInt: spelling = "unsigned int"; break;
                case TokenKind::KwLong: case TokenKind::KwNInt: spelling = "long long"; break;
                case TokenKind::KwULong: case TokenKind::KwNUInt: spelling = "unsigned long long"; break;
                case TokenKind::KwFloat: spelling = "float"; break;
                case TokenKind::KwDouble: spelling = "double"; break;
                case TokenKind::KwVoid: spelling = "void"; break;
                case TokenKind::Star: spelling = "*"; break;
                default: return {};
                }
                text += spelling;
            }
            return text;
        }
        return {};
    }
    const auto range = argument->range();
    for (const auto& unit : units_)
    {
        if (!unit || !(unit->fileId() == range.begin().fileId())) continue;
        std::string text;
        std::uint32_t previousEnd = 0;
        for (const auto& token : unit->tokens())
        {
            const auto tokenRange = token.range();
            if (tokenRange.begin().offset() < range.begin().offset()) continue;
            if (tokenRange.end().offset() > range.end().offset()) break;
            if (!text.empty() && tokenRange.begin().offset() > previousEnd) text += ' ';
            text += std::string(token.lexeme());
            previousEnd = tokenRange.end().offset();
        }
        return text;
    }
    return {};
}

bool ProjectCompilation::deduceCppTemplateCalls(std::span<const CppTemplateDeductionRequest> requests)
{
    static const std::map<std::string, std::string, std::less<>> builtins{
        {"bool", "bool"}, {"sbyte", "signed char"}, {"byte", "unsigned char"}, {"short", "short"},
        {"ushort", "unsigned short"}, {"int", "int"}, {"uint", "unsigned int"}, {"long", "long long"},
        {"ulong", "unsigned long long"}, {"nint", "long long"}, {"nuint", "unsigned long long"},
        {"float", "float"}, {"double", "double"}};
    bool ok = true;
    std::set<const ast::AstNode*> seen;
    // EN: T19-003. The macro calls go first: in C++ the wrapper of one replaces the translation unit, and the
    //     deductions made after it stay in the new one.
    // ES: T19-003. Las llamadas a macros van primero: en C++ el envoltorio de una reemplaza la unidad, y las
    //     deducciones hechas despues quedan en la nueva.
    std::vector<CppTemplateDeductionRequest> ordered(requests.begin(), requests.end());
    std::stable_partition(ordered.begin(), ordered.end(),
                          [](const CppTemplateDeductionRequest& request) { return request.functionMacro; });
    for (const auto& request : ordered)
    {
        if (!seen.insert(request.syntax).second) continue;
        std::vector<interop::CppTemplateArgument> arguments;
        bool mapped = true;
        for (std::size_t index = 0; index < request.argumentTypeKeys.size(); ++index)
        {
            const auto& key = request.argumentTypeKeys[index];
            interop::CppTemplateArgument argument;
            // T19-040: the text of the argument, for `#` and `##`. / El texto del argumento, para `#` y `##`.
            if (request.functionMacro && request.syntax && index < request.syntax->children().size())
            {
                argument.sourceSpelling = argumentSpelling(request.syntax->children()[index]);
                argument.sideEffectFree = macroArgumentSideEffectFree(request.syntax->children()[index]);
            }
            if (key == "token:")
            {
                argument.token = true;
                arguments.push_back(std::move(argument));
                continue;
            }
            std::string_view base(key);
            if (base.starts_with("lvalue:")) { argument.lvalue = true; base.remove_prefix(7); }
            // C5-02: a C `signed char` value. / Un valor C `signed char`.
            if (base.starts_with("csigned:")) { argument.cSignedChar = true; base.remove_prefix(8); }
            while (base.ends_with('*')) { ++argument.pointerDepth; base.remove_suffix(1); }
            if (base.starts_with("builtin:"))
            {
                const auto found = builtins.find(base.substr(8));
                if (found == builtins.end()) { mapped = false; break; }
                argument.builtin = found->second;
            }
            else if (base.starts_with("type:") && nameLookup_)
            {
                const auto ordinal = nameLookup_->typeOrdinalForIdentity(base.substr(5));
                const auto* imported = ordinal ? nameLookup_->cppImportedType(*ordinal) : nullptr;
                if (!imported || !imported->clangUsr) { mapped = false; break; }
                argument.recordUsr = imported->clangUsr;
            }
            else { mapped = false; break; }
            arguments.push_back(std::move(argument));
        }
        bool deduced = false;
        std::vector<std::string> notes;
        // C2-06: the NativeSharp site of a macro call, for __FILE__ and __LINE__. / El sitio NativeSharp de una
        //     llamada a macro, para __FILE__ y __LINE__.
        interop::CppMacroCallSite site;
        if (request.functionMacro && request.syntax)
        {
            const auto begin = request.syntax->range().begin();
            site.file = std::string(sourceManager_.getPath(begin));
            site.line = sourceManager_.getPosition(begin).line;
        }
        if (mapped && request.projectOrdinal < projectGraph_->projects().size())
            for (const auto& group : cppProjectSessions_)
            {
                std::error_code ec;
                if (!std::filesystem::equivalent(group.projectFile,
                        projectGraph_->projects()[request.projectOrdinal].projectFile(), ec) || ec) continue;
                for (const auto& session : group.sessions)
                    for (const auto& name : request.qualifiedNames)
                        if (const auto result = !session ? std::nullopt
                                : request.functionMacro ? session->deduceFunctionMacroCall(name, arguments, site)
                                : session->deduceFunctionTemplateCall(name, arguments))
                        {
                            if (result->instantiated) deduced = true;
                            // T19-040: this call calls its own wrapper. / Esta llamada llama a su propio envoltorio.
                            if (result->instantiated && !result->cMacroWrapper.empty())
                            {
                                CppMacroCallWrapper wrapper{result->cMacroWrapper, {}};
                                for (std::size_t index = 0; index < arguments.size(); ++index)
                                    if (arguments[index].token) wrapper.tokenArguments.push_back(index);
                                cppMacroCallWrappers_[request.syntax] = std::move(wrapper);
                            }
                            else notes.insert(notes.end(), result->diagnostics.begin(), result->diagnostics.end());
                        }
            }
        if (deduced || (namespaceFunctionModel_ && namespaceFunctionModel_->referenceBinding(request.syntax)))
        {
            // T19-003: not deduced again in a later round. / No se deduce otra vez en otra ronda.
            cppDeducedTemplateCalls_.insert(request.syntax);
            continue;
        }
        if (!mapped) notes.push_back(request.functionMacro ? "an argument has no C type"
                                                           : "an argument has no C++ type that a template can deduce");
        // T19-003: a macro Clang cannot expand with these types. / Una macro que Clang no expande con estos tipos.
        diagnostics_.reportStructured(diagnostics::DiagnosticId::InvalidCall, diagnostics::DiagnosticSeverity::Error,
            request.range, request.functionMacro
                ? "Clang cannot expand the C/C++ function-like macro '" + request.qualifiedNames.front() +
                      "' with the argument types of this call"
                : "Clang cannot deduce the template arguments of the C++ function template '" +
                      request.qualifiedNames.front() + "' from this call", {}, std::move(notes));
        ok = false;
    }
    return ok;
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
    const auto bodiesDiagnosticsBegin = diagnostics_.diagnostics().size();
    const auto analyzeBodies = [&]() -> bool {
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
        // EN: Freeze (item 130a): previews constructor selection while the bodies are analyzed.
        // ES: Congelamiento (elemento 130a): anticipa la eleccion de constructores mientras se analizan
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
        return true;
    };
    bool bodiesAnalyzed = analyzeBodies();
    // EN: T13-005. Calls to C++ function templates recorded their argument types: Clang deduces the template
    //     arguments, the namespace functions are built again with the deduced specializations, and the bodies
    //     are analyzed again; the first analysis leaves no diagnostics.
    // ES: T13-005. Las llamadas a plantillas de funcion C++ registraron los tipos de sus argumentos: Clang
    //     deduce los argumentos de plantilla, las funciones de namespace se construyen otra vez con las
    //     especializaciones deducidas y los cuerpos se analizan otra vez; el primer analisis no deja diagnosticos.
    // EN: T19-003. Again while a round leaves calls to deduce, such as a call whose argument is a call deduced
    //     in the previous round.
    // ES: T19-003. Otra vez mientras una ronda deje llamadas por deducir, como una cuyo argumento es una llamada
    //     deducida en la ronda anterior.
    std::size_t roundBodiesBegin = bodiesDiagnosticsBegin;
    while (cppTemplateDeductionRound_ < kCppTemplateDeductionRounds && namespaceFunctionModel_)
    {
        std::vector<CppTemplateDeductionRequest> requests;
        for (auto& request : namespaceFunctionModel_->cppTemplateDeductions())
            if (!cppDeducedTemplateCalls_.contains(request.syntax)) requests.push_back(std::move(request));
        if (requests.empty()) break;
        {
            ++cppTemplateDeductionRound_;
            const std::vector<diagnostics::Diagnostic> previous = diagnostics_.diagnostics();
            const auto namespaceBegin = std::min(namespaceDiagnosticsBegin_, previous.size());
            const auto namespaceEnd = std::min(std::max(namespaceDiagnosticsEnd_, namespaceBegin), previous.size());
            const auto bodiesBegin = std::min(std::max(roundBodiesBegin, namespaceEnd), previous.size());
            const bool deduced = deduceCppTemplateCalls(requests);
            const std::vector<diagnostics::Diagnostic> rejections(
                diagnostics_.diagnostics().begin() + static_cast<std::ptrdiff_t>(previous.size()),
                diagnostics_.diagnostics().end());
            diagnostics_.clear();
            if (!deduced)
            {
                for (std::size_t index = 0; index < bodiesBegin; ++index) diagnostics_.append(previous[index]);
                for (const auto& rejection : rejections) diagnostics_.append(rejection);
                return false;
            }
            for (std::size_t index = 0; index < namespaceBegin; ++index) diagnostics_.append(previous[index]);
            loadError.clear();
            if (!buildNamespaceFunctions(loadError)) return false;
            for (std::size_t index = namespaceEnd; index < bodiesBegin; ++index) diagnostics_.append(previous[index]);
            roundBodiesBegin = diagnostics_.diagnostics().size();
            bodiesAnalyzed = analyzeBodies();
        }
    }
    if (!bodiesAnalyzed) return false;
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
    oopHirModel_->setCppVariadicFunctionPointerPlanProvider(cppVariadicFunctionPointerPlanProvider_);
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
        // EN: C5-07. The plan of a sysv_abi callback comes from a C typedef of the same pointer type: any one of them
        //     names the same canonical function type, so Clang classifies it once.
        // ES: C5-07. El plan de un callback sysv_abi viene de un typedef C del mismo tipo de puntero: cualquiera nombra
        //     el mismo tipo de funcion canonico, asi que Clang lo clasifica una vez.
        std::unordered_map<std::string, std::shared_ptr<const interop::abi_plan::Plan>> sysvPlans;
        if (!oopHirModel_->attachSysVCallbackPlans([&](std::string_view typeKey) {
                const std::string key(typeKey);
                if (const auto found = sysvPlans.find(key); found != sysvPlans.end()) return found->second;
                std::shared_ptr<const interop::abi_plan::Plan> plan;
                for (const auto& alias : cppImportedTypeAliases_)
                {
                    const auto& pointer = alias.declaration.underlyingType;
                    if (!cppFunctionPointerPlanProvider_ || pointer.kind != interop::CppTypeKind::Pointer ||
                        !pointer.element || !pointer.element->function ||
                        pointer.element->function->callingConvention != "sysv_abi") continue;
                    if ((plan = cppFunctionPointerPlanProvider_(alias, typeKey))) break;
                }
                sysvPlans.emplace(key, plan);
                return plan;
            }, loadError))
            return false;
        oopHirModel_->ownCppTemporaries(); // X2-05
        if (experimentalCppDiscardedLifetimeLoweringForTests_ || experimentalCppLocalLifetimeAnalysisForTests_)
        {
            using HirNode = ProjectOopHirNode;
            // The source position of a node, before the message, as for a HIR lowering error. / La posicion del
            //     nodo, antes del mensaje, como en un error de bajada a HIR.
            const auto located = [&](const HirNode* at, std::string message) {
                if (at == nullptr || at->syntax == nullptr || !at->syntax->range().isValid()) return message;
                const auto begin = at->syntax->range().begin();
                const std::string_view path = sourceManager_.getPath(begin);
                if (path.empty()) return message;
                const source::SourcePosition position = sourceManager_.getPosition(begin);
                return std::string(path) + ':' + std::to_string(position.line) + ':' + std::to_string(position.column) +
                    ": " + message;
            };
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
                const auto* importedValue = valueOrdinal ? nameLookup_->cppImportedType(*valueOrdinal) : nullptr;
                const bool cppValue = importedValue && importedValue->origin != CppImportedTypeOrigin::CppEnum;
                const bool guardedCppValue = cppValue && !nameLookup_->cppTrivialValueEligible(*valueOrdinal);
                // EN: Item 10.1. A local of a C++ type whose default constructor is not trivial
                //     (`ImVec2() : x(0), y(0) {}`) cannot be default-initialized as bytes: it is constructed
                //     explicitly (`V v = new V();`) or initialized from a value.
                // ES: Item 10.1. Un local de un tipo C++ cuyo constructor por defecto no es trivial no se puede
                //     inicializar por defecto como bytes: se construye explicitamente o se inicializa con un valor.
                if (cppValue && !guardedCppValue && importedValue->cppValueTraits &&
                    !importedValue->cppValueTraits->trivialDefaultConstructor &&
                    node->kind == ProjectOopHirNodeKind::LocalDeclaration && !node->referenceLocal &&
                    (node->children.empty() || !node->children[0] ||
                     node->children[0]->kind == ProjectOopHirNodeKind::DefaultValue))
                {
                    loadError = "C++ type '" + importedValue->identity.fullyQualifiedName() + "' has a non-trivial "
                        "default constructor; initialize the local with 'new " +
                        importedValue->identity.fullyQualifiedName() + "()' or with a value";
                    return false;
                }
                if (experimentalCppLocalLifetimeAnalysisForTests_ && guardedCppValue &&
                    node->kind == ProjectOopHirNodeKind::LocalDeclaration && !node->referenceLocal)
                {
                    // X2-05: or the call of a NativeSharp function that constructs it there. / O la llamada de una
                    //     funcion NativeSharp que lo construye alli.
                    // X2-05: also an instance method or a property getter. / Tambien un metodo de instancia o un getter.
                    const bool constructedCall = node->children.size() == 1 && node->children[0] &&
                        (node->children[0]->kind == ProjectOopHirNodeKind::StaticCall ||
                         node->children[0]->kind == ProjectOopHirNodeKind::InstanceCall) &&
                        node->children[0]->typeKey == node->typeKey && !node->children[0]->referenceResult;
                    if (node->staticLocal || !node->localOrdinal || !node->cleanupRegionOrdinal ||
                        !node->automaticDestruction || node->children.size() != 1 || !node->children[0] ||
                        (!constructedCall &&
                         (node->children[0]->kind != ProjectOopHirNodeKind::ValueConstruction ||
                          node->children[0]->targetTypeOrdinal != valueOrdinal)))
                    {
                        loadError = located(node, "experimental C++ local requires direct automatic construction and cleanup");
                        return false;
                    }
                }
                // X2-05: a by-value C++ parameter of a NativeSharp function. / Un parametro C++ por valor de una
                //     funcion NativeSharp.
                else if (experimentalCppLocalLifetimeAnalysisForTests_ && guardedCppValue &&
                    node->kind == ProjectOopHirNodeKind::OwnedParameterCleanup)
                {
                    loadError = located(node, "a NativeSharp function cannot take C++ type '" +
                        importedValue->identity.fullyQualifiedName() + "' by value (C++ would construct it in the "
                        "parameter and destroy it in the callee); take it by 'in' (const T&) or 'ref' (T&)");
                    return false;
                }
                else if (experimentalCppLocalLifetimeAnalysisForTests_ && guardedCppValue &&
                    node->kind != ProjectOopHirNodeKind::ValueConstruction &&
                    node->kind != ProjectOopHirNodeKind::LocalAddress &&
                    // X2-06: the address of a C++ field. / La direccion de un campo C++.
                    node->kind != ProjectOopHirNodeKind::FieldAddress &&
                    // X2-05: the hidden owner of a receiver temporary, built in place. / El dueno oculto de un
                    //     temporal receptor, construido en su lugar.
                    // X2-05: a NativeSharp function's constructed result, into a local or returned again. / El
                    //     resultado construido de una funcion NativeSharp, en un local o devuelto de nuevo.
                    !((node->kind == ProjectOopHirNodeKind::StaticCall ||
                       node->kind == ProjectOopHirNodeKind::InstanceCall) && parent &&
                      ((parent->kind == ProjectOopHirNodeKind::LocalDeclaration && !parent->referenceLocal &&
                        !parent->staticLocal) || parent->kind == ProjectOopHirNodeKind::Return ||
                       (parent->kind == ProjectOopHirNodeKind::OwnedTemporaryBorrow && parent->referenceResult &&
                        parent->automaticDestruction))) &&
                    // X2-06: the assignment of a field initializer that builds a C++ field. / La asignacion de un
                    //     inicializador que construye un campo C++.
                    !(node->kind == ProjectOopHirNodeKind::Assignment && node->children.size() == 2 && node->children[0] &&
                      node->children[0]->kind == ProjectOopHirNodeKind::FieldAddress && node->children[1] &&
                      node->children[1]->kind == ProjectOopHirNodeKind::ValueConstruction) &&
                    !(node->kind == ProjectOopHirNodeKind::OwnedTemporaryBorrow && node->referenceResult &&
                      (node->automaticDestruction || node->cppCalleeDestroyed) && node->localOrdinal &&
                      node->cleanupRegionOrdinal &&
                      node->children.size() == 1 && node->children[0] &&
                      (node->children[0]->kind == ProjectOopHirNodeKind::ValueConstruction ||
                       node->children[0]->kind == ProjectOopHirNodeKind::StaticCall ||
                       node->children[0]->kind == ProjectOopHirNodeKind::InstanceCall)) &&
                    // X2-08: a call whose result is an address (`T& operator+=`, `T& operator++`) does not copy
                    //     the object; using it as a value adds a PointerLoad, which is checked. / Una llamada
                    //     cuyo resultado es una direccion no copia el objeto; usarla como valor agrega PointerLoad.
                    !node->referenceResult &&
                    !(node->kind == ProjectOopHirNodeKind::LocalDeclaration && node->referenceLocal))
                {
                    loadError = located(node, std::string("experimental C++ local cannot copy or transfer a C++ value (") +
                        projectOopHirNodeKindName(node->kind) + " of '" + node->typeKey + "' under " +
                        (parent ? projectOopHirNodeKindName(parent->kind) : "the body") + ")");
                    return false;
                }
                const bool construction = node->kind == ProjectOopHirNodeKind::ValueConstruction ||
                    node->kind == ProjectOopHirNodeKind::ObjectConstruction ||
                    node->kind == ProjectOopHirNodeKind::PlacementConstruction;
                if (!construction || !node->targetTypeOrdinal ||
                    !nameLookup_->cppImportedType(*node->targetTypeOrdinal)) continue;
                // X1-05: a trivial C++ record materialized as a temporary needs no destructor; its bytes are its value.
                //     / Un record C++ trivial materializado como temporal no necesita destructor.
                if (parent && parent->kind == ProjectOopHirNodeKind::TemporaryAddress &&
                    nameLookup_->cppTrivialValueEligible(*node->targetTypeOrdinal)) continue;
                const bool discarded = experimentalCppDiscardedLifetimeLoweringForTests_ && parent &&
                    parent->kind == ProjectOopHirNodeKind::ExpressionStatement && parent->children.size() == 1;
                // X2-05: `return construction;` builds the result in the caller's storage. / Construye el resultado en
                //     el almacenamiento del llamador.
                const bool returned = experimentalCppLocalLifetimeAnalysisForTests_ && parent &&
                    parent->kind == ProjectOopHirNodeKind::Return && parent->children.size() == 1 &&
                    parent->children[0].get() == node;
                // X2-06: a field initializer builds a C++ field in place. / Un inicializador de campo construye un
                //     campo C++ en su lugar.
                const bool fieldInitializer = parent && parent->kind == ProjectOopHirNodeKind::Assignment &&
                    parent->children.size() == 2 && parent->children[0] &&
                    parent->children[0]->kind == ProjectOopHirNodeKind::FieldAddress && parent->children[1].get() == node;
                if (fieldInitializer) continue;
                const bool local = returned || (experimentalCppLocalLifetimeAnalysisForTests_ && parent &&
                    ((parent->kind == ProjectOopHirNodeKind::LocalDeclaration && !parent->referenceLocal &&
                      !parent->staticLocal) ||
                     (parent->kind == ProjectOopHirNodeKind::OwnedTemporaryBorrow && parent->referenceResult)) &&
                    parent->children.size() == 1 && parent->children[0].get() == node &&
                    parent->typeKey == node->typeKey && parent->localOrdinal && parent->cleanupRegionOrdinal &&
                    (parent->automaticDestruction || parent->cppCalleeDestroyed));
                if (node->kind != ProjectOopHirNodeKind::ValueConstruction || (!discarded && !local) ||
                    !node->memberOrdinal || *node->memberOrdinal >= memberModel_->members().size())
                {
                    loadError = located(node, std::string("experimental C++ lifetime requires direct local or discarded construction (") +
                        "under " + (parent ? projectOopHirNodeKindName(parent->kind) : "the body") + ")");
                    return false;
                }
                const auto& constructor = memberModel_->members()[*node->memberOrdinal];
                const auto destructors = memberModel_->findMembers(*node->targetTypeOrdinal, ".dtor");
                // EN: T11-006. The member model admitted only scalars, pointers, enums and trivial records by
                //     value and ref/in references; a by-value C++ argument here must still be trivial.
                // ES: T11-006. El modelo de miembros solo admitio escalares, punteros, enums y records triviales
                //     por valor y referencias ref/in; un argumento C++ por valor aqui debe seguir siendo trivial.
                const bool scalarArguments = constructor.parameters.size() == node->argumentCount &&
                    std::all_of(constructor.parameters.begin(), constructor.parameters.end(), [&](const auto& parameter) {
                        if (parameter.passKind == ParameterPassKind::Ref || parameter.passKind == ParameterPassKind::In)
                            return parameter.typeKey.ends_with('*');
                        if (parameter.passKind != ParameterPassKind::Value) return false;
                        std::string_view type = parameter.typeKey;
                        if (type.ends_with('*') || type.starts_with("builtin:")) return type != "builtin:void";
                        const auto ordinal = type.starts_with("type:")
                            ? nameLookup_->typeOrdinalForIdentity(type.substr(5)) : std::nullopt;
                        const auto* value = ordinal ? nameLookup_->cppImportedType(*ordinal) : nullptr;
                        // X4-05: a NativeSharp struct mirrored for a C++ class template is bytes too. / Un struct
                        //     NativeSharp reflejado para una plantilla de clase C++ tambien son bytes.
                        if (!value && type.starts_with("type:") && nameLookup_->isCppNativeMirrorType(type.substr(5)))
                            return true;
                        return value && (value->origin == CppImportedTypeOrigin::CppEnum ||
                                         value->origin == CppImportedTypeOrigin::CEnum ||
                                         nameLookup_->cppTrivialValueEligible(*ordinal));
                    });
                if (!constructor.isCppExternal || constructor.kind != ClassMemberKind::Constructor ||
                    !scalarArguments || destructors.size() != 1 ||
                    destructors[0] >= memberModel_->members().size() ||
                    !memberModel_->members()[destructors[0]].isCppExternal ||
                    memberModel_->members()[destructors[0]].kind != ClassMemberKind::Destructor)
                { loadError = "experimental C++ lifetime requires a supported constructor and unique destructor"; return false; }
                // T11-007: a local is destroyed where it ends, so its destructor must be accessible there.
                if (local && memberModel_->members()[destructors[0]].accessibility != ClassMemberAccessibility::Public)
                { loadError = "C++ local requires an accessible destructor"; return false; }
                // EN: T18-011. A constructor may throw: the local is built in place and its drop flag is set only
                //     after the constructor returns, so an exception leaves it unconstructed and its cleanup skips
                //     the destructor, as in C++. Its destructor must not throw.
                // ES: T18-011. Un constructor puede lanzar: el local se construye en su lugar y su indicador de
                //     destruccion se activa solo cuando el constructor vuelve, asi que una excepcion lo deja sin
                //     construir y su limpieza omite el destructor, como en C++. Su destructor no debe lanzar.
                if (local && cppNoexceptLocalLifetimeOnly_ &&
                    !memberModel_->members()[destructors[0]].cppLifetimeNonThrowing)
                { loadError = "C++ local lifetime currently requires a non-throwing destructor"; return false; }
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
    // EN: The MIR cache stays disconnected. Its compilation identity embeds the
    //     complete textual HIR dump plus six semantic-model dumps, so every edit invalidates every
    //     slot, while each build still pays those dumps, one SHA-256 per callable, one lookup per
    //     callable and, on a miss, two write-through file publications per callable. Unchanged
    //     inputs are already served by the compilation bundle cache before the frontend runs.
    // ES: La caché MIR sigue desconectada. Su identidad de compilación incluye
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
    const auto pathKey = [](const std::filesystem::path& path) {
        auto key = utf8Path(std::filesystem::absolute(path).lexically_normal());
        std::transform(key.begin(), key.end(), key.begin(),
                       [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
        return key;
    };
    for (const PrecompiledSyntaxModule& module : precompiledModules_)
    {
        // EN: The modules named by the external type keys ("type:<module>::..."). When the graph provides one as a
        //     source project (for example NativeSharp.Core referenced by source), the synthetic project of this
        //     metadata sees it, so a published template that imports one of its namespaces still binds. The owner
        //     project is left out: it already sees this metadata.
        // ES: Los modulos que nombran las claves de tipo externas ("type:<modulo>::..."). Si el grafo aporta uno
        //     como proyecto fuente (por ejemplo NativeSharp.Core referenciado por fuente), el proyecto sintetico de
        //     estos metadatos lo ve, de modo que una plantilla publicada que importa uno de sus namespaces sigue
        //     enlazando. Se excluye el proyecto propietario: ya ve estos metadatos.
        std::set<std::string, std::less<>> namedModules;
        for (const std::string& typeKey : module.externalTypeKeys)
        {
            for (std::size_t start = typeKey.find("type:"); start != std::string::npos;
                 start = typeKey.find("type:", start + 5U))
            {
                const std::size_t separator = typeKey.find("::", start + 5U);
                if (separator == std::string::npos) break;
                const std::string_view name(typeKey.data() + start + 5U, separator - start - 5U);
                if (!name.empty() && name.find_first_of("<>,() :") == std::string_view::npos)
                    namedModules.emplace(name);
            }
        }
        std::vector<std::filesystem::path> sourceDependencies;
        for (const compilation::Project& project : projectGraph_->projects())
        {
            if (project.moduleName() == module.moduleName || !namedModules.contains(project.moduleName()) ||
                pathKey(project.projectFile()) == pathKey(module.ownerProjectFile)) continue;
            sourceDependencies.push_back(project.projectFile());
        }
        descriptors.push_back({
            module.ownerProjectFile,
            module.metadataPath,
            module.moduleName,
            std::move(sourceDependencies)});
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
        false,
        SyntaxUnitOrigin::Source));
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
    if (isPrecompiledTemplateFrame(module.templateSourceText))
    {
        std::vector<std::string> sources;
        if (!decodePrecompiledTemplateSources(module.templateSourceText, sources, loadError)) return false;
        for (std::size_t index = 0; index < sources.size(); ++index)
        {
            const std::string suffix = ".public-template-" + std::to_string(index) + ".ns";
            if (!materializePrecompiledUnit(module, projectOrdinal, index + 1U, suffix,
                    sources[index], {}, false, "templates", loadError)) return false;
        }
        return true;
    }
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
        isPrecompiled,
        SyntaxUnitOrigin::PrecompiledMetadata));
    precompiledSyntaxCacheKeysByUnit_.resize(units_.size());
    precompiledSyntaxCacheHitsByUnit_.resize(units_.size());
    std::string& syntaxCacheKey = precompiledSyntaxCacheKeysByUnit_.back();
    if (!experimentalCppLifetimeMethodProvider_ && !experimentalCppInstanceMethodProvider_ && !experimentalCppInstanceFieldProvider_ && !experimentalCppStaticMethodProvider_ && !experimentalCppStaticFieldProvider_)
        syntaxCacheKey = precompiledSyntaxCacheKey(module.productCacheKey, sourceOrdinal, experimentalCppImportedTypes_);
    const bool hasPublishedSyntax = !experimentalCppLifetimeMethodProvider_ &&
        !experimentalCppInstanceMethodProvider_ && !experimentalCppInstanceFieldProvider_ &&
        !experimentalCppStaticMethodProvider_ && !experimentalCppStaticFieldProvider_ &&
        experimentalCppImportedTypes_.empty() && !module.productSidecarPath.empty() && !publishedSyntaxProduct.empty();
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
    // T13-006: C++ template uses rewrite the tokens with session state. / Los usos de plantillas C++ dependen de la sesion.
    if (!cppProjectSessions_.empty()) return true;
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
// EN: T13-006. The imported C/C++ types and aliases as the sessions declare them now; run again after a
//     specialization written in NativeSharp is instantiated.
// ES: T13-006. Los tipos y alias C/C++ importados como los declaran ahora las sesiones; se repite despues de
//     instanciar una especializacion escrita en NativeSharp.
bool ProjectCompilation::refreshCppImportedTypes(std::string& loadError)
{
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
    return true;
}

namespace
{
// EN: T13-006. A C++ template specialization written in NativeSharp, `A.Box<int>` or `A.twice<int>`, found in
//     the tokens before parsing: a qualified name of a template declared by a C/C++ library of the project,
//     followed by an argument list of builtin types, imported C++ types, nested specializations, pointers and
//     integral or bool constants. Sema of the session instantiates it and the tokens from the template name to
//     the closing `>` become one identifier naming the specialization, as the importer names it.
// ES: T13-006. Una especializacion de plantilla C++ escrita en NativeSharp, `A.Box<int>` o `A.twice<int>`,
//     encontrada en los tokens antes de analizar: un nombre calificado de una plantilla que declara una
//     biblioteca C/C++ del proyecto, seguido de una lista de argumentos con tipos basicos, tipos C++
//     importados, especializaciones anidadas, punteros y constantes enteras o bool. Sema de la sesion la
//     instancia, y los tokens desde el nombre de la plantilla hasta el `>` de cierre pasan a ser un
//     identificador que nombra la especializacion, como la nombra el importador.
// EN: T13-007, T13-008, T13-009. The NativeSharp structs and classes a unit declares, read from its tokens
//     before parsing: a struct whose instance fields are all builtin types, pointers or other such structs,
//     with no destructor and no property, is trivial and can be a C++ template argument; a struct with a
//     destructor, a string or another owning field, or a property is not; a class is a reference type.
// ES: T13-007, T13-008, T13-009. Los structs y clases NativeSharp que declara una unidad, leidos de sus tokens
//     antes de analizar: un struct cuyos campos de instancia son tipos basicos, punteros u otros structs asi,
//     sin destructor ni propiedades, es trivial y puede ser argumento de plantilla C++; un struct con
//     destructor, una cadena u otro campo con propiedad, o una propiedad no lo es; una clase es de referencia.
struct NativeTypeShape
{
    std::vector<std::string> namespaceSegments;
    std::string name;
    bool isClass = false;
    std::string unsupported;
    struct Field { std::string name; std::string builtin; std::string structure; unsigned pointerDepth = 0; };
    std::vector<Field> fields;
};

std::string_view nativeBuiltinCppName(lexer::TokenKind kind)
{
    switch (kind)
    {
    case lexer::TokenKind::KwBool: return "bool";
    case lexer::TokenKind::KwSByte: return "signed char";
    case lexer::TokenKind::KwByte: return "unsigned char";
    case lexer::TokenKind::KwShort: return "short";
    case lexer::TokenKind::KwUShort: return "unsigned short";
    case lexer::TokenKind::KwInt: return "int";
    case lexer::TokenKind::KwUInt: return "unsigned int";
    case lexer::TokenKind::KwLong: case lexer::TokenKind::KwNInt: return "long long";
    case lexer::TokenKind::KwULong: case lexer::TokenKind::KwNUInt: return "unsigned long long";
    case lexer::TokenKind::KwFloat: return "float";
    case lexer::TokenKind::KwDouble: return "double";
    case lexer::TokenKind::KwVoid: return "void";
    default: return {};
    }
}

void scanNativeTypes(std::span<const lexer::Token> tokens, std::map<std::string, NativeTypeShape>& shapes)
{
    using K = lexer::TokenKind;
    const auto kindAt = [&](std::size_t index) { return index < tokens.size() ? tokens[index].kind() : K::EndOfFile; };
    const auto skipBalanced = [&](std::size_t index, K open, K close) {
        int depth = 0;
        for (; index < tokens.size(); ++index)
        {
            if (tokens[index].kind() == open) ++depth;
            else if (tokens[index].kind() == close && --depth == 0) return index + 1;
        }
        return index;
    };
    std::vector<std::string> fileNamespace;
    std::vector<std::pair<int, std::size_t>> blocks; // brace depth at open, segments pushed
    std::vector<std::string> current;
    int depth = 0;
    for (std::size_t index = 0; index < tokens.size(); ++index)
    {
        const auto kind = tokens[index].kind();
        if (kind == K::LeftBrace) { ++depth; continue; }
        if (kind == K::RightBrace)
        {
            --depth;
            if (!blocks.empty() && blocks.back().first == depth)
            {
                current.resize(current.size() - blocks.back().second);
                blocks.pop_back();
            }
            continue;
        }
        if (kind == K::KwNamespace)
        {
            std::vector<std::string> segments;
            std::size_t next = index + 1;
            while (kindAt(next) == K::Identifier)
            {
                segments.emplace_back(tokens[next].lexeme());
                if (kindAt(next + 1) != K::Dot) { ++next; break; }
                next += 2;
            }
            if (kindAt(next) == K::LeftBrace)
            {
                blocks.emplace_back(depth, segments.size());
                current.insert(current.end(), segments.begin(), segments.end());
                ++depth;
                index = next;
            }
            else if (kindAt(next) == K::Semicolon)
            {
                current.insert(current.end(), segments.begin(), segments.end());
                index = next;
            }
            continue;
        }
        if ((kind != K::KwStruct && kind != K::KwClass) || kindAt(index + 1) != K::Identifier) continue;
        NativeTypeShape shape;
        shape.namespaceSegments = current;
        shape.name = std::string(tokens[index + 1].lexeme());
        shape.isClass = kind == K::KwClass;
        std::size_t position = index + 2;
        if (kindAt(position) == K::Less) shape.unsupported = "it is generic";
        while (position < tokens.size() && kindAt(position) != K::LeftBrace && kindAt(position) != K::Semicolon)
            ++position;
        if (kindAt(position) != K::LeftBrace) continue;
        const std::size_t end = skipBalanced(position, K::LeftBrace, K::RightBrace);
        // Members at depth 1 of the body. / Miembros en el nivel 1 del cuerpo.
        for (std::size_t member = position + 1; !shape.isClass && member + 1 < end;)
        {
            bool isStatic = false;
            while (kindAt(member) == K::KwPublic || kindAt(member) == K::KwPrivate || kindAt(member) == K::KwInternal ||
                   kindAt(member) == K::KwProtected || kindAt(member) == K::KwReadonly || kindAt(member) == K::KwStatic ||
                   kindAt(member) == K::KwConst || kindAt(member) == K::KwInline)
            {
                isStatic = isStatic || kindAt(member) == K::KwStatic || kindAt(member) == K::KwConst;
                ++member;
            }
            if (member + 1 >= end) break;
            if (kindAt(member) == K::Tilde)
            {
                if (shape.unsupported.empty()) shape.unsupported = "it has a destructor";
                while (member < end && kindAt(member) != K::LeftBrace && kindAt(member) != K::Semicolon) ++member;
                member = kindAt(member) == K::LeftBrace ? skipBalanced(member, K::LeftBrace, K::RightBrace) : member + 1;
                continue;
            }
            if (kindAt(member) == K::KwStruct || kindAt(member) == K::KwClass || kindAt(member) == K::KwEnum ||
                kindAt(member) == K::KwInterface || kindAt(member) == K::KwDelegate)
            {
                if (shape.unsupported.empty()) shape.unsupported = "it declares a nested type";
                while (member < end && kindAt(member) != K::LeftBrace) ++member;
                member = skipBalanced(member, K::LeftBrace, K::RightBrace);
                continue;
            }
            // Type. / Tipo.
            NativeTypeShape::Field field;
            bool knownType = true;
            if (const auto builtin = nativeBuiltinCppName(kindAt(member)); !builtin.empty())
            {
                field.builtin = std::string(builtin);
                ++member;
            }
            else if (kindAt(member) == K::Identifier)
            {
                field.structure = std::string(tokens[member].lexeme());
                ++member;
                while (kindAt(member) == K::Dot && kindAt(member + 1) == K::Identifier)
                {
                    field.structure += "." + std::string(tokens[member + 1].lexeme());
                    member += 2;
                }
            }
            else knownType = false;
            while (kindAt(member) == K::Star) { ++field.pointerDepth; ++member; }
            if (knownType && kindAt(member) == K::LeftParen)
            {
                // A constructor. / Un constructor.
                member = skipBalanced(member, K::LeftParen, K::RightParen);
                if (kindAt(member) == K::Colon)
                    while (member < end && kindAt(member) != K::LeftBrace) ++member;
                member = kindAt(member) == K::LeftBrace ? skipBalanced(member, K::LeftBrace, K::RightBrace) : member + 1;
                continue;
            }
            if (!knownType || kindAt(member) != K::Identifier)
            {
                if (shape.unsupported.empty())
                    shape.unsupported = kindAt(member - 1) == K::KwString || kindAt(member) == K::KwString ?
                        "it has a string field, which owns its storage" : "it has a member NSC cannot read";
                while (member < end && kindAt(member) != K::Semicolon && kindAt(member) != K::LeftBrace) ++member;
                member = kindAt(member) == K::LeftBrace ? skipBalanced(member, K::LeftBrace, K::RightBrace) : member + 1;
                continue;
            }
            field.name = std::string(tokens[member].lexeme());
            ++member;
            if (kindAt(member) == K::LeftParen || kindAt(member) == K::Less)
            {
                // A method: its body does not change the layout. / Un metodo: su cuerpo no cambia el layout.
                member = kindAt(member) == K::Less ? skipBalanced(member, K::Less, K::Greater) : member;
                member = skipBalanced(member, K::LeftParen, K::RightParen);
                while (member < end && kindAt(member) != K::LeftBrace && kindAt(member) != K::Semicolon &&
                       kindAt(member) != K::FatArrow) ++member;
                if (kindAt(member) == K::FatArrow)
                    while (member < end && kindAt(member) != K::Semicolon) ++member;
                member = kindAt(member) == K::LeftBrace ? skipBalanced(member, K::LeftBrace, K::RightBrace) : member + 1;
                continue;
            }
            if (kindAt(member) == K::LeftBrace || kindAt(member) == K::FatArrow)
            {
                if (shape.unsupported.empty()) shape.unsupported = "it has a property";
                if (kindAt(member) == K::FatArrow)
                    while (member < end && kindAt(member) != K::Semicolon) ++member;
                member = kindAt(member) == K::LeftBrace ? skipBalanced(member, K::LeftBrace, K::RightBrace) : member + 1;
                continue;
            }
            // A field, possibly with an initializer. / Un campo, quiza con inicializador.
            while (member < end && kindAt(member) != K::Semicolon) ++member;
            ++member;
            if (!isStatic) shape.fields.push_back(std::move(field));
        }
        std::string key;
        for (const auto& segment : shape.namespaceSegments) key += segment + ".";
        key += shape.name;
        shapes.emplace(std::move(key), std::move(shape));
        index = end - 1;
        // The closing brace of the type is consumed; depth is unchanged. / La llave de cierre ya se consumio.
    }
}

class CppTemplateUseRewriter final
{
public:
    struct Position
    {
        std::size_t index = 0;
        // The token at index is `>>` and its first `>` closed an inner list. / Su primer `>` cerro una lista interna.
        bool half = false;
    };
    struct Use
    {
        std::size_t nameIndex = 0;
        std::size_t closeIndex = 0;
        Position next;
        std::string name;
        std::optional<std::string> usr;
        bool failed = false;
        // T19-005: the type an alias template names. / El tipo que nombra una plantilla alias.
        std::optional<interop::CppTemplateAliasTarget> alias;
    };
    struct Specialization
    {
        std::string name;
        std::optional<std::string> usr;
        std::optional<interop::CppTemplateAliasTarget> alias;
    };
    using Instantiate = std::function<std::optional<Specialization>(
        const std::string& qualifiedName, bool function, const std::vector<interop::CppTemplateArgument>& arguments,
        SourceRange range)>;
    using TypeUsr = std::function<std::optional<std::string>(const std::string& dottedName)>;
    // T13-007: a NativeSharp struct by name, or why it cannot be an argument. / Un struct NativeSharp, o por que no.
    using NativeStruct = std::function<std::optional<std::variant<std::shared_ptr<const interop::CppNativeStruct>,
        std::string>>(const std::string& dottedName)>;
    using Reject = std::function<void(SourceRange, std::string)>;

    CppTemplateUseRewriter(std::span<const lexer::Token> tokens, const std::map<std::string, bool>& templates,
                           Instantiate instantiate, TypeUsr typeUsr, NativeStruct nativeStruct = {}, Reject reject = {})
        : tokens_(tokens), templates_(templates), instantiate_(std::move(instantiate)), typeUsr_(std::move(typeUsr)),
          nativeStruct_(std::move(nativeStruct)), reject_(std::move(reject)) {}

    // A specialization use whose qualified name starts at index, or nullopt when the tokens are not one.
    std::optional<Use> match(std::size_t index, unsigned depth = 0) const
    {
        if (depth > 32 || index >= tokens_.size() || tokens_[index].kind() != lexer::TokenKind::Identifier)
            return std::nullopt;
        std::string path(tokens_[index].lexeme());
        std::size_t last = index;
        std::size_t position = index + 1;
        while (position + 1 < tokens_.size() && tokens_[position].kind() == lexer::TokenKind::Dot &&
               tokens_[position + 1].kind() == lexer::TokenKind::Identifier)
        {
            path += '.';
            path += tokens_[position + 1].lexeme();
            last = position + 1;
            position += 2;
        }
        if (position >= tokens_.size() || tokens_[position].kind() != lexer::TokenKind::Less) return std::nullopt;
        const auto found = templates_.find(path);
        if (found == templates_.end()) return std::nullopt;
        std::vector<interop::CppTemplateArgument> arguments;
        Position cursor{position + 1, false};
        std::optional<Position> closed;
        std::size_t closeIndex = 0;
        if (cursor.index < tokens_.size() && tokens_[cursor.index].kind() == lexer::TokenKind::Greater)
        {
            closeIndex = cursor.index;
            closed = Position{cursor.index + 1, false};
        }
        while (!closed)
        {
            auto argument = parseArgument(cursor, depth);
            if (!argument) return std::nullopt;
            if (argument->failed) { Use use; use.failed = true; return use; }
            arguments.push_back(std::move(argument->value));
            cursor = argument->next;
            if (cursor.index >= tokens_.size()) return std::nullopt;
            const auto kind = tokens_[cursor.index].kind();
            if (cursor.half) { closeIndex = cursor.index; closed = Position{cursor.index + 1, false}; }
            else if (kind == lexer::TokenKind::Greater) { closeIndex = cursor.index; closed = Position{cursor.index + 1, false}; }
            else if (kind == lexer::TokenKind::GreaterGreater) { closeIndex = cursor.index; closed = Position{cursor.index, true}; }
            else if (kind == lexer::TokenKind::Comma) cursor = Position{cursor.index + 1, false};
            else return std::nullopt;
        }
        std::string qualified;
        for (std::size_t start = 0; start <= path.size();)
        {
            const auto end = path.find('.', start);
            if (!qualified.empty()) qualified += "::";
            qualified += path.substr(start, end == std::string::npos ? std::string::npos : end - start);
            if (end == std::string::npos) break;
            start = end + 1;
        }
        Use use;
        use.nameIndex = last;
        use.closeIndex = closeIndex;
        use.next = *closed;
        const SourceRange range(tokens_[last].range().begin(), tokens_[closeIndex].range().end());
        const auto specialization = instantiate_(qualified, found->second, arguments, range);
        if (!specialization) use.failed = true;
        else { use.name = specialization->name; use.usr = specialization->usr; use.alias = specialization->alias; }
        return use;
    }

private:
    struct Argument
    {
        interop::CppTemplateArgument value;
        Position next;
        bool failed = false;
    };

    std::optional<Argument> parseArgument(Position cursor, unsigned depth) const
    {
        if (cursor.half || cursor.index >= tokens_.size()) return std::nullopt;
        Argument result;
        auto& value = result.value;
        std::size_t index = cursor.index;
        const auto kind = tokens_[index].kind();
        const auto builtin = builtinName(kind);
        if (!builtin.empty())
        {
            value.kind = interop::CppTemplateArgument::Kind::Type;
            value.builtin = std::string(builtin);
            ++index;
        }
        else if (kind == lexer::TokenKind::KwTrue || kind == lexer::TokenKind::KwFalse)
        {
            value.kind = interop::CppTemplateArgument::Kind::Boolean;
            value.boolean = kind == lexer::TokenKind::KwTrue;
            result.next = Position{index + 1, false};
            return result;
        }
        else if (kind == lexer::TokenKind::IntegerLiteral ||
                 (kind == lexer::TokenKind::Minus && index + 1 < tokens_.size() &&
                  tokens_[index + 1].kind() == lexer::TokenKind::IntegerLiteral))
        {
            const bool negative = kind == lexer::TokenKind::Minus;
            const auto* number = std::get_if<std::uint64_t>(&tokens_[index + (negative ? 1 : 0)].value());
            // Item 13.6: a positive literal above INT64_MAX is an unsigned argument (`big<18446744073709551615ul>`).
            //     / Un literal positivo mayor que INT64_MAX es un argumento sin signo.
            if (!number || (negative && *number > (std::uint64_t(1) << 63))) return std::nullopt;
            value.kind = interop::CppTemplateArgument::Kind::Integral;
            value.integralUnsigned = !negative && *number > std::uint64_t(INT64_MAX);
            value.integral = negative ? static_cast<std::int64_t>(std::uint64_t(0) - *number)
                                      : static_cast<std::int64_t>(*number);
            result.next = Position{index + (negative ? 2 : 1), false};
            return result;
        }
        else if (kind == lexer::TokenKind::Identifier)
        {
            value.kind = interop::CppTemplateArgument::Kind::Type;
            if (const auto nested = match(index, depth + 1))
            {
                // A function template is not a type argument. / Una plantilla de funcion no es un tipo.
                // T19-005: an alias template that names a builtin scalar. / Una alias que nombra un escalar.
                if (!nested->failed && nested->alias && !nested->alias->builtin.empty() &&
                    !templates_.at(nestedPath(index, nested->nameIndex)))
                {
                    value.builtin = nested->alias->builtin;
                    value.pointerDepth = nested->alias->pointerDepth;
                }
                else if (nested->failed || !nested->usr || templates_.at(nestedPath(index, nested->nameIndex)))
                { result.failed = true; return result; }
                else value.recordUsr = nested->usr;
                if (nested->alias) value.pointerDepth = nested->alias->pointerDepth;
                if (nested->next.half) { result.next = nested->next; return result; }
                index = nested->next.index;
            }
            else
            {
                const std::size_t start = index;
                std::string path(tokens_[index].lexeme());
                ++index;
                while (index + 1 < tokens_.size() && tokens_[index].kind() == lexer::TokenKind::Dot &&
                       tokens_[index + 1].kind() == lexer::TokenKind::Identifier)
                {
                    path += '.';
                    path += tokens_[index + 1].lexeme();
                    index += 2;
                }
                // T13-013: a class template named without arguments. / Una plantilla de clase sin argumentos.
                if (const auto named = templates_.find(path); named != templates_.end() && !named->second &&
                    (index >= tokens_.size() || tokens_[index].kind() != lexer::TokenKind::Less))
                {
                    value.kind = interop::CppTemplateArgument::Kind::Template;
                    for (const char c : path) value.templateName += c == '.' ? std::string("::") : std::string(1, c);
                    result.next = Position{index, false};
                    return result;
                }
                const auto usr = typeUsr_(path);
                if (usr) value.recordUsr = *usr;
                else
                {
                    // T13-007: a NativeSharp struct. / Un struct NativeSharp.
                    const auto native = nativeStruct_ ? nativeStruct_(path) : std::nullopt;
                    if (!native) return std::nullopt;
                    if (const auto* reason = std::get_if<std::string>(&*native))
                    {
                        if (reject_)
                            reject_(SourceRange(tokens_[start].range().begin(), tokens_[index - 1].range().end()), *reason);
                        result.failed = true;
                        return result;
                    }
                    value.kind = interop::CppTemplateArgument::Kind::NativeStruct;
                    value.nativeStruct = std::get<std::shared_ptr<const interop::CppNativeStruct>>(*native);
                }
            }
        }
        else return std::nullopt;
        while (index < tokens_.size() && tokens_[index].kind() == lexer::TokenKind::Star)
        {
            ++value.pointerDepth;
            ++index;
        }
        result.next = Position{index, false};
        return result;
    }

    // The dotted template name of a use from start to its name token. / El nombre de plantilla de un uso.
    std::string nestedPath(std::size_t start, std::size_t nameIndex) const
    {
        std::string path;
        for (std::size_t index = start; index <= nameIndex; ++index) path += tokens_[index].lexeme();
        return path;
    }

    static std::string_view builtinName(lexer::TokenKind kind)
    {
        switch (kind)
        {
        case lexer::TokenKind::KwBool: return "bool";
        case lexer::TokenKind::KwSByte: return "signed char";
        case lexer::TokenKind::KwByte: return "unsigned char";
        case lexer::TokenKind::KwShort: return "short";
        case lexer::TokenKind::KwUShort: return "unsigned short";
        case lexer::TokenKind::KwInt: return "int";
        case lexer::TokenKind::KwUInt: return "unsigned int";
        case lexer::TokenKind::KwLong: case lexer::TokenKind::KwNInt: return "long long";
        case lexer::TokenKind::KwULong: case lexer::TokenKind::KwNUInt: return "unsigned long long";
        case lexer::TokenKind::KwFloat: return "float";
        case lexer::TokenKind::KwDouble: return "double";
        case lexer::TokenKind::KwVoid: return "void";
        default: return {};
        }
    }

    std::span<const lexer::Token> tokens_;
    const std::map<std::string, bool>& templates_;
    Instantiate instantiate_;
    TypeUsr typeUsr_;
    NativeStruct nativeStruct_;
    Reject reject_;
};
}

// EN: T13-006. Before parsing, each C++ template specialization that a NativeSharp unit writes is instantiated
//     by the sessions of its project that declare the template, and the use is rewritten to the name of the
//     specialization. A new class specialization is imported like those the header names.
// ES: T13-006. Antes de analizar, cada especializacion de plantilla C++ que escribe una unidad NativeSharp la
//     instancian las sesiones de su proyecto que declaran la plantilla, y el uso se reescribe con el nombre de
//     la especializacion. Una especializacion de clase nueva se importa como las que nombra el encabezado.
namespace
{
void collectDeclaredNames(const ast::AstNode& node, std::set<std::string, std::less<>>& names)
{
    switch (node.kind())
    {
        case ast::AstNodeKind::ParameterDeclaration:
        case ast::AstNodeKind::FieldDeclaration:
        case ast::AstNodeKind::MethodDeclaration:
        case ast::AstNodeKind::PropertyDeclaration:
        case ast::AstNodeKind::FunctionDeclaration:
        case ast::AstNodeKind::GlobalVariableDeclaration:
        case ast::AstNodeKind::VariableDeclarationStatement:
            names.insert(std::string(node.text()));
            break;
        default:
            break;
    }
    for (const ast::AstNode* child : {node.first(), node.second(), node.third()})
        if (child) collectDeclaredNames(*child, names);
    for (const ast::AstNode* child : node.children())
        if (child) collectDeclaredNames(*child, names);
}

struct MisplacedArgumentList
{
    SourceRange range;
    std::string name;
    std::size_t count = 0;
};

void expandArgumentListCalls(ast::AstNode& node, ast::AstArena& arena,
                             const std::map<std::string, std::size_t, std::less<>>& macros,
                             std::set<const ast::AstNode*>& visited, std::vector<MisplacedArgumentList>& misplaced)
{
    if (!visited.insert(&node).second) return;
    if (node.kind() == ast::AstNodeKind::IdentifierExpression)
        if (const auto macro = macros.find(node.text()); macro != macros.end())
            misplaced.push_back({node.range(), macro->first, macro->second});
    if (node.kind() == ast::AstNodeKind::CallExpression)
        for (std::size_t index = 0; index < node.children().size(); ++index)
        {
            ast::AstNode* argument = node.children()[index];
            if (!argument || argument->kind() != ast::AstNodeKind::IdentifierExpression) continue;
            const auto macro = macros.find(argument->text());
            if (macro == macros.end()) continue;
            for (std::size_t element = 0; element < macro->second; ++element)
            {
                auto* replacement = arena.make<ast::AstNode>(ast::AstNodeKind::IdentifierExpression, argument->range());
                replacement->setText(macro->first + "__nsc_arg" + std::to_string(element));
                if (element == 0) node.replaceChild(index, replacement);
                else node.insertChild(index + element, replacement);
            }
            index += macro->second - 1U;
        }
    for (ast::AstNode* child : {node.first(), node.second(), node.third()})
        if (child) expandArgumentListCalls(*child, arena, macros, visited, misplaced);
    for (ast::AstNode* child : std::vector<ast::AstNode*>(node.children()))
        if (child) expandArgumentListCalls(*child, arena, macros, visited, misplaced);
}

// EN: C2-03. `T v = M;` with M a brace-initializer macro takes the value Clang built for T; any other use of M is
//     reported with the types it fits.
// ES: C2-03. `T v = M;` con M una macro de llaves toma el valor que Clang construyo para T; cualquier otro uso de M
//     se informa con los tipos a los que se ajusta.
void applyBraceInitializerMacros(ast::AstNode& node, ast::AstArena& arena,
    const std::map<std::string, std::vector<std::string>, std::less<>>& macros,
    std::set<const ast::AstNode*>& visited, std::vector<MisplacedArgumentList>& misplaced,
    std::vector<std::string>& reasons)
{
    if (!visited.insert(&node).second) return;
    if (node.kind() == ast::AstNodeKind::VariableDeclarationStatement && node.first() &&
        node.first()->kind() == ast::AstNodeKind::IdentifierExpression)
        if (const auto macro = macros.find(node.first()->text()); macro != macros.end())
        {
            const ast::TypeSyntax* type = node.typeSyntax();
            const bool plain = type && type->modifiers().empty() && type->typeArguments().empty();
            const std::string spelled = plain ? std::string(type->baseName()) : std::string{};
            if (plain && std::find(macro->second.begin(), macro->second.end(), spelled) != macro->second.end())
            {
                auto* value = arena.make<ast::AstNode>(ast::AstNodeKind::IdentifierExpression, node.first()->range());
                value->setText(macro->first + "__nsc_init_" + spelled);
                node.setFirst(value);
                visited.insert(value);
            }
        }
    if (node.kind() == ast::AstNodeKind::IdentifierExpression)
        if (const auto macro = macros.find(node.text()); macro != macros.end())
        {
            std::string types;
            for (const auto& type : macro->second) types += (types.empty() ? "'" : ", '") + type + "'";
            misplaced.push_back({node.range(), macro->first, 0});
            reasons.push_back("unknown identifier '" + macro->first + "': the C macro is a brace initializer list, "
                "which C gives a value only as the initializer of a declaration of a record type Clang accepts it "
                "for: " + types);
        }
    for (ast::AstNode* child : {node.first(), node.second(), node.third()})
        if (child) applyBraceInitializerMacros(*child, arena, macros, visited, misplaced, reasons);
    for (ast::AstNode* child : std::vector<ast::AstNode*>(node.children()))
        if (child) applyBraceInitializerMacros(*child, arena, macros, visited, misplaced, reasons);
}

// EN: C2-04. The C spelling of a constant macro argument written in NativeSharp: integer literals, names (macros,
//     enumerators) and nested calls to function-like macros. Null for anything else.
// ES: C2-04. La escritura C de un argumento constante de macro escrito en NativeSharp: literales enteros, nombres
//     (macros, enumeradores) y llamadas anidadas a macros de funcion. Nulo para lo demas.
std::optional<std::string> cMacroConstantSpelling(const ast::AstNode* node,
                                                  const std::set<std::string, std::less<>>& functionMacros)
{
    if (!node) return std::nullopt;
    if (node->kind() == ast::AstNodeKind::IntegerLiteralExpression ||
        node->kind() == ast::AstNodeKind::IdentifierExpression)
        return std::string(node->text());
    if (node->kind() != ast::AstNodeKind::CallExpression || !node->first() ||
        node->first()->kind() != ast::AstNodeKind::IdentifierExpression ||
        !functionMacros.contains(node->first()->text()))
        return std::nullopt;
    std::string text = std::string(node->first()->text()) + "(";
    for (std::size_t index = 0; index < node->children().size(); ++index)
    {
        const auto argument = cMacroConstantSpelling(node->children()[index], functionMacros);
        if (!argument) return std::nullopt;
        text += (index ? ", " : "") + *argument;
    }
    return text + ")";
}

// EN: C2-04. An array dimension that calls a function-like macro (`byte[LZ4_COMPRESSBOUND(64)]`) needs a constant,
//     not the runtime wrapper of the macro: Clang evaluates the expansion as an integer constant expression of the
//     header's language and the dimension takes that value. A dimension Clang does not accept stays as written.
// ES: C2-04. Una dimension de arreglo que llama a una macro de funcion necesita una constante, no el envoltorio de
//     ejecucion de la macro: Clang evalua la expansion como expresion constante entera del lenguaje del encabezado y
//     la dimension toma ese valor. Una dimension que Clang no acepta queda como esta escrita.
void evaluateMacroArrayDimensions(ast::TypeSyntax* type, ast::AstArena& arena,
    const std::set<std::string, std::less<>>& functionMacros,
    const std::vector<std::unique_ptr<interop::CppInteropSession>>& sessions)
{
    if (!type) return;
    for (std::size_t index = 0; index < type->modifiers().size(); ++index)
    {
        const ast::AstNode* count = type->modifiers()[index].arrayCount;
        if (!count || count->kind() != ast::AstNodeKind::CallExpression) continue;
        const auto spelling = cMacroConstantSpelling(count, functionMacros);
        if (!spelling) continue;
        for (const auto& session : sessions)
            if (session)
                if (const auto value = session->evaluateIntegerConstantExpression(*spelling))
                {
                    auto* literal = arena.make<ast::AstNode>(ast::AstNodeKind::IntegerLiteralExpression, count->range());
                    literal->setTokenKind(lexer::TokenKind::IntegerLiteral);
                    literal->setText(value->second ? std::to_string(static_cast<std::int64_t>(value->first))
                                                   : std::to_string(value->first));
                    literal->setIntegerValue(value->first);
                    type->setArrayCount(index, literal);
                    break;
                }
    }
    for (ast::TypeSyntax* argument : type->typeArguments())
        evaluateMacroArrayDimensions(argument, arena, functionMacros, sessions);
    evaluateMacroArrayDimensions(type->functionReturnType(), arena, functionMacros, sessions);
}

// EN: X4-04. A C++ variable template specialization written as a value is spelled in C++ (`lib.sz<long>` is
//     `lib::sz<long long>`) and Clang evaluates it as a constant expression of the header: a bool becomes a bool
//     literal and an integral value an integer literal. Anything else stays and is rejected with its reason.
// ES: X4-04. Una especializacion de plantilla de variable C++ escrita como valor se escribe en C++ y Clang la evalua
//     como expresion constante del encabezado: un bool se vuelve literal bool y un valor entero, literal entero. Lo
//     demas queda y se rechaza con su motivo.
std::optional<std::string> variableTemplateSpelling(const ProjectSyntaxUnit& unit, const ast::AstNode& node)
{
    const auto range = node.range();
    std::string text;
    for (const auto& token : unit.tokens())
    {
        if (token.range().begin().offset() < range.begin().offset()) continue;
        if (token.range().end().offset() > range.end().offset()) break;
        using lexer::TokenKind;
        std::string spelling;
        switch (token.kind())
        {
        case TokenKind::Identifier: case TokenKind::IntegerLiteral: spelling = std::string(token.lexeme()); break;
        case TokenKind::Dot: spelling = "::"; break;
        case TokenKind::Less: spelling = "<"; break;
        case TokenKind::Greater: spelling = ">"; break;
        case TokenKind::GreaterGreater: spelling = "> >"; break;
        case TokenKind::Comma: spelling = ", "; break;
        case TokenKind::Star: spelling = "*"; break;
        case TokenKind::Minus: spelling = "-"; break;
        case TokenKind::KwTrue: spelling = "true"; break;
        case TokenKind::KwFalse: spelling = "false"; break;
        case TokenKind::KwBool: spelling = "bool"; break;
        case TokenKind::KwSByte: spelling = "signed char"; break;
        case TokenKind::KwByte: spelling = "unsigned char"; break;
        case TokenKind::KwShort: spelling = "short"; break;
        case TokenKind::KwUShort: spelling = "unsigned short"; break;
        case TokenKind::KwInt: spelling = "int"; break;
        case TokenKind::KwUInt: spelling = "unsigned int"; break;
        case TokenKind::KwLong: case TokenKind::KwNInt: spelling = "long long"; break;
        case TokenKind::KwULong: case TokenKind::KwNUInt: spelling = "unsigned long long"; break;
        case TokenKind::KwFloat: spelling = "float"; break;
        case TokenKind::KwDouble: spelling = "double"; break;
        case TokenKind::KwVoid: spelling = "void"; break;
        default: return std::nullopt;
        }
        if (!text.empty() && !spelling.empty() && std::isalnum(static_cast<unsigned char>(text.back())) &&
            std::isalnum(static_cast<unsigned char>(spelling.front()))) text += ' ';
        text += spelling;
    }
    return text.empty() ? std::nullopt : std::optional<std::string>("(" + text + ")");
}

void evaluateVariableTemplatesInTree(ast::AstNode& node, ast::AstArena& arena, const ProjectSyntaxUnit& unit,
    const std::vector<std::unique_ptr<interop::CppInteropSession>>& sessions, std::set<const ast::AstNode*>& visited)
{
    if (!visited.insert(&node).second) return;
    const auto evaluated = [&](ast::AstNode* child) -> ast::AstNode* {
        if (!child || child->kind() != ast::AstNodeKind::VariableTemplateExpression) return child;
        const auto spelling = variableTemplateSpelling(unit, *child);
        if (!spelling) return child;
        for (const auto& session : sessions)
        {
            if (!session) continue;
            // EN: X4-04. A `float` or `double` constant is read through its bits (`__builtin_bit_cast` is an integer
            //     constant expression) and written as the decimal literal that converts back to the same value.
            // ES: X4-04. Una constante `float` o `double` se lee por sus bits y se escribe como el literal decimal
            //     que vuelve al mismo valor.
            for (const bool single : {false, true})
            {
                const std::string type = single ? "float" : "double";
                const auto isFloating = session->evaluateIntegerConstantExpression(
                    "__is_same(__remove_cvref(decltype(" + *spelling + ")), " + type + ") ? 1 : 0");
                if (!isFloating || !isFloating->first) continue;
                const auto bits = session->evaluateIntegerConstantExpression("__builtin_bit_cast(" +
                    std::string(single ? "unsigned int" : "unsigned long long") + ", (" + type + ")(" + *spelling + "))");
                if (!bits) break;
                double value = 0.0;
                if (single)
                {
                    const auto raw = static_cast<std::uint32_t>(bits->first);
                    float narrow = 0.0F;
                    std::memcpy(&narrow, &raw, sizeof narrow);
                    value = narrow;
                }
                else
                {
                    const std::uint64_t raw = bits->first;
                    std::memcpy(&value, &raw, sizeof value);
                }
                if (!std::isfinite(value)) break;
                char text[64];
                std::snprintf(text, sizeof text, single ? "%.9g" : "%.17g", value);
                std::string literalText(text);
                if (literalText.find_first_of(".eE") == std::string::npos) literalText += ".0";
                if (single) literalText += "f";
                auto* literal = arena.make<ast::AstNode>(ast::AstNodeKind::FloatingLiteralExpression, child->range());
                literal->setTokenKind(lexer::TokenKind::FloatingLiteral);
                literal->setText(literalText);
                literal->setFloatingValue(single ? static_cast<double>(static_cast<float>(value)) : value);
                return literal;
            }
            // The probe accepts integer types; a bool is read through `? 1 : 0`. / La sonda acepta enteros; un bool
            //     se lee con `? 1 : 0`.
            const auto isBool = session->evaluateIntegerConstantExpression(
                "__is_same(__remove_cvref(decltype(" + *spelling + ")), bool) ? 1 : 0");
            if (!isBool) continue;
            const auto value = session->evaluateIntegerConstantExpression(
                isBool->first ? *spelling + " ? 1 : 0" : *spelling);
            if (!value)
            {
                // EN: X4-04. A specialization that is not a constant (`inline T zero`) is its storage: a global the
                //     session reports through an accessor to its address, read and written like any C++ global.
                // ES: X4-04. Una especializacion que no es constante es su almacenamiento: una global que la sesion
                //     informa por una accesora a su direccion, leida y escrita como cualquier global C++.
                if (const auto global = session->declareVariableTemplateAccessor(*spelling))
                {
                    auto* identifier = arena.make<ast::AstNode>(ast::AstNodeKind::IdentifierExpression, child->range());
                    identifier->setText(*global);
                    return identifier;
                }
                continue;
            }
            if (isBool->first)
            {
                auto* literal = arena.make<ast::AstNode>(ast::AstNodeKind::BooleanLiteralExpression, child->range());
                literal->setTokenKind(value->first ? lexer::TokenKind::KwTrue : lexer::TokenKind::KwFalse);
                literal->setIntegerValue(value->first ? 1U : 0U);
                literal->setText(value->first ? "true" : "false");
                return literal;
            }
            auto* literal = arena.make<ast::AstNode>(ast::AstNodeKind::IntegerLiteralExpression, child->range());
            literal->setTokenKind(lexer::TokenKind::IntegerLiteral);
            literal->setText(value->second ? std::to_string(static_cast<std::int64_t>(value->first))
                                           : std::to_string(value->first));
            literal->setIntegerValue(value->first);
            return literal;
        }
        return child;
    };
    if (auto* replaced = evaluated(node.first()); replaced != node.first()) node.setFirst(replaced);
    if (auto* replaced = evaluated(node.second()); replaced != node.second()) node.setSecond(replaced);
    if (auto* replaced = evaluated(node.third()); replaced != node.third()) node.setThird(replaced);
    for (std::size_t index = 0; index < node.children().size(); ++index)
        if (auto* replaced = evaluated(node.children()[index]); replaced != node.children()[index])
            node.replaceChild(index, replaced);
    for (ast::AstNode* child : {node.first(), node.second(), node.third()})
        if (child) evaluateVariableTemplatesInTree(*child, arena, unit, sessions, visited);
    for (ast::AstNode* child : node.children())
        if (child) evaluateVariableTemplatesInTree(*child, arena, unit, sessions, visited);
}

void evaluateMacroArrayDimensionsInTree(ast::AstNode& node, ast::AstArena& arena,
    const std::set<std::string, std::less<>>& functionMacros,
    const std::vector<std::unique_ptr<interop::CppInteropSession>>& sessions, std::set<const ast::AstNode*>& visited)
{
    if (!visited.insert(&node).second) return;
    evaluateMacroArrayDimensions(const_cast<ast::TypeSyntax*>(node.typeSyntax()), arena, functionMacros, sessions);
    for (ast::AstNode* child : {node.first(), node.second(), node.third()})
        if (child) evaluateMacroArrayDimensionsInTree(*child, arena, functionMacros, sessions, visited);
    for (ast::AstNode* child : node.children())
        if (child) evaluateMacroArrayDimensionsInTree(*child, arena, functionMacros, sessions, visited);
}

std::vector<MisplacedArgumentList> expandCppArgumentListMacros(ast::AstNode& root, ast::AstArena& arena,
                                 const std::map<std::string, std::size_t, std::less<>>& macros)
{
    std::set<std::string, std::less<>> declared;
    collectDeclaredNames(root, declared);
    std::map<std::string, std::size_t, std::less<>> visible;
    for (const auto& [name, count] : macros)
        if (!declared.contains(name)) visible.emplace(name, count);
    if (visible.empty()) return {};
    std::set<const ast::AstNode*> visited;
    std::vector<MisplacedArgumentList> misplaced;
    expandArgumentListCalls(root, arena, visible, visited, misplaced);
    return misplaced;
}
}

bool ProjectCompilation::applyCppTemplateUses(std::string& loadError)
{
    cppFunctionTemplates_.clear();
    cppNativeMirrorTypes_.clear();
    cppNativeMirrorLayouts_.clear();
    cppUnimportedMacros_.clear();
    cppFunctionMacros_.clear();
    cppFunctionMacroTokens_.clear();
    cppArgumentListMacros_.clear();
    cppBraceInitializerMacros_.clear();
    if (cppProjectSessions_.empty() || !projectGraph_) return true;
    // EN: T14-001, T14-003. The macros of each project that are not imported as constants, so a call to one says
    //     why: NativeSharp does not run the C preprocessor over its own code.
    // ES: T14-001, T14-003. Las macros de cada proyecto que no se importan como constantes, para que una llamada
    //     a una diga por que: NativeSharp no aplica el preprocesador C a su propio codigo.
    for (std::size_t ordinal = 0; ordinal < projectGraph_->projects().size(); ++ordinal)
        for (const auto& group : cppProjectSessions_)
        {
            std::error_code ec;
            if (!std::filesystem::equivalent(group.projectFile, projectGraph_->projects()[ordinal].projectFile(), ec) || ec)
                continue;
            for (const auto& session : group.sessions)
                if (session)
                    for (const auto& macro : session->macrosSnapshot())
                    {
                        // EN: T19-003. A function-like macro is expanded at each call by a Clang wrapper.
                        // ES: T19-003. Una macro de funcion se expande en cada llamada con un envoltorio de Clang.
                        // C2-02: an argument-list macro whose elements were all imported. / Una macro lista cuyos
                        //     elementos se importaron todos.
                        // C2-03: a brace-initializer macro with at least one accepted record type. / Una macro
                        //     de llaves con al menos un tipo record aceptado.
                        if (!macro.braceInitializerTypes.empty())
                        {
                            cppBraceInitializerMacros_[ordinal][macro.name] = macro.braceInitializerTypes;
                            continue;
                        }
                        if (macro.argumentListCount != 0 && macro.constant)
                        {
                            cppArgumentListMacros_[ordinal][macro.name] = macro.argumentListCount;
                            cppUnimportedMacros_[ordinal].emplace(macro.name,
                                "expands to a list of " + std::to_string(macro.argumentListCount) +
                                " arguments; write it as call arguments, where it stands for all of them");
                            continue;
                        }
                        if (macro.functionLike)
                        {
                            cppFunctionMacros_[ordinal].insert(macro.name);
                            if (!macro.tokenParameters.empty())
                                cppFunctionMacroTokens_[ordinal][macro.name] = macro.tokenParameters;
                        }
                        else if (!macro.constant)
                            cppUnimportedMacros_[ordinal].emplace(macro.name, macro.functionLike ?
                                "is a function-like macro: NativeSharp does not expand C/C++ macros; declare an "
                                "inline function in the header instead" :
                                macro.argumentListCount != 0
                                ? "expands to a list of arguments and only fits a call argument; one of its elements "
                                  "is not a value NativeSharp can import"
                                : "does not expand to a scalar constant that NativeSharp can import");
                    }
            break;
        }
    struct ProjectTemplates
    {
        std::vector<interop::CppInteropSession*> sessions;
        std::map<std::string, bool> names;
    };
    std::map<std::size_t, ProjectTemplates> byProject;
    for (std::size_t ordinal = 0; ordinal < projectGraph_->projects().size(); ++ordinal)
    {
        const auto& project = projectGraph_->projects()[ordinal];
        if (project.cppLibraryReferences().empty()) continue;
        for (const auto& group : cppProjectSessions_)
        {
            std::error_code ec;
            if (!std::filesystem::equivalent(group.projectFile, project.projectFile(), ec) || ec) continue;
            ProjectTemplates templates;
            for (const auto& session : group.sessions)
            {
                if (!session) continue;
                const auto names = session->templatesSnapshot();
                for (const auto& name : names)
                {
                    std::string dotted;
                    for (std::size_t start = 0; start <= name.qualifiedName.size();)
                    {
                        const auto end = name.qualifiedName.find("::", start);
                        if (!dotted.empty()) dotted += '.';
                        dotted += name.qualifiedName.substr(start, end == std::string::npos ? std::string::npos : end - start);
                        if (end == std::string::npos) break;
                        start = end + 2;
                    }
                    templates.names.emplace(std::move(dotted), name.function);
                    if (name.function) cppFunctionTemplates_[ordinal].insert(name.qualifiedName);
                }
                if (!names.empty()) templates.sessions.push_back(session.get());
            }
            if (!templates.names.empty()) byProject.emplace(ordinal, std::move(templates));
            break;
        }
    }
    if (byProject.empty()) return true;

    // T13-007: the NativeSharp types of each project. / Los tipos NativeSharp de cada proyecto.
    std::map<std::size_t, std::map<std::string, NativeTypeShape>> nativeTypes;
    for (const auto& unit : units_)
        if (unit && !unit->isPrecompiled() && unit->isLexicallyValid() && byProject.contains(unit->projectOrdinal()))
            scanNativeTypes(unit->tokens_, nativeTypes[unit->projectOrdinal()]);
    std::map<std::pair<std::size_t, std::string>, std::shared_ptr<const interop::CppNativeStruct>> nativeStructs;
    using NativeResult = std::optional<std::variant<std::shared_ptr<const interop::CppNativeStruct>, std::string>>;
    std::function<NativeResult(std::size_t, const std::string&, const std::vector<std::string>&, unsigned)> resolveNative =
        [&](std::size_t project, const std::string& path, const std::vector<std::string>& scope, unsigned depth) -> NativeResult {
        const auto types = nativeTypes.find(project);
        if (types == nativeTypes.end() || depth > 16) return std::nullopt;
        const NativeTypeShape* shape = nullptr;
        std::string key;
        for (std::size_t length = scope.size() + 1; length-- > 0 && !shape;)
        {
            std::string candidate;
            for (std::size_t segment = 0; segment < length; ++segment) candidate += scope[segment] + ".";
            candidate += path;
            if (const auto found = types->second.find(candidate); found != types->second.end())
            { shape = &found->second; key = candidate; }
        }
        if (!shape)
            for (const auto& [name, candidate] : types->second)
                if (name.ends_with("." + path))
                {
                    if (shape) return std::nullopt;
                    shape = &candidate;
                    key = name;
                }
        if (!shape) return std::nullopt;
        if (shape->isClass)
            return NativeResult(std::string("NativeSharp class '" + key +
                "' is a reference type; a C++ template argument needs a NativeSharp struct"));
        if (!shape->unsupported.empty())
            return NativeResult(std::string("NativeSharp struct '" + key +
                "' cannot be a C++ template argument: " + shape->unsupported));
        if (const auto cached = nativeStructs.find({project, key}); cached != nativeStructs.end())
            return NativeResult(cached->second);
        auto structure = std::make_shared<interop::CppNativeStruct>();
        structure->moduleName = projectGraph_->projects()[project].moduleName();
        structure->namespaceSegments = shape->namespaceSegments;
        structure->name = shape->name;
        for (const auto& field : shape->fields)
        {
            interop::CppNativeStructField mirrored;
            mirrored.name = field.name;
            mirrored.builtin = field.builtin;
            mirrored.pointerDepth = field.pointerDepth;
            if (!field.structure.empty())
            {
                const auto nested = resolveNative(project, field.structure, shape->namespaceSegments, depth + 1);
                if (!nested || std::holds_alternative<std::string>(*nested))
                    return NativeResult(std::string("NativeSharp struct '" + key + "' cannot be a C++ template "
                        "argument: field '" + field.name + "' has a type with no C++ equivalent"));
                mirrored.structure = std::get<std::shared_ptr<const interop::CppNativeStruct>>(*nested);
            }
            structure->fields.push_back(std::move(mirrored));
        }
        if (structure->fields.empty())
            return NativeResult(std::string("NativeSharp struct '" + key + "' cannot be a C++ template argument: "
                "it has no instance fields"));
        std::shared_ptr<const interop::CppNativeStruct> result = std::move(structure);
        nativeStructs.emplace(std::make_pair(project, key), result);
        cppNativeMirrorTypes_.insert(compilation::TypeIdentity(result->moduleName, result->namespaceSegments, {},
                                                               result->name).key());
        return NativeResult(result);
    };

    bool classInstantiated = false;
    bool failed = false;
    std::map<std::string, std::optional<CppTemplateUseRewriter::Specialization>> instantiated;
    for (auto& unit : units_)
    {
        if (!unit || unit->isPrecompiled() || !unit->isLexicallyValid() || unit->isSyntaxValid()) continue;
        const auto project = byProject.find(unit->projectOrdinal());
        if (project == byProject.end()) continue;
        const auto projectOrdinal = unit->projectOrdinal();
        using Specialization = CppTemplateUseRewriter::Specialization;
        const auto instantiate = [&](const std::string& qualifiedName, bool function,
                                     const std::vector<interop::CppTemplateArgument>& arguments,
                                     SourceRange range) -> std::optional<Specialization> {
            std::string key = std::to_string(projectOrdinal) + (function ? "|f|" : "|c|") + qualifiedName;
            for (const auto& argument : arguments)
                key += "|" + std::to_string(static_cast<int>(argument.kind)) + ":" + argument.builtin + ":" +
                    argument.recordUsr.value_or("") + ":" + std::to_string(argument.pointerDepth) + ":" +
                    std::to_string(argument.integral) + ":" + (argument.boolean ? "1" : "0") + ":" +
                    argument.templateName + ":" + (argument.nativeStruct ? argument.nativeStruct->moduleName + "::" +
                        argument.nativeStruct->name + "@" + std::to_string(
                            reinterpret_cast<std::uintptr_t>(argument.nativeStruct.get())) : std::string());
            if (const auto cached = instantiated.find(key); cached != instantiated.end()) return cached->second;
            std::optional<Specialization> name;
            std::vector<std::string> notes;
            bool rejected = false;
            for (auto* session : project->second.sessions)
            {
                const auto result = session->instantiateTemplate(qualifiedName, function, arguments);
                if (!result) continue;
                if (!result->instantiated)
                {
                    rejected = true;
                    notes.insert(notes.end(), result->diagnostics.begin(), result->diagnostics.end());
                    continue;
                }
                if (name && name->name != result->name) rejected = true;
                name = Specialization{result->name, result->clangUsr, result->aliasTarget};
                for (const auto& layout : result->nativeLayouts)
                    cppNativeMirrorLayouts_.emplace(compilation::TypeIdentity(layout.moduleName, layout.namespaceSegments,
                        {}, layout.name).key(), layout);
            }
            if (!name || rejected)
            {
                diagnostics_.reportStructured(diagnostics::DiagnosticId::UnknownTypeName,
                    diagnostics::DiagnosticSeverity::Error, range,
                    "Clang cannot instantiate the C++ template '" + qualifiedName + "' with these arguments",
                    {}, std::move(notes));
                failed = true;
                name.reset();
            }
            else if (!function) classInstantiated = true;
            instantiated.emplace(key, name);
            return name;
        };
        const auto typeUsr = [&](const std::string& dottedName) -> std::optional<std::string> {
            for (const auto& type : experimentalCppImportedTypes_)
                if (type.projectOrdinal == projectOrdinal && type.clangUsr &&
                    (type.origin == CppImportedTypeOrigin::CppRecord || type.origin == CppImportedTypeOrigin::CppEnum) &&
                    type.identity.fullyQualifiedName() == dottedName)
                    return type.clangUsr;
            // A C++ alias of a record. / Un alias C++ de un record.
            for (const auto& alias : cppImportedTypeAliases_)
            {
                if (alias.projectOrdinal != projectOrdinal) continue;
                const auto& target = alias.declaration.underlyingType;
                if (target.kind != interop::CppTypeKind::Record || !target.recordIdentity ||
                    !target.recordIdentity->clangUsr) continue;
                std::string dotted = alias.declaration.qualifiedName;
                for (std::size_t at = 0; (at = dotted.find("::", at)) != std::string::npos;) dotted.replace(at, 2, ".");
                if (dotted == dottedName) return target.recordIdentity->clangUsr;
            }
            return std::nullopt;
        };
        auto& tokens = unit->tokens_;
        std::vector<lexer::Token> rewritten;
        bool changed = false;
        for (std::size_t index = 0; index < tokens.size();)
        {
            const bool qualifiedStart = index == 0 || (tokens[index - 1].kind() != lexer::TokenKind::Dot &&
                tokens[index - 1].kind() != lexer::TokenKind::QuestionDot && tokens[index - 1].kind() != lexer::TokenKind::Arrow);
            std::optional<CppTemplateUseRewriter::Use> use;
            if (qualifiedStart && tokens[index].kind() == lexer::TokenKind::Identifier)
                use = CppTemplateUseRewriter(tokens, project->second.names, instantiate, typeUsr,
                    [&](const std::string& path) { return resolveNative(projectOrdinal, path, {}, 0); },
                    [&](SourceRange range, std::string message) {
                        diagnostics_.report(diagnostics::DiagnosticId::UnknownTypeName,
                            diagnostics::DiagnosticSeverity::Error, range, std::move(message));
                        failed = true;
                    }).match(index);
            if (!use || use->failed || use->next.half)
            {
                if (changed) rewritten.push_back(tokens[index]);
                ++index;
                continue;
            }
            if (!changed) { rewritten.assign(tokens.begin(), tokens.begin() + index); changed = true; }
            // EN: T19-005. A use of an alias template is replaced by the type it names, written in full: a keyword
            //     for a builtin scalar, or the namespaces, records and name of a record or enum; then its pointers.
            // ES: T19-005. Un uso de una plantilla alias se reemplaza por el tipo que nombra, escrito completo: una
            //     palabra clave para un escalar, o los namespaces, records y nombre de un record o enum; luego sus
            //     punteros.
            if (use->alias)
            {
                const SourceRange whole(tokens[index].range().begin(), tokens[use->closeIndex].range().end());
                const auto& target = *use->alias;
                if (!target.builtin.empty())
                {
                    using lexer::TokenKind;
                    static const std::map<std::string, std::pair<TokenKind, const char*>> keywords{
                        {"bool", {TokenKind::KwBool, "bool"}}, {"signed char", {TokenKind::KwSByte, "sbyte"}},
                        {"unsigned char", {TokenKind::KwByte, "byte"}}, {"short", {TokenKind::KwShort, "short"}},
                        {"unsigned short", {TokenKind::KwUShort, "ushort"}}, {"int", {TokenKind::KwInt, "int"}},
                        {"unsigned int", {TokenKind::KwUInt, "uint"}}, {"long long", {TokenKind::KwLong, "long"}},
                        {"unsigned long long", {TokenKind::KwULong, "ulong"}},
                        {"float", {TokenKind::KwFloat, "float"}}, {"double", {TokenKind::KwDouble, "double"}},
                        {"void", {TokenKind::KwVoid, "void"}}};
                    const auto keyword = keywords.find(target.builtin);
                    if (keyword == keywords.end())
                    {
                        diagnostics_.report(diagnostics::DiagnosticId::UnknownTypeName,
                            diagnostics::DiagnosticSeverity::Error, whole,
                            "the C++ alias template names '" + target.builtin + "', which has no NativeSharp keyword");
                        failed = true;
                        rewritten.push_back(tokens[index]);
                        ++index;
                        continue;
                    }
                    rewritten.emplace_back(keyword->second.first, whole, keyword->second.second);
                }
                else
                    for (std::size_t segment = 0; segment < target.path.size(); ++segment)
                    {
                        if (segment) rewritten.emplace_back(lexer::TokenKind::Dot, whole, ".");
                        const auto& lexeme = cppTemplateLexemes_.emplace_back(target.path[segment]);
                        rewritten.emplace_back(lexer::TokenKind::Identifier, whole, lexeme);
                    }
                for (unsigned depth = 0; depth < target.pointerDepth; ++depth)
                    rewritten.emplace_back(lexer::TokenKind::Star, whole, "*");
                index = use->next.index;
                continue;
            }
            for (auto prefix = index; prefix < use->nameIndex; ++prefix) rewritten.push_back(tokens[prefix]);
            const auto& lexeme = cppTemplateLexemes_.emplace_back(use->name);
            rewritten.emplace_back(lexer::TokenKind::Identifier,
                SourceRange(tokens[use->nameIndex].range().begin(), tokens[use->closeIndex].range().end()), lexeme);
            index = use->next.index;
        }
        if (changed) tokens = std::move(rewritten);
    }
    // The diagnostics carry the rejection, as a parse error does. / Los diagnosticos llevan el rechazo.
    if (failed) return false;
    return !classInstantiated || refreshCppImportedTypes(loadError);
}

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
    if (!applyCppTemplateUses(loadError)) return false;
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
    // EN: C2-02. A call argument that names an argument-list macro stands for its elements, in order, as the C
    //     preprocessor expands it: `aiGetMaterialColor(m, AI_MATKEY_COLOR, &c)` passes `"$clr.diffuse", 0, 0`. A unit
    //     that declares a variable, parameter, field or member with the macro's name keeps NativeSharp's lookup.
    // ES: C2-02. Un argumento de llamada que nombra una macro lista equivale a sus elementos, en orden, como los
    //     expande el preprocesador C. Una unidad que declara una variable, parametro, campo o miembro con el nombre de
    //     la macro conserva la busqueda de NativeSharp.
    for (auto& unit : units_)
    {
        if (!unit || !unit->syntaxTree()) continue;
        // X4-04: C++ variable templates written as values. / Plantillas de variable C++ escritas como valores.
        for (const auto& group : cppProjectSessions_)
        {
            std::error_code ec;
            if (!std::filesystem::equivalent(group.projectFile,
                    projectGraph_->projects()[unit->projectOrdinal()].projectFile(), ec) || ec)
                continue;
            std::set<const ast::AstNode*> visited;
            evaluateVariableTemplatesInTree(*unit->syntaxTree(), unit->arena(), *unit, group.sessions, visited);
            break;
        }
        if (const auto macros = cppFunctionMacros_.find(unit->projectOrdinal()); macros != cppFunctionMacros_.end())
            for (const auto& group : cppProjectSessions_)
            {
                std::error_code ec;
                if (!std::filesystem::equivalent(group.projectFile,
                        projectGraph_->projects()[unit->projectOrdinal()].projectFile(), ec) || ec)
                    continue;
                std::set<const ast::AstNode*> visited;
                evaluateMacroArrayDimensionsInTree(*unit->syntaxTree(), unit->arena(), macros->second, group.sessions,
                                                   visited);
                break;
            }
        if (const auto braces = cppBraceInitializerMacros_.find(unit->projectOrdinal());
            braces != cppBraceInitializerMacros_.end())
        {
            std::set<std::string, std::less<>> declared;
            collectDeclaredNames(*unit->syntaxTree(), declared);
            std::map<std::string, std::vector<std::string>, std::less<>> visible;
            for (const auto& [name, types] : braces->second)
                if (!declared.contains(name)) visible.emplace(name, types);
            std::set<const ast::AstNode*> visited;
            std::vector<MisplacedArgumentList> misplaced;
            std::vector<std::string> reasons;
            if (!visible.empty())
                applyBraceInitializerMacros(*unit->syntaxTree(), unit->arena(), visible, visited, misplaced, reasons);
            for (std::size_t index = 0; index < misplaced.size(); ++index)
                diagnostics_.report(diagnostics::DiagnosticId::UnknownIdentifier,
                    diagnostics::DiagnosticSeverity::Error, misplaced[index].range, reasons[index]);
        }
        const auto lists = cppArgumentListMacros_.find(unit->projectOrdinal());
        if (lists == cppArgumentListMacros_.end()) continue;
        for (const auto& use : expandCppArgumentListMacros(*unit->syntaxTree(), unit->arena(), lists->second))
            diagnostics_.report(diagnostics::DiagnosticId::UnknownIdentifier, diagnostics::DiagnosticSeverity::Error,
                use.range, "unknown identifier '" + use.name + "': the C macro expands to a list of " +
                    std::to_string(use.count) + " arguments; write it as call arguments, where it stands for all "
                    "of them");
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
    if (!refreshCppImportedTypes(loadError)) return false;
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
    static_cast<void>(diagnostics);
    std::vector<std::unique_ptr<ProjectSyntaxUnit>> copied;
    copied.reserve(units_.size());
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
        else
        {
            // T13-006: the tokens as parsed, with the C++ template uses already rewritten. / Los tokens como se
            // analizaron, con los usos de plantillas C++ ya reescritos.
            // EN: T19-025. The observation runs after the generic pass, which declared the specializations and
            //     renamed the generic uses inside these trees. Reparsing the tokens brought back the open uses
            //     (`Span<T>`, `Holder<int>`), which the name lookup never declares, so the tree is copied instead.
            // ES: T19-025. La observacion corre despues de la fase generica, que declaro las especializaciones y
            //     renombro los usos genericos dentro de estos arboles. Volver a analizar los tokens traia de vuelta
            //     los usos abiertos (`Span<T>`, `Holder<int>`), que la busqueda de nombres nunca declara, asi que se
            //     copia el arbol.
            clone->tokens_ = unit->tokens_;
            clone->lexicallyValid_ = unit->isLexicallyValid();
            if (!clone->lexicallyValid_ ||
                !internal::copySyntaxTree(unit->syntaxTree(), clone->arena_, clone->syntaxTree_))
            {
                error = "syntax observation could not copy a source syntax tree";
                return false;
            }
            clone->syntaxValid_ = true;
        }
        copied.push_back(std::move(clone));
    }
    output = std::move(copied);
    return true;
}

std::span<const CppImportedTypeAlias> ProjectCompilation::cppImportedTypeAliases() const noexcept
{
    return cppImportedTypeAliases_;
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
