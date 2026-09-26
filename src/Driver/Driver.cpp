#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Driver/ProjectCommands.h"
#include "nativesharp/Driver/BreakingChangeLedger.h"
#include "nativesharp/Driver/CompatibilityPolicy.h"
#include "nativesharp/Driver/CompilationProfile.h"
#include "nativesharp/Driver/CompilationJobPolicy.h"
#include "nativesharp/Driver/DiagnosticBundle.h"
#include "nativesharp/Driver/FinalBaselineContract.h"
#include "nativesharp/Driver/InstallationManifest.h"
#include "nativesharp/Driver/InstallationVerifier.h"
#include "nativesharp/Driver/MigrationBaseline.h"
#include "nativesharp/Driver/MigrationGuide.h"
#include "nativesharp/Driver/PackageLifecycle.h"
#include "nativesharp/Driver/PromotionAct.h"
#include "nativesharp/Driver/ReleaseCandidateFreeze.h"
#include "nativesharp/Driver/ToolchainContract.h"
#include "nativesharp/Linker/WindowsRuntimeLibrary.h"
#include "nativesharp/Runtime/RuntimeAbiContract.h"
#include <algorithm>

#include "nativesharp/AST/AstDumper.h"
#include "nativesharp/Basic/Version.h"
#include "nativesharp/CFG/CfgDumper.h"
#include "nativesharp/CFG/CfgVerifier.h"
#include "nativesharp/Compilation/ProjectDumper.h"
#include "nativesharp/Compilation/ProjectGraphDumper.h"
#include "nativesharp/Compilation/ProjectGraphLoader.h"
#include "nativesharp/Compilation/ProjectLoader.h"
#include "nativesharp/Diagnostics/DiagnosticRenderer.h"
#include "nativesharp/Dominance/DominanceDumper.h"
#include "nativesharp/Dominance/DominanceVerifier.h"
#include "nativesharp/Frontend/FrontendCompilation.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectCompilationDumper.h"
#include "nativesharp/Frontend/ProjectConstructionModelDumper.h"
#include "nativesharp/Frontend/ProjectDestructionModelDumper.h"
#include "nativesharp/Frontend/ProjectGenericModelDumper.h"
#include "nativesharp/Frontend/ProjectInheritanceModelDumper.h"
#include "nativesharp/Frontend/ProjectInterfaceModelDumper.h"
#include "nativesharp/Frontend/ProjectMemberModelDumper.h"
#include "nativesharp/Frontend/ProjectMethodBodyModelDumper.h"
#include "nativesharp/Frontend/ProjectNameLookupDumper.h"
#include "nativesharp/Frontend/ProjectObjectLayoutModelDumper.h"
#include "nativesharp/Frontend/ProjectOopHirModelDumper.h"
#include "nativesharp/Frontend/ProjectOopMirModelDumper.h"
#include "nativesharp/Frontend/ProjectVirtualDispatchModelDumper.h"
#include "nativesharp/HIR/HirDumper.h"
#include "nativesharp/HIR/HirVerifier.h"
#include "nativesharp/Interop/CHeaderGenerator.h"
#include "nativesharp/Interop/LibraryMetadata.h"
#include "nativesharp/LLVM/CoffObjectVerifier.h"
#include "nativesharp/LLVM/CompilationArtifactCache.h"
#include "nativesharp/LLVM/FloatingPointMode.h"
#include "nativesharp/LLVM/LlvmLowerer.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/LLVM/LlvmOptimizationObservation.h"
#include "nativesharp/LLVM/LinkTimeOptimizationMode.h"
#include "nativesharp/LLVM/LlvmOptimizer.h"
#include "nativesharp/LLVM/LlvmProcessTrace.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/LLVM/LlvmVersion.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"
#include "nativesharp/LLVM/ProjectOopObjectGenerator.h"
#include "nativesharp/Lexer/TokenDumper.h"
#include "nativesharp/Linker/CoffArchiveVerifier.h"
#include "nativesharp/Linker/LldLinker.h"
#include "nativesharp/Linker/PeExecutableVerifier.h"
#include "nativesharp/Linker/PdbVerifier.h"
#include "nativesharp/Linker/WindowsExecutableBuilder.h"
#include "nativesharp/Linker/WindowsLibraryBuilder.h"
#include "nativesharp/MIR/MirDumper.h"
#include "nativesharp/MIR/MirVerifier.h"
#include "nativesharp/Passes/OptimizationDumper.h"
#include "nativesharp/Passes/OptimizationVerifier.h"
#include "nativesharp/Passes/PassPipelineDumper.h"
#include "nativesharp/Runtime/WindowsRuntimeBuilder.h"
#include "nativesharp/SSA/SsaDumper.h"
#include "nativesharp/SSA/SsaVerifier.h"
#include "nativesharp/Sema/SemanticDumper.h"
#include "nativesharp/Sema/SymbolDumper.h"
#include "nativesharp/Source/SourceFile.h"
#include "nativesharp/Target/TargetDataLayout.h"
#include "nativesharp/Target/TargetInfo.h"

#include <array>
#include <bit>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <ostream>
#include <set>
#include <sstream>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace nativesharp::driver
{
namespace
{
using target::BuiltinLayoutKind;

bool parseBoundedDecimal(
    std::string_view text, std::size_t maximum, std::size_t& value) noexcept
{
    if (text.empty()) return false;
    std::size_t parsed = 0;
    for (const unsigned char ch : text)
    {
        if (std::isdigit(ch) == 0) return false;
        const std::size_t digit = static_cast<std::size_t>(ch - '0');
        if (parsed > (maximum - digit) / 10U) return false;
        parsed = parsed * 10U + digit;
    }
    value = parsed;
    return true;
}

struct DriverExecutionScope final
{
    DriverExecutionScope(std::ostream& destination, std::ostream& diagnosticDestination)
        : output(destination), error(diagnosticDestination),
          started(std::chrono::steady_clock::now()) {}
    ~DriverExecutionScope()
    {
        if (o2Budget.invocations() != 0)
        {
            if (reportTiming) output << o2Budget.report() << '\n';
            if (o2Budget.exceeded())
            {
                error << "nsc: warning: O2 optimization budget exceeded; "
                      << o2Budget.report() << '\n';
            }
        }
        if (!reportTiming) return;
        for (const auto& [phase, elapsed] : phaseTimings)
        {
            output << "timing phase=" << phase << " us="
                   << std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count()
                   << '\n';
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started).count();
        output << "timing total_ms=" << elapsed << '\n';
    }
    void writeProgress(std::string_view message)
    {
        if (!reportProgress) return;
        const std::lock_guard<std::mutex> lock(progressMutex);
        output << "progress " << message << '\n';
        output.flush();
    }
    void recordPhase(std::string_view phase, std::chrono::nanoseconds elapsed)
    {
        if (!reportTiming) return;
        phaseTimings.emplace_back(std::string(phase), elapsed);
    }
    bool expired() const noexcept
    {
        return deadline.has_value() && std::chrono::steady_clock::now() >= *deadline;
    }
    std::ostream& output;
    std::ostream& error;
    std::chrono::steady_clock::time_point started;
    std::mutex progressMutex;
    std::optional<std::chrono::steady_clock::time_point> deadline;
    std::vector<std::pair<std::string, std::chrono::nanoseconds>> phaseTimings;
    llvm_backend::LlvmOptimizationBudgetTracker o2Budget;
    compilation::ProjectLoadOptions projectLoadOptions;
    bool reportTiming = false;
    bool reportProgress = false;
};

thread_local DriverExecutionScope* activeExecutionScope = nullptr;

const compilation::ProjectLoadOptions& activeProjectLoadOptions() noexcept
{
    static const compilation::ProjectLoadOptions defaults;
    return activeExecutionScope != nullptr
        ? activeExecutionScope->projectLoadOptions
        : defaults;
}

class DriverPhaseTimer final
{
public:
    explicit DriverPhaseTimer(std::string_view phase)
        : scope_(activeExecutionScope != nullptr &&
                         (activeExecutionScope->reportTiming || activeExecutionScope->reportProgress)
                     ? activeExecutionScope
                     : nullptr),
          phase_(phase),
          started_(scope_ != nullptr ? std::chrono::steady_clock::now()
                                     : std::chrono::steady_clock::time_point{})
    {
        if (scope_ != nullptr)
            scope_->writeProgress("phase=" + std::string(phase_) + " state=begin");
    }

    ~DriverPhaseTimer()
    {
        if (scope_ == nullptr) return;
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - started_);
        scope_->recordPhase(phase_, elapsed);
        scope_->writeProgress("phase=" + std::string(phase_) + " state=end elapsed_ms=" +
            std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()));
    }

private:
    DriverExecutionScope* scope_;
    std::string_view phase_;
    std::chrono::steady_clock::time_point started_;
};

template <typename Callback>
auto measureDriverPhase(std::string_view phase, Callback callback)
{
    const DriverPhaseTimer timing(phase);
    return callback();
}

void configureProjectExecution(frontend::ProjectCompilation& compilation)
{
    const CompilationJobPolicy* const jobs = activeCompilationJobPolicy();
    compilation.setCompilationJobs(jobs != nullptr ? jobs->effectiveJobs() : 1);
    compilation.setProjectLoadOptions(activeProjectLoadOptions());
    compilation.setCancellationCheck([] {
        return activeExecutionScope != nullptr && activeExecutionScope->expired();
    });
    if (activeExecutionScope != nullptr && activeExecutionScope->reportTiming)
    {
        DriverExecutionScope* const scope = activeExecutionScope;
        compilation.setPhaseTimingSink(
            [scope](std::string_view phase, std::chrono::nanoseconds elapsed) {
                scope->recordPhase(phase, elapsed);
            });
    }
    if (activeExecutionScope != nullptr && activeExecutionScope->reportProgress)
    {
        DriverExecutionScope* const scope = activeExecutionScope;
        compilation.setProgressSink([scope](std::string_view message) {
            scope->writeProgress(message);
        });
    }
}

std::size_t effectiveCompilationJobs() noexcept
{
    const CompilationJobPolicy* const jobs = activeCompilationJobPolicy();
    return jobs != nullptr ? jobs->effectiveJobs() : 1;
}

struct LayoutRow
{
    std::string_view name;
    BuiltinLayoutKind kind;
};

constexpr std::array<LayoutRow, 15> kLayoutRows = {{{"bool", BuiltinLayoutKind::Bool},
    {"byte", BuiltinLayoutKind::Byte},
    {"char", BuiltinLayoutKind::Char},
    {"short", BuiltinLayoutKind::Short},
    {"ushort", BuiltinLayoutKind::UShort},
    {"int", BuiltinLayoutKind::Int},
    {"uint", BuiltinLayoutKind::UInt},
    {"long", BuiltinLayoutKind::Long},
    {"ulong", BuiltinLayoutKind::ULong},
    {"nint", BuiltinLayoutKind::NInt},
    {"nuint", BuiltinLayoutKind::NUInt},
    {"float", BuiltinLayoutKind::Float},
    {"double", BuiltinLayoutKind::Double},
    {"enum", BuiltinLayoutKind::Enum},
    {"pointer", BuiltinLayoutKind::Pointer}}};

std::string foldedPathKey(const std::filesystem::path &path)
{
    const std::u8string encoded = path.lexically_normal().generic_u8string();
    std::string key(reinterpret_cast<const char *>(encoded.data()), encoded.size());
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return key;
}

struct PrecompiledExportContract final
{
    std::string moduleName;
    std::string runtimeName;
    std::string returnTypeKey;
    std::vector<std::string> parameterTypeKeys;
};

struct LoadedPrecompiledMetadata final
{
    std::filesystem::path ownerProjectFile;
    std::filesystem::path metadataPath;
    interop::LibraryMetadata metadata;
};

std::string foldedName(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::string_view subsystemDisplayName(linker::WindowsExecutableSubsystem subsystem)
{
    return subsystem == linker::WindowsExecutableSubsystem::Windows ? "Windows GUI" : "Windows CUI";
}

std::string escapedStringLiteral(std::string_view value)
{
    std::string result;
    result.push_back('"');
    for (const char character : value)
    {
        switch (character)
        {
            case '\\': result += "\\\\"; break;
            case '"': result += "\\\""; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            case '\0': result += "\\0"; break;
            default: result.push_back(character); break;
        }
    }
    result.push_back('"');
    return result;
}

bool sourceTypeForMetadataKey(std::string typeKey, std::string &sourceType, std::string &error)
{
    std::vector<std::string> modifiers;
    while (!typeKey.empty())
    {
        if (typeKey.ends_with("[]"))
        {
            modifiers.emplace_back("[]");
            typeKey.resize(typeKey.size() - 2);
            continue;
        }
        if (typeKey.ends_with("*"))
        {
            modifiers.emplace_back("*");
            typeKey.pop_back();
            continue;
        }
        if (typeKey.ends_with("]"))
        {
            const std::size_t open = typeKey.find_last_of('[');
            if (open == std::string::npos) break;
            modifiers.push_back(typeKey.substr(open));
            typeKey.resize(open);
            continue;
        }
        break;
    }

    if (typeKey.starts_with("builtin:"))
    {
        sourceType = typeKey.substr(8);
    }
    else if (typeKey.starts_with("type:"))
    {
        const std::string identity = typeKey.substr(5);
        const std::size_t moduleSeparator = identity.find("::");
        if (moduleSeparator == std::string::npos)
        {
            error = "metadata contains malformed named type key '" + typeKey + "'";
            return false;
        }
        sourceType = identity.substr(moduleSeparator + 2);
        std::replace(sourceType.begin(), sourceType.end(), '+', '.');
        if (sourceType.find('`') != std::string::npos)
        {
            error = "metadata-backed source import does not yet support open generic "
                    "type key '" +
                    typeKey + "'";
            return false;
        }
    }
    else
    {
        error = "metadata-backed source import cannot materialize type key '" + typeKey + "'";
        return false;
    }
    for (auto iterator = modifiers.rbegin(); iterator != modifiers.rend(); ++iterator)
    {
        sourceType += *iterator;
    }
    return true;
}

std::string ownershipPrefix(interop::LibraryMetadataOwnershipKind ownership)
{
    switch (ownership)
    {
        case interop::LibraryMetadataOwnershipKind::Owned: return "owned ";
        case interop::LibraryMetadataOwnershipKind::Borrowed: return "borrowed ";
        case interop::LibraryMetadataOwnershipKind::None: return {};
    }
    return {};
}

// DLL-P4 publication rule. A member that legal consumer source can name directly must have an
// importable entry. Public members qualify everywhere; protected members qualify through a
// consumer-owned derived class (including `base.Foo()`). Internal/private implementation details
// remain in producer-owned dispatch tables and must not be over-exported.
bool hasConsumerCallableAccessibility(const std::uint8_t accessibility) noexcept
{
    constexpr std::uint8_t kProtectedAccessibility = 1U;
    constexpr std::uint8_t kPublicAccessibility = 3U;
    return accessibility == kProtectedAccessibility || accessibility == kPublicAccessibility;
}

bool hasConsumerCallableAccessibility(const frontend::ClassMemberAccessibility accessibility) noexcept
{
    return accessibility == frontend::ClassMemberAccessibility::Protected ||
           accessibility == frontend::ClassMemberAccessibility::Public;
}

bool isConsumerCallableNativeMethod(const interop::LibraryMetadataMember &member) noexcept
{
    constexpr std::uint32_t kAbstract = 1U << 5U;
    constexpr std::uint32_t kExternC = 1U << 8U;
    constexpr std::uint32_t kExportedC = 1U << 9U;
    constexpr std::uint32_t kCompilerGenerated = 1U << 10U;
    constexpr std::uint32_t kExcluded = kAbstract | kCompilerGenerated;
    constexpr std::uint8_t kMethodKind = 2U;
    return member.kind == kMethodKind && hasConsumerCallableAccessibility(member.accessibility) &&
           (member.flags & kExcluded) == 0U && ((member.flags & kExternC) == 0U || (member.flags & kExportedC) != 0U) &&
           !member.mangledName.empty();
}

std::string metadataBaseTypeKey(std::string typeKey)
{
    while (!typeKey.empty())
    {
        if (typeKey.ends_with("[]"))
        {
            typeKey.resize(typeKey.size() - 2);
            continue;
        }
        if (typeKey.ends_with('*'))
        {
            typeKey.pop_back();
            continue;
        }
        if (typeKey.ends_with(']'))
        {
            const std::size_t open = typeKey.find_last_of('[');
            if (open != std::string::npos)
            {
                typeKey.resize(open);
                continue;
            }
        }
        break;
    }
    return typeKey;
}

// The metadata spelling of an open generic declaration carries CLR-style arity
// (`Module::Namespace.Type`1`).  That spelling is intentionally rejected by
// sourceTypeForMetadataKey(): it is not a complete source type until arguments
// have been supplied.  The parser's declaration lookahead needs less
// information, however; it only needs to know that `Type` can begin a type.
// Keep that lexical concern separate from metadata type materialization so a
// reconstructed template can name a generic base that is emitted later in the
// same source buffer.
void appendParserKnownTypeNames(
    const interop::LibraryMetadata &metadata,
    const std::string &writtenTypeKey,
    std::vector<std::string> &knownTypeNames)
{
    const std::string typeKey = metadataBaseTypeKey(writtenTypeKey);
    if (!typeKey.starts_with("type:")) return;

    const std::string_view identity(typeKey.data() + 5U, typeKey.size() - 5U);
    const std::size_t moduleSeparator = identity.find("::");
    if (moduleSeparator == std::string_view::npos || moduleSeparator + 2U == identity.size()) return;

    std::string sourceName;
    sourceName.reserve(identity.size() - moduleSeparator - 2U);
    bool skippingArity = false;
    for (const char character : identity.substr(moduleSeparator + 2U))
    {
        if (character == '`')
        {
            skippingArity = true;
            continue;
        }
        if (skippingArity && character >= '0' && character <= '9') continue;
        skippingArity = false;
        sourceName.push_back(character == '+' ? '.' : character);
    }
    if (sourceName.empty()) return;

    knownTypeNames.push_back(sourceName);
    const std::size_t terminalSeparator = sourceName.find_last_of('.');
    knownTypeNames.push_back(terminalSeparator == std::string::npos
        ? sourceName
        : sourceName.substr(terminalSeparator + 1U));

    const std::string_view module = identity.substr(0, moduleSeparator);
    if (!module.empty() && module != metadata.moduleName)
        knownTypeNames.push_back(std::string(module) + "::" + sourceName);
}

const interop::LibraryMetadataType *metadataTypeForKey(
    const interop::LibraryMetadata &metadata, const std::string &typeKey)
{
    const std::string base = metadataBaseTypeKey(typeKey);
    if (!base.starts_with("type:")) return nullptr;
    const std::string_view identity(base.data() + 5, base.size() - 5);
    const auto found = std::lower_bound(
        metadata.types.begin(), metadata.types.end(), identity,
        [](const interop::LibraryMetadataType &candidate, std::string_view value) {
            return candidate.identityKey < value;
        });
    return found == metadata.types.end() || found->identityKey != identity ? nullptr : &*found;
}

std::optional<std::string> metadataSpecializedTypeKey(
    const interop::LibraryMetadataGenericSpecialization &specialization)
{
    constexpr std::uint8_t kGenericMethodKind = 3U;
    if (specialization.kind == kGenericMethodKind || specialization.templateKey.empty() ||
        specialization.specializedName.empty())
    {
        return std::nullopt;
    }
    const std::size_t moduleSeparator = specialization.templateKey.find("::");
    if (moduleSeparator == std::string::npos) return std::nullopt;
    const std::size_t terminal = specialization.templateKey.find_last_of(".+");
    const std::size_t terminalBegin = terminal == std::string::npos || terminal < moduleSeparator + 2U
        ? moduleSeparator + 2U
        : terminal + 1U;
    if (specialization.templateKey.find('`', terminalBegin) == std::string::npos) return std::nullopt;
    return "type:" + specialization.templateKey.substr(0, terminalBegin) + specialization.specializedName;
}

std::optional<std::string> metadataTypeModule(const std::string &writtenTypeKey)
{
    const std::string typeKey = metadataBaseTypeKey(writtenTypeKey);
    if (!typeKey.starts_with("type:")) return std::nullopt;
    const std::size_t separator = typeKey.find("::", 5U);
    if (separator == std::string::npos || separator == 5U) return std::nullopt;
    return typeKey.substr(5U, separator - 5U);
}

bool metadataProvidesType(
    const interop::LibraryMetadata &metadata, const std::string &writtenTypeKey)
{
    const std::string typeKey = metadataBaseTypeKey(writtenTypeKey);
    if (metadataTypeForKey(metadata, typeKey) != nullptr) return true;
    return std::ranges::any_of(metadata.genericSpecializations, [&](const auto &specialization) {
        const std::optional<std::string> candidate = metadataSpecializedTypeKey(specialization);
        return candidate.has_value() && *candidate == typeKey;
    });
}

bool validatePrecompiledExternalTypeClosure(
    std::span<const LoadedPrecompiledMetadata> loaded, std::string &error)
{
    std::unordered_map<std::string, const interop::LibraryMetadata *> modules;
    for (const LoadedPrecompiledMetadata &entry : loaded)
        modules.emplace(entry.metadata.moduleName, &entry.metadata);

    std::set<std::string> failures;
    for (const LoadedPrecompiledMetadata &entry : loaded)
    {
        for (const std::string &typeKey : entry.metadata.externalTypeKeys)
        {
            const std::optional<std::string> owner = metadataTypeModule(typeKey);
            if (!owner.has_value())
            {
                failures.insert("module '" + entry.metadata.moduleName +
                    "' records malformed external type '" + typeKey + "'");
                continue;
            }
            const auto provider = modules.find(*owner);
            if (provider == modules.end())
            {
                failures.insert("module '" + entry.metadata.moduleName + "' requires external type '" +
                    typeKey + "', but LibraryReference module '" + *owner + "' is not present");
                continue;
            }
            if (!metadataProvidesType(*provider->second, typeKey))
            {
                failures.insert("module '" + entry.metadata.moduleName + "' requires external type '" +
                    typeKey + "', but provider module '" + *owner + "' does not declare it");
            }
        }
    }
    if (failures.empty()) return true;
    error = "precompiled LibraryReference type closure is incomplete";
    for (const std::string &failure : failures) error += "\n  " + failure;
    return false;
}

bool sourceTypeForGenericSpecialization(
    const interop::LibraryMetadata &metadata,
    const interop::LibraryMetadataGenericSpecialization &specialization,
    std::string &sourceType,
    std::string &error);

bool sourceTypeForMetadataKey(
    const interop::LibraryMetadata &metadata,
    const std::string &typeKey,
    std::string &sourceType,
    std::string &error)
{
    const std::string baseTypeKey = metadataBaseTypeKey(typeKey);
    if (metadataTypeForKey(metadata, baseTypeKey) == nullptr)
    {
        const auto specialization = std::ranges::find_if(
            metadata.genericSpecializations,
            [&](const auto &candidate) {
                const std::optional<std::string> candidateTypeKey = metadataSpecializedTypeKey(candidate);
                return candidateTypeKey.has_value() && *candidateTypeKey == baseTypeKey;
            });
        if (specialization != metadata.genericSpecializations.end())
        {
            if (!sourceTypeForGenericSpecialization(metadata, *specialization, sourceType, error)) return false;
            sourceType += typeKey.substr(baseTypeKey.size());
            return true;
        }
    }
    if (!sourceTypeForMetadataKey(typeKey, sourceType, error)) return false;

    // A canonical NSMETA key owns more identity than its C#-style namespace spelling:
    // `type:Module::Namespace.Type`. The public-source bridge used to discard `Module::`, which
    // is harmless only while exactly one visible module declares that name. Store templates are
    // commonly visible both from their source ProjectReference and from a LibraryReference's
    // materialized template closure, so the unqualified spelling can bind to two templates. Keep
    // the module for every external named type; declarations owned by this metadata module remain
    // ordinary source names so their reconstructed bodies can refer to one another naturally.
    if (baseTypeKey.starts_with("type:"))
    {
        const std::string_view identity(baseTypeKey.data() + 5U, baseTypeKey.size() - 5U);
        const std::size_t moduleSeparator = identity.find("::");
        if (moduleSeparator != std::string_view::npos &&
            identity.substr(0, moduleSeparator) != metadata.moduleName)
        {
            sourceType.insert(0, std::string(identity.substr(0, moduleSeparator)) + "::");
        }
    }
    return true;
}

bool sourceTypeForGenericSpecialization(
    const interop::LibraryMetadata &metadata,
    const interop::LibraryMetadataGenericSpecialization &specialization,
    std::string &sourceType,
    std::string &error)
{
    constexpr std::uint8_t kGenericMethodKind = 3U;
    if (specialization.kind == kGenericMethodKind)
    {
        error = "metadata method specialization cannot be used as a type";
        return false;
    }
    const std::size_t moduleSeparator = specialization.templateKey.find("::");
    if (moduleSeparator == std::string::npos)
    {
        error = "metadata contains malformed generic template key '" + specialization.templateKey + "'";
        return false;
    }
    const std::size_t terminal = specialization.templateKey.find_last_of(".+");
    const std::size_t terminalBegin = terminal == std::string::npos || terminal < moduleSeparator + 2U
        ? moduleSeparator + 2U
        : terminal + 1U;
    const std::size_t arity = specialization.templateKey.find('`', terminalBegin);
    if (arity == std::string::npos || specialization.typeArguments.empty())
    {
        error = "metadata contains an incomplete closed generic specialization '" +
            specialization.specializationKey + "'";
        return false;
    }

    const std::string_view templateModule(specialization.templateKey.data(), moduleSeparator);
    sourceType = specialization.templateKey.substr(moduleSeparator + 2U, arity - (moduleSeparator + 2U));
    std::replace(sourceType.begin(), sourceType.end(), '+', '.');
    if (templateModule != metadata.moduleName)
        sourceType.insert(0, std::string(templateModule) + "::");
    sourceType.push_back('<');
    for (std::size_t index = 0; index < specialization.typeArguments.size(); ++index)
    {
        if (index != 0U) sourceType += ", ";
        const std::string &argument = specialization.typeArguments[index];
        std::string argumentType;
        if (argument.starts_with("generic:"))
        {
            const std::string_view requested(argument.data() + 8U, argument.size() - 8U);
            const auto nested = std::ranges::find_if(
                metadata.genericSpecializations,
                [&](const auto &candidate) { return requested == candidate.specializationKey; });
            if (nested == metadata.genericSpecializations.end() ||
                !sourceTypeForGenericSpecialization(metadata, *nested, argumentType, error))
            {
                if (nested == metadata.genericSpecializations.end())
                    error = "metadata generic specialization '" + specialization.specializationKey +
                        "' refers to missing nested specialization '" + std::string(requested) + "'";
                return false;
            }
        }
        else if (argument.starts_with("type:") || argument.starts_with("builtin:"))
        {
            if (!sourceTypeForMetadataKey(metadata, argument, argumentType, error)) return false;
        }
        else
        {
            // NSMETA 1.7-1.11 stored readable spellings.  Builtins remain valid as written; the
            // producer-side fix now stores canonical named identities for cross-module demands.
            argumentType = argument;
        }
        sourceType += argumentType;
    }
    sourceType.push_back('>');
    return true;
}

bool isPubliclyReachableNamedType(
    std::span<const frontend::NamedTypeDeclaration> types,
    std::size_t typeOrdinal) noexcept
{
    while (typeOrdinal < types.size())
    {
        const frontend::NamedTypeDeclaration &type = types[typeOrdinal];
        if (type.accessibility != frontend::NamedTypeAccessibility::Public) return false;
        if (!type.containingTypeOrdinal.has_value()) return true;
        typeOrdinal = *type.containingTypeOrdinal;
    }
    return false;
}

bool isPubliclyReachableMetadataType(
    const interop::LibraryMetadata &metadata,
    const interop::LibraryMetadataType &type) noexcept
{
    constexpr std::uint8_t kPublicAccessibility = 3U;
    const interop::LibraryMetadataType *current = &type;
    while (current != nullptr)
    {
        if (current->accessibility != kPublicAccessibility) return false;
        const std::size_t separator = current->fullyQualifiedName.find_last_of('+');
        if (separator == std::string::npos) return true;
        const std::string parentName = current->fullyQualifiedName.substr(0, separator);
        current = metadataTypeForKey(
            metadata, "type:" + metadata.moduleName + "::" + parentName);
        if (current == nullptr) return false;
    }
    return false;
}

bool isPublishedNativeConstructor(
    const interop::LibraryMetadata &metadata, const interop::LibraryMetadataMember &member) noexcept
{
    constexpr std::uint8_t kConstructorKind = 3U;
    constexpr std::uint8_t kPublicAccessibility = 3U;
    if (member.kind != kConstructorKind || !hasConsumerCallableAccessibility(member.accessibility) ||
        member.mangledName.empty())
    {
        return false;
    }
    const interop::LibraryMetadataType *declaringType = metadataTypeForKey(metadata, member.declaringTypeKey);
    return declaringType != nullptr && declaringType->accessibility == kPublicAccessibility &&
        isPubliclyReachableMetadataType(metadata, *declaringType);
}

bool canMaterializeContractType(
    const interop::LibraryMetadata &metadata, const std::string &typeKey, std::unordered_set<std::string> &visiting)
{
    const std::string base = metadataBaseTypeKey(typeKey);
    if (base.starts_with("builtin:")) return true;
    const interop::LibraryMetadataType *type = metadataTypeForKey(metadata, base);
    if (type == nullptr)
    {
        // A public signature may name a type owned by another LibraryReference. NSMETA keeps
        // ownership honest by storing only the producer's own declarations, so absence here is
        // not evidence that the signature cannot be reconstructed. The generated declaration
        // spells the canonical qualified name from the type key; the combined consumer graph is
        // then responsible for resolving that name to the other metadata module.
        std::string ignoredSourceType;
        std::string ignoredError;
        return sourceTypeForMetadataKey(metadata, typeKey, ignoredSourceType, ignoredError);
    }
    if (type->fullyQualifiedName.find('`') != std::string::npos ||
        (type->kind != interop::LibraryMetadataTypeKind::Class &&
            type->kind != interop::LibraryMetadataTypeKind::Interface &&
            type->kind != interop::LibraryMetadataTypeKind::Struct &&
            type->kind != interop::LibraryMetadataTypeKind::Enum &&
            type->kind != interop::LibraryMetadataTypeKind::Delegate))
    {
        return false;
    }
    if (type->kind == interop::LibraryMetadataTypeKind::Enum) return true;
    if (type->kind == interop::LibraryMetadataTypeKind::Delegate)
    {
        const bool externC = (type->delegateFlags & 1U) != 0U;
        // NSMETA 1.5 hizo reconstruible el puntero de funcion C. Un delegate
        // ordinario necesita una garantía adicional: que productor y consumidor
        // hablan la misma celda code/context/ destroy/reserved. Esa garantía
        // aparece por primera vez en el contrato 1.8.
        const bool managedAbiKnown = metadata.formatMinor >= 8 &&
                                     metadata.delegateAbiVersion == interop::kDelegateAbiVersion &&
                                     metadata.managedDelegateCellSize == interop::kManagedDelegateCellSize;
        if (metadata.formatMinor < 5 || (!externC && !managedAbiKnown) ||
            type->delegateReturnTypeKey.empty())
        {
            return false;
        }
        // Re-entering a delegate is a completed nominal edge, not an invalid signature.  A
        // callback can receive a pointer to a struct that stores that same callback type (the
        // ImDrawCallback -> ImDrawCmd* -> ImDrawCallback shape).  Struct recursion below already
        // accepts this closure; rejecting it only at the delegate node made otherwise complete
        // NSMETA methods disappear from the materialized contract and left their physical DLL
        // exports looking unexpected.
        if (!visiting.insert(type->identityKey).second) return true;
        bool valid = canMaterializeContractType(metadata, type->delegateReturnTypeKey, visiting);
        for (const std::string &parameterTypeKey : type->delegateParameterTypeKeys)
            valid = canMaterializeContractType(metadata, parameterTypeKey, visiting) && valid;
        visiting.erase(type->identityKey);
        return valid;
    }
    if (type->kind == interop::LibraryMetadataTypeKind::Class ||
        type->kind == interop::LibraryMetadataTypeKind::Interface)
    {
        // Precompiled classes and interfaces are opaque native references in a
        // signature, not values copied by layout. The same is true when the
        // reference is wrapped in a NativeSharp array: source reconstruction
        // preserves the [] suffix while ABI validation remains anchored to the
        // publicly reachable element contract. Interfaces additionally have to
        // be publicly reachable because admitting a hidden contract here would
        // make the physical DLL surface broader than the reconstructible NSMETA
        // surface. A trailing pointer is accepted because ref/out/in parameters
        // use it internally.
        constexpr std::uint8_t kPublicAccessibility = 3U;
        if (type->kind == interop::LibraryMetadataTypeKind::Interface &&
            (type->accessibility != kPublicAccessibility || !isPubliclyReachableMetadataType(metadata, *type)))
        {
            return false;
        }
        if (type->referenceSize != metadata.pointerSize || type->referenceAlignment != metadata.pointerAlignment)
        {
            return false;
        }
        return true;
    }
    if (!visiting.insert(type->identityKey).second) return true;
    for (const interop::LibraryMetadataField &field : type->fields)
    {
        if (!canMaterializeContractType(metadata, field.typeKey, visiting))
        {
            visiting.erase(type->identityKey);
            return false;
        }
    }
    visiting.erase(type->identityKey);
    return true;
}

bool canMaterializeContractType(const interop::LibraryMetadata &metadata, const std::string &typeKey)
{
    std::unordered_set<std::string> visiting;
    return canMaterializeContractType(metadata, typeKey, visiting);
}

bool canMaterializeNativeMethod(const interop::LibraryMetadata &metadata, const interop::LibraryMetadataMember &member)
{
    if (!isConsumerCallableNativeMethod(member)) return false;
    constexpr std::uint32_t kStatic = 1U << 0U;
    constexpr std::uint8_t kPublicAccessibility = 3U;
    const interop::LibraryMetadataType *declaringType = metadataTypeForKey(metadata, member.declaringTypeKey);
    if (declaringType == nullptr || declaringType->accessibility != kPublicAccessibility ||
        !isPubliclyReachableMetadataType(metadata, *declaringType)) return false;
    if ((member.flags & kStatic) == 0U && declaringType->kind != interop::LibraryMetadataTypeKind::Struct &&
        declaringType->kind != interop::LibraryMetadataTypeKind::Class)
    {
        return false;
    }
    // NSMETA already carries the complete layout for structs, the underlying type
    // for enums and the pointer-sized reference carrier for opaque classes. Admit
    // a method only when every nominal type in its signature can be reconstructed
    // under those exact rules.
    if (!canMaterializeContractType(metadata, member.declaredTypeKey)) return false;
    if (std::ranges::any_of(member.parameters,
            [&](const auto &parameter) { return !canMaterializeContractType(metadata, parameter.typeKey); }))
    {
        return false;
    }
    if (metadata.formatMinor >= 4) return true;
    // NSMETA <=1.3 encoded the pointer carrier but not whether the source said
    // ref/out/in.
    return member.signatureKey.find("ref ") == std::string::npos &&
           member.signatureKey.find("out ") == std::string::npos &&
           member.signatureKey.find("in ") == std::string::npos;
}

bool canMaterializeConstructor(const interop::LibraryMetadata &metadata, const interop::LibraryMetadataMember &member)
{
    constexpr std::uint8_t kConstructorKind = 3U;
    if (member.kind != kConstructorKind || !hasConsumerCallableAccessibility(member.accessibility) ||
        member.mangledName.empty())
    {
        return false;
    }
    const interop::LibraryMetadataType *declaringType = metadataTypeForKey(metadata, member.declaringTypeKey);
    if (declaringType == nullptr ||
        (declaringType->kind != interop::LibraryMetadataTypeKind::Class &&
            declaringType->kind != interop::LibraryMetadataTypeKind::Struct))
    {
        return false;
    }
    if (declaringType->kind == interop::LibraryMetadataTypeKind::Struct)
    {
        // Value-type construction calls the constructor symbol directly. Unlike a
        // class allocation it does not require a descriptor accessor, so rejecting
        // it here silently removed a constructor that was already present in NSMETA
        // and in the producer's physical export set.
        return std::ranges::all_of(member.parameters,
            [&](const auto &parameter) { return canMaterializeContractType(metadata, parameter.typeKey); });
    }
    const auto dispatch = std::ranges::find_if(
        metadata.dispatchTables, [&](const auto &candidate) { return candidate.typeKey == member.declaringTypeKey; });
    if (dispatch == metadata.dispatchTables.end() || dispatch->descriptorAccessorName.empty())
    {
        // NSMETA <=1.5 remains readable and its ordinary methods remain consumable,
        // but it did not publish a PE-safe way to obtain the class descriptor, so
        // `new` is intentionally not materialized for that predecessor.
        return false;
    }
    return std::ranges::all_of(member.parameters,
        [&](const auto &parameter) { return canMaterializeContractType(metadata, parameter.typeKey); });
}

bool canMaterializePrecompiledDeclaration(
    const interop::LibraryMetadata &metadata, const interop::LibraryMetadataMember &member)
{
    constexpr std::uint8_t kFieldKind = 0U;
    constexpr std::uint8_t kEventKind = 1U;
    constexpr std::uint8_t kMethodKind = 2U;
    constexpr std::uint8_t kConstructorKind = 3U;
    constexpr std::uint8_t kDestructorKind = 4U;
    constexpr std::uint32_t kAbstract = 1U << 5U;
    constexpr std::uint32_t kExternC = 1U << 8U;
    constexpr std::uint32_t kExportedC = 1U << 9U;
    constexpr std::uint32_t kCompilerGenerated = 1U << 10U;
    constexpr std::uint32_t kImportedC = 1U << 20U;

    const interop::LibraryMetadataType *declaringType = metadataTypeForKey(metadata, member.declaringTypeKey);
    if (declaringType == nullptr) return false;

    if (member.kind == kFieldKind)
    {
        constexpr std::uint32_t kStatic = 1U << 0U;
        constexpr std::uint32_t kConst = 1U << 2U;
        const bool materializable = hasConsumerCallableAccessibility(member.accessibility) &&
            canMaterializeContractType(metadata, member.declaredTypeKey);
        if ((member.flags & kConst) != 0U) return materializable && member.constantKind != 0U;
        return materializable && (member.flags & kStatic) != 0U && !member.mangledName.empty();
    }
    if (member.kind == kEventKind)
    {
        return hasConsumerCallableAccessibility(member.accessibility) &&
               canMaterializeContractType(metadata, member.declaredTypeKey);
    }
    if (member.kind == kConstructorKind) return canMaterializeConstructor(metadata, member);
    if (member.kind == kDestructorKind)
    {
        return declaringType->kind == interop::LibraryMetadataTypeKind::Class && !member.mangledName.empty();
    }
    if (member.kind != kMethodKind || (member.flags & kCompilerGenerated) != 0U)
    {
        return false;
    }
    // A plain `extern C` declaration belongs to the producer's native implementation. Its raw
    // support symbol can remain undefined in a static archive and is intentionally not exported
    // by a DLL. Reconstructing it as an ordinary precompiled NativeSharp method invents a mangled
    // symbol that neither product publishes. Only `export extern C` is part of the consumable
    // NativeSharp surface (and keeps its ordinary NativeSharp entry alongside the C ABI thunk).
    // An imported C declaration is different: its owning DLL is part of NSMETA 1.10 and the
    // consumer must recreate that external import instead of looking for it in the producer.
    if ((member.flags & kExternC) != 0U &&
        (member.flags & (kExportedC | kImportedC)) == 0U)
    {
        return false;
    }

    if (!hasConsumerCallableAccessibility(member.accessibility) &&
        declaringType->kind != interop::LibraryMetadataTypeKind::Interface)
    {
        return false;
    }
    if ((member.flags & kAbstract) == 0U && member.mangledName.empty()) return false;
    if (!canMaterializeContractType(metadata, member.declaredTypeKey)) return false;
    return std::ranges::all_of(member.parameters,
        [&](const auto &parameter) { return canMaterializeContractType(metadata, parameter.typeKey); });
}

void collectMaterializedContractTypes(
    const interop::LibraryMetadata &metadata, const std::string &typeKey, std::set<std::string> &identities)
{
    const interop::LibraryMetadataType *type = metadataTypeForKey(metadata, typeKey);
    if (type == nullptr || !identities.insert(type->identityKey).second) return;
    if (type->kind == interop::LibraryMetadataTypeKind::Struct)
    {
        for (const interop::LibraryMetadataField &field : type->fields)
            collectMaterializedContractTypes(metadata, field.typeKey, identities);
    }
    else if (type->kind == interop::LibraryMetadataTypeKind::Class ||
             type->kind == interop::LibraryMetadataTypeKind::Interface)
    {
        // Una clase no es materializable sólo por su carrier ptr: su identidad RTTI
        // depende de la cadena de bases e interfaces. Esto es especialmente visible
        // al importar una excepción: el catch necesita reconstruir también el
        // ancestro que demuestra que es lanzable.
        if (!type->baseTypeKey.empty()) collectMaterializedContractTypes(metadata, type->baseTypeKey, identities);
        for (const std::string &interfaceTypeKey : type->interfaceTypeKeys)
            collectMaterializedContractTypes(metadata, interfaceTypeKey, identities);
    }
    else if (type->kind == interop::LibraryMetadataTypeKind::Delegate)
    {
        collectMaterializedContractTypes(metadata, type->delegateReturnTypeKey, identities);
        for (const std::string &parameterTypeKey : type->delegateParameterTypeKeys)
            collectMaterializedContractTypes(metadata, parameterTypeKey, identities);
    }
}

std::string_view sourceAccessibility(std::uint8_t accessibility) noexcept
{
    switch (accessibility)
    {
        case 0U: return "private";
        case 1U: return "protected";
        case 2U: return "internal";
        case 3U: return "public";
        default: return "private";
    }
}

const interop::LibraryMetadataMember *metadataFieldMember(
    const interop::LibraryMetadata &metadata, const interop::LibraryMetadataType &type, std::string_view name)
{
    constexpr std::uint8_t kFieldKind = 0U;
    const std::string typeKey = "type:" + type.identityKey;
    auto found = std::lower_bound(
        metadata.members.begin(), metadata.members.end(), typeKey,
        [](const interop::LibraryMetadataMember &member, const std::string &value) {
            return member.declaringTypeKey < value;
        });
    for (; found != metadata.members.end() && found->declaringTypeKey == typeKey; ++found)
    {
        if (found->kind == kFieldKind && found->name == name) return &*found;
    }
    return nullptr;
}

const interop::LibraryMetadataMember *metadataEventForBacking(
    const interop::LibraryMetadata &metadata,
    const interop::LibraryMetadataType &type,
    const interop::LibraryMetadataField &field)
{
    constexpr std::uint8_t kEventKind = 1U;
    constexpr std::uint32_t kCompilerGenerated = 1U << 10U;
    if ((field.flags & kCompilerGenerated) == 0U) return nullptr;
    const std::string declaringTypeKey = "type:" + type.identityKey;
    auto found = std::lower_bound(
        metadata.members.begin(), metadata.members.end(), declaringTypeKey,
        [](const interop::LibraryMetadataMember &member, const std::string &value) {
            return member.declaringTypeKey < value;
        });
    for (; found != metadata.members.end() && found->declaringTypeKey == declaringTypeKey; ++found)
    {
        if (found->kind == kEventKind && found->declaredTypeKey == field.typeKey &&
            field.name == "__nsc_event_" + found->name + "$stage42") return &*found;
    }
    return nullptr;
}

std::string enumConstantSource(const interop::LibraryMetadataType &type, std::uint64_t bits)
{
    const bool signedUnderlying =
        type.enumUnderlyingTypeKey == "builtin:sbyte" || type.enumUnderlyingTypeKey == "builtin:short" ||
        type.enumUnderlyingTypeKey == "builtin:int" || type.enumUnderlyingTypeKey == "builtin:long" ||
        type.enumUnderlyingTypeKey == "builtin:nint";
    if (signedUnderlying) return std::to_string(std::bit_cast<std::int64_t>(bits));
    return std::to_string(bits);
}

bool metadataConstantSource(
    const interop::LibraryMetadataMember &member, std::string &source, std::string &error)
{
    switch (member.constantKind)
    {
        case 1U: {
            const auto signedValue = [&](const unsigned width) {
                if (width == 64U) return std::bit_cast<std::int64_t>(member.constantBits);
                const std::uint64_t sign = std::uint64_t{1} << (width - 1U);
                const std::uint64_t mask = (std::uint64_t{1} << width) - 1U;
                const std::uint64_t value = member.constantBits & mask;
                return static_cast<std::int64_t>((value ^ sign) - sign);
            };
            if (member.declaredTypeKey == "builtin:sbyte") source = std::to_string(signedValue(8));
            else if (member.declaredTypeKey == "builtin:short") source = std::to_string(signedValue(16));
            else if (member.declaredTypeKey == "builtin:int") source = std::to_string(signedValue(32));
            else if (member.declaredTypeKey == "builtin:long" || member.declaredTypeKey == "builtin:nint")
                source = std::to_string(signedValue(64)) + "l";
            else
            {
                source = std::to_string(member.constantBits);
                if (member.declaredTypeKey == "builtin:uint") source += 'u';
                else if (member.declaredTypeKey == "builtin:ulong" || member.declaredTypeKey == "builtin:nuint")
                    source += "ul";
            }
            return true;
        }
        case 3U: source = member.constantBits == 0U ? "false" : "true"; return true;
        case 4U: {
            std::ostringstream stream;
            stream << std::setprecision(std::numeric_limits<float>::max_digits10)
                   << std::bit_cast<float>(static_cast<std::uint32_t>(member.constantBits));
            source = stream.str();
            if (source.find_first_of(".eE") == std::string::npos) source += ".0";
            source += 'f';
            return true;
        }
        case 5U: {
            std::ostringstream stream;
            stream << std::setprecision(std::numeric_limits<double>::max_digits10)
                   << std::bit_cast<double>(member.constantBits);
            source = stream.str();
            if (source.find_first_of(".eE") == std::string::npos) source += ".0";
            return true;
        }
        case 6U: source = escapedStringLiteral(member.constantText); return true;
        default:
            error = "metadata constant '" + member.name + "' has an unsupported value kind";
            return false;
    }
}

std::string_view metadataOperatorToken(std::string_view name) noexcept
{
    if (name == "op_UnaryPlus" || name == "op_Addition") return "+";
    if (name == "op_UnaryNegation" || name == "op_Subtraction") return "-";
    if (name == "op_Multiply") return "*";
    if (name == "op_Division") return "/";
    if (name == "op_Modulus") return "%";
    if (name == "op_LogicalNot") return "!";
    if (name == "op_OnesComplement") return "~";
    if (name == "op_BitwiseAnd") return "&";
    if (name == "op_BitwiseOr") return "|";
    if (name == "op_ExclusiveOr") return "^";
    if (name == "op_LeftShift") return "<<";
    if (name == "op_RightShift") return ">>";
    if (name == "op_Equality") return "==";
    if (name == "op_Inequality") return "!=";
    if (name == "op_LessThan") return "<";
    if (name == "op_LessThanOrEqual") return "<=";
    if (name == "op_GreaterThan") return ">";
    if (name == "op_GreaterThanOrEqual") return ">=";
    return {};
}

std::string_view parameterPassPrefix(interop::LibraryMetadataParameterPassKind passKind) noexcept
{
    switch (passKind)
    {
        case interop::LibraryMetadataParameterPassKind::Value: return {};
        case interop::LibraryMetadataParameterPassKind::Ref: return "ref ";
        case interop::LibraryMetadataParameterPassKind::Out: return "out ";
        case interop::LibraryMetadataParameterPassKind::In: return "in ";
    }
    return {};
}

std::vector<std::string> publishedNativeSymbols(const interop::LibraryMetadata &metadata)
{
    std::vector<std::string> symbols;
    for (const interop::LibraryMetadataMember &member : metadata.members)
    {
        // Physical validation and source materialization answer different
        // questions. The verifier must account for every symbol the producer
        // exported, including struct constructors that a precompiled consumer
        // does not reconstruct as `new` declarations.
        if (isConsumerCallableNativeMethod(member) && canMaterializeNativeMethod(metadata, member))
        {
            symbols.push_back(member.mangledName);
        }
        else if (isPublishedNativeConstructor(metadata, member))
        {
            symbols.push_back(member.mangledName);
        }
        else if (member.kind == 0U && (member.flags & (1U << 0U)) != 0U &&
                 (member.flags & (1U << 2U)) == 0U &&
                 hasConsumerCallableAccessibility(member.accessibility) &&
                 !member.mangledName.empty())
        {
            constexpr std::uint8_t kPublicAccessibility = 3U;
            const interop::LibraryMetadataType *declaringType =
                metadataTypeForKey(metadata, member.declaringTypeKey);
            // A member cannot be more visible than its declaring type. Public fields inside an
            // internal or privately nested type remain producer implementation details and must
            // not become required archive/DLL symbols for consumers.
            if (declaringType != nullptr && declaringType->accessibility == kPublicAccessibility &&
                isPubliclyReachableMetadataType(metadata, *declaringType))
            {
                symbols.push_back(member.mangledName);
            }
        }
    }
    constexpr std::uint8_t kPublicAccessibility = 3U;
    for (const interop::LibraryMetadataDispatch &dispatch : metadata.dispatchTables)
    {
        const interop::LibraryMetadataType *type = metadataTypeForKey(metadata, dispatch.typeKey);
        if (type != nullptr && type->accessibility == kPublicAccessibility &&
            (type->flags & (1U << 3U)) == 0U &&
            isPubliclyReachableMetadataType(metadata, *type) && !dispatch.descriptorAccessorName.empty())
        {
            symbols.push_back(dispatch.descriptorAccessorName);
        }
    }
    std::sort(symbols.begin(), symbols.end());
    symbols.erase(std::unique(symbols.begin(), symbols.end()), symbols.end());
    return symbols;
}

std::vector<std::string> metadataRequiredSymbols(const interop::LibraryMetadata &metadata)
{
    std::vector<std::string> symbols = publishedNativeSymbols(metadata);
    symbols.reserve(symbols.size() + metadata.exports.size());
    for (const interop::LibraryMetadataExport &exportRecord : metadata.exports)
    {
        symbols.push_back(exportRecord.cAbiName);
    }
    std::sort(symbols.begin(), symbols.end());
    symbols.erase(std::unique(symbols.begin(), symbols.end()), symbols.end());
    return symbols;
}

std::vector<std::string> missingSymbols(std::span<const std::string> expected, std::span<const std::string> actual)
{
    const std::unordered_set<std::string> actualSet(actual.begin(), actual.end());
    std::vector<std::string> missing;
    for (const std::string &symbol : expected)
    {
        if (!actualSet.contains(symbol)) missing.push_back(symbol);
    }
    return missing;
}

std::string describeSymbols(std::span<const std::string> symbols)
{
    std::ostringstream description;
    constexpr std::size_t kDisplayedLimit = 12;
    const std::size_t displayed = std::min(symbols.size(), kDisplayedLimit);
    for (std::size_t index = 0; index < displayed; ++index)
    {
        if (index != 0) description << ", ";
        description << '\'' << symbols[index] << '\'';
    }
    if (symbols.size() > displayed) description << ", ... (" << (symbols.size() - displayed) << " more)";
    return description.str();
}

void printMetadataAbiContract(const interop::LibraryMetadata &metadata, std::ostream &output)
{
    if (metadata.formatMinor < 8)
    {
        output << "abi-contract: legacy implicit contract (NSMETA " << metadata.formatMajor << '.'
               << metadata.formatMinor << ")\n";
        return;
    }
    output << "native-abi: " << metadata.nativeAbiMajor << '.' << metadata.nativeAbiMinor << '\n'
           << "runtime-abi: " << metadata.runtimeAbiMajor << '.' << metadata.runtimeAbiMinor << '\n'
           << "mangling-abi: " << metadata.manglingAbiVersion << '\n'
           << "object-abi: " << metadata.objectAbiVersion << '\n'
           << "delegate-abi: " << metadata.delegateAbiVersion << '\n'
           << "delegate-cell-size: " << metadata.managedDelegateCellSize << '\n'
           << "exception-abi: " << metadata.exceptionAbiVersion << '\n';
}

bool sourceDefaultValue(const interop::LibraryMetadataParameter &parameter, std::string &source, std::string &error)
{
    using Kind = interop::LibraryMetadataDefaultValueKind;
    switch (parameter.defaultValueKind)
    {
        case Kind::None: source.clear(); return true;
        case Kind::Null: source = "null"; return true;
        case Kind::Boolean: source = parameter.defaultValueBits == 0 ? "false" : "true"; return true;
        case Kind::Integer:
            source = (parameter.defaultValueFlags & 1U) != 0U ? "-" : "";
            source += std::to_string(parameter.defaultValueBits);
            return true;
        case Kind::Floating: {
            std::ostringstream stream;
            stream << std::setprecision(std::numeric_limits<double>::max_digits10)
                   << std::bit_cast<double>(parameter.defaultValueBits);
            source = stream.str();
            if (source.find_first_of(".eE") == std::string::npos) source += ".0";
            return true;
        }
        case Kind::Character: {
            const std::uint64_t value = parameter.defaultValueBits;
            if (value == '\\') source = "'\\\\'";
            else if (value == '\'') source = "'\\\''";
            else if (value == '\n') source = "'\\n'";
            else if (value == '\r') source = "'\\r'";
            else if (value == '\t') source = "'\\t'";
            else if (value >= 32 && value <= 126) source = std::string("'") + static_cast<char>(value) + "'";
            else
            {
                error = "metadata-backed source import cannot spell non-ASCII character "
                        "default";
                return false;
            }
            return true;
        }
        case Kind::String: source = escapedStringLiteral(parameter.defaultValueText); return true;
    }
    error = "metadata contains an unknown default-value kind";
    return false;
}

bool buildPrecompiledPublicSource(const interop::LibraryMetadata &metadata, std::string &source, std::string &error)
{
    struct PublishedType final
    {
        const interop::LibraryMetadataType *type = nullptr;
        std::vector<const interop::LibraryMetadataMember *> methods;
    };
    std::map<std::string, PublishedType> published;
    std::set<std::string> contractTypeIdentities;
    std::set<std::string> genericMethodOwners;
    constexpr std::uint8_t kPublicAccessibility = 3U;
    constexpr std::uint8_t kGenericMethodKind = 3U;
    for (const interop::LibraryMetadataGenericTemplate &generic : metadata.genericTemplates)
    {
        if (generic.kind != kGenericMethodKind) continue;
        std::string ownerPrefix;
        std::size_t begin = 0;
        while (begin <= generic.ownerName.size())
        {
            const std::size_t separator = generic.ownerName.find('+', begin);
            const std::size_t stop = separator == std::string::npos
                ? generic.ownerName.size()
                : separator;
            if (stop > begin)
            {
                if (!ownerPrefix.empty()) ownerPrefix.push_back('+');
                ownerPrefix.append(generic.ownerName.substr(begin, stop - begin));
                genericMethodOwners.insert(generic.namespaceName.empty()
                    ? ownerPrefix
                    : generic.namespaceName + "." + ownerPrefix);
            }
            if (separator == std::string::npos) break;
            begin = separator + 1U;
        }
    }
    for (const interop::LibraryMetadataType &type : metadata.types)
    {
        // A public nominal type is API even when no exported method happens to mention it. This
        // matters when a consumer supplies a standalone struct or enum from one metadata module
        // as the type argument of a public template from another module: omitting the declaration
        // makes the generic pass fall back to the consumer as its origin and the cloned template
        // later looks for the argument in the wrong project. Generic template declarations live
        // in metadata.genericTemplates; every concrete type in this table can be reconstructed by
        // the kind-specific source emitter below.
        if (type.accessibility == kPublicAccessibility &&
            isPubliclyReachableMetadataType(metadata, type))
        {
            published[type.identityKey].type = &type;
            collectMaterializedContractTypes(metadata, type.baseTypeKey, contractTypeIdentities);
            for (const std::string &interfaceTypeKey : type.interfaceTypeKeys)
                collectMaterializedContractTypes(metadata, interfaceTypeKey, contractTypeIdentities);
            for (const interop::LibraryMetadataField &field : type.fields)
                collectMaterializedContractTypes(metadata, field.typeKey, contractTypeIdentities);
        }
    }
    for (const interop::LibraryMetadataMember &member : metadata.members)
    {
        if (!canMaterializePrecompiledDeclaration(metadata, member))
        {
            continue;
        }
        const auto type = std::find_if(metadata.types.begin(), metadata.types.end(), [&](const auto &candidate) {
            return "type:" + candidate.identityKey == member.declaringTypeKey;
        });
        if (type == metadata.types.end())
        {
            error = "metadata member '" + member.name + "' refers to a declaring type that is not present";
            return false;
        }
        if (!isPubliclyReachableMetadataType(metadata, *type)) continue;
        if (type->accessibility != kPublicAccessibility)
        {
            constexpr std::uint32_t kVirtual = 1U << 3U;
            constexpr std::uint32_t kOverride = 1U << 4U;
            constexpr std::uint32_t kAbstract = 1U << 5U;
            constexpr std::uint8_t kDestructorKind = 4U;
            if ((member.flags & (kVirtual | kOverride | kAbstract)) == 0U && member.kind != kDestructorKind) continue;
        }
        PublishedType &group = published[type->identityKey];
        group.type = &*type;
        group.methods.push_back(&member);
        collectMaterializedContractTypes(metadata, member.declaredTypeKey, contractTypeIdentities);
        for (const interop::LibraryMetadataParameter &parameter : member.parameters)
            collectMaterializedContractTypes(metadata, parameter.typeKey, contractTypeIdentities);
    }
    for (const std::string &identity : contractTypeIdentities)
    {
        const auto type = std::ranges::find_if(
            metadata.types, [&](const auto &candidate) { return candidate.identityKey == identity; });
        if (type == metadata.types.end())
        {
            error = "metadata contract refers to missing type '" + identity + "'";
            return false;
        }
        published[identity].type = &*type;
    }

    std::ostringstream generated;
    // Every named type spelling below already comes from its canonical NSMETA key and therefore
    // carries the complete namespace. Do not synthesize namespace imports here. In NativeSharp a
    // `using` name deliberately has precedence over a type in the producer's global namespace;
    // importing `NativeSharp.Glfw` would consequently rebind the producer's own global `Keys` to
    // `NativeSharp.Glfw.Keys` and change the callable signature before physical-name binding.
    // Base lists keep their private aliases below because that grammar requires a simple name.
    // Base lists in NativeSharp intentionally use a simple identifier. Give every
    // metadata base/interface an unambiguous private alias so two dependencies may
    // both publish (for example) `Base` without losing their module/namespace
    // identity while reconstructing the consumer declaration.
    std::map<std::string, std::string> baseAliases;
    const auto collectBaseAlias = [&](const std::string &typeKey) -> bool {
        const std::string baseKey = metadataBaseTypeKey(typeKey);
        if (!baseKey.starts_with("type:")) return true;
        if (baseAliases.contains(baseKey)) return true;
        std::string sourceType;
        if (!sourceTypeForMetadataKey(metadata, baseKey, sourceType, error)) return false;
        const std::string alias = "__NscMetadataBase" + std::to_string(baseAliases.size());
        baseAliases.emplace(baseKey, alias);
        generated << "using " << alias << " = " << sourceType << ";\n";
        return true;
    };
    for (const interop::LibraryMetadataType &type : metadata.types)
    {
        if (!collectBaseAlias(type.baseTypeKey)) return false;
        for (const std::string &interfaceTypeKey : type.interfaceTypeKeys)
            if (!collectBaseAlias(interfaceTypeKey)) return false;
    }
    if (!baseAliases.empty()) generated << '\n';
    const auto sourceBaseType = [&](const std::string &typeKey, std::string &sourceType) -> bool {
        const std::string baseKey = metadataBaseTypeKey(typeKey);
        const auto alias = baseAliases.find(baseKey);
        if (alias != baseAliases.end())
        {
            sourceType = alias->second;
            return true;
        }
        return sourceTypeForMetadataKey(metadata, typeKey, sourceType, error);
    };
    const auto emitParameter = [&](const interop::LibraryMetadataParameter &parameter,
                                   const std::string &memberName,
                                   const std::size_t index) -> bool {
            std::string parameterType;
            std::string sourceTypeKey = parameter.typeKey;
            if (parameter.passKind != interop::LibraryMetadataParameterPassKind::Value)
            {
                if (sourceTypeKey.empty() || !sourceTypeKey.ends_with('*'))
                {
                    error = "metadata by-reference parameter has no pointer carrier in "
                            "member '" +
                            memberName + "'";
                    return false;
                }
                sourceTypeKey.pop_back();
            }
            if (!sourceTypeForMetadataKey(metadata, sourceTypeKey, parameterType, error)) return false;
            generated << parameterPassPrefix(parameter.passKind) << ownershipPrefix(parameter.ownership)
                      << parameterType << ' '
                      << (parameter.name.empty() ? "arg" + std::to_string(index) : parameter.name);
            std::string defaultValue;
            if (!sourceDefaultValue(parameter, defaultValue, error)) return false;
            if (!defaultValue.empty()) generated << " = " << defaultValue;
            return true;
    };
    const auto emitParameterRange = [&](const interop::LibraryMetadataMember &member,
                                        const std::size_t count) -> bool {
        if (count > member.parameters.size())
        {
            error = "metadata member '" + member.name + "' has an invalid parameter range";
            return false;
        }
        for (std::size_t index = 0; index < count; ++index)
        {
            if (index != 0) generated << ", ";
            if (!emitParameter(member.parameters[index], member.name, index)) return false;
        }
        return true;
    };
    const auto emitParameters = [&](const interop::LibraryMetadataMember &member) -> bool {
        if (!emitParameterRange(member, member.parameters.size())) return false;
        if ((member.flags & interop::kMetadataMemberVariadic) != 0U)
        {
            if (!member.parameters.empty()) generated << ", ";
            generated << "...";
        }
        return true;
    };

    struct AccessorContract final
    {
        bool isIndexer = false;
        std::string name;
        const interop::LibraryMetadataMember *getter = nullptr;
        const interop::LibraryMetadataMember *setter = nullptr;
        std::vector<const interop::LibraryMetadataMember *> declarationOrder;
    };
    const auto emitAccessorContracts = [&](const std::vector<const interop::LibraryMetadataMember *> &members,
                                           const bool interfaceContext,
                                           std::set<const interop::LibraryMetadataMember *> &materialized) -> bool {
        constexpr std::uint32_t kStatic = 1U << 0U;
        constexpr std::uint32_t kReadonly = 1U << 1U;
        constexpr std::uint32_t kVirtual = 1U << 3U;
        constexpr std::uint32_t kOverride = 1U << 4U;
        constexpr std::uint32_t kAbstract = 1U << 5U;
        constexpr std::uint32_t kSealed = 1U << 6U;
        constexpr std::uint32_t kPropertyAccessor = 1U << 12U;
        constexpr std::uint32_t kIndexerAccessor = 1U << 13U;
        constexpr std::uint32_t kGetter = 1U << 14U;
        constexpr std::uint32_t kSetter = 1U << 15U;
        constexpr std::uint32_t kNewSlot = 1U << 19U;
        constexpr std::uint32_t kDeclarationFlags =
            kStatic | kReadonly | kVirtual | kOverride | kAbstract | kSealed | kNewSlot;
        std::vector<AccessorContract> contracts;
        std::unordered_map<std::string, std::size_t> contractByKey;
        for (const interop::LibraryMetadataMember *member : members)
        {
            const bool property = (member->flags & kPropertyAccessor) != 0U;
            const bool indexer = (member->flags & kIndexerAccessor) != 0U;
            if (!property && !indexer) continue;
            const bool getter = (member->flags & kGetter) != 0U;
            const bool setter = (member->flags & kSetter) != 0U;
            if (property == indexer || getter == setter)
            {
                error = "metadata accessor '" + member->name + "' has inconsistent property/indexer flags";
                return false;
            }
            std::string associatedName;
            if (property)
            {
                const std::string_view prefix = getter ? std::string_view("get_") : std::string_view("set_");
                if (!member->name.starts_with(prefix) || member->name.size() == prefix.size())
                {
                    error = "metadata property accessor '" + member->name + "' has no recoverable property name";
                    return false;
                }
                associatedName = member->name.substr(prefix.size());
            }
            else
            {
                associatedName = "Item";
            }
            std::ostringstream key;
            key << (indexer ? "indexer:" : "property:") << associatedName;
            if (indexer)
            {
                if (setter && member->parameters.empty())
                {
                    error = "metadata indexer setter '" + member->name + "' has no value parameter";
                    return false;
                }
                const std::size_t indexCount = member->parameters.size() - (setter ? 1U : 0U);
                for (std::size_t index = 0; index < indexCount; ++index)
                    key << ':' << static_cast<unsigned>(member->parameters[index].passKind) << ':'
                        << member->parameters[index].typeKey;
            }
            const std::string contractKey = key.str();
            const auto [position, inserted] = contractByKey.emplace(contractKey, contracts.size());
            if (inserted) contracts.push_back({});
            AccessorContract &contract = contracts[position->second];
            contract.isIndexer = indexer;
            contract.name = associatedName;
            const interop::LibraryMetadataMember *&slot = getter ? contract.getter : contract.setter;
            if (slot != nullptr)
            {
                error = "metadata contains duplicate accessor '" + member->name + "'";
                return false;
            }
            slot = member;
            contract.declarationOrder.push_back(member);
        }

        for (const AccessorContract &contract : contracts)
        {
            if (contract.getter == nullptr)
            {
                error = "metadata-backed source import cannot materialize set-only accessor '" +
                    contract.setter->name + "'";
                return false;
            }
            const interop::LibraryMetadataMember &shape = *contract.getter;
            if (contract.setter != nullptr)
            {
                if ((shape.flags & interop::kMetadataMemberReferenceReturn) != 0U ||
                    (contract.setter->flags & interop::kMetadataMemberReferenceReturn) != 0U)
                {
                    error = "metadata reference-returning indexer '" + contract.name +
                        "' cannot declare a setter";
                    return false;
                }
                if (contract.setter->parameters.empty() ||
                    contract.setter->parameters.back().typeKey != shape.declaredTypeKey ||
                    (contract.setter->flags & kDeclarationFlags) != (shape.flags & kDeclarationFlags))
                {
                    error = "metadata accessor pair for '" + contract.name + "' has inconsistent type or modifiers";
                    return false;
                }
            }
            std::uint8_t accessibility = shape.accessibility;
            if (contract.setter != nullptr)
            {
                if (shape.accessibility == 3U || contract.setter->accessibility == 3U) accessibility = 3U;
                else if (shape.accessibility == 1U || contract.setter->accessibility == 1U) accessibility = 1U;
                else if (shape.accessibility == 2U || contract.setter->accessibility == 2U) accessibility = 2U;
                else accessibility = 0U;
            }
            std::string declaredType;
            if (!sourceTypeForMetadataKey(metadata, shape.declaredTypeKey, declaredType, error)) return false;
            interop::LibraryMetadataOwnershipKind declaredOwnership = shape.declaredOwnership;
            if (contract.setter != nullptr &&
                contract.setter->parameters.back().ownership != interop::LibraryMetadataOwnershipKind::None)
            {
                declaredOwnership = contract.setter->parameters.back().ownership;
            }
            generated << "    " << sourceAccessibility(accessibility) << ' ';
            // Interface membership is the semantic contract. Older and third-party NSMETA may
            // preserve virtual-style dispatch flags without kAbstract; none of those class
            // implementation modifiers is valid in Stage 38 interface source.
            if (!interfaceContext)
            {
                if ((shape.flags & kStatic) != 0U) generated << "static ";
                if ((shape.flags & kReadonly) != 0U) generated << "readonly ";
                if ((shape.flags & kSealed) != 0U) generated << "sealed ";
                if ((shape.flags & kNewSlot) != 0U) generated << "new ";
                if ((shape.flags & kOverride) != 0U) generated << "override ";
                else if ((shape.flags & kAbstract) != 0U) generated << "abstract ";
                else if ((shape.flags & kVirtual) != 0U) generated << "virtual ";
            }
            if ((shape.flags & interop::kMetadataMemberReferenceReturn) != 0U) generated << "ref ";
            generated << ownershipPrefix(declaredOwnership) << declaredType << ' ';
            if (contract.isIndexer)
            {
                generated << "this[";
                if (!emitParameterRange(shape, shape.parameters.size())) return false;
                generated << ']';
            }
            else
            {
                generated << contract.name;
            }
            generated << "\n    {\n";
            const auto emitAccessor = [&](const interop::LibraryMetadataMember &accessor,
                                          const std::string_view keyword) {
                generated << "        ";
                if (!interfaceContext && accessor.accessibility != accessibility)
                    generated << sourceAccessibility(accessor.accessibility) << ' ';
                generated << keyword << (((accessor.flags & kAbstract) != 0U || interfaceContext) ? ";\n" : " { }\n");
            };
            for (const interop::LibraryMetadataMember *accessor : contract.declarationOrder)
            {
                emitAccessor(*accessor, accessor == contract.getter ? "get" : "set");
                materialized.insert(accessor);
            }
            generated << "    }\n";
        }
        return true;
    };
    std::map<std::string, std::vector<std::string>> nestedChildren;
    for (const auto &[identity, group] : published)
    {
        const std::size_t separator = group.type->fullyQualifiedName.find_last_of('+');
        if (separator != std::string::npos)
            nestedChildren[group.type->fullyQualifiedName.substr(0, separator)].push_back(identity);
    }
    std::function<bool(const std::string&, bool)> emitPublishedType;
    emitPublishedType = [&](const std::string &identity, const bool topLevel) -> bool {
        const auto groupFound = published.find(identity);
        if (groupFound == published.end() || groupFound->second.type == nullptr) return true;
        const PublishedType &group = groupFound->second;
        const interop::LibraryMetadataType &type = *group.type;
        if (type.fullyQualifiedName.find('`') != std::string::npos)
        {
            error = "metadata-backed source import does not yet support generic declaring type '" +
                    type.fullyQualifiedName + "'";
            return false;
        }
        const std::size_t topTypeEnd = type.fullyQualifiedName.find('+');
        const std::string_view topTypeName(
            type.fullyQualifiedName.data(),
            topTypeEnd == std::string::npos ? type.fullyQualifiedName.size() : topTypeEnd);
        const std::size_t separator = topTypeName.find_last_of('.');
        const std::string namespaceName =
            separator == std::string::npos ? std::string() : std::string(topTypeName.substr(0, separator));
        const std::size_t nestedSeparator = type.fullyQualifiedName.find_last_of('+');
        const std::string typeName = nestedSeparator == std::string::npos
            ? (separator == std::string::npos
                ? type.fullyQualifiedName
                : type.fullyQualifiedName.substr(separator + 1U))
            : type.fullyQualifiedName.substr(nestedSeparator + 1U);
        std::vector<const interop::LibraryMetadataMember *> orderedMethods = group.methods;
        const std::string declaringTypeKey = "type:" + type.identityKey;
        const auto dispatch = std::ranges::find_if(metadata.dispatchTables,
            [&](const interop::LibraryMetadataDispatch &candidate) {
                return candidate.typeKey == declaringTypeKey;
            });
        const auto dispatchRank = [&](const interop::LibraryMetadataMember *member) {
            if (dispatch != metadata.dispatchTables.end())
            {
                const auto slot = std::ranges::find(dispatch->slotSignatures, member->signatureKey);
                if (slot != dispatch->slotSignatures.end())
                    return static_cast<std::size_t>(slot - dispatch->slotSignatures.begin());
            }
            if (type.kind == interop::LibraryMetadataTypeKind::Interface)
            {
                for (const interop::LibraryMetadataDispatch &owner : metadata.dispatchTables)
                {
                    for (const interop::LibraryMetadataDispatchInterface &interface : owner.interfaces)
                    {
                        if (interface.interfaceTypeKey != declaringTypeKey) continue;
                        const auto slot = std::ranges::find(interface.interfaceSignatures, member->signatureKey);
                        if (slot != interface.interfaceSignatures.end())
                            return static_cast<std::size_t>(slot - interface.interfaceSignatures.begin());
                    }
                }
            }
            return std::numeric_limits<std::size_t>::max();
        };
        std::stable_sort(orderedMethods.begin(), orderedMethods.end(), [&](const auto *left, const auto *right) {
            return dispatchRank(left) < dispatchRank(right);
        });
        if (topLevel && !namespaceName.empty()) generated << "namespace " << namespaceName << "\n{\n";
        const std::string_view typeAccessibility =
            type.accessibility == kPublicAccessibility ? std::string_view("public") : std::string_view("internal");
        if (type.kind == interop::LibraryMetadataTypeKind::Delegate)
        {
            std::string returnType;
            if (!sourceTypeForMetadataKey(metadata, type.delegateReturnTypeKey, returnType, error)) return false;
            generated << typeAccessibility << ' ';
            if ((type.delegateFlags & 1U) != 0U)
                generated << (((type.delegateFlags & 2U) != 0U) ? "extern vectorcall " : "extern C ");
            generated << "delegate " << returnType << ' ' << typeName << '(';
            for (std::size_t parameterIndex = 0; parameterIndex < type.delegateParameterTypeKeys.size();
                ++parameterIndex)
            {
                if (parameterIndex != 0) generated << ", ";
                std::string parameterType;
                if (!sourceTypeForMetadataKey(metadata, type.delegateParameterTypeKeys[parameterIndex], parameterType, error))
                    return false;
                generated << parameterType << " arg" << parameterIndex;
            }
            generated << ");\n";
            if (topLevel && !namespaceName.empty()) generated << "}\n";
            return true;
        }
        if (type.kind == interop::LibraryMetadataTypeKind::Enum)
        {
            std::string underlyingType;
            if (!sourceTypeForMetadataKey(metadata,
                    type.enumUnderlyingTypeKey.empty() ? "builtin:int" : type.enumUnderlyingTypeKey,
                    underlyingType,
                    error))
            {
                return false;
            }
            generated << typeAccessibility << " enum " << typeName << " : " << underlyingType << "\n{\n";
            bool firstEnumerator = true;
            constexpr std::uint8_t kFieldKind = 0U;
            const std::string declaringTypeKey = "type:" + type.identityKey;
            for (const interop::LibraryMetadataMember &member : metadata.members)
            {
                if (member.kind != kFieldKind || member.declaringTypeKey != declaringTypeKey ||
                    member.constantKind == 0U)
                {
                    continue;
                }
                if (!firstEnumerator) generated << ",\n";
                generated << "    " << member.name << " = " << enumConstantSource(type, member.constantBits);
                firstEnumerator = false;
            }
            if (!firstEnumerator) generated << '\n';
            generated << "}\n";
            if (topLevel && !namespaceName.empty()) generated << "}\n";
            return true;
        }

        if (type.kind == interop::LibraryMetadataTypeKind::Interface)
        {
            generated << typeAccessibility << " interface " << typeName;
            if (!type.interfaceTypeKeys.empty()) generated << " : ";
            for (std::size_t index = 0; index < type.interfaceTypeKeys.size(); ++index)
            {
                if (index != 0) generated << ", ";
                std::string baseInterface;
                if (!sourceBaseType(type.interfaceTypeKeys[index], baseInterface)) return false;
                generated << baseInterface;
            }
            generated << "\n{\n";
            constexpr std::uint8_t kMethodKind = 2U;
            std::set<const interop::LibraryMetadataMember *> materializedAccessors;
            if (!emitAccessorContracts(orderedMethods, true, materializedAccessors)) return false;
            for (const interop::LibraryMetadataMember *member : orderedMethods)
            {
                if (member->kind != kMethodKind || materializedAccessors.contains(member)) continue;
                std::string returnType;
                if (!sourceTypeForMetadataKey(metadata, member->declaredTypeKey, returnType, error)) return false;
                generated << "    public ";
                if ((member->flags & interop::kMetadataMemberNoHeap) != 0U) generated << "noheap ";
                if ((member->flags & interop::kMetadataMemberReferenceReturn) != 0U) generated << "ref ";
                generated << ownershipPrefix(member->declaredOwnership) << returnType << ' '
                          << member->name << '(';
                if (!emitParameters(*member)) return false;
                generated << ");\n";
            }
            generated << "}\n";
            if (topLevel && !namespaceName.empty()) generated << "}\n";
            return true;
        }
        if (type.kind != interop::LibraryMetadataTypeKind::Struct &&
            type.kind != interop::LibraryMetadataTypeKind::Class)
        {
            error = "metadata-backed source import cannot materialize declaration "
                    "kind for '" +
                    type.fullyQualifiedName + "'";
            return false;
        }
        generated << typeAccessibility << ' ';
        if (type.kind == interop::LibraryMetadataTypeKind::Struct)
        {
            // Recreate the producer's physical contract, including
            // packed/union/explicit layouts, from the final layout already frozen in
            // NSMETA. `pack(alignment)` preserves a lowered aggregate alignment,
            // `align` preserves a raised one, every `at` freezes its field offset,
            // and `size` makes the consumer reject a declaration it cannot reproduce.
            generated << "pack(" << type.alignment << ") align(" << type.alignment << ") size(" << type.size
                      << ") ";
            if ((type.flags & (1U << 6U)) != 0U) generated << "readonly ";
            if ((type.flags & (1U << 4U)) != 0U) generated << "borrowed ";
            generated << ((type.flags & (1U << 5U)) != 0U ? "union " : "struct ");
        }
        else
        {
            if (genericMethodOwners.contains(type.fullyQualifiedName)) generated << "partial ";
            if ((type.flags & (1U << 1U)) != 0U) generated << "abstract ";
            if ((type.flags & (1U << 2U)) != 0U) generated << "sealed ";
            if ((type.flags & (1U << 3U)) != 0U) generated << "static ";
            generated << "class ";
        }
        generated << typeName;
        // A static class has no instance hierarchy. Some NSMETA 1.10 producers recorded the
        // implicit Object edge anyway; ignore that legacy pollution instead of generating the
        // invalid `static class Utility : Object` source that the parser correctly rejects.
        if (type.kind == interop::LibraryMetadataTypeKind::Class &&
            (type.flags & (1U << 3U)) == 0U &&
            (!type.baseTypeKey.empty() || !type.interfaceTypeKeys.empty()))
        {
            generated << " : ";
            bool wroteBase = false;
            if (!type.baseTypeKey.empty())
            {
                std::string baseType;
                if (!sourceBaseType(type.baseTypeKey, baseType)) return false;
                generated << baseType;
                wroteBase = true;
            }
            for (const std::string &interfaceTypeKey : type.interfaceTypeKeys)
            {
                std::string interfaceType;
                if (!sourceBaseType(interfaceTypeKey, interfaceType)) return false;
                if (wroteBase) generated << ", ";
                generated << interfaceType;
                wroteBase = true;
            }
        }
        generated << "\n{\n";

        std::set<const interop::LibraryMetadataMember *> materializedEvents;
        std::set<const interop::LibraryMetadataMember *> materializedAccessors;
        std::function<bool(
            const interop::LibraryMetadataType &,
            const interop::LibraryMetadataField &,
            std::string_view)> emitAnonymousCarrier;
        emitAnonymousCarrier = [&]
            (const interop::LibraryMetadataType &owner,
             const interop::LibraryMetadataField &carrier,
             const std::string_view indentation) -> bool {
            if (!carrier.typeKey.starts_with("type:"))
            {
                error = "metadata anonymous-member carrier '" + carrier.name +
                    "' does not refer to a nominal aggregate type";
                return false;
            }
            const interop::LibraryMetadataType *nestedFound =
                metadataTypeForKey(metadata, carrier.typeKey);
            if (nestedFound == nullptr ||
                nestedFound->kind != interop::LibraryMetadataTypeKind::Struct)
            {
                error = "metadata anonymous-member carrier '" + carrier.name +
                    "' refers to a missing or non-aggregate type '" + carrier.typeKey + "'";
                return false;
            }
            const interop::LibraryMetadataType &nested = *nestedFound;
            const interop::LibraryMetadataMember *carrierMember =
                metadataFieldMember(metadata, owner, carrier.name);
            generated << indentation;
            if (owner.kind == interop::LibraryMetadataTypeKind::Struct)
                generated << "at(" << carrier.offset << ") ";
            generated << sourceAccessibility(
                carrierMember == nullptr ? 0U : carrierMember->accessibility) << ' ';
            generated << "pack(" << nested.alignment << ") align(" << nested.alignment
                      << ") size(" << nested.size << ") "
                      << ((nested.flags & (1U << 5U)) != 0U ? "union" : "struct") << "\n"
                      << indentation << "{\n";
            const std::string childIndentation = std::string(indentation) + "    ";
            for (std::size_t childIndex = 0; childIndex < nested.fields.size(); ++childIndex)
            {
                const interop::LibraryMetadataField &child = nested.fields[childIndex];
                if ((child.flags & interop::kMetadataFieldAnonymousCarrier) != 0U)
                {
                    if (!emitAnonymousCarrier(nested, child, childIndentation)) return false;
                    continue;
                }
                std::string childType;
                if (!sourceTypeForMetadataKey(metadata, child.typeKey, childType, error)) return false;
                const interop::LibraryMetadataMember *childMember =
                    metadataFieldMember(metadata, nested, child.name);
                const std::string childName = child.name.empty()
                    ? "__nsc_layout_field_" + std::to_string(childIndex)
                    : child.name;
                const bool childBitField =
                    (child.flags & interop::kMetadataFieldBitField) != 0U;
                const bool childUnnamedBitField =
                    (child.flags & interop::kMetadataFieldUnnamedBitField) != 0U;
                generated << childIndentation;
                if (!childBitField)
                    generated << "at(" << child.offset << ") align(" << child.alignment << ") ";
                generated << sourceAccessibility(
                    childMember == nullptr ? 0U : childMember->accessibility) << ' ';
                if ((child.flags & (1U << 1U)) != 0U || (nested.flags & (1U << 6U)) != 0U)
                    generated << "readonly ";
                if ((child.flags & interop::kMetadataMemberVolatile) != 0U) generated << "volatile ";
                generated << ownershipPrefix(child.ownership) << childType;
                if (!childUnnamedBitField) generated << ' ' << childName;
                if (childBitField) generated << " : " << child.bitWidth;
                generated << ";\n";
            }
            generated << indentation << "};\n";
            return true;
        };
        if (type.kind == interop::LibraryMetadataTypeKind::Struct ||
            type.kind == interop::LibraryMetadataTypeKind::Class)
        {
            for (std::size_t fieldIndex = 0; fieldIndex < type.fields.size(); ++fieldIndex)
            {
                const interop::LibraryMetadataField &field = type.fields[fieldIndex];
                if ((field.flags & interop::kMetadataFieldAnonymousCarrier) != 0U)
                {
                    if (!emitAnonymousCarrier(type, field, "    ")) return false;
                    continue;
                }
                std::string fieldType;
                if (!sourceTypeForMetadataKey(metadata, field.typeKey, fieldType, error)) return false;
                if (const interop::LibraryMetadataMember *event = metadataEventForBacking(metadata, type, field);
                    event != nullptr)
                {
                    generated << "    " << sourceAccessibility(event->accessibility) << ' ';
                    if ((event->flags & (1U << 0U)) != 0U) generated << "static ";
                    generated << "event " << ownershipPrefix(event->declaredOwnership) << fieldType << ' '
                              << event->name << ";\n";
                    materializedEvents.insert(event);
                    continue;
                }
                const interop::LibraryMetadataMember *member = metadataFieldMember(metadata, type, field.name);
                constexpr std::uint32_t kCompilerGenerated = 1U << 10U;
                const bool propertyBacking = (field.flags & kCompilerGenerated) != 0U &&
                    field.name.starts_with("__nsc_backing_") && field.name.ends_with("$stage38");
                const std::string fieldName = propertyBacking
                    ? "__nsc_layout_property_" + std::to_string(fieldIndex)
                    : field.name.empty() ? "__nsc_layout_field_" + std::to_string(fieldIndex) : field.name;
                const bool bitField =
                    (field.flags & interop::kMetadataFieldBitField) != 0U;
                const bool unnamedBitField =
                    (field.flags & interop::kMetadataFieldUnnamedBitField) != 0U;
                generated << "    ";
                if (!bitField && type.kind == interop::LibraryMetadataTypeKind::Struct)
                    generated << "at(" << field.offset << ") ";
                if (!bitField) generated << "align(" << field.alignment << ") ";
                generated << (member == nullptr ? std::string_view("private")
                                                : sourceAccessibility(member->accessibility))
                          << ' ';
                if ((field.flags & (1U << 1U)) != 0U || (type.flags & (1U << 6U)) != 0U)
                    generated << "readonly ";
                if ((field.flags & interop::kMetadataMemberVolatile) != 0U) generated << "volatile ";
                generated << ownershipPrefix(field.ownership) << fieldType;
                if (!unnamedBitField) generated << ' ' << fieldName;
                if (bitField) generated << " : " << field.bitWidth;
                generated << ";\n";
            }
        }

        if (!emitAccessorContracts(orderedMethods, false, materializedAccessors)) return false;

        for (const interop::LibraryMetadataMember *member : orderedMethods)
        {
            constexpr std::uint8_t kFieldKind = 0U;
            constexpr std::uint8_t kMethodKind = 2U;
            constexpr std::uint8_t kEventKind = 1U;
            constexpr std::uint8_t kConstructorKind = 3U;
            constexpr std::uint8_t kDestructorKind = 4U;
            if (materializedAccessors.contains(member)) continue;
            if (member->kind == kFieldKind)
            {
                std::string fieldType;
                if (!sourceTypeForMetadataKey(metadata, member->declaredTypeKey, fieldType, error)) return false;
                if ((member->flags & (1U << 2U)) != 0U)
                {
                    std::string value;
                    if (!metadataConstantSource(*member, value, error)) return false;
                    generated << "    " << sourceAccessibility(member->accessibility) << " const "
                              << fieldType << ' ' << member->name << " = " << value << ";\n";
                }
                else
                {
                    generated << "    " << sourceAccessibility(member->accessibility) << " static ";
                    if ((member->flags & (1U << 1U)) != 0U) generated << "readonly ";
                    if ((member->flags & interop::kMetadataMemberVolatile) != 0U) generated << "volatile ";
                    generated << fieldType << ' ' << member->name << ";\n";
                }
                continue;
            }
            if (member->kind == kEventKind)
            {
                if (materializedEvents.contains(member)) continue;
                std::string eventType;
                if (!sourceTypeForMetadataKey(metadata, member->declaredTypeKey, eventType, error)) return false;
                generated << "    " << sourceAccessibility(member->accessibility) << ' ';
                if ((member->flags & (1U << 0U)) != 0U) generated << "static ";
                generated << "event " << ownershipPrefix(member->declaredOwnership) << eventType << ' '
                          << member->name << ";\n";
                continue;
            }
            if (member->kind == kDestructorKind)
            {
                generated << "    ~" << typeName << "() { }\n";
                continue;
            }
            if (member->kind == kConstructorKind)
            {
                generated << "    " << sourceAccessibility(member->accessibility) << ' ';
                if ((member->flags & interop::kMetadataMemberNoHeap) != 0U) generated << "noheap ";
                generated << typeName << '(';
                if (!emitParameters(*member)) return false;
                generated << ") { }\n";
                continue;
            }
            if (member->kind != kMethodKind) continue;
            if (!member->inlineSourceText.empty())
            {
                // NSMETA 1.13 transports the producer's complete ordinary inline declaration.
                // It must stay in this otherwise-precompiled type part so structs need no new
                // partial-type syntax. ProjectMemberModel recognizes the inline modifier as the
                // executable exception and analyzes this body in the consumer.
                generated << "    " << member->inlineSourceText << '\n';
                continue;
            }
            std::string returnType;
            if (!sourceTypeForMetadataKey(metadata, member->declaredTypeKey, returnType, error)) return false;
            constexpr std::uint32_t kStatic = 1U << 0U;
            constexpr std::uint32_t kReadonly = 1U << 1U;
            constexpr std::uint32_t kVirtual = 1U << 3U;
            constexpr std::uint32_t kOverride = 1U << 4U;
            constexpr std::uint32_t kAbstract = 1U << 5U;
            constexpr std::uint32_t kSealed = 1U << 6U;
            constexpr std::uint32_t kUserOperator = 1U << 16U;
            constexpr std::uint32_t kUserConversion = 1U << 17U;
            constexpr std::uint32_t kImplicitConversion = 1U << 18U;
            constexpr std::uint32_t kNewSlot = 1U << 19U;
            constexpr std::uint32_t kImportedC = 1U << 20U;
            if ((member->flags & kImportedC) != 0U)
            {
                generated << "    extern C";
                if (!member->importDllName.empty())
                    generated << " from " << escapedStringLiteral(member->importDllName);
                generated << "\n    {\n        " << sourceAccessibility(member->accessibility) << ' '
                          << ownershipPrefix(member->declaredOwnership) << returnType << ' ' << member->name << '(';
                if (!emitParameters(*member)) return false;
                generated << ");\n    }\n";
                continue;
            }
            if ((member->flags & kUserOperator) != 0U)
            {
                const std::string_view token = metadataOperatorToken(member->name);
                if (token.empty())
                {
                    error = "metadata-backed source import cannot spell user operator '" + member->name + "'";
                    return false;
                }
                generated << "    " << sourceAccessibility(member->accessibility) << " static "
                          << ownershipPrefix(member->declaredOwnership) << returnType << " operator " << token << '(';
                if (!emitParameters(*member)) return false;
                generated << ") { }\n";
                continue;
            }
            if ((member->flags & kUserConversion) != 0U)
            {
                generated << "    " << sourceAccessibility(member->accessibility) << " static "
                          << ((member->flags & kImplicitConversion) != 0U ? "implicit" : "explicit")
                          << " operator " << ownershipPrefix(member->declaredOwnership) << returnType << '(';
                if (!emitParameters(*member)) return false;
                generated << ") { }\n";
                continue;
            }
            generated << "    " << sourceAccessibility(member->accessibility) << ' ';
            if ((member->flags & kStatic) != 0U) generated << "static ";
            if ((member->flags & kReadonly) != 0U) generated << "readonly ";
            if ((member->flags & kSealed) != 0U) generated << "sealed ";
            if ((member->flags & kNewSlot) != 0U) generated << "new ";
            if ((member->flags & kOverride) != 0U) generated << "override ";
            else if ((member->flags & kAbstract) != 0U) generated << "abstract ";
            else if ((member->flags & kVirtual) != 0U) generated << "virtual ";
            const std::uint32_t inlining =
                (member->flags & interop::kMetadataMemberInliningMask) >>
                interop::kMetadataMemberInliningShift;
            if (inlining == 3U) generated << "noinline ";
            if ((member->flags & interop::kMetadataMemberNoHeap) != 0U) generated << "noheap ";
            if ((member->flags & interop::kMetadataMemberReferenceReturn) != 0U) generated << "ref ";
            generated << ownershipPrefix(member->declaredOwnership) << returnType << ' ' << member->name << '(';
            if (!emitParameters(*member)) return false;
            // The empty body is an implementation detail of the generated syntax.
            // Precompiled units are skipped by body analysis and lowered as external
            // NativeSharp declarations.
            generated << (((member->flags & kAbstract) != 0U) ? ");\n" : ") { }\n");
        }
        const auto children = nestedChildren.find(type.fullyQualifiedName);
        if (children != nestedChildren.end())
        {
            for (const std::string &childIdentity : children->second)
                if (!emitPublishedType(childIdentity, false)) return false;
        }
        generated << "}\n";
        if (topLevel && !namespaceName.empty()) generated << "}\n";
        return true;
    };
    for (const auto &[identity, group] : published)
    {
        if (group.type != nullptr && group.type->fullyQualifiedName.find('+') == std::string::npos &&
            !emitPublishedType(identity, true))
        {
            return false;
        }
    }
    source = generated.str();
    return true;
}

bool verifyPrecompiledPublicSource(const interop::LibraryMetadata &metadata, std::string &error)
{
    std::string generated;
    if (!buildPrecompiledPublicSource(metadata, generated, error)) return false;

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
    frontend::ProjectSyntaxUnit unit(0, 0, metadata.moduleName, fileId, true);
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
        for (const std::string &parameterTypeKey : type.delegateParameterTypeKeys) appendKnownType(parameterTypeKey);
    }
    for (const interop::LibraryMetadataMember &member : metadata.members)
    {
        appendKnownType(member.declaringTypeKey);
        appendKnownType(member.declaredTypeKey);
        for (const interop::LibraryMetadataParameter &parameter : member.parameters) appendKnownType(parameter.typeKey);
    }
    constexpr std::uint8_t kGenericMethodKind = 3U;
    for (const interop::LibraryMetadataGenericTemplate &generic : metadata.genericTemplates)
        if (generic.kind != kGenericMethodKind) appendKnownType("type:" + generic.canonicalKey);
    std::sort(knownTypeNames.begin(), knownTypeNames.end());
    knownTypeNames.erase(std::unique(knownTypeNames.begin(), knownTypeNames.end()), knownTypeNames.end());
    if (unit.lex(*file, diagnostics) && unit.parse(diagnostics, knownTypeNames)) return true;

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

bool buildPrecompiledGenericTemplateSource(
    const interop::LibraryMetadata &metadata, std::string &source, std::string &error)
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
        if (!generic.namespaceName.empty()) generated << "namespace " << generic.namespaceName << "\n{\n";
        if (generic.kind == kGenericMethodKind)
        {
            if (generic.ownerName.empty())
            {
                error =
                    "metadata generic method '" + generic.canonicalKey + "' has an unsupported declaring-type identity";
                return false;
            }

            std::size_t openedOwners = 0;
            std::string ownerPrefix;
            std::size_t begin = 0;
            while (begin <= generic.ownerName.size())
            {
                const std::size_t separator = generic.ownerName.find('+', begin);
                const std::size_t stop = separator == std::string::npos
                    ? generic.ownerName.size()
                    : separator;
                if (stop == begin)
                {
                    error = "metadata generic method '" + generic.canonicalKey +
                        "' has an empty declaring-type segment";
                    return false;
                }
                const std::string segment = generic.ownerName.substr(begin, stop - begin);
                if (!ownerPrefix.empty()) ownerPrefix.push_back('+');
                ownerPrefix += segment;
                const std::string fullyQualifiedOwner = generic.namespaceName.empty()
                    ? ownerPrefix
                    : generic.namespaceName + "." + ownerPrefix;
                const interop::LibraryMetadataType *owner = metadataTypeForKey(
                    metadata, "type:" + metadata.moduleName + "::" + fullyQualifiedOwner);
                if (owner == nullptr ||
                    owner->kind != interop::LibraryMetadataTypeKind::Class)
                {
                    error = "metadata generic method '" + generic.canonicalKey +
                        "' does not have a materializable class owner '" + fullyQualifiedOwner + "'";
                    return false;
                }
                generated << sourceAccessibility(owner->accessibility) << " partial ";
                if ((owner->flags & (1U << 1U)) != 0U) generated << "abstract ";
                if ((owner->flags & (1U << 2U)) != 0U) generated << "sealed ";
                if ((owner->flags & (1U << 3U)) != 0U) generated << "static ";
                generated << "class " << segment << "\n{\n";
                ++openedOwners;
                if (separator == std::string::npos) break;
                begin = separator + 1U;
            }
            generated << generic.sourceText << '\n';
            while (openedOwners-- > 0U) generated << "}\n";
        }
        else
        {
            generated << generic.sourceText << '\n';
        }
        if (!generic.namespaceName.empty()) generated << "}\n";
    }
    source = generated.str();
    return true;
}

bool verifyPrecompiledGenericTemplateSource(
    const interop::LibraryMetadata &metadata, std::string &error)
{
    std::string generated;
    if (!buildPrecompiledGenericTemplateSource(metadata, generated, error)) return false;
    if (generated.empty()) return true;

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
    const auto appendKnownType = [&](const std::string &typeKey) {
        appendParserKnownTypeNames(metadata, typeKey, knownTypeNames);
    };
    for (const interop::LibraryMetadataType &type : metadata.types)
        appendKnownType("type:" + type.identityKey);
    constexpr std::uint8_t kGenericMethodKind = 3U;
    for (const interop::LibraryMetadataGenericTemplate &generic : metadata.genericTemplates)
        if (generic.kind != kGenericMethodKind) appendKnownType("type:" + generic.canonicalKey);
    for (const std::string &typeKey : metadata.externalTypeKeys) appendKnownType(typeKey);
    std::sort(knownTypeNames.begin(), knownTypeNames.end());
    knownTypeNames.erase(
        std::unique(knownTypeNames.begin(), knownTypeNames.end()), knownTypeNames.end());

    frontend::ProjectSyntaxUnit unit(0, 0, metadata.moduleName, fileId, false);
    if (unit.lex(*file, diagnostics) && unit.parse(diagnostics, knownTypeNames)) return true;

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

bool writeVerifiedLibraryMetadata(
    const frontend::ProjectCompilation &compilation,
    const std::filesystem::path &destination,
    std::string &error)
{
    interop::LibraryMetadataCodec codec;
    if (!codec.write(compilation, destination, error)) return false;

    interop::LibraryMetadata written;
    if (codec.read(destination, written, error) &&
        codec.verifyCurrentCompiler(written, error) &&
        verifyPrecompiledPublicSource(written, error) &&
        verifyPrecompiledGenericTemplateSource(written, error))
    {
        return true;
    }

    std::error_code ignored;
    std::filesystem::remove(destination, ignored);
    error = "metadata producer read-back verification failed: " + error;
    return false;
}

bool configurePrecompiledModules(
    const std::filesystem::path &projectFile, frontend::ProjectCompilation &compilation, std::string &error)
{
    const CompilationJobPolicy* const jobs = activeCompilationJobPolicy();
    compilation::ProjectGraphLoader loader(
        jobs != nullptr ? jobs->effectiveJobs() : 1,
        {},
        activeProjectLoadOptions());
    std::optional<compilation::ProjectGraph> graph = loader.load(projectFile, error);
    if (!graph.has_value()) return false;

    interop::LibraryMetadataCodec codec;
    std::vector<LoadedPrecompiledMetadata> loaded;
    for (const compilation::Project &project : graph->projects())
    {
        for (const compilation::LibraryReference &reference : project.libraryReferences())
        {
            interop::LibraryMetadata metadata;
            if (!codec.read(reference.metadataPath, metadata, error) || !codec.verifyCurrentCompiler(metadata, error))
            {
                error =
                    "cannot import precompiled metadata '" + reference.metadataPath.generic_string() + "': " + error;
                return false;
            }
            loaded.push_back({project.projectFile(), reference.metadataPath, std::move(metadata)});
        }
    }
    if (!validatePrecompiledExternalTypeClosure(loaded, error)) return false;

    std::vector<frontend::PrecompiledSyntaxModule> modules;
    modules.reserve(loaded.size());
    for (LoadedPrecompiledMetadata &entry : loaded)
    {
            interop::LibraryMetadata &metadata = entry.metadata;
            std::string publicSource;
            if (!buildPrecompiledPublicSource(metadata, publicSource, error))
            {
                error = "cannot materialize public API for module '" + metadata.moduleName + "': " + error;
                return false;
            }
            std::string templateSource;
            if (!buildPrecompiledGenericTemplateSource(metadata, templateSource, error))
            {
                error = "cannot materialize public templates for module '" + metadata.moduleName + "': " + error;
                return false;
            }
            modules.push_back({entry.ownerProjectFile,
                entry.metadataPath,
                metadata.moduleName,
                std::move(publicSource),
                std::move(templateSource),
                [&]() {
                    std::vector<frontend::PrecompiledMemberIdentity> result;
                    result.reserve(metadata.members.size());
                    for (const interop::LibraryMetadataMember &member : metadata.members)
                    {
                        result.push_back({static_cast<frontend::ClassMemberKind>(member.kind),
                            member.declaringTypeKey,
                            member.name,
                            member.signatureKey,
                            member.mangledName});
                    }
                    return result;
                }(),
                [&]() {
                    std::vector<frontend::PrecompiledGenericSpecialization> result;
                    result.reserve(metadata.genericSpecializations.size());
                    for (const interop::LibraryMetadataGenericSpecialization &specialization :
                         metadata.genericSpecializations)
                    {
                        bool hasConcreteDeclaration = false;
                        constexpr std::uint8_t kGenericMethodKind = 3U;
                        if (specialization.kind == kGenericMethodKind)
                        {
                            hasConcreteDeclaration = std::ranges::any_of(
                                metadata.members,
                                [&](const auto &member) { return member.name == specialization.specializedName; });
                        }
                        else if (const std::optional<std::string> typeKey =
                                     metadataSpecializedTypeKey(specialization);
                                 typeKey.has_value())
                        {
                            hasConcreteDeclaration = metadataTypeForKey(metadata, *typeKey) != nullptr;
                        }
                        result.push_back({static_cast<frontend::GenericTemplateKind>(specialization.kind),
                            specialization.templateKey,
                            specialization.specializationKey,
                            specialization.specializedName,
                            specialization.typeArguments,
                            hasConcreteDeclaration});
                    }
                    return result;
                }()});
    }
    if (!compilation.setPrecompiledModules(std::move(modules), error)) return false;
    return compilation.setPreloadedProjectGraph(std::move(*graph), error);
}

bool filesHaveEqualContents(const std::filesystem::path &left, const std::filesystem::path &right);

bool appendValidatedLibraryReferences(const frontend::ProjectCompilation &compilation,
    std::vector<std::filesystem::path> &libraries,
    std::vector<std::filesystem::path> &runtimeLibraries,
    std::string &error)
{
    error.clear();
    if (compilation.projectGraph() == nullptr)
    {
        error = "precompiled library validation requires a project graph";
        return false;
    }

    std::unordered_set<std::string> linkedLibraries;
    for (const std::filesystem::path &library : libraries)
    {
        linkedLibraries.insert(foldedPathKey(library));
    }
    std::unordered_set<std::string> validatedReferences;
    std::unordered_map<std::string, std::filesystem::path> runtimeNames;
    std::unordered_map<std::string, std::filesystem::path> runtimeByModule;
    std::unordered_map<std::string, std::filesystem::path> moduleOwners;
    std::unordered_map<std::string, std::vector<PrecompiledExportContract>> publishedExports;
    std::unordered_map<std::string, std::string> publishedNativeMethods;
    for (const compilation::Project &project : compilation.projectGraph()->projects())
    {
        moduleOwners.emplace(project.moduleName(), project.projectFile());
    }

    interop::LibraryMetadataCodec metadataCodec;
    for (const compilation::Project &project : compilation.projectGraph()->projects())
    {
        for (const compilation::LibraryReference &reference : project.libraryReferences())
        {
            const std::string libraryKey = foldedPathKey(reference.libraryPath);
            if (validatedReferences.contains(libraryKey)) continue;

            linker::CoffArchiveVerifier archiveVerifier;
            if (!archiveVerifier.verify(reference.libraryPath))
            {
                error = "precompiled library '" + reference.libraryPath.string() +
                        "' is not a valid COFF archive: " + archiveVerifier.error();
                return false;
            }

            interop::LibraryMetadata metadata;
            std::string metadataError;
            if (!metadataCodec.read(reference.metadataPath, metadata, metadataError))
            {
                error = "cannot read metadata for precompiled library '" + reference.libraryPath.string() +
                        "': " + metadataError;
                return false;
            }
            if (!metadataCodec.verifyCurrentCompiler(metadata, metadataError))
            {
                error = "incompatible metadata for precompiled library '" + reference.libraryPath.string() +
                        "': " + metadataError;
                return false;
            }
            if (metadata.moduleName.empty())
            {
                error = "precompiled library metadata has an empty module identity: '" +
                        reference.metadataPath.string() + "'";
                return false;
            }

            const std::unordered_set<std::string> archiveSymbols(
                archiveVerifier.info().symbolNames.begin(), archiveVerifier.info().symbolNames.end());
            for (const interop::LibraryMetadataExport &exportRecord : metadata.exports)
            {
                if (archiveSymbols.contains(exportRecord.cAbiName)) continue;
                error = "precompiled library '" + reference.libraryPath.string() + "' does not contain C ABI symbol '" +
                        exportRecord.cAbiName + "' published by metadata module '" + metadata.moduleName + "'";
                return false;
            }
            const std::vector<std::string> nativeSymbols = publishedNativeSymbols(metadata);
            for (const std::string &symbol : nativeSymbols)
            {
                if (!archiveSymbols.contains(symbol))
                {
                    error = "precompiled library '" + reference.libraryPath.string() +
                            "' does not contain NativeSharp symbol '" + symbol + "' published by metadata module '" +
                            metadata.moduleName + "'";
                    return false;
                }
                const auto [ownerIterator, inserted] = publishedNativeMethods.emplace(symbol, metadata.moduleName);
                if (!inserted && ownerIterator->second != metadata.moduleName)
                {
                    error = "NativeSharp symbol '" + symbol + "' is published by both module '" +
                            ownerIterator->second + "' and '" + metadata.moduleName + "'";
                    return false;
                }
            }

            const auto owner = moduleOwners.find(metadata.moduleName);
            if (owner != moduleOwners.end() && foldedPathKey(owner->second) != foldedPathKey(reference.metadataPath))
            {
                error = "duplicate module name '" + metadata.moduleName + "' in precompiled library '" +
                        reference.libraryPath.string() + "' and '" + owner->second.string() + "'";
                return false;
            }
            if (owner == moduleOwners.end()) moduleOwners.emplace(metadata.moduleName, reference.libraryPath);

            const std::string runtimeName =
                reference.runtimePath.empty() ? std::string() : foldedName(reference.runtimePath.filename().string());
            for (const interop::LibraryMetadataExport &exportRecord : metadata.exports)
            {
                publishedExports[exportRecord.cAbiName].push_back(
                    {metadata.moduleName, runtimeName, exportRecord.returnTypeKey, exportRecord.parameterTypeKeys});
            }

            if (!reference.runtimePath.empty())
            {
                linker::PeExecutableVerifier peVerifier;
                if (!peVerifier.verifyDynamicLibrary(reference.runtimePath))
                {
                    error =
                        "runtime '" + reference.runtimePath.string() + "' is not a valid DLL: " + peVerifier.error();
                    return false;
                }
                std::vector<std::string> expectedExports;
                expectedExports.reserve(metadata.exports.size() + nativeSymbols.size());
                for (const interop::LibraryMetadataExport &exportRecord : metadata.exports)
                {
                    expectedExports.push_back(exportRecord.cAbiName);
                }
                expectedExports.insert(expectedExports.end(), nativeSymbols.begin(), nativeSymbols.end());
                std::sort(expectedExports.begin(), expectedExports.end());
                expectedExports.erase(
                    std::unique(expectedExports.begin(), expectedExports.end()), expectedExports.end());
                std::vector<std::string> actualExports = peVerifier.info().exportedSymbols;
                std::sort(actualExports.begin(), actualExports.end());
                actualExports.erase(std::unique(actualExports.begin(), actualExports.end()), actualExports.end());
                if (actualExports != expectedExports)
                {
                    const std::vector<std::string> missing = missingSymbols(expectedExports, actualExports);
                    const std::vector<std::string> unexpected = missingSymbols(actualExports, expectedExports);
                    error = "runtime DLL export table does not match metadata contract "
                            "for module '" +
                            metadata.moduleName + "'";
                    if (!missing.empty())
                        error += "; missing " + std::to_string(missing.size()) + ": " + describeSymbols(missing);
                    if (!unexpected.empty())
                        error +=
                            "; unexpected " + std::to_string(unexpected.size()) + ": " + describeSymbols(unexpected);
                    return false;
                }
                const auto runtimeOwner = runtimeNames.find(runtimeName);
                if (runtimeOwner != runtimeNames.end() &&
                    foldedPathKey(runtimeOwner->second) != foldedPathKey(reference.runtimePath) &&
                    !filesHaveEqualContents(runtimeOwner->second, reference.runtimePath))
                {
                    error = "two precompiled library references require different DLLs "
                            "named '" +
                            reference.runtimePath.filename().string() + "'";
                    return false;
                }
                if (runtimeOwner == runtimeNames.end())
                {
                    runtimeNames.emplace(runtimeName, reference.runtimePath);
                }
                runtimeByModule.emplace(metadata.moduleName, reference.runtimePath);
            }

            validatedReferences.insert(libraryKey);
            if (!linkedLibraries.contains(libraryKey))
            {
                linkedLibraries.insert(libraryKey);
                libraries.push_back(reference.libraryPath);
            }
        }
    }

    if (compilation.oopMirModel() == nullptr) return true;
    std::unordered_set<std::string> referencedCallees;
    std::unordered_set<std::size_t> referencedStaticFields;
    const auto collectReferencedCallees =
        [&](const frontend::ProjectOopMirNode *node, auto &&self) -> void {
        if (node == nullptr) return;
        if (!node->calleeName.empty()) referencedCallees.insert(node->calleeName);
        if ((node->kind == frontend::ProjectOopMirNodeKind::StaticFieldLoad ||
             node->kind == frontend::ProjectOopMirNodeKind::StaticFieldAddress) &&
            node->memberOrdinal.has_value())
        {
            referencedStaticFields.insert(*node->memberOrdinal);
        }
        for (const auto &child : node->children) self(child.get(), self);
    };
    for (const frontend::ProjectOopMirFunction &function : compilation.oopMirModel()->functions())
    {
        collectReferencedCallees(function.body.get(), collectReferencedCallees);
    }
    for (const frontend::ProjectOopMirFieldInitializer &initializer :
         compilation.oopMirModel()->fieldInitializers())
    {
        collectReferencedCallees(initializer.value.get(), collectReferencedCallees);
    }
    std::unordered_set<std::string> selectedRuntimePaths;
    const auto selectRuntimeForModule = [&](const std::string &moduleName) {
        const auto runtime = runtimeByModule.find(moduleName);
        if (runtime == runtimeByModule.end()) return;
        const std::string key = foldedPathKey(runtime->second);
        if (selectedRuntimePaths.insert(key).second) runtimeLibraries.push_back(runtime->second);
    };
    for (const frontend::ProjectOopMirFunction &function : compilation.oopMirModel()->functions())
    {
        if (function.isImportedNative)
        {
            if (!referencedCallees.contains(function.mangledName)) continue;
            const auto owner = publishedNativeMethods.find(function.mangledName);
            if (owner == publishedNativeMethods.end())
            {
                error = "metadata-backed NativeSharp call resolved to unpublished "
                        "symbol '" +
                        function.mangledName + "'";
                return false;
            }
            selectRuntimeForModule(owner->second);
            continue;
        }
        if (!function.isImportedC) continue;
        if (!referencedCallees.contains(function.mangledName)) continue;
        const auto found = publishedExports.find(function.cAbiName);
        if (found == publishedExports.end()) continue;

        const PrecompiledExportContract *contract = nullptr;
        if (!function.importDllName.empty())
        {
            const std::string requestedRuntime =
                foldedName(std::filesystem::path(function.importDllName).filename().string());
            for (const PrecompiledExportContract &candidate : found->second)
            {
                if (candidate.runtimeName != requestedRuntime) continue;
                if (contract != nullptr)
                {
                    error = "extern C import '" + function.cAbiName + "' is published more than once by runtime DLL '" +
                            function.importDllName + "'";
                    return false;
                }
                contract = &candidate;
            }
            // This symbol may belong to a foreign DLL that happens to share its name
            // with an unrelated LibraryReference export. Only metadata for the
            // requested DLL is binding.
            if (contract == nullptr) continue;
        }
        else
        {
            if (found->second.size() != 1)
            {
                error = "extern C import '" + function.cAbiName +
                        "' is ambiguous across precompiled library metadata; qualify "
                        "the DLL import";
                return false;
            }
            contract = &found->second.front();
        }

        if (function.returnTypeKey != contract->returnTypeKey ||
            function.parameterTypeKeys != contract->parameterTypeKeys)
        {
            error = "extern C import '" + function.cAbiName +
                    "' does not match the metadata signature published by module '" + contract->moduleName + "'";
            return false;
        }
        // A C export implemented by the NativeSharp producer reaches that producer's runtime.
        // A reconstructed `extern C from "kernel32.dll"` import never enters metadata.exports
        // and therefore does not select the otherwise valid but unused NativeSharp DLL.
        selectRuntimeForModule(contract->moduleName);
    }
    for (const frontend::ProjectOopMirStaticField &field : compilation.oopMirModel()->staticFields())
    {
        if (!field.isImported || field.isConst ||
            !referencedStaticFields.contains(field.memberOrdinal))
        {
            continue;
        }
        const auto owner = publishedNativeMethods.find(field.accessorName);
        if (owner == publishedNativeMethods.end())
        {
            error = "metadata-backed NativeSharp static field resolved to unpublished accessor '" +
                field.accessorName + "'";
            return false;
        }
        selectRuntimeForModule(owner->second);
    }
    return true;
}

bool verifyLinkedRuntimeLibraries(
    std::span<const std::filesystem::path> runtimeLibraries, const linker::PeExecutableInfo &image, std::string &error)
{
    std::unordered_set<std::string> reachable;
    for (const std::string &imported : image.importedLibraries) reachable.insert(foldedName(imported));

    // An import archive is lazy: a runtime whose types are used only in another DLL's public
    // signature need not contribute a direct EXE import. Follow the declared runtime DLLs so a
    // valid transitive edge (Consumer -> Api.dll -> Types.dll) is certified without weakening
    // the Runtime identity check for libraries that do contribute symbols.
    std::unordered_set<std::string> expanded;
    bool changed = true;
    while (changed)
    {
        changed = false;
        for (const std::filesystem::path &runtime : runtimeLibraries)
        {
            const std::string runtimeName = foldedName(runtime.filename().string());
            if (!reachable.contains(runtimeName) || !expanded.insert(runtimeName).second) continue;
            linker::PeExecutableVerifier verifier;
            if (!verifier.verifyDynamicLibrary(runtime))
            {
                error = "runtime '" + runtime.string() + "' is not a valid DLL: " + verifier.error();
                return false;
            }
            for (const std::string &imported : verifier.info().importedLibraries)
            {
                changed = reachable.insert(foldedName(imported)).second || changed;
            }
        }
    }

    for (const std::filesystem::path &runtime : runtimeLibraries)
    {
        const std::string expected = foldedName(runtime.filename().string());
        if (!reachable.contains(expected))
        {
            error = "linked PE does not import declared runtime DLL '" + runtime.filename().string() +
                    "' directly or transitively; the import .lib does not match Runtime";
            return false;
        }
    }
    return true;
}

std::filesystem::path pathFromUtf8(std::string_view value)
{
    const std::u8string encoded(reinterpret_cast<const char8_t *>(value.data()), value.size());
    return std::filesystem::path(encoded);
}

bool filesHaveEqualContents(const std::filesystem::path &left, const std::filesystem::path &right)
{
    std::error_code sizeError;
    const std::uintmax_t leftSize = std::filesystem::file_size(left, sizeError);
    if (sizeError) return false;
    const std::uintmax_t rightSize = std::filesystem::file_size(right, sizeError);
    if (sizeError || leftSize != rightSize) return false;

    std::ifstream leftStream(left, std::ios::binary);
    std::ifstream rightStream(right, std::ios::binary);
    std::array<char, 64 * 1024> leftBuffer{};
    std::array<char, 64 * 1024> rightBuffer{};
    while (leftStream && rightStream)
    {
        leftStream.read(leftBuffer.data(), static_cast<std::streamsize>(leftBuffer.size()));
        rightStream.read(rightBuffer.data(), static_cast<std::streamsize>(rightBuffer.size()));
        const std::streamsize count = leftStream.gcount();
        if (count != rightStream.gcount() ||
            !std::equal(leftBuffer.begin(), leftBuffer.begin() + count, rightBuffer.begin()))
        {
            return false;
        }
    }
    return leftStream.eof() && rightStream.eof();
}

bool packageRuntimeClosure(std::span<const std::filesystem::path> runtimeLibraries,
                           const std::filesystem::path &destination,
                           std::vector<std::filesystem::path> &copiedFiles,
                           std::string &error)
{
    // Packaging is deliberately rooted in explicit LibraryReference runtimes.  A
    // dependency is followed only when the PE names it and a sibling DLL with that
    // exact name exists; no PATH lookup or directory scan is performed.
    std::vector<std::filesystem::path> closure;
    std::unordered_map<std::string, std::filesystem::path> owners;
    auto addRuntime = [&](const std::filesystem::path &runtime) {
        const std::string name = foldedName(runtime.filename().string());
        const auto owner = owners.find(name);
        if (owner != owners.end())
        {
            if (foldedPathKey(owner->second) != foldedPathKey(runtime) &&
                !filesHaveEqualContents(owner->second, runtime))
            {
                error = "runtime package contains different DLLs named '" + runtime.filename().string() + "': '" +
                        owner->second.string() + "' and '" + runtime.string() + "'";
                return false;
            }
            return true;
        }
        owners.emplace(name, runtime);
        closure.push_back(runtime);
        return true;
    };

    for (const std::filesystem::path &runtime : runtimeLibraries)
    {
        if (!addRuntime(runtime)) return false;
    }

    for (std::size_t index = 0; index < closure.size(); ++index)
    {
        linker::PeExecutableVerifier verifier;
        if (!verifier.verifyDynamicLibrary(closure[index]))
        {
            error = "runtime package dependency '" + closure[index].string() + "' is not a valid AMD64 DLL: " +
                    verifier.error();
            return false;
        }
        for (const std::string &importedName : verifier.info().importedLibraries)
        {
            const std::filesystem::path sibling = closure[index].parent_path() / importedName;
            std::error_code existsError;
            if (!std::filesystem::is_regular_file(sibling, existsError)) continue;
            if (!addRuntime(sibling)) return false;
        }
    }

    const std::filesystem::path packageDirectory = destination.parent_path();
    for (const std::filesystem::path &source : closure)
    {
        const std::filesystem::path target = packageDirectory / source.filename();
        std::error_code equivalentError;
        if (std::filesystem::equivalent(source, target, equivalentError) && !equivalentError) continue;
        std::error_code existsError;
        if (std::filesystem::exists(target, existsError))
        {
            if (!existsError && filesHaveEqualContents(source, target)) continue;
            error = "runtime package destination already contains a different '" + target.filename().string() +
                    "': '" + target.string() + "'";
            return false;
        }
        std::error_code copyError;
        std::filesystem::copy_file(source, target, std::filesystem::copy_options::none, copyError);
        if (copyError)
        {
            error = "cannot package runtime '" + source.string() + "' as '" + target.string() + "': " +
                    copyError.message();
            return false;
        }
        copiedFiles.push_back(target);
    }
    return true;
}

void removePackagedFiles(std::span<const std::filesystem::path> copiedFiles)
{
    std::error_code removeError;
    for (auto iterator = copiedFiles.rbegin(); iterator != copiedFiles.rend(); ++iterator)
    {
        std::filesystem::remove(*iterator, removeError);
        removeError.clear();
    }
}
} // namespace

ExitCode Driver::run(std::span<const std::string_view> arguments, std::ostream &output, std::ostream &error) const
{
    if (!arguments.empty() && isProjectCommand(arguments.front()))
    {
        return runProjectCommand(arguments, output, error,
            [this](std::span<const std::string_view> nested, std::ostream& nestedOutput,
                   std::ostream& nestedError) {
                return run(nested, nestedOutput, nestedError);
            });
    }
    DriverExecutionScope executionScope(output, error);
    llvm_backend::LlvmOptimizationBudgetScope optimizationBudgetScope(executionScope.o2Budget);
    CompilationJobPolicy jobPolicy;
    CompilationJobPolicyScope jobPolicyScope(jobPolicy);
    llvm_backend::CompilationArtifactCache artifactCache;
    artifactCache.setToolchainIdentity(std::string(kCompilerBuildId));
    llvm_backend::CompilationArtifactCacheScope artifactCacheScope(artifactCache);
    llvm_backend::FloatingPointModeScope floatingPointModeScope;
    DriverExecutionScope* previousScope = activeExecutionScope;
    activeExecutionScope = &executionScope;
    struct RestoreScope final
    {
        DriverExecutionScope* previous;
        ~RestoreScope() { activeExecutionScope = previous; }
    } restoreScope{previousScope};
    if (arguments.empty())
    {
        printHelp(output);
        return ExitCode::Success;
    }

    llvm_backend::LlvmOptimizationLevel optimizationLevel = llvm_backend::LlvmOptimizationLevel::O0;
    bool optimizationLevelSeen = false;
    std::optional<CompilationProfile> compilationProfile;
    bool compilationProfileSeen = false;
    bool optimizationBudgetSeen = false;
    bool jobCountSeen = false;
    bool reservedCoresSeen = false;
    bool artifactCacheSeen = false;
    bool libraryReferenceRootSeen = false;
    llvm_backend::LinkTimeOptimizationMode linkTimeOptimization =
        llvm_backend::LinkTimeOptimizationMode::None;
    bool linkTimeOptimizationSeen = false;
    bool incrementalLink = false;
    llvm_backend::FloatingPointMode floatingPointMode = llvm_backend::FloatingPointMode::Precise;
    bool floatingPointModeSeen = false;
    llvm_backend::FmaContractPolicy fmaContractPolicy = llvm_backend::FmaContractPolicy::Off;
    bool fmaContractPolicySeen = false;
    llvm_backend::FloatingPointRoundingMode floatingPointRoundingMode =
        llvm_backend::FloatingPointRoundingMode::Dynamic;
    bool floatingPointRoundingModeSeen = false;
    llvm_backend::FloatingPointDenormalMode floatingPointDenormalMode =
        llvm_backend::FloatingPointDenormalMode::Preserve;
    bool floatingPointDenormalModeSeen = false;
    llvm_backend::DebugInformationKind debugInformation = llvm_backend::DebugInformationKind::None;
    bool debugInformationSeen = false;
    std::vector<llvm_backend::DebugPrefixMap> debugPrefixMaps;
    linker::WindowsExecutableSubsystem subsystem = linker::WindowsExecutableSubsystem::Console;
    bool subsystemSeen = false;
    std::vector<std::string_view> normalizedArguments;
    normalizedArguments.reserve(arguments.size());
    // Interop Foundation: external .lib inputs for the link. Extracted here (like
    // -O) so every positional action keeps its exact shape; consumed by the
    // exe/dll emission paths.
    std::vector<std::filesystem::path> extraLinkLibraries;

    for (std::size_t argumentIndex = 0; argumentIndex < arguments.size(); ++argumentIndex)
    {
        const std::string_view argument = arguments[argumentIndex];
        const std::string foldedArgument = foldedName(std::string(argument));
        if (argument == "--time")
        {
            executionScope.reportTiming = true;
            continue;
        }
        if (argument == "--progress")
        {
            executionScope.reportProgress = true;
            continue;
        }
        if (argument == "--llvm-pass-trace")
        {
            continue;
        }
        if (argument == "--incremental-link")
        {
            incrementalLink = true;
            continue;
        }
        if (argument == "-flto")
        {
            if (linkTimeOptimizationSeen)
            {
                error << "nsc: error: duplicate link-time optimization mode\n";
                return ExitCode::CommandLineError;
            }
            linkTimeOptimizationSeen = true;
            linkTimeOptimization = llvm_backend::LinkTimeOptimizationMode::Full;
            continue;
        }
        if (argument.starts_with("-flto="))
        {
            if (linkTimeOptimizationSeen)
            {
                error << "nsc: error: duplicate link-time optimization mode\n";
                return ExitCode::CommandLineError;
            }
            linkTimeOptimizationSeen = true;
            if (argument == "-flto=thin")
                linkTimeOptimization = llvm_backend::LinkTimeOptimizationMode::Thin;
            else if (argument == "-flto=full")
                linkTimeOptimization = llvm_backend::LinkTimeOptimizationMode::Full;
            else
            {
                error << "nsc: error: unsupported link-time optimization mode '" << argument
                      << "'; expected -flto, -flto=full, or -flto=thin\n";
                return ExitCode::CommandLineError;
            }
            continue;
        }
        static constexpr std::string_view floatingPointModePrefix = "--fp-mode=";
        if (argument.starts_with(floatingPointModePrefix))
        {
            if (floatingPointModeSeen)
            {
                error << "nsc: error: duplicate floating-point mode\n";
                return ExitCode::CommandLineError;
            }
            floatingPointModeSeen = true;
            const std::optional<llvm_backend::FloatingPointMode> parsed =
                llvm_backend::parseFloatingPointMode(argument.substr(floatingPointModePrefix.size()));
            if (!parsed.has_value())
            {
                error << "nsc: error: unsupported floating-point mode '" << argument
                      << "'; expected --fp-mode=precise, fast, or deterministic\n";
                return ExitCode::CommandLineError;
            }
            floatingPointMode = *parsed;
            continue;
        }
        static constexpr std::string_view fmaContractPrefix = "--fp-contract=";
        if (argument.starts_with(fmaContractPrefix))
        {
            if (fmaContractPolicySeen)
            {
                error << "nsc: error: duplicate FMA contraction policy\n";
                return ExitCode::CommandLineError;
            }
            fmaContractPolicySeen = true;
            const std::optional<llvm_backend::FmaContractPolicy> parsed =
                llvm_backend::parseFmaContractPolicy(argument.substr(fmaContractPrefix.size()));
            if (!parsed.has_value())
            {
                error << "nsc: error: unsupported FMA contraction policy '" << argument
                      << "'; expected --fp-contract=off, on, or fast\n";
                return ExitCode::CommandLineError;
            }
            fmaContractPolicy = *parsed;
            continue;
        }
        static constexpr std::string_view floatingPointRoundingPrefix = "--fp-rounding=";
        if (argument.starts_with(floatingPointRoundingPrefix))
        {
            if (floatingPointRoundingModeSeen)
            {
                error << "nsc: error: duplicate floating-point rounding mode\n";
                return ExitCode::CommandLineError;
            }
            floatingPointRoundingModeSeen = true;
            const std::optional<llvm_backend::FloatingPointRoundingMode> parsed =
                llvm_backend::parseFloatingPointRoundingMode(
                    argument.substr(floatingPointRoundingPrefix.size()));
            if (!parsed.has_value())
            {
                error << "nsc: error: unsupported floating-point rounding mode '" << argument
                      << "'; expected --fp-rounding=dynamic, nearest-even, toward-zero, upward, or downward\n";
                return ExitCode::CommandLineError;
            }
            floatingPointRoundingMode = *parsed;
            continue;
        }
        static constexpr std::string_view floatingPointDenormalPrefix = "--fp-denormal=";
        if (argument.starts_with(floatingPointDenormalPrefix))
        {
            if (floatingPointDenormalModeSeen)
            {
                error << "nsc: error: duplicate floating-point denormal mode\n";
                return ExitCode::CommandLineError;
            }
            floatingPointDenormalModeSeen = true;
            const std::optional<llvm_backend::FloatingPointDenormalMode> parsed =
                llvm_backend::parseFloatingPointDenormalMode(
                    argument.substr(floatingPointDenormalPrefix.size()));
            if (!parsed.has_value())
            {
                error << "nsc: error: unsupported floating-point denormal mode '" << argument
                      << "'; expected --fp-denormal=preserve, ftz, daz, or ftz-daz\n";
                return ExitCode::CommandLineError;
            }
            floatingPointDenormalMode = *parsed;
            continue;
        }
        static constexpr std::string_view compilationProfilePrefix = "--profile=";
        if (argument.starts_with(compilationProfilePrefix))
        {
            if (compilationProfileSeen)
            {
                error << "nsc: error: duplicate compilation profile\n";
                return ExitCode::CommandLineError;
            }
            compilationProfileSeen = true;
            compilationProfile = parseCompilationProfile(
                argument.substr(compilationProfilePrefix.size()));
            if (!compilationProfile.has_value())
            {
                error << "nsc: error: unsupported compilation profile '" << argument
                      << "'; expected --profile=debug or --profile=release\n";
                return ExitCode::CommandLineError;
            }
            continue;
        }
        if (argument == "-j" || (argument.starts_with("-j") && argument.size() > 2U))
        {
            if (jobCountSeen)
            {
                error << "nsc: error: duplicate compilation job count\n";
                return ExitCode::CommandLineError;
            }
            jobCountSeen = true;
            std::string_view value;
            if (argument == "-j")
            {
                if (argumentIndex + 1U >= arguments.size())
                {
                    error << "nsc: error: -j requires a job count from 1 through 1024\n";
                    return ExitCode::CommandLineError;
                }
                value = arguments[++argumentIndex];
            }
            else
            {
                value = argument.substr(2);
            }
            std::size_t jobs = 0;
            if (!parseBoundedDecimal(value, kMaximumCompilationJobs, jobs) || jobs == 0)
            {
                error << "nsc: error: compilation job count must be between 1 and 1024\n";
                return ExitCode::CommandLineError;
            }
            jobPolicy.setRequestedJobs(jobs);
            continue;
        }
        static constexpr std::string_view reservedCoresPrefix = "--reserve-cores=";
        if (argument.starts_with(reservedCoresPrefix))
        {
            if (reservedCoresSeen)
            {
                error << "nsc: error: duplicate reserved-core count\n";
                return ExitCode::CommandLineError;
            }
            reservedCoresSeen = true;
            const std::string_view value = argument.substr(reservedCoresPrefix.size());
            std::size_t reserved = 0;
            if (!parseBoundedDecimal(value, kMaximumCompilationJobs - 1U, reserved))
            {
                error << "nsc: error: reserved-core count must be between 0 and 1023\n";
                return ExitCode::CommandLineError;
            }
            jobPolicy.setReservedCores(reserved);
            continue;
        }
        static constexpr std::string_view artifactCachePrefix = "--artifact-cache=";
        if (argument.starts_with(artifactCachePrefix))
        {
            if (artifactCacheSeen)
            {
                error << "nsc: error: duplicate artifact-cache directory\n";
                return ExitCode::CommandLineError;
            }
            artifactCacheSeen = true;
            const std::string_view value = argument.substr(artifactCachePrefix.size());
            const std::filesystem::path cachePath{std::string(value)};
            if (value.empty() || !cachePath.is_absolute())
            {
                error << "nsc: error: artifact cache requires an absolute directory: '"
                      << value << "'\n";
                return ExitCode::CommandLineError;
            }
            artifactCache.setRoot(cachePath);
            continue;
        }
        static constexpr std::string_view libraryReferenceRootOption = "--library-reference-root";
        static constexpr std::string_view libraryReferenceRootPrefix = "--library-reference-root=";
        if (argument == libraryReferenceRootOption || argument.starts_with(libraryReferenceRootPrefix))
        {
            if (libraryReferenceRootSeen)
            {
                error << "nsc: error: duplicate library-reference root\n";
                return ExitCode::CommandLineError;
            }
            libraryReferenceRootSeen = true;
            std::string_view value;
            if (argument == libraryReferenceRootOption)
            {
                if (argumentIndex + 1U >= arguments.size())
                {
                    error << "nsc: error: --library-reference-root requires an absolute directory\n";
                    return ExitCode::CommandLineError;
                }
                value = arguments[++argumentIndex];
            }
            else
            {
                value = argument.substr(libraryReferenceRootPrefix.size());
            }
            const std::filesystem::path root{std::string(value)};
            if (value.empty() || !root.is_absolute())
            {
                error << "nsc: error: library-reference root requires an absolute directory: '"
                      << value << "'\n";
                return ExitCode::CommandLineError;
            }
            executionScope.projectLoadOptions.libraryReferenceRoot = root.lexically_normal();
            continue;
        }
        static constexpr std::string_view timeoutPrefix = "--timeout-ms=";
        if (argument.starts_with(timeoutPrefix))
        {
            const std::string_view value = argument.substr(timeoutPrefix.size());
            std::uint64_t milliseconds = 0;
            if (value.empty() || value.size() > 8U ||
                !std::ranges::all_of(value, [](unsigned char ch) { return std::isdigit(ch) != 0; }))
            {
                error << "nsc: error: malformed timeout; expected --timeout-ms=1..86400000\n";
                return ExitCode::CommandLineError;
            }
            for (char ch : value) milliseconds = milliseconds * 10U + static_cast<unsigned>(ch - '0');
            if (milliseconds == 0 || milliseconds > 86400000U)
            {
                error << "nsc: error: timeout must be between 1 and 86400000 milliseconds\n";
                return ExitCode::CommandLineError;
            }
            executionScope.deadline = executionScope.started + std::chrono::milliseconds(milliseconds);
            continue;
        }
        static constexpr std::string_view o2BudgetPrefix = "--o2-budget-ms=";
        if (argument.starts_with(o2BudgetPrefix))
        {
            if (optimizationBudgetSeen)
            {
                error << "nsc: error: duplicate O2 optimization budget\n";
                return ExitCode::CommandLineError;
            }
            optimizationBudgetSeen = true;
            const std::string_view value = argument.substr(o2BudgetPrefix.size());
            std::uint64_t milliseconds = 0;
            if (value.empty() || value.size() > 8U ||
                !std::ranges::all_of(value, [](unsigned char ch) { return std::isdigit(ch) != 0; }))
            {
                error << "nsc: error: malformed O2 budget; expected --o2-budget-ms=1..86400000\n";
                return ExitCode::CommandLineError;
            }
            for (char ch : value) milliseconds = milliseconds * 10U + static_cast<unsigned>(ch - '0');
            if (milliseconds == 0 || milliseconds > 86400000U)
            {
                error << "nsc: error: O2 budget must be between 1 and 86400000 milliseconds\n";
                return ExitCode::CommandLineError;
            }
            executionScope.o2Budget.setBudgetMilliseconds(milliseconds);
            continue;
        }
        if (foldedArgument.starts_with("/subsystem:"))
        {
            if (subsystemSeen)
            {
                error << "nsc: error: duplicate executable subsystem\n";
                return ExitCode::CommandLineError;
            }
            subsystemSeen = true;
            if (foldedArgument == "/subsystem:console")
            {
                subsystem = linker::WindowsExecutableSubsystem::Console;
            }
            else if (foldedArgument == "/subsystem:windows")
            {
                subsystem = linker::WindowsExecutableSubsystem::Windows;
            }
            else
            {
                error << "nsc: error: unsupported executable subsystem '" << argument
                      << "'; expected /subsystem:console or /subsystem:windows\n";
                return ExitCode::CommandLineError;
            }
            continue;
        }
        if (argument == "--link-lib")
        {
            if (argumentIndex + 1 >= arguments.size())
            {
                error << "nsc: error: --link-lib requires a library file path\n";
                return ExitCode::CommandLineError;
            }
            extraLinkLibraries.emplace_back(std::string(arguments[++argumentIndex]));
            continue;
        }
        static constexpr std::string_view debugPrefixMapOption = "-fdebug-prefix-map=";
        if (argument.starts_with(debugPrefixMapOption))
        {
            const std::string_view mapping = argument.substr(debugPrefixMapOption.size());
            const std::size_t separator = mapping.find('=');
            if (separator == std::string_view::npos || separator == 0 || separator + 1 == mapping.size())
            {
                error << "nsc: error: malformed debug prefix map '" << argument
                      << "'; expected -fdebug-prefix-map=<absolute-source-prefix>=<replacement-prefix>\n";
                return ExitCode::CommandLineError;
            }
            std::filesystem::path sourcePrefix(std::string(mapping.substr(0, separator)));
            if (!sourcePrefix.is_absolute())
            {
                error << "nsc: error: debug prefix map source must be absolute: '"
                      << mapping.substr(0, separator) << "'\n";
                return ExitCode::CommandLineError;
            }
            debugPrefixMaps.push_back(llvm_backend::DebugPrefixMap{
                std::move(sourcePrefix),
                std::filesystem::path(std::string(mapping.substr(separator + 1)))});
            continue;
        }
        if (argument.starts_with("-fdebug-prefix-map"))
        {
            error << "nsc: error: malformed debug prefix map '" << argument
                  << "'; expected -fdebug-prefix-map=<absolute-source-prefix>=<replacement-prefix>\n";
            return ExitCode::CommandLineError;
        }
        if (argument.size() == 3 && argument[0] == '-' && argument[1] == 'O')
        {
            if (optimizationLevelSeen)
            {
                error << "nsc: error: duplicate LLVM optimization level\n";
                return ExitCode::CommandLineError;
            }
            optimizationLevelSeen = true;
            switch (argument[2])
            {
                case '0': optimizationLevel = llvm_backend::LlvmOptimizationLevel::O0; break;
                case '1': optimizationLevel = llvm_backend::LlvmOptimizationLevel::O1; break;
                case '2': optimizationLevel = llvm_backend::LlvmOptimizationLevel::O2; break;
                case '3': optimizationLevel = llvm_backend::LlvmOptimizationLevel::O3; break;
                default:
                    error << "nsc: error: unsupported LLVM optimization level '" << argument << "'\n";
                    return ExitCode::CommandLineError;
            }
            continue;
        }
        if (argument == "-g0")
        {
            debugInformation = llvm_backend::DebugInformationKind::None;
            debugInformationSeen = true;
            continue;
        }
        if (argument == "-gline-tables-only")
        {
            debugInformation = llvm_backend::DebugInformationKind::LineTablesOnly;
            debugInformationSeen = true;
            continue;
        }
        if (argument == "-g")
        {
            debugInformation = llvm_backend::DebugInformationKind::Full;
            debugInformationSeen = true;
            continue;
        }
        if (argument.starts_with("-g"))
        {
            error << "nsc: error: unsupported debug information option '" << argument
                  << "'; expected -g0, -gline-tables-only or -g\n";
            return ExitCode::CommandLineError;
        }
        if (argument.size() >= 2 && argument[0] == '-' && argument[1] == 'O')
        {
            error << "nsc: error: unsupported LLVM optimization level '" << argument << "'\n";
            return ExitCode::CommandLineError;
        }
        normalizedArguments.push_back(argument);
    }

    const CompilationProfileSettings profileSettings = resolveCompilationProfile(
        compilationProfile,
        optimizationLevelSeen
            ? std::optional<llvm_backend::LlvmOptimizationLevel>{optimizationLevel}
            : std::nullopt,
        debugInformationSeen
            ? std::optional<llvm_backend::DebugInformationKind>{debugInformation}
            : std::nullopt);
    optimizationLevel = profileSettings.optimizationLevel;
    debugInformation = profileSettings.debugInformation;
    floatingPointModeScope.set(floatingPointMode);
    if (!fmaContractPolicySeen && floatingPointMode == llvm_backend::FloatingPointMode::Fast)
        fmaContractPolicy = llvm_backend::FmaContractPolicy::Fast;
    floatingPointModeScope.setFmaContractPolicy(fmaContractPolicy);
    if (!floatingPointRoundingModeSeen &&
        floatingPointMode == llvm_backend::FloatingPointMode::Deterministic)
        floatingPointRoundingMode = llvm_backend::FloatingPointRoundingMode::NearestEven;
    floatingPointModeScope.setRoundingMode(floatingPointRoundingMode);
    floatingPointModeScope.setDenormalMode(floatingPointDenormalMode);

    if (floatingPointModeSeen)
        output << "floating-point-mode: " << llvm_backend::floatingPointModeName(floatingPointMode) << '\n';
    if (fmaContractPolicySeen)
        output << "fma-contract-policy: " << llvm_backend::fmaContractPolicyName(fmaContractPolicy) << '\n';
    if (floatingPointRoundingModeSeen)
        output << "floating-point-rounding: "
               << llvm_backend::floatingPointRoundingModeName(floatingPointRoundingMode) << '\n';
    if (floatingPointDenormalModeSeen)
        output << "floating-point-denormal: "
               << llvm_backend::floatingPointDenormalModeName(floatingPointDenormalMode) << '\n';

    if (normalizedArguments.empty())
    {
        error << "nsc: error: compilation options require an action or input file\n";
        return ExitCode::CommandLineError;
    }

    const std::span<const std::string_view> args(normalizedArguments);

    const bool executableInvocation =
        (args.size() == 3 && args[1] == "-o" && !args[0].empty() && args[0].front() != '-') ||
        (args.size() == 4 && args[2] == "-o" &&
            (args[0] == "--emit-exe" || args[0] == "--emit-project-oop-exe"));
    const bool debugCapableInvocation = args.size() == 4 && args[2] == "-o" &&
        (args[0] == "--emit-project-oop-obj" || args[0] == "--emit-project-oop-exe" ||
         args[0] == "--emit-project-oop-lib" || args[0] == "--emit-project-oop-dll");
    const bool artifactCacheCapableInvocation = args.size() == 4 && args[2] == "-o" &&
        (args[0] == "--emit-project-oop-exe" || args[0] == "--emit-project-oop-lib" ||
         args[0] == "--emit-project-oop-dll");
    const bool linkTimeOptimizationCapableInvocation = args.size() == 4 && args[2] == "-o" &&
        (args[0] == "--emit-project-oop-exe" || args[0] == "--emit-project-oop-lib" ||
         args[0] == "--emit-project-oop-dll");
    const bool incrementalLinkCapableInvocation = args.size() == 4 && args[2] == "-o" &&
        args[0] == "--emit-project-oop-exe";
    if (subsystemSeen && !executableInvocation)
    {
        error << "nsc: error: /subsystem is only valid when emitting an executable\n";
        return ExitCode::CommandLineError;
    }
    if (debugInformationSeen && !debugCapableInvocation)
    {
        error << "nsc: error: debug information is currently supported when emitting "
                 "a project OOP object, executable, static library, or DLL\n";
        return ExitCode::CommandLineError;
    }
    if (!debugPrefixMaps.empty() && !debugCapableInvocation)
    {
        error << "nsc: error: debug prefix maps are currently supported when emitting "
                 "a project OOP object, executable, static library, or DLL\n";
        return ExitCode::CommandLineError;
    }
    if (compilationProfileSeen && !debugCapableInvocation)
    {
        error << "nsc: error: compilation profiles are supported when emitting a project OOP "
                 "object, executable, static library, or DLL\n";
        return ExitCode::CommandLineError;
    }
    if (artifactCacheSeen && !artifactCacheCapableInvocation)
    {
        error << "nsc: error: --artifact-cache is supported when emitting a project OOP "
                 "executable, static library, or DLL\n";
        return ExitCode::CommandLineError;
    }
    if (linkTimeOptimizationSeen && !linkTimeOptimizationCapableInvocation)
    {
        error << "nsc: error: -flto is supported when emitting a project OOP executable, "
                 "static library, or DLL\n";
        return ExitCode::CommandLineError;
    }
    if (incrementalLink && !incrementalLinkCapableInvocation)
    {
        error << "nsc: error: --incremental-link is supported when emitting a project OOP executable\n";
        return ExitCode::CommandLineError;
    }

    if (compilationProfile.has_value())
    {
        output << "compilation-profile: " << compilationProfileName(*compilationProfile) << '\n'
               << "profile-optimization: "
               << llvm_backend::llvmOptimizationLevelName(optimizationLevel) << '\n'
               << "profile-debug-information: "
               << llvm_backend::debugInformationKindName(debugInformation) << '\n';
    }

    if (args.size() == 1)
    {
        const std::string_view argument = args.front();
        if (argument == "--help" || argument == "-h")
        {
            printHelp(output);
            return ExitCode::Success;
        }
        if (argument == "--version")
        {
            printVersion(output);
            return ExitCode::Success;
        }
        if (argument == "--print-target")
        {
            printTarget(output);
            return ExitCode::Success;
        }
        if (argument == "--print-data-layout")
        {
            printDataLayout(output);
            return ExitCode::Success;
        }
        if (argument == "--print-codegen")
        {
            return printCodeGeneration(optimizationLevel, output, error);
        }
        if (argument == "--print-llvm-pipeline")
        {
            printLlvmPipeline(optimizationLevel, output);
            return ExitCode::Success;
        }
        if (argument == "--print-llvm-version")
        {
            const llvm_backend::LlvmVersion version = llvm_backend::loadedLlvmVersion();
            output << "LLVM " << llvm_backend::llvmVersionString(version)
                   << " (supported: " << llvm_backend::kSupportedLlvmMajor << ".x)\n";
            std::string versionError;
            if (!llvm_backend::verifyLoadedLlvmVersion(versionError))
            {
                error << "nsc: internal error: " << versionError << '\n';
                return ExitCode::InternalError;
            }
            return ExitCode::Success;
        }
        if (argument == "--print-toolchain-contract")
        {
            output << currentToolchainContract();
            return ExitCode::Success;
        }
        if (argument == "--print-runtime-abi-contract")
        {
            output << runtime::runtimeAbiContractManifest();
            return ExitCode::Success;
        }
        if (argument == "--print-compatibility-policy")
        {
            output << currentCompatibilityPolicy();
            return ExitCode::Success;
        }
        if (argument == "--print-compatibility-matrix")
        {
            output << currentCompatibilityMatrix();
            return ExitCode::Success;
        }
        if (argument == "--print-breaking-change-ledger")
        {
            output << currentBreakingChangeLedger();
            return ExitCode::Success;
        }
        if (argument == "--print-job-policy")
        {
            output << "hardware-threads: " << jobPolicy.hardwareThreads() << '\n'
                   << "requested-jobs: ";
            if (jobPolicy.usesAutomaticJobs()) output << "auto";
            else output << jobPolicy.requestedJobs();
            output << '\n'
                   << "reserved-cores: " << jobPolicy.reservedCores() << '\n'
                   << "effective-jobs: " << jobPolicy.effectiveJobs() << '\n'
                   << "scheduler: project-references\n";
            return ExitCode::Success;
        }
        if (argument == "--print-migration-baseline")
        {
            output << currentMigrationBaseline();
            return ExitCode::Success;
        }
        if (argument == "--print-migration-guide")
        {
            output << currentMigrationGuide();
            return ExitCode::Success;
        }
        if (argument == "--print-release-candidate-freeze")
        {
            output << currentReleaseCandidateFreeze();
            return ExitCode::Success;
        }
        if (argument == "--print-final-baseline-contract")
        {
            output << currentFinalBaselineContract();
            return ExitCode::Success;
        }
        if (argument == "--print-promotion-act")
        {
            output << currentPromotionAct();
            return ExitCode::Success;
        }
        if (argument == "--print-linker")
        {
            return printLinker(output, error);
        }
        if (argument == "--dump-runtime-llvm")
        {
            return dumpRuntimeLlvm(output, error);
        }
        if (!argument.empty() && argument.front() == '-')
        {
            error << "nsc: error: unknown option '" << argument << "'\n";
            return ExitCode::CommandLineError;
        }
        return runFrontend("--program", argument, optimizationLevel, output, error);
    }

    if (args.size() == 2)
    {
        const std::string_view action = args[0];
        if (action == "--write-toolchain-contract")
        {
            std::string contractError;
            const std::filesystem::path destination{std::string(args[1])};
            if (!writeToolchainContract(destination, contractError))
            {
                error << "nsc: error: " << contractError << '\n';
                return ExitCode::CompilationError;
            }
            output << "toolchain contract written: " << destination.string() << '\n';
            return ExitCode::Success;
        }
        if (action == "--write-runtime-abi-contract")
        {
            std::string contractError;
            const std::filesystem::path destination{std::string(args[1])};
            if (!runtime::writeRuntimeAbiContract(destination, contractError))
            {
                error << "nsc: error: " << contractError << '\n';
                return ExitCode::CompilationError;
            }
            output << "runtime ABI contract written: " << destination.string() << '\n';
            return ExitCode::Success;
        }
        if (action == "--verify-runtime-abi-contract")
        {
            std::string contractError;
            const std::filesystem::path source{std::string(args[1])};
            if (!runtime::verifyRuntimeAbiContractFile(source, contractError))
            {
                error << "nsc: error: " << contractError << '\n';
                return ExitCode::CompilationError;
            }
            output << "runtime ABI contract verified: " << source.string() << '\n';
            return ExitCode::Success;
        }
        if (action == "--write-windows-runtime-library")
        {
            std::string runtimeError;
            const std::filesystem::path destination{std::string(args[1])};
            if (!linker::writeWindowsRuntimeLibrary(destination, runtimeError))
            {
                error << "nsc: error: " << runtimeError << '\n';
                return ExitCode::CompilationError;
            }
            output << "Windows runtime library written: " << destination.string() << '\n';
            return ExitCode::Success;
        }
        if (action == "--verify-windows-runtime-library")
        {
            std::string runtimeError;
            const std::filesystem::path source{std::string(args[1])};
            if (!linker::verifyWindowsRuntimeLibrary(source, runtimeError))
            {
                error << "nsc: error: " << runtimeError << '\n';
                return ExitCode::CompilationError;
            }
            output << "Windows runtime library verified: " << source.string() << '\n';
            return ExitCode::Success;
        }
        if (action == "--verify-pe")
        {
            const std::filesystem::path source{std::string(args[1])};
            linker::PeExecutableVerifier verifier;
            if (!verifier.verify(source))
            {
                error << "nsc: error: PE verification failed: " << verifier.error() << '\n';
                return ExitCode::CompilationError;
            }
            const linker::PeExecutableInfo& info = verifier.info();
            output << "PE executable verified: " << source.string()
                   << " (machine=0x" << std::hex << info.machine << std::dec
                   << ", sections=" << info.sectionCount
                   << ", imports=" << info.importedSymbols.size() << ")\n";
            return ExitCode::Success;
        }
        if (action == "--verify-toolchain-contract")
        {
            std::string contractError;
            const std::filesystem::path source{std::string(args[1])};
            if (!verifyToolchainContract(source, contractError))
            {
                error << "nsc: error: " << contractError << '\n';
                return ExitCode::CompilationError;
            }
            output << "toolchain contract verified: " << source.string() << '\n';
            return ExitCode::Success;
        }
        if (action == "--write-compatibility-policy")
        {
            std::string policyError;
            const std::filesystem::path destination{std::string(args[1])};
            if (!writeCompatibilityPolicy(destination, policyError))
            {
                error << "nsc: error: " << policyError << '\n';
                return ExitCode::CompilationError;
            }
            output << "compatibility policy written: " << destination.string() << '\n';
            return ExitCode::Success;
        }
        if (action == "--verify-compatibility-policy")
        {
            std::string policyError;
            const std::filesystem::path source{std::string(args[1])};
            if (!verifyCompatibilityPolicy(source, policyError))
            {
                error << "nsc: error: " << policyError << '\n';
                return ExitCode::CompilationError;
            }
            output << "compatibility policy verified: " << source.string() << '\n';
            return ExitCode::Success;
        }
        if (action == "--write-compatibility-matrix")
        {
            std::string matrixError;
            const std::filesystem::path destination{std::string(args[1])};
            if (!writeCompatibilityMatrix(destination, matrixError))
            {
                error << "nsc: error: " << matrixError << '\n';
                return ExitCode::CompilationError;
            }
            output << "compatibility matrix written: " << destination.string() << '\n';
            return ExitCode::Success;
        }
        if (action == "--verify-compatibility-matrix")
        {
            std::string matrixError;
            const std::filesystem::path source{std::string(args[1])};
            if (!verifyCompatibilityMatrix(source, matrixError))
            {
                error << "nsc: error: " << matrixError << '\n';
                return ExitCode::CompilationError;
            }
            output << "compatibility matrix verified: " << source.string() << '\n';
            return ExitCode::Success;
        }
        if (action == "--check-compiler-compatibility")
        {
            std::string explanation;
            std::string compatibilityError;
            const bool compatible = checkCompilerCompatibility(args[1], explanation, compatibilityError);
            if (!compatible)
            {
                error << "nsc: error: " << (compatibilityError.empty() ? explanation : compatibilityError) << '\n';
                return ExitCode::CompilationError;
            }
            output << explanation << '\n';
            return ExitCode::Success;
        }
        if (action == "--write-breaking-change-ledger")
        {
            std::string ledgerError;
            const std::filesystem::path destination{std::string(args[1])};
            if (!writeBreakingChangeLedger(destination, ledgerError))
            {
                error << "nsc: error: " << ledgerError << '\n';
                return ExitCode::CompilationError;
            }
            output << "breaking-change ledger written: " << destination.string() << '\n';
            return ExitCode::Success;
        }
        if (action == "--verify-breaking-change-ledger")
        {
            std::string ledgerError;
            const std::filesystem::path source{std::string(args[1])};
            if (!verifyBreakingChangeLedger(source, ledgerError))
            {
                error << "nsc: error: " << ledgerError << '\n';
                return ExitCode::CompilationError;
            }
            output << "breaking-change ledger verified: " << source.string() << '\n';
            return ExitCode::Success;
        }
        if (action == "--write-migration-baseline")
        {
            std::string baselineError;
            const std::filesystem::path destination{std::string(args[1])};
            if (!writeMigrationBaseline(destination, baselineError))
            {
                error << "nsc: error: " << baselineError << '\n';
                return ExitCode::CompilationError;
            }
            output << "migration baseline written: " << destination.string() << '\n';
            return ExitCode::Success;
        }
        if (action == "--verify-migration-baseline")
        {
            std::string baselineError;
            const std::filesystem::path source{std::string(args[1])};
            if (!verifyMigrationBaseline(source, baselineError))
            {
                error << "nsc: error: " << baselineError << '\n';
                return ExitCode::CompilationError;
            }
            output << "migration baseline verified: " << source.string() << '\n';
            return ExitCode::Success;
        }
        if (action == "--write-migration-guide")
        {
            std::string guideError;
            const std::filesystem::path destination{std::string(args[1])};
            if (!writeMigrationGuide(destination, guideError))
            {
                error << "nsc: error: " << guideError << '\n';
                return ExitCode::CompilationError;
            }
            output << "migration guide written: " << destination.string() << '\n';
            return ExitCode::Success;
        }
        if (action == "--verify-migration-guide")
        {
            std::string guideError;
            const std::filesystem::path source{std::string(args[1])};
            if (!verifyMigrationGuide(source, guideError))
            {
                error << "nsc: error: " << guideError << '\n';
                return ExitCode::CompilationError;
            }
            output << "migration guide verified: " << source.string() << '\n';
            return ExitCode::Success;
        }
        if (action == "--write-release-candidate-freeze")
        {
            std::string freezeError;
            const std::filesystem::path destination{std::string(args[1])};
            if (!writeReleaseCandidateFreeze(destination, freezeError))
            {
                error << "nsc: error: " << freezeError << '\n';
                return ExitCode::CompilationError;
            }
            output << "release-candidate freeze written: " << destination.string() << '\n';
            return ExitCode::Success;
        }
        if (action == "--verify-release-candidate-freeze")
        {
            std::string freezeError;
            const std::filesystem::path source{std::string(args[1])};
            if (!verifyReleaseCandidateFreeze(source, freezeError))
            {
                error << "nsc: error: " << freezeError << '\n';
                return ExitCode::CompilationError;
            }
            output << "release-candidate freeze verified: " << source.string() << '\n';
            return ExitCode::Success;
        }
        if (action == "--write-final-baseline-contract")
        {
            std::string contractError;
            const std::filesystem::path destination{std::string(args[1])};
            if (!writeFinalBaselineContract(destination, contractError))
            {
                error << "nsc: error: " << contractError << '\n';
                return ExitCode::CompilationError;
            }
            output << "final baseline contract written: " << destination.string() << '\n';
            return ExitCode::Success;
        }
        if (action == "--verify-final-baseline-contract")
        {
            std::string contractError;
            const std::filesystem::path source{std::string(args[1])};
            if (!verifyFinalBaselineContract(source, contractError))
            {
                error << "nsc: error: " << contractError << '\n';
                return ExitCode::CompilationError;
            }
            output << "final baseline contract verified: " << source.string() << '\n';
            return ExitCode::Success;
        }
        if (action == "--write-promotion-act")
        {
            std::string actError;
            const std::filesystem::path destination{std::string(args[1])};
            if (!writePromotionAct(destination, actError))
            {
                error << "nsc: error: " << actError << '\n';
                return ExitCode::CompilationError;
            }
            output << "promotion act written: " << destination.string() << '\n';
            return ExitCode::Success;
        }
        if (action == "--verify-promotion-act")
        {
            std::string actError;
            const std::filesystem::path source{std::string(args[1])};
            if (!verifyPromotionAct(source, actError))
            {
                error << "nsc: error: " << actError << '\n';
                return ExitCode::CompilationError;
            }
            output << "promotion act verified: " << source.string() << '\n';
            return ExitCode::Success;
        }
        if (action == "--verify-installation")
        {
            std::string installationError;
            const std::filesystem::path prefix{std::string(args[1])};
            if (!verifyInstallation(prefix, installationError))
            {
                error << "nsc: error: " << installationError << '\n';
                return ExitCode::CompilationError;
            }
            output << "NativeSharp installation verified: " << prefix.string() << '\n';
            return ExitCode::Success;
        }
        if (action == "--verify-library-metadata")
        {
            return verifyLibraryMetadata(args[1], output, error);
        }
        if (action == "--check-project" || action == "--dump-project" || action == "--check-project-graph" ||
            action == "--dump-project-graph" || action == "--check-project-syntax" ||
            action == "--dump-project-compilation" || action == "--check-project-generics" ||
            action == "--dump-project-generics" || action == "--check-project-names" ||
            action == "--dump-project-names" || action == "--check-project-members" ||
            action == "--dump-project-members" || action == "--check-project-inheritance" ||
            action == "--dump-project-inheritance" || action == "--check-project-interfaces" ||
            action == "--dump-project-interfaces" || action == "--check-project-generic-constraints" ||
            action == "--check-project-method-bodies" || action == "--dump-project-method-bodies" ||
            action == "--check-project-virtual-dispatch" || action == "--dump-project-virtual-dispatch" ||
            action == "--check-project-destruction" || action == "--dump-project-destruction" ||
            action == "--check-project-constructions" || action == "--dump-project-constructions" ||
            action == "--check-project-layouts" || action == "--dump-project-layouts" ||
            action == "--check-project-oop-hir" || action == "--dump-project-oop-hir" ||
            action == "--check-project-oop-mir" || action == "--dump-project-oop-mir" ||
            action == "--verify-project-oop-llvm" || action == "--dump-project-oop-llvm")
        {
            return runProject(action, args[1], output, error);
        }
        if (action == "--check" || action == "--dump-source" || action == "--dump-tokens" || action == "--dump-ast" ||
            action == "--dump-symbols" || action == "--dump-sema" || action == "--dump-hir" ||
            action == "--verify-hir" || action == "--dump-mir" || action == "--verify-mir" || action == "--dump-cfg" ||
            action == "--verify-cfg" || action == "--dump-dominance" || action == "--verify-dominance" ||
            action == "--dump-ssa" || action == "--verify-ssa" || action == "--dump-opt" || action == "--verify-opt" ||
            action == "--dump-pass-pipeline" || action == "--dump-llvm" || action == "--verify-llvm" ||
            action == "--dump-llvm-opt" || action == "--verify-llvm-opt")
        {
            return runFrontend(action, args[1], optimizationLevel, output, error);
        }
    }

    if (args.size() == 3 && args[0] == "--verify-library-artifacts")
    {
        return verifyLibraryArtifacts(args[1], args[2], {}, {}, output, error);
    }

    if (args.size() == 3 && args[0] == "--verify-installation-manifest")
    {
        std::string manifestError;
        const std::filesystem::path prefix{std::string(args[1])};
        const std::filesystem::path manifest{std::string(args[2])};
        if (!verifyInstallationManifest(prefix, manifest, manifestError))
        {
            error << "nsc: error: " << manifestError << '\n';
            return ExitCode::CompilationError;
        }
        output << "installation manifest verified: " << manifest.string() << '\n';
        return ExitCode::Success;
    }

    if (args.size() == 3 && args[0] == "--compare-installation-manifests")
    {
        ReproducibilityComparison comparison;
        std::string comparisonError;
        if (!compareInstallationManifests(std::filesystem::path(std::string(args[1])),
                std::filesystem::path(std::string(args[2])),
                comparison,
                comparisonError))
        {
            error << "nsc: error: " << comparisonError << '\n';
            return ExitCode::CompilationError;
        }
        output << comparison.report;
        if (!comparison.reproducible)
        {
            error << "nsc: error: installation builds differ in " << comparison.differences << " deterministic entr"
                  << (comparison.differences == 1 ? "y" : "ies") << '\n';
            return ExitCode::CompilationError;
        }
        return ExitCode::Success;
    }

    if (args.size() == 3 && args[0] == "--write-installation-removal-plan")
    {
        std::string planError;
        const std::filesystem::path destination{std::string(args[2])};
        if (!writeInstallationRemovalPlan(std::filesystem::path(std::string(args[1])), destination, planError))
        {
            error << "nsc: error: " << planError << '\n';
            return ExitCode::CompilationError;
        }
        output << "installation removal plan written: " << destination.string() << '\n';
        return ExitCode::Success;
    }

    if (args.size() == 4 && args[0] == "--verify-library-artifacts")
    {
        return verifyLibraryArtifacts(args[1], args[2], args[3], {}, output, error);
    }

    if (args.size() == 3 && args[0] == "--write-diagnostic-bundle")
    {
        std::ostringstream capturedOutput;
        std::ostringstream capturedError;
        const ExitCode result = runProject(
            "--verify-project-oop-llvm", args[1], capturedOutput, capturedError);
        std::string bundleError;
        if (!writeDiagnosticBundle(std::filesystem::path(std::string(args[1])),
                std::filesystem::path(std::string(args[2])), result,
                capturedOutput.str(), capturedError.str(), activeProjectLoadOptions(), bundleError))
        {
            error << "nsc: error: " << bundleError << '\n';
            return ExitCode::CompilationError;
        }
        output << "diagnostic bundle written: " << args[2] << '\n';
        return result;
    }

    if (args.size() == 5 && args[0] == "--verify-library-artifacts")
    {
        return verifyLibraryArtifacts(args[1], args[2], args[3], args[4], output, error);
    }

    if (args.size() == 4 && args[0] == "--write-installation-manifest")
    {
        std::string manifestError;
        const std::filesystem::path prefix{std::string(args[1])};
        const std::filesystem::path expectedFiles{std::string(args[2])};
        const std::filesystem::path destination{std::string(args[3])};
        if (!writeInstallationManifest(prefix, expectedFiles, destination, manifestError))
        {
            error << "nsc: error: " << manifestError << '\n';
            return ExitCode::CompilationError;
        }
        output << "installation manifest written: " << destination.string() << '\n';
        return ExitCode::Success;
    }

    if (args.size() == 4 && args[0] == "--write-installation-update-plan")
    {
        std::string planError;
        const std::filesystem::path destination{std::string(args[3])};
        if (!writeInstallationUpdatePlan(std::filesystem::path(std::string(args[1])),
                std::filesystem::path(std::string(args[2])),
                destination,
                planError))
        {
            error << "nsc: error: " << planError << '\n';
            return ExitCode::CompilationError;
        }
        output << "installation update plan written: " << destination.string() << '\n';
        return ExitCode::Success;
    }

    if (args.size() == 5 && args[0] == "--apply-installation-removal-plan")
    {
        std::string planError;
        const std::filesystem::path nextManifest =
            args[3] == "none" ? std::filesystem::path{} : std::filesystem::path(std::string(args[3]));
        if (!applyInstallationRemovalPlan(std::filesystem::path(std::string(args[1])),
                std::filesystem::path(std::string(args[2])),
                nextManifest,
                std::filesystem::path(std::string(args[4])),
                planError))
        {
            error << "nsc: error: " << planError << '\n';
            return ExitCode::CompilationError;
        }
        output << "installation removal plan applied: " << args[4] << '\n';
        return ExitCode::Success;
    }

    if (args.size() == 3 && args[1] == "-o" && !args[0].empty() && args[0].front() != '-')
    {
        return runExecutableGeneration(args[0], args[2], optimizationLevel, subsystem, output, error);
    }

    if (args.size() == 4 && args[2] == "-o")
    {
        const std::string_view action = args[0];
        if (action == "--emit-obj" || action == "--emit-asm")
        {
            return runCodeGeneration(action, args[1], args[3], optimizationLevel, output, error);
        }
        if (action == "--emit-project-oop-obj")
        {
            return runProjectOopCodeGeneration(
                args[1], args[3], optimizationLevel, debugInformation, debugPrefixMaps,
                output, error);
        }
        if (action == "--emit-project-oop-exe")
        {
            return runProjectOopExecutableGeneration(
                args[1], args[3], optimizationLevel, debugInformation, debugPrefixMaps,
                linkTimeOptimization,
                incrementalLink,
                subsystem, extraLinkLibraries, output, error);
        }
        if (action == "--emit-project-oop-lib")
        {
            return runProjectOopStaticLibraryGeneration(
                args[1], args[3], optimizationLevel, debugInformation, debugPrefixMaps,
                linkTimeOptimization, output, error);
        }
        if (action == "--emit-project-oop-dll")
        {
            return runProjectOopDynamicLibraryGeneration(
                args[1], args[3], optimizationLevel, debugInformation, debugPrefixMaps,
                linkTimeOptimization,
                extraLinkLibraries, output, error);
        }
        if (action == "--emit-project-host-header")
        {
            return runProjectHostHeaderGeneration(args[1], args[3], output, error);
        }
        if (action == "--emit-project-library-metadata")
        {
            return runProjectLibraryMetadataGeneration(args[1], args[3], output, error);
        }
        if (action == "--emit-exe")
        {
            return runExecutableGeneration(args[1], args[3], optimizationLevel, subsystem, output, error);
        }
    }

    error << "nsc: error: invalid command line; use --help for supported options "
             "(stage "
          << kCompilerStage << ")\n";
    return ExitCode::CommandLineError;
}

void Driver::printHelp(std::ostream &output)
{
    output << "NativeSharp compiler (NativeSharp " << kLanguageVersion << ", stage " << kCompilerStage << ")\n\n"
           << "Usage:\n"
           << "  nsc new <console|classlib|dll> -n <name> [-o <directory>] [--force]\n"
           << "  nsc restore [file.nsproj] [--force|--locked-mode|--verify] [--source <directory>]\n"
           << "  nsc build [file.nsproj] [-c Debug|Release] [-O0|-O1|-O2|-O3] "
              "[-g0|-gline-tables-only|-g] [--linker NSLink|lld-link] [--no-restore]\n"
           << "  nsc run [file.nsproj] [build options] [-- program arguments]\n"
           << "  nsc clean [file.nsproj]\n"
           << "  nsc add package <id> [--version <version>] [--source <directory>] [file.nsproj]\n"
           << "  nsc remove package <id> [file.nsproj]\n"
           << "  nsc list package [file.nsproj]\n"
           << "  Add --output-format=json to project commands for machine-readable output.\n\n"
           << "  nsc --check-project <file.nsproj>\n"
           << "  nsc --dump-project <file.nsproj>\n"
           << "  nsc --check-project-graph <file.nsproj>\n"
           << "  nsc --dump-project-graph <file.nsproj>\n"
           << "  nsc --check-project-syntax <file.nsproj>\n"
           << "  nsc --dump-project-compilation <file.nsproj>\n"
           << "  nsc --check-project-generics <file.nsproj>\n"
           << "  nsc --dump-project-generics <file.nsproj>\n"
           << "  nsc --check-project-names <file.nsproj>\n"
           << "  nsc --dump-project-names <file.nsproj>\n"
           << "  nsc --check-project-members <file.nsproj>\n"
           << "  nsc --dump-project-members <file.nsproj>\n"
           << "  nsc --check-project-inheritance <file.nsproj>\n"
           << "  nsc --dump-project-inheritance <file.nsproj>\n"
           << "  nsc --check-project-interfaces <file.nsproj>\n"
           << "  nsc --check-project-generic-constraints <file.nsproj>\n"
           << "  nsc --dump-project-interfaces <file.nsproj>\n"
           << "  nsc --check-project-method-bodies <file.nsproj>\n"
           << "  nsc --dump-project-method-bodies <file.nsproj>\n"
           << "  nsc --check-project-virtual-dispatch <file.nsproj>\n"
           << "  nsc --dump-project-virtual-dispatch <file.nsproj>\n"
           << "  nsc --check-project-destruction <file.nsproj>\n"
           << "  nsc --dump-project-destruction <file.nsproj>\n"
           << "  nsc --check-project-constructions <file.nsproj>\n"
           << "  nsc --dump-project-constructions <file.nsproj>\n"
           << "  nsc --check-project-layouts <file.nsproj>\n"
           << "  nsc --dump-project-layouts <file.nsproj>\n"
           << "  nsc --check-project-oop-hir <file.nsproj>\n"
           << "  nsc --dump-project-oop-hir <file.nsproj>\n"
           << "  nsc --check-project-oop-mir <file.nsproj>\n"
           << "  nsc --dump-project-oop-mir <file.nsproj>\n"
           << "  nsc --verify-project-oop-llvm <file.nsproj>\n"
           << "  nsc --dump-project-oop-llvm <file.nsproj>\n"
           << "  nsc --emit-project-oop-obj <file.nsproj> [-O0|-O1|-O2|-O3] -o "
              "<file.obj> [--profile=debug|release] [-g0|-gline-tables-only|-g] "
              "[-fdebug-prefix-map=<absolute-source-prefix>=<replacement-prefix>]\n"
           << "  nsc --emit-project-oop-exe <file.nsproj> [-O0|-O1|-O2|-O3] -o "
              "<file.exe> [--profile=debug|release] [-g0|-gline-tables-only|-g] "
              "[-fdebug-prefix-map=<absolute-source-prefix>=<replacement-prefix>] "
              "[/subsystem:console|/subsystem:windows]\n"
           << "  nsc --emit-project-oop-lib <file.nsproj> [-O0|-O1|-O2|-O3] -o "
              "<file.lib> [--profile=debug|release] [-g0|-gline-tables-only|-g] "
              "[-fdebug-prefix-map=<absolute-source-prefix>=<replacement-prefix>]\n"
           << "  nsc --emit-project-oop-dll <file.nsproj> [-O0|-O1|-O2|-O3] -o "
              "<file.dll> [--profile=debug|release] [-g0|-gline-tables-only|-g] "
              "[-fdebug-prefix-map=<absolute-source-prefix>=<replacement-prefix>]\n"
           << "  nsc --emit-project-host-header <file.nsproj> -o <file.h>\n"
           << "  nsc --emit-project-library-metadata <file.nsproj> -o "
              "<file.nsmeta>\n"
           << "  nsc --verify-library-metadata <file.nsmeta>\n"
           << "  nsc --verify-library-artifacts <file.nsmeta> <file.lib> "
              "[file.dll [file.pdb]]\n"
           << "  nsc --write-diagnostic-bundle <file.nsproj> <file.nsdiag>\n"
           << "  nsc --write-toolchain-contract <file.contract>\n"
           << "  nsc --verify-toolchain-contract <file.contract>\n"
           << "  nsc --print-runtime-abi-contract\n"
           << "  nsc --write-runtime-abi-contract <file.abi>\n"
           << "  nsc --verify-runtime-abi-contract <file.abi>\n"
           << "  nsc --write-windows-runtime-library <file.lib>\n"
           << "  nsc --verify-windows-runtime-library <file.lib>\n"
           << "  nsc --verify-pe <file.exe>\n"
           << "  nsc --print-compatibility-policy\n"
           << "  nsc --write-compatibility-policy <file.policy>\n"
           << "  nsc --verify-compatibility-policy <file.policy>\n"
           << "  nsc --check-compiler-compatibility <major.minor.patch>\n"
           << "  nsc --print-breaking-change-ledger\n"
           << "  nsc --write-breaking-change-ledger <file.ledger>\n"
           << "  nsc --verify-breaking-change-ledger <file.ledger>\n"
           << "  nsc --print-migration-baseline\n"
           << "  nsc --write-migration-baseline <file.baseline>\n"
           << "  nsc --verify-migration-baseline <file.baseline>\n"
           << "  nsc --print-migration-guide\n"
           << "  nsc --write-migration-guide <file.guide>\n"
           << "  nsc --verify-migration-guide <file.guide>\n"
           << "  nsc --print-release-candidate-freeze\n"
           << "  nsc --write-release-candidate-freeze <file.freeze>\n"
           << "  nsc --verify-release-candidate-freeze <file.freeze>\n"
           << "  nsc --print-final-baseline-contract\n"
           << "  nsc --write-final-baseline-contract <file.contract>\n"
           << "  nsc --verify-final-baseline-contract <file.contract>\n"
           << "  nsc --print-promotion-act\n"
           << "  nsc --write-promotion-act <file.act>\n"
           << "  nsc --verify-promotion-act <file.act>\n"
           << "  nsc --verify-installation <prefix>\n"
           << "  nsc --write-installation-manifest <prefix> <file-list> <manifest>\n"
           << "  nsc --verify-installation-manifest <prefix> <manifest>\n"
           << "  nsc --compare-installation-manifests <left> <right>\n"
           << "  nsc --write-installation-update-plan <current> <next> <plan>\n"
           << "  nsc --write-installation-removal-plan <current> <plan>\n"
           << "  nsc --apply-installation-removal-plan <prefix> <current> <next|none> <plan>\n"
           << "  nsc <file.ns>\n"
           << "  nsc --check <file.ns>\n"
           << "  nsc --dump-source <file.ns>\n"
           << "  nsc --dump-tokens <file.ns>\n"
           << "  nsc --dump-ast <file.ns>\n"
           << "  nsc --dump-symbols <file.ns>\n"
           << "  nsc --dump-sema <file.ns>\n"
           << "  nsc --dump-hir <file.ns>\n"
           << "  nsc --verify-hir <file.ns>\n"
           << "  nsc --dump-mir <file.ns>\n"
           << "  nsc --verify-mir <file.ns>\n"
           << "  nsc --dump-cfg <file.ns>\n"
           << "  nsc --verify-cfg <file.ns>\n"
           << "  nsc --dump-dominance <file.ns>\n"
           << "  nsc --verify-dominance <file.ns>\n"
           << "  nsc --dump-ssa <file.ns>\n"
           << "  nsc --verify-ssa <file.ns>\n"
           << "  nsc --dump-opt <file.ns>\n"
           << "  nsc --verify-opt <file.ns>\n"
           << "  nsc --dump-pass-pipeline <file.ns>\n"
           << "  nsc --dump-llvm <file.ns>\n"
           << "  nsc --verify-llvm <file.ns>\n"
           << "  nsc --dump-llvm-opt <file.ns> [-O0|-O1|-O2|-O3]\n"
           << "  nsc --verify-llvm-opt <file.ns> [-O0|-O1|-O2|-O3]\n"
           << "  nsc --emit-obj <file.ns> [-O0|-O1|-O2|-O3] -o <file.obj>\n"
           << "  nsc --emit-asm <file.ns> [-O0|-O1|-O2|-O3] -o <file.asm>\n"
           << "  nsc --emit-exe <file.ns> [-O0|-O1|-O2|-O3] -o <file.exe> "
              "[/subsystem:console|/subsystem:windows]\n"
           << "  nsc <file.ns> [-O0|-O1|-O2|-O3] -o <file.exe> "
              "[/subsystem:console|/subsystem:windows]\n"
           << "  nsc --print-codegen [-O0|-O1|-O2|-O3]\n"
           << "  nsc --print-llvm-pipeline [-O0|-O1|-O2|-O3]\n"
           << "  nsc --print-llvm-version\n"
           << "  nsc --print-toolchain-contract\n"
           << "  nsc --print-runtime-abi-contract\n"
           << "  nsc --print-linker\n"
           << "  nsc --dump-runtime-llvm\n"
           << "  nsc --print-target\n"
           << "  nsc --print-data-layout\n"
           << "  nsc --version\n\n"
           << "Optimization flags -O0, -O1, -O2 and -O3 may appear anywhere in a "
              "compilation command.\n"
           << "--fp-mode=precise|fast|deterministic selects the floating-point IR contract; "
              "precise is the default and fast is explicit opt-in.\n"
           << "--fp-contract=off|on|fast controls whether multiply/add sequences may contract "
              "to FMA; off is the strict default.\n"
           << "--fp-rounding=dynamic|nearest-even|toward-zero|upward|downward controls constrained "
              "floating-point rounding; deterministic mode defaults to nearest-even.\n"
           << "--fp-denormal=preserve|ftz|daz|ftz-daz controls subnormal inputs and results.\n"
           << "--time reports deterministic per-phase microseconds plus total wall time; "
              "--timeout-ms=N requests cooperative cancellation at safe project phase boundaries.\n"
           << "--progress streams and flushes phase boundaries plus source read, lex and parse "
              "completion so long-running builds remain observable.\n"
           << "--quiet suppresses normal stdout; --verbose enables progress plus timing. "
              "--time-report is an alias for --time and --time-trace=<file> writes a JSON trace.\n"
           << "--log=<file> records stdout, stderr, arguments and exit status; "
              "--log-format=text|json selects its schema and --log-on-error=<file> publishes only "
              "after failure. Logs are written transactionally.\n"
           << "--o2-budget-ms=N sets the advisory cumulative LLVM O2 budget; exceeding it warns "
              "without changing the artifact or exit code.\n"
           << "--llvm-pass-trace enables the expensive full LLVM pass log and verify-each "
              "diagnostics for optimized builds.\n"
           << "-j N (or -jN) caps compiler jobs; --reserve-cores=N keeps host capacity free. "
              "Use --print-job-policy to inspect the effective value.\n"
           << "--artifact-cache=<absolute-directory> enables verified content-addressed reuse "
              "for optimized project object units.\n"
           << "--library-reference-root <absolute-directory> resolves relative LibraryReference "
              "Include, Metadata, and Runtime paths from one configuration-specific directory.\n"
           << "--profile=debug selects O0 with full debug information; --profile=release selects "
              "O2 with line tables. Explicit -O and -g flags override profile defaults.\n"
           << "Debug flags -g0, -gline-tables-only and -g are independent from "
              "optimization and currently apply to project OOP object/executable emission.\n"
           << "NSC_LLD_LINK selects the PE linker; NSC_LLVM_LIB independently "
              "selects the static librarian.\n"
           << "Stage 37 semantics remain preserved: static members, deterministic "
              "initialization, const and readonly.\n"
           << "Recovered Stage 35 adds abstract/sealed classes and methods plus "
              "descriptor-driven native destruction.\n"
           << "Stage 36 interfaces, Stage 38 properties/indexers/operators, and "
              "Stage 39 native RTTI remain preserved. "
           << "Stage 40 adds native struct value ABI; Stage 41 arrays/strings/Core; "
              "Stage 42 delegates/events; "
           << "Stage 43 lambdas/closures; and Stage 44 AOT generics by "
              "monomorphization. "
           << "Stage 45 adds native Win64 exceptions, cleanup/finally/using, RTTI "
              "catches, and PE EH closure. "
           << "Stage 46 adds static COFF .lib and PE32+ DLL + import-library "
              "products; "
           << "Stage 47 adds stable Windows x64 export extern C host ABI thunks; "
           << "Stage 48.1 adds deterministic generated C/C++ host headers from the "
              "semantic ABI model; "
           << "Stage 48.2 adds library metadata (.nsmeta) with self-verification. "
           << "Stage 49 adds `raii` locals with scope-bound native destruction; "
           << "Stage 50 adds the ownership type system (owned/borrow parameters, "
              "returns and lifetimes). "
           << "Stage 51 completes the native string type; Stage 52 adds generic "
              "collections, `var` and "
           << "`foreach`. "
           << "Stage 53 adds the platform library: time, environment and "
              "environment variables, "
           << "filesystem paths and contents, command-line arguments, and directory "
              "enumeration. "
           << "Stage 54 adds threading: threads, atomics, mutexes, monitors, "
              "semaphores and events. "
           << "Stage 55 adds process, hardware and diagnostics support. "
           << "Stage 56 completes interop and the type system: C aggregates, FFI "
              "aliases, "
           << "ref/out/in, struct members, definite assignment and the Object type "
              "root. "
           << "Stage 57 completes the language surface: the conditional operator, "
              "do-while and "
           << "switch/case. "
           << "The 1.0 library gates add component-level ABI diagnostics and "
              "standalone metadata/archive/DLL artifact verification. "
           << "This executable is the stable NativeSharp 2.0 release; "
              "the installed current documentation is under share/nativesharp/docs/2.0.\n";
}

ExitCode Driver::printCodeGeneration(
    llvm_backend::LlvmOptimizationLevel optimizationLevel, std::ostream &output, std::ostream &error)
{
    std::string targetError;
    std::unique_ptr<llvm_backend::LlvmTargetMachine> targetMachine =
        llvm_backend::LlvmTargetMachine::createWindowsX64(optimizationLevel, targetError);
    if (targetMachine == nullptr)
    {
        error << "nsc: internal error: target machine creation failed: " << targetError << '\n';
        return ExitCode::CompilationError;
    }

    output << "target-triple: " << targetMachine->triple() << '\n'
           << "data-layout:   " << targetMachine->dataLayout() << '\n'
           << "object-format: COFF\n"
           << "machine:       AMD64 (0x8664)\n"
           << "optimization:  " << llvm_backend::llvmOptimizationLevelName(optimizationLevel) << '\n';
    return ExitCode::Success;
}

void Driver::printLlvmPipeline(llvm_backend::LlvmOptimizationLevel optimizationLevel, std::ostream &output)
{
    output << "optimization-level: " << llvm_backend::llvmOptimizationLevelName(optimizationLevel) << '\n'
           << "pass-pipeline: " << llvm_backend::llvmOptimizationPipelineName(optimizationLevel) << '\n'
           << "target-aware: yes\n"
           << "verify-each: "
           << (llvm_backend::llvmVerifyEachPassRequired(
                   optimizationLevel, llvm_backend::llvmProcessTraceActive())
                   ? "yes"
                   : "no")
           << '\n';
}

ExitCode Driver::runCodeGeneration(std::string_view action,
    std::string_view inputPath,
    std::string_view outputPath,
    llvm_backend::LlvmOptimizationLevel optimizationLevel,
    std::ostream &output,
    std::ostream &error)
{
    if (outputPath.empty())
    {
        error << "nsc: error: code generation output path is empty\n";
        return ExitCode::CommandLineError;
    }

    frontend::FrontendCompilation compilation;
    std::string loadError;
    const bool success = compilation.compileFile(std::filesystem::path(inputPath), loadError, false);
    if (!loadError.empty())
    {
        error << "nsc: error: " << loadError << '\n';
        return ExitCode::CompilationError;
    }
    if (!success || compilation.optimizationModule() == nullptr || compilation.semanticAnalyzer() == nullptr)
    {
        if (compilation.diagnostics().hasErrors())
        {
            diagnostics::DiagnosticRenderer renderer(compilation.sourceManager());
            renderer.renderAll(compilation.diagnostics(), error);
        }
        error << "nsc: error: code generation requires a successful optimized "
                 "compilation\n";
        return ExitCode::CompilationError;
    }

    llvm_backend::LlvmLowerer lowerer(compilation.semanticAnalyzer()->globalSymbols());
    std::unique_ptr<llvm_backend::LlvmModule> llvmModule = lowerer.lower(compilation.optimizationModule());
    if (llvmModule == nullptr)
    {
        error << "nsc: internal error: LLVM lowering failed: " << lowerer.error() << '\n';
        return ExitCode::CompilationError;
    }

    std::string verificationError;
    if (!llvmModule->verify(verificationError))
    {
        error << "nsc: internal error: LLVM verification failed: " << verificationError << '\n';
        return ExitCode::CompilationError;
    }

    std::string targetError;
    std::unique_ptr<llvm_backend::LlvmTargetMachine> targetMachine =
        llvm_backend::LlvmTargetMachine::createWindowsX64(optimizationLevel, targetError);
    if (targetMachine == nullptr)
    {
        error << "nsc: internal error: target machine creation failed: " << targetError << '\n';
        return ExitCode::CompilationError;
    }

    // NSC-LIMIT-005. La observacion va SIEMPRE, no bajo bandera. Un fallo del
    // optimizador que no dice contra que LLVM, con que pipeline ni por donde iba
    // obliga a REPRODUCIRLO para poder diagnosticarlo, y eso es exactamente lo
    // que no se pudo hacer con `c38_enum_ancho`: cuando el sintoma es un codigo
    // de salida distinto a -O2, la corrida que fallo ya termino. Ver
    // LlvmOptimizer.h y LlvmOptimizationObservation.h.
    //
    // COSTE MEDIDO: ~3% del tiempo de compilacion completo a -O2 y 0% a -O0, y el
    // IR resultante es byte a byte el mismo con y sin observar --hay una prueba
    // que lo compara en los tres niveles--. Se paga en cada construccion a cambio
    // de que el PRIMER fallo sea legible.
    llvm_backend::LlvmOptimizer optimizer;
    llvm_backend::LlvmOptimizationObservation observation;
    std::string optimizationError;
    if (!optimizer.optimize(*llvmModule, *targetMachine, optimizationLevel, &observation, optimizationError))
    {
        error << "nsc: internal error: LLVM optimization failed: " << optimizationError << '\n';
        return ExitCode::CompilationError;
    }

    const llvm_backend::LlvmCodeGenFileType fileType = action == "--emit-obj"
                                                           ? llvm_backend::LlvmCodeGenFileType::Object
                                                           : llvm_backend::LlvmCodeGenFileType::Assembly;
    const std::filesystem::path destination = pathFromUtf8(outputPath);
    std::string emissionError;
    if (!targetMachine->emit(*llvmModule, destination, fileType, emissionError))
    {
        error << "nsc: error: target emission failed: " << emissionError << '\n';
        return ExitCode::CompilationError;
    }

    if (fileType == llvm_backend::LlvmCodeGenFileType::Object)
    {
        llvm_backend::CoffObjectVerifier verifier;
        if (!verifier.verify(destination))
        {
            error << "nsc: internal error: emitted COFF verification failed: " << verifier.error() << '\n';
            std::error_code removeError;
            std::filesystem::remove(destination, removeError);
            return ExitCode::CompilationError;
        }
        output << "COFF object emission: ok\n"
               << "machine: AMD64 (0x8664)\n"
               << "optimization: " << llvm_backend::llvmOptimizationLevelName(optimizationLevel) << '\n'
               << "sections: " << verifier.info().sectionCount << '\n';
    }
    else
    {
        std::error_code sizeError;
        const std::uintmax_t size = std::filesystem::file_size(destination, sizeError);
        if (sizeError || size == 0)
        {
            error << "nsc: internal error: emitted assembly file is empty or "
                     "unreadable\n";
            return ExitCode::CompilationError;
        }
        output << "assembly emission: ok\n"
               << "optimization: " << llvm_backend::llvmOptimizationLevelName(optimizationLevel) << '\n'
               << "bytes: " << size << '\n';
    }
    return ExitCode::Success;
}

ExitCode Driver::printLinker(std::ostream &output, std::ostream &error)
{
    std::string linkerError;
    std::unique_ptr<linker::LldLinker> linker = linker::LldLinker::discover(linkerError);
    if (linker == nullptr)
    {
        error << "nsc: error: " << linkerError << '\n';
        return ExitCode::CompilationError;
    }

    output << "linker: " << linker->executablePath().string() << '\n'
           << "librarian: "
           << (linker->librarianExecutablePath().empty() ? std::string("unavailable (set NSC_LLVM_LIB)")
                                                         : linker->librarianExecutablePath().string())
           << '\n'
           << "flavor: Windows COFF/PE\n"
           << "target: x86_64-pc-windows-msvc\n"
           << "subsystem: console\n";
    return ExitCode::Success;
}

ExitCode Driver::dumpRuntimeLlvm(std::ostream &output, std::ostream &error)
{
    runtime::WindowsRuntimeBuilder builder;
    std::string runtimeError;
    std::unique_ptr<llvm_backend::LlvmModule> module = builder.build(runtimeError);
    if (module == nullptr)
    {
        error << "nsc: internal error: runtime construction failed: " << runtimeError << '\n';
        return ExitCode::CompilationError;
    }

    output << module->text();
    return ExitCode::Success;
}

ExitCode Driver::runExecutableGeneration(std::string_view inputPath,
    std::string_view outputPath,
    llvm_backend::LlvmOptimizationLevel optimizationLevel,
    linker::WindowsExecutableSubsystem subsystem,
    std::ostream &output,
    std::ostream &error)
{
    if (outputPath.empty())
    {
        error << "nsc: error: executable output path is empty\n";
        return ExitCode::CommandLineError;
    }

    frontend::FrontendCompilation compilation;
    std::string loadError;
    const bool success = compilation.compileFile(std::filesystem::path(inputPath), loadError, true);
    if (!loadError.empty())
    {
        error << "nsc: error: " << loadError << '\n';
        return ExitCode::CompilationError;
    }
    if (!success || compilation.optimizationModule() == nullptr || compilation.semanticAnalyzer() == nullptr)
    {
        if (compilation.diagnostics().hasErrors())
        {
            diagnostics::DiagnosticRenderer renderer(compilation.sourceManager());
            renderer.renderAll(compilation.diagnostics(), error);
        }
        error << "nsc: error: executable generation requires a valid int Main() "
                 "program\n";
        return ExitCode::CompilationError;
    }

    llvm_backend::LlvmLowerer lowerer(compilation.semanticAnalyzer()->globalSymbols());
    std::unique_ptr<llvm_backend::LlvmModule> module = lowerer.lower(compilation.optimizationModule());
    if (module == nullptr)
    {
        error << "nsc: internal error: LLVM lowering failed: " << lowerer.error() << '\n';
        return ExitCode::CompilationError;
    }

    std::string verificationError;
    if (!module->verify(verificationError))
    {
        error << "nsc: internal error: LLVM verification failed: " << verificationError << '\n';
        return ExitCode::CompilationError;
    }

    linker::WindowsExecutableBuilder builder;
    builder.setRuntimeLibrary(linker::configuredWindowsRuntimeLibrary());
    builder.setSubsystem(subsystem);
    std::string buildError;
    const std::filesystem::path destination = pathFromUtf8(outputPath);
    if (!builder.build(*module, destination, optimizationLevel, buildError))
    {
        error << "nsc: error: Windows executable generation failed: " << buildError << '\n';
        return ExitCode::CompilationError;
    }

    output << "PE executable emission: ok\n"
           << "machine: AMD64 (0x8664)\n"
           << "format: PE32+\n"
           << "subsystem: " << subsystemDisplayName(subsystem) << '\n'
           << "optimization: " << llvm_backend::llvmOptimizationLevelName(optimizationLevel) << '\n'
           << "sections: " << builder.info().sectionCount << '\n'
           << "entry-rva: " << builder.info().entryPointRva << '\n'
           << "imports:";
    for (const std::string &library : builder.info().importedLibraries)
    {
        output << ' ' << library;
    }
    output << '\n';
    return ExitCode::Success;
}

void Driver::printVersion(std::ostream &output)
{
    // CC1-000: the unique build id appears both in the SemVer build metadata of
    // `kCompilerVersion` and on its own machine-friendly line. A consumer can
    // compare either one without hashing nsc.exe.
    output << "nsc " << kCompilerVersion << " (NativeSharp " << kLanguageVersion << ", stage " << kCompilerStage
           << ")\n"
           << "build " << kCompilerBuildId << '\n';
}

void Driver::printTarget(std::ostream &output)
{
    const target::TargetInfo targetInfo = target::TargetInfo::WindowsX64();
    output << "architecture: " << targetInfo.architectureName() << '\n'
           << "os:           " << targetInfo.operatingSystemName() << '\n'
           << "object:       " << targetInfo.objectFormatName() << '\n'
           << "abi:          " << targetInfo.abiName() << '\n'
           << "llvm-triple:  " << targetInfo.llvmTriple() << '\n';
}

void Driver::printDataLayout(std::ostream &output)
{
    const target::TargetDataLayout dataLayout = target::TargetDataLayout::WindowsX64();
    output << "endianness: little\n"
           << "max-natural-alignment: " << dataLayout.maximumNaturalAlignment() << "\n"
           << "type size align\n";

    for (const LayoutRow &row : kLayoutRows)
    {
        const target::TypeLayout layout = dataLayout.layoutOf(row.kind);
        output << row.name << ' ' << layout.size << ' ' << layout.alignment << '\n';
    }
}

ExitCode Driver::runProject(std::string_view action, std::string_view path, std::ostream &output, std::ostream &error)
{
    if (action == "--check-project" || action == "--dump-project")
    {
        compilation::ProjectLoader loader;
        std::string loadError;
        const std::optional<compilation::Project> project = loader.load(
            std::filesystem::path(path), activeProjectLoadOptions(), loadError);
        if (!project.has_value())
        {
            error << "nsc: error: " << loadError << '\n';
            return ExitCode::CompilationError;
        }

        if (action == "--dump-project")
        {
            compilation::ProjectDumper dumper;
            dumper.dump(*project, output);
            return ExitCode::Success;
        }

        const std::size_t sourceCount = project->sourceSet().sources().size();
        const std::size_t referenceCount = project->projectReferences().size();
        output << "project check: ok (" << sourceCount << " source" << (sourceCount == 1 ? "" : "s") << ", "
               << referenceCount << " project reference" << (referenceCount == 1 ? "" : "s") << ")\n";
        return ExitCode::Success;
    }

    if (action == "--check-project-graph" || action == "--dump-project-graph")
    {
        const CompilationJobPolicy* const jobs = activeCompilationJobPolicy();
        compilation::ProjectGraphLoader loader(
            jobs != nullptr ? jobs->effectiveJobs() : 1,
            {},
            activeProjectLoadOptions());
        std::string loadError;
        const std::optional<compilation::ProjectGraph> graph = loader.load(std::filesystem::path(path), loadError);
        if (!graph.has_value())
        {
            error << "nsc: error: " << loadError << '\n';
            return ExitCode::CompilationError;
        }

        if (action == "--dump-project-graph")
        {
            compilation::ProjectGraphDumper dumper;
            dumper.dump(*graph, output);
            return ExitCode::Success;
        }

        output << "project graph check: ok (" << graph->projects().size() << " project"
               << (graph->projects().size() == 1 ? "" : "s") << ")\n";
        return ExitCode::Success;
    }

    frontend::ProjectCompilation projectCompilation;
    configureProjectExecution(projectCompilation);
    std::string loadError;
    if (!measureDriverPhase("precompiled-metadata", [&] {
            return configurePrecompiledModules(
                std::filesystem::path(path), projectCompilation, loadError);
        }))
    {
        error << "nsc: error: " << loadError << '\n';
        return ExitCode::CompilationError;
    }
    const bool oopLlvmAction = action == "--verify-project-oop-llvm" || action == "--dump-project-oop-llvm";
    const bool oopMirAction =
        oopLlvmAction || action == "--check-project-oop-mir" || action == "--dump-project-oop-mir";
    const bool oopHirAction = oopMirAction || action == "--check-project-oop-hir" || action == "--dump-project-oop-hir";
    const bool layoutAction = oopHirAction || action == "--check-project-layouts" || action == "--dump-project-layouts";
    const bool constructionAction =
        layoutAction || action == "--check-project-constructions" || action == "--dump-project-constructions";
    const bool destructionAction =
        constructionAction || action == "--check-project-destruction" || action == "--dump-project-destruction";
    const bool virtualDispatchAction = destructionAction || action == "--check-project-virtual-dispatch" ||
                                       action == "--dump-project-virtual-dispatch";
    const bool methodBodyAction =
        virtualDispatchAction || action == "--check-project-method-bodies" || action == "--dump-project-method-bodies";
    // NSC-LIMIT-007. FASE PROPIA, entre las interfaces y los cuerpos. Va DEBAJO
    // de methodBodyAction
    // --toda fase posterior la arrastra-- y ENCIMA de interfaceAction, que es lo
    // ultimo que necesita. Antes se sumaba a `methodBodyAction`, y entonces pedir
    // la comprobacion de constraints analizaba tambien todos los cuerpos: un rojo
    // podia venir de un error que no tenia nada que ver con un `where`.
    const bool genericConstraintAction = methodBodyAction || action == "--check-project-generic-constraints";
    const bool interfaceAction =
        genericConstraintAction || action == "--check-project-interfaces" || action == "--dump-project-interfaces";
    const bool inheritanceAction =
        interfaceAction || action == "--check-project-inheritance" || action == "--dump-project-inheritance";
    const bool memberBindingAction =
        inheritanceAction || action == "--check-project-members" || action == "--dump-project-members";
    const bool nameBindingAction =
        memberBindingAction || action == "--check-project-names" || action == "--dump-project-names";
    const bool genericAction =
        nameBindingAction || action == "--check-project-generics" || action == "--dump-project-generics";
    const bool success =
        oopMirAction         ? projectCompilation.compileProjectOopMir(std::filesystem::path(path), loadError)
        : oopHirAction       ? projectCompilation.compileProjectOopHir(std::filesystem::path(path), loadError)
        : layoutAction       ? projectCompilation.compileProjectObjectLayouts(std::filesystem::path(path), loadError)
        : constructionAction ? projectCompilation.compileProjectConstructions(std::filesystem::path(path), loadError)
        : destructionAction  ? projectCompilation.compileProjectDestruction(std::filesystem::path(path), loadError)
        : virtualDispatchAction
            ? projectCompilation.compileProjectVirtualDispatch(std::filesystem::path(path), loadError)
        : methodBodyAction ? projectCompilation.compileProjectMethodBodies(std::filesystem::path(path), loadError)
        : genericConstraintAction
            ? projectCompilation.compileProjectGenericConstraints(std::filesystem::path(path), loadError)
        : interfaceAction     ? projectCompilation.compileProjectInterfaces(std::filesystem::path(path), loadError)
        : inheritanceAction   ? projectCompilation.compileProjectInheritance(std::filesystem::path(path), loadError)
        : memberBindingAction ? projectCompilation.compileProjectMembers(std::filesystem::path(path), loadError)
        : nameBindingAction   ? projectCompilation.compileProjectNames(std::filesystem::path(path), loadError)
        : genericAction       ? projectCompilation.compileProjectGenerics(std::filesystem::path(path), loadError)
                              : projectCompilation.compileProjectSyntax(std::filesystem::path(path), loadError);
    if (!loadError.empty())
    {
        error << "nsc: error: " << loadError << '\n';
        return ExitCode::CompilationError;
    }

    if (action == "--dump-project-compilation")
    {
        frontend::ProjectCompilationDumper dumper;
        dumper.dump(projectCompilation, output);
    }

    if (action == "--dump-project-generics" && projectCompilation.genericModel() != nullptr)
    {
        output << frontend::ProjectGenericModelDumper::dump(*projectCompilation.genericModel());
    }

    if (action == "--dump-project-names" && projectCompilation.nameLookup() != nullptr)
    {
        frontend::ProjectNameLookupDumper dumper;
        dumper.dump(*projectCompilation.nameLookup(), output);
    }

    if (action == "--dump-project-members" && projectCompilation.nameLookup() != nullptr &&
        projectCompilation.memberModel() != nullptr)
    {
        frontend::ProjectMemberModelDumper dumper;
        dumper.dump(*projectCompilation.memberModel(), *projectCompilation.nameLookup(), output);
    }

    if (action == "--dump-project-inheritance" && projectCompilation.nameLookup() != nullptr &&
        projectCompilation.inheritanceModel() != nullptr)
    {
        frontend::ProjectInheritanceModelDumper dumper;
        dumper.dump(*projectCompilation.inheritanceModel(), *projectCompilation.nameLookup(), output);
    }

    if (action == "--dump-project-interfaces" && projectCompilation.interfaceModel() != nullptr)
    {
        frontend::ProjectInterfaceModelDumper::dump(*projectCompilation.interfaceModel(), output);
    }

    if (action == "--dump-project-virtual-dispatch" && projectCompilation.nameLookup() != nullptr &&
        projectCompilation.memberModel() != nullptr && projectCompilation.virtualDispatchModel() != nullptr)
    {
        frontend::ProjectVirtualDispatchModelDumper dumper;
        dumper.dump(*projectCompilation.virtualDispatchModel(),
            *projectCompilation.memberModel(),
            *projectCompilation.nameLookup(),
            output);
    }

    if (action == "--dump-project-method-bodies" && projectCompilation.nameLookup() != nullptr &&
        projectCompilation.memberModel() != nullptr && projectCompilation.methodBodyModel() != nullptr)
    {
        frontend::ProjectMethodBodyModelDumper dumper;
        dumper.dump(*projectCompilation.methodBodyModel(),
            *projectCompilation.memberModel(),
            *projectCompilation.nameLookup(),
            output);
    }

    if (action == "--dump-project-destruction" && projectCompilation.nameLookup() != nullptr &&
        projectCompilation.memberModel() != nullptr && projectCompilation.destructionModel() != nullptr)
    {
        frontend::ProjectDestructionModelDumper::dump(*projectCompilation.destructionModel(),
            *projectCompilation.nameLookup(),
            *projectCompilation.memberModel(),
            output);
    }

    if (action == "--dump-project-constructions" && projectCompilation.nameLookup() != nullptr &&
        projectCompilation.memberModel() != nullptr && projectCompilation.constructionModel() != nullptr)
    {
        frontend::ProjectConstructionModelDumper dumper;
        dumper.dump(*projectCompilation.constructionModel(),
            *projectCompilation.memberModel(),
            *projectCompilation.nameLookup(),
            output);
    }

    if (action == "--dump-project-layouts" && projectCompilation.nameLookup() != nullptr &&
        projectCompilation.memberModel() != nullptr && projectCompilation.objectLayoutModel() != nullptr)
    {
        frontend::ProjectObjectLayoutModelDumper dumper;
        dumper.dump(*projectCompilation.objectLayoutModel(),
            *projectCompilation.memberModel(),
            *projectCompilation.nameLookup(),
            output);
    }

    if (action == "--dump-project-oop-hir" && projectCompilation.oopHirModel() != nullptr)
    {
        output << frontend::ProjectOopHirModelDumper::dump(*projectCompilation.oopHirModel());
    }

    if (action == "--dump-project-oop-mir" && projectCompilation.oopMirModel() != nullptr)
    {
        output << frontend::ProjectOopMirModelDumper::dump(*projectCompilation.oopMirModel());
    }

    std::unique_ptr<llvm_backend::LlvmModule> oopLlvmModule;
    if (oopLlvmAction && success && projectCompilation.nameLookup() != nullptr &&
        projectCompilation.memberModel() != nullptr && projectCompilation.objectLayoutModel() != nullptr &&
        projectCompilation.oopMirModel() != nullptr)
    {
        llvm_backend::ProjectOopLlvmLowerer lowerer;
        oopLlvmModule = measureDriverPhase("llvm-lowering", [&] {
            return lowerer.lower(llvm_backend::ProjectOopLlvmLoweringRequest::withoutDebug(
                *projectCompilation.nameLookup(), *projectCompilation.oopMirModel()));
        });
        if (oopLlvmModule == nullptr)
        {
            error << "nsc: error: " << lowerer.error() << '\n';
            return ExitCode::CompilationError;
        }
        if (action == "--dump-project-oop-llvm")
        {
            output << oopLlvmModule->text();
        }
    }

    if (projectCompilation.diagnostics().hasErrors())
    {
        diagnostics::DiagnosticRenderer renderer(projectCompilation.sourceManager());
        renderer.renderAll(projectCompilation.diagnostics(), error);
    }
    if (!success)
    {
        return ExitCode::CompilationError;
    }

    // HIR is the first consumer-facing checkpoint at which every referenced
    // precompiled public contract has been materialized.  Validate the same
    // NSMETA/archive/runtime triplets used by emission here, without linking.
    // With no MIR the helper deliberately stops after artifact validation;
    // reachability-driven runtime packaging remains an emission concern.
    if (oopHirAction)
    {
        std::vector<std::filesystem::path> referencedLibraries;
        std::vector<std::filesystem::path> referencedRuntimes;
        std::string referenceError;
        if (!appendValidatedLibraryReferences(
                projectCompilation, referencedLibraries, referencedRuntimes, referenceError))
        {
            error << "nsc: error: precompiled library contract validation failed: "
                  << referenceError << '\n';
            return ExitCode::CompilationError;
        }
    }

    if (action == "--check-project-syntax")
    {
        output << "project syntax check: ok (" << projectCompilation.units().size() << " syntax unit"
               << (projectCompilation.units().size() == 1 ? "" : "s") << ")\n";
    }
    else if (action == "--check-project-generics" && projectCompilation.genericModel() != nullptr)
    {
        output << "project generic monomorphization check: ok ("
               << projectCompilation.genericModel()->templates().size() << " template"
               << (projectCompilation.genericModel()->templates().size() == 1 ? "" : "s") << ", "
               << projectCompilation.genericModel()->specializations().size() << " specialization"
               << (projectCompilation.genericModel()->specializations().size() == 1 ? "" : "s") << ")\n";
    }
    else if (action == "--check-project-names" && projectCompilation.nameLookup() != nullptr)
    {
        output << "project name lookup check: ok (" << projectCompilation.nameLookup()->namedTypes().size()
               << " named type" << (projectCompilation.nameLookup()->namedTypes().size() == 1 ? "" : "s") << ", "
               << projectCompilation.nameLookup()->bindings().size() << " type binding"
               << (projectCompilation.nameLookup()->bindings().size() == 1 ? "" : "s") << ")\n";
    }
    else if (action == "--check-project-members" && projectCompilation.memberModel() != nullptr)
    {
        output << "project member binding check: ok (" << projectCompilation.memberModel()->members().size()
               << " member" << (projectCompilation.memberModel()->members().size() == 1 ? "" : "s") << ", "
               << projectCompilation.memberModel()->bodyBindings().size() << " body binding"
               << (projectCompilation.memberModel()->bodyBindings().size() == 1 ? "" : "s") << ")\n";
    }
    else if (action == "--check-project-inheritance" && projectCompilation.inheritanceModel() != nullptr)
    {
        std::size_t edgeCount = 0;
        for (const frontend::ClassInheritanceInfo &info : projectCompilation.inheritanceModel()->classes())
        {
            edgeCount += info.baseTypeOrdinal.has_value() ? 1u : 0u;
        }
        output << "project inheritance check: ok (" << projectCompilation.inheritanceModel()->classes().size()
               << " class" << (projectCompilation.inheritanceModel()->classes().size() == 1 ? "" : "es") << ", "
               << edgeCount << " base edge" << (edgeCount == 1 ? "" : "s") << ")\n";
    }
    else if (action == "--check-project-interfaces" && projectCompilation.interfaceModel() != nullptr)
    {
        output << "project interface semantic check: ok (" << projectCompilation.interfaceModel()->interfaces().size()
               << " interface" << (projectCompilation.interfaceModel()->interfaces().size() == 1 ? "" : "s") << ", "
               << projectCompilation.interfaceModel()->tablePlans().size() << " class-interface table"
               << (projectCompilation.interfaceModel()->tablePlans().size() == 1 ? "" : "s") << ")\n";
    }
    else if (action == "--check-project-generic-constraints" && projectCompilation.genericConstraintModel() != nullptr)
    {
        const std::size_t violations = projectCompilation.genericConstraintModel()->violations().size();
        output << "project generic constraint check: ok (" << violations << " violation" << (violations == 1 ? "" : "s")
               << ")\n";
    }
    else if (action == "--check-project-virtual-dispatch" && projectCompilation.virtualDispatchModel() != nullptr)
    {
        output << "project virtual dispatch check: ok ("
               << projectCompilation.virtualDispatchModel()->virtualMethods().size() << " virtual method"
               << (projectCompilation.virtualDispatchModel()->virtualMethods().size() == 1 ? "" : "s") << ", "
               << projectCompilation.virtualDispatchModel()->classTables().size() << " class table"
               << (projectCompilation.virtualDispatchModel()->classTables().size() == 1 ? "" : "s") << ")\n";
    }
    else if (action == "--check-project-method-bodies" && projectCompilation.methodBodyModel() != nullptr)
    {
        output << "project method-body semantic check: ok (" << projectCompilation.methodBodyModel()->scopes().size()
               << " scope" << (projectCompilation.methodBodyModel()->scopes().size() == 1 ? "" : "s") << ", "
               << projectCompilation.methodBodyModel()->locals().size() << " local"
               << (projectCompilation.methodBodyModel()->locals().size() == 1 ? "" : "s") << ", "
               << projectCompilation.methodBodyModel()->nameBindings().size() << " name binding"
               << (projectCompilation.methodBodyModel()->nameBindings().size() == 1 ? "" : "s") << ", "
               << projectCompilation.methodBodyModel()->calls().size() << " call"
               << (projectCompilation.methodBodyModel()->calls().size() == 1 ? "" : "s") << ")\n";
    }
    else if (action == "--check-project-destruction" && projectCompilation.destructionModel() != nullptr)
    {
        std::size_t explicitDestructorCount = 0;
        for (const frontend::ClassDestructionPlan &plan : projectCompilation.destructionModel()->plans())
        {
            explicitDestructorCount += plan.explicitDestructorMemberOrdinal.has_value() ? 1u : 0u;
        }
        output << "project destruction semantic check: ok (" << projectCompilation.destructionModel()->plans().size()
               << " class plan" << (projectCompilation.destructionModel()->plans().size() == 1 ? "" : "s") << ", "
               << explicitDestructorCount << " explicit destructor" << (explicitDestructorCount == 1 ? "" : "s")
               << ")\n";
    }
    else if (action == "--check-project-constructions" && projectCompilation.constructionModel() != nullptr)
    {
        std::size_t explicitCount = 0;
        std::size_t implicitCount = 0;
        for (const frontend::ObjectConstructionBinding &construction :
            projectCompilation.constructionModel()->constructions())
        {
            explicitCount +=
                construction.resolution == frontend::ConstructionResolutionKind::ResolvedExplicit ? 1u : 0u;
            implicitCount +=
                construction.resolution == frontend::ConstructionResolutionKind::ResolvedImplicitDefault ? 1u : 0u;
        }
        output << "project construction semantic check: ok ("
               << projectCompilation.constructionModel()->constructions().size() << " construction"
               << (projectCompilation.constructionModel()->constructions().size() == 1 ? "" : "s") << ", "
               << explicitCount << " explicit, " << implicitCount << " implicit-default)\n";
    }
    else if (action == "--check-project-layouts" && projectCompilation.objectLayoutModel() != nullptr)
    {
        std::size_t fieldCount = 0;
        for (const frontend::ClassObjectLayout &layout : projectCompilation.objectLayoutModel()->classLayouts())
        {
            fieldCount += layout.fields.size();
        }
        output << "project object layout check: ok (" << projectCompilation.objectLayoutModel()->classLayouts().size()
               << " class layout" << (projectCompilation.objectLayoutModel()->classLayouts().size() == 1 ? "" : "s")
               << ", " << fieldCount << " field layout" << (fieldCount == 1 ? "" : "s") << ", "
               << projectCompilation.objectLayoutModel()->allocationPlans().size() << " allocation plan"
               << (projectCompilation.objectLayoutModel()->allocationPlans().size() == 1 ? "" : "s") << ")\n";
    }
    else if (action == "--check-project-oop-hir" && projectCompilation.oopHirModel() != nullptr)
    {
        output << "project OOP HIR check: ok (" << projectCompilation.oopHirModel()->functions().size() << " callable"
               << (projectCompilation.oopHirModel()->functions().size() == 1 ? "" : "s") << ")\n";
    }
    else if (action == "--check-project-oop-mir" && projectCompilation.oopMirModel() != nullptr)
    {
        output << "project OOP MIR check: ok (" << projectCompilation.oopMirModel()->functions().size() << " callable"
               << (projectCompilation.oopMirModel()->functions().size() == 1 ? "" : "s") << ")\n";
    }
    else if (action == "--verify-project-oop-llvm" && oopLlvmModule != nullptr)
    {
        output << "project OOP LLVM verification: ok (" << projectCompilation.oopMirModel()->functions().size()
               << " lowered callable" << (projectCompilation.oopMirModel()->functions().size() == 1 ? "" : "s")
               << ")\n";
    }
    return ExitCode::Success;
}

ExitCode Driver::runProjectOopCodeGeneration(std::string_view inputPath,
    std::string_view outputPath,
    llvm_backend::LlvmOptimizationLevel optimizationLevel,
    llvm_backend::DebugInformationKind debugInformation,
    const std::vector<llvm_backend::DebugPrefixMap> &debugPrefixMaps,
    std::ostream &output,
    std::ostream &error)
{
    frontend::ProjectCompilation compilation;
    configureProjectExecution(compilation);
    std::string loadError;
    if (!measureDriverPhase("precompiled-metadata", [&] {
            return configurePrecompiledModules(
                std::filesystem::path(inputPath), compilation, loadError);
        }))
    {
        error << "nsc: error: " << loadError << '\n';
        return ExitCode::CompilationError;
    }
    if (!compilation.compileProjectOopMir(std::filesystem::path(inputPath), loadError))
    {
        if (!loadError.empty())
        {
            error << "nsc: error: " << loadError << '\n';
        }
        if (compilation.diagnostics().hasErrors())
        {
            diagnostics::DiagnosticRenderer renderer(compilation.sourceManager());
            renderer.renderAll(compilation.diagnostics(), error);
        }
        return ExitCode::CompilationError;
    }
    if (compilation.nameLookup() == nullptr || compilation.memberModel() == nullptr ||
        compilation.objectLayoutModel() == nullptr || compilation.oopMirModel() == nullptr)
    {
        error << "nsc: error: Stage 44 OOP project compilation did not produce "
                 "required lowering models\n";
        return ExitCode::CompilationError;
    }

    llvm_backend::ProjectOopLlvmLowerer lowerer;
    std::unique_ptr<llvm_backend::LlvmModule> module =
        measureDriverPhase("llvm-lowering", [&] {
            return lowerer.lower(llvm_backend::ProjectOopLlvmLoweringRequest::withDebug(
                *compilation.nameLookup(), *compilation.memberModel(),
                *compilation.oopMirModel(), compilation.sourceManager(),
                debugInformation, optimizationLevel, debugPrefixMaps));
        });
    if (module == nullptr)
    {
        error << "nsc: error: " << lowerer.error() << '\n';
        return ExitCode::CompilationError;
    }

    std::string targetError;
    std::unique_ptr<llvm_backend::LlvmTargetMachine> targetMachine;
    const bool targetConfigured = measureDriverPhase("target-configuration", [&] {
        targetMachine = llvm_backend::LlvmTargetMachine::createWindowsX64(
            optimizationLevel, targetError);
        return targetMachine != nullptr &&
            targetMachine->configureModuleForTarget(*module, targetError);
    });
    if (!targetConfigured)
    {
        error << "nsc: error: " << targetError << '\n';
        return ExitCode::CompilationError;
    }

    // NSC-LIMIT-005: observada, como las demas llamadas al optimizador. Ver
    // LlvmOptimizer.h.
    llvm_backend::LlvmOptimizer optimizer;
    llvm_backend::LlvmOptimizationObservation observation;
    if (!measureDriverPhase("llvm-optimization", [&] {
            return optimizer.optimize(
                *module, *targetMachine, optimizationLevel, &observation, targetError);
        }))
    {
        error << "nsc: error: " << targetError << '\n';
        return ExitCode::CompilationError;
    }

    const std::filesystem::path destination = pathFromUtf8(outputPath);
    if (!measureDriverPhase("object-emission", [&] {
            return targetMachine->emit(
                *module, destination, llvm_backend::LlvmCodeGenFileType::Object, targetError);
        }))
    {
        error << "nsc: error: " << targetError << '\n';
        return ExitCode::CompilationError;
    }
    llvm_backend::CoffObjectVerifier verifier;
    if (!measureDriverPhase("coff-verification", [&] {
            return verifier.verify(destination);
        }))
    {
        error << "nsc: error: Stage 44 emitted invalid COFF object: " << verifier.error() << '\n';
        return ExitCode::CompilationError;
    }
    output << "project OOP object emission: ok (machine=0x" << std::hex << verifier.info().machine << std::dec
           << ", sections=" << verifier.info().sectionCount << ")\n"
           << "optimization: " << llvm_backend::llvmOptimizationLevelName(optimizationLevel) << '\n'
           << "debug-information: " << llvm_backend::debugInformationKindName(debugInformation) << '\n';
    return ExitCode::Success;
}

namespace
{
// Interop Foundation: gather the project's `extern C from "<dll>"` declarations
// as (dll -> its symbols), in first-appearance order, for the linker's
// import-library step. The compiler learned no API by name at any point -- this
// is purely the shape the frontend recorded.
std::vector<linker::ExternalImportModule> collectExternalImports(const frontend::ProjectOopMirModel &mirModel)
{
    std::vector<linker::ExternalImportModule> modules;
    bool anyRuntimeBoundImport = false;
    const auto addSymbol = [&modules](const std::string &dllName, const std::string &symbol) {
        linker::ExternalImportModule *found = nullptr;
        for (linker::ExternalImportModule &module : modules)
        {
            if (module.dllName == dllName)
            {
                found = &module;
                break;
            }
        }
        if (found == nullptr)
        {
            modules.push_back(linker::ExternalImportModule{dllName, {}});
            found = &modules.back();
        }
        if (std::find(found->symbols.begin(), found->symbols.end(), symbol) == found->symbols.end())
        {
            found->symbols.push_back(symbol);
        }
    };
    for (const frontend::ProjectOopMirFunction &function : mirModel.functions())
    {
        if (!function.isImportedC || function.importDllName.empty()) continue;
        // C1.2 "import identity": an import whose C symbol name is shared by two
        // libraries has NO import-library entry at all -- there is exactly one slot
        // per symbol name in a PE import table and the two would fight over it. The
        // backend binds those at first call instead.
        if (function.importRequiresRuntimeBinding)
        {
            anyRuntimeBoundImport = true;
            continue;
        }
        // The symbol that goes into the import library is the EXTERNAL one
        // (`cAbiName`), never the internal mangled name -- they coincide for a
        // statically resolved import, and this is the line that keeps them from
        // being confused if they ever stop coinciding.
        addSymbol(function.importDllName, function.cAbiName);
    }
    if (anyRuntimeBoundImport)
    {
        // The two entry points every runtime-bound import needs, requested through
        // the SAME door the project's own imports use, so the runtime's single
        // import table merges and deduplicates them (a project that never needs
        // runtime binding never imports them).
        addSymbol("KERNEL32.dll", "LoadLibraryA");
        addSymbol("KERNEL32.dll", "GetProcAddress");
    }
    return modules;
}
} // namespace

ExitCode Driver::runProjectOopExecutableGeneration(std::string_view inputPath,
    std::string_view outputPath,
    llvm_backend::LlvmOptimizationLevel optimizationLevel,
    llvm_backend::DebugInformationKind debugInformation,
    const std::vector<llvm_backend::DebugPrefixMap> &debugPrefixMaps,
    llvm_backend::LinkTimeOptimizationMode linkTimeOptimization,
    bool incrementalLink,
    linker::WindowsExecutableSubsystem subsystem,
    const std::vector<std::filesystem::path> &extraLinkLibraries,
    std::ostream &output,
    std::ostream &error)
{
    frontend::ProjectCompilation compilation;
    configureProjectExecution(compilation);
    std::string loadError;
    if (!measureDriverPhase("precompiled-metadata", [&] {
            return configurePrecompiledModules(
                std::filesystem::path(inputPath), compilation, loadError);
        }))
    {
        error << "nsc: error: " << loadError << '\n';
        return ExitCode::CompilationError;
    }
    if (!compilation.compileProjectOopMir(std::filesystem::path(inputPath), loadError))
    {
        if (!loadError.empty())
        {
            error << "nsc: error: " << loadError << '\n';
        }
        if (compilation.diagnostics().hasErrors())
        {
            diagnostics::DiagnosticRenderer renderer(compilation.sourceManager());
            renderer.renderAll(compilation.diagnostics(), error);
        }
        return ExitCode::CompilationError;
    }
    if (compilation.nameLookup() == nullptr || compilation.memberModel() == nullptr ||
        compilation.oopMirModel() == nullptr)
    {
        error << "nsc: error: Stage 45.12 OOP executable compilation did not "
                 "produce required lowering models\n";
        return ExitCode::CompilationError;
    }

    const frontend::ProjectOopMirFunction *entryFunction = nullptr;
    const auto members = compilation.memberModel()->members();
    for (const frontend::ProjectOopMirFunction &function : compilation.oopMirModel()->functions())
    {
        if (function.isLambda || function.constructor || function.destructor ||
            function.memberOrdinal >= members.size())
        {
            continue;
        }
        const frontend::ClassMemberSymbol &member = members[function.memberOrdinal];
        if (member.kind != frontend::ClassMemberKind::Method || member.name != "Main" || !member.isStatic ||
            !member.parameters.empty() || member.declaredTypeKey != "builtin:int")
        {
            continue;
        }
        if (entryFunction != nullptr)
        {
            error << "nsc: error: OOP executable has multiple static int Main() "
                     "entry points\n";
            return ExitCode::CompilationError;
        }
        entryFunction = &function;
    }
    if (entryFunction == nullptr)
    {
        error << "nsc: error: OOP executable requires exactly one static int "
                 "Main() method with no parameters\n";
        return ExitCode::CompilationError;
    }

    llvm_backend::ProjectOopObjectGenerator objectGenerator;
    std::string codeGenerationError;
    std::unique_ptr<llvm_backend::ProjectOopObjectBundle> objectBundle =
        measureDriverPhase("llvm-object-generation", [&] {
            return objectGenerator.generate(
                *compilation.nameLookup(), *compilation.memberModel(), *compilation.oopMirModel(),
                compilation.sourceManager(), debugInformation, optimizationLevel,
                effectiveCompilationJobs(), debugPrefixMaps, linkTimeOptimization,
                codeGenerationError);
        });
    if (objectBundle == nullptr)
    {
        error << "nsc: error: " << codeGenerationError << '\n';
        return ExitCode::CompilationError;
    }

    std::vector<std::filesystem::path> linkLibraries = extraLinkLibraries;
    std::vector<std::filesystem::path> runtimeLibraries;
    std::string referenceError;
    if (!appendValidatedLibraryReferences(compilation, linkLibraries, runtimeLibraries, referenceError))
    {
        error << "nsc: error: precompiled library reference validation failed: " << referenceError << '\n';
        return ExitCode::CompilationError;
    }

    linker::WindowsExecutableBuilder builder;
    builder.setRuntimeLibrary(linker::configuredWindowsRuntimeLibrary());
    builder.setSubsystem(subsystem);
    builder.setDebugInformation(debugInformation);
    builder.setIncrementalLink(incrementalLink);
    builder.setExternalImports(collectExternalImports(*compilation.oopMirModel()));
    builder.setExtraLinkLibraries(linkLibraries);
    std::string buildError;
    const std::filesystem::path destination = pathFromUtf8(outputPath);
    if (!measureDriverPhase("executable-build", [&] {
            return builder.buildObjects(
                objectBundle->objectFiles(), objectBundle->combinedLlvmText(), destination,
                optimizationLevel, entryFunction->mangledName, buildError);
        }))
    {
        error << "nsc: error: OOP Windows executable generation failed: " << buildError << '\n';
        return ExitCode::CompilationError;
    }
    if (!verifyLinkedRuntimeLibraries(runtimeLibraries, builder.info(), referenceError))
    {
        std::error_code removeError;
        std::filesystem::remove(destination, removeError);
        error << "nsc: error: precompiled DLL/import-library mismatch: " << referenceError << '\n';
        return ExitCode::CompilationError;
    }
    std::vector<std::filesystem::path> packagedRuntimes;
    if (!packageRuntimeClosure(runtimeLibraries, destination, packagedRuntimes, referenceError))
    {
        removePackagedFiles(packagedRuntimes);
        std::error_code removeError;
        std::filesystem::remove(destination, removeError);
        error << "nsc: error: runtime packaging failed: " << referenceError << '\n';
        return ExitCode::CompilationError;
    }

    output << "project OOP PE executable emission: ok\n"
           << "entry-symbol: " << entryFunction->mangledName << '\n'
           << "machine: AMD64 (0x8664)\n"
           << "format: PE32+\n"
           << "subsystem: " << subsystemDisplayName(subsystem) << '\n'
           << "optimization: " << llvm_backend::llvmOptimizationLevelName(optimizationLevel) << '\n'
           << "debug-information: " << llvm_backend::debugInformationKindName(debugInformation) << '\n'
           << "link-time-optimization: "
           << llvm_backend::linkTimeOptimizationModeName(linkTimeOptimization) << '\n'
           << "code-units: " << objectBundle->codeUnitCount() << '\n'
           << "artifact-cache-hits: " << objectBundle->cacheHitCount() << '\n'
           << "artifact-cache-misses: " << objectBundle->cacheMissCount() << '\n'
           << "thinlto-summaries: " << objectBundle->thinLtoSummaryCount() << '\n'
           << "thinlto-index: "
           << (objectBundle->thinLtoIndexDigest().empty()
                   ? "none"
                   : objectBundle->thinLtoIndexDigest()) << '\n'
           << "incremental-link: "
           << (incrementalLink ? (builder.reusedIncrementalLink() ? "hit" : "miss") : "disabled") << '\n'
           << "linked-libraries: " << linkLibraries.size() << '\n'
           << "packaged-runtimes: " << packagedRuntimes.size() << '\n'
           << "sections: " << builder.info().sectionCount << '\n'
           << "entry-rva: " << builder.info().entryPointRva << '\n'
           << "exception-directory-rva: " << builder.info().exceptionDirectoryRva << '\n'
           << "exception-directory-size: " << builder.info().exceptionDirectorySize << '\n'
           << "imports:";
    for (const std::string &library : builder.info().importedLibraries)
    {
        output << ' ' << library;
    }
    output << '\n';
    return ExitCode::Success;
}

ExitCode Driver::runProjectOopStaticLibraryGeneration(std::string_view inputPath,
    std::string_view outputPath,
    llvm_backend::LlvmOptimizationLevel optimizationLevel,
    llvm_backend::DebugInformationKind debugInformation,
    const std::vector<llvm_backend::DebugPrefixMap> &debugPrefixMaps,
    llvm_backend::LinkTimeOptimizationMode linkTimeOptimization,
    std::ostream &output,
    std::ostream &error)
{
    frontend::ProjectCompilation compilation;
    configureProjectExecution(compilation);
    std::string loadError;
    if (!measureDriverPhase("precompiled-metadata", [&] {
            return configurePrecompiledModules(
                std::filesystem::path(inputPath), compilation, loadError);
        }))
    {
        error << "nsc: error: " << loadError << '\n';
        return ExitCode::CompilationError;
    }
    if (!compilation.compileProjectOopMir(std::filesystem::path(inputPath), loadError))
    {
        if (!loadError.empty())
        {
            error << "nsc: error: " << loadError << '\n';
        }
        if (compilation.diagnostics().hasErrors())
        {
            diagnostics::DiagnosticRenderer renderer(compilation.sourceManager());
            renderer.renderAll(compilation.diagnostics(), error);
        }
        return ExitCode::CompilationError;
    }
    if (compilation.projectGraph() == nullptr || compilation.nameLookup() == nullptr ||
        compilation.memberModel() == nullptr || compilation.oopMirModel() == nullptr)
    {
        error << "nsc: error: Stage 46 static-library compilation did not produce "
                 "required models\n";
        return ExitCode::CompilationError;
    }
    const compilation::Project *rootProject = compilation.projectGraph()->rootProject();
    if (rootProject == nullptr || rootProject->outputKind() != compilation::ProjectOutputKind::Library)
    {
        error << "nsc: error: --emit-project-oop-lib requires root OutputType "
                 "Library or StaticLibrary\n";
        return ExitCode::CompilationError;
    }
    std::vector<std::filesystem::path> referencedLibraries;
    std::vector<std::filesystem::path> runtimeLibraries;
    std::string referenceError;
    if (!appendValidatedLibraryReferences(compilation, referencedLibraries, runtimeLibraries, referenceError))
    {
        error << "nsc: error: precompiled library reference validation failed: " << referenceError << '\n';
        return ExitCode::CompilationError;
    }

    llvm_backend::ProjectOopObjectGenerator objectGenerator;
    std::string codeGenerationError;
    std::unique_ptr<llvm_backend::ProjectOopObjectBundle> objectBundle =
        measureDriverPhase("llvm-object-generation", [&] {
            return objectGenerator.generate(
                *compilation.nameLookup(), *compilation.memberModel(), *compilation.oopMirModel(),
                compilation.sourceManager(), debugInformation, optimizationLevel,
                effectiveCompilationJobs(), debugPrefixMaps, linkTimeOptimization,
                codeGenerationError);
        });
    if (objectBundle == nullptr)
    {
        error << "nsc: error: " << codeGenerationError << '\n';
        return ExitCode::CompilationError;
    }

    linker::WindowsLibraryBuilder builder;
    builder.setRuntimeLibrary(linker::configuredWindowsRuntimeLibrary());
    builder.setExternalImports(collectExternalImports(*compilation.oopMirModel()));
    builder.setExtraLinkLibraries(referencedLibraries);
    std::string buildError;
    const std::filesystem::path destination = pathFromUtf8(outputPath);
    if (!measureDriverPhase("static-library-build", [&] {
            return builder.buildStaticObjects(
                objectBundle->objectFiles(), destination, optimizationLevel, buildError);
        }))
    {
        error << "nsc: error: NativeSharp static-library generation failed: " << buildError << '\n';
        return ExitCode::CompilationError;
    }

    std::filesystem::path headerDestination = destination;
    headerDestination.replace_extension(".h");
    interop::CHeaderGenerator headerGenerator;
    std::string headerError;
    if (!measureDriverPhase("host-header", [&] {
            return headerGenerator.writeHeader(compilation, headerDestination, headerError);
        }))
    {
        std::error_code removeError;
        std::filesystem::remove(destination, removeError);
        error << "nsc: error: Stage 48.1 static-library C header generation failed: " << headerError << '\n';
        return ExitCode::CompilationError;
    }

    std::filesystem::path metadataDestination = destination;
    metadataDestination.replace_extension(".nsmeta");
    std::string metadataError;
    if (!measureDriverPhase("library-metadata", [&] {
            return writeVerifiedLibraryMetadata(compilation, metadataDestination, metadataError);
        }))
    {
        std::error_code removeError;
        std::filesystem::remove(destination, removeError);
        std::filesystem::remove(headerDestination, removeError);
        error << "nsc: error: Stage 48.2 static-library metadata generation failed: " << metadataError << '\n';
        return ExitCode::CompilationError;
    }

    output << "project OOP static library emission: ok\n"
           << "format: COFF archive (.lib)\n"
           << "machine-members: AMD64 COFF\n"
           << "optimization: " << llvm_backend::llvmOptimizationLevelName(optimizationLevel) << '\n'
           << "debug-information: " << llvm_backend::debugInformationKindName(debugInformation) << '\n'
           << "link-time-optimization: "
           << llvm_backend::linkTimeOptimizationModeName(linkTimeOptimization) << '\n'
           << "code-units: " << objectBundle->codeUnitCount() << '\n'
           << "artifact-cache-hits: " << objectBundle->cacheHitCount() << '\n'
           << "artifact-cache-misses: " << objectBundle->cacheMissCount() << '\n'
           << "thinlto-summaries: " << objectBundle->thinLtoSummaryCount() << '\n'
           << "thinlto-index: "
           << (objectBundle->thinLtoIndexDigest().empty()
                   ? "none"
                   : objectBundle->thinLtoIndexDigest()) << '\n'
           << "debug-companion: "
           << (debugInformation == llvm_backend::DebugInformationKind::None
                   ? "none"
                   : "CodeView in COFF archive members")
           << '\n'
           << "library-references: " << referencedLibraries.size() << '\n'
           << "archive-members: " << builder.archiveInfo().memberCount << '\n'
           << "host-header: " << headerDestination.generic_string() << '\n'
           << "library-metadata: " << metadataDestination.generic_string() << '\n';
    return ExitCode::Success;
}

ExitCode Driver::runProjectOopDynamicLibraryGeneration(std::string_view inputPath,
    std::string_view outputPath,
    llvm_backend::LlvmOptimizationLevel optimizationLevel,
    llvm_backend::DebugInformationKind debugInformation,
    const std::vector<llvm_backend::DebugPrefixMap> &debugPrefixMaps,
    llvm_backend::LinkTimeOptimizationMode linkTimeOptimization,
    const std::vector<std::filesystem::path> &extraLinkLibraries,
    std::ostream &output,
    std::ostream &error)
{
    frontend::ProjectCompilation compilation;
    configureProjectExecution(compilation);
    std::string loadError;
    if (!measureDriverPhase("precompiled-metadata", [&] {
            return configurePrecompiledModules(
                std::filesystem::path(inputPath), compilation, loadError);
        }))
    {
        error << "nsc: error: " << loadError << '\n';
        return ExitCode::CompilationError;
    }
    if (!compilation.compileProjectOopMir(std::filesystem::path(inputPath), loadError))
    {
        if (!loadError.empty())
        {
            error << "nsc: error: " << loadError << '\n';
        }
        if (compilation.diagnostics().hasErrors())
        {
            diagnostics::DiagnosticRenderer renderer(compilation.sourceManager());
            renderer.renderAll(compilation.diagnostics(), error);
        }
        return ExitCode::CompilationError;
    }
    if (compilation.projectGraph() == nullptr || compilation.nameLookup() == nullptr ||
        compilation.memberModel() == nullptr || compilation.oopMirModel() == nullptr)
    {
        error << "nsc: error: Stage 47 DLL compilation did not produce required "
                 "models\n";
        return ExitCode::CompilationError;
    }
    const compilation::Project *rootProject = compilation.projectGraph()->rootProject();
    if (rootProject == nullptr || rootProject->outputKind() != compilation::ProjectOutputKind::SharedLibrary)
    {
        error << "nsc: error: --emit-project-oop-dll requires root OutputType "
                 "SharedLibrary\n";
        return ExitCode::CompilationError;
    }

    const auto types = compilation.nameLookup()->namedTypes();
    const auto members = compilation.memberModel()->members();
    const std::size_t rootProjectOrdinal = compilation.projectGraph()->projects().size() - 1;
    std::vector<std::string> exportSymbols;
    for (const frontend::ProjectOopMirFunction &function : compilation.oopMirModel()->functions())
    {
        if (function.isLambda || function.destructor || function.declaringTypeOrdinal >= types.size())
        {
            continue;
        }
        const frontend::NamedTypeDeclaration &type = types[function.declaringTypeOrdinal];
        if (function.isSyntheticImplicitConstructor)
        {
            if (type.projectOrdinal == rootProjectOrdinal &&
                isPubliclyReachableNamedType(types, function.declaringTypeOrdinal))
            {
                exportSymbols.push_back(function.mangledName);
            }
            continue;
        }
        if (function.memberOrdinal >= members.size()) continue;
        const frontend::ClassMemberSymbol &member = members[function.memberOrdinal];
        if (type.projectOrdinal != rootProjectOrdinal ||
            !isPubliclyReachableNamedType(types, function.declaringTypeOrdinal) ||
            !hasConsumerCallableAccessibility(member.accessibility))
        {
            continue;
        }
        if (function.constructor)
        {
            exportSymbols.push_back(function.mangledName);
            continue;
        }
        if (member.kind != frontend::ClassMemberKind::Method ||
            (!member.isStatic && type.kind != frontend::NamedTypeDeclarationKind::Struct &&
                type.kind != frontend::NamedTypeDeclarationKind::Class) ||
            member.isAbstract || member.isCompilerGenerated ||
            (member.isExternC && !member.isExportedC))
        {
            continue;
        }
        exportSymbols.push_back(function.mangledName);
        if (member.isExportedC) exportSymbols.push_back(member.cAbiName);
    }
    for (const frontend::ProjectOopMirDispatchTable &table : compilation.oopMirModel()->dispatchTables())
    {
        if (table.isImported || table.typeOrdinal >= types.size()) continue;
        const frontend::NamedTypeDeclaration &type = types[table.typeOrdinal];
        if (type.projectOrdinal == rootProjectOrdinal && !type.isStatic &&
            isPubliclyReachableNamedType(types, table.typeOrdinal))
        {
            exportSymbols.push_back(table.descriptorAccessorName);
        }
    }
    for (const frontend::ProjectOopMirStaticField &field : compilation.oopMirModel()->staticFields())
    {
        if (field.isImported || field.isConst || field.declaringTypeOrdinal >= types.size() ||
            field.memberOrdinal >= members.size())
        {
            continue;
        }
        const frontend::NamedTypeDeclaration &type = types[field.declaringTypeOrdinal];
        const frontend::ClassMemberSymbol &member = members[field.memberOrdinal];
        if (type.projectOrdinal == rootProjectOrdinal &&
            isPubliclyReachableNamedType(types, field.declaringTypeOrdinal) &&
            hasConsumerCallableAccessibility(member.accessibility))
        {
            exportSymbols.push_back(field.accessorName);
        }
    }
    std::sort(exportSymbols.begin(), exportSymbols.end());
    exportSymbols.erase(std::unique(exportSymbols.begin(), exportSymbols.end()), exportSymbols.end());
    const bool hasPublicMetadataSurface = std::ranges::any_of(types, [&](const frontend::NamedTypeDeclaration &type) {
        return type.projectOrdinal == rootProjectOrdinal &&
            isPubliclyReachableNamedType(types, static_cast<std::size_t>(&type - types.data()));
    }) || (compilation.genericModel() != nullptr &&
           std::ranges::any_of(compilation.genericModel()->templates(), [&](const auto& generic) {
               if (generic.projectOrdinal != rootProjectOrdinal || generic.syntax == nullptr) return false;
               return generic.kind == frontend::GenericTemplateKind::Method
                   ? generic.syntax->hasModifier(lexer::TokenKind::KwPublic)
                   : generic.syntax->tokenKind() == lexer::TokenKind::KwPublic;
           }));
    if (exportSymbols.empty() && !hasPublicMetadataSurface)
    {
        error << "nsc: error: NativeSharp DLL requires at least one publishable "
                 "public type or method in the root project\n";
        return ExitCode::CompilationError;
    }
    llvm_backend::ProjectOopObjectGenerator objectGenerator;
    std::string codeGenerationError;
    std::unique_ptr<llvm_backend::ProjectOopObjectBundle> objectBundle =
        measureDriverPhase("llvm-object-generation", [&] {
            return objectGenerator.generate(
                *compilation.nameLookup(), *compilation.memberModel(), *compilation.oopMirModel(),
                compilation.sourceManager(), debugInformation, optimizationLevel,
                effectiveCompilationJobs(), debugPrefixMaps, linkTimeOptimization,
                codeGenerationError);
        });
    if (objectBundle == nullptr)
    {
        error << "nsc: error: " << codeGenerationError << '\n';
        return ExitCode::CompilationError;
    }

    const std::filesystem::path destination = pathFromUtf8(outputPath);
    std::filesystem::path importLibrary = destination;
    importLibrary.replace_extension(".lib");
    if (importLibrary == destination)
    {
        importLibrary += ".import.lib";
    }
    std::filesystem::path pdbDestination;
    if (debugInformation != llvm_backend::DebugInformationKind::None)
    {
        pdbDestination = destination;
        pdbDestination.replace_extension(".pdb");
    }

    std::vector<std::filesystem::path> linkLibraries = extraLinkLibraries;
    std::vector<std::filesystem::path> runtimeLibraries;
    std::string referenceError;
    if (!appendValidatedLibraryReferences(compilation, linkLibraries, runtimeLibraries, referenceError))
    {
        error << "nsc: error: precompiled library reference validation failed: " << referenceError << '\n';
        return ExitCode::CompilationError;
    }

    linker::WindowsLibraryBuilder builder;
    builder.setRuntimeLibrary(linker::configuredWindowsRuntimeLibrary());
    builder.setExternalImports(collectExternalImports(*compilation.oopMirModel()));
    builder.setExtraLinkLibraries(linkLibraries);
    std::string buildError;
    if (!measureDriverPhase("dynamic-library-build", [&] {
            return builder.buildDynamicObjects(
                objectBundle->objectFiles(), objectBundle->combinedLlvmText(), exportSymbols,
                destination, importLibrary, optimizationLevel, debugInformation, buildError);
        }))
    {
        error << "nsc: error: NativeSharp DLL generation failed: " << buildError << '\n';
        return ExitCode::CompilationError;
    }
    if (!verifyLinkedRuntimeLibraries(runtimeLibraries, builder.dynamicLibraryInfo(), referenceError))
    {
        std::error_code removeError;
        std::filesystem::remove(destination, removeError);
        std::filesystem::remove(importLibrary, removeError);
        if (!pdbDestination.empty()) std::filesystem::remove(pdbDestination, removeError);
        error << "nsc: error: precompiled DLL/import-library mismatch: " << referenceError << '\n';
        return ExitCode::CompilationError;
    }

    std::filesystem::path headerDestination = destination;
    headerDestination.replace_extension(".h");
    interop::CHeaderGenerator headerGenerator;
    std::string headerError;
    if (!measureDriverPhase("host-header", [&] {
            return headerGenerator.writeHeader(compilation, headerDestination, headerError);
        }))
    {
        std::error_code removeError;
        std::filesystem::remove(destination, removeError);
        std::filesystem::remove(importLibrary, removeError);
        if (!pdbDestination.empty()) std::filesystem::remove(pdbDestination, removeError);
        error << "nsc: error: Stage 48.1 DLL C header generation failed: " << headerError << '\n';
        return ExitCode::CompilationError;
    }

    std::filesystem::path metadataDestination = destination;
    metadataDestination.replace_extension(".nsmeta");
    std::string metadataError;
    if (!measureDriverPhase("library-metadata", [&] {
            return writeVerifiedLibraryMetadata(compilation, metadataDestination, metadataError);
        }))
    {
        std::error_code removeError;
        std::filesystem::remove(destination, removeError);
        std::filesystem::remove(importLibrary, removeError);
        std::filesystem::remove(headerDestination, removeError);
        if (!pdbDestination.empty()) std::filesystem::remove(pdbDestination, removeError);
        error << "nsc: error: Stage 48.2 DLL metadata generation failed: " << metadataError << '\n';
        return ExitCode::CompilationError;
    }

    std::vector<std::filesystem::path> packagedRuntimes;
    if (!packageRuntimeClosure(runtimeLibraries, destination, packagedRuntimes, referenceError))
    {
        removePackagedFiles(packagedRuntimes);
        std::error_code removeError;
        std::filesystem::remove(destination, removeError);
        std::filesystem::remove(importLibrary, removeError);
        std::filesystem::remove(headerDestination, removeError);
        std::filesystem::remove(metadataDestination, removeError);
        if (!pdbDestination.empty()) std::filesystem::remove(pdbDestination, removeError);
        error << "nsc: error: runtime packaging failed: " << referenceError << '\n';
        return ExitCode::CompilationError;
    }

    output << "project OOP DLL emission: ok\n"
           << "format: PE32+ DLL\n"
           << "machine: AMD64 (0x8664)\n"
           << "optimization: " << llvm_backend::llvmOptimizationLevelName(optimizationLevel) << '\n'
           << "debug-information: " << llvm_backend::debugInformationKindName(debugInformation) << '\n'
           << "code-units: " << objectBundle->codeUnitCount() << '\n'
           << "artifact-cache-hits: " << objectBundle->cacheHitCount() << '\n'
           << "artifact-cache-misses: " << objectBundle->cacheMissCount() << '\n'
           << "program-database: "
           << (debugInformation == llvm_backend::DebugInformationKind::None
                   ? "none"
                   : pdbDestination.generic_string())
           << '\n'
           << "linked-libraries: " << linkLibraries.size() << '\n'
           << "packaged-runtimes: " << packagedRuntimes.size() << '\n'
           << "import-library: " << importLibrary.generic_string() << '\n'
           << "host-header: " << headerDestination.generic_string() << '\n'
           << "library-metadata: " << metadataDestination.generic_string() << '\n'
           << "exports: " << builder.dynamicLibraryInfo().exportedSymbols.size() << '\n';
    for (const std::string &symbol : builder.dynamicLibraryInfo().exportedSymbols)
    {
        output << "  " << symbol << '\n';
    }
    output << "exception-directory-rva: " << builder.dynamicLibraryInfo().exceptionDirectoryRva << '\n'
           << "exception-directory-size: " << builder.dynamicLibraryInfo().exceptionDirectorySize << '\n'
           << "clr-header-rva: " << builder.dynamicLibraryInfo().clrDirectoryRva << '\n';
    return ExitCode::Success;
}

ExitCode Driver::runProjectHostHeaderGeneration(
    std::string_view inputPath, std::string_view outputPath, std::ostream &output, std::ostream &error)
{
    frontend::ProjectCompilation compilation;
    configureProjectExecution(compilation);
    std::string loadError;
    if (!measureDriverPhase("precompiled-metadata", [&] {
            return configurePrecompiledModules(
                std::filesystem::path(inputPath), compilation, loadError);
        }))
    {
        error << "nsc: error: " << loadError << '\n';
        return ExitCode::CompilationError;
    }
    if (!compilation.compileProjectObjectLayouts(std::filesystem::path(inputPath), loadError))
    {
        if (!loadError.empty()) error << "nsc: error: " << loadError << '\n';
        if (compilation.diagnostics().hasErrors())
        {
            diagnostics::DiagnosticRenderer renderer(compilation.sourceManager());
            renderer.renderAll(compilation.diagnostics(), error);
        }
        return ExitCode::CompilationError;
    }

    if (compilation.projectGraph() == nullptr || compilation.projectGraph()->rootProject() == nullptr)
    {
        error << "nsc: error: Stage 48.1 C header generation requires a valid root "
                 "project\n";
        return ExitCode::CompilationError;
    }
    const compilation::ProjectOutputKind outputKind = compilation.projectGraph()->rootProject()->outputKind();
    if (outputKind != compilation::ProjectOutputKind::Library &&
        outputKind != compilation::ProjectOutputKind::SharedLibrary)
    {
        error << "nsc: error: Stage 48.1 C headers require OutputType Library, "
                 "StaticLibrary, or SharedLibrary\n";
        return ExitCode::CompilationError;
    }

    interop::CHeaderGenerator generator;
    std::string generationError;
    const std::filesystem::path destination = pathFromUtf8(outputPath);
    if (!measureDriverPhase("host-header", [&] {
            return generator.writeHeader(compilation, destination, generationError);
        }))
    {
        error << "nsc: error: Stage 48.1 C header generation failed: " << generationError << '\n';
        return ExitCode::CompilationError;
    }

    output << "C host header emission: ok\n"
           << "output: " << destination.generic_string() << '\n';
    return ExitCode::Success;
}

ExitCode Driver::runProjectLibraryMetadataGeneration(
    std::string_view inputPath, std::string_view outputPath, std::ostream &output, std::ostream &error)
{
    frontend::ProjectCompilation compilation;
    configureProjectExecution(compilation);
    std::string loadError;
    if (!measureDriverPhase("precompiled-metadata", [&] {
            return configurePrecompiledModules(
                std::filesystem::path(inputPath), compilation, loadError);
        }))
    {
        error << "nsc: error: " << loadError << '\n';
        return ExitCode::CompilationError;
    }
    if (!compilation.compileProjectOopMir(std::filesystem::path(inputPath), loadError))
    {
        if (!loadError.empty()) error << "nsc: error: " << loadError << '\n';
        if (compilation.diagnostics().hasErrors())
        {
            diagnostics::DiagnosticRenderer renderer(compilation.sourceManager());
            renderer.renderAll(compilation.diagnostics(), error);
        }
        return ExitCode::CompilationError;
    }
    if (compilation.projectGraph() == nullptr || compilation.projectGraph()->rootProject() == nullptr)
    {
        error << "nsc: error: Stage 48.2 metadata generation requires a valid root "
                 "project\n";
        return ExitCode::CompilationError;
    }
    const compilation::ProjectOutputKind outputKind = compilation.projectGraph()->rootProject()->outputKind();
    if (outputKind != compilation::ProjectOutputKind::Library &&
        outputKind != compilation::ProjectOutputKind::SharedLibrary)
    {
        error << "nsc: error: Stage 48.2 metadata requires OutputType Library, "
                 "StaticLibrary, or SharedLibrary\n";
        return ExitCode::CompilationError;
    }

    interop::LibraryMetadataCodec codec;
    std::string metadataError;
    const std::filesystem::path destination = pathFromUtf8(outputPath);
    if (!measureDriverPhase("library-metadata", [&] {
            return writeVerifiedLibraryMetadata(compilation, destination, metadataError);
        }))
    {
        error << "nsc: error: Stage 48.2 metadata generation failed: " << metadataError << '\n';
        return ExitCode::CompilationError;
    }

    interop::LibraryMetadata metadata;
    if (!codec.read(destination, metadata, metadataError) || !codec.verifyCurrentCompiler(metadata, metadataError))
    {
        std::error_code removeError;
        std::filesystem::remove(destination, removeError);
        error << "nsc: error: Stage 48.2 generated metadata failed "
                 "self-verification: "
              << metadataError << '\n';
        return ExitCode::CompilationError;
    }

    output << "NativeSharp library metadata emission: ok\n"
           << "output: " << destination.generic_string() << '\n'
           << "format: NSMETA " << metadata.formatMajor << '.' << metadata.formatMinor << '\n'
           << "module: " << metadata.moduleName << '\n'
           << "target: " << metadata.targetTriple << '\n'
           << "types: " << metadata.types.size() << '\n'
           << "members: " << metadata.members.size() << '\n'
           << "exports: " << metadata.exports.size() << '\n'
           << "abi-fingerprint: " << interop::LibraryMetadataCodec::fingerprintHex(metadata.abiFingerprint) << '\n';
    return ExitCode::Success;
}

ExitCode Driver::verifyLibraryMetadata(std::string_view inputPath, std::ostream &output, std::ostream &error)
{
    interop::LibraryMetadataCodec codec;
    interop::LibraryMetadata metadata;
    std::string metadataError;
    if (!codec.read(std::filesystem::path(inputPath), metadata, metadataError))
    {
        error << "nsc: error: invalid NativeSharp library metadata: " << metadataError << '\n';
        return ExitCode::CompilationError;
    }
    if (!codec.verifyCurrentCompiler(metadata, metadataError))
    {
        error << "nsc: error: incompatible NativeSharp library metadata: " << metadataError << '\n';
        return ExitCode::CompilationError;
    }
    output << "NativeSharp library metadata: valid\n"
           << "producer: " << metadata.producerVersion << '\n'
           << "format: NSMETA " << metadata.formatMajor << '.' << metadata.formatMinor << '\n'
           << "module: " << metadata.moduleName << '\n'
           << "target: " << metadata.targetTriple << '\n'
           << "types: " << metadata.types.size() << '\n'
           << "members: " << metadata.members.size() << '\n'
           << "generic-templates: " << metadata.genericTemplates.size() << '\n'
           << "generic-specializations: " << metadata.genericSpecializations.size() << '\n'
           << "dispatch-tables: " << metadata.dispatchTables.size() << '\n'
           << "exports: " << metadata.exports.size() << '\n'
           << "abi-fingerprint: " << interop::LibraryMetadataCodec::fingerprintHex(metadata.abiFingerprint) << '\n';
    printMetadataAbiContract(metadata, output);
    return ExitCode::Success;
}

ExitCode Driver::verifyLibraryArtifacts(std::string_view metadataPath,
    std::string_view libraryPath,
    std::string_view runtimePath,
    std::string_view pdbPath,
    std::ostream &output,
    std::ostream &error)
{
    const std::filesystem::path metadataFile(metadataPath);
    const std::filesystem::path libraryFile(libraryPath);
    const std::filesystem::path runtimeFile(runtimePath);
    const std::filesystem::path pdbFile(pdbPath);

    interop::LibraryMetadataCodec codec;
    interop::LibraryMetadata metadata;
    std::string detail;
    if (!codec.read(metadataFile, metadata, detail))
    {
        error << "nsc: error: invalid metadata in library artifact set: " << detail << '\n';
        return ExitCode::CompilationError;
    }
    if (!codec.verifyCurrentCompiler(metadata, detail))
    {
        error << "nsc: error: incompatible metadata in library artifact set: " << detail << '\n';
        return ExitCode::CompilationError;
    }
    linker::CoffArchiveVerifier archiveVerifier;
    if (!archiveVerifier.verify(libraryFile))
    {
        error << "nsc: error: library artifact '" << libraryFile.generic_string()
              << "' is not a valid COFF archive: " << archiveVerifier.error() << '\n';
        return ExitCode::CompilationError;
    }

    const std::vector<std::string> requiredSymbols = metadataRequiredSymbols(metadata);
    const std::vector<std::string> archiveMissing = missingSymbols(requiredSymbols, archiveVerifier.info().symbolNames);
    if (!archiveMissing.empty())
    {
        error << "nsc: error: library artifact '" << libraryFile.generic_string() << "' is missing "
              << archiveMissing.size() << " symbol(s) required by metadata module '" << metadata.moduleName
              << "': " << describeSymbols(archiveMissing) << '\n';
        return ExitCode::CompilationError;
    }

    std::size_t runtimeExportCount = 0;
    linker::PeExecutableInfo runtimeInfo;
    if (!runtimePath.empty())
    {
        linker::PeExecutableVerifier runtimeVerifier;
        if (!runtimeVerifier.verifyDynamicLibrary(runtimeFile))
        {
            error << "nsc: error: runtime artifact '" << runtimeFile.generic_string()
                  << "' is not a valid PE DLL: " << runtimeVerifier.error() << '\n';
            return ExitCode::CompilationError;
        }
        std::vector<std::string> actualExports = runtimeVerifier.info().exportedSymbols;
        std::sort(actualExports.begin(), actualExports.end());
        actualExports.erase(std::unique(actualExports.begin(), actualExports.end()), actualExports.end());
        const std::vector<std::string> runtimeMissing = missingSymbols(requiredSymbols, actualExports);
        const std::vector<std::string> runtimeUnexpected = missingSymbols(actualExports, requiredSymbols);
        if (!runtimeMissing.empty() || !runtimeUnexpected.empty())
        {
            error << "nsc: error: runtime DLL export table does not match metadata "
                     "module '"
                  << metadata.moduleName << "'";
            if (!runtimeMissing.empty())
                error << "; missing " << runtimeMissing.size() << ": " << describeSymbols(runtimeMissing);
            if (!runtimeUnexpected.empty())
                error << "; unexpected " << runtimeUnexpected.size() << ": " << describeSymbols(runtimeUnexpected);
            error << '\n';
            return ExitCode::CompilationError;
        }
        runtimeExportCount = actualExports.size();
        runtimeInfo = runtimeVerifier.info();
    }

    std::optional<linker::PdbInfo> debugInfo;
    if (!pdbPath.empty())
    {
        if (runtimePath.empty())
        {
            error << "nsc: error: a PDB artifact requires its PE DLL artifact\n";
            return ExitCode::CommandLineError;
        }
        if (!runtimeInfo.hasCodeViewRsds)
        {
            error << "nsc: error: runtime DLL has no RSDS CodeView record for PDB verification\n";
            return ExitCode::CompilationError;
        }
        linker::PdbVerifier pdbVerifier;
        if (!pdbVerifier.verify(pdbFile))
        {
            error << "nsc: error: PDB artifact '" << pdbFile.generic_string()
                  << "' is not a valid Microsoft PDB: " << pdbVerifier.error() << '\n';
            return ExitCode::CompilationError;
        }
        const linker::PdbInfo &pdb = pdbVerifier.info();
        if (pdb.guid != runtimeInfo.codeViewGuid || pdb.age != runtimeInfo.codeViewAge)
        {
            error << "nsc: error: PDB identity does not match the runtime DLL RSDS record\n";
            return ExitCode::CompilationError;
        }
        if (std::filesystem::path(runtimeInfo.codeViewPdbFileName).filename() != pdbFile.filename() ||
            std::filesystem::path(runtimeInfo.codeViewPdbFileName).has_parent_path())
        {
            error << "nsc: error: runtime DLL RSDS PDB name is not the portable matching basename: '"
                  << runtimeInfo.codeViewPdbFileName << "'\n";
            return ExitCode::CompilationError;
        }
        if (!pdb.hasModuleRecords || !pdb.hasPublicSymbols || !pdb.hasSourceFiles || !pdb.hasSourceChecksums)
        {
            error << "nsc: error: PDB debug surface is incomplete; modules=" << pdb.hasModuleRecords
                  << ", publics=" << pdb.hasPublicSymbols << ", sources=" << pdb.hasSourceFiles
                  << ", checksums=" << pdb.hasSourceChecksums << '\n';
            return ExitCode::CompilationError;
        }
        debugInfo = pdb;
    }

    // Artifact integrity is independent from source reconstruction. Validate COFF and the
    // optional runtime first so a syntax failure in generated public API does not hide a broken
    // archive/export set (or falsely suggest that those layers were never examined).
    if (!verifyPrecompiledPublicSource(metadata, detail))
    {
        error << "nsc: error: metadata public API cannot be materialized for a consumer: "
              << detail << "\nphysical archive"
              << (runtimePath.empty() ? "" : " and runtime")
              << " validated before source materialization\n";
        return ExitCode::CompilationError;
    }

    output << "NativeSharp library artifacts: valid\n"
           << "module: " << metadata.moduleName << '\n'
           << "metadata: " << metadataFile.generic_string() << '\n'
           << "library: " << libraryFile.generic_string() << '\n'
           << "archive-members: " << archiveVerifier.info().memberCount << '\n'
           << "archive-symbols: " << archiveVerifier.info().symbolNames.size() << '\n'
           << "required-symbols: " << requiredSymbols.size() << '\n';
    if (runtimePath.empty())
    {
        output << "product-kind: static archive\n";
    }
    else
    {
        output << "product-kind: DLL plus import archive\n"
               << "runtime: " << runtimeFile.generic_string() << '\n'
               << "runtime-exports: " << runtimeExportCount << '\n';
    }
    if (debugInfo.has_value())
    {
        output << "pdb: " << pdbFile.generic_string() << '\n'
               << "pdb-age: " << debugInfo->age << '\n'
               << "pdb-streams: " << debugInfo->streamCount << '\n'
               << "pdb-modules: present\n"
               << "pdb-public-symbols: present\n"
               << "pdb-sources: present\n"
               << "pdb-source-checksums: present\n"
               << "rsds-pdb-name: " << runtimeInfo.codeViewPdbFileName << '\n';
    }
    output << "abi-fingerprint: " << interop::LibraryMetadataCodec::fingerprintHex(metadata.abiFingerprint) << '\n';
    printMetadataAbiContract(metadata, output);
    return ExitCode::Success;
}

ExitCode Driver::runFrontend(std::string_view action,
    std::string_view path,
    llvm_backend::LlvmOptimizationLevel optimizationLevel,
    std::ostream &output,
    std::ostream &error)
{
    frontend::FrontendCompilation compilation;
    std::string loadError;
    const bool requireMain = action == "--program";
    const bool success = compilation.compileFile(std::filesystem::path(path), loadError, requireMain);

    if (!loadError.empty())
    {
        error << "nsc: error: " << loadError << '\n';
        return ExitCode::CompilationError;
    }

    const source::SourceFile *sourceFile = compilation.sourceManager().getFile(compilation.primaryFileId());
    if (action == "--dump-source" && sourceFile != nullptr)
    {
        output << "file: " << sourceFile->displayPath() << '\n'
               << "bytes: " << sourceFile->size() << '\n'
               << "lines: " << sourceFile->lineCount() << '\n';
        for (std::uint32_t line = 1; line <= sourceFile->lineCount(); ++line)
        {
            output << line << ": " << sourceFile->lineText(line) << '\n';
        }
    }
    else if (action == "--dump-tokens")
    {
        lexer::TokenDumper dumper;
        dumper.dump(compilation.tokens(), output);
    }
    else if (action == "--dump-ast" && compilation.syntaxTree() != nullptr)
    {
        ast::AstDumper dumper;
        dumper.dump(compilation.syntaxTree(), output);
    }
    else if (action == "--dump-symbols" && compilation.semanticAnalyzer() != nullptr)
    {
        sema::SymbolDumper dumper;
        dumper.dump(compilation.semanticAnalyzer()->globalSymbols(), output);
    }
    else if (action == "--dump-sema" && compilation.semanticAnalyzer() != nullptr &&
             compilation.syntaxTree() != nullptr)
    {
        sema::SemanticDumper dumper;
        dumper.dump(compilation.syntaxTree(), compilation.semanticAnalyzer()->model(), output);
    }
    else if (action == "--dump-hir" && compilation.hirRoot() != nullptr)
    {
        hir::HirDumper dumper;
        dumper.dump(compilation.hirRoot(), output);
    }
    else if (action == "--verify-hir" && compilation.hirRoot() != nullptr)
    {
        hir::HirVerifier verifier;
        if (!verifier.verify(compilation.hirRoot()))
        {
            error << "nsc: internal error: " << verifier.error() << '\n';
            return ExitCode::CompilationError;
        }
        output << "HIR verification: ok\n";
    }
    else if (action == "--dump-mir" && compilation.mirModule() != nullptr)
    {
        mir::MirDumper dumper;
        dumper.dump(compilation.mirModule(), output);
    }
    else if (action == "--verify-mir" && compilation.mirModule() != nullptr)
    {
        mir::MirVerifier verifier;
        if (!verifier.verify(compilation.mirModule()))
        {
            error << "nsc: internal error: " << verifier.error() << '\n';
            return ExitCode::CompilationError;
        }
        output << "MIR verification: ok\n";
    }
    else if (action == "--dump-cfg" && compilation.cfgModule() != nullptr)
    {
        cfg::CfgDumper dumper;
        dumper.dump(compilation.cfgModule(), output);
    }
    else if (action == "--verify-cfg" && compilation.cfgModule() != nullptr)
    {
        cfg::CfgVerifier verifier;
        if (!verifier.verify(compilation.cfgModule()))
        {
            error << "nsc: internal error: " << verifier.error() << '\n';
            return ExitCode::CompilationError;
        }
        output << "CFG verification: ok\n";
    }
    else if (action == "--dump-dominance" && compilation.dominanceModule() != nullptr)
    {
        dominance::DominanceDumper dumper;
        dumper.dump(compilation.dominanceModule(), output);
    }
    else if (action == "--verify-dominance" && compilation.dominanceModule() != nullptr)
    {
        dominance::DominanceVerifier verifier;
        if (!verifier.verify(compilation.dominanceModule()))
        {
            error << "nsc: internal error: " << verifier.error() << '\n';
            return ExitCode::CompilationError;
        }
        output << "Dominance verification: ok\n";
    }
    else if (action == "--dump-ssa" && compilation.ssaModule() != nullptr)
    {
        ssa::SsaDumper dumper;
        dumper.dump(compilation.ssaModule(), output);
    }
    else if (action == "--verify-ssa" && compilation.ssaModule() != nullptr)
    {
        ssa::SsaVerifier verifier;
        if (!verifier.verify(compilation.ssaModule()))
        {
            error << "nsc: internal error: " << verifier.error() << '\n';
            return ExitCode::CompilationError;
        }
        output << "SSA verification: ok\n";
    }
    else if (action == "--dump-opt" && compilation.optimizationModule() != nullptr)
    {
        passes::OptimizationDumper dumper;
        dumper.dump(compilation.optimizationModule(), output);
    }
    else if (action == "--verify-opt" && compilation.optimizationModule() != nullptr)
    {
        passes::OptimizationVerifier verifier;
        if (!verifier.verify(compilation.optimizationModule()))
        {
            error << "nsc: internal error: " << verifier.error() << '\n';
            return ExitCode::CompilationError;
        }
        output << "Optimization verification: ok\n";
    }
    else if (action == "--dump-pass-pipeline" && compilation.optimizationModule() != nullptr)
    {
        passes::PassPipelineDumper dumper;
        dumper.dump(compilation.optimizationModule(), output);
    }
    else if ((action == "--dump-llvm" || action == "--verify-llvm" || action == "--dump-llvm-opt" ||
                 action == "--verify-llvm-opt" || action == "--program" || action == "--check") &&
             success && compilation.optimizationModule() != nullptr && compilation.semanticAnalyzer() != nullptr)
    {
        llvm_backend::LlvmLowerer lowerer(compilation.semanticAnalyzer()->globalSymbols());
        std::unique_ptr<llvm_backend::LlvmModule> llvmModule = lowerer.lower(compilation.optimizationModule());
        if (llvmModule == nullptr)
        {
            error << "nsc: internal error: LLVM lowering failed: " << lowerer.error() << '\n';
            return ExitCode::CompilationError;
        }

        if (action == "--dump-llvm")
        {
            output << llvmModule->text();
        }
        else
        {
            std::string verificationError;
            if (!llvmModule->verify(verificationError))
            {
                error << "nsc: internal error: LLVM verification failed: " << verificationError << '\n';
                return ExitCode::CompilationError;
            }

            if (action == "--verify-llvm")
            {
                output << "LLVM verification: ok\n";
            }
            else
            {
                std::string targetError;
                std::unique_ptr<llvm_backend::LlvmTargetMachine> targetMachine =
                    llvm_backend::LlvmTargetMachine::createWindowsX64(optimizationLevel, targetError);
                if (targetMachine == nullptr)
                {
                    error << "nsc: internal error: target machine creation failed: " << targetError << '\n';
                    return ExitCode::CompilationError;
                }

                // NSC-LIMIT-005: observada. Ver LlvmOptimizer.h.
                llvm_backend::LlvmOptimizer optimizer;
                llvm_backend::LlvmOptimizationObservation observation;
                std::string optimizationError;
                if (!optimizer.optimize(
                        *llvmModule, *targetMachine, optimizationLevel, &observation, optimizationError))
                {
                    error << "nsc: internal error: LLVM optimization failed: " << optimizationError << '\n';
                    return ExitCode::CompilationError;
                }

                if (action == "--dump-llvm-opt")
                {
                    output << llvmModule->text();
                }
                else if (action == "--verify-llvm-opt")
                {
                    output << "LLVM optimization verification: ok\n"
                           << "optimization: " << llvm_backend::llvmOptimizationLevelName(optimizationLevel) << '\n';
                }
            }
        }
    }

    if (compilation.diagnostics().hasErrors())
    {
        diagnostics::DiagnosticRenderer renderer(compilation.sourceManager());
        renderer.renderAll(compilation.diagnostics(), error);
    }
    if (!compilation.hirError().empty())
    {
        error << "nsc: internal error: " << compilation.hirError() << '\n';
    }
    if (!compilation.mirError().empty())
    {
        error << "nsc: internal error: " << compilation.mirError() << '\n';
    }
    if (!compilation.cfgError().empty())
    {
        error << "nsc: internal error: " << compilation.cfgError() << '\n';
    }
    if (!compilation.dominanceError().empty())
    {
        error << "nsc: internal error: " << compilation.dominanceError() << '\n';
    }
    if (!compilation.ssaError().empty())
    {
        error << "nsc: internal error: " << compilation.ssaError() << '\n';
    }
    if (!compilation.optimizationError().empty())
    {
        error << "nsc: internal error: " << compilation.optimizationError() << '\n';
    }

    return success ? ExitCode::Success : ExitCode::CompilationError;
}
} // namespace nativesharp::driver
