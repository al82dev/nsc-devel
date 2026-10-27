#include "nativesharp/Frontend/ProjectNamespaceFunctionModel.h"

#include "Snapshot/NamespaceMemberSnapshotCodec.h"

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
namespace
{
constexpr std::string_view kNamespaceMembersSection = "namespace-members";

// EN: The payload of the `namespace-members` section, or nullptr when the snapshot has none.
// ES: El payload de la sección `namespace-members`, o nullptr si el snapshot no la tiene.
const std::string* namespaceMembersSection(const PrecompiledSnapshotUnit& snapshot) noexcept
{
    for (const compilation::PrecompiledSnapshotSection& section : snapshot.snapshot.sections)
    {
        if (section.model == kNamespaceMembersSection) return &section.payload;
    }
    return nullptr;
}

bool sameRange(const SourceRange& left, const SourceRange& right) noexcept
{
    return left.begin() == right.begin() && left.end() == right.end();
}

// EN: True when the parameter FileId and ranges are what the installer derives and every typed
//     pack takes its element from its own syntax (never from the consumer's generic model).
// ES: True si el FileId y los rangos de los parámetros son los que deriva el instalador y cada pack
//     tipado toma su elemento de su propia sintaxis (nunca del modelo genérico del consumidor).
bool derivableParameters(const std::vector<MethodParameterSymbol>& parameters, FileId fileId) noexcept
{
    for (const MethodParameterSymbol& parameter : parameters)
    {
        if (parameter.syntax == nullptr || parameter.fileId != fileId ||
            !sameRange(parameter.range, parameter.syntax->range()))
            return false;
        const bool packFromSyntax = parameter.typeSyntax != nullptr &&
            (parameter.typeSyntax->baseName() == "ReadOnlySpan" ||
             parameter.typeSyntax->baseName().ends_with(".ReadOnlySpan")) &&
            parameter.typeSyntax->typeArguments().size() == 1U;
        if (parameter.isParameterPack && !packFromSyntax) return false;
    }
    return true;
}

// EN: Encodes captured entries keyed by declaration node; a node captured twice must encode the
//     same. Appends the entry count and the entries to `payload`.
// ES: Codifica las entradas capturadas por nodo de declaración; un nodo capturado dos veces debe
//     codificarse igual. Agrega a `payload` el número de entradas y las entradas.
template <typename Symbol, typename Encode>
bool writeEntries(
    const PrecompiledSyntaxNodeIndex& index,
    const std::vector<std::pair<const ast::AstNode*, Symbol>>& captured,
    Encode encode,
    std::string& payload)
{
    std::unordered_map<const ast::AstNode*, std::string> encodedByNode;
    std::string entries;
    std::uint64_t count = 0;
    for (const std::pair<const ast::AstNode*, Symbol>& entry : captured)
    {
        PrecompiledSnapshotWriter writer(index);
        writer.node(entry.first);
        encode(writer, entry.second);
        if (writer.failed()) return false;
        std::string encoded = writer.take();
        const auto [found, inserted] = encodedByNode.emplace(entry.first, encoded);
        if (!inserted && found->second != encoded) return false;
        if (!inserted) continue;
        entries += encoded;
        ++count;
    }
    PrecompiledSnapshotWriter header(index);
    header.u64(count);
    payload += header.take() + entries;
    return true;
}

// EN: Reads `count`-prefixed entries written by writeEntries into `entries`.
// ES: Lee en `entries` las entradas precedidas por su número escritas por writeEntries.
template <typename Symbol, typename Decode>
bool readEntries(
    PrecompiledSnapshotReader& reader,
    Decode decode,
    std::unordered_map<const ast::AstNode*, Symbol>& entries)
{
    const std::uint64_t count = reader.u64();
    for (std::uint64_t entry = 0; entry < count && !reader.failed(); ++entry)
    {
        const ast::AstNode* const node = reader.node();
        Symbol symbol;
        if (node == nullptr || !decode(reader, symbol)) return false;
        entries.emplace(node, std::move(symbol));
    }
    return !reader.failed();
}
}

void ProjectNamespaceFunctionModel::setPrecompiledSnapshots(std::span<const PrecompiledSnapshotUnit> snapshots)
{
    for (const PrecompiledSnapshotUnit& snapshot : snapshots)
    {
        const std::string* const payload = namespaceMembersSection(snapshot);
        if (payload == nullptr) continue;
        PrecompiledSnapshotReader reader(*payload, snapshot.index);
        std::unordered_map<const ast::AstNode*, NamespaceFunctionSymbol> functions;
        std::unordered_map<const ast::AstNode*, NamespaceStateSymbol> states;
        if (!readEntries(reader, readNamespaceFunctionSnapshot, functions) ||
            !readEntries(reader, readNamespaceStateSnapshot, states) || !reader.atEnd())
            continue;
        snapshotFunctions_.merge(functions);
        snapshotStates_.merge(states);
    }
}

std::size_t ProjectNamespaceFunctionModel::installedSnapshotFunctionCount() const noexcept
{
    return installedSnapshotFunctions_;
}

std::size_t ProjectNamespaceFunctionModel::installedSnapshotStateCount() const noexcept
{
    return installedSnapshotStates_;
}

void ProjectNamespaceFunctionModel::setPrecompiledSnapshotCaptureFile(FileId fileId) noexcept
{
    snapshotCaptureFile_ = fileId;
}

void ProjectNamespaceFunctionModel::captureSnapshotFunction(
    const NamespaceFunctionDeclaration& declaration,
    const NamespaceFunctionSymbol& candidate)
{
    if (snapshotCaptureFile_.has_value() && declaration.fileId == *snapshotCaptureFile_)
        capturedFunctions_.emplace_back(declaration.syntax, candidate);
}

void ProjectNamespaceFunctionModel::captureSnapshotState(
    const NamespaceStateDeclaration& declaration,
    const NamespaceStateSymbol& state)
{
    if (snapshotCaptureFile_.has_value() && declaration.fileId == *snapshotCaptureFile_)
        capturedStates_.emplace_back(declaration.syntax, state);
}

bool ProjectNamespaceFunctionModel::writePrecompiledSnapshotNamespaceMembers(
    const PrecompiledSyntaxNodeIndex& index,
    std::string& payload,
    std::string& error) const
{
    if (!snapshotCaptureFile_.has_value())
    {
        error = "the API unit namespace members were not captured";
        return false;
    }
    for (const std::pair<const ast::AstNode*, NamespaceFunctionSymbol>& entry : capturedFunctions_)
    {
        if (!derivableParameters(entry.second.parameters, *snapshotCaptureFile_))
        {
            error = "a namespace function of the API unit cannot be derived from its unit";
            return false;
        }
    }
    payload.clear();
    if (!writeEntries(index, capturedFunctions_, writeNamespaceFunctionSnapshot, payload) ||
        !writeEntries(index, capturedStates_, writeNamespaceStateSnapshot, payload))
    {
        error = "a namespace member of the API unit cannot be written by identity";
        return false;
    }
    return true;
}

// EN: Installs the function computed by the producer for this declaration and fills what
//     initializeNamespaceFunction copies from the declaration, including its single part.
// ES: Instala la función calculada por el productor para esta declaración y llena lo que
//     initializeNamespaceFunction copia de la declaración, incluida su parte única.
bool ProjectNamespaceFunctionModel::installSnapshotFunction(
    const NamespaceFunctionDeclaration& declaration,
    NamespaceFunctionSymbol& candidate)
{
    const auto found = snapshotFunctions_.find(declaration.syntax);
    if (found == snapshotFunctions_.end()) return false;
    candidate = found->second;
    candidate.accessibility = declaration.accessibility;
    candidate.projectOrdinal = declaration.projectOrdinal;
    candidate.sourceOrdinal = declaration.sourceOrdinal;
    candidate.fileId = declaration.fileId;
    candidate.range = declaration.range;
    candidate.sourcePath = declaration.sourcePath;
    candidate.moduleName = declaration.moduleName;
    candidate.namespaceName = declaration.namespaceName;
    candidate.name = declaration.name;
    candidate.declarationSyntax = declaration.syntax;
    candidate.definitionSyntax = declaration.hasBody ? declaration.syntax : nullptr;
    candidate.isPrecompiled = declaration.isPrecompiled;
    candidate.parts.push_back(NamespaceFunctionPart{
        declaration.accessibility,
        declaration.sourceOrdinal,
        declaration.fileId,
        declaration.range,
        declaration.sourcePath,
        declaration.syntax,
        declaration.hasBody});
    for (MethodParameterSymbol& parameter : candidate.parameters)
    {
        parameter.fileId = declaration.fileId;
        parameter.range = parameter.syntax->range();
    }
    ++installedSnapshotFunctions_;
    return true;
}

// EN: Installs the state computed by the producer and fills what initializeNamespaceState copies
//     from the declaration.
// ES: Instala el estado calculado por el productor y llena lo que initializeNamespaceState copia
//     de la declaración.
bool ProjectNamespaceFunctionModel::installSnapshotState(
    const NamespaceStateDeclaration& declaration,
    NamespaceStateSymbol& state)
{
    const auto found = snapshotStates_.find(declaration.syntax);
    if (found == snapshotStates_.end()) return false;
    state = found->second;
    state.accessibility = declaration.accessibility;
    state.projectOrdinal = declaration.projectOrdinal;
    state.sourceOrdinal = declaration.sourceOrdinal;
    state.fileId = declaration.fileId;
    state.range = declaration.range;
    state.sourcePath = declaration.sourcePath;
    state.moduleName = declaration.moduleName;
    state.namespaceName = declaration.namespaceName;
    state.name = declaration.name;
    state.declarationSyntax = declaration.syntax;
    state.isConst = declaration.isConst;
    state.isPrecompiled = declaration.isPrecompiled;
    ++installedSnapshotStates_;
    return true;
}
}
