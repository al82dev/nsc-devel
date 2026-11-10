#pragma once

// EN: T08-D25. Public C type surface of a NativeSharp library. A CppLibraryReference declared with
//     PublicTypes="Header" publishes, in the library's NSMeta C catalog, every named C record, enum and typedef
//     that its header itself declares or that a header under the same directory declares, even when no public
//     NativeSharp signature uses it yet. Headers outside that directory (CRT, SDK, other libraries) only contribute
//     through the closure of a published type, as before. Reserved identifiers (`_X`, `__x`) are implementation
//     details and are not part of the surface. Two surface declarations with the same name and a different
//     definition are rejected: NSC never chooses one of them silently.
// ES: T08-D25. Superficie publica de tipos C de una biblioteca NativeSharp. Una CppLibraryReference declarada con
//     PublicTypes="Header" publica en el catalogo C de NSMeta de la biblioteca todo record, enum y typedef C con
//     nombre que declare su encabezado o un encabezado bajo el mismo directorio, aunque ninguna firma publica
//     NativeSharp lo use todavia. Los encabezados fuera de ese directorio (CRT, SDK, otras bibliotecas) solo
//     aportan por la clausura de un tipo publicado, como antes. Los identificadores reservados (`_X`, `__x`) son
//     detalles de implementacion y no forman parte de la superficie. Dos declaraciones de la superficie con el
//     mismo nombre y distinta definicion se rechazan: NSC nunca elige una en silencio.

#include "nativesharp/Compilation/ProjectGraph.h"
#include "nativesharp/Compilation/TypeIdentity.h"
#include "nativesharp/Interop/CppSemanticInventory.h"
#include "nativesharp/Interop/CppSemanticRecord.h"

#include <cctype>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace nativesharp::interop::detail
{
inline std::string publicHeaderFoldedPath(const std::filesystem::path& path)
{
    std::error_code ec;
    std::filesystem::path normal = std::filesystem::weakly_canonical(path, ec);
    if (ec) normal = path.lexically_normal();
    std::string text = normal.generic_string();
    for (char& value : text) value = static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
    return text;
}

// EN: True when the declaration file is the header itself or lies under the header's directory.
// ES: Verdadero si el archivo de la declaracion es el encabezado o esta bajo su directorio.
inline bool publicHeaderOwnsFile(const std::string& foldedRoot, const std::filesystem::path& file)
{
    if (file.empty()) return false;
    const std::string folded = publicHeaderFoldedPath(file);
    return folded.size() > foldedRoot.size() && folded.compare(0, foldedRoot.size(), foldedRoot) == 0 &&
        folded[foldedRoot.size()] == '/';
}

// EN: C reserves identifiers that begin with two underscores or with an underscore and an uppercase letter.
// ES: C reserva los identificadores que empiezan con dos guiones bajos o con guion bajo y mayuscula.
inline bool publicHeaderReservedName(const std::string& name)
{
    if (name.size() < 2 || name[0] != '_') return false;
    return name[1] == '_' || std::isupper(static_cast<unsigned char>(name[1])) != 0;
}

struct PublicHeaderReference final
{
    std::size_t projectOrdinal = 0;
    std::size_t referenceIndex = 0;
    std::string foldedRoot;
};

struct PublicHeaderDeclaration final
{
    std::string signature;
    std::string file;
};

inline std::vector<PublicHeaderReference> collectPublicHeaderReferences(const compilation::ProjectGraph& graph)
{
    std::vector<PublicHeaderReference> result;
    for (std::size_t project = 0; project < graph.projects().size(); ++project)
    {
        const auto references = graph.projects()[project].cppLibraryReferences();
        for (std::size_t index = 0; index < references.size(); ++index)
        {
            if (!references[index].publicTypes || references[index].language != CppLanguageMode::C) continue;
            result.push_back({project, index, publicHeaderFoldedPath(references[index].headerPath.parent_path())});
        }
    }
    return result;
}

inline const PublicHeaderReference* findPublicHeaderReference(
    const std::vector<PublicHeaderReference>& references, std::size_t project, std::size_t index)
{
    for (const auto& reference : references)
    {
        if (reference.projectOrdinal == project && reference.referenceIndex == index) return &reference;
    }
    return nullptr;
}

// EN: Records the first declaration of a name in one C name space (tags or ordinary identifiers) and rejects a
//     different definition under the same name in the same project.
// ES: Registra la primera declaracion de un nombre en un espacio de nombres C (etiquetas o identificadores
//     ordinarios) y rechaza otra definicion con el mismo nombre en el mismo proyecto.
inline bool registerPublicHeaderName(
    std::map<std::pair<std::size_t, std::string>, PublicHeaderDeclaration>& seen, std::size_t project,
    const std::string& key, const std::string& name, const std::string& signature,
    const std::filesystem::path& file, bool& isNew, std::string& error)
{
    const auto inserted = seen.emplace(std::make_pair(project, key), PublicHeaderDeclaration{signature,
        file.generic_string()});
    isNew = inserted.second;
    if (inserted.second || inserted.first->second.signature == signature) return true;
    error = "CppLibraryReference PublicTypes: C type '" + name + "' has different declarations in '" +
        inserted.first->second.file + "' and '" + file.generic_string() +
        "'; the library cannot publish both under the same name";
    return false;
}

inline std::string publicHeaderRecordSignature(const CppRecordDeclaration& record)
{
    std::string text = std::to_string(static_cast<int>(record.kind)) + ":" +
        (record.complete ? std::string("complete") : std::string("incomplete")) + ":" +
        std::to_string(record.sizeBits.value_or(0)) + ":" + std::to_string(record.alignmentBits.value_or(0));
    for (const auto& field : record.fields)
    {
        text += "|" + field.name + ":" + field.type.canonicalSpelling;
    }
    return text;
}

inline std::string publicHeaderEnumSignature(const CppEnumDeclaration& value)
{
    std::string text = value.underlyingType ? value.underlyingType->canonicalSpelling : std::string("?");
    for (const auto& enumerator : value.enumerators)
    {
        text += "|" + enumerator.name + "=" + enumerator.decimalValue;
    }
    return text;
}

struct PublicHeaderSelection final
{
    std::map<std::size_t, std::set<std::string>> recordIdentities;
    std::map<std::size_t, std::set<std::string>> enumIdentities;
    std::vector<CppPublishedTemplateAliasCandidates> aliasCandidates;
    std::vector<CppSemanticTypeAliasEntry> aliasInventory;
};

// EN: Selects the public surface of every PublicTypes reference. Returns false with the cause when an inventory
//     fails or two surface declarations conflict.
// ES: Selecciona la superficie publica de cada referencia PublicTypes. Devuelve false con la causa si falla un
//     inventario o si dos declaraciones de la superficie entran en conflicto.
inline bool selectPublicHeaderTypes(const compilation::ProjectGraph& graph,
    std::span<const CppProjectSessions> sessions, PublicHeaderSelection& output, std::string& error)
{
    output = {};
    const std::vector<PublicHeaderReference> references = collectPublicHeaderReferences(graph);
    if (references.empty()) return true;
    std::map<std::pair<std::size_t, std::string>, PublicHeaderDeclaration> seen;

    auto records = collectCppSemanticRecords(graph, sessions);
    if (const auto* failure = std::get_if<CppSemanticRecordFailure>(&records))
    {
        error = failure->message;
        return false;
    }
    for (const auto& entry : std::get<std::vector<CppSemanticRecordEntry>>(records))
    {
        const PublicHeaderReference* reference =
            findPublicHeaderReference(references, entry.projectOrdinal, entry.referenceIndex);
        // EN: An incomplete declaration only names a type defined elsewhere; the definition carries the surface.
        // ES: Una declaracion incompleta solo nombra un tipo definido en otro lugar; la definicion es la superficie.
        if (!reference || entry.declaration.isCxx || !entry.identity || entry.declaration.anonymousContext ||
            !entry.declaration.complete || entry.declaration.isAnonymousCInternal || !entry.declaration.location ||
            !publicHeaderOwnsFile(reference->foldedRoot, entry.declaration.location->file)) continue;
        const std::string name = entry.identity->name();
        if (name.empty() || publicHeaderReservedName(name)) continue;
        bool isNew = false;
        if (!registerPublicHeaderName(seen, entry.projectOrdinal, "tag:" + name, name,
                publicHeaderRecordSignature(entry.declaration), entry.declaration.location->file, isNew, error))
            return false;
        output.recordIdentities[entry.projectOrdinal].insert(entry.identity->key());
    }

    auto enums = collectCppSemanticEnums(graph, sessions);
    if (const auto* failure = std::get_if<CppSemanticInventoryFailure>(&enums))
    {
        error = failure->message;
        return false;
    }
    for (const auto& entry : std::get<std::vector<CppSemanticEnumEntry>>(enums))
    {
        const PublicHeaderReference* reference =
            findPublicHeaderReference(references, entry.projectOrdinal, entry.referenceIndex);
        if (!reference || entry.declaration.isCxx || entry.declaration.name.empty() ||
            publicHeaderReservedName(entry.declaration.name) || !entry.declaration.location ||
            !publicHeaderOwnsFile(reference->foldedRoot, entry.declaration.location->file)) continue;
        bool isNew = false;
        if (!registerPublicHeaderName(seen, entry.projectOrdinal, "tag:" + entry.declaration.name,
                entry.declaration.name, publicHeaderEnumSignature(entry.declaration),
                entry.declaration.location->file, isNew, error))
            return false;
        compilation::TypeIdentity identity(graph.projects()[entry.projectOrdinal].moduleName(), {}, {},
            entry.declaration.name);
        if (identity.isValid()) output.enumIdentities[entry.projectOrdinal].insert(identity.key());
    }

    auto aliases = collectCppSemanticTypeAliases(graph, sessions);
    if (const auto* failure = std::get_if<CppSemanticInventoryFailure>(&aliases))
    {
        error = failure->message;
        return false;
    }
    output.aliasInventory = std::move(std::get<std::vector<CppSemanticTypeAliasEntry>>(aliases));
    for (std::size_t index = 0; index < output.aliasInventory.size(); ++index)
    {
        const auto& entry = output.aliasInventory[index];
        const PublicHeaderReference* reference =
            findPublicHeaderReference(references, entry.projectOrdinal, entry.referenceIndex);
        if (!reference || entry.declaration.insideAnonymousNamespace || entry.declaration.name.empty() ||
            publicHeaderReservedName(entry.declaration.name) || !entry.declaration.location ||
            !publicHeaderOwnsFile(reference->foldedRoot, entry.declaration.location->file)) continue;
        bool isNew = false;
        if (!registerPublicHeaderName(seen, entry.projectOrdinal, "typedef:" + entry.declaration.name,
                entry.declaration.name, entry.declaration.underlyingType.canonicalSpelling,
                entry.declaration.location->file, isNew, error))
            return false;
        // EN: The same typedef seen through a second reference is published once.
        // ES: El mismo typedef visto por una segunda referencia se publica una vez.
        if (!isNew) continue;
        CppPublishedTemplateAliasCandidates candidate{};
        candidate.templateKey = "cpp-public-types";
        candidate.candidateIndex = index;
        candidate.inventoryIndices.push_back(index);
        output.aliasCandidates.push_back(std::move(candidate));
    }
    error.clear();
    return true;
}
}
