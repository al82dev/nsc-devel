#include "nativesharp/LanguageService/LanguageService.h"

#include "nativesharp/Basic/Version.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/Frontend/ProjectNamespaceFunctionModel.h"
#include "nativesharp/Target/TargetInfo.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

namespace nativesharp::language_service
{
namespace
{
struct IndexedReference final
{
    LanguageLocation location;
    std::vector<std::size_t> targetSymbolIndices;
};

struct SemanticIndex final
{
    std::shared_ptr<frontend::ProjectCompilation> compilation;
    std::vector<LanguageSymbol> symbols;
    std::vector<IndexedReference> references;
};

std::string pathKey(const std::filesystem::path& path)
{
    std::string key = path.lexically_normal().generic_string();
    std::ranges::transform(key, key.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return key;
}

bool positionLess(TextPosition left, TextPosition right) noexcept
{
    return left.line < right.line || (left.line == right.line && left.character < right.character);
}

bool contains(TextRange range, TextPosition position) noexcept
{
    return !positionLess(position, range.start) && positionLess(position, range.end);
}

std::uint64_t rangeSize(TextRange range) noexcept
{
    return (static_cast<std::uint64_t>(range.end.line - range.start.line) << 32U) +
        range.end.character - (range.start.line == range.end.line ? range.start.character : 0U);
}

std::optional<LanguageLocation> languageLocation(
    const source::SourceManager& sources,
    FileId fileId,
    SourceRange range)
{
    const source::SourceFile* file = sources.getFile(fileId);
    if (file == nullptr || !range.isValid()) return std::nullopt;
    const auto start = utf16PositionForOffset(file->buffer(), range.begin().offset());
    const auto end = utf16PositionForOffset(file->buffer(), range.end().offset());
    if (!start.has_value() || !end.has_value()) return std::nullopt;
    return LanguageLocation{file->path(), TextRange{*start, *end}};
}

LanguageSymbolKind typeSymbolKind(frontend::NamedTypeDeclarationKind kind) noexcept
{
    switch (kind)
    {
        case frontend::NamedTypeDeclarationKind::Class: return LanguageSymbolKind::Class;
        case frontend::NamedTypeDeclarationKind::Interface: return LanguageSymbolKind::Interface;
        case frontend::NamedTypeDeclarationKind::Struct: return LanguageSymbolKind::Struct;
        case frontend::NamedTypeDeclarationKind::Enum: return LanguageSymbolKind::Enum;
        case frontend::NamedTypeDeclarationKind::Delegate: return LanguageSymbolKind::Delegate;
    }
    return LanguageSymbolKind::Struct;
}

LanguageSymbolKind memberSymbolKind(frontend::ClassMemberKind kind) noexcept
{
    switch (kind)
    {
        case frontend::ClassMemberKind::Field: return LanguageSymbolKind::Field;
        case frontend::ClassMemberKind::Event: return LanguageSymbolKind::Event;
        case frontend::ClassMemberKind::Method: return LanguageSymbolKind::Method;
        case frontend::ClassMemberKind::Constructor: return LanguageSymbolKind::Constructor;
        case frontend::ClassMemberKind::Destructor: return LanguageSymbolKind::Destructor;
    }
    return LanguageSymbolKind::Field;
}

std::string callableDetail(
    std::string_view resultType,
    std::string_view qualifiedName,
    std::span<const frontend::MethodParameterSymbol> parameters)
{
    std::string detail;
    if (!resultType.empty())
    {
        detail.append(resultType);
        detail.push_back(' ');
    }
    detail.append(qualifiedName);
    detail.push_back('(');
    for (std::size_t index = 0; index < parameters.size(); ++index)
    {
        if (index != 0) detail.append(", ");
        detail.append(parameters[index].typeKey);
        detail.push_back(' ');
        detail.append(parameters[index].name);
    }
    detail.push_back(')');
    return detail;
}

std::shared_ptr<SemanticIndex> buildSemanticIndex(
    std::shared_ptr<frontend::ProjectCompilation> compilation)
{
    auto index = std::make_shared<SemanticIndex>();
    index->compilation = std::move(compilation);
    const auto* names = index->compilation->nameLookup();
    const auto* members = index->compilation->memberModel();
    const auto* functions = index->compilation->namespaceFunctionModel();
    const auto* bodies = index->compilation->methodBodyModel();
    if (names == nullptr || members == nullptr || functions == nullptr || bodies == nullptr) return index;
    const source::SourceManager& sources = index->compilation->sourceManager();

    const auto addSymbol = [&](LanguageSymbol symbol) {
        index->symbols.push_back(std::move(symbol));
        return index->symbols.size() - 1;
    };
    const auto makeSymbol = [&](std::string name, std::string qualifiedName, std::string detail,
                                LanguageSymbolKind kind, FileId fileId, SourceRange range)
        -> std::optional<std::size_t> {
        const auto location = languageLocation(sources, fileId, range);
        if (!location.has_value()) return std::nullopt;
        return addSymbol(LanguageSymbol{
            std::move(name), std::move(qualifiedName), std::move(detail), kind,
            *location, location->range});
    };

    std::vector<std::optional<std::size_t>> typeSymbols(names->namedTypes().size());
    for (std::size_t ordinal = 0; ordinal < names->namedTypes().size(); ++ordinal)
    {
        const auto& type = names->namedTypes()[ordinal];
        if (type.isPrecompiled) continue;
        const std::string qualified = type.identity.fullyQualifiedName();
        typeSymbols[ordinal] = makeSymbol(
            type.identity.name(), qualified,
            std::string(frontend::namedTypeDeclarationKindName(type.kind)) + " " + qualified,
            typeSymbolKind(type.kind), type.fileId, type.range);
    }

    std::vector<std::optional<std::size_t>> memberSymbols(members->members().size());
    std::unordered_map<std::uint64_t, std::size_t> parameterSymbols;
    for (std::size_t ordinal = 0; ordinal < members->members().size(); ++ordinal)
    {
        const auto& member = members->members()[ordinal];
        if (member.isPrecompiled || member.isCompilerGenerated ||
            member.declaringTypeOrdinal >= names->namedTypes().size()) continue;
        const std::string qualified =
            names->namedTypes()[member.declaringTypeOrdinal].identity.fullyQualifiedName() + "." + member.name;
        std::string detail = member.kind == frontend::ClassMemberKind::Method ||
                member.kind == frontend::ClassMemberKind::Constructor ||
                member.kind == frontend::ClassMemberKind::Destructor
            ? callableDetail(member.declaredTypeKey, qualified, member.parameters)
            : member.declaredTypeKey + " " + qualified;
        memberSymbols[ordinal] = makeSymbol(
            member.name, qualified, std::move(detail), memberSymbolKind(member.kind),
            member.fileId, member.range);
        for (std::size_t parameterOrdinal = 0; parameterOrdinal < member.parameters.size(); ++parameterOrdinal)
        {
            const auto& parameter = member.parameters[parameterOrdinal];
            const auto added = makeSymbol(
                parameter.name, qualified + "." + parameter.name,
                parameter.typeKey + " " + parameter.name,
                LanguageSymbolKind::Parameter, parameter.fileId, parameter.range);
            if (added.has_value())
            {
                parameterSymbols.emplace((static_cast<std::uint64_t>(ordinal) << 32U) | parameterOrdinal, *added);
            }
        }
    }

    std::vector<std::optional<std::size_t>> functionSymbols(functions->functions().size());
    for (std::size_t ordinal = 0; ordinal < functions->functions().size(); ++ordinal)
    {
        const auto& function = functions->functions()[ordinal];
        if (function.isPrecompiled) continue;
        const std::string qualified = function.namespaceName.empty()
            ? function.name : function.namespaceName + "." + function.name;
        functionSymbols[ordinal] = makeSymbol(
            function.name, qualified,
            callableDetail(function.declaredTypeKey, qualified, function.parameters),
            LanguageSymbolKind::Function, function.fileId, function.range);
    }

    std::vector<std::optional<std::size_t>> stateSymbols(functions->states().size());
    for (std::size_t ordinal = 0; ordinal < functions->states().size(); ++ordinal)
    {
        const auto& state = functions->states()[ordinal];
        if (state.isPrecompiled) continue;
        const std::string qualified = state.namespaceName.empty()
            ? state.name : state.namespaceName + "." + state.name;
        stateSymbols[ordinal] = makeSymbol(
            state.name, qualified, state.declaredTypeKey + " " + qualified,
            LanguageSymbolKind::Variable, state.fileId, state.range);
    }

    std::vector<std::optional<std::size_t>> localSymbols(bodies->locals().size());
    for (std::size_t ordinal = 0; ordinal < bodies->locals().size(); ++ordinal)
    {
        const auto& local = bodies->locals()[ordinal];
        const std::string qualified = "local::" + local.name;
        localSymbols[ordinal] = makeSymbol(
            local.name, qualified, local.typeKey + " " + local.name,
            LanguageSymbolKind::Variable, local.fileId, local.range);
    }

    const auto addReference = [&](FileId fileId, SourceRange range, std::vector<std::size_t> targets) {
        if (targets.empty()) return;
        const auto location = languageLocation(sources, fileId, range);
        if (location.has_value()) index->references.push_back({*location, std::move(targets)});
    };
    for (const auto& binding : names->bindings())
    {
        if (binding.syntax == nullptr || !binding.targetTypeOrdinal.has_value()) continue;
        const std::size_t target = *binding.targetTypeOrdinal;
        if (target < typeSymbols.size() && typeSymbols[target].has_value())
            addReference(binding.fileId, binding.syntax->range(), {*typeSymbols[target]});
    }
    for (const auto& binding : bodies->nameBindings())
    {
        if (binding.syntax == nullptr) continue;
        std::vector<std::size_t> targets;
        if (binding.localOrdinal.has_value() && *binding.localOrdinal < localSymbols.size() &&
            localSymbols[*binding.localOrdinal].has_value())
            targets.push_back(*localSymbols[*binding.localOrdinal]);
        if (binding.parameterOrdinal.has_value())
        {
            const std::uint64_t key = (static_cast<std::uint64_t>(binding.methodMemberOrdinal) << 32U) |
                *binding.parameterOrdinal;
            if (const auto found = parameterSymbols.find(key); found != parameterSymbols.end())
                targets.push_back(found->second);
        }
        for (const std::size_t member : binding.candidateMemberOrdinals)
            if (member < memberSymbols.size() && memberSymbols[member].has_value())
                targets.push_back(*memberSymbols[member]);
        addReference(binding.syntax->range().begin().fileId(), binding.syntax->range(), std::move(targets));
    }
    for (const auto& call : bodies->calls())
    {
        if (call.syntax == nullptr || !call.selectedMemberOrdinal.has_value()) continue;
        const std::size_t member = *call.selectedMemberOrdinal;
        if (member < memberSymbols.size() && memberSymbols[member].has_value())
        {
            const ast::AstNode* callee = call.syntax->first() != nullptr ? call.syntax->first() : call.syntax;
            addReference(callee->range().begin().fileId(), callee->range(), {*memberSymbols[member]});
        }
    }
    for (const auto& reference : functions->referenceBindings())
    {
        if (reference.calleeSyntax == nullptr) continue;
        std::vector<std::size_t> targets;
        for (const std::size_t function : reference.candidateFunctionOrdinals)
            if (function < functionSymbols.size() && functionSymbols[function].has_value())
                targets.push_back(*functionSymbols[function]);
        addReference(reference.fileId, reference.calleeSyntax->range(), std::move(targets));
    }
    for (const auto& reference : functions->stateReferenceBindings())
    {
        if (reference.expressionSyntax == nullptr) continue;
        std::vector<std::size_t> targets;
        for (const std::size_t state : reference.candidateStateOrdinals)
            if (state < stateSymbols.size() && stateSymbols[state].has_value())
                targets.push_back(*stateSymbols[state]);
        addReference(reference.fileId, reference.expressionSyntax->range(), std::move(targets));
    }
    return index;
}
}

class AnalysisSnapshot::Implementation final
{
public:
    std::uint64_t generation = 0;
    std::filesystem::path projectPath;
    bool currentAnalysisSucceeded = false;
    bool usedPreviousSemanticModel = false;
    bool cancelled = false;
    std::string error;
    std::vector<LanguageDiagnostic> diagnostics;
    std::shared_ptr<frontend::ProjectCompilation> currentCompilation;
    std::shared_ptr<SemanticIndex> semanticIndex;
};

class LanguageService::Implementation final
{
public:
    struct StoredDocument final
    {
        std::filesystem::path path;
        std::string text;
        std::int64_t version = 0;
    };

    static std::optional<std::pair<std::string, std::filesystem::path>> documentKey(
        const std::filesystem::path& path)
    {
        if (path.empty()) return std::nullopt;
        std::error_code error;
        std::filesystem::path absolute = std::filesystem::absolute(path, error);
        if (error) return std::nullopt;
        absolute = absolute.lexically_normal();
        std::string key = absolute.generic_string();
        std::ranges::transform(key, key.begin(), [](unsigned char value) {
            return static_cast<char>(std::tolower(value));
        });
        return std::pair{std::move(key), std::move(absolute)};
    }

    mutable std::mutex mutex;
    std::unordered_map<std::string, StoredDocument> documents;
    std::unordered_map<std::string, std::shared_ptr<SemanticIndex>> lastGoodSemantic;
    std::atomic_uint64_t nextGeneration = 1;
};

AnalysisSnapshot::AnalysisSnapshot(std::shared_ptr<Implementation> implementation) noexcept
    : implementation_(std::move(implementation))
{
}

bool AnalysisSnapshot::available() const noexcept
{
    return implementation_ != nullptr;
}

std::uint64_t AnalysisSnapshot::generation() const noexcept
{
    return implementation_ != nullptr ? implementation_->generation : 0;
}

const std::filesystem::path& AnalysisSnapshot::projectPath() const noexcept
{
    static const std::filesystem::path empty;
    return implementation_ != nullptr ? implementation_->projectPath : empty;
}

bool AnalysisSnapshot::currentAnalysisSucceeded() const noexcept
{
    return implementation_ != nullptr && implementation_->currentAnalysisSucceeded;
}

bool AnalysisSnapshot::semanticModelAvailable() const noexcept
{
    return implementation_ != nullptr && implementation_->semanticIndex != nullptr;
}

bool AnalysisSnapshot::usedPreviousSemanticModel() const noexcept
{
    return implementation_ != nullptr && implementation_->usedPreviousSemanticModel;
}

bool AnalysisSnapshot::cancelled() const noexcept
{
    return implementation_ != nullptr && implementation_->cancelled;
}

std::string_view AnalysisSnapshot::error() const noexcept
{
    return implementation_ != nullptr ? std::string_view(implementation_->error) : std::string_view{};
}

std::span<const LanguageDiagnostic> AnalysisSnapshot::diagnostics() const noexcept
{
    return implementation_ != nullptr
        ? std::span<const LanguageDiagnostic>(implementation_->diagnostics)
        : std::span<const LanguageDiagnostic>{};
}

std::vector<LanguageSymbol> AnalysisSnapshot::documentSymbols(
    const std::filesystem::path& path) const
{
    std::vector<LanguageSymbol> result;
    if (implementation_ == nullptr || implementation_->semanticIndex == nullptr) return result;
    const std::string wanted = pathKey(path);
    for (const LanguageSymbol& symbol : implementation_->semanticIndex->symbols)
        if (pathKey(symbol.location.path) == wanted) result.push_back(symbol);
    return result;
}

std::vector<LanguageSymbol> AnalysisSnapshot::workspaceSymbols(
    std::string_view query,
    std::size_t limit) const
{
    std::vector<LanguageSymbol> result;
    if (implementation_ == nullptr || implementation_->semanticIndex == nullptr || limit == 0) return result;
    std::string needle(query);
    std::ranges::transform(needle, needle.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    for (const LanguageSymbol& symbol : implementation_->semanticIndex->symbols)
    {
        std::string candidate = symbol.qualifiedName;
        std::ranges::transform(candidate, candidate.begin(), [](unsigned char value) {
            return static_cast<char>(std::tolower(value));
        });
        if (needle.empty() || candidate.find(needle) != std::string::npos)
        {
            result.push_back(symbol);
            if (result.size() == limit) break;
        }
    }
    return result;
}

std::optional<LanguageSymbol> AnalysisSnapshot::symbolAt(
    const std::filesystem::path& path,
    TextPosition position) const
{
    if (implementation_ == nullptr || implementation_->semanticIndex == nullptr) return std::nullopt;
    const std::string wanted = pathKey(path);
    const IndexedReference* bestReference = nullptr;
    for (const IndexedReference& reference : implementation_->semanticIndex->references)
    {
        if (pathKey(reference.location.path) != wanted || !contains(reference.location.range, position)) continue;
        if (bestReference == nullptr || rangeSize(reference.location.range) < rangeSize(bestReference->location.range) ||
            (rangeSize(reference.location.range) == rangeSize(bestReference->location.range) &&
             reference.targetSymbolIndices.size() < bestReference->targetSymbolIndices.size()))
            bestReference = &reference;
    }
    if (bestReference != nullptr && !bestReference->targetSymbolIndices.empty())
        return implementation_->semanticIndex->symbols[bestReference->targetSymbolIndices.front()];

    const LanguageSymbol* bestSymbol = nullptr;
    for (const LanguageSymbol& symbol : implementation_->semanticIndex->symbols)
    {
        if (pathKey(symbol.location.path) != wanted || !contains(symbol.location.range, position)) continue;
        if (bestSymbol == nullptr || rangeSize(symbol.location.range) < rangeSize(bestSymbol->location.range))
            bestSymbol = &symbol;
    }
    return bestSymbol != nullptr ? std::optional<LanguageSymbol>(*bestSymbol) : std::nullopt;
}

std::vector<LanguageLocation> AnalysisSnapshot::definitions(
    const std::filesystem::path& path,
    TextPosition position) const
{
    std::vector<LanguageLocation> result;
    if (implementation_ == nullptr || implementation_->semanticIndex == nullptr) return result;
    const std::string wanted = pathKey(path);
    const IndexedReference* bestReference = nullptr;
    for (const IndexedReference& reference : implementation_->semanticIndex->references)
    {
        if (pathKey(reference.location.path) != wanted || !contains(reference.location.range, position)) continue;
        if (bestReference == nullptr || rangeSize(reference.location.range) < rangeSize(bestReference->location.range) ||
            (rangeSize(reference.location.range) == rangeSize(bestReference->location.range) &&
             reference.targetSymbolIndices.size() < bestReference->targetSymbolIndices.size()))
            bestReference = &reference;
    }
    if (bestReference != nullptr)
    {
        for (const std::size_t target : bestReference->targetSymbolIndices)
            if (target < implementation_->semanticIndex->symbols.size())
                result.push_back(implementation_->semanticIndex->symbols[target].location);
        return result;
    }
    if (const auto symbol = symbolAt(path, position)) result.push_back(symbol->location);
    return result;
}

LanguageService::LanguageService()
    : implementation_(std::make_unique<Implementation>())
{
}

LanguageService::LanguageService(LanguageService&&) noexcept = default;
LanguageService& LanguageService::operator=(LanguageService&&) noexcept = default;
LanguageService::~LanguageService() = default;

LanguageServiceIdentity LanguageService::identity() const noexcept
{
    return LanguageServiceIdentity{
        kCompilerReleaseVersion,
        kLanguageVersion,
        kCompilerStage,
        target::TargetInfo::WindowsX64().llvmTriple(),
        kCompilerBuildId,
        kAnalysisAbiVersion};
}

bool LanguageService::openDocument(
    const std::filesystem::path& path,
    std::string text,
    std::int64_t version,
    std::string& error)
{
    error.clear();
    const auto keyedPath = Implementation::documentKey(path);
    if (!keyedPath.has_value() || version < 0)
    {
        error = version < 0 ? "document version cannot be negative" : "document path is invalid";
        return false;
    }
    std::scoped_lock lock(implementation_->mutex);
    if (implementation_->documents.contains(keyedPath->first))
    {
        error = "document is already open";
        return false;
    }
    implementation_->documents.emplace(
        keyedPath->first,
        Implementation::StoredDocument{keyedPath->second, std::move(text), version});
    return true;
}

bool LanguageService::updateDocument(
    const std::filesystem::path& path,
    std::string text,
    std::int64_t version,
    std::string& error)
{
    error.clear();
    const auto keyedPath = Implementation::documentKey(path);
    if (!keyedPath.has_value() || version < 0)
    {
        error = version < 0 ? "document version cannot be negative" : "document path is invalid";
        return false;
    }
    std::scoped_lock lock(implementation_->mutex);
    const auto found = implementation_->documents.find(keyedPath->first);
    if (found == implementation_->documents.end())
    {
        error = "document is not open";
        return false;
    }
    if (version <= found->second.version)
    {
        error = "document version is not newer than the current version";
        return false;
    }
    found->second.text = std::move(text);
    found->second.version = version;
    return true;
}

bool LanguageService::closeDocument(const std::filesystem::path& path) noexcept
{
    const auto keyedPath = Implementation::documentKey(path);
    if (!keyedPath.has_value()) return false;
    std::scoped_lock lock(implementation_->mutex);
    return implementation_->documents.erase(keyedPath->first) != 0;
}

std::optional<DocumentSnapshot> LanguageService::document(
    const std::filesystem::path& path) const
{
    const auto keyedPath = Implementation::documentKey(path);
    if (!keyedPath.has_value()) return std::nullopt;
    std::scoped_lock lock(implementation_->mutex);
    const auto found = implementation_->documents.find(keyedPath->first);
    if (found == implementation_->documents.end()) return std::nullopt;
    return DocumentSnapshot{found->second.path, found->second.text, found->second.version};
}

std::size_t LanguageService::documentCount() const noexcept
{
    std::scoped_lock lock(implementation_->mutex);
    return implementation_->documents.size();
}

AnalysisSnapshot LanguageService::analyzeProject(
    const std::filesystem::path& project,
    std::size_t jobs,
    std::function<bool()> cancellationCheck)
{
    auto result = std::make_shared<AnalysisSnapshot::Implementation>();
    result->generation = implementation_->nextGeneration.fetch_add(1, std::memory_order_relaxed);
    const auto keyedProject = Implementation::documentKey(project);
    if (!keyedProject.has_value())
    {
        result->error = "project path is invalid";
        return AnalysisSnapshot(std::move(result));
    }
    result->projectPath = keyedProject->second;

    std::unordered_map<std::string, std::string> overlays;
    {
        std::scoped_lock lock(implementation_->mutex);
        overlays.reserve(implementation_->documents.size());
        for (const auto& [key, document] : implementation_->documents)
        {
            overlays.emplace(key, document.text);
        }
    }

    auto compilation = std::make_shared<frontend::ProjectCompilation>();
    compilation->setCompilationJobs(std::max<std::size_t>(jobs, 1));
    compilation->setCancellationCheck(cancellationCheck);
    compilation->setSourceBufferProvider(
        [overlays = std::move(overlays)](const std::filesystem::path& path)
            -> std::optional<std::string> {
            const auto keyedPath = Implementation::documentKey(path);
            if (!keyedPath.has_value()) return std::nullopt;
            const auto found = overlays.find(keyedPath->first);
            return found != overlays.end() ? std::optional<std::string>(found->second) : std::nullopt;
        });

    result->currentCompilation = compilation;
    result->currentAnalysisSucceeded =
        compilation->compileProjectMethodBodies(result->projectPath, result->error);
    result->cancelled = cancellationCheck && cancellationCheck();
    for (const diagnostics::Diagnostic& diagnostic : compilation->diagnostics().diagnostics())
    {
        if (auto converted = makeLanguageDiagnostic(diagnostic, compilation->sourceManager()))
        {
            result->diagnostics.push_back(std::move(*converted));
        }
    }

    std::scoped_lock lock(implementation_->mutex);
    if (result->currentAnalysisSucceeded && !compilation->diagnostics().hasErrors())
    {
        result->semanticIndex = buildSemanticIndex(compilation);
        implementation_->lastGoodSemantic[keyedProject->first] = result->semanticIndex;
    }
    else
    {
        const auto previous = implementation_->lastGoodSemantic.find(keyedProject->first);
        if (previous != implementation_->lastGoodSemantic.end())
        {
            result->semanticIndex = previous->second;
            result->usedPreviousSemanticModel = true;
        }
    }
    return AnalysisSnapshot(std::move(result));
}
}
