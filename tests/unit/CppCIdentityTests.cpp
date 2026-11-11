// T08-002: identity of imported C declarations.
//
// EN: Targeted checks of the Clang USR carried by imported C declarations. The first mode opens
//     real Clang 23.1.0 sessions on headers written beneath a work directory and records, per
//     declaration category, whether the USR exists, whether it is stable across independent
//     sessions of the same header, and whether it changes when the same text lives in a header
//     with another file name. It also exercises the NSC selectors with two such headers in one
//     project. The second mode restores a C catalogue from a real producer NSMETA through the
//     same frontend providers used by a consumer and compares every restored USR with Clang.
// ES: Pruebas dirigidas del USR de Clang que conservan las declaraciones C importadas. El primer
//     modo abre sesiones reales de Clang 23.1.0 sobre encabezados escritos bajo un directorio de
//     trabajo y registra, por categoría de declaración, si el USR existe, si es estable entre
//     sesiones independientes del mismo encabezado y si cambia cuando el mismo texto vive en un
//     encabezado con otro nombre de archivo. También ejercita los selectores de NSC con dos de esos
//     encabezados en un proyecto. El segundo modo restaura un catálogo C desde el NSMETA de un
//     productor real mediante los mismos proveedores del frontend que usa un consumidor y compara
//     cada USR restaurado con Clang.
// EN: The third mode reads typedefs/casos.txt and compares, per case, the current alias rule and the
//     proposed one (T08-002-PROPUESTA-TYPEDEF.md) with the outcomes nsc gives for the same headers.
// ES: El tercer modo lee typedefs/casos.txt y compara, por caso, la regla actual de alias y la
//     propuesta (T08-002-PROPUESTA-TYPEDEF.md) con los resultados que da nsc con esos encabezados.
//
// Usage / Uso:
//   nsc_cpp_c_identity_tests <clang-resource-dir> <work-dir> <clang-driver>
//   nsc_cpp_c_identity_tests --verify-restored-c-usr <producer.nsmeta> <header> <clang-resource-dir>
//                            <clang-driver> <work-dir>
//   nsc_cpp_c_identity_tests --verify-driver-restored-c-usr <producer.lib> <header> <clang-resource-dir>
//                            <clang-driver>
//   nsc_cpp_c_identity_tests --typedef-matrix <typedefs-dir> <clang-resource-dir> <clang-driver>
//   nsc_cpp_c_identity_tests --inspect-c-constants <producer.nsmeta> <forbidden-state> <required-enumerator>
//   nsc_cpp_c_identity_tests --catalogue-version-controls <fixtures-dir> <producer.nsmeta>...

#include "nativesharp/Interop/CppClangSessionFactory.h"
#include "nativesharp/Interop/CppAbiPlanQueries.h"
#include "nativesharp/Interop/CppProjectSessions.h"
#include "nativesharp/Interop/CppSemanticFunctionGroups.h"
#include "nativesharp/Interop/CppSemanticInventory.h"
#include "nativesharp/Interop/CppSemanticRecord.h"
#include "nativesharp/Interop/LibraryMetadata.h"
#include "nativesharp/Interop/LibraryMetadataCodec.h"
#include "nativesharp/Frontend/CppAliasEnumType.h"
#include "nativesharp/Frontend/CppAliasRecordType.h"
#include "nativesharp/Frontend/PrecompiledSyntaxModule.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNamespaceFunctionModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Compilation/CompilationInputManifest.h"
#include "nativesharp/Compilation/ProjectGraphLoader.h"
#include "nativesharp/Compilation/TypeIdentity.h"
#include "nativesharp/Frontend/NamespaceStateSymbol.h"
#include "nativesharp/Frontend/CppImportedTypeAlias.h"
#include "../../src/Interop/Internal/CppCRecordMetadataCodec.h"
#include "../../src/Interop/Internal/LibraryMetadataCodecSupport.h"
#include "../../src/Driver/Internal/DriverInternal.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

using namespace nativesharp::interop;

namespace
{
using UsrTable = std::map<std::string, std::optional<std::string>>;

// EN: Header text shared by every probe; each line declares one category under test.
// ES: Texto de encabezado común a todas las sondas; cada línea declara una categoría probada.
const char* const kProbeHeader =
    "typedef int CIdInt;\n"
    "struct CIdRecord { int value; };\n"
    "struct __attribute__((packed)) CIdPacked { int value; };\n"
    "int __attribute__((sysv_abi)) cid_variadic(int, ...);\n"
    "struct CIdCarriers { int tag; union { int x; float f; struct { short z; }; }; struct { short y; }; unsigned : 1; };\n"
    "typedef struct { int raw; } CIdAnonRecord;\n"
    "union CIdUnion { int whole; unsigned char bytes[4]; };\n"
    "enum CIdKind { CIdFirst = 1, CIdSecond = 2 };\n"
    "enum { CIdLoose = 7 };\n"
    "extern int cid_global;\n"
    "static int cid_static_global = 3;\n"
    "int cid_function(int value);\n"
    "static inline int cid_inline(void) { return 1; }\n";

// EN: Categories whose USR Clang 23.1.0 derives without the header file name. Observed forms:
//     c:@F@f, c:@g, c:@S@R, c:@S@R@FI@f, c:@U@U, c:@E@E, c:@E@E@X, c:@SA@T for a struct named
//     only by a typedef and, for an anonymous enum, c:@Ea@<first enumerator>@X (the anonymous
//     enum is named after its first enumerator).
//     The remaining categories carry the header file name: c:a.h@T@T, c:a.h@F@f, c:a.h@g.
// ES: Categorías cuyo USR deriva Clang 23.1.0 sin el nombre del encabezado. Formas observadas:
//     c:@F@f, c:@g, c:@S@R, c:@S@R@FI@f, c:@U@U, c:@E@E, c:@E@E@X, c:@SA@T para un struct
//     nombrado solo por un typedef y, para un enum anónimo, c:@Ea@<primer enumerador>@X (el enum
//     anónimo se nombra por su primer enumerador).
//     Las demás categorías incluyen el nombre del encabezado: c:a.h@T@T, c:a.h@F@f, c:a.h@g.
const std::vector<std::string> kHeaderIndependent = {
    "function", "global", "record", "field", "union", "enum", "enumerator", "anonymous-enumerator",
    "typedef-anonymous-record"};
const std::vector<std::string> kHeaderDependent = {"inline-function", "static-global", "alias"};

// EN: Homonymous headers: dirA/same.h and dirB/same.h have this identical text.
// ES: Encabezados homónimos: dirA/same.h y dirB/same.h tienen este texto idéntico.
const char* const kHomonymHeader =
    "typedef int CSameInt;\n"
    "static int c_same_static = 3;\n"
    "static inline int c_same_inline(void) { return 1; }\n";

// EN: dirC/same.h: same names and file name, different declarations.
// ES: dirC/same.h: mismos nombres y nombre de archivo, declaraciones distintas.
const char* const kHomonymOtherHeader =
    "typedef long long CSameInt;\n"
    "static int c_same_static = 4;\n"
    "static inline int c_same_inline(void) { return 2; }\n";

// EN: Reports a failed expectation on stderr. Returns the condition unchanged; no side effects
//     besides the message. O(1).
// ES: Informa una expectativa fallida por stderr. Devuelve la condición sin cambios; sin efectos
//     además del mensaje. O(1).
bool expect(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}

// EN: Builds C11 options for a buffer that includes `header` from `workingDirectory`.
//     The returned value owns its strings; nothing is opened here.
// ES: Construye opciones C11 para un buffer que incluye `header` desde `workingDirectory`.
//     El valor devuelto es dueño de sus cadenas; aquí no se abre nada.
CppSessionOptions headerOptions(const std::string& resource, const std::filesystem::path& workingDirectory,
                                const std::string& driver, const std::string& header)
{
    CppSessionOptions result;
    result.language = CppLanguageMode::C;
    result.languageStandard = "c11";
    result.targetTriple = "x86_64-pc-windows-msvc";
    result.driverReferencePath = driver;
    result.resourceDirectory = resource;
    result.workingDirectory = workingDirectory;
    result.input = CppBufferInput{"probe.c", "#include \"" + header + "\"\n"};
    return result;
}

// EN: Opens one session and prints the failure phase and diagnostics. Returns null on failure.
//     The caller owns and closes the session.
// ES: Abre una sesión e imprime la fase y los diagnósticos si falla. Devuelve nulo si falla.
//     El llamador es dueño de la sesión y la cierra.
std::unique_ptr<CppInteropSession> openSession(CppClangSessionFactory& factory, CppSessionOptions options)
{
    auto result = factory.open(std::move(options), {});
    if (auto* failure = std::get_if<CppSessionOpenFailure>(&result))
    {
        std::cerr << "Open failed in " << failure->phase << ": " << failure->message << '\n';
        for (const auto& diagnostic : failure->diagnostics)
        {
            std::cerr << diagnostic.message << '\n';
        }
        return {};
    }
    return std::move(std::get<std::unique_ptr<CppInteropSession>>(result));
}

// EN: Writes `text` to `path`, replacing any previous content. Returns false on I/O failure.
// ES: Escribe `text` en `path` y reemplaza el contenido previo. Devuelve false si falla la E/S.
bool writeText(const std::filesystem::path& path, const std::string& text)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << text;
    return static_cast<bool>(file);
}

// EN: Reads function, global and alias USRs of the probe header into `table`. Missing
//     declarations stay absent so the caller can report them. O(n) in the snapshot sizes.
// ES: Lee los USR de funciones, globales y alias del encabezado de sonda en `table`. Las
//     declaraciones ausentes quedan ausentes para que el llamador las informe. O(n) en los snapshots.
void collectCallableAndAliasUsrs(const CppInteropSession& session, UsrTable& table)
{
    if (const auto functions = session.freeFunctionsSnapshot())
    {
        for (const auto& function : *functions)
        {
            if (function.qualifiedName == "cid_function") table["function"] = function.clangUsr;
            if (function.qualifiedName == "cid_inline") table["inline-function"] = function.clangUsr;
        }
    }
    if (const auto globals = session.globalsSnapshot())
    {
        for (const auto& global : *globals)
        {
            if (global.qualifiedName == "cid_global") table["global"] = global.clangUsr;
            if (global.qualifiedName == "cid_static_global") table["static-global"] = global.clangUsr;
        }
    }
    if (const auto aliases = session.typeAliasesSnapshot())
    {
        for (const auto& alias : *aliases)
        {
            if (alias.qualifiedName == "CIdInt") table["alias"] = alias.clangUsr;
        }
    }
}

// EN: Reads record, field, enum and enumerator USRs of the probe header into `table`.
//     The anonymous enum is identified by its only enumerator. O(n) in the snapshot sizes.
// ES: Lee los USR de record, campo, enum y enumerador del encabezado de sonda en `table`.
//     El enum anónimo se identifica por su único enumerador. O(n) en los snapshots.
void collectTypeUsrs(const CppInteropSession& session, UsrTable& table)
{
    if (const auto records = session.recordsSnapshot())
    {
        for (const auto& record : *records)
        {
            if (record.qualifiedName == "CIdRecord")
            {
                table["record"] = record.clangUsr;
                if (!record.fields.empty()) table["field"] = record.fields.front().clangUsr;
            }
            if (record.qualifiedName == "CIdUnion") table["union"] = record.clangUsr;
            if (record.qualifiedName == "CIdAnonRecord") table["typedef-anonymous-record"] = record.clangUsr;
        }
    }
    if (const auto enums = session.enumsSnapshot())
    {
        for (const auto& enumeration : *enums)
        {
            if (enumeration.name == "CIdKind")
            {
                table["enum"] = enumeration.clangUsr;
                if (!enumeration.enumerators.empty()) table["enumerator"] = enumeration.enumerators.front().clangUsr;
            }
            const bool loose = enumeration.name.empty() && enumeration.enumerators.size() == 1 &&
                enumeration.enumerators.front().name == "CIdLoose";
            if (loose) table["anonymous-enumerator"] = enumeration.enumerators.front().clangUsr;
        }
    }
}

// EN: Collects the complete USR table of one session. The table owns copies of the strings.
// ES: Reúne la tabla completa de USR de una sesión. La tabla es dueña de copias de las cadenas.
UsrTable collectUsrs(const CppInteropSession& session)
{
    UsrTable table;
    collectCallableAndAliasUsrs(session, table);
    collectTypeUsrs(session, table);
    return table;
}

// EN: Returns the USR stored for `key`, or "<absent>". Used only for evidence output.
// ES: Devuelve el USR guardado para `key`, o "<absent>". Solo se usa para la salida de evidencia.
std::string show(const UsrTable& table, const std::string& key)
{
    const auto found = table.find(key);
    if (found == table.end() || !found->second) return "<absent>";
    return *found->second;
}

// EN: Prints one evidence line per category: USR in the first header, in an independent session
//     of the same header and in the renamed copy.
// ES: Imprime una línea de evidencia por categoría: USR en el primer encabezado, en una sesión
//     independiente del mismo encabezado y en la copia renombrada.
void printTables(const UsrTable& first, const UsrTable& again, const UsrTable& renamed)
{
    for (const auto& entry : first)
    {
        const auto& key = entry.first;
        std::cout << "USR " << key << " a.h=" << show(first, key) << " a.h(again)=" << show(again, key)
                  << " b.h=" << show(renamed, key) << '\n';
    }
}

// EN: Checks presence, cross-session stability and the documented header dependence of each USR.
// ES: Comprueba presencia, estabilidad entre sesiones y la dependencia de encabezado de cada USR.
bool checkUsrTables(const UsrTable& first, const UsrTable& again, const UsrTable& renamed)
{
    bool ok = true;
    std::vector<std::string> all = kHeaderIndependent;
    all.insert(all.end(), kHeaderDependent.begin(), kHeaderDependent.end());
    for (const auto& key : all)
    {
        const auto found = first.find(key);
        const bool present = found != first.end() && found->second && !found->second->empty();
        ok &= expect(present, "Clang provides a USR for C category " + key);
        if (!present) continue;
        const auto stable = again.find(key);
        ok &= expect(stable != again.end() && stable->second == found->second,
                     "USR of " + key + " is identical in an independent session of the same header");
        const auto moved = renamed.find(key);
        const bool movedPresent = moved != renamed.end() && moved->second && !moved->second->empty();
        ok &= expect(movedPresent, "renamed header still provides a USR for " + key);
        if (!movedPresent) continue;
        const bool independent = std::find(kHeaderIndependent.begin(), kHeaderIndependent.end(), key) !=
            kHeaderIndependent.end();
        ok &= expect(independent ? moved->second == found->second : moved->second != found->second,
                     independent ? "USR of " + key + " does not depend on the header file name"
                                 : "USR of " + key + " depends on the header file name");
    }
    return ok;
}

// EN: Probe 1. Opens a.h twice and b.h once (identical text) and checks the USR relations.
// ES: Sonda 1. Abre a.h dos veces y b.h una vez (texto idéntico) y comprueba las relaciones de USR.
bool checkUsrStability(CppClangSessionFactory& factory, const std::string& resource,
                       const std::filesystem::path& directory, const std::string& driver)
{
    auto first = openSession(factory, headerOptions(resource, directory, driver, "a.h"));
    auto again = openSession(factory, headerOptions(resource, directory, driver, "a.h"));
    auto renamed = openSession(factory, headerOptions(resource, directory, driver, "b.h"));
    if (!expect(first && again && renamed, "probe sessions open on a.h and b.h")) return false;
    const auto firstTable = collectUsrs(*first);
    const auto records = first->recordsSnapshot();
    bool carriersOk = expect(records.has_value(), "anonymous carrier record snapshot is available");
    if (records)
    {
        const auto record = std::find_if(records->begin(), records->end(), [](const auto& item) {
            return item.qualifiedName == "CIdCarriers";
        });
        carriersOk &= expect(record != records->end(), "anonymous carrier owner is captured");
        if (record != records->end())
        {
            carriersOk &= expect(record->fields.size() == 4 && !record->fields[0].isAnonymousCarrier &&
                record->fields[1].isAnonymousCarrier && record->fields[2].isAnonymousCarrier &&
                !record->fields[3].isAnonymousCarrier && record->fields[3].bitWidth.has_value(),
                "anonymous union and struct carriers are distinguished from unnamed bit-fields");
            const auto findPath = [&](std::vector<std::size_t> path) {
                return std::find_if(record->anonymousCRecords.begin(), record->anonymousCRecords.end(),
                    [&](const auto& nested) { return nested.carrierPath == path; });
            };
            const auto nestedUnion = findPath({1});
            const auto nestedStruct = findPath({2});
            const auto deepStruct = findPath({1, 2});
            carriersOk &= expect(record->anonymousCRecords.size() == 3 &&
                nestedUnion != record->anonymousCRecords.end() && nestedUnion->kind == CppRecordKind::Union &&
                nestedUnion->fields.size() == 3 && nestedUnion->fields[2].isAnonymousCarrier &&
                nestedStruct != record->anonymousCRecords.end() && nestedStruct->kind == CppRecordKind::Struct &&
                deepStruct != record->anonymousCRecords.end() && deepStruct->fields.size() == 1 &&
                deepStruct->fields[0].name == "z" && deepStruct->fields[0].offsetBits == 0 &&
                deepStruct->sizeBits == 16 && deepStruct->alignmentBits == 16,
                "anonymous aggregates preserve nested paths, layout and source fields");
            const auto materialized = materializeCppAnonymousCRecords(*record);
            const auto* declarations = std::get_if<std::vector<CppRecordDeclaration>>(&materialized);
            carriersOk &= expect(declarations && declarations->size() == 4 &&
                (*declarations)[0].fields[1].name == "__nsc_c_anonymous_1" &&
                (*declarations)[0].fields[1].type.recordIdentity &&
                (*declarations)[0].fields[1].type.recordIdentity->containingRecords ==
                    std::vector<std::string>{"CIdCarriers"} &&
                (*declarations)[1].kind == CppRecordKind::Union &&
                (*declarations)[1].fields[2].name == "__nsc_c_anonymous_2" &&
                (*declarations)[3].containingRecords ==
                    std::vector<std::string>({"CIdCarriers", "__nsc_c_anonymous_1"}) &&
                record->fields[1].name.empty(),
                "anonymous records get deterministic nested identities without modifying the Clang observation");
            if (declarations)
            {
                std::vector<CppCRecordSnapshot> snapshots;
                for (const auto& declaration : *declarations)
                {
                    const auto identity = projectCppRecordIdentity(declaration, "Probe");
                    if (!identity) { carriersOk = false; break; }
                    CppCRecordSnapshot snapshot;
                    snapshot.identityKey = identity->key();
                    snapshot.moduleName = "Probe";
                    snapshot.namespaceSegments = declaration.namespaceSegments;
                    snapshot.containingRecords = declaration.containingRecords;
                    snapshot.name = declaration.name;
                    snapshot.targetTriple = "x86_64-pc-windows-msvc";
                    snapshot.kind = declaration.kind;
                    snapshot.complete = true;
                    snapshot.sizeBits = declaration.sizeBits;
                    snapshot.alignmentBits = declaration.alignmentBits;
                    snapshot.fields = declaration.fields;
                    snapshots.push_back(std::move(snapshot));
                }
                std::sort(snapshots.begin(), snapshots.end(), [](const auto& a, const auto& b) {
                    return a.identityKey < b.identityKey;
                });
                std::vector<std::uint8_t> bytes;
                std::string error;
                metadata_detail::CDependencyCatalog dependencies, restoredDependencies;
                std::vector<CppCRecordSnapshot> restored;
                std::vector<CppCEnumSnapshot> restoredEnums;
                const bool encoded = snapshots.size() == declarations->size() &&
                    metadata_detail::encodeCTypeCatalogWithAnonymousCarriers(snapshots, {}, dependencies, bytes, error);
                const bool decoded = encoded && metadata_detail::decodeCTypeCatalogWithAnonymousCarriers(
                    bytes, restored, restoredEnums, restoredDependencies, error);
                std::size_t carrierCount = 0;
                for (const auto& item : restored)
                    for (const auto& field : item.fields) carrierCount += field.isAnonymousCarrier;
                carriersOk &= expect(decoded && restored.size() == 4 && carrierCount == 3,
                    "anonymous carrier envelope preserves all nested carrier markers");
                dependencies.enumerators.push_back({"Probe", "header-id", "CConstant", "c:@E@CConstant",
                                                    "builtin:int", 7});
                std::vector<std::uint8_t> combined;
                std::vector<CppCRecordSnapshot> combinedRecords;
                std::vector<CppCEnumSnapshot> combinedEnums;
                metadata_detail::CDependencyCatalog combinedDependencies;
                const bool combinedOk = metadata_detail::encodeCTypeCatalogWithAnonymousCarriers(
                    snapshots, {}, dependencies, combined, error) &&
                    metadata_detail::decodeCTypeCatalogAndDependencies(combined, 48, combinedRecords,
                        combinedEnums, combinedDependencies, error);
                std::size_t combinedCarriers = 0;
                for (const auto& item : combinedRecords)
                    for (const auto& field : item.fields) combinedCarriers += field.isAnonymousCarrier;
                carriersOk &= expect(combinedOk && combinedCarriers == 3 &&
                    combinedDependencies.enumerators.size() == 1 &&
                    combinedDependencies.enumerators[0].name == "CConstant" &&
                    combinedDependencies.enumerators[0].bits == 7,
                    "anonymous carriers and C enumerator dependencies survive the same envelope");
                const auto observedEnums = first->enumsSnapshot();
                if (observedEnums)
                {
                    const auto enumeration = std::find_if(observedEnums->begin(), observedEnums->end(),
                        [](const auto& item) { return item.name == "CIdKind"; });
                    if (enumeration != observedEnums->end())
                    {
                        std::vector<CppCEnumSnapshot> inputEnums{{"Probe::CIdKind", "x86_64-pc-windows-msvc", *enumeration}};
                        std::vector<std::uint8_t> provenanceBytes;
                        std::vector<CppCRecordSnapshot> provenanceRecords;
                        std::vector<CppCEnumSnapshot> provenanceEnums;
                        metadata_detail::CDependencyCatalog provenanceDependencies;
                        const bool provenanceOk = enumeration->sourceProvenance &&
                            metadata_detail::encodeCTypeCatalogWithEnumProvenance(snapshots, inputEnums,
                                dependencies, provenanceBytes, error) &&
                            metadata_detail::decodeCTypeCatalogWithEnumProvenance(provenanceBytes, provenanceRecords,
                                provenanceEnums, provenanceDependencies, error);
                        std::size_t restoredCarriers = 0;
                        for (const auto& item : provenanceRecords)
                            for (const auto& field : item.fields) restoredCarriers += field.isAnonymousCarrier;
                        carriersOk &= expect(provenanceOk && provenanceEnums.size() == 1 &&
                            provenanceEnums[0].declaration.sourceProvenance == enumeration->sourceProvenance &&
                            restoredCarriers == 3 && provenanceDependencies.enumerators.size() == 1,
                            "enum source provenance survives with carriers and dependencies");
                        if (provenanceOk)
                        {
                            using Comparison = metadata_detail::CRecordContractComparison;
                            const auto compare = [&](const CppCEnumSnapshot& candidate, Comparison expected,
                                                     const char* message) {
                                Comparison actual{};
                                const bool compared = metadata_detail::compareCEnumSourceEvidence(
                                    inputEnums[0], candidate, actual, error);
                                carriersOk &= expect(compared && actual == expected, message);
                            };
                            compare(provenanceEnums[0], Comparison::Matching,
                                "same enum contract and source are reconciled");
                            auto changed = provenanceEnums[0];
                            changed.declaration.enumerators[0].decimalValue = "999";
                            compare(changed, Comparison::Different,
                                "changed enum value prevents reconciliation");
                            changed = provenanceEnums[0];
                            ++changed.declaration.sourceProvenance->byteOffset;
                            compare(changed, Comparison::Different,
                                "different enum source offset prevents reconciliation");
                            changed = provenanceEnums[0];
                            changed.declaration.sourceProvenance.reset();
                            compare(changed, Comparison::Insufficient,
                                "missing enum source cannot establish reconciliation");
                            changed = provenanceEnums[0];
                            changed.declaration.clangUsr.reset();
                            compare(changed, Comparison::Insufficient,
                                "missing enum USR cannot establish reconciliation");
                            std::vector<CppCRecordSnapshot> dispatchedRecords;
                            std::vector<CppCEnumSnapshot> dispatchedEnums;
                            metadata_detail::CDependencyCatalog dispatchedDependencies;
                            const bool acceptedOld = metadata_detail::decodeCTypeCatalogAndDependencies(provenanceBytes,
                                48, dispatchedRecords, dispatchedEnums, dispatchedDependencies, error);
                            const bool acceptedNew = metadata_detail::decodeCTypeCatalogAndDependencies(provenanceBytes,
                                49, dispatchedRecords, dispatchedEnums, dispatchedDependencies, error);
                            carriersOk &= expect(!acceptedOld && acceptedNew && dispatchedEnums.size() == 1 &&
                                dispatchedEnums[0].declaration.sourceProvenance == enumeration->sourceProvenance,
                                "enum provenance envelope requires metadata 1.49");
                            provenanceBytes.push_back(0);
                            const auto oldCount = provenanceEnums.size();
                            carriersOk &= expect(!metadata_detail::decodeCTypeCatalogWithEnumProvenance(provenanceBytes,
                                provenanceRecords, provenanceEnums, provenanceDependencies, error) &&
                                provenanceEnums.size() == oldCount,
                                "enum provenance envelope rejects trailing bytes without changing output");
                            inputEnums[0].declaration.sourceProvenance->bufferSha256 = "invalid";
                            carriersOk &= expect(!metadata_detail::encodeCTypeCatalogWithEnumProvenance(snapshots,
                                inputEnums, dependencies, provenanceBytes, error),
                                "enum provenance envelope rejects invalid digest");
                        }
                    }
                    else carriersOk &= expect(false, "named enum for provenance is available");
                }
                else carriersOk &= expect(false, "enum snapshot for provenance is available");
                if (encoded)
                {
                    std::vector<CppCRecordSnapshot> dispatched;
                    std::vector<CppCEnumSnapshot> dispatchedEnums;
                    metadata_detail::CDependencyCatalog dispatchedDependencies;
                    const bool oldAccepted = metadata_detail::decodeCTypeCatalogAndDependencies(
                        bytes, 47, dispatched, dispatchedEnums, dispatchedDependencies, error);
                    const bool currentAccepted = metadata_detail::decodeCTypeCatalogAndDependencies(
                        bytes, 48, dispatched, dispatchedEnums, dispatchedDependencies, error);
                    carriersOk &= expect(!oldAccepted && currentAccepted && dispatched.size() == 4,
                        "anonymous carrier envelope requires the newer metadata version");
                }
                if (encoded)
                {
                    auto duplicate = bytes;
                    std::copy_n(duplicate.end() - 24, 8, duplicate.end() - 8);
                    auto untouched = restored;
                    const auto before = untouched.size();
                    carriersOk &= expect(!metadata_detail::decodeCTypeCatalogWithAnonymousCarriers(
                        duplicate, untouched, restoredEnums, restoredDependencies, error) && untouched.size() == before,
                        "anonymous carrier envelope rejects duplicate indices without changing output");
                    auto outside = bytes;
                    std::fill_n(outside.end() - 8, 4, static_cast<std::uint8_t>(0xff));
                    carriersOk &= expect(!metadata_detail::decodeCTypeCatalogWithAnonymousCarriers(
                        outside, untouched, restoredEnums, restoredDependencies, error) && untouched.size() == before,
                        "anonymous carrier envelope rejects out-of-range record indices");
                    bytes.push_back(0);
                    restored.clear();
                    carriersOk &= expect(!metadata_detail::decodeCTypeCatalogWithAnonymousCarriers(
                        bytes, restored, restoredEnums, restoredDependencies, error) && restored.empty(),
                        "anonymous carrier envelope rejects trailing bytes without publishing output");
                }
            }
            auto conflicting = *record;
            conflicting.fields[0].name = "__nsc_c_anonymous_1";
            carriersOk &= expect(std::holds_alternative<std::string>(materializeCppAnonymousCRecords(conflicting)),
                "anonymous carrier names do not overwrite a source field");
        }
    }
    const auto againTable = collectUsrs(*again);
    const auto renamedTable = collectUsrs(*renamed);
    printTables(firstTable, againTable, renamedTable);
    first->close();
    again->close();
    renamed->close();
    return checkUsrTables(firstTable, againTable, renamedTable) && carriersOk;
}

// EN: Writes a minimal NativeSharp project and its Main.ns beneath `directory`.
//     Returns the absolute project path, or an empty path on I/O failure.
// ES: Escribe un proyecto NativeSharp mínimo y su Main.ns bajo `directory`.
//     Devuelve la ruta absoluta del proyecto, o una ruta vacía si falla la E/S.
std::filesystem::path writeProject(const std::filesystem::path& directory, const std::string& name,
                                   const std::string& mainSource)
{
    std::filesystem::create_directories(directory);
    const auto project = std::filesystem::absolute(directory / (name + ".nsproj"));
    const bool written = writeText(project, "<Project><PropertyGroup><Name>" + name +
                                   "</Name></PropertyGroup></Project>\n") &&
        writeText(directory / "Main.ns", mainSource);
    return written ? project : std::filesystem::path();
}

// EN: Opens the `first` and `second` header sessions as two references of one project.
// ES: Abre las sesiones de los encabezados `first` y `second` como dos referencias de un proyecto.
std::vector<CppProjectSessions> twoHeaderSessions(CppClangSessionFactory& factory, const std::string& resource,
                                                  const std::filesystem::path& directory, const std::string& driver,
                                                  const std::filesystem::path& project, const std::string& first,
                                                  const std::string& second)
{
    CppProjectSessions group;
    group.projectFile = project;
    group.sessions.push_back(openSession(factory, headerOptions(resource, directory, driver, first)));
    group.sessions.push_back(openSession(factory, headerOptions(resource, directory, driver, second)));
    std::vector<CppProjectSessions> groups;
    groups.push_back(std::move(group));
    return groups;
}

// EN: Function and global selection when a.h and b.h declare the same external entities.
// ES: Selección de funciones y globales cuando a.h y b.h declaran las mismas entidades externas.
bool checkExternalSelection(const nativesharp::frontend::ProjectCompilation& compilation)
{
    bool ok = true;
    const auto* graph = compilation.projectGraph();
    if (!expect(graph != nullptr, "two-header project graph is available")) return false;
    auto functions = collectCppSemanticFunctions(*graph, compilation.cppProjectSessions());
    const auto* entries = std::get_if<std::vector<CppSemanticInventoryEntry>>(&functions);
    ok &= expect(entries != nullptr, "two-header function inventory is collected");
    if (entries)
    {
        auto selected = selectCppSemanticFunctions(*entries, 0, "cid_function");
        const auto* groups = std::get_if<CppSemanticFunctionGroups>(&selected);
        ok &= expect(groups && groups->groups.size() == 1 && groups->groups.front().inventoryIndices.size() == 2 &&
                     groups->groups.front().symbol.cppClangUsr,
                     "one external C function declared by two headers forms one group with its USR");
        if (groups && groups->groups.size() == 1 && !groups->groups.front().inventoryIndices.empty())
        {
            const auto& group = groups->groups.front();
            const auto& entry = entries->at(group.inventoryIndices.front());
            metadata_detail::CDependencyFunctionSnapshot captured;
            std::string why;
            ok &= expect(metadata_detail::captureCDependencyFunction(*entries, group.symbol, entry.referenceIndex,
                "reference-a", captured, why) && captured.declaration.clangUsr == group.symbol.cppClangUsr &&
                captured.linkSymbol == group.symbol.mangledName,
                "selected real Clang C function is captured with its symbol and USR");
            if (!why.empty()) std::cerr << "real function capture: " << why << '\n';
            std::vector<std::uint8_t> bytes;
            metadata_detail::CDependencyFunctionSnapshot restored;
            const bool decoded = metadata_detail::encodeCDependencyFunction(captured, bytes, why) &&
                metadata_detail::decodeCDependencyFunction(bytes, restored, why);
            ok &= expect(decoded && compilation.nameLookup(), "real C function snapshot can be decoded");
            if (decoded && compilation.nameLookup())
            {
                auto projected = projectCppPrecompiledCFunction(restored, {}, 0, group.symbol.moduleName,
                    entry.targetTriple, *compilation.nameLookup());
                const auto* symbol = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(&projected);
                ok &= expect(symbol && symbol->cppCanonicalIdentity == group.symbol.cppCanonicalIdentity &&
                    symbol->mangledName == group.symbol.mangledName && symbol->cppClangUsr == group.symbol.cppClangUsr,
                    "restored C function projects to the same ABI identity as the live declaration");
                const auto invalid = projectCppPrecompiledCFunction(restored, {}, 0, "wrong-owner",
                    entry.targetTriple, *compilation.nameLookup());
                ok &= expect(std::holds_alternative<CppSemanticFunctionFailure>(invalid),
                    "restored C function rejects a different owner");
            }
        }
        auto inlineSelected = selectCppSemanticFunctions(*entries, 0, "cid_inline");
        const auto* inlineGroups = std::get_if<CppSemanticFunctionGroups>(&inlineSelected);
        std::cout << "OBSERVATION static inline cid_inline from a.h and b.h: "
                  << (inlineGroups ? std::to_string(inlineGroups->groups.size()) + " group(s)"
                                   : "selection failure: " +
                                     std::get<CppSemanticFunctionGroupFailure>(inlineSelected).message) << '\n';
        ok &= expect(inlineGroups && inlineGroups->groups.size() == 2,
                     "two C static inline definitions retain distinct groups");
        if (inlineGroups)
            for (const auto& group : inlineGroups->groups)
            {
                if (group.inventoryIndices.empty()) { ok = false; continue; }
                const auto& entry = entries->at(group.inventoryIndices.front());
                metadata_detail::CDependencyFunctionSnapshot captured, restored;
                std::vector<std::uint8_t> encoded;
                std::string why;
                const bool capturedInline = metadata_detail::captureCDependencyFunction(*entries, group.symbol,
                    entry.referenceIndex, "inline-reference-" + std::to_string(entry.referenceIndex),
                    captured, why) && metadata_detail::encodeCDependencyFunction(captured, encoded, why) &&
                    metadata_detail::decodeCDependencyFunction(encoded, restored, why) &&
                    restored.declaration.isCStaticInlineDefinition &&
                    restored.declaration.languageLinkage == entry.declaration.languageLinkage &&
                    restored.declaration.clangUsr == group.symbol.cppClangUsr &&
                    restored.linkSymbol == group.symbol.mangledName;
                ok &= expect(capturedInline, "C static inline dependency retains its own symbol and USR");
                if (!why.empty()) std::cerr << "static inline capture: " << why
                    << " linkage=" << static_cast<int>(entry.declaration.languageLinkage)
                    << " inline=" << entry.declaration.isCStaticInlineDefinition << '\n';
            }
    }
    auto globals = collectCppSemanticGlobals(*graph, compilation.cppProjectSessions());
    const auto* globalEntries = std::get_if<std::vector<CppSemanticGlobalEntry>>(&globals);
    ok &= expect(globalEntries != nullptr, "two-header global inventory is collected");
    if (globalEntries)
    {
        const auto selection = selectCppSemanticGlobal(*globalEntries, 0, "cid_global");
        ok &= expect(selection.candidate && selection.conflict.empty() && selection.candidate->clangUsr,
                     "one external C global declared by two headers is selected with its USR");
        if (selection.candidate && !selection.declarationIndices.empty())
        {
            const auto& candidate = *selection.candidate;
            const auto& entry = globalEntries->at(selection.declarationIndices.front());
            nativesharp::frontend::NamespaceStateSymbol selectedState;
            selectedState.projectOrdinal = candidate.projectOrdinal;
            selectedState.moduleName = candidate.moduleName;
            selectedState.name = candidate.name;
            selectedState.storageName = candidate.externalSymbol;
            selectedState.isCppExternal = true;
            selectedState.cppCGlobal = true;
            selectedState.cppThreadLocal = candidate.threadLocal;
            selectedState.cppClangUsr = candidate.clangUsr;
            selectedState.cppMicrosoftVolatileSemantics = entry.microsoftVolatileSemantics;
            metadata_detail::CDependencyGlobalSnapshot captured;
            std::string why;
            ok &= expect(metadata_detail::captureCDependencyGlobal(*globalEntries, selectedState, entry.referenceIndex,
                "reference-a", captured, why) && captured.linkSymbol == candidate.externalSymbol &&
                captured.declaration.clangUsr == candidate.clangUsr,
                "selected real Clang C global is captured with its symbol and USR");
            if (!why.empty()) std::cerr << "real global capture: " << why << '\n';
            std::vector<std::uint8_t> bytes;
            metadata_detail::CDependencyGlobalSnapshot restored;
            const bool decoded = metadata_detail::encodeCDependencyGlobal(captured, bytes, why) &&
                metadata_detail::decodeCDependencyGlobal(bytes, restored, why);
            ok &= expect(decoded && compilation.nameLookup(), "real C global snapshot can be decoded");
            if (decoded && compilation.nameLookup())
            {
                auto projected = projectCppPrecompiledCGlobal(restored, candidate.projectOrdinal,
                    candidate.moduleName, entry.targetTriple, *compilation.nameLookup());
                const auto* value = std::get_if<CppSemanticGlobal>(&projected);
                ok &= expect(value && value->externalSymbol == candidate.externalSymbol &&
                    value->typeKey == candidate.typeKey && value->clangUsr == candidate.clangUsr &&
                    value->isVolatile == candidate.isVolatile && value->threadLocal == candidate.threadLocal,
                    "restored C global projects to the live symbol, type and qualifiers");
                auto tlsDll = restored;
                tlsDll.declaration.threadLocal = true;
                tlsDll.declaration.dllImport = false;
                tlsDll.effectiveDllImport = true;
                auto projectedTls = projectCppPrecompiledCGlobal(tlsDll, candidate.projectOrdinal,
                    candidate.moduleName, entry.targetTriple, *compilation.nameLookup());
                const auto* tlsValue = std::get_if<CppSemanticGlobal>(&projectedTls);
                ok &= expect(tlsValue && tlsValue->threadLocal && tlsValue->dllImport,
                    "restored TLS global preserves effective DLL import without a declaration attribute");
                const auto invalid = projectCppPrecompiledCGlobal(restored, candidate.projectOrdinal,
                    "wrong-owner", entry.targetTriple, *compilation.nameLookup());
                ok &= expect(std::holds_alternative<CppSemanticGlobalFailure>(invalid),
                    "restored C global rejects a different owner");
            }
        }
        const auto internal = selectCppSemanticGlobal(*globalEntries, 0, "cid_static_global");
        std::cout << "OBSERVATION static cid_static_global from a.h and b.h: candidate="
                  << (internal.candidate ? "yes" : "no") << " conflict='" << internal.conflict << "'\n";
    }
    return ok;
}

// EN: Record and enum identity when a.h and b.h contain the same tagged definitions.
// ES: Identidad de records y enums cuando a.h y b.h contienen las mismas definiciones con etiqueta.
bool checkTaggedTypes(const nativesharp::frontend::ProjectCompilation& compilation)
{
    bool ok = true;
    const auto* graph = compilation.projectGraph();
    if (!graph) return false;
    auto inventory = collectCppSemanticRecords(*graph, compilation.cppProjectSessions());
    const auto* records = std::get_if<std::vector<CppSemanticRecordEntry>>(&inventory);
    ok &= expect(records != nullptr, "two-header record inventory is collected");
    if (records)
    {
        const auto grouped = groupCppSemanticRecords(*records);
        const auto group = std::find_if(grouped.groups.begin(), grouped.groups.end(), [](const auto& item) {
            return item.identity.name() == "CIdRecord";
        });
        ok &= expect(group != grouped.groups.end() && group->inventoryIndices.size() == 2 &&
                     group->status != CppRecordGroupStatus::Conflict && group->clangUsr,
                     "one tagged C struct defined by two headers keeps one group and USR");
    }
    std::vector<nativesharp::frontend::CppImportedTypeDeclaration> types;
    std::string error;
    const bool provided = provideCppEnumTypes(*graph, compilation.cppProjectSessions(), types, error);
    const auto kind = std::find_if(types.begin(), types.end(), [](const auto& type) {
        return type.identity.name() == "CIdKind";
    });
    if (!provided) std::cerr << "two-header enum provider: " << error << '\n';
    ok &= expect(provided && kind != types.end() && kind->clangUsr,
                 "one tagged C enum defined by two headers is provided with its USR");
    return ok;
}

// EN: Probe 2. Installs a.h and b.h in one project and checks external and tagged selection.
// ES: Sonda 2. Instala a.h y b.h en un proyecto y comprueba la selección externa y con etiqueta.
bool checkTwoHeaderProject(CppClangSessionFactory& factory, const std::string& resource,
                           const std::filesystem::path& directory, const std::string& driver)
{
    const auto project = writeProject(directory / "two-headers", "CIdentityTwoHeaders", "int Main() { return 0; }\n");
    if (!expect(!project.empty(), "two-header project is written")) return false;
    nativesharp::frontend::ProjectCompilation compilation;
    std::string error;
    const bool loaded = compilation.setCppProjectSessions(
                            twoHeaderSessions(factory, resource, directory, driver, project, "a.h", "b.h"), error) &&
        compilation.compileProjectNames(project, error);
    if (!loaded) std::cerr << "two-header project: " << error << '\n';
    if (!expect(loaded, "two-header project loads its names")) return false;
    const bool external = checkExternalSelection(compilation);
    const bool tagged = checkTaggedTypes(compilation);
    const auto* graph = compilation.projectGraph();
    auto observations = collectCppSemanticTypeAliases(*graph, compilation.cppProjectSessions());
    const auto* aliases = std::get_if<std::vector<CppSemanticTypeAliasEntry>>(&observations);
    std::vector<nativesharp::frontend::CppImportedTypeAlias> provided;
    bool aliasCaptured = aliases && provideCppTypeAliases(*graph, compilation.cppProjectSessions(), provided, error);
    if (aliasCaptured)
    {
        const auto selected = std::find_if(provided.begin(), provided.end(), [](const auto& item) {
            return item.referenceIndex == 0 && item.declaration.name == "CIdInt";
        });
        metadata_detail::CDependencyAliasSnapshot captured;
        aliasCaptured = selected != provided.end() && metadata_detail::captureCDependencyAlias(
            *aliases, *selected, graph->projects()[selected->projectOrdinal].moduleName(),
            "reference-a", captured, error) && captured.declaration.clangUsr == selected->declaration.clangUsr;
        if (aliasCaptured)
        {
            std::vector<std::uint8_t> bytes;
            metadata_detail::CDependencyAliasSnapshot decoded;
            nativesharp::frontend::CppImportedTypeAlias restored{};
            aliasCaptured = metadata_detail::encodeCDependencyAlias(captured, bytes, error) &&
                metadata_detail::decodeCDependencyAlias(bytes, decoded, error) &&
                reconstructCppPrecompiledCAlias(decoded, *graph, selected->projectOrdinal,
                    selected->referenceIndex, selected->targetTriple, restored, error) &&
                restored.declaration.clangUsr == selected->declaration.clangUsr &&
                restored.declaration.underlyingType.canonicalSpelling ==
                    selected->declaration.underlyingType.canonicalSpelling;
            if (aliasCaptured)
            {
                metadata_detail::CDependencyCatalog dependencies;
                dependencies.aliases.push_back(decoded);
                nativesharp::frontend::PrecompiledSyntaxModule module;
                module.moduleName = decoded.moduleName;
                module.cppCTypeCatalogFormatMinor = 46;
                std::vector<nativesharp::frontend::CppImportedTypeAlias> fromMetadata;
                aliasCaptured = metadata_detail::encodeCTypeCatalogWithDependencies({}, {}, dependencies,
                    module.cppCTypeCatalog, error) &&
                    provideCppTypeAliasesWithPrecompiledCatalogs(*graph, {}, std::span(&module, 1),
                        fromMetadata, error) && fromMetadata.size() == 1 &&
                    fromMetadata[0].declaration.clangUsr == decoded.declaration.clangUsr &&
                    fromMetadata[0].projectOrdinal == selected->projectOrdinal;
                if (!aliasCaptured) std::cerr << "precompiled alias provider: " << error << '\n';
            }
            if (aliasCaptured)
            {
                const auto previous = restored.declaration.name;
                aliasCaptured = !reconstructCppPrecompiledCAlias(decoded, *graph, graph->projects().size(),
                    selected->referenceIndex, selected->targetTriple, restored, error) &&
                    restored.declaration.name == previous;
            }
        }
    }
    if (!aliasCaptured) std::cerr << "real alias capture: " << error << '\n';
    return external && tagged && expect(aliasCaptured, "real Clang C alias observation is captured with its USR");
}

// EN: Prints every diagnostic message of `compilation` on one evidence line.
// ES: Imprime todos los mensajes de diagnóstico de `compilation` en una línea de evidencia.
std::string diagnosticText(const nativesharp::frontend::ProjectCompilation& compilation)
{
    std::string text;
    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
    {
        text += "[" + diagnostic.message() + "]";
    }
    return text;
}

// EN: Binds the typedef name `alias` declared by two sessions whose headers are `first` and
//     `second`. `label` names the project folder. Returns true when the project names compile
//     without errors; the diagnostics are printed as evidence.
// ES: Enlaza el typedef `alias` declarado por dos sesiones cuyos encabezados son `first` y
//     `second`. `label` nombra la carpeta del proyecto. Devuelve true si los nombres del proyecto
//     compilan sin errores; los diagnósticos se imprimen como evidencia.
bool bindTwoSessionAlias(CppClangSessionFactory& factory, const std::string& resource,
                         const std::filesystem::path& directory, const std::string& driver,
                         const std::string& label, const std::string& first, const std::string& second,
                         const std::string& alias)
{
    const auto project = writeProject(directory / ("alias-" + label), "CIdentityAlias",
                                      "int Use(" + alias + " value) { return 0; } int Main() { return 0; }\n");
    nativesharp::frontend::ProjectCompilation compilation;
    std::string error;
    const bool configured = !project.empty() &&
        compilation.setCppProjectSessions(
            twoHeaderSessions(factory, resource, directory, driver, project, first, second), error) &&
        compilation.setCppTypeAliasProvider(provideCppTypeAliases, error);
    const bool bound = configured && compilation.compileProjectNames(project, error) &&
        !compilation.diagnostics().hasErrors();
    const auto* lookup = compilation.nameLookup();
    const bool observedAlias = bound && lookup && std::any_of(lookup->bindings().begin(), lookup->bindings().end(),
        [&](const auto& binding) {
            return binding.writtenName == alias && binding.expandedCppAlias &&
                binding.expandedCppAlias->declaration.name == alias &&
                binding.expandedCppAlias->declaration.clangUsr.has_value();
        });
    std::cout << "OBSERVATION typedef " << alias << " from " << first << " and " << second << ": bound="
              << (bound ? "yes" : "no") << " diagnostics=" << diagnosticText(compilation)
              << " error='" << error << "'\n";
    return bound && expect(observedAlias, "resolved C typedef retains the selected declaration");
}

// EN: Probe 3. The same typedef seen twice through one header file name must bind (control).
//     The same text in two header file names is only observed: its USRs differ (c:a.h@T@ vs
//     c:b.h@T@) and the outcome is a pending decision recorded in the T08-002 audit.
// ES: Sonda 3. El mismo typedef visto dos veces con un solo nombre de encabezado debe enlazar
//     (control). El mismo texto en dos nombres de encabezado solo se observa: sus USR difieren
//     (c:a.h@T@ frente a c:b.h@T@) y el resultado es una decisión pendiente de la auditoría T08-002.
bool checkTwoSessionAlias(CppClangSessionFactory& factory, const std::string& resource,
                          const std::filesystem::path& directory, const std::string& driver)
{
    const bool control = bindTwoSessionAlias(factory, resource, directory, driver, "a-a", "a.h", "a.h", "CIdInt");
    const bool distinctHeaders = bindTwoSessionAlias(factory, resource, directory, driver, "a-b", "a.h", "b.h", "CIdInt");
    return expect(control, "one typedef observed by two sessions of the same header binds") &&
        expect(distinctHeaders, "equivalent C typedefs from different headers bind");
}

// EN: Probe 4 (observation). Two anonymous enums from c.h and d.h share the first enumerator
//     name and therefore the USR c:@Ea@CIdShared@CIdShared, but have different values. Prints
//     how NSC resolves the demanded name; nothing is asserted.
// ES: Sonda 4 (observación). Dos enums anónimos de c.h y d.h comparten el nombre del primer
//     enumerador y por tanto el USR c:@Ea@CIdShared@CIdShared, pero tienen valores distintos.
//     Imprime cómo resuelve NSC el nombre demandado; no se afirma nada.
void observeAnonymousEnumCollision(CppClangSessionFactory& factory, const std::string& resource,
                                   const std::filesystem::path& directory, const std::string& driver)
{
    if (!writeText(directory / "c.h", "enum { CIdShared = 1 };\n") ||
        !writeText(directory / "d.h", "enum { CIdShared = 2 };\n")) return;
    const auto project = writeProject(directory / "anonymous-enum", "CIdentityAnonymousEnum",
                                      "int Main() { if (CIdShared == 1) return 0; return 1; }\n");
    CppProjectSessions group;
    group.projectFile = project;
    group.sessions.push_back(openSession(factory, headerOptions(resource, directory, driver, "c.h")));
    group.sessions.push_back(openSession(factory, headerOptions(resource, directory, driver, "d.h")));
    std::vector<CppProjectSessions> groups;
    groups.push_back(std::move(group));
    nativesharp::frontend::ProjectCompilation compilation;
    std::string error;
    const bool configured = !project.empty() && compilation.setCppProjectSessions(std::move(groups), error) &&
        compilation.setCppGlobalProvider(
            [&compilation](const auto& graph, auto projects, auto demands, auto& states, auto& why) {
                const auto* lookup = compilation.nameLookup();
                if (!lookup) { why = "name lookup is unavailable"; return false; }
                return provideCppSemanticValuesWithLookup(graph, projects, *lookup, demands, states, why);
            }, error);
    const bool members = configured && compilation.compileProjectMembers(project, error);
    std::string values;
    if (members && compilation.namespaceFunctionModel())
    {
        for (const auto& state : compilation.namespaceFunctionModel()->states())
        {
            if (state.name != "CIdShared") continue;
            values += "[value=" + std::to_string(state.integerValue) + " importError='" + state.cppImportError + "']";
        }
    }
    std::cout << "OBSERVATION anonymous CIdShared from c.h (1) and d.h (2): members=" << (members ? "yes" : "no")
              << " states=" << values << " diagnostics=" << diagnosticText(compilation)
              << " error='" << error << "'\n";
}

// EN: Reads the USRs of the homonymous-header declarations (typedef, static inline function and
//     static global) of one session into a table.
// ES: Lee en una tabla los USR de las declaraciones del encabezado homónimo (typedef, función
//     static inline y global static) de una sesión.
UsrTable collectHomonymUsrs(const CppInteropSession& session)
{
    UsrTable table;
    if (const auto aliases = session.typeAliasesSnapshot())
    {
        for (const auto& alias : *aliases)
        {
            if (alias.qualifiedName == "CSameInt") table["alias"] = alias.clangUsr;
        }
    }
    if (const auto functions = session.freeFunctionsSnapshot())
    {
        for (const auto& function : *functions)
        {
            if (function.qualifiedName == "c_same_inline") table["inline-function"] = function.clangUsr;
        }
    }
    if (const auto globals = session.globalsSnapshot())
    {
        for (const auto& global : *globals)
        {
            if (global.qualifiedName == "c_same_static") table["static-global"] = global.clangUsr;
        }
    }
    return table;
}

// EN: Probe 5a. dirA/same.h, dirB/same.h (identical) and dirC/same.h (different) share the file
//     name. Clang qualifies the USR of internal declarations with the file name only, so the
//     three headers are expected to produce the same USR for each category.
// ES: Sonda 5a. dirA/same.h, dirB/same.h (idénticos) y dirC/same.h (distinto) comparten el nombre
//     de archivo. Clang califica el USR de las declaraciones internas solo con el nombre del
//     archivo, por lo que se espera el mismo USR en los tres encabezados para cada categoría.
bool checkHomonymUsrs(CppClangSessionFactory& factory, const std::string& resource,
                      const std::filesystem::path& directory, const std::string& driver)
{
    auto first = openSession(factory, headerOptions(resource, directory, driver, "dirA/same.h"));
    auto twin = openSession(factory, headerOptions(resource, directory, driver, "dirB/same.h"));
    auto other = openSession(factory, headerOptions(resource, directory, driver, "dirC/same.h"));
    if (!expect(first && twin && other, "homonymous header sessions open")) return false;
    const auto firstTable = collectHomonymUsrs(*first);
    const auto twinTable = collectHomonymUsrs(*twin);
    const auto otherTable = collectHomonymUsrs(*other);
    bool ok = true;
    for (const auto& key : kHeaderDependent)
    {
        std::cout << "USR homonym " << key << " dirA=" << show(firstTable, key) << " dirB=" << show(twinTable, key)
                  << " dirC=" << show(otherTable, key) << '\n';
        const auto found = firstTable.find(key);
        const bool present = found != firstTable.end() && found->second && !found->second->empty();
        ok &= expect(present && twinTable.count(key) && otherTable.count(key) &&
                     twinTable.at(key) == found->second && otherTable.at(key) == found->second,
                     "homonymous headers in different directories give one USR for " + key);
    }
    first->close();
    twin->close();
    other->close();
    return ok;
}

// EN: Probe 5b. Selection with homonymous headers in one project. A typedef of the same type in
//     dirA and dirB binds; a typedef of another type in dirC is rejected by its type; the static
//     inline functions of dirA and dirC share their USR but remain two distinct definitions.
// ES: Sonda 5b. Selección con encabezados homónimos en un proyecto. Un typedef del mismo tipo en
//     dirA y dirB enlaza; un typedef de otro tipo en dirC se rechaza por su tipo; las funciones
//     static inline de dirA y dirC comparten USR pero siguen siendo dos definiciones distintas.
bool checkHomonymProject(CppClangSessionFactory& factory, const std::string& resource,
                         const std::filesystem::path& directory, const std::string& driver)
{
    bool ok = expect(bindTwoSessionAlias(factory, resource, directory, driver, "same-a-b", "dirA/same.h",
                                         "dirB/same.h", "CSameInt"),
                     "one typedef type from homonymous headers in two directories binds");
    ok &= expect(!bindTwoSessionAlias(factory, resource, directory, driver, "same-a-c", "dirA/same.h",
                                      "dirC/same.h", "CSameInt"),
                 "typedefs of different types from homonymous headers do not bind");
    const auto project = writeProject(directory / "homonym-functions", "CIdentityHomonym",
                                      "int Main() { return 0; }\n");
    nativesharp::frontend::ProjectCompilation compilation;
    std::string error;
    const bool loaded = !project.empty() &&
        compilation.setCppProjectSessions(
            twoHeaderSessions(factory, resource, directory, driver, project, "dirA/same.h", "dirC/same.h"), error) &&
        compilation.compileProjectNames(project, error);
    const auto* graph = compilation.projectGraph();
    auto inventory = loaded && graph ? collectCppSemanticFunctions(*graph, compilation.cppProjectSessions())
                                     : CppSemanticInventoryResult{CppSemanticInventoryFailure{{}, 0, error}};
    const auto* entries = std::get_if<std::vector<CppSemanticInventoryEntry>>(&inventory);
    auto selected = entries ? selectCppSemanticFunctions(*entries, 0, "c_same_inline")
                            : CppSemanticFunctionGroupsResult{CppSemanticFunctionGroupFailure{0, 0, "no inventory"}};
    const auto* groups = std::get_if<CppSemanticFunctionGroups>(&selected);
    ok &= expect(groups && groups->groups.size() == 2,
                 "static inline definitions of homonymous headers with one USR are not merged");
    return ok;
}

// EN: Probe 5. Writes the homonymous headers and runs 5a and 5b.
// ES: Sonda 5. Escribe los encabezados homónimos y ejecuta 5a y 5b.
bool checkHomonymHeaders(CppClangSessionFactory& factory, const std::string& resource,
                         const std::filesystem::path& directory, const std::string& driver)
{
    std::filesystem::create_directories(directory / "dirA");
    std::filesystem::create_directories(directory / "dirB");
    std::filesystem::create_directories(directory / "dirC");
    const bool written = writeText(directory / "dirA" / "same.h", kHomonymHeader) &&
        writeText(directory / "dirB" / "same.h", kHomonymHeader) &&
        writeText(directory / "dirC" / "same.h", kHomonymOtherHeader);
    if (!expect(written, "homonymous headers are written")) return false;
    const bool usrs = checkHomonymUsrs(factory, resource, directory, driver);
    const bool project = checkHomonymProject(factory, resource, directory, driver);
    return usrs && project;
}

// EN: Header of probe 6, the same declarations as the fixture enum-campo/Nested.h without the
//     functions: an anonymous enum and a named enum, each declared inside the field that uses it.
// ES: Encabezado de la sonda 6, las mismas declaraciones que el fixture enum-campo/Nested.h sin
//     las funciones: un enum anónimo y un enum con nombre, cada uno declarado dentro del campo que
//     lo usa.
const char* const kNestedEnumHeader =
    "typedef struct { int raw; enum { CNestLow = 1, CNestHigh = 2 } level; } CNestAnonField;\n"
    "typedef struct { int raw; enum CNestNamed { CNestNamedA = 3, CNestNamedB = 4 } kind; }"
    " CNestNamedField;\n";

// EN: Identity of the enum type of one record field, as captured by the session.
// ES: Identidad del tipo enum de un campo de record, tal como la captura la sesión.
struct NestedFieldEnum
{
    std::optional<std::string> enumUsr;
    std::optional<std::string> fieldUsr;
    std::string name;
    std::string qualifiedName;
};

// EN: Reads and prints the enum identity Clang gives to field `fieldName` of record `recordName`.
//     Returns nothing when the field or its enum identity is missing.
// ES: Lee e imprime la identidad de enum que Clang da al campo `fieldName` del record `recordName`.
//     Devuelve nada si faltan el campo o su identidad de enum.
std::optional<NestedFieldEnum> readNestedFieldEnum(const CppInteropSession& session, const std::string& recordName,
                                                   const std::string& fieldName)
{
    const auto records = session.recordsSnapshot();
    if (!records) return std::nullopt;
    for (const auto& record : *records)
    {
        if (record.qualifiedName != recordName) continue;
        for (const auto& field : record.fields)
        {
            if (field.name != fieldName || field.type.kind != CppTypeKind::Enumeration || !field.type.enumIdentity)
                continue;
            const auto& identity = *field.type.enumIdentity;
            std::cout << "OBSERVATION nested enum " << recordName << '.' << fieldName << ": name='" << identity.name
                      << "' qualified='" << identity.qualifiedName << "' anonymousContext="
                      << (identity.anonymousContext ? "yes" : "no") << " usr=" << identity.clangUsr.value_or("<absent>")
                      << " fieldUsr=" << field.clangUsr.value_or("<absent>") << " underlying="
                      << (field.type.enumUnderlying ? field.type.enumUnderlying->canonicalSpelling : "<absent>")
                      << '\n';
            return NestedFieldEnum{identity.clangUsr, field.clangUsr, identity.name, identity.qualifiedName};
        }
    }
    return std::nullopt;
}

// EN: True when the enum snapshot of `session` lists an enumerator named `enumerator`.
// ES: True si el snapshot de enums de `session` contiene un enumerador llamado `enumerator`.
bool listsEnumerator(const CppInteropSession& session, const std::string& enumerator)
{
    const auto enums = session.enumsSnapshot();
    if (!enums) return false;
    return std::any_of(enums->begin(), enums->end(), [&](const auto& enumeration) {
        return std::any_of(enumeration.enumerators.begin(), enumeration.enumerators.end(),
                           [&](const auto& item) { return item.name == enumerator; });
    });
}

// EN: Probe 6. Enums declared inside a C record field (finding H4), read from nested.h twice and
//     from nested-copy.h, which has the same text. Observed with Clang 23.1.0 and asserted here:
//     - the named enum has the file-scope USR c:@E@<name> and a qualified name equal to its name;
//     - the anonymous enum has the USR c:@SA@<record>@E@<file>@<offset>, which depends on the header
//       file name, and a qualified name that embeds the absolute header path; only the field USR
//       c:@SA@<record>@FI@<field> is independent of the header.
//     Prints, without asserting, whether the enum snapshot lists the nested enumerators.
// ES: Sonda 6. Enums declarados dentro de un campo de record C (hallazgo H4), leídos dos veces de
//     nested.h y de nested-copy.h, con el mismo texto. Observado con Clang 23.1.0 y afirmado aquí:
//     - el enum con nombre tiene el USR de archivo c:@E@<nombre> y nombre calificado igual al nombre;
//     - el enum anónimo tiene el USR c:@SA@<record>@E@<archivo>@<desplazamiento>, que depende del
//       nombre del encabezado, y un nombre calificado que contiene la ruta absoluta del encabezado;
//       solo el USR del campo c:@SA@<record>@FI@<campo> es independiente del encabezado.
//     Imprime, sin afirmarlo, si el snapshot de enums contiene los enumeradores anidados.
bool checkNestedFieldEnums(CppClangSessionFactory& factory, const std::string& resource,
                           const std::filesystem::path& directory, const std::string& driver)
{
    if (!expect(writeText(directory / "nested.h", kNestedEnumHeader) &&
                writeText(directory / "nested-copy.h", kNestedEnumHeader), "nested enum headers are written"))
        return false;
    auto first = openSession(factory, headerOptions(resource, directory, driver, "nested.h"));
    auto again = openSession(factory, headerOptions(resource, directory, driver, "nested.h"));
    auto renamed = openSession(factory, headerOptions(resource, directory, driver, "nested-copy.h"));
    if (!expect(first && again && renamed, "nested enum sessions open")) return false;
    const auto anonymous = readNestedFieldEnum(*first, "CNestAnonField", "level");
    const auto anonymousAgain = readNestedFieldEnum(*again, "CNestAnonField", "level");
    const auto anonymousRenamed = readNestedFieldEnum(*renamed, "CNestAnonField", "level");
    const auto named = readNestedFieldEnum(*first, "CNestNamedField", "kind");
    const auto namedAgain = readNestedFieldEnum(*again, "CNestNamedField", "kind");
    const auto namedRenamed = readNestedFieldEnum(*renamed, "CNestNamedField", "kind");
    bool ok = expect(anonymous && anonymousAgain && anonymousRenamed && named && namedAgain && namedRenamed,
                     "nested enum fields are captured");
    if (!ok) return false;
    ok &= expect(named->enumUsr == std::optional<std::string>("c:@E@CNestNamed") &&
                 namedAgain->enumUsr == named->enumUsr && namedRenamed->enumUsr == named->enumUsr &&
                 named->qualifiedName == "CNestNamed",
                 "named nested enum has the header-independent USR c:@E@CNestNamed and file-scope name");
    ok &= expect(anonymous->enumUsr && anonymous->enumUsr->starts_with("c:@SA@CNestAnonField@E@nested.h@") &&
                 anonymousAgain->enumUsr == anonymous->enumUsr && anonymousRenamed->enumUsr &&
                 anonymousRenamed->enumUsr->starts_with("c:@SA@CNestAnonField@E@nested-copy.h@"),
                 "anonymous nested enum USR is stable per header and depends on the header file name");
    ok &= expect(anonymous->name.empty() &&
                 anonymous->qualifiedName.find(directory.filename().string()) != std::string::npos,
                 "anonymous nested enum qualified name embeds the header path");
    ok &= expect(anonymous->fieldUsr == std::optional<std::string>("c:@SA@CNestAnonField@FI@level") &&
                 anonymousAgain->fieldUsr == anonymous->fieldUsr && anonymousRenamed->fieldUsr == anonymous->fieldUsr,
                 "owning field USR of the anonymous enum is header-independent");
    for (const auto* name : {"CNestLow", "CNestNamedA"})
    {
        std::cout << "OBSERVATION enum snapshot lists nested enumerator " << name << ": "
                  << (listsEnumerator(*first, name) ? "yes" : "no") << '\n';
    }
    first->close();
    again->close();
    renamed->close();
    return ok;
}

// EN: The synthetic C enum of probe 7 and of the minimal H8 fixture: module CIdentityEnvelope,
//     enum CEnvOnly with the single enumerator CEnvOne = 1 of type int.
// ES: El enum C sintético de la sonda 7 y del fixture mínimo de H8: módulo CIdentityEnvelope, enum
//     CEnvOnly con el único enumerador CEnvOne = 1 de tipo int.
CppCEnumSnapshot syntheticEnumSnapshot()
{
    CppType integer;
    integer.kind = CppTypeKind::SignedInteger;
    integer.spelling = integer.canonicalSpelling = integer.unqualifiedCanonicalSpelling = "int";
    integer.sizeBits = 32;
    integer.alignmentBits = 32;
    CppCEnumSnapshot only;
    only.identityKey = nativesharp::compilation::TypeIdentity("CIdentityEnvelope", {}, {}, "CEnvOnly").key();
    only.targetTriple = "x86_64-pc-windows-msvc";
    only.declaration.name = only.declaration.qualifiedName = "CEnvOnly";
    only.declaration.isCxx = false;
    only.declaration.complete = true;
    only.declaration.underlyingType = integer;
    only.declaration.sizeBits = 32;
    only.declaration.alignmentBits = 32;
    CppEnumeratorDeclaration value;
    value.name = "CEnvOne";
    value.decimalValue = "1";
    value.valueBits = 32;
    value.valueType = integer;
    only.declaration.enumerators.push_back(value);
    return only;
}

// EN: Probe 7 (observation). Shows how the reader tells the legacy C type catalogue from the
//     versioned envelope. A legacy catalogue with no records starts with the 64-bit record block
//     size 4, the same first four bytes as envelope 4. Prints the bytes and the decoder result.
// ES: Sonda 7 (observación). Muestra cómo distingue el lector el catálogo C heredado de la
//     envoltura versionada. Un catálogo heredado sin records empieza con el tamaño de 64 bits 4 del
//     bloque de records, los mismos cuatro primeros bytes que la envoltura 4. Imprime los bytes y el
//     resultado del decodificador.
void observeCatalogueEnvelope()
{
    const std::vector<CppCEnumSnapshot> enums{syntheticEnumSnapshot()};
    std::vector<std::uint8_t> legacy;
    std::vector<std::uint8_t> versioned;
    std::string error;
    using namespace nativesharp::interop::metadata_detail;
    const bool encoded = encodeCTypeCatalog({}, enums, legacy, error);
    const bool envelope = encodeCTypeCatalogWithProvenance({}, enums, versioned, error);
    const auto prefix = [](const std::vector<std::uint8_t>& bytes) {
        static const char* const digits = "0123456789abcdef";
        std::string text;
        for (std::size_t i = 0; i < bytes.size() && i < 12; ++i)
        {
            text += digits[bytes[i] >> 4];
            text += digits[bytes[i] & 15];
            text += ' ';
        }
        return text;
    };
    std::vector<CppCRecordSnapshot> records;
    std::vector<CppCEnumSnapshot> decoded;
    std::string detail;
    const bool roundTrip = encoded && decodeVersionedCTypeCatalog(legacy, records, decoded, detail, 42);
    std::cout << "OBSERVATION legacy enum-only C catalogue encoded=" << (encoded ? "yes" : "no")
              << " first bytes=" << prefix(legacy) << "classified-as-envelope="
              << (encoded && isCTypeCatalogWithProvenance(legacy) ? "yes" : "no") << " decoded="
              << (roundTrip ? "yes" : "no") << " detail='" << detail << "'\n";
    std::cout << "OBSERVATION versioned enum-only C catalogue encoded=" << (envelope ? "yes" : "no")
              << " first bytes=" << prefix(versioned) << '\n';
}

bool checkGenericForeignCandidates(CppClangSessionFactory& factory, const std::string& resource,
                                   const std::filesystem::path& directory, const std::string& driver)
{
    const auto project = writeProject(directory / "generic-candidates", "CGenericCandidates",
        "namespace Example { class Bridge { public static int Test<T>(T value) { "
        "CIdInt x = 0; cid_function(x); return cid_global; } "
        "public static int Shadow<T>(T value) { int cid_global = 3; return cid_global; } } }\n");
    nativesharp::frontend::ProjectCompilation compilation;
    std::string error;
    const bool ready = !project.empty() && compilation.setCppProjectSessions(
        twoHeaderSessions(factory, resource, directory, driver, project, "a.h", "b.h"), error) &&
        compilation.setCppTypeAliasProvider(provideCppTypeAliases, error) &&
        compilation.setCppGlobalProvider([](const auto&, auto, auto, auto&, auto&) { return true; }, error) &&
        compilation.compileProjectMembers(project, error);
    if (!ready) std::cerr << "generic foreign candidates: " << error << '\n';
    if (!expect(ready && compilation.genericModel(), "generic candidates fixture compiles")) return false;
    bool call = false, global = false, alias = false, shadow = false;
    for (const auto& record : compilation.genericModel()->templates())
        for (const auto& candidate : record.foreignReferenceCandidates)
        {
            if (candidate.lexicalNamespace != "Example" || !candidate.syntax) continue;
            if (record.name == "Test")
            {
                call |= candidate.syntax->kind() == nativesharp::ast::AstNodeKind::IdentifierExpression &&
                    candidate.syntax->text() == "cid_function";
                global |= candidate.syntax->kind() == nativesharp::ast::AstNodeKind::IdentifierExpression &&
                    candidate.syntax->text() == "cid_global";
                alias |= candidate.typeSyntax && candidate.writtenTypeName == "CIdInt";
            }
            if (record.name == "Shadow")
                shadow |= candidate.syntax->kind() == nativesharp::ast::AstNodeKind::IdentifierExpression &&
                    candidate.syntax->text() == "cid_global";
        }
    bool ok = expect(call && global && alias && shadow,
        "generic candidate capture retains lexical context and does not claim shadowed names are imports");
    nativesharp::frontend::ProjectNamespaceFunctionModel observer;
    nativesharp::diagnostics::DiagnosticEngine observerDiagnostics;
    const bool observed = compilation.nameLookup() && compilation.memberModel() &&
        observer.setCaptureCppFunctionDemands(true) &&
        observer.build(*compilation.nameLookup(), *compilation.memberModel(), compilation.units(),
                       observerDiagnostics, compilation.genericModel());
    if (!expect(observed, "namespace observer builds for generic candidate fixture")) return false;
    bool observedCall = false, observedGlobal = false, observedShadow = false;
    for (const auto& record : compilation.genericModel()->templates())
        for (const auto& candidate : record.foreignReferenceCandidates)
            for (const auto& demand : observer.cppFunctionDemands())
            {
                if (candidate.syntax != demand.syntax || candidate.typeSyntax ||
                    record.projectOrdinal != demand.projectOrdinal) continue;
                if (record.name == "Test" && demand.writtenName == "cid_function" && demand.directCall)
                    observedCall = true;
                if (record.name == "Test" && demand.writtenName == "cid_global" && !demand.directCall)
                    observedGlobal = true;
                if (record.name == "Shadow" && demand.writtenName == "cid_global" && !demand.directCall)
                    observedShadow = true;
            }
    ok &= expect(observedCall && observedGlobal && observedShadow,
        "observer demands include the generic C call, C global and shadowed local");
    bool retainedCall = false, retainedGlobal = false, retainedShadow = false;
    for (const auto& record : compilation.genericModel()->templates())
        for (const auto& retained : compilation.cppTemplateDemands())
        {
            if (retained.templateKey != record.canonicalKey) continue;
            if (record.name == "Test" && retained.demand.writtenName == "cid_function" &&
                retained.demand.directCall) retainedCall = true;
            if (record.name == "Test" && retained.demand.writtenName == "cid_global" &&
                !retained.demand.directCall) retainedGlobal = true;
            if (record.name == "Shadow" && retained.demand.writtenName == "cid_global" &&
                !retained.demand.directCall) retainedShadow = true;
        }
    ok &= expect(retainedCall && retainedGlobal && !retainedShadow,
        "compilation retains C demands and excludes the proven shadowed block local");
    nativesharp::interop::LibraryMetadata published;
    for (const auto& record : compilation.genericModel()->templates())
        if (record.name == "Test")
        {
            nativesharp::interop::LibraryMetadataGenericTemplate item;
            item.canonicalKey = record.canonicalKey;
            published.genericTemplates.push_back(std::move(item));
        }
    auto inventory = collectCppSemanticFunctions(*compilation.projectGraph(), compilation.cppProjectSessions());
    const auto* entries = std::get_if<std::vector<CppSemanticInventoryEntry>>(&inventory);
    if (!expect(entries != nullptr, "generic fixture function inventory is available")) return false;
    const auto candidates = selectCppPublishedTemplateFunctions(compilation, published, *entries);
    bool selectedFunction = false, selectedOtherTemplate = false;
    for (const auto& candidate : candidates)
    {
        selectedOtherTemplate |= candidate.templateKey != published.genericTemplates.front().canonicalKey;
        for (const auto& group : candidate.selection.candidates.groups)
            if (group.symbol.name == "cid_function" && group.inventoryIndices.size() == 2)
                selectedFunction = true;
    }
    ok &= expect(published.genericTemplates.size() == 1 && selectedFunction && !selectedOtherTemplate,
        "published generic function candidates retain both header origins and exclude other templates");
    auto globalInventory = collectCppSemanticGlobals(*compilation.projectGraph(), compilation.cppProjectSessions());
    const auto* globals = std::get_if<std::vector<CppSemanticGlobalEntry>>(&globalInventory);
    if (!expect(globals != nullptr, "generic fixture global inventory is available")) return false;
    const auto globalCandidates = selectCppPublishedTemplateGlobals(compilation, published, *globals);
    bool selectedGlobal = false, globalFromOtherTemplate = false;
    for (const auto& candidate : globalCandidates)
    {
        globalFromOtherTemplate |= candidate.templateKey != published.genericTemplates.front().canonicalKey;
        if (candidate.selection.candidate && candidate.selection.candidate->name == "cid_global" &&
            candidate.selection.declarationIndices.size() == 2) selectedGlobal = true;
    }
    ok &= expect(selectedGlobal && !globalFromOtherTemplate,
        "published generic global candidates retain both header origins and exclude other templates");
    auto aliasInventory = collectCppSemanticTypeAliases(*compilation.projectGraph(), compilation.cppProjectSessions());
    const auto* aliases = std::get_if<std::vector<CppSemanticTypeAliasEntry>>(&aliasInventory);
    if (!expect(aliases != nullptr, "generic fixture alias inventory is available")) return false;
    const auto aliasCandidates = selectCppPublishedTemplateAliases(compilation, published, *aliases);
    bool selectedAlias = false, aliasFromOtherTemplate = false;
    bool bindingMatches = true;
    for (const auto& candidate : aliasCandidates)
    {
        aliasFromOtherTemplate |= candidate.templateKey != published.genericTemplates.front().canonicalKey;
        for (const auto& record : compilation.genericModel()->templates())
            if (record.canonicalKey == candidate.templateKey && candidate.candidateIndex < record.foreignReferenceCandidates.size())
            {
                const auto* syntax = record.foreignReferenceCandidates[candidate.candidateIndex].typeSyntax;
                const auto bindings = compilation.nameLookup()->bindings();
                const auto binding = std::find_if(bindings.begin(), bindings.end(),
                    [&](const auto& item) { return item.syntax == syntax; });
                bindingMatches &= candidate.hasNameBinding == (binding != bindings.end());
                if (binding != bindings.end()) bindingMatches &= candidate.resolvedAlias == binding->expandedCppAlias;
            }
        std::size_t origins = 0;
        for (const auto index : candidate.inventoryIndices)
            if (aliases->at(index).declaration.name == "CIdInt") ++origins;
        if (origins == 2) selectedAlias = true;
    }
    ok &= expect(selectedAlias && !aliasFromOtherTemplate,
        "published generic alias candidates retain both header origins and exclude other templates");
    ok &= expect(bindingMatches, "published generic alias candidates retain actual binding observations");
    if (!aliases->empty())
    {
        const auto& entry = aliases->front();
        nativesharp::frontend::CppImportedTypeAlias resolved{
            entry.projectOrdinal, entry.projectFile, entry.referenceIndex, entry.targetTriple, entry.declaration};
        CppPublishedTemplateAliasCandidates candidate;
        candidate.resolvedAlias = &resolved;
        candidate.hasNameBinding = true;
        std::vector<metadata_detail::CDependencyAliasSnapshot> captured(1);
        captured.front().moduleName = "sentinel";
        std::string why;
        if (entry.referenceIndex >= compilation.projectGraph()->projects()[entry.projectOrdinal].cppLibraryReferences().size())
            ok &= expect(!captureCppTemplateAliasCandidates(*compilation.projectGraph(), std::span(&candidate, 1),
                *aliases, captured, why) && captured.size() == 1 && captured.front().moduleName == "sentinel",
                "resolved template alias rejects missing source reference without changing output");
    }
    return ok;
}

bool checkGenericAliasPublication(CppClangSessionFactory& factory, const std::string& resource,
                                  const std::filesystem::path& directory, const std::string& driver)
{
    const std::string templateSource =
        "namespace Published { public class Bridge { public static CIdInt Test<T>(T value) { return 0; } "
        "public static int Call<T>(T value) { return cid_function(2); } "
        "public static int Global<T>(T value) { return cid_global; } "
        "public static int Record<T>(T value) { CIdRecord record; return 0; } "
        "public static int Packed<T>(T value) { CIdPacked record; return 0; } "
        "public static int Variadic<T>(T value) { return cid_variadic(1, 2); } } }\n";
    const auto project = writeProject(directory / "generic-publish", "CGenericPublish", templateSource);
    const auto header = std::filesystem::absolute(directory / "a.h");
    if (project.empty() || !writeText(project,
        "<Project><PropertyGroup><Name>CGenericPublish</Name></PropertyGroup><ItemGroup>"
        "<CppLibraryReference Header=\"" + header.generic_string() + "\" Language=\"C\" Standard=\"c11\" />"
        "</ItemGroup></Project>\n")) return false;
    auto options = headerOptions(resource, directory, driver, "a.h");
    options.input = CppFileInput{header};
    CppProjectSessions sessions;
    sessions.projectFile = project;
    sessions.sessions.push_back(openSession(factory, std::move(options)));
    if (!sessions.sessions.front()) return false;
    std::vector<CppProjectSessions> groups;
    groups.push_back(std::move(sessions));
    nativesharp::frontend::ProjectCompilation compilation;
    std::string error;
    const bool ready = compilation.setCppProjectSessions(std::move(groups), error) &&
        compilation.setExperimentalCppTypeProvider(provideExperimentalCppRecordTypes, error) &&
        compilation.setCppTypeAliasProvider(provideCppTypeAliases, error) &&
        compilation.setCppNamespaceProvider([](const auto&, auto, auto&, auto&) { return true; }, error) &&
        compilation.setCppFunctionProvider([](const auto&, auto, auto, auto, auto&, auto&, auto&) { return true; }, error) &&
        compilation.compileProjectMembers(project, error);
    if (!ready) std::cerr << "generic publication fixture: " << error << ' ' << diagnosticText(compilation) << '\n';
    if (!expect(ready && compilation.genericModel(), "generic publication fixture compiles with C project reference")) return false;
    LibraryMetadata metadata;
    metadata.formatMinor = 46;
    for (const auto& record : compilation.genericModel()->templates())
        if (record.name == "Test" || record.name == "Call" || record.name == "Global" || record.name == "Record" ||
            record.name == "Packed" || record.name == "Variadic")
        {
            LibraryMetadataGenericTemplate item;
            item.canonicalKey = record.canonicalKey;
            metadata.genericTemplates.push_back(std::move(item));
        }
    const auto functionsResult = collectCppSemanticFunctions(*compilation.projectGraph(), compilation.cppProjectSessions());
    const auto* functions = std::get_if<std::vector<CppSemanticInventoryEntry>>(&functionsResult);
    if (!expect(functions != nullptr, "real C function inventory is available")) return false;
    const auto functionCandidates = selectCppPublishedTemplateFunctions(compilation, metadata, *functions);
    std::vector<metadata_detail::CDependencyFunctionSnapshot> capturedFunctions;
    if (!expect(captureCppTemplateFunctionCandidates(*compilation.projectGraph(), functionCandidates,
            *functions, capturedFunctions, error) && capturedFunctions.size() == 2 &&
            std::any_of(capturedFunctions.begin(), capturedFunctions.end(), [](const auto& value) {
                return value.declaration.name == "cid_function" && value.linkSymbol == "cid_function";
            }),
            "real public template captures the external C function candidate")) return false;
    const auto globalsResult = collectCppSemanticGlobals(*compilation.projectGraph(), compilation.cppProjectSessions());
    const auto* globals = std::get_if<std::vector<CppSemanticGlobalEntry>>(&globalsResult);
    if (!expect(globals != nullptr, "real C global inventory is available")) return false;
    std::vector<metadata_detail::CDependencyGlobalSnapshot> capturedGlobals;
    if (!expect(captureCppTemplateGlobalCandidates(compilation, metadata, *globals, capturedGlobals, error) &&
            capturedGlobals.size() == 1 && capturedGlobals.front().declaration.name == "cid_global" &&
            capturedGlobals.front().linkSymbol == "cid_global",
            "real public template captures the external C global candidate")) return false;
    std::vector<std::uint8_t> bytes;
    std::vector<CppCRecordSnapshot> records;
    std::vector<CppCEnumSnapshot> enums;
    metadata_detail::CDependencyCatalog dependencies;
    const auto observedInventory = collectCppSemanticTypeAliases(*compilation.projectGraph(), compilation.cppProjectSessions());
    if (const auto* observed = std::get_if<std::vector<CppSemanticTypeAliasEntry>>(&observedInventory))
    {
        const auto candidates = selectCppPublishedTemplateAliases(compilation, metadata, *observed);
        for (const auto& item : candidates)
            std::cout << "OBSERVATION generic publication alias candidates=" << item.inventoryIndices.size()
                      << " binding=" << item.hasNameBinding << " selected=" << (item.resolvedAlias != nullptr) << '\n';
    }
    const bool produced = !metadata.genericTemplates.empty() &&
        provideCppCRecordMetadataCatalog(compilation, metadata, bytes, error);
    if (produced && bytes.empty()) error = "producer returned an empty C dependency catalogue";
    const bool captured = produced && !bytes.empty() &&
        metadata_detail::decodeCTypeCatalogAndDependencies(bytes, 46, records, enums, dependencies, error) &&
        dependencies.aliases.size() == 1 && dependencies.aliases[0].declaration.name == "CIdInt";
    if (!captured) std::cerr << "generic alias publication: " << error << '\n';
    if (!expect(captured, "producer retains a template alias candidate from a real C project reference")) return false;
    const auto variadicFunction = std::find_if(dependencies.functions.begin(), dependencies.functions.end(),
        [](const auto& function) { return function.declaration.name == "cid_variadic"; });
    if (!expect(variadicFunction != dependencies.functions.end() && variadicFunction->variadicBasePlan &&
                variadicFunction->variadicBasePlan->symbol == "cid_variadic" &&
                variadicFunction->variadicBasePlan->arguments.size() == 1,
                "producer publishes the Clang plan for the variadic fixed prefix")) return false;
    auto mismatchedVariadic = *variadicFunction;
    auto badBase = *mismatchedVariadic.variadicBasePlan;
    badBase.symbol = "other_symbol";
    mismatchedVariadic.variadicBasePlan = std::make_shared<const abi_plan::VariadicCallPlan>(std::move(badBase));
    std::vector<std::uint8_t> badVariadicBytes;
    if (!expect(!metadata_detail::encodeCDependencyFunction(mismatchedVariadic, badVariadicBytes, error),
                "variadic base plan is rejected when bound to another symbol")) return false;
    std::vector<std::uint8_t> validVariadicBytes;
    if (!metadata_detail::encodeCDependencyFunction(*variadicFunction, validVariadicBytes, error)) return false;
    for (std::size_t length = 0; length < validVariadicBytes.size(); ++length)
    {
        metadata_detail::CDependencyFunctionSnapshot rejected;
        if (!expect(!metadata_detail::decodeCDependencyFunction(
                        std::span(validVariadicBytes).first(length), rejected, error),
                    "truncated variadic dependency is rejected")) return false;
    }
    const auto publishedRecord = std::find_if(records.begin(), records.end(), [](const auto& record) {
        return record.name == "CIdRecord";
    });
    if (!expect(publishedRecord != records.end() && publishedRecord->plainReplaySyntax && publishedRecord->clangUsr,
                "producer publishes the plain C record observation with its Clang identity")) return false;
    const auto packedRecord = std::find_if(records.begin(), records.end(), [](const auto& record) {
        return record.name == "CIdPacked";
    });
    if (!expect(packedRecord != records.end() && !packedRecord->plainReplaySyntax && packedRecord->packedReplaySyntax,
                "producer captures packed syntax without attesting it as plain")) return false;
    auto contradictoryRecord = *packedRecord;
    contradictoryRecord.plainReplaySyntax = true;
    std::vector<std::uint8_t> rejectedRecord;
    if (!expect(!metadata_detail::encodeCRecordCatalog(std::span(&contradictoryRecord, 1), rejectedRecord, error),
                "contradictory plain and packed observations are rejected")) return false;
    auto withoutObservation = records;
    for (auto& record : withoutObservation) { record.plainReplaySyntax = false; record.packedReplaySyntax = false; }
    nativesharp::frontend::PrecompiledSyntaxModule observedModule, unobservedModule;
    observedModule.metadataPath = directory / "observed.nsmeta";
    unobservedModule.metadataPath = directory / "unobserved.nsmeta";
    observedModule.cppCTypeCatalogFormatMinor = unobservedModule.cppCTypeCatalogFormatMinor = 46;
    observedModule.cppCTypeCatalog = bytes;
    if (!metadata_detail::encodeCTypeCatalogWithDependencies(withoutObservation, enums, dependencies,
            unobservedModule.cppCTypeCatalog, error)) return false;
    std::vector<nativesharp::frontend::PrecompiledSyntaxModule> repeatedModules{observedModule, unobservedModule};
    std::vector<nativesharp::frontend::CppImportedTypeDeclaration> restoredTypes;
    const bool repeatedOk = metadata_detail::reconstructPrecompiledCImportedTypes(repeatedModules,
                    *compilation.projectGraph(), "x86_64-pc-windows-msvc", restoredTypes, error);
    if (!repeatedOk || restoredTypes.size() != records.size())
        std::cerr << "repeated catalogues: " << error << " restored=" << restoredTypes.size()
                  << " records=" << records.size() << '\n';
    if (!expect(repeatedOk && restoredTypes.size() == records.size(),
                "different replay observations do not change C type identity")) return false;
    metadata.targetTriple = "x86_64-pc-windows-msvc";
    metadata.cppCRecordCatalog = bytes;
    if (!expect(metadata_detail::verifyBinaryClosureMetadata(metadata, error),
                "extended C catalogue passes metadata verification")) return false;
    auto invalidDependencies = dependencies;
    invalidDependencies.aliases.front().declaration.clangUsr.reset();
    std::vector<std::uint8_t> invalidBytes;
    if (!expect(metadata_detail::encodeCTypeCatalogWithDependencies({}, {}, invalidDependencies, invalidBytes, error),
                "incomplete alias catalogue is constructed for the verifier")) return false;
    metadata.cppCRecordCatalog = invalidBytes;
    if (!expect(!metadata_detail::verifyBinaryClosureMetadata(metadata, error),
                "metadata verification rejects an alias without Clang identity")) return false;

    const auto consumerProject = writeProject(directory / "generic-alias-consumer", "CGenericConsumer",
        "int Main() { return Published.Bridge.Test<int>(1) + Published.Bridge.Call<int>(1); }\n");
    const auto metadataPath = std::filesystem::absolute(directory / "generic-publish" / "synthetic.nsmeta");
    if (consumerProject.empty() || !writeText(metadataPath, "test metadata identity")) return false;
    nativesharp::frontend::PrecompiledSyntaxModule module;
    module.ownerProjectFile = consumerProject;
    module.metadataPath = metadataPath;
    module.moduleName = "CGenericPublish";
    module.templateSourceText = templateSource;
    dependencies.functions = capturedFunctions;
    if (!metadata_detail::encodeCTypeCatalogWithDependencies(records, enums, dependencies, bytes, error)) return false;
    module.cppCTypeCatalog = bytes;
    module.cppCTypeCatalogFormatMinor = 46;
    nativesharp::frontend::ProjectCompilation consumer;
    const bool consumed = consumer.setPrecompiledModules({std::move(module)}, error) &&
        consumer.setCppTypeAliasProvider(
            [&consumer](const auto& graph, auto sessions, auto& aliases, auto& why) {
                return provideCppTypeAliasesWithPrecompiledCatalogs(
                    graph, sessions, consumer.precompiledModules(), aliases, why);
            }, error) &&
        consumer.setCppFunctionProvider(
            [&consumer](const auto& graph, auto sessions, auto, auto demands, auto& functions, auto& deferred, auto& why) {
                const auto* lookup = consumer.nameLookup();
                return lookup && provideCppFunctionsWithPrecompiledCatalogs(graph, sessions,
                    consumer.precompiledModules(), *lookup, demands, functions, deferred, why);
            }, error) && consumer.compileProjectMembers(consumerProject, error);
    if (!consumed) std::cerr << "generic alias consumer: " << error << ' ' << diagnosticText(consumer) << '\n';
    if (!expect(consumed, "consumer compiles generic alias using only precompiled catalogue")) return false;
    const auto* model = consumer.namespaceFunctionModel();
    const auto restoredFunctions = model ? model->functions() : std::span<const nativesharp::frontend::NamespaceFunctionSymbol>{};
    return expect(std::any_of(restoredFunctions.begin(), restoredFunctions.end(), [](const auto& item) {
        return item.name == "cid_function" && item.mangledName == "cid_function";
    }), "consumer restores a demanded C function from the precompiled catalogue");
}

bool checkDependencyTypeCodec()
{
    using namespace nativesharp::interop::metadata_detail;
    CppType floating;
    floating.kind = CppTypeKind::Floating;
    floating.unqualifiedCanonicalSpelling = "long double";
    floating.sizeBits = 64;
    floating.ieeeBinary64 = true;
    CppType atomic;
    atomic.kind = CppTypeKind::Atomic;
    atomic.isVolatile = true;
    atomic.element = std::make_shared<CppType>(floating);
    CppType pointer;
    pointer.kind = CppTypeKind::Pointer;
    pointer.isConst = true;
    pointer.element = std::make_shared<CppType>(atomic);
    std::vector<std::uint8_t> bytes;
    std::string error;
    CppType restored;
    bool ok = expect(encodeCDependencyType(pointer, bytes, error) &&
        decodeCDependencyType(bytes, restored, error) && restored.kind == CppTypeKind::Pointer &&
        restored.isConst && restored.element && restored.element->kind == CppTypeKind::Atomic &&
        restored.element->isVolatile && restored.element->element && restored.element->element->ieeeBinary64,
        "C dependency type round trip preserves atomic qualifiers and floating format");
    const auto validBytes = bytes;
    bytes.push_back(0);
    CppType trailing;
    trailing.spelling = "sentinel";
    ok &= expect(!decodeCDependencyType(bytes, trailing, error) && trailing.spelling == "sentinel",
        "C dependency type rejects trailing data without changing output");
    CppType malformedAtomic;
    malformedAtomic.kind = CppTypeKind::Atomic;
    std::vector<std::uint8_t> unchanged{42};
    ok &= expect(!encodeCDependencyType(malformedAtomic, unchanged, error) && unchanged == std::vector<std::uint8_t>{42},
        "C dependency type rejects an atomic without an element");
    bytes = validBytes;
    CppType untouched;
    untouched.spelling = "sentinel";
    if (!bytes.empty()) bytes.pop_back();
    ok &= expect(!decodeCDependencyType(bytes, untouched, error) && untouched.spelling == "sentinel",
        "truncated C dependency type preserves output on failure");
    CDependencyFunctionSnapshot function;
    function.moduleName = "Producer";
    function.referenceIdentity = "header-digest";
    function.linkSymbol = "c_function";
    function.declaration.name = "c_function";
    function.declaration.qualifiedName = "c_function";
    function.declaration.callingConvention = "cdecl";
    function.declaration.languageLinkage = CppLanguageLinkage::C;
    function.declaration.clangUsr = "c:@F@c_function";
    function.declaration.externalSymbol = "c_function";
    function.declaration.externalLinkage = true;
    function.declaration.returnType = floating;
    function.declaration.parameters.push_back({"value", "T*", "T*", pointer, false});
    nativesharp::frontend::NamespaceFunctionSymbol selected;
    selected.projectOrdinal = 4;
    selected.moduleName = "Producer";
    selected.name = "c_function";
    selected.mangledName = "c_function";
    selected.cppCanonicalIdentity = "c_function|signature";
    selected.cppClangUsr = function.declaration.clangUsr;
    std::vector<CppSemanticInventoryEntry> inventory{{{}, 2, 4, "x86_64-pc-windows-msvc",
        function.declaration, selected}};
    CDependencyFunctionSnapshot captured;
    ok &= expect(captureCDependencyFunction(inventory, selected, 2, "header-digest", captured, error) &&
        captured.linkSymbol == selected.mangledName && captured.declaration.clangUsr == selected.cppClangUsr,
        "C dependency capture requires an explicitly selected function with matching provenance");
    captured.moduleName = "sentinel";
    ok &= expect(!captureCDependencyFunction(inventory, selected, 3, "header-digest", captured, error) &&
        captured.moduleName == "sentinel", "C dependency capture rejects another reference without changing output");
    inventory.push_back(inventory.front());
    ok &= expect(!captureCDependencyFunction(inventory, selected, 2, "header-digest", captured, error) &&
        captured.moduleName == "sentinel", "C dependency capture rejects ambiguous observations");
    std::vector<std::uint8_t> functionBytes;
    CDependencyFunctionSnapshot decoded;
    ok &= expect(encodeCDependencyFunction(function, functionBytes, error) &&
        decodeCDependencyFunction(functionBytes, decoded, error) &&
        decoded.declaration.clangUsr == function.declaration.clangUsr &&
        decoded.declaration.returnType.ieeeBinary64 && decoded.declaration.parameters.size() == 1 &&
        decoded.declaration.parameters[0].type.element &&
        decoded.declaration.parameters[0].type.element->kind == CppTypeKind::Atomic,
        "C dependency function round trip preserves identity, signature and atomic type");
    auto recordFunction = function;
    recordFunction.declaration.callingConvention = "sysv_abi";
    recordFunction.declaration.returnType = CppType{};
    recordFunction.declaration.returnType.kind = CppTypeKind::Record;
    recordFunction.declaration.returnType.sizeBits = 24;
    recordFunction.declaration.parameters[0].type = recordFunction.declaration.returnType;
    auto plan = std::make_shared<abi_plan::Plan>();
    plan->symbol = recordFunction.linkSymbol;
    plan->targetTriple = "x86_64-pc-windows-msvc";
    plan->dataLayout = "e-test";
    plan->callingConvention = 78;
    const abi_plan::Type i24{abi_plan::TypeKind::Integer, 24};
    plan->physicalReturn = i24;
    plan->logicalReturn.passing = abi_plan::Passing::Direct;
    plan->logicalReturn.coercion = i24;
    abi_plan::LogicalValue logical;
    logical.passing = abi_plan::Passing::Direct;
    logical.coercion = i24;
    logical.physicalCount = 1;
    plan->logicalParameters.push_back(logical);
    abi_plan::PhysicalParameter physical;
    physical.type = i24;
    plan->physicalParameters.push_back(physical);
    recordFunction.declaration.abiPlan = plan;
    std::vector<std::uint8_t> recordBytes;
    CDependencyFunctionSnapshot restoredRecord;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        abi_plan::isSysVThreeByteRoundTrip(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol),
        "C dependency function round trip retains the supported SysV record plan");
    auto truncatedRecord = recordBytes;
    if (!truncatedRecord.empty()) truncatedRecord.pop_back();
    restoredRecord.moduleName = "sentinel";
    ok &= expect(!decodeCDependencyFunction(truncatedRecord, restoredRecord, error) &&
        restoredRecord.moduleName == "sentinel", "truncated SysV record plan is rejected without changing output");
    recordFunction.declaration.parameters[0].type.sizeBits = 32;
    std::vector<std::uint8_t> unchangedRecord{42};
    ok &= expect(!encodeCDependencyFunction(recordFunction, unchangedRecord, error) &&
        unchangedRecord == std::vector<std::uint8_t>{42}, "SysV record plan rejects a conflicting C signature");
    auto plan4 = std::make_shared<abi_plan::Plan>(*plan);
    plan4->physicalReturn.widthOrCount = 32;
    plan4->logicalReturn.coercion->widthOrCount = 32;
    plan4->logicalParameters[0].coercion->widthOrCount = 32;
    plan4->physicalParameters[0].type.widthOrCount = 32;
    recordFunction.declaration.abiPlan = plan4;
    recordFunction.declaration.returnType.sizeBits = 32;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        abi_plan::isSysVSmallRecordRoundTrip(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol) &&
        restoredRecord.declaration.abiPlan->physicalReturn.widthOrCount == 32,
        "C dependency function retains a direct four-byte SysV record plan");
    auto twoSmallPlan = std::make_shared<abi_plan::Plan>(*plan4);
    auto secondLogical = twoSmallPlan->logicalParameters[0];
    secondLogical.firstPhysical = 1;
    twoSmallPlan->logicalParameters.push_back(secondLogical);
    twoSmallPlan->physicalParameters.push_back(twoSmallPlan->physicalParameters[0]);
    recordFunction.declaration.abiPlan = twoSmallPlan;
    recordFunction.declaration.parameters.push_back(recordFunction.declaration.parameters[0]);
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        abi_plan::isSysVTwoSmallRecordsRoundTrip(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol),
        "C dependency function retains two direct record arguments");
    auto heteroPlan = std::make_shared<abi_plan::Plan>(*twoSmallPlan);
    heteroPlan->logicalParameters[1].coercion->widthOrCount = 64;
    heteroPlan->physicalParameters[1].type.widthOrCount = 64;
    recordFunction.declaration.abiPlan = heteroPlan;
    recordFunction.declaration.parameters[1].type.sizeBits = 64;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        abi_plan::isSysVSmallRecordsRoundTrip(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol) &&
        restoredRecord.declaration.abiPlan->physicalParameters[1].type.widthOrCount == 64,
        "C dependency function retains direct record arguments of different sizes");
    recordFunction.declaration.parameters[1].type.sizeBits = 32;
    const auto savedSecondType = recordFunction.declaration.parameters[1].type;
    recordFunction.declaration.parameters[1].type.kind = CppTypeKind::SignedInteger;
    recordFunction.declaration.abiPlan = twoSmallPlan;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        restoredRecord.declaration.parameters[1].type.kind == CppTypeKind::SignedInteger &&
        abi_plan::isSysVSmallRecordsRoundTrip(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol),
        "C dependency function retains a record and an integer argument");
    recordFunction.declaration.parameters[1].type = savedSecondType;
    auto scalarFloatPlan = std::make_shared<abi_plan::Plan>(*twoSmallPlan);
    scalarFloatPlan->logicalParameters[1].coercion = abi_plan::Type{abi_plan::TypeKind::Float};
    scalarFloatPlan->physicalParameters[1].type = abi_plan::Type{abi_plan::TypeKind::Float};
    recordFunction.declaration.parameters[1].type.kind = CppTypeKind::Floating;
    recordFunction.declaration.abiPlan = scalarFloatPlan;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        restoredRecord.declaration.abiPlan->physicalParameters[1].type.kind == abi_plan::TypeKind::Float,
        "C dependency function retains a record and a float argument");
    scalarFloatPlan->logicalParameters[1].coercion = abi_plan::Type{abi_plan::TypeKind::Double};
    scalarFloatPlan->physicalParameters[1].type = abi_plan::Type{abi_plan::TypeKind::Double};
    recordFunction.declaration.parameters[1].type.sizeBits = 64;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        restoredRecord.declaration.abiPlan->physicalParameters[1].type.kind == abi_plan::TypeKind::Double,
        "C dependency function retains a record and a double argument");
    recordFunction.declaration.parameters[1].type = savedSecondType;
    recordFunction.declaration.abiPlan = twoSmallPlan;
    const auto savedFirstType = recordFunction.declaration.parameters[0].type;
    recordFunction.declaration.parameters[0].type.kind = CppTypeKind::SignedInteger;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        restoredRecord.declaration.parameters[0].type.kind == CppTypeKind::SignedInteger &&
        abi_plan::isSysVSmallRecordsRoundTrip(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol),
        "C dependency function retains an integer and a record argument");
    recordFunction.declaration.parameters[0].type = savedFirstType;
    auto firstFloatPlan = std::make_shared<abi_plan::Plan>(*twoSmallPlan);
    firstFloatPlan->logicalParameters[0].coercion = abi_plan::Type{abi_plan::TypeKind::Float};
    firstFloatPlan->physicalParameters[0].type = abi_plan::Type{abi_plan::TypeKind::Float};
    recordFunction.declaration.parameters[0].type.kind = CppTypeKind::Floating;
    recordFunction.declaration.abiPlan = firstFloatPlan;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        restoredRecord.declaration.abiPlan->physicalParameters[0].type.kind == abi_plan::TypeKind::Float,
        "C dependency function retains a float and a record argument");
    firstFloatPlan->logicalParameters[0].coercion = abi_plan::Type{abi_plan::TypeKind::Double};
    firstFloatPlan->physicalParameters[0].type = abi_plan::Type{abi_plan::TypeKind::Double};
    recordFunction.declaration.parameters[0].type.sizeBits = 64;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        restoredRecord.declaration.abiPlan->physicalParameters[0].type.kind == abi_plan::TypeKind::Double,
        "C dependency function retains a double and a record argument");
    firstFloatPlan->logicalParameters[0].coercion = abi_plan::Type{abi_plan::TypeKind::Float};
    firstFloatPlan->physicalParameters[0].type = abi_plan::Type{abi_plan::TypeKind::Float};
    auto thirdFloat = firstFloatPlan->logicalParameters[0];
    thirdFloat.firstPhysical = 2;
    firstFloatPlan->logicalParameters.push_back(thirdFloat);
    firstFloatPlan->physicalParameters.push_back(firstFloatPlan->physicalParameters[0]);
    recordFunction.declaration.parameters[0].type.sizeBits = 32;
    recordFunction.declaration.parameters.push_back(recordFunction.declaration.parameters[0]);
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        restoredRecord.declaration.abiPlan->physicalParameters.size() == 3 &&
        restoredRecord.declaration.abiPlan->physicalParameters[2].type.kind == abi_plan::TypeKind::Float,
        "C dependency function retains two floats and a record argument");
    for (std::size_t i = 3; i < 10; ++i)
    {
        auto logical = firstFloatPlan->logicalParameters[0];
        logical.firstPhysical = i;
        auto physical = firstFloatPlan->physicalParameters[0];
        auto parameter = recordFunction.declaration.parameters[0];
        if (i % 2 == 0)
        {
            logical.coercion = abi_plan::Type{abi_plan::TypeKind::Double};
            physical.type = *logical.coercion;
            parameter.type.sizeBits = 64;
        }
        firstFloatPlan->logicalParameters.push_back(logical);
        firstFloatPlan->physicalParameters.push_back(physical);
        recordFunction.declaration.parameters.push_back(parameter);
    }
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        restoredRecord.declaration.abiPlan->physicalParameters.size() == 10 &&
        restoredRecord.declaration.abiPlan->physicalParameters[8].type.kind == abi_plan::TypeKind::Double,
        "C dependency function retains mixed floating arguments beyond register capacity");
    recordFunction.declaration.parameters.resize(3);
    recordFunction.declaration.parameters.pop_back();
    recordFunction.declaration.parameters[0].type = savedFirstType;
    recordFunction.declaration.abiPlan = twoSmallPlan;
    auto eightPlan = std::make_shared<abi_plan::Plan>(*twoSmallPlan);
    for (std::size_t i = 2; i < 8; ++i)
    {
        auto logical = eightPlan->logicalParameters[0];
        logical.firstPhysical = i;
        eightPlan->logicalParameters.push_back(logical);
        eightPlan->physicalParameters.push_back(eightPlan->physicalParameters[0]);
        recordFunction.declaration.parameters.push_back(recordFunction.declaration.parameters[0]);
    }
    recordFunction.declaration.abiPlan = eightPlan;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        abi_plan::isSysVSmallRecordsRoundTrip(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol) &&
        restoredRecord.declaration.abiPlan->logicalParameters.size() == 8,
        "C dependency function retains eight direct record arguments");
    for (const auto index : {2U, 4U, 6U})
        recordFunction.declaration.parameters[index].type.kind = CppTypeKind::SignedInteger;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        restoredRecord.declaration.parameters[6].type.kind == CppTypeKind::SignedInteger &&
        abi_plan::isSysVSmallRecordsRoundTrip(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol) &&
        restoredRecord.declaration.abiPlan->logicalParameters.size() == 8,
        "C dependency function retains eight mixed record and integer arguments");
    for (const auto index : {2U, 4U, 6U})
        recordFunction.declaration.parameters[index].type.kind = CppTypeKind::Record;
    recordFunction.declaration.parameters.resize(2);
    recordFunction.declaration.parameters.pop_back();
    for (const std::uint64_t bits : {8U, 16U, 40U, 48U, 56U, 64U})
    {
        auto smallPlan = std::make_shared<abi_plan::Plan>(*plan4);
        smallPlan->physicalReturn.widthOrCount = bits;
        smallPlan->logicalReturn.coercion->widthOrCount = bits;
        smallPlan->logicalParameters[0].coercion->widthOrCount = bits;
        smallPlan->physicalParameters[0].type.widthOrCount = bits;
        recordFunction.declaration.abiPlan = smallPlan;
        recordFunction.declaration.returnType.sizeBits = bits;
        recordFunction.declaration.parameters[0].type.sizeBits = bits;
        ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
            decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
            restoredRecord.declaration.abiPlan->physicalReturn.widthOrCount == bits &&
            abi_plan::isSysVSmallRecordRoundTrip(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol),
            "C dependency function retains a small SysV record plan");
    }
    auto pairPlan = std::make_shared<abi_plan::Plan>(*plan4);
    abi_plan::Type pair;
    pair.kind = abi_plan::TypeKind::Structure;
    pair.elements = {{abi_plan::TypeKind::Integer, 64}, {abi_plan::TypeKind::Integer, 64}};
    pairPlan->physicalReturn = pair;
    pairPlan->logicalReturn.coercion = pair;
    pairPlan->logicalParameters[0].coercion = pair;
    pairPlan->logicalParameters[0].physicalCount = 2;
    pairPlan->physicalParameters[0].type = pair.elements[0];
    pairPlan->physicalParameters.push_back(pairPlan->physicalParameters[0]);
    recordFunction.declaration.abiPlan = pairPlan;
    recordFunction.declaration.returnType.sizeBits = 128;
    recordFunction.declaration.parameters[0].type.sizeBits = 128;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        abi_plan::isSysVTwoI64RecordRoundTrip(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol),
        "C dependency function retains a two-piece SysV record plan");
    auto pairScalarPlan = std::make_shared<abi_plan::Plan>(*pairPlan);
    auto scalarParameter = recordFunction.declaration.parameters[0];
    scalarParameter.type.kind = CppTypeKind::SignedInteger;
    scalarParameter.type.sizeBits = 32;
    for (std::size_t i = 0; i < 2; ++i)
    {
        abi_plan::LogicalValue scalar;
        scalar.passing = abi_plan::Passing::Direct;
        scalar.firstPhysical = i + 2;
        scalar.physicalCount = 1;
        scalar.coercion = i == 0 ? abi_plan::Type{abi_plan::TypeKind::Integer, 32} : abi_plan::Type{abi_plan::TypeKind::Float};
        pairScalarPlan->logicalParameters.push_back(scalar);
        abi_plan::PhysicalParameter physical;
        physical.type = *scalar.coercion;
        pairScalarPlan->physicalParameters.push_back(physical);
        if (i == 1) scalarParameter.type.kind = CppTypeKind::Floating;
        recordFunction.declaration.parameters.push_back(scalarParameter);
    }
    recordFunction.declaration.abiPlan = pairScalarPlan;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        abi_plan::isSysVTwoIntegerRecordWithScalars(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol) &&
        restoredRecord.declaration.abiPlan->logicalParameters[2].firstPhysical == 3,
        "C dependency function retains a two-piece record and scalar arguments");
    recordFunction.declaration.parameters.resize(1);
    auto twoPairPlan = std::make_shared<abi_plan::Plan>(*pairPlan);
    auto secondPair = twoPairPlan->logicalParameters[0];
    secondPair.firstPhysical = 2;
    twoPairPlan->logicalParameters.push_back(secondPair);
    twoPairPlan->physicalParameters.push_back(twoPairPlan->physicalParameters[0]);
    twoPairPlan->physicalParameters.push_back(twoPairPlan->physicalParameters[1]);
    abi_plan::LogicalValue trailingInteger;
    trailingInteger.passing = abi_plan::Passing::Direct;
    trailingInteger.firstPhysical = 4;
    trailingInteger.physicalCount = 1;
    trailingInteger.coercion = abi_plan::Type{abi_plan::TypeKind::Integer, 32};
    twoPairPlan->logicalParameters.push_back(trailingInteger);
    abi_plan::PhysicalParameter integerPhysical;
    integerPhysical.type = *trailingInteger.coercion;
    twoPairPlan->physicalParameters.push_back(integerPhysical);
    recordFunction.declaration.parameters.push_back(recordFunction.declaration.parameters[0]);
    scalarParameter.type.kind = CppTypeKind::SignedInteger;
    recordFunction.declaration.parameters.push_back(scalarParameter);
    recordFunction.declaration.abiPlan = twoPairPlan;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        abi_plan::isSysVTwoIntegerRecordWithScalars(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol) &&
        restoredRecord.declaration.abiPlan->logicalParameters[2].firstPhysical == 4,
        "C dependency function retains two two-piece records and an integer argument");
    auto differentPairPlan = std::make_shared<abi_plan::Plan>(*twoPairPlan);
    differentPairPlan->physicalReturn.elements[1].widthOrCount = 32;
    differentPairPlan->logicalReturn.coercion->elements[1].widthOrCount = 32;
    differentPairPlan->logicalParameters[0].coercion->elements[1].widthOrCount = 32;
    differentPairPlan->physicalParameters[1].type.widthOrCount = 32;
    recordFunction.declaration.returnType.sizeBits = 96;
    recordFunction.declaration.parameters[0].type.sizeBits = 96;
    recordFunction.declaration.abiPlan = differentPairPlan;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        abi_plan::isSysVTwoIntegerRecordWithScalars(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol) &&
        restoredRecord.declaration.abiPlan->physicalParameters[1].type.widthOrCount == 32 &&
        restoredRecord.declaration.abiPlan->physicalParameters[3].type.widthOrCount == 64,
        "C dependency function retains two-piece records of different sizes");
    auto scalarFirstPlan = std::make_shared<abi_plan::Plan>(*differentPairPlan);
    std::rotate(scalarFirstPlan->logicalParameters.begin(), scalarFirstPlan->logicalParameters.end() - 1,
                scalarFirstPlan->logicalParameters.end());
    std::rotate(scalarFirstPlan->physicalParameters.begin(), scalarFirstPlan->physicalParameters.end() - 1,
                scalarFirstPlan->physicalParameters.end());
    scalarFirstPlan->logicalParameters[0].firstPhysical = 0;
    scalarFirstPlan->logicalParameters[1].firstPhysical = 1;
    scalarFirstPlan->logicalParameters[2].firstPhysical = 3;
    std::rotate(recordFunction.declaration.parameters.begin(), recordFunction.declaration.parameters.end() - 1,
                recordFunction.declaration.parameters.end());
    recordFunction.declaration.abiPlan = scalarFirstPlan;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        abi_plan::isSysVTwoIntegerRecordWithScalars(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol) &&
        restoredRecord.declaration.abiPlan->logicalParameters[1].firstPhysical == 1 &&
        restoredRecord.declaration.abiPlan->logicalParameters[2].firstPhysical == 3,
        "C dependency function retains a scalar before two records");
    std::rotate(recordFunction.declaration.parameters.begin(), recordFunction.declaration.parameters.begin() + 1,
                recordFunction.declaration.parameters.end());
    auto pairSmallPlan = std::make_shared<abi_plan::Plan>(*differentPairPlan);
    pairSmallPlan->logicalParameters[1].coercion = abi_plan::Type{abi_plan::TypeKind::Integer, 32};
    pairSmallPlan->logicalParameters[1].physicalCount = 1;
    pairSmallPlan->logicalParameters[2].firstPhysical = 3;
    pairSmallPlan->physicalParameters[2].type = abi_plan::Type{abi_plan::TypeKind::Integer, 32};
    pairSmallPlan->physicalParameters.erase(pairSmallPlan->physicalParameters.begin() + 3);
    recordFunction.declaration.parameters[1].type.sizeBits = 32;
    recordFunction.declaration.abiPlan = pairSmallPlan;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        abi_plan::isSysVTwoIntegerRecordWithScalars(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol) &&
        restoredRecord.declaration.abiPlan->logicalParameters[1].physicalCount == 1 &&
        restoredRecord.declaration.abiPlan->logicalParameters[2].firstPhysical == 3,
        "C dependency function retains records passed in one and two pieces");
    recordFunction.declaration.returnType.sizeBits = 128;
    recordFunction.declaration.parameters[0].type.sizeBits = 128;
    recordFunction.declaration.parameters.resize(1);
    for (const std::uint64_t bits : {8U, 16U, 24U, 32U, 40U, 48U, 56U})
    {
        auto varied = std::make_shared<abi_plan::Plan>(*pairPlan);
        varied->physicalReturn.elements[1].widthOrCount = bits;
        varied->logicalReturn.coercion->elements[1].widthOrCount = bits;
        varied->logicalParameters[0].coercion->elements[1].widthOrCount = bits;
        varied->physicalParameters[1].type.widthOrCount = bits;
        recordFunction.declaration.abiPlan = varied;
        recordFunction.declaration.returnType.sizeBits = 64 + bits;
        recordFunction.declaration.parameters[0].type.sizeBits = 64 + bits;
        ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
            decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
            abi_plan::isSysVTwoIntegerRecordRoundTrip(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol) &&
            restoredRecord.declaration.abiPlan->physicalReturn.elements[1].widthOrCount == bits,
            "C dependency function retains the second piece width");
    }
    auto indirectPlan = std::make_shared<abi_plan::Plan>();
    indirectPlan->symbol = recordFunction.linkSymbol;
    indirectPlan->targetTriple = "x86_64-pc-windows-msvc";
    indirectPlan->dataLayout = "e-test";
    indirectPlan->callingConvention = 78;
    indirectPlan->logicalReturn.passing = abi_plan::Passing::Indirect;
    indirectPlan->logicalReturn.alignment = 1;
    abi_plan::LogicalValue indirectLogical;
    indirectLogical.passing = abi_plan::Passing::Indirect;
    indirectLogical.alignment = 8;
    indirectLogical.byValue = true;
    indirectLogical.firstPhysical = 1;
    indirectLogical.physicalCount = 1;
    indirectPlan->logicalParameters.push_back(indirectLogical);
    abi_plan::PhysicalParameter indirectResult;
    indirectResult.type.kind = abi_plan::TypeKind::Pointer;
    indirectResult.pointee = abi_plan::Type{};
    indirectResult.structureReturn = true;
    indirectResult.alignment = 1;
    indirectPlan->physicalParameters.push_back(indirectResult);
    abi_plan::PhysicalParameter indirectArgument;
    indirectArgument.type.kind = abi_plan::TypeKind::Pointer;
    indirectArgument.pointee = abi_plan::Type{};
    indirectArgument.byValue = true;
    indirectArgument.alignment = 8;
    indirectPlan->physicalParameters.push_back(indirectArgument);
    recordFunction.declaration.abiPlan = indirectPlan;
    recordFunction.declaration.returnType.sizeBits = 192;
    recordFunction.declaration.returnType.alignmentBits = 8;
    recordFunction.declaration.parameters[0].type.sizeBits = 192;
    recordFunction.declaration.parameters[0].type.alignmentBits = 8;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        abi_plan::isSysVIndirectRecordRoundTrip(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol),
        "C dependency function retains the indirect SysV record plan");
    auto indirectMixedPlan = std::make_shared<abi_plan::Plan>(*indirectPlan);
    indirectMixedPlan->logicalParameters.clear();
    indirectMixedPlan->physicalParameters.resize(1);
    auto indirectRecordParameter = recordFunction.declaration.parameters[0];
    recordFunction.declaration.parameters.clear();
    for (std::size_t i = 0; i < 4; ++i)
    {
        auto parameter = indirectRecordParameter;
        abi_plan::LogicalValue logical;
        logical.firstPhysical = i + 1;
        logical.physicalCount = 1;
        abi_plan::PhysicalParameter physical;
        if (i == 1 || i == 3)
        {
            logical.passing = abi_plan::Passing::Indirect;
            logical.byValue = true;
            logical.alignment = 8;
            physical = indirectArgument;
        }
        else
        {
            logical.passing = abi_plan::Passing::Direct;
            logical.coercion = i == 0 ? abi_plan::Type{abi_plan::TypeKind::Float} : abi_plan::Type{abi_plan::TypeKind::Integer, 32};
            physical.type = *logical.coercion;
            parameter.type.kind = i == 0 ? CppTypeKind::Floating : CppTypeKind::SignedInteger;
            parameter.type.sizeBits = 32;
        }
        indirectMixedPlan->logicalParameters.push_back(logical);
        indirectMixedPlan->physicalParameters.push_back(physical);
        recordFunction.declaration.parameters.push_back(parameter);
    }
    recordFunction.declaration.abiPlan = indirectMixedPlan;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        abi_plan::isSysVIndirectRecordWithArguments(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol) &&
        restoredRecord.declaration.abiPlan->physicalParameters[2].byValue &&
        restoredRecord.declaration.abiPlan->physicalParameters[4].byValue,
        "C dependency function retains two indirect records mixed with scalars");
    auto differentIndirectPlan = std::make_shared<abi_plan::Plan>(*indirectMixedPlan);
    differentIndirectPlan->logicalReturn.alignment = 4;
    differentIndirectPlan->physicalParameters[0].alignment = 4;
    differentIndirectPlan->logicalParameters[3].alignment = 16;
    differentIndirectPlan->physicalParameters[4].alignment = 16;
    recordFunction.declaration.returnType.sizeBits = 160;
    recordFunction.declaration.returnType.alignmentBits = 32;
    recordFunction.declaration.parameters[3].type.sizeBits = 256;
    recordFunction.declaration.parameters[3].type.alignmentBits = 128;
    recordFunction.declaration.abiPlan = differentIndirectPlan;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        abi_plan::isSysVIndirectRecordWithArguments(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol) &&
        restoredRecord.declaration.abiPlan->logicalReturn.alignment == 4 &&
        restoredRecord.declaration.abiPlan->physicalParameters[2].alignment == 8 &&
        restoredRecord.declaration.abiPlan->physicalParameters[4].alignment == 16,
        "C dependency function retains different indirect record sizes and alignments");
    auto directIndirectPlan = std::make_shared<abi_plan::Plan>(*differentIndirectPlan);
    directIndirectPlan->logicalParameters[1].passing = abi_plan::Passing::Direct;
    directIndirectPlan->logicalParameters[1].byValue = false;
    directIndirectPlan->logicalParameters[1].alignment = 0;
    directIndirectPlan->logicalParameters[1].coercion = abi_plan::Type{abi_plan::TypeKind::Integer, 32};
    directIndirectPlan->physicalParameters[2] = {};
    directIndirectPlan->physicalParameters[2].type = *directIndirectPlan->logicalParameters[1].coercion;
    recordFunction.declaration.parameters[1].type.sizeBits = 32;
    recordFunction.declaration.parameters[1].type.alignmentBits = 32;
    recordFunction.declaration.abiPlan = directIndirectPlan;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        abi_plan::isSysVIndirectRecordWithArguments(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol) &&
        restoredRecord.declaration.abiPlan->logicalParameters[1].passing == abi_plan::Passing::Direct &&
        restoredRecord.declaration.abiPlan->physicalParameters[4].byValue,
        "C dependency function retains direct and indirect record arguments");
    auto pairIndirectPlan = std::make_shared<abi_plan::Plan>(*directIndirectPlan);
    abi_plan::Type directPair;
    directPair.kind = abi_plan::TypeKind::Structure;
    directPair.elements = {{abi_plan::TypeKind::Integer, 64}, {abi_plan::TypeKind::Integer, 32}};
    pairIndirectPlan->logicalParameters[1].coercion = directPair;
    pairIndirectPlan->logicalParameters[1].physicalCount = 2;
    pairIndirectPlan->logicalParameters[2].firstPhysical = 4;
    pairIndirectPlan->logicalParameters[3].firstPhysical = 5;
    pairIndirectPlan->physicalParameters[2].type = directPair.elements[0];
    abi_plan::PhysicalParameter pairSecond;
    pairSecond.type = directPair.elements[1];
    pairIndirectPlan->physicalParameters.insert(pairIndirectPlan->physicalParameters.begin() + 3, pairSecond);
    recordFunction.declaration.parameters[1].type.sizeBits = 96;
    recordFunction.declaration.abiPlan = pairIndirectPlan;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        abi_plan::isSysVIndirectRecordWithArguments(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol) &&
        restoredRecord.declaration.abiPlan->logicalParameters[1].physicalCount == 2 &&
        restoredRecord.declaration.abiPlan->physicalParameters[5].byValue,
        "C dependency function retains a direct two-piece record and an indirect record");
    auto mixedIndirectPlan = std::make_shared<abi_plan::Plan>(*pairIndirectPlan);
    mixedIndirectPlan->logicalParameters[1].coercion->elements[0] = abi_plan::Type{abi_plan::TypeKind::Double};
    mixedIndirectPlan->physicalParameters[2].type = abi_plan::Type{abi_plan::TypeKind::Double};
    recordFunction.declaration.parameters[1].type.sizeBits = 128;
    recordFunction.declaration.parameters[1].type.alignmentBits = 64;
    recordFunction.declaration.abiPlan = mixedIndirectPlan;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        abi_plan::isSysVIndirectRecordWithArguments(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol) &&
        restoredRecord.declaration.abiPlan->physicalParameters[2].type.kind == abi_plan::TypeKind::Double &&
        restoredRecord.declaration.abiPlan->physicalParameters[3].type.widthOrCount == 32,
        "C dependency function retains mixed direct pieces and an indirect record");
    const auto savedPackedReturnType = recordFunction.declaration.returnType;
    const auto savedPackedArgumentType = recordFunction.declaration.parameters[3].type;
    auto packedIndirectPlan = std::make_shared<abi_plan::Plan>(*mixedIndirectPlan);
    packedIndirectPlan->logicalReturn.alignment = 1;
    packedIndirectPlan->physicalParameters[0].alignment = 1;
    packedIndirectPlan->logicalParameters[3].alignment = 8;
    packedIndirectPlan->physicalParameters[5].alignment = 8;
    recordFunction.declaration.returnType.sizeBits = 72;
    recordFunction.declaration.returnType.alignmentBits = 8;
    recordFunction.declaration.parameters[3].type.sizeBits = 72;
    recordFunction.declaration.parameters[3].type.alignmentBits = 8;
    recordFunction.declaration.abiPlan = packedIndirectPlan;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        restoredRecord.declaration.abiPlan->logicalReturn.alignment == 1 &&
        restoredRecord.declaration.abiPlan->logicalParameters[3].passing == abi_plan::Passing::Indirect &&
        restoredRecord.declaration.abiPlan->physicalParameters[5].byValue &&
        restoredRecord.declaration.abiPlan->physicalParameters[5].alignment == 8,
        "C dependency function retains a small indirect record and indirect return");
    auto invalidSmallIndirectBytes = recordBytes;
    if (!invalidSmallIndirectBytes.empty()) invalidSmallIndirectBytes.back() = 2;
    ok &= expect(!invalidSmallIndirectBytes.empty() &&
        !decodeCDependencyFunction(invalidSmallIndirectBytes, restoredRecord, error),
        "C dependency function rejects a conflicting small indirect descriptor");
    recordFunction.declaration.returnType = savedPackedReturnType;
    recordFunction.declaration.parameters[3].type = savedPackedArgumentType;
    auto directPairReturnPlan = std::make_shared<abi_plan::Plan>(*mixedIndirectPlan);
    directPairReturnPlan->physicalParameters.erase(directPairReturnPlan->physicalParameters.begin());
    for (auto& logical : directPairReturnPlan->logicalParameters) --logical.firstPhysical;
    directPairReturnPlan->physicalReturn = *directPairReturnPlan->logicalParameters[1].coercion;
    directPairReturnPlan->logicalReturn = {};
    directPairReturnPlan->logicalReturn.passing = abi_plan::Passing::Direct;
    directPairReturnPlan->logicalReturn.coercion = directPairReturnPlan->physicalReturn;
    const auto originalIndirectReturnType = recordFunction.declaration.returnType;
    recordFunction.declaration.returnType.sizeBits = 128;
    recordFunction.declaration.returnType.alignmentBits = 64;
    recordFunction.declaration.abiPlan = directPairReturnPlan;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        abi_plan::isSysVDirectPairReturnWithArguments(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol) &&
        restoredRecord.declaration.abiPlan->physicalReturn.elements[0].kind == abi_plan::TypeKind::Double &&
        restoredRecord.declaration.abiPlan->physicalReturn.elements[1].widthOrCount == 32 &&
        restoredRecord.declaration.abiPlan->physicalParameters[4].byValue,
        "C dependency function retains a direct mixed pair return and indirect argument");
    for (unsigned width : {8u, 16u, 32u})
    {
        auto integerPairPlan = std::make_shared<abi_plan::Plan>(*directPairReturnPlan);
        integerPairPlan->physicalReturn.elements = {
            abi_plan::Type{abi_plan::TypeKind::Integer, 64}, abi_plan::Type{abi_plan::TypeKind::Integer, width}};
        integerPairPlan->logicalReturn.coercion = integerPairPlan->physicalReturn;
        recordFunction.declaration.abiPlan = integerPairPlan;
        ok &= expect(abi_plan::isSysVDirectPairLayout(integerPairPlan->physicalReturn, 128, 64) &&
            !abi_plan::isSysVDirectPairLayout(integerPairPlan->physicalReturn, 128, 32) &&
            encodeCDependencyFunction(recordFunction, recordBytes, error) &&
            decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
            restoredRecord.declaration.abiPlan->physicalReturn.elements[1].widthOrCount == width,
            "C dependency function retains a padded integer pair return");
    }
    auto vectorPairReturnPlan = std::make_shared<abi_plan::Plan>(*directPairReturnPlan);
    abi_plan::Type returnedVector{abi_plan::TypeKind::Vector, 2};
    returnedVector.elements.push_back(abi_plan::Type{abi_plan::TypeKind::Float});
    vectorPairReturnPlan->physicalReturn.elements = {returnedVector, returnedVector};
    vectorPairReturnPlan->logicalReturn.coercion = vectorPairReturnPlan->physicalReturn;
    recordFunction.declaration.returnType.alignmentBits = 32;
    recordFunction.declaration.abiPlan = vectorPairReturnPlan;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        restoredRecord.declaration.abiPlan->physicalReturn.elements[0].kind == abi_plan::TypeKind::Vector &&
        restoredRecord.declaration.abiPlan->physicalReturn.elements[1].kind == abi_plan::TypeKind::Vector,
        "C dependency function retains both vector return pieces with indirect argument");
    for (unsigned variant = 0; variant < 7; ++variant)
    {
        auto paddedPlan = std::make_shared<abi_plan::Plan>(*directPairReturnPlan);
        const bool vectorFirst = variant >= 5;
        const bool integerFirst = variant < 3;
        const unsigned width = integerFirst ? (variant == 0 ? 8 : variant == 1 ? 16 : 32) :
            (variant == 3 || variant == 5 ? 8 : 16);
        const abi_plan::Type integerPiece{abi_plan::TypeKind::Integer, width};
        const abi_plan::Type doublePiece{abi_plan::TypeKind::Double};
        paddedPlan->physicalReturn.elements = integerFirst ? std::vector<abi_plan::Type>{integerPiece, doublePiece} :
            std::vector<abi_plan::Type>{vectorFirst ? returnedVector : doublePiece, integerPiece};
        paddedPlan->logicalReturn.coercion = paddedPlan->physicalReturn;
        recordFunction.declaration.returnType.sizeBits = vectorFirst ? 96 : 128;
        recordFunction.declaration.returnType.alignmentBits = vectorFirst ? 32 : 64;
        recordFunction.declaration.abiPlan = paddedPlan;
        ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
            decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
            restoredRecord.declaration.abiPlan->physicalReturn.elements[integerFirst ? 0 : 1].widthOrCount == width,
            "C dependency function retains padded integer and floating return pieces");
        ok &= expect(!abi_plan::isSysVDirectPairLayout(paddedPlan->physicalReturn, 64, vectorFirst ? 32 : 64),
            "padded SysV pair rejects a record smaller than its second piece");
    }
    for (unsigned variant = 0; variant < 4; ++variant)
    {
        auto alignedPlan = std::make_shared<abi_plan::Plan>(*directPairReturnPlan);
        const abi_plan::Type floatPiece{abi_plan::TypeKind::Float};
        const abi_plan::Type integerPiece{abi_plan::TypeKind::Integer, 64};
        alignedPlan->physicalReturn.elements = variant == 0 ? std::vector<abi_plan::Type>{floatPiece, integerPiece} :
            variant == 1 ? std::vector<abi_plan::Type>{integerPiece, floatPiece} :
            variant == 2 ? std::vector<abi_plan::Type>{floatPiece, returnedVector} :
                           std::vector<abi_plan::Type>{returnedVector, floatPiece};
        alignedPlan->logicalReturn.coercion = alignedPlan->physicalReturn;
        recordFunction.declaration.returnType.sizeBits = 128;
        recordFunction.declaration.returnType.alignmentBits = 64;
        recordFunction.declaration.abiPlan = alignedPlan;
        ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
            decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
            restoredRecord.declaration.abiPlan->physicalReturn.elements[0].kind == alignedPlan->physicalReturn.elements[0].kind &&
            restoredRecord.declaration.abiPlan->physicalReturn.elements[1].kind == alignedPlan->physicalReturn.elements[1].kind,
            "C dependency function retains aligned float and integer or vector pieces");
    }
    for (unsigned variant = 0; variant < 5; ++variant)
    {
        auto singleReturnPlan = std::make_shared<abi_plan::Plan>(*directPairReturnPlan);
        singleReturnPlan->physicalReturn = variant == 0 ? abi_plan::Type{abi_plan::TypeKind::Integer, 24} :
            variant == 1 ? abi_plan::Type{abi_plan::TypeKind::Float} : variant == 2 ? returnedVector :
            abi_plan::Type{abi_plan::TypeKind::Integer, variant == 3 ? 32U : 64U};
        singleReturnPlan->logicalReturn.coercion = singleReturnPlan->physicalReturn;
        recordFunction.declaration.returnType.sizeBits = variant == 0 ? 24 : variant == 1 || variant == 3 ? 32 : 64;
        recordFunction.declaration.returnType.alignmentBits = variant == 0 ? 8 : variant == 4 ? 64 : 32;
        recordFunction.declaration.abiPlan = singleReturnPlan;
        ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
            decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
            abi_plan::isSysVDirectSingleReturnWithArguments(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol) &&
            restoredRecord.declaration.abiPlan->physicalReturn.kind == singleReturnPlan->physicalReturn.kind &&
            restoredRecord.declaration.abiPlan->physicalReturn.widthOrCount == singleReturnPlan->physicalReturn.widthOrCount,
            "C dependency function retains a single direct record return with indirect argument");
    }
    auto invalidPairReturn = *directPairReturnPlan;
    invalidPairReturn.logicalReturn.coercion->elements[1].widthOrCount = 64;
    ok &= expect(!abi_plan::isSysVDirectPairReturnWithArguments(invalidPairReturn, recordFunction.linkSymbol),
        "SysV record plan rejects inconsistent direct return pieces");
    recordFunction.declaration.returnType = originalIndirectReturnType;
    auto scalarReturnPlan = std::make_shared<abi_plan::Plan>(*mixedIndirectPlan);
    scalarReturnPlan->physicalParameters.erase(scalarReturnPlan->physicalParameters.begin());
    for (auto& logical : scalarReturnPlan->logicalParameters) --logical.firstPhysical;
    scalarReturnPlan->physicalReturn = abi_plan::Type{abi_plan::TypeKind::Integer, 32};
    scalarReturnPlan->logicalReturn = {};
    scalarReturnPlan->logicalReturn.passing = abi_plan::Passing::Direct;
    scalarReturnPlan->logicalReturn.coercion = scalarReturnPlan->physicalReturn;
    const auto savedReturnType = recordFunction.declaration.returnType;
    recordFunction.declaration.returnType.kind = CppTypeKind::SignedInteger;
    recordFunction.declaration.returnType.sizeBits = 32;
    recordFunction.declaration.returnType.alignmentBits = 32;
    const auto scalarReturnType = recordFunction.declaration.returnType;
    recordFunction.declaration.abiPlan = scalarReturnPlan;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        abi_plan::isSysVScalarReturnWithRecordArguments(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol) &&
        restoredRecord.declaration.abiPlan->physicalParameters[1].type.kind == abi_plan::TypeKind::Double &&
        restoredRecord.declaration.abiPlan->physicalParameters[4].byValue,
        "C dependency function retains scalar return and direct and indirect record arguments");
    auto packedScalarPlan = std::make_shared<abi_plan::Plan>(*scalarReturnPlan);
    packedScalarPlan->logicalParameters[3].alignment = 8;
    packedScalarPlan->physicalParameters[4].alignment = 8;
    recordFunction.declaration.parameters[3].type.sizeBits = 72;
    recordFunction.declaration.parameters[3].type.alignmentBits = 8;
    recordFunction.declaration.abiPlan = packedScalarPlan;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        restoredRecord.declaration.abiPlan->logicalParameters[3].passing == abi_plan::Passing::Indirect &&
        restoredRecord.declaration.abiPlan->physicalParameters[4].byValue,
        "C dependency function retains a small indirect argument with scalar return");
    recordFunction.declaration.parameters[3].type = savedPackedArgumentType;
    const auto savedScalarParameters = recordFunction.declaration.parameters;
    auto scalarSmallRecord = std::make_shared<abi_plan::Plan>(*scalarReturnPlan);
    scalarSmallRecord->logicalParameters.pop_back();
    scalarSmallRecord->physicalParameters.pop_back();
    scalarSmallRecord->logicalParameters[1].physicalCount = 1;
    scalarSmallRecord->logicalParameters[2].firstPhysical = 2;
    scalarSmallRecord->physicalParameters.erase(scalarSmallRecord->physicalParameters.begin() + 2);
    scalarSmallRecord->logicalParameters[1].coercion = abi_plan::Type{abi_plan::TypeKind::Integer, 32};
    scalarSmallRecord->physicalParameters[1].type = *scalarSmallRecord->logicalParameters[1].coercion;
    recordFunction.declaration.parameters.resize(3);
    recordFunction.declaration.parameters[1].type.sizeBits = 32;
    recordFunction.declaration.parameters[1].type.alignmentBits = 32;
    recordFunction.declaration.abiPlan = scalarSmallRecord;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        abi_plan::isSysVScalarReturnWithRecordArguments(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol) &&
        restoredRecord.declaration.abiPlan->physicalParameters[1].type.widthOrCount == 32,
        "C dependency function retains a one-piece integer record with scalar return");
    abi_plan::Type vectorArgument{abi_plan::TypeKind::Vector, 2};
    vectorArgument.elements.push_back(abi_plan::Type{abi_plan::TypeKind::Float});
    scalarSmallRecord->logicalParameters[1].coercion = vectorArgument;
    scalarSmallRecord->physicalParameters[1].type = vectorArgument;
    recordFunction.declaration.parameters[1].type.sizeBits = 64;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        restoredRecord.declaration.abiPlan->physicalParameters[1].type.kind == abi_plan::TypeKind::Vector,
        "C dependency function retains a one-piece vector record with scalar return");
    recordFunction.declaration.parameters = savedScalarParameters;
    for (unsigned variant = 0; variant < 4; ++variant)
    {
        const bool isSigned = variant == 0 || variant == 2;
        const unsigned bits = variant < 2 ? 8 : 16;
        auto narrowPlan = std::make_shared<abi_plan::Plan>(*scalarReturnPlan);
        narrowPlan->physicalReturn = abi_plan::Type{abi_plan::TypeKind::Integer, bits};
        narrowPlan->logicalReturn.passing = abi_plan::Passing::Extend;
        narrowPlan->logicalReturn.coercion = narrowPlan->physicalReturn;
        narrowPlan->returnSignExtend = narrowPlan->logicalReturn.signExtend = isSigned;
        narrowPlan->returnZeroExtend = narrowPlan->logicalReturn.zeroExtend = !isSigned;
        auto& narrow = narrowPlan->logicalParameters[2];
        narrow.passing = abi_plan::Passing::Extend;
        narrow.coercion = abi_plan::Type{abi_plan::TypeKind::Integer, bits};
        narrow.signExtend = isSigned;
        narrow.zeroExtend = !isSigned;
        auto& physical = narrowPlan->physicalParameters[narrow.firstPhysical];
        physical.type = *narrow.coercion;
        physical.signExtend = isSigned;
        physical.zeroExtend = !isSigned;
        recordFunction.declaration.returnType.kind = isSigned ? CppTypeKind::SignedInteger : CppTypeKind::UnsignedInteger;
        recordFunction.declaration.returnType.sizeBits = bits;
        recordFunction.declaration.returnType.alignmentBits = bits;
        recordFunction.declaration.parameters[2].type.kind = recordFunction.declaration.returnType.kind;
        recordFunction.declaration.parameters[2].type.sizeBits = bits;
        recordFunction.declaration.parameters[2].type.alignmentBits = bits;
        recordFunction.declaration.abiPlan = narrowPlan;
        ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
            decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
            abi_plan::isSysVScalarReturnWithRecordArguments(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol) &&
            restoredRecord.declaration.abiPlan->returnSignExtend == isSigned &&
            restoredRecord.declaration.abiPlan->returnZeroExtend == !isSigned &&
            restoredRecord.declaration.abiPlan->physicalParameters[3].signExtend == isSigned &&
            restoredRecord.declaration.abiPlan->physicalParameters[3].zeroExtend == !isSigned,
            "C dependency function retains narrow signedness on return and argument");
        auto invalidNarrow = *narrowPlan;
        invalidNarrow.physicalParameters[3].signExtend = !isSigned;
        invalidNarrow.physicalParameters[3].zeroExtend = isSigned;
        ok &= expect(!abi_plan::isSysVScalarReturnWithRecordArguments(invalidNarrow, recordFunction.linkSymbol),
            "SysV record argument rejects an inconsistent narrow extension");
    }
    recordFunction.declaration.parameters = savedScalarParameters;
    for (const bool boolean : {true, false})
    {
        auto typedPlan = std::make_shared<abi_plan::Plan>(*scalarReturnPlan);
        typedPlan->physicalReturn = boolean ? abi_plan::Type{abi_plan::TypeKind::Integer, 1} : abi_plan::Type{abi_plan::TypeKind::Pointer};
        typedPlan->logicalReturn = {};
        typedPlan->logicalReturn.passing = boolean ? abi_plan::Passing::Extend : abi_plan::Passing::Direct;
        typedPlan->logicalReturn.coercion = typedPlan->physicalReturn;
        typedPlan->returnZeroExtend = typedPlan->logicalReturn.zeroExtend = boolean;
        auto& logical = typedPlan->logicalParameters[2];
        auto& physical = typedPlan->physicalParameters[logical.firstPhysical];
        logical.passing = boolean ? abi_plan::Passing::Extend : abi_plan::Passing::Direct;
        logical.coercion = typedPlan->physicalReturn;
        logical.zeroExtend = physical.zeroExtend = boolean;
        physical.type = typedPlan->physicalReturn;
        auto semantic = scalarReturnType;
        semantic.kind = boolean ? CppTypeKind::Boolean : CppTypeKind::Pointer;
        semantic.sizeBits = boolean ? 8 : 64;
        semantic.alignmentBits = boolean ? 8 : 64;
        if (!boolean) semantic.element = std::make_shared<CppType>(scalarReturnType);
        recordFunction.declaration.returnType = semantic;
        recordFunction.declaration.parameters[2].type = semantic;
        recordFunction.declaration.abiPlan = typedPlan;
        ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
            decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
            abi_plan::isSysVScalarReturnWithRecordArguments(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol) &&
            restoredRecord.declaration.abiPlan->physicalReturn.kind == typedPlan->physicalReturn.kind &&
            restoredRecord.declaration.abiPlan->returnZeroExtend == boolean &&
            restoredRecord.declaration.abiPlan->physicalParameters[3].zeroExtend == boolean,
            "C dependency function retains boolean or pointer return and argument");
        if (boolean)
        {
            auto invalidBoolean = *typedPlan;
            invalidBoolean.returnZeroExtend = invalidBoolean.logicalReturn.zeroExtend = false;
            invalidBoolean.returnSignExtend = invalidBoolean.logicalReturn.signExtend = true;
            ok &= expect(!abi_plan::isSysVScalarReturnWithRecordArguments(invalidBoolean, recordFunction.linkSymbol),
                "SysV record plan rejects signed extension of a boolean return");
        }
    }
    recordFunction.declaration.returnType = scalarReturnType;
    recordFunction.declaration.parameters = savedScalarParameters;
    for (const bool isSigned : {true, false})
    {
        auto underlying = scalarReturnType;
        underlying.kind = isSigned ? CppTypeKind::SignedInteger : CppTypeKind::UnsignedInteger;
        CppType enumeration;
        enumeration.kind = CppTypeKind::Enumeration;
        enumeration.sizeBits = 32;
        enumeration.alignmentBits = 32;
        enumeration.enumIdentity = CppEnumTypeIdentity{"T09Color", "T09Color", false, std::nullopt};
        enumeration.enumUnderlying = std::make_shared<CppType>(underlying);
        recordFunction.declaration.returnType = enumeration;
        recordFunction.declaration.parameters[2].type = enumeration;
        recordFunction.declaration.abiPlan = scalarReturnPlan;
        ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
            decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
            restoredRecord.declaration.returnType.enumUnderlying &&
            restoredRecord.declaration.returnType.enumUnderlying->kind == underlying.kind &&
            restoredRecord.declaration.abiPlan->physicalReturn.kind == abi_plan::TypeKind::Integer &&
            restoredRecord.declaration.abiPlan->physicalReturn.widthOrCount == 32,
            "C dependency function retains enum underlying type with record arguments");
        recordFunction.declaration.returnType.enumUnderlying.reset();
        unchangedRecord = {42};
        ok &= expect(!encodeCDependencyFunction(recordFunction, unchangedRecord, error) && unchangedRecord == std::vector<std::uint8_t>{42},
            "C dependency function rejects enum without an underlying type");
    }
    recordFunction.declaration.returnType = scalarReturnType;
    recordFunction.declaration.parameters = savedScalarParameters;
    auto invalidScalarPlan = *scalarReturnPlan;
    invalidScalarPlan.physicalParameters[4].byValue = false;
    ok &= expect(!abi_plan::isSysVScalarReturnWithRecordArguments(invalidScalarPlan, recordFunction.linkSymbol),
        "scalar SysV plan rejects a missing byval attribute");
    invalidScalarPlan = *scalarReturnPlan;
    invalidScalarPlan.physicalReturn.widthOrCount = 64;
    ok &= expect(!abi_plan::isSysVScalarReturnWithRecordArguments(invalidScalarPlan, recordFunction.linkSymbol),
        "scalar SysV plan rejects a conflicting return width");
    for (unsigned variant = 0; variant < 6; ++variant)
    {
        auto changedReturn = std::make_shared<abi_plan::Plan>(*scalarReturnPlan);
        const bool noReturn = variant == 4;
        const bool floating = variant == 2 || variant == 3;
        const unsigned bits = variant == 0 || variant == 3 || variant == 5 ? 64 : 32;
        changedReturn->physicalReturn = noReturn ? abi_plan::Type{abi_plan::TypeKind::Void} :
            floating ? abi_plan::Type{bits == 32 ? abi_plan::TypeKind::Float : abi_plan::TypeKind::Double} :
                       abi_plan::Type{abi_plan::TypeKind::Integer, bits};
        changedReturn->logicalReturn = {};
        changedReturn->logicalReturn.passing = noReturn ? abi_plan::Passing::Ignore : abi_plan::Passing::Direct;
        if (!noReturn) changedReturn->logicalReturn.coercion = changedReturn->physicalReturn;
        recordFunction.declaration.returnType.kind = noReturn ? CppTypeKind::Void : floating ? CppTypeKind::Floating :
            variant == 1 || variant == 5 ? CppTypeKind::UnsignedInteger : CppTypeKind::SignedInteger;
        recordFunction.declaration.returnType.sizeBits = noReturn ? std::nullopt : std::optional<std::uint64_t>{bits};
        recordFunction.declaration.returnType.alignmentBits = noReturn ? std::nullopt : std::optional<std::uint64_t>{bits};
        recordFunction.declaration.abiPlan = changedReturn;
        ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
            decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
            abi_plan::isSysVScalarReturnWithRecordArguments(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol) &&
            restoredRecord.declaration.abiPlan->physicalReturn.kind == changedReturn->physicalReturn.kind &&
            restoredRecord.declaration.abiPlan->physicalReturn.widthOrCount == changedReturn->physicalReturn.widthOrCount,
            "C dependency function retains scalar or void return with record arguments");
    }
    recordFunction.declaration.returnType = savedReturnType;
    auto inconsistentIndirectPlan = *mixedIndirectPlan;
    inconsistentIndirectPlan.physicalParameters[2].type.kind = abi_plan::TypeKind::Float;
    ok &= expect(!abi_plan::isSysVIndirectRecordWithArguments(inconsistentIndirectPlan, recordFunction.linkSymbol),
        "indirect SysV plan rejects inconsistent mixed piece classification");
    auto integerDoublePlan = std::make_shared<abi_plan::Plan>(*mixedIndirectPlan);
    integerDoublePlan->logicalParameters[1].coercion->elements = {{abi_plan::TypeKind::Integer, 64}, {abi_plan::TypeKind::Double}};
    integerDoublePlan->physicalParameters[2].type = integerDoublePlan->logicalParameters[1].coercion->elements[0];
    integerDoublePlan->physicalParameters[3].type = integerDoublePlan->logicalParameters[1].coercion->elements[1];
    recordFunction.declaration.abiPlan = integerDoublePlan;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        restoredRecord.declaration.abiPlan->physicalParameters[2].type.kind == abi_plan::TypeKind::Integer &&
        restoredRecord.declaration.abiPlan->physicalParameters[3].type.kind == abi_plan::TypeKind::Double,
        "C dependency function retains integer and double direct pieces with sret");
    auto vectorFloatWithIndirectPlan = std::make_shared<abi_plan::Plan>(*mixedIndirectPlan);
    abi_plan::Type vectorPiece{abi_plan::TypeKind::Vector, 2};
    vectorPiece.elements.push_back(abi_plan::Type{abi_plan::TypeKind::Float});
    vectorFloatWithIndirectPlan->logicalParameters[1].coercion->elements = {vectorPiece, abi_plan::Type{abi_plan::TypeKind::Float}};
    vectorFloatWithIndirectPlan->physicalParameters[2].type = vectorPiece;
    vectorFloatWithIndirectPlan->physicalParameters[3].type = abi_plan::Type{abi_plan::TypeKind::Float};
    recordFunction.declaration.parameters[1].type.sizeBits = 96;
    recordFunction.declaration.parameters[1].type.alignmentBits = 32;
    recordFunction.declaration.abiPlan = vectorFloatWithIndirectPlan;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        restoredRecord.declaration.abiPlan->physicalParameters[2].type.kind == abi_plan::TypeKind::Vector &&
        restoredRecord.declaration.abiPlan->physicalParameters[3].type.kind == abi_plan::TypeKind::Float,
        "C dependency function retains vector and float direct pieces with sret");
    auto corruptedPieceBytes = recordBytes;
    if (corruptedPieceBytes.size() >= 8) corruptedPieceBytes[corruptedPieceBytes.size() - 6] = 255;
    ok &= expect(corruptedPieceBytes.size() >= 8 &&
        !decodeCDependencyFunction(corruptedPieceBytes, restoredRecord, error),
        "C dependency function rejects an invalid direct piece descriptor");
    auto inconsistentVectorPlan = *vectorFloatWithIndirectPlan;
    inconsistentVectorPlan.physicalParameters[2].type.elements[0].widthOrCount = 1;
    ok &= expect(!abi_plan::isSysVIndirectRecordWithArguments(inconsistentVectorPlan, recordFunction.linkSymbol),
        "indirect SysV plan rejects an inconsistent vector element");
    inconsistentVectorPlan = *vectorFloatWithIndirectPlan;
    inconsistentVectorPlan.logicalParameters[1].byValue = true;
    ok &= expect(!abi_plan::isSysVIndirectRecordWithArguments(inconsistentVectorPlan, recordFunction.linkSymbol),
        "indirect SysV plan rejects direct pieces with indirect attributes");
    auto mixedWithoutByval = std::make_shared<abi_plan::Plan>(*mixedIndirectPlan);
    mixedWithoutByval->logicalParameters.pop_back();
    mixedWithoutByval->physicalParameters.pop_back();
    recordFunction.declaration.parameters.resize(3);
    recordFunction.declaration.parameters[1].type.sizeBits = 128;
    recordFunction.declaration.parameters[1].type.alignmentBits = 64;
    recordFunction.declaration.abiPlan = mixedWithoutByval;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        abi_plan::isSysVIndirectRecordWithArguments(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol) &&
        restoredRecord.declaration.abiPlan->logicalParameters.size() == 3,
        "C dependency function retains an indirect return without byval arguments");
    auto doubleI64WithoutByval = std::make_shared<abi_plan::Plan>(*mixedWithoutByval);
    doubleI64WithoutByval->logicalParameters[1].coercion->elements[1].widthOrCount = 64;
    doubleI64WithoutByval->physicalParameters[3].type.widthOrCount = 64;
    recordFunction.declaration.abiPlan = doubleI64WithoutByval;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        restoredRecord.declaration.abiPlan->physicalParameters[3].type.widthOrCount == 64,
        "C dependency function retains double and i64 record pieces with indirect return");
    auto singleWithoutByval = std::make_shared<abi_plan::Plan>(*mixedWithoutByval);
    singleWithoutByval->logicalParameters[1].physicalCount = 1;
    singleWithoutByval->logicalParameters[2].firstPhysical = 3;
    singleWithoutByval->physicalParameters.erase(singleWithoutByval->physicalParameters.begin() + 3);
    for (const auto kind : {abi_plan::TypeKind::Float, abi_plan::TypeKind::Double, abi_plan::TypeKind::Vector})
    {
        abi_plan::Type piece{kind, kind == abi_plan::TypeKind::Vector ? 2U : 0U};
        if (kind == abi_plan::TypeKind::Vector) piece.elements.push_back(abi_plan::Type{abi_plan::TypeKind::Float});
        singleWithoutByval->logicalParameters[1].coercion = piece;
        singleWithoutByval->physicalParameters[2].type = piece;
        recordFunction.declaration.parameters[1].type.sizeBits = kind == abi_plan::TypeKind::Float ? 32 : 64;
        recordFunction.declaration.parameters[1].type.alignmentBits = kind == abi_plan::TypeKind::Double ? 64 : 32;
        recordFunction.declaration.abiPlan = singleWithoutByval;
        ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
            decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
            restoredRecord.declaration.abiPlan->physicalParameters[2].type.kind == kind,
            "C dependency function retains a single direct floating or vector record piece");
    }
    recordFunction.declaration.returnType.sizeBits = 192;
    recordFunction.declaration.returnType.alignmentBits = 8;
    auto sretOnly = std::make_shared<abi_plan::Plan>(*indirectPlan);
    sretOnly->logicalParameters.clear();
    sretOnly->physicalParameters.resize(1);
    recordFunction.declaration.parameters.clear();
    recordFunction.declaration.abiPlan = sretOnly;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        abi_plan::isSysVIndirectRecordWithArguments(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol) &&
        restoredRecord.declaration.abiPlan->physicalParameters.size() == 1,
        "C dependency function retains an indirect return without parameters");
    recordFunction.declaration.parameters.resize(1);
    recordFunction.declaration.parameters[0] = indirectRecordParameter;
    recordFunction.declaration.abiPlan = indirectPlan;
    recordFunction.declaration.returnType.alignmentBits = 32;
    unchangedRecord = {42};
    ok &= expect(!encodeCDependencyFunction(recordFunction, unchangedRecord, error) &&
        unchangedRecord == std::vector<std::uint8_t>{42}, "indirect SysV plan rejects a conflicting record alignment");
    for (const std::uint64_t alignment : {2U, 4U, 8U, 16U, 32U, 64U, 128U, 256U})
    {
        auto alignedPlan = std::make_shared<abi_plan::Plan>(*indirectPlan);
        alignedPlan->logicalReturn.alignment = alignment;
        alignedPlan->physicalParameters[0].alignment = alignment;
        alignedPlan->logicalParameters[0].alignment = alignment < 8 ? 8 : alignment;
        alignedPlan->physicalParameters[1].alignment = alignment < 8 ? 8 : alignment;
        recordFunction.declaration.abiPlan = alignedPlan;
        recordFunction.declaration.returnType.sizeBits = alignment >= 16 ? (alignment < 32 ? 256 : alignment * 8) : 192;
        recordFunction.declaration.parameters[0].type.sizeBits = alignment >= 16 ? (alignment < 32 ? 256 : alignment * 8) : 192;
        recordFunction.declaration.returnType.alignmentBits = alignment * 8;
        recordFunction.declaration.parameters[0].type.alignmentBits = alignment * 8;
        ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
            decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
            abi_plan::isSysVIndirectRecordRoundTrip(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol) &&
            restoredRecord.declaration.abiPlan->logicalReturn.alignment == alignment,
            "indirect SysV dependency retains the return alignment");
    }
    auto mixedPlan = std::make_shared<abi_plan::Plan>(*pairPlan);
    const abi_plan::Type doubleType{abi_plan::TypeKind::Double};
    const abi_plan::Type int32Type{abi_plan::TypeKind::Integer, 32};
    mixedPlan->physicalReturn.elements = {doubleType, int32Type};
    mixedPlan->logicalReturn.coercion = mixedPlan->physicalReturn;
    mixedPlan->logicalParameters[0].coercion = mixedPlan->physicalReturn;
    mixedPlan->physicalParameters[0].type = doubleType;
    mixedPlan->physicalParameters[1].type = int32Type;
    recordFunction.declaration.abiPlan = mixedPlan;
    recordFunction.declaration.returnType.sizeBits = 128;
    recordFunction.declaration.returnType.alignmentBits = 64;
    recordFunction.declaration.parameters[0].type.sizeBits = 128;
    recordFunction.declaration.parameters[0].type.alignmentBits = 64;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        abi_plan::isSysVMixedDoubleI32RecordRoundTrip(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol),
        "C dependency function retains a mixed double and integer SysV plan");
    auto reversePlan = std::make_shared<abi_plan::Plan>(*mixedPlan);
    const abi_plan::Type int64Type{abi_plan::TypeKind::Integer, 64};
    reversePlan->physicalReturn.elements = {int64Type, doubleType};
    reversePlan->logicalReturn.coercion = reversePlan->physicalReturn;
    reversePlan->logicalParameters[0].coercion = reversePlan->physicalReturn;
    reversePlan->physicalParameters[0].type = int64Type;
    reversePlan->physicalParameters[1].type = doubleType;
    recordFunction.declaration.abiPlan = reversePlan;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        abi_plan::isSysVMixedRecordRoundTrip(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol) &&
        !abi_plan::isSysVMixedDoubleI32RecordRoundTrip(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol),
        "C dependency function retains an integer and double SysV plan");
    auto doublePairPlan = std::make_shared<abi_plan::Plan>(*reversePlan);
    doublePairPlan->physicalReturn.elements = {doubleType, doubleType};
    doublePairPlan->logicalReturn.coercion = doublePairPlan->physicalReturn;
    doublePairPlan->logicalParameters[0].coercion = doublePairPlan->physicalReturn;
    doublePairPlan->physicalParameters[0].type = doubleType;
    doublePairPlan->physicalParameters[1].type = doubleType;
    recordFunction.declaration.abiPlan = doublePairPlan;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        abi_plan::isSysVMixedRecordRoundTrip(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol) &&
        restoredRecord.declaration.abiPlan->physicalReturn.elements[0].kind == abi_plan::TypeKind::Double,
        "C dependency function retains a pair of doubles SysV plan");
    auto doubleFloatPlan = std::make_shared<abi_plan::Plan>(*doublePairPlan);
    const abi_plan::Type floatForDouble{abi_plan::TypeKind::Float};
    doubleFloatPlan->physicalReturn.elements[1] = floatForDouble;
    doubleFloatPlan->logicalReturn.coercion = doubleFloatPlan->physicalReturn;
    doubleFloatPlan->logicalParameters[0].coercion = doubleFloatPlan->physicalReturn;
    doubleFloatPlan->physicalParameters[1].type = floatForDouble;
    recordFunction.declaration.abiPlan = doubleFloatPlan;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        abi_plan::isSysVMixedRecordRoundTrip(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol) &&
        restoredRecord.declaration.abiPlan->physicalReturn.elements[1].kind == abi_plan::TypeKind::Float,
        "C dependency function retains a double and float SysV plan");
    auto floatDoublePlan = std::make_shared<abi_plan::Plan>(*doublePairPlan);
    floatDoublePlan->physicalReturn.elements[0] = floatForDouble;
    floatDoublePlan->logicalReturn.coercion = floatDoublePlan->physicalReturn;
    floatDoublePlan->logicalParameters[0].coercion = floatDoublePlan->physicalReturn;
    floatDoublePlan->physicalParameters[0].type = floatForDouble;
    recordFunction.declaration.abiPlan = floatDoublePlan;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        abi_plan::isSysVMixedRecordRoundTrip(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol) &&
        restoredRecord.declaration.abiPlan->physicalReturn.elements[0].kind == abi_plan::TypeKind::Float,
        "C dependency function retains a float and double SysV plan");
    auto floatPlan = std::make_shared<abi_plan::Plan>(*reversePlan);
    const abi_plan::Type floatType{abi_plan::TypeKind::Float};
    floatPlan->physicalReturn.elements[1] = floatType;
    floatPlan->logicalReturn.coercion = floatPlan->physicalReturn;
    floatPlan->logicalParameters[0].coercion = floatPlan->physicalReturn;
    floatPlan->physicalParameters[1].type = floatType;
    recordFunction.declaration.abiPlan = floatPlan;
    recordFunction.declaration.returnType.sizeBits = 96;
    recordFunction.declaration.returnType.alignmentBits = 32;
    recordFunction.declaration.parameters[0].type.sizeBits = 96;
    recordFunction.declaration.parameters[0].type.alignmentBits = 32;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        abi_plan::isSysVMixedRecordRoundTrip(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol) &&
        restoredRecord.declaration.abiPlan->physicalReturn.elements[1].kind == abi_plan::TypeKind::Float,
        "C dependency function retains an integer and float SysV plan");
    auto vectorPlan = std::make_shared<abi_plan::Plan>(*floatPlan);
    abi_plan::Type vectorType{abi_plan::TypeKind::Vector, 2};
    vectorType.elements.push_back(floatType);
    vectorPlan->physicalReturn.elements[1] = vectorType;
    vectorPlan->logicalReturn.coercion = vectorPlan->physicalReturn;
    vectorPlan->logicalParameters[0].coercion = vectorPlan->physicalReturn;
    vectorPlan->physicalParameters[1].type = vectorType;
    recordFunction.declaration.abiPlan = vectorPlan;
    recordFunction.declaration.returnType.sizeBits = 128;
    recordFunction.declaration.parameters[0].type.sizeBits = 128;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        abi_plan::isSysVMixedRecordRoundTrip(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol) &&
        restoredRecord.declaration.abiPlan->physicalReturn.elements[1].kind == abi_plan::TypeKind::Vector,
        "C dependency function retains a two-float vector SysV plan");
    auto vectorFirstPlan = std::make_shared<abi_plan::Plan>(*vectorPlan);
    vectorFirstPlan->physicalReturn.elements = {vectorType, int64Type};
    vectorFirstPlan->logicalReturn.coercion = vectorFirstPlan->physicalReturn;
    vectorFirstPlan->logicalParameters[0].coercion = vectorFirstPlan->physicalReturn;
    vectorFirstPlan->physicalParameters[0].type = vectorType;
    vectorFirstPlan->physicalParameters[1].type = int64Type;
    recordFunction.declaration.abiPlan = vectorFirstPlan;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        abi_plan::isSysVMixedRecordRoundTrip(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol) &&
        restoredRecord.declaration.abiPlan->physicalReturn.elements[0].kind == abi_plan::TypeKind::Vector,
        "C dependency function retains a vector-first SysV plan");
    for (const bool vectorFirst : {true, false})
    {
        auto vectorDoublePlan = std::make_shared<abi_plan::Plan>(*vectorFirstPlan);
        vectorDoublePlan->physicalReturn.elements = vectorFirst ?
            std::vector<abi_plan::Type>{vectorType, doubleType} : std::vector<abi_plan::Type>{doubleType, vectorType};
        vectorDoublePlan->logicalReturn.coercion = vectorDoublePlan->physicalReturn;
        vectorDoublePlan->logicalParameters[0].coercion = vectorDoublePlan->physicalReturn;
        vectorDoublePlan->physicalParameters[0].type = vectorDoublePlan->physicalReturn.elements[0];
        vectorDoublePlan->physicalParameters[1].type = vectorDoublePlan->physicalReturn.elements[1];
        recordFunction.declaration.abiPlan = vectorDoublePlan;
        recordFunction.declaration.returnType.sizeBits = 128;
        recordFunction.declaration.returnType.alignmentBits = 64;
        recordFunction.declaration.parameters[0].type.sizeBits = 128;
        recordFunction.declaration.parameters[0].type.alignmentBits = 64;
        ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
            decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
            abi_plan::isSysVMixedRecordRoundTrip(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol) &&
            (restoredRecord.declaration.abiPlan->physicalReturn.elements[0].kind == abi_plan::TypeKind::Vector) == vectorFirst,
            "C dependency function retains vector and double order");
    }
    recordFunction.declaration.returnType.alignmentBits = 32;
    recordFunction.declaration.parameters[0].type.alignmentBits = 32;
    auto vectorShortPlan = std::make_shared<abi_plan::Plan>(*vectorFirstPlan);
    vectorShortPlan->physicalReturn.elements[1] = int32Type;
    vectorShortPlan->logicalReturn.coercion = vectorShortPlan->physicalReturn;
    vectorShortPlan->logicalParameters[0].coercion = vectorShortPlan->physicalReturn;
    vectorShortPlan->physicalParameters[1].type = int32Type;
    recordFunction.declaration.abiPlan = vectorShortPlan;
    recordFunction.declaration.returnType.sizeBits = 96;
    recordFunction.declaration.parameters[0].type.sizeBits = 96;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        abi_plan::isSysVMixedRecordRoundTrip(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol) &&
        restoredRecord.declaration.abiPlan->physicalReturn.elements[1].widthOrCount == 32,
        "C dependency function retains a vector and i32 SysV plan");
    auto twoVectorPlan = std::make_shared<abi_plan::Plan>(*vectorFirstPlan);
    twoVectorPlan->physicalReturn.elements[1] = vectorType;
    twoVectorPlan->logicalReturn.coercion = twoVectorPlan->physicalReturn;
    twoVectorPlan->logicalParameters[0].coercion = twoVectorPlan->physicalReturn;
    twoVectorPlan->physicalParameters[1].type = vectorType;
    recordFunction.declaration.abiPlan = twoVectorPlan;
    recordFunction.declaration.returnType.sizeBits = 128;
    recordFunction.declaration.parameters[0].type.sizeBits = 128;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        abi_plan::isSysVMixedRecordRoundTrip(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol) &&
        restoredRecord.declaration.abiPlan->physicalReturn.elements[1].kind == abi_plan::TypeKind::Vector,
        "C dependency function retains a two-vector SysV plan");
    recordFunction.declaration.returnType.alignmentBits = 128;
    recordFunction.declaration.parameters[0].type.alignmentBits = 128;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        restoredRecord.declaration.returnType.alignmentBits == 128 &&
        abi_plan::isSysVMixedRecordRoundTrip(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol),
        "C dependency function retains sixteen-byte vector record alignment");
    recordFunction.declaration.returnType.alignmentBits = 32;
    recordFunction.declaration.parameters[0].type.alignmentBits = 32;
    auto vectorFloatPlan = std::make_shared<abi_plan::Plan>(*twoVectorPlan);
    vectorFloatPlan->physicalReturn.elements[1] = floatType;
    vectorFloatPlan->logicalReturn.coercion = vectorFloatPlan->physicalReturn;
    vectorFloatPlan->logicalParameters[0].coercion = vectorFloatPlan->physicalReturn;
    vectorFloatPlan->physicalParameters[1].type = floatType;
    recordFunction.declaration.abiPlan = vectorFloatPlan;
    recordFunction.declaration.returnType.sizeBits = 96;
    recordFunction.declaration.parameters[0].type.sizeBits = 96;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        abi_plan::isSysVMixedRecordRoundTrip(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol) &&
        restoredRecord.declaration.abiPlan->physicalReturn.elements[1].kind == abi_plan::TypeKind::Float,
        "C dependency function retains a vector and float SysV plan");
    auto singleVectorPlan = std::make_shared<abi_plan::Plan>(*vectorPlan);
    singleVectorPlan->physicalReturn = vectorType;
    singleVectorPlan->logicalReturn.coercion = vectorType;
    singleVectorPlan->logicalParameters[0].coercion = vectorType;
    singleVectorPlan->logicalParameters[0].physicalCount = 1;
    singleVectorPlan->physicalParameters.resize(1);
    singleVectorPlan->physicalParameters[0].type = vectorType;
    recordFunction.declaration.abiPlan = singleVectorPlan;
    recordFunction.declaration.returnType.sizeBits = 64;
    recordFunction.declaration.parameters[0].type.sizeBits = 64;
    ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
        decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
        abi_plan::isSysVSingleVector2FRecordRoundTrip(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol),
        "C dependency function retains a single-vector SysV plan");
    for (const bool isFloat : {true, false})
    {
        auto floatingPlan = std::make_shared<abi_plan::Plan>(*singleVectorPlan);
        const auto type = isFloat ? floatType : doubleType;
        floatingPlan->physicalReturn = type;
        floatingPlan->logicalReturn.coercion = type;
        floatingPlan->logicalParameters[0].coercion = type;
        floatingPlan->physicalParameters[0].type = type;
        recordFunction.declaration.abiPlan = floatingPlan;
        recordFunction.declaration.returnType.sizeBits = isFloat ? 32 : 64;
        recordFunction.declaration.returnType.alignmentBits = isFloat ? 32 : 64;
        recordFunction.declaration.parameters[0].type.sizeBits = isFloat ? 32 : 64;
        recordFunction.declaration.parameters[0].type.alignmentBits = isFloat ? 32 : 64;
        ok &= expect(encodeCDependencyFunction(recordFunction, recordBytes, error) &&
            decodeCDependencyFunction(recordBytes, restoredRecord, error) && restoredRecord.declaration.abiPlan &&
            abi_plan::isSysVSingleFloatingRecordRoundTrip(*restoredRecord.declaration.abiPlan, recordFunction.linkSymbol) &&
            restoredRecord.declaration.abiPlan->physicalReturn.kind == type.kind,
            "C dependency function retains a single floating SysV plan");
    }
    if (!functionBytes.empty()) functionBytes.pop_back();
    decoded.moduleName = "sentinel";
    ok &= expect(!decodeCDependencyFunction(functionBytes, decoded, error) && decoded.moduleName == "sentinel",
        "truncated C dependency function preserves output on failure");
    CDependencyGlobalSnapshot global;
    global.moduleName = "Producer";
    global.referenceIdentity = "header-digest";
    global.linkSymbol = "c_global";
    global.microsoftVolatileSemantics = false;
    global.declaration.name = "c_global";
    global.declaration.qualifiedName = "c_global";
    global.declaration.externalLinkage = true;
    global.declaration.externalSymbol = "c_global";
    global.declaration.clangUsr = "c:@c_global";
    global.declaration.threadLocal = true;
    global.declaration.declarationAlignmentBits = 64;
    global.declaration.type = pointer;
    nativesharp::frontend::NamespaceStateSymbol selectedGlobal;
    selectedGlobal.projectOrdinal = 4;
    selectedGlobal.moduleName = "Producer";
    selectedGlobal.name = "c_global";
    selectedGlobal.storageName = "c_global";
    selectedGlobal.isCppExternal = true;
    selectedGlobal.cppCGlobal = true;
    selectedGlobal.cppThreadLocal = true;
    selectedGlobal.cppDllImport = true;
    selectedGlobal.cppClangUsr = global.declaration.clangUsr;
    selectedGlobal.cppMicrosoftVolatileSemantics = false;
    std::vector<CppSemanticGlobalEntry> globalInventory{{{}, 2, 4, "x86_64-pc-windows-msvc",
        global.declaration, CppSemanticGlobalFailure{"not used"}, true, false}};
    CDependencyGlobalSnapshot capturedGlobal;
    ok &= expect(captureCDependencyGlobal(globalInventory, selectedGlobal, 2, "header-digest", capturedGlobal, error) &&
        capturedGlobal.linkSymbol == selectedGlobal.storageName && capturedGlobal.declaration.threadLocal &&
        capturedGlobal.effectiveDllImport && !capturedGlobal.declaration.dllImport,
        "C dependency capture retains TLS DLL import inferred from the project reference");
    global.effectiveDllImport = true;
    capturedGlobal.moduleName = "sentinel";
    ok &= expect(!captureCDependencyGlobal(globalInventory, selectedGlobal, 3, "header-digest", capturedGlobal, error) &&
        capturedGlobal.moduleName == "sentinel", "C global capture rejects another reference without changing output");
    globalInventory.push_back(globalInventory.front());
    ok &= expect(!captureCDependencyGlobal(globalInventory, selectedGlobal, 2, "header-digest", capturedGlobal, error) &&
        capturedGlobal.moduleName == "sentinel", "C global capture rejects ambiguous observations");
    std::vector<std::uint8_t> globalBytes;
    CDependencyGlobalSnapshot decodedGlobal;
    ok &= expect(encodeCDependencyGlobal(global, globalBytes, error) &&
        decodeCDependencyGlobal(globalBytes, decodedGlobal, error) &&
        decodedGlobal.microsoftVolatileSemantics == std::optional<bool>{false} &&
        decodedGlobal.declaration.threadLocal && decodedGlobal.effectiveDllImport &&
        !decodedGlobal.declaration.dllImport && decodedGlobal.declaration.declarationAlignmentBits == 64 &&
        decodedGlobal.declaration.clangUsr == global.declaration.clangUsr &&
        decodedGlobal.declaration.type.element && decodedGlobal.declaration.type.element->kind == CppTypeKind::Atomic,
        "C dependency global preserves identity, TLS, alignment, mode and type");
    if (!globalBytes.empty()) globalBytes.pop_back();
    decodedGlobal.moduleName = "sentinel";
    ok &= expect(!decodeCDependencyGlobal(globalBytes, decodedGlobal, error) && decodedGlobal.moduleName == "sentinel",
        "truncated C dependency global preserves output on failure");
    CDependencyAliasSnapshot alias;
    alias.moduleName = "Producer";
    alias.referenceIdentity = "header-digest";
    alias.declaration.name = "CAlias";
    alias.declaration.qualifiedName = "CAlias";
    alias.declaration.clangUsr = "c:test.h@T@CAlias";
    alias.declaration.underlyingType = pointer;
    nativesharp::frontend::CppImportedTypeAlias selectedAlias{4, {}, 2, "x86_64-pc-windows-msvc", alias.declaration};
    std::vector<CppSemanticTypeAliasEntry> aliasInventory{{{}, 2, 4, "x86_64-pc-windows-msvc", alias.declaration}};
    CDependencyAliasSnapshot capturedAlias;
    ok &= expect(captureCDependencyAlias(aliasInventory, selectedAlias, "Producer", "header-digest", capturedAlias, error) &&
        capturedAlias.declaration.clangUsr == alias.declaration.clangUsr,
        "C alias capture retains the selected reference identity");
    capturedAlias.moduleName = "sentinel";
    selectedAlias.referenceIndex = 3;
    ok &= expect(!captureCDependencyAlias(aliasInventory, selectedAlias, "Producer", "header-digest", capturedAlias, error) &&
        capturedAlias.moduleName == "sentinel", "C alias capture rejects another reference without changing output");
    selectedAlias.referenceIndex = 2;
    aliasInventory.push_back(aliasInventory.front());
    ok &= expect(!captureCDependencyAlias(aliasInventory, selectedAlias, "Producer", "header-digest", capturedAlias, error) &&
        capturedAlias.moduleName == "sentinel", "C alias capture rejects ambiguous observations");
    std::vector<std::uint8_t> aliasBytes;
    CDependencyAliasSnapshot decodedAlias;
    ok &= expect(encodeCDependencyAlias(alias, aliasBytes, error) &&
        decodeCDependencyAlias(aliasBytes, decodedAlias, error) &&
        decodedAlias.declaration.clangUsr == alias.declaration.clangUsr &&
        decodedAlias.declaration.underlyingType.element &&
        decodedAlias.declaration.underlyingType.element->kind == CppTypeKind::Atomic,
        "C dependency alias preserves identity and underlying type");
    if (!aliasBytes.empty()) aliasBytes.pop_back();
    decodedAlias.moduleName = "sentinel";
    ok &= expect(!decodeCDependencyAlias(aliasBytes, decodedAlias, error) && decodedAlias.moduleName == "sentinel",
        "truncated C dependency alias preserves output on failure");
    CDependencyCatalog catalogue;
    catalogue.functions.push_back(function);
    catalogue.globals.push_back(global);
    catalogue.aliases.push_back(alias);
    std::vector<std::uint8_t> catalogueBytes;
    CDependencyCatalog restoredCatalogue;
    ok &= expect(encodeCDependencyCatalog(catalogue, catalogueBytes, error) &&
        decodeCDependencyCatalog(catalogueBytes, restoredCatalogue, error) &&
        restoredCatalogue.functions.size() == 1 && restoredCatalogue.globals.size() == 1 &&
        restoredCatalogue.aliases.size() == 1 &&
        restoredCatalogue.functions[0].linkSymbol == function.linkSymbol &&
        restoredCatalogue.globals[0].declaration.threadLocal &&
        restoredCatalogue.aliases[0].declaration.clangUsr == alias.declaration.clangUsr,
        "C dependency catalogue preserves three declaration categories");
    ok &= expect(!catalogueBytes.empty() && catalogueBytes[0] == 1,
        "C dependency catalogue without enumerators retains version one");
    auto withEnumerator = catalogue;
    withEnumerator.enumerators.push_back({"TestModule", "cpp-reference:0", "CAnonValue",
        "c:@Ea@CAnonValue@CAnonValue", "builtin:int", 42});
    std::vector<std::uint8_t> enumeratorBytes;
    CDependencyCatalog restoredEnumerator;
    ok &= expect(encodeCDependencyCatalog(withEnumerator, enumeratorBytes, error) &&
        !enumeratorBytes.empty() && enumeratorBytes[0] == 2 &&
        decodeCDependencyCatalog(enumeratorBytes, restoredEnumerator, error) &&
        restoredEnumerator.enumerators.size() == 1 &&
        restoredEnumerator.enumerators[0].name == "CAnonValue" &&
        restoredEnumerator.enumerators[0].clangUsr == "c:@Ea@CAnonValue@CAnonValue" &&
        restoredEnumerator.enumerators[0].typeKey == "builtin:int" &&
        restoredEnumerator.enumerators[0].bits == 42,
        "C dependency catalogue preserves an anonymous enumerator in version two");
    const std::vector<CppCEnumSnapshot> sourceEnums{syntheticEnumSnapshot()};
    std::vector<std::uint8_t> enumeratorEnvelope;
    std::vector<CppCRecordSnapshot> enumVersionRecords;
    std::vector<CppCEnumSnapshot> enumVersionEnums;
    CDependencyCatalog enumVersionDependencies;
    ok &= expect(encodeCTypeCatalogWithDependencies({}, {}, withEnumerator, enumeratorEnvelope, error) &&
        !decodeCTypeCatalogAndDependencies(enumeratorEnvelope, 46, enumVersionRecords, enumVersionEnums,
            enumVersionDependencies, error) && enumVersionDependencies.enumerators.empty() &&
        decodeCTypeCatalogAndDependencies(enumeratorEnvelope, 47, enumVersionRecords, enumVersionEnums,
            enumVersionDependencies, error) && enumVersionDependencies.enumerators.size() == 1,
        "anonymous enumerator dependencies require minor 47");
    std::vector<std::uint8_t> envelope;
    std::vector<CppCRecordSnapshot> restoredRecords;
    std::vector<CppCEnumSnapshot> restoredEnums;
    CDependencyCatalog restoredDependencies;
    ok &= expect(encodeCTypeCatalogWithDependencies({}, sourceEnums, catalogue, envelope, error) &&
        decodeCTypeCatalogWithDependencies(envelope, restoredRecords, restoredEnums, restoredDependencies, error) &&
        restoredEnums.size() == 1 && restoredEnums[0].identityKey == sourceEnums[0].identityKey &&
        restoredDependencies.functions.size() == 1 && restoredDependencies.globals.size() == 1 &&
        restoredDependencies.aliases.size() == 1, "C dependency envelope preserves types and declarations");
    CDependencyCatalog dispatched;
    std::vector<CppCRecordSnapshot> dispatchedRecords;
    std::vector<CppCEnumSnapshot> dispatchedEnums;
    ok &= expect(decodeCTypeCatalogAndDependencies(envelope, 46, dispatchedRecords, dispatchedEnums,
        dispatched, error) && dispatched.functions.size() == 1 && dispatchedEnums.size() == 1,
        "C dependency envelope dispatch accepts explicit future minor");
    dispatchedEnums.clear();
    ok &= expect(decodeVersionedCTypeCatalog(envelope, dispatchedRecords, dispatchedEnums, error, 46) &&
        dispatchedEnums.size() == 1 && dispatchedEnums[0].identityKey == sourceEnums[0].identityKey,
        "versioned type reader extracts enums from the dependency envelope");
    dispatchedEnums.clear();
    ok &= expect(!decodeVersionedCTypeCatalog(envelope, dispatchedRecords, dispatchedEnums, error, 45) &&
        dispatchedEnums.empty(), "versioned type reader does not accept the new envelope under minor 45");
    dispatched.functions.clear();
    ok &= expect(!decodeCTypeCatalogAndDependencies(envelope, 45, dispatchedRecords, dispatchedEnums,
        dispatched, error) && dispatched.functions.empty(),
        "C dependency envelope dispatch rejects an older minor without changing outputs");
    std::vector<std::uint8_t> legacy;
    ok &= expect(encodeCTypeCatalog({}, sourceEnums, legacy, error) &&
        decodeCTypeCatalogAndDependencies(legacy, 42, dispatchedRecords, dispatchedEnums, dispatched, error) &&
        dispatchedEnums.size() == 1 && dispatched.functions.empty(),
        "C dependency dispatch preserves legacy enum-only catalogue");
    if (!envelope.empty()) envelope.pop_back();
    restoredDependencies.functions.clear();
    ok &= expect(!decodeCTypeCatalogWithDependencies(envelope, restoredRecords, restoredEnums,
        restoredDependencies, error) && restoredDependencies.functions.empty(),
        "truncated C dependency envelope preserves outputs");
    catalogue.functions.push_back(function);
    std::vector<std::uint8_t> unchangedCatalogue{42};
    ok &= expect(!encodeCDependencyCatalog(catalogue, unchangedCatalogue, error) &&
        unchangedCatalogue == std::vector<std::uint8_t>{42},
        "C dependency catalogue rejects duplicate snapshots without changing output");
    if (!catalogueBytes.empty()) catalogueBytes.pop_back();
    restoredCatalogue.functions.clear();
    ok &= expect(!decodeCDependencyCatalog(catalogueBytes, restoredCatalogue, error) &&
        restoredCatalogue.functions.empty(), "truncated C dependency catalogue preserves output");
    return ok;
}

// EN: Entry of the probe mode. Writes a.h and b.h with identical text and runs probes 1-7.
// ES: Entrada del modo de sondas. Escribe a.h y b.h con texto idéntico y ejecuta las sondas 1-7.
int runProbes(const std::string& resource, const std::filesystem::path& work, const std::string& driver)
{
    const auto directory = std::filesystem::absolute(work / "c-identity");
    std::filesystem::create_directories(directory);
    if (!writeText(directory / "a.h", kProbeHeader) || !writeText(directory / "b.h", kProbeHeader))
    {
        std::cerr << "cannot write probe headers beneath " << directory.string() << '\n';
        return 2;
    }
    CppClangSessionFactory factory;
    bool ok = checkUsrStability(factory, resource, directory, driver);
    ok &= checkTwoHeaderProject(factory, resource, directory, driver);
    ok &= checkTwoSessionAlias(factory, resource, directory, driver);
    observeAnonymousEnumCollision(factory, resource, directory, driver);
    ok &= checkHomonymHeaders(factory, resource, directory, driver);
    ok &= checkNestedFieldEnums(factory, resource, directory, driver);
    ok &= checkDependencyTypeCodec();
    ok &= checkGenericForeignCandidates(factory, resource, directory, driver);
    ok &= checkGenericAliasPublication(factory, resource, directory, driver);
    observeCatalogueEnvelope();
    std::cout << (ok ? "C declaration identity probes passed\n" : "C declaration identity probes failed\n");
    return ok ? 0 : 1;
}

// EN: Builds the consumer Main.ns that demands every enumerator stored in the catalogue, so that
//     the consumer value provider restores all of them. Enumerator names come from Clang.
// ES: Construye el Main.ns consumidor que demanda cada enumerador guardado en el catálogo, para que
//     el proveedor de valores del consumidor los restaure todos. Los nombres vienen de Clang.
std::string consumerSource(const std::vector<CppCEnumSnapshot>& enums)
{
    std::string body = "int Main()\n{\n";
    for (const auto& enumeration : enums)
    {
        for (const auto& value : enumeration.declaration.enumerators)
        {
            body += "    if (" + value.name + " != " + value.name + ") return 1;\n";
        }
    }
    return body + "    return 0;\n}\n";
}

// EN: Configures a consumer compilation exactly like the driver does for precompiled C catalogues
//     (selector, field provider and value provider) and compiles its members.
// ES: Configura una compilación consumidora igual que el driver con catálogos C precompilados
//     (selector, proveedor de campos y de valores) y compila sus miembros.
bool compileConsumer(nativesharp::frontend::ProjectCompilation& consumer,
                     nativesharp::frontend::PrecompiledSyntaxModule module,
                     const std::filesystem::path& project, std::string& error)
{
    const bool configured = consumer.setPrecompiledModules({std::move(module)}, error) &&
        consumer.setExperimentalCppTypeSelector(selectCppTypesWithPrecompiledCatalogs, error) &&
        consumer.setExperimentalCppInstanceFieldProvider(
            [&consumer](const auto& graph, auto projects, const auto& names, auto& out, auto& why) {
                return provideCppFieldsWithPrecompiledCatalogs(graph, projects, consumer.precompiledModules(),
                                                               names, out, why);
            }, error) &&
        consumer.setCppGlobalProvider(
            [&consumer](const auto& graph, auto projects, auto wanted, auto& out, auto& why) {
                const auto* names = consumer.nameLookup();
                if (!names) { why = "consumer name lookup is unavailable"; return false; }
                return provideCppValuesWithPrecompiledCatalogs(graph, projects, consumer.precompiledModules(),
                                                               *names, wanted, out, why);
            }, error) &&
        consumer.enableCppImportedFieldLowering(error);
    return configured && consumer.compileProjectMembers(project, error);
}

// EN: Compares restored record and field USRs with the live Clang records of the same header.
// ES: Compara los USR restaurados de records y campos con los records vivos de Clang del encabezado.
bool checkRestoredRecords(const nativesharp::frontend::ProjectCompilation& consumer,
                          const std::vector<CppCRecordSnapshot>& stored, const std::vector<CppRecordDeclaration>& live)
{
    bool ok = true;
    const auto* lookup = consumer.nameLookup();
    const auto* members = consumer.memberModel();
    if (!expect(lookup && members, "consumer name lookup and member model exist")) return false;
    for (const auto& record : stored)
    {
        const auto clang = std::find_if(live.begin(), live.end(), [&](const auto& item) {
            return item.qualifiedName == record.name;
        });
        const auto ordinal = lookup->typeOrdinalForIdentity(record.identityKey);
        const auto* restored = ordinal ? lookup->cppImportedType(*ordinal) : nullptr;
        const bool known = clang != live.end() && clang->clangUsr && !clang->clangUsr->empty();
        ok &= expect(known && restored && restored->clangUsr == clang->clangUsr,
                     "consumer restores the Clang USR of C record " + record.name);
        if (!known || !ordinal || !restored) continue;
        std::cout << "RESTORED record " << record.name << " usr=" << restored->clangUsr.value_or("<absent>") << '\n';
        for (const auto& field : clang->fields)
        {
            const auto indices = members->findMembers(*ordinal, field.name);
            const bool matched = indices.size() == 1 && field.clangUsr &&
                members->members()[indices.front()].cppClangUsr == field.clangUsr;
            ok &= expect(matched, "consumer restores the Clang USR of C field " + record.name + "." + field.name);
            if (matched)
            {
                std::cout << "RESTORED field " << record.name << "." << field.name << " usr=" << *field.clangUsr
                          << '\n';
            }
        }
    }
    return ok;
}

// EN: True when both enums declare the same enumerator names in the same order. Used to find the
//     live declaration of an enum whose name may be empty (anonymous enum named by a typedef).
// ES: True si ambos enums declaran los mismos nombres de enumeradores en el mismo orden. Sirve para
//     hallar la declaración viva de un enum cuyo nombre puede estar vacío (enum anónimo con typedef).
bool sameEnumerators(const CppEnumDeclaration& left, const CppEnumDeclaration& right)
{
    if (left.enumerators.size() != right.enumerators.size() || left.enumerators.empty()) return false;
    for (std::size_t index = 0; index < left.enumerators.size(); ++index)
    {
        if (left.enumerators[index].name != right.enumerators[index].name) return false;
    }
    return true;
}

// EN: Compares restored enum type and enumerator USRs with the live Clang enums of the header.
// ES: Compara los USR restaurados de tipos enum y enumeradores con los enums vivos de Clang.
bool checkRestoredEnums(const nativesharp::frontend::ProjectCompilation& consumer,
                        const std::vector<CppCEnumSnapshot>& stored, const std::vector<CppEnumDeclaration>& live)
{
    bool ok = true;
    const auto* lookup = consumer.nameLookup();
    const auto* values = consumer.namespaceFunctionModel();
    if (!expect(lookup && values, "consumer name lookup and namespace model exist")) return false;
    for (const auto& enumeration : stored)
    {
        const auto& name = enumeration.declaration.name;
        const auto clang = std::find_if(live.begin(), live.end(), [&](const auto& item) {
            return sameEnumerators(item, enumeration.declaration);
        });
        const auto ordinal = lookup->typeOrdinalForIdentity(enumeration.identityKey);
        const auto* restored = ordinal ? lookup->cppImportedType(*ordinal) : nullptr;
        const bool known = clang != live.end() && clang->clangUsr && !clang->clangUsr->empty();
        ok &= expect(known && restored && restored->clangUsr == clang->clangUsr,
                     "consumer restores the Clang USR of C enum " + name);
        if (!known || !restored) continue;
        std::cout << "RESTORED enum " << name << " usr=" << restored->clangUsr.value_or("<absent>") << '\n';
        for (const auto& value : clang->enumerators)
        {
            const auto states = values->states();
            const auto state = std::find_if(states.begin(), states.end(), [&](const auto& item) {
                return item.name == value.name && item.isCppEnumConstant;
            });
            const bool matched = state != states.end() && value.clangUsr && state->cppClangUsr == value.clangUsr;
            ok &= expect(matched, "consumer restores the Clang USR of C enumerator " + value.name);
            if (matched) std::cout << "RESTORED enumerator " << value.name << " usr=" << *value.clangUsr << '\n';
        }
    }
    return ok;
}

// EN: Reads and authenticates the producer NSMETA and decodes its C catalogue.
// ES: Lee y autentica el NSMETA del productor y decodifica su catálogo C.
bool readProducerCatalogue(const std::filesystem::path& path, LibraryMetadata& metadata,
                           std::vector<CppCRecordSnapshot>& records, std::vector<CppCEnumSnapshot>& enums)
{
    LibraryMetadataCodec codec;
    std::string error;
    if (!codec.read(path, metadata, error) || !codec.verifyCurrentCompiler(metadata, error))
    {
        std::cerr << "NSMETA verification failed: " << error << '\n';
        return false;
    }
    if (!metadata_detail::decodeVersionedCTypeCatalog(metadata.cppCRecordCatalog, records, enums, error, metadata.formatMinor))
    {
        std::cerr << "C catalogue decode failed: " << error << '\n';
        return false;
    }
    return true;
}

// EN: Restoration mode. The producer NSMETA is real; the consumer wraps its catalogue in a
//     precompiled module (as the driver does after loading it) and has no C header of its own.
// ES: Modo de restauración. El NSMETA del productor es real; el consumidor envuelve su catálogo en
//     un módulo precompilado (como el driver tras cargarlo) y no tiene encabezado C propio.
int verifyRestoredUsrs(char** argv)
{
    LibraryMetadata metadata;
    std::vector<CppCRecordSnapshot> records;
    std::vector<CppCEnumSnapshot> enums;
    if (!readProducerCatalogue(argv[2], metadata, records, enums)) return 1;
    if (!expect(!records.empty() || !enums.empty(), "producer catalogue contains C records or enums")) return 1;
    const auto header = std::filesystem::absolute(argv[3]);
    CppClangSessionFactory factory;
    auto session = openSession(factory, headerOptions(argv[4], header.parent_path(), argv[5],
                                                      header.filename().string()));
    const auto liveRecords = session ? session->recordsSnapshot() : std::nullopt;
    const auto liveEnums = session ? session->enumsSnapshot() : std::nullopt;
    if (!expect(liveRecords && liveEnums, "live Clang session exposes the producer header")) return 1;
    const auto directory = std::filesystem::absolute(std::filesystem::path(argv[6]) / "c-identity-consumer");
    const auto project = writeProject(directory, "CIdentityConsumer", consumerSource(enums));
    if (!expect(!project.empty(), "consumer project is written")) return 1;
    nativesharp::frontend::PrecompiledSyntaxModule module;
    module.ownerProjectFile = project;
    module.metadataPath = std::filesystem::absolute(argv[2]);
    module.moduleName = metadata.moduleName;
    module.sourceText = "namespace CIdentityRestoredSurface { public struct Marker {} }\n";
    module.cppCTypeCatalog = metadata.cppCRecordCatalog;
    module.cppCTypeCatalogFormatMinor = metadata.formatMinor;
    nativesharp::frontend::ProjectCompilation consumer;
    std::string error;
    const bool compiled = compileConsumer(consumer, std::move(module), project, error);
    session->close();
    if (!compiled) std::cerr << "consumer: " << error << '\n';
    bool ok = expect(compiled, "consumer compiles members from the producer catalogue");
    ok &= compiled && checkRestoredRecords(consumer, records, *liveRecords);
    ok &= compiled && checkRestoredEnums(consumer, enums, *liveEnums);
    std::cout << (ok ? "Consumer restores Clang USRs from the real producer NSMETA\n"
                     : "Consumer USR restoration failed\n");
    return ok ? 0 : 1;
}
// EN: Writes a consumer project beneath `directory` that references `library` through a real
//     LibraryReference and compiles Main.ns. Returns the absolute project path or an empty path.
// ES: Escribe bajo `directory` un proyecto consumidor que referencia `library` mediante un
//     LibraryReference real y compila Main.ns. Devuelve la ruta absoluta del proyecto o una vacía.
std::filesystem::path writeDriverConsumer(const std::filesystem::path& directory,
                                          const std::filesystem::path& library, const std::string& mainSource)
{
    std::filesystem::create_directories(directory);
    const auto project = std::filesystem::absolute(directory / "CIdentityDriverConsumer.nsproj");
    const std::string text =
        "<Project>\n"
        "  <PropertyGroup><Name>CIdentityDriverConsumer</Name></PropertyGroup>\n"
        "  <ItemGroup>\n"
        "    <Compile Include=\"Main.ns\" />\n"
        "    <LibraryReference Include=\"" + library.string() + "\" />\n"
        "  </ItemGroup>\n"
        "</Project>\n";
    const bool written = writeText(project, text) && writeText(directory / "Main.ns", mainSource);
    return written ? project : std::filesystem::path();
}

// EN: Loads the consumer project graph and lets the driver prepare the precompiled modules
//     (configurePrecompiledModules: library loading, catalogue transport and provider
//     installation), then compiles the members.
// ES: Carga el grafo del proyecto consumidor y deja que el driver prepare los módulos precompilados
//     (configurePrecompiledModules: carga de bibliotecas, transporte del catálogo e instalación de
//     proveedores) y después compila los miembros.
bool compileThroughDriver(nativesharp::frontend::ProjectCompilation& consumer,
                          const std::filesystem::path& project, std::string& error)
{
    nativesharp::compilation::ProjectLoadOptions loadOptions;
    nativesharp::compilation::ProjectGraphLoader loader(1, {}, loadOptions);
    auto graph = loader.load(project, error);
    if (!graph) return false;
    nativesharp::compilation::CompilationInputManifest manifest(std::move(*graph), loadOptions);
    if (!nativesharp::driver::configurePrecompiledModules(manifest, consumer, error, false)) return false;
    return consumer.compileProjectMembers(project, error);
}

// EN: Driver restoration mode. The producer library and NSMETA are real and the consumer reaches
//     them only through its LibraryReference; it has no C header of its own.
// ES: Modo de restauración por el driver. La biblioteca y el NSMETA del productor son reales y el
//     consumidor solo los alcanza por su LibraryReference; no tiene encabezado C propio.
int verifyDriverRestoredUsrs(char** argv)
{
    const auto library = std::filesystem::absolute(argv[2]);
    auto metadataPath = library;
    metadataPath.replace_extension(".nsmeta");
    LibraryMetadata metadata;
    std::vector<CppCRecordSnapshot> records;
    std::vector<CppCEnumSnapshot> enums;
    if (!readProducerCatalogue(metadataPath, metadata, records, enums)) return 1;
    if (!expect(!records.empty() || !enums.empty(), "producer catalogue contains C records or enums")) return 1;
    const auto header = std::filesystem::absolute(argv[3]);
    CppClangSessionFactory factory;
    auto session = openSession(factory, headerOptions(argv[4], header.parent_path(), argv[5],
                                                      header.filename().string()));
    const auto liveRecords = session ? session->recordsSnapshot() : std::nullopt;
    const auto liveEnums = session ? session->enumsSnapshot() : std::nullopt;
    if (!expect(liveRecords && liveEnums, "live Clang session exposes the producer header")) return 1;
    session->close();
    const auto project = writeDriverConsumer(library.parent_path() / "c-identity-driver-consumer", library,
                                             consumerSource(enums));
    if (!expect(!project.empty(), "driver consumer project is written")) return 1;
    nativesharp::frontend::ProjectCompilation consumer;
    std::string error;
    const bool compiled = compileThroughDriver(consumer, project, error);
    if (!compiled) std::cerr << "driver consumer: " << error << " " << diagnosticText(consumer) << '\n';
    bool ok = expect(compiled, "driver consumer compiles members from the precompiled library");
    ok &= compiled && checkRestoredRecords(consumer, records, *liveRecords);
    ok &= compiled && checkRestoredEnums(consumer, enums, *liveEnums);
    std::cout << (ok ? "Driver consumer restores Clang USRs from the real producer library\n"
                     : "Driver consumer USR restoration failed\n");
    return ok ? 0 : 1;
}
// EN: One row of typedefs/casos.txt. Outcomes are runs, alias, record or member; see validar.cmd.
// ES: Una fila de typedefs/casos.txt. Los resultados son runs, alias, record o member; ver validar.cmd.
struct TypedefCase
{
    std::string name;
    std::string alias;
    std::string kind;
    std::string single;
    std::string distinctFiles;
    std::string homonymFiles;
    std::string proposal;
};

// EN: Reads the case table, ignoring blank lines and lines that start with '#'. Returns an empty
//     vector when the file is missing or a row does not have seven columns.
// ES: Lee la tabla de casos e ignora líneas vacías y las que empiezan por '#'. Devuelve un vector
//     vacío si falta el archivo o una fila no tiene siete columnas.
std::vector<TypedefCase> readTypedefCases(const std::filesystem::path& file)
{
    std::ifstream input(file);
    std::vector<TypedefCase> result;
    std::string line;
    while (std::getline(input, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line.front() == '#') continue;
        std::istringstream row(line);
        TypedefCase item;
        std::string extra;
        if (!(row >> item.name >> item.alias >> item.kind >> item.single >> item.distinctFiles >> item.homonymFiles >>
              item.proposal) || (row >> extra)) return {};
        result.push_back(std::move(item));
    }
    return result;
}

// EN: What one session says about typedef `alias`: its USR, canonical type and the USR of the
//     record or enum it names, read with the same helpers the name resolution uses.
// ES: Lo que una sesión dice del typedef `alias`: su USR, el tipo canónico y el USR del record o
//     enum al que remite, leídos con las mismas funciones que usa la resolución de nombres.
struct AliasObservation
{
    bool found = false;
    std::optional<std::string> usr;
    std::string canonical;
    std::optional<std::string> recordUsr;
    std::optional<std::string> enumUsr;
};

// EN: Reads the first typedef named `alias` from `session`; `found` stays false when absent.
// ES: Lee el primer typedef llamado `alias` de `session`; `found` queda false si no existe.
AliasObservation observeAlias(const CppInteropSession& session, const std::string& alias)
{
    AliasObservation result;
    const auto aliases = session.typeAliasesSnapshot();
    if (!aliases) return result;
    for (const auto& item : *aliases)
    {
        if (item.qualifiedName != alias) continue;
        result.found = true;
        result.usr = item.clangUsr;
        result.canonical = item.underlyingType.canonicalSpelling;
        if (const auto record = nativesharp::frontend::cppAliasRecordType(item.underlyingType))
            result.recordUsr = record->clangUsr;
        if (const auto enumeration = nativesharp::frontend::cppAliasEnumType(item.underlyingType))
            result.enumUsr = enumeration->clangUsr;
        break;
    }
    return result;
}

// EN: Replica of the current rule of QualifiedTypeNames.cpp for two observations of one name in
//     one project and target: equal canonical type and, when both USRs are known, equal USRs.
// ES: Réplica de la regla actual de QualifiedTypeNames.cpp para dos observaciones de un nombre en
//     un proyecto y destino: mismo tipo canónico y, si ambos USR se conocen, USR iguales.
bool currentAliasRule(const AliasObservation& left, const AliasObservation& right)
{
    const bool known = left.usr && !left.usr->empty() && right.usr && !right.usr->empty();
    return left.canonical == right.canonical && (!known || *left.usr == *right.usr);
}

// EN: True when `usr` has the file-scope typedef form c:<file>@T@<name>, with a non-empty file
//     segment free of '@'. Test replica of the rule in T08-002-PROPUESTA-TYPEDEF.md.
// ES: True si `usr` tiene la forma de typedef de ámbito de archivo c:<archivo>@T@<nombre>, con un
//     segmento de archivo no vacío y sin '@'. Réplica de prueba de T08-002-PROPUESTA-TYPEDEF.md.
bool fileScopeTypedefUsr(std::string_view usr, std::string_view name)
{
    const std::string tail = "@T@" + std::string(name);
    if (!usr.starts_with("c:") || usr.size() <= 2 + tail.size() || !usr.ends_with(tail)) return false;
    const auto file = usr.substr(2, usr.size() - 2 - tail.size());
    return !file.empty() && file.find('@') == std::string_view::npos;
}

// EN: Proposed rule: equal canonical type, typedef identity equal or differing only in the file
//     segment, and compatible (absent or equal) USRs of the record or enum named by the alias.
// ES: Regla propuesta: mismo tipo canónico, identidad de typedef igual o distinta solo en el
//     segmento de archivo, y USR compatibles (ausentes o iguales) del record o enum referido.
bool proposedAliasRule(const AliasObservation& left, const AliasObservation& right, const std::string& name)
{
    const auto compatible = [](const auto& a, const auto& b) { return !a || !b || a == b; };
    const bool known = left.usr && !left.usr->empty() && right.usr && !right.usr->empty();
    const bool sameIdentity = !known || *left.usr == *right.usr ||
        (fileScopeTypedefUsr(*left.usr, name) && fileScopeTypedefUsr(*right.usr, name));
    return left.canonical == right.canonical && sameIdentity && compatible(left.recordUsr, right.recordUsr) &&
        compatible(left.enumUsr, right.enumUsr);
}

// EN: Opens the two headers of one pair and returns both observations of `alias`.
// ES: Abre los dos encabezados de un par y devuelve ambas observaciones de `alias`.
std::optional<std::pair<AliasObservation, AliasObservation>> observePair(
    CppClangSessionFactory& factory, const std::string& resource, const std::filesystem::path& directory,
    const std::string& driver, const std::string& first, const std::string& second, const std::string& alias)
{
    auto left = openSession(factory, headerOptions(resource, directory, driver, first));
    auto right = openSession(factory, headerOptions(resource, directory, driver, second));
    if (!left || !right) return std::nullopt;
    auto result = std::pair{observeAlias(*left, alias), observeAlias(*right, alias)};
    left->close();
    right->close();
    return result;
}

// EN: One evidence line for an observation.
// ES: Una línea de evidencia para una observación.
std::string describe(const AliasObservation& value)
{
    if (!value.found) return "<no alias>";
    return value.usr.value_or("<no usr>") + " '" + value.canonical + "' record=" +
        value.recordUsr.value_or("-") + " enum=" + value.enumUsr.value_or("-");
}

// EN: Checks one row. Rows whose alias column is '-' or starts with '-' have no typedef of that
//     name under test (the record carries the name); they are only printed. For the others, the
//     replicated current rule must match the observed outcomes of XY and PQ, and the proposed rule
//     must accept exactly when the PROP outcome is not an alias conflict.
// ES: Comprueba una fila. Las filas cuyo alias es '-' o empieza por '-' no prueban un typedef de
//     ese nombre (el record lleva el nombre); solo se imprimen. En las demás, la regla actual
//     replicada debe coincidir con los resultados observados de XY y PQ, y la regla propuesta debe
//     aceptar exactamente cuando el resultado PROP no es un conflicto de alias.
bool checkTypedefCase(CppClangSessionFactory& factory, const std::string& resource,
                      const std::filesystem::path& directory, const std::string& driver, const TypedefCase& row)
{
    const bool observeOnly = row.alias.front() == '-';
    const std::string alias = observeOnly ? row.alias.substr(1) : row.alias;
    if (alias.empty())
    {
        std::cout << "TYPEDEF " << row.name << " (" << row.kind << "): no typedef under test\n";
        return true;
    }
    const auto xy = observePair(factory, resource, directory, driver, row.name + "/x.h", row.name + "/y.h", alias);
    const auto pq = observePair(factory, resource, directory, driver, row.name + "/p/t.h", row.name + "/q/t.h", alias);
    if (!expect(xy && pq, "typedef case " + row.name + " sessions open")) return false;
    const bool currentXy = currentAliasRule(xy->first, xy->second);
    const bool currentPq = currentAliasRule(pq->first, pq->second);
    const bool proposedXy = proposedAliasRule(xy->first, xy->second, alias);
    const bool proposedPq = proposedAliasRule(pq->first, pq->second, alias);
    std::cout << "TYPEDEF " << row.name << " (" << row.kind << ") " << alias << "\n  x.h   " << describe(xy->first)
              << "\n  y.h   " << describe(xy->second) << "\n  p/t.h " << describe(pq->first) << "\n  q/t.h "
              << describe(pq->second) << "\n  current XY=" << (currentXy ? "accept" : "reject")
              << " PQ=" << (currentPq ? "accept" : "reject") << "; proposed XY=" << (proposedXy ? "accept" : "reject")
              << " PQ=" << (proposedPq ? "accept" : "reject") << '\n';
    if (observeOnly) return true;
    bool ok = expect(xy->first.found && xy->second.found && pq->first.found && pq->second.found,
                     row.name + ": every header declares the typedef");
    ok &= expect(proposedXy == (row.distinctFiles != "alias"), row.name + ": active rule matches XY outcome");
    ok &= expect(currentPq == (row.homonymFiles != "alias"), row.name + ": current rule matches PQ outcome");
    ok &= expect(proposedXy == (row.proposal != "alias"), row.name + ": proposed rule matches PROP outcome");
    ok &= expect(proposedPq == currentPq, row.name + ": proposed rule leaves homonymous headers unchanged");
    return ok;
}

// EN: Evidence for the typedef rule (H1) beyond the matrix: typedef USRs when the header file name
//     contains '@' or a space, and when a macro defined in a shared header declares the typedef.
//     Asserted as observed with Clang 23.1.0: the file segment is the expansion file, copied
//     verbatim ('@' and spaces included); the proposed rule accepts the space but rejects the '@'
//     file (conservative), and rejects the macro pair by its canonical type.
// ES: Evidencia para la regla de typedef (H1) más allá de la matriz: USR de typedef cuando el
//     nombre del encabezado contiene '@' o un espacio, y cuando una macro definida en un encabezado
//     común declara el typedef. Afirmado tal como se observó con Clang 23.1.0: el segmento de
//     archivo es el archivo de expansión, copiado literalmente ('@' y espacios incluidos); la regla
//     propuesta acepta el espacio pero rechaza el archivo con '@' (conservador) y rechaza el par de
//     macros por su tipo canónico.
bool checkTypedefFileForms(CppClangSessionFactory& factory, const std::string& resource,
                           const std::filesystem::path& directory, const std::string& driver)
{
    const auto forms = directory / "formas";
    std::filesystem::create_directories(forms);
    const bool written = writeText(forms / "plain.h", "typedef int CTdForm;\n") &&
        writeText(forms / "at@sign.h", "typedef int CTdForm;\n") &&
        writeText(forms / "with space.h", "typedef int CTdForm;\n") &&
        writeText(forms / "macros.h", "#define CTD_TYPEDEF(name, type) typedef type name;\n") &&
        writeText(forms / "macro-x.h", "#include \"macros.h\"\nCTD_TYPEDEF(CTdMacro, int)\n") &&
        writeText(forms / "macro-y.h", "#include \"macros.h\"\nCTD_TYPEDEF(CTdMacro, long long)\n");
    if (!expect(written, "typedef form headers are written")) return false;
    struct Pair
    {
        std::string first;
        std::string second;
        std::string alias;
        std::string secondUsr;
        bool proposedAccepts;
    };
    const std::vector<Pair> pairs = {
        {"formas/plain.h", "formas/at@sign.h", "CTdForm", "c:at@sign.h@T@CTdForm", false},
        {"formas/plain.h", "formas/with space.h", "CTdForm", "c:with space.h@T@CTdForm", true},
        {"formas/macro-x.h", "formas/macro-y.h", "CTdMacro", "c:macro-y.h@T@CTdMacro", false}};
    bool ok = true;
    for (const auto& pair : pairs)
    {
        const auto observed = observePair(factory, resource, directory, driver, pair.first, pair.second, pair.alias);
        if (!expect(observed.has_value(), "typedef form sessions open: " + pair.second)) return false;
        std::cout << "TYPEDEF FORM " << pair.first << " + " << pair.second << "\n  first  "
                  << describe(observed->first) << "\n  second " << describe(observed->second) << "\n  current="
                  << (currentAliasRule(observed->first, observed->second) ? "accept" : "reject") << " proposed="
                  << (proposedAliasRule(observed->first, observed->second, pair.alias) ? "accept" : "reject") << '\n';
        ok &= expect(observed->second.usr == std::optional<std::string>(pair.secondUsr),
                     "typedef USR keeps the expansion file name verbatim: " + pair.secondUsr);
        ok &= expect(!currentAliasRule(observed->first, observed->second),
                     "current rule rejects " + pair.first + " + " + pair.second);
        ok &= expect(proposedAliasRule(observed->first, observed->second, pair.alias) == pair.proposedAccepts,
                     "proposed rule verdict for " + pair.first + " + " + pair.second);
    }
    return ok;
}

// EN: Entry of --typedef-matrix. `directory` is the copied typedefs fixture with casos.txt.
// ES: Entrada de --typedef-matrix. `directory` es el fixture typedefs copiado con casos.txt.
int runTypedefMatrix(const std::filesystem::path& directory, const std::string& resource, const std::string& driver)
{
    const auto cases = readTypedefCases(directory / "casos.txt");
    if (cases.empty())
    {
        std::cerr << "cannot read " << (directory / "casos.txt").string() << '\n';
        return 2;
    }
    CppClangSessionFactory factory;
    bool ok = true;
    const auto absolute = std::filesystem::absolute(directory);
    for (const auto& row : cases) ok &= checkTypedefCase(factory, resource, absolute, driver, row);
    ok &= checkTypedefFileForms(factory, resource, absolute, driver);
    std::cout << (ok ? "Typedef identity matrix passed\n" : "Typedef identity matrix failed\n");
    return ok ? 0 : 1;
}

// EN: Mode --inspect-c-constants <nsmeta> <forbidden-state> <required-enumerator>. Reads a real
//     producer NSMETA and checks how C enum constants were published (finding H7): no namespace
//     state without a namespace, no state named `forbidden` ('-' skips the name check) and, unless
//     `required` is '-', a C catalogue enumerator with that name. Prints the published states and
//     the catalogue enums as evidence. Returns 0 when every check passes.
// ES: Modo --inspect-c-constants <nsmeta> <estado-prohibido> <enumerador-requerido>. Lee el NSMETA
//     real de un productor y comprueba cómo se publicaron las constantes de enum C (hallazgo H7):
//     ningún estado de espacio de nombres sin espacio de nombres, ningún estado llamado
//     `forbidden` ('-' omite la comprobación del nombre) y, salvo que `required` sea '-', un
//     enumerador del catálogo C con ese nombre. Imprime los estados publicados y los enums del
//     catálogo como evidencia. Devuelve 0 si pasan todas las comprobaciones.
int inspectTemplateAlias(char** argv)
{
    LibraryMetadataCodec codec;
    LibraryMetadata metadata;
    std::string error;
    if (!codec.read(argv[2], metadata, error) || !codec.verifyCurrentCompiler(metadata, error))
    { std::cerr << "metadata read: " << error << '\n'; return 1; }
    std::vector<CppCRecordSnapshot> records;
    std::vector<CppCEnumSnapshot> enums;
    metadata_detail::CDependencyCatalog dependencies;
    const bool found = metadata.formatMinor >= 46 &&
        metadata_detail::decodeCTypeCatalogAndDependencies(metadata.cppCRecordCatalog, metadata.formatMinor,
            records, enums, dependencies, error) &&
        std::any_of(dependencies.aliases.begin(), dependencies.aliases.end(),
            [&](const auto& alias) { return alias.declaration.name == argv[3]; });
    if (!found) std::cerr << "template alias inspection: " << error << '\n';
    return expect(found, "real NSMETA carries the requested C template alias") ? 0 : 1;
}

int inspectTemplateFunction(char** argv)
{
    LibraryMetadataCodec codec;
    LibraryMetadata metadata;
    std::string error;
    if (!codec.read(argv[2], metadata, error) || !codec.verifyCurrentCompiler(metadata, error))
    { std::cerr << "metadata read: " << error << '\n'; return 1; }
    std::vector<CppCRecordSnapshot> records;
    std::vector<CppCEnumSnapshot> enums;
    metadata_detail::CDependencyCatalog dependencies;
    const bool found = metadata.formatMinor >= 46 &&
        metadata_detail::decodeCTypeCatalogAndDependencies(metadata.cppCRecordCatalog, metadata.formatMinor,
            records, enums, dependencies, error) &&
        std::any_of(dependencies.functions.begin(), dependencies.functions.end(),
            [&](const auto& function) { return function.declaration.name == argv[3] && function.linkSymbol == argv[3]; });
    if (!found) std::cerr << "template function inspection: " << error << '\n';
    return expect(found, "real NSMETA carries the requested C template function") ? 0 : 1;
}

int inspectTemplateGlobal(char** argv)
{
    LibraryMetadataCodec codec;
    LibraryMetadata metadata;
    std::string error;
    if (!codec.read(argv[2], metadata, error) || !codec.verifyCurrentCompiler(metadata, error))
    { std::cerr << "metadata read: " << error << '\n'; return 1; }
    std::vector<CppCRecordSnapshot> records;
    std::vector<CppCEnumSnapshot> enums;
    metadata_detail::CDependencyCatalog dependencies;
    const bool found = metadata.formatMinor >= 46 &&
        metadata_detail::decodeCTypeCatalogAndDependencies(metadata.cppCRecordCatalog, metadata.formatMinor,
            records, enums, dependencies, error) &&
        std::any_of(dependencies.globals.begin(), dependencies.globals.end(),
            [&](const auto& global) { return global.declaration.name == argv[3] && global.linkSymbol == argv[3]; });
    if (!found) std::cerr << "template global inspection: " << error << '\n';
    return expect(found, "real NSMETA carries the requested C template global") ? 0 : 1;
}

int inspectTemplateEnumerator(char** argv)
{
    LibraryMetadataCodec codec;
    LibraryMetadata metadata;
    std::string error;
    if (!codec.read(argv[2], metadata, error) || !codec.verifyCurrentCompiler(metadata, error))
    { std::cerr << "metadata read: " << error << '\n'; return 1; }
    std::vector<CppCRecordSnapshot> records;
    std::vector<CppCEnumSnapshot> enums;
    metadata_detail::CDependencyCatalog dependencies;
    if (!metadata_detail::decodeCTypeCatalogAndDependencies(metadata.cppCRecordCatalog, metadata.formatMinor,
            records, enums, dependencies, error))
    { std::cerr << "catalogue read: " << error << '\n'; return 1; }
    const bool found = std::any_of(dependencies.enumerators.begin(), dependencies.enumerators.end(),
        [&](const auto& value) { return value.name == argv[3]; });
    return expect(found, "real NSMETA carries the requested C template enumerator") ? 0 : 1;
}

int inspectCConstants(char** argv)
{
    const std::string forbidden = argv[3];
    const std::string required = argv[4];
    LibraryMetadataCodec codec;
    LibraryMetadata metadata;
    std::string error;
    const bool read = codec.read(argv[2], metadata, error) && codec.verifyCurrentCompiler(metadata, error);
    if (!expect(read, "producer NSMETA reads and verifies: " + error)) return 1;
    std::vector<CppCRecordSnapshot> records;
    std::vector<CppCEnumSnapshot> enums;
    if (!metadata.cppCRecordCatalog.empty())
    {
        const bool decoded =
            metadata_detail::decodeVersionedCTypeCatalog(metadata.cppCRecordCatalog, records, enums, error, metadata.formatMinor);
        if (!expect(decoded, "producer C catalogue decodes: " + error)) return 1;
    }
    bool ok = true;
    for (const auto& state : metadata.namespaceStates)
    {
        std::cout << "PUBLISHED state '" << state.namespaceName << '.' << state.name << "' type="
                  << state.declaredTypeKey << '\n';
        ok &= expect(!state.namespaceName.empty(), "no namespace state without a namespace: " + state.name);
        ok &= expect(forbidden == "-" || state.name != forbidden,
                     "C enum constant is not published as a namespace state: " + state.name);
    }
    bool found = required == "-";
    for (const auto& enumeration : enums)
    {
        std::string names;
        for (const auto& item : enumeration.declaration.enumerators)
        {
            names += ' ' + item.name + '=' + item.decimalValue;
            if (item.name == required) found = true;
        }
        std::cout << "CATALOGUE enum " << enumeration.identityKey << ':' << names << '\n';
    }
    ok &= expect(found, "C catalogue keeps the enumerator " + required);
    std::cout << "PUBLISHED " << argv[2] << ": states=" << metadata.namespaceStates.size()
              << " catalogue-records=" << records.size() << " catalogue-enums=" << enums.size() << '\n';
    return ok ? 0 : 1;
}

// EN: Layout of the C type catalogue that the proposal of finding H8 derives from the declared
//     NSMETA minor instead of the first bytes: none before 1.42, legacy in 1.42, versioned envelope
//     in 1.43-1.45. Test replica of T08-002-H8-CATALOGO.md; the codec is unchanged.
// ES: Disposición del catálogo C que la propuesta del hallazgo H8 deduce del minor de NSMETA
//     declarado en lugar de los primeros bytes: ninguna antes de 1.42, heredada en 1.42 y envoltura
//     versionada en 1.43-1.45. Réplica de prueba de T08-002-H8-CATALOGO.md; el codec no cambia.
enum class CatalogueLayout
{
    None,
    Legacy,
    Envelope,
    Unknown
};

CatalogueLayout proposedCatalogueLayout(std::uint16_t minor)
{
    if (minor < 42) return CatalogueLayout::None;
    if (minor == 42) return CatalogueLayout::Legacy;
    if (minor <= 45) return CatalogueLayout::Envelope;
    return CatalogueLayout::Unknown;
}

// EN: Envelope versions each minor can represent, as gated today by the writer and by
//     LibraryMetadataVerification.cpp: 1.43 only version 1, 1.44 up to 2, 1.45 versions 1-4.
// ES: Versiones de envoltura que puede representar cada minor, como hoy las limitan el escritor y
//     LibraryMetadataVerification.cpp: 1.43 solo la versión 1, 1.44 hasta la 2, 1.45 de la 1 a la 4.
bool envelopeVersionAllowed(std::uint16_t minor, std::uint32_t version)
{
    if (minor == 43) return version == 1;
    if (minor == 44) return version >= 1 && version <= 2;
    if (minor == 45) return version >= 1 && version <= 4;
    return false;
}

// EN: Reads the little-endian u32 at the start of `bytes`; 0 when there are fewer than four bytes.
// ES: Lee el u32 little-endian al inicio de `bytes`; 0 si hay menos de cuatro bytes.
std::uint32_t leadingWord(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() < 4) return 0;
    return static_cast<std::uint32_t>(bytes[0]) | (static_cast<std::uint32_t>(bytes[1]) << 8) |
        (static_cast<std::uint32_t>(bytes[2]) << 16) | (static_cast<std::uint32_t>(bytes[3]) << 24);
}

// EN: Proposed decoding: the declared minor selects the decoder, and an envelope version the minor
//     cannot represent is rejected. Leaves the outputs unchanged on failure, like the codec.
// ES: Decodificación propuesta: el minor declarado elige el decodificador y se rechaza una versión
//     de envoltura que el minor no puede representar. Deja las salidas sin cambios si falla, como
//     el codec.
bool proposedCatalogueDecode(std::uint16_t minor, std::span<const std::uint8_t> bytes,
                             std::vector<CppCRecordSnapshot>& records, std::vector<CppCEnumSnapshot>& enums,
                             std::string& error)
{
    const auto layout = proposedCatalogueLayout(minor);
    if (layout == CatalogueLayout::Legacy) return metadata_detail::decodeCTypeCatalog(bytes, records, enums, error);
    if (layout != CatalogueLayout::Envelope)
    {
        error = "no C catalogue layout for NSMETA 1." + std::to_string(minor);
        return false;
    }
    if (!envelopeVersionAllowed(minor, leadingWord(bytes)))
    {
        error = "envelope version " + std::to_string(leadingWord(bytes)) + " is not valid in NSMETA 1." +
            std::to_string(minor);
        return false;
    }
    return metadata_detail::decodeCTypeCatalogWithProvenance(bytes, records, enums, error);
}

// EN: Lower-case hexadecimal text of `bytes`, one space between bytes.
// ES: Texto hexadecimal en minúsculas de `bytes`, con un espacio entre bytes.
std::string hexText(std::span<const std::uint8_t> bytes)
{
    static const char* const digits = "0123456789abcdef";
    std::string text;
    for (std::size_t i = 0; i < bytes.size(); ++i)
    {
        if (i != 0) text += ' ';
        text += digits[bytes[i] >> 4];
        text += digits[bytes[i] & 15];
    }
    return text;
}

// EN: Parses the hexadecimal fixture text; whitespace separates bytes and '#' starts a comment
//     line. Returns nothing on a malformed token.
// ES: Interpreta el texto hexadecimal del fixture; los espacios separan bytes y '#' inicia una
//     línea de comentario. Devuelve nada ante un elemento mal formado.
std::optional<std::vector<std::uint8_t>> readHexFixture(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input) return std::nullopt;
    std::vector<std::uint8_t> bytes;
    std::string line;
    while (std::getline(input, line))
    {
        if (!line.empty() && line.front() == '#') continue;
        std::istringstream tokens(line);
        std::string token;
        while (tokens >> token)
        {
            if (token.size() != 2 || !std::isxdigit(static_cast<unsigned char>(token[0])) ||
                !std::isxdigit(static_cast<unsigned char>(token[1]))) return std::nullopt;
            bytes.push_back(static_cast<std::uint8_t>(std::stoul(token, nullptr, 16)));
        }
    }
    return bytes;
}

// EN: Checks the committed minimal fixture: the legacy (NSMETA 1.42) catalogue that holds only the
//     synthetic enum of probe 7. The encoder must still produce exactly these bytes; the current
//     reader classifies them as envelope 4 and fails; the proposed rule decodes them as 1.42.
// ES: Comprueba el fixture mínimo versionado: el catálogo heredado (NSMETA 1.42) que solo contiene
//     el enum sintético de la sonda 7. El codificador debe seguir produciendo exactamente esos
//     bytes; el lector actual los clasifica como envoltura 4 y falla; la regla propuesta los
//     decodifica como 1.42.
bool checkLegacyEnumOnlyFixture(const std::filesystem::path& directory)
{
    const std::vector<CppCEnumSnapshot> enums{syntheticEnumSnapshot()};
    std::vector<std::uint8_t> encoded;
    std::string error;
    if (!expect(metadata_detail::encodeCTypeCatalog({}, enums, encoded, error), "synthetic legacy catalogue encodes"))
        return false;
    const auto fixture = readHexFixture(directory / "legacy-enum-only.hex");
    if (!fixture || *fixture != encoded)
    {
        std::cerr << "legacy-enum-only.hex differs from the encoder; encoder bytes:\n" << hexText(encoded) << '\n';
        return false;
    }
    std::vector<CppCRecordSnapshot> records;
    std::vector<CppCEnumSnapshot> decoded;
    std::string current;
    const bool sniffed = metadata_detail::isCTypeCatalogWithProvenance(*fixture);
    const bool currentDecodes = metadata_detail::decodeVersionedCTypeCatalog(*fixture, records, decoded, current, 42);
    std::string proposed;
    std::vector<CppCEnumSnapshot> proposedEnums;
    const bool proposedDecodes = proposedCatalogueDecode(42, *fixture, records, proposedEnums, proposed);
    const auto head = std::span(*fixture).first(std::min<std::size_t>(8, fixture->size()));
    std::cout << "FIXTURE legacy-enum-only.hex bytes=" << fixture->size() << " first=" << hexText(head)
              << " current: envelope=" << (sniffed ? "yes" : "no") << " decoded="
              << (currentDecodes ? "yes" : "no") << " '" << current << "'; proposed 1.42: decoded="
              << (proposedDecodes ? "yes" : "no") << " enums=" << proposedEnums.size() << '\n';
    bool ok = expect(sniffed && currentDecodes && decoded.size() == 1,
                     "declared 1.42 decodes the legacy enum-only catalogue despite its ambiguous prefix");
    ok &= expect(proposedDecodes && proposedEnums.size() == 1, "proposed rule decodes it with the declared 1.42");
    return ok;
}

// EN: Writes `metadata` down-level as NSMETA 1.<minor> with `catalogue`, reads it back with the
//     current reader and compares the current and proposed catalogue decoding. Prints one row.
//     `expectCurrent` is the outcome the current reader must give. Returns false on a mismatch.
// ES: Escribe `metadata` en versión anterior como NSMETA 1.<minor> con `catalogue`, lo vuelve a
//     leer con el lector actual y compara la decodificación actual y la propuesta del catálogo.
//     Imprime una fila. `expectCurrent` es el resultado que debe dar el lector actual. Devuelve
//     false si no coincide.
bool checkCatalogueVariant(const LibraryMetadata& metadata, const std::string& stem, std::uint16_t minor,
                           const std::vector<std::uint8_t>& catalogue, const std::filesystem::path& directory,
                           bool expectCurrent)
{
    LibraryMetadataCodec codec;
    LibraryMetadata model = metadata;
    model.formatMinor = minor;
    model.cppCRecordCatalog = catalogue;
    const auto path = directory / (stem + "-1." + std::to_string(minor) + ".nsmeta");
    std::string writeError;
    if (!codec.writeModel(model, path, writeError))
    {
        std::cout << "VARIANT " << stem << " 1." << minor << ": writer refused: " << writeError << '\n';
        return expect(false, stem + " 1." + std::to_string(minor) + " can be written down-level");
    }
    LibraryMetadata decoded;
    std::string readError;
    const bool current = codec.read(path, decoded, readError) && codec.verifyCurrentCompiler(decoded, readError);
    std::vector<CppCRecordSnapshot> records;
    std::vector<CppCEnumSnapshot> enums;
    std::string proposedError;
    const bool proposed = proposedCatalogueDecode(minor, catalogue, records, enums, proposedError);
    const auto head = std::span(catalogue).first(std::min<std::size_t>(8, catalogue.size()));
    std::cout << "VARIANT " << stem << " 1." << minor << ": first=" << hexText(head)
              << " sniffed-envelope=" << (metadata_detail::isCTypeCatalogWithProvenance(catalogue) ? "yes" : "no")
              << " current=" << (current ? "reads" : "fails '" + readError + "'") << " proposed="
              << (proposed ? "decodes" : "fails '" + proposedError + "'") << " records=" << records.size()
              << " enums=" << enums.size() << '\n';
    bool ok = expect(current == expectCurrent, stem + " 1." + std::to_string(minor) + ": current reader outcome");
    ok &= expect(proposed, stem + " 1." + std::to_string(minor) + ": proposed rule decodes the catalogue");
    return ok;
}

// EN: Compatibility controls for one real producer NSMETA. T08-D24: the writer always uses the current contract
//     (kMetadataFormatMinor), whatever the envelope. Rewrites the catalogue for every minor that can represent
//     it: 1.42 (legacy) only for envelope version 1, 1.43 only for version 1, 1.44 up to version 2 and 1.45 for
//     every version. The current reader must fail exactly for the 1.42 catalogue without records (H8).
// ES: Controles de compatibilidad para el NSMETA real de un productor. T08-D24: el escritor siempre usa el
//     contrato vigente (kMetadataFormatMinor), sea cual sea la envoltura. Reescribe el catalogo en cada minor que
//     pueda representarlo: 1.42 (heredado) solo para la envoltura version 1, 1.43 solo para la version 1, 1.44
//     hasta la version 2 y 1.45 para todas. El lector actual debe fallar exactamente con el catalogo 1.42 sin
//     records (H8).
bool checkCatalogueVersions(const std::filesystem::path& nsmeta, const std::filesystem::path& directory)
{
    LibraryMetadata metadata;
    std::vector<CppCRecordSnapshot> records;
    std::vector<CppCEnumSnapshot> enums;
    if (!readProducerCatalogue(nsmeta, metadata, records, enums)) return false;
    const auto stem = nsmeta.stem().string();
    const auto version = leadingWord(metadata.cppCRecordCatalog);
    std::cout << "PRODUCER " << stem << ": NSMETA 1." << metadata.formatMinor << " envelope=" << version
              << " records=" << records.size() << " enums=" << enums.size() << '\n';
    std::string error;
    bool ok = expect(metadata.formatMinor == kMetadataFormatMinor && (version == 7 ||
        metadata_detail::isCTypeCatalogWithProvenance(metadata.cppCRecordCatalog)),
        stem + " uses the current NSMETA contract (T08-D24)");
    std::vector<std::uint8_t> historical = metadata.cppCRecordCatalog;
    if (version == 7 && !metadata_detail::encodeCTypeCatalogWithProvenance(records, enums, historical, error))
        return expect(false, stem + " historical catalogue encoding");
    const auto historicalVersion = leadingWord(historical);
    if (historicalVersion == 1)
    {
        std::vector<std::uint8_t> legacy;
        ok &= expect(metadata_detail::encodeCTypeCatalog(records, enums, legacy, error), stem + " legacy encoding");
        ok &= checkCatalogueVariant(metadata, stem, 42, legacy, directory, true);
    }
    for (const std::uint16_t minor : {std::uint16_t{43}, std::uint16_t{44}, std::uint16_t{45}})
    {
        if (!envelopeVersionAllowed(minor, historicalVersion)) continue;
        ok &= checkCatalogueVariant(metadata, stem, minor, historical, directory, true);
    }
    return ok;
}

// EN: Mode --catalogue-version-controls <fixtures-dir> <producer.nsmeta>... (finding H8). Checks
//     the minimal 1.42 fixture and the down-level variants of each real producer, written beneath
//     <fixtures-dir>/variantes.
// ES: Modo --catalogue-version-controls <dir-fixtures> <productor.nsmeta>... (hallazgo H8).
//     Comprueba el fixture mínimo 1.42 y las variantes en versión anterior de cada productor real,
//     escritas bajo <dir-fixtures>/variantes.
int runCatalogueVersionControls(int argc, char** argv)
{
    const auto directory = std::filesystem::absolute(argv[2]);
    const auto variants = directory / "variantes";
    std::filesystem::create_directories(variants);
    bool ok = checkLegacyEnumOnlyFixture(directory);
    for (int i = 3; i < argc; ++i) ok &= checkCatalogueVersions(argv[i], variants);
    std::cout << (ok ? "C catalogue version controls passed\n" : "C catalogue version controls failed\n");
    return ok ? 0 : 1;
}

}

int main(int argc, char** argv)
{
    if (argc == 7 && std::string_view(argv[1]) == "--verify-restored-c-usr") return verifyRestoredUsrs(argv);
    if (argc == 6 && std::string_view(argv[1]) == "--verify-driver-restored-c-usr")
    {
        return verifyDriverRestoredUsrs(argv);
    }
    if (argc == 5 && std::string_view(argv[1]) == "--typedef-matrix")
    {
        return runTypedefMatrix(argv[2], argv[3], argv[4]);
    }
    if (argc == 5 && std::string_view(argv[1]) == "--inspect-c-constants") return inspectCConstants(argv);
    if (argc == 4 && std::string_view(argv[1]) == "--inspect-template-alias") return inspectTemplateAlias(argv);
    if (argc == 4 && std::string_view(argv[1]) == "--inspect-template-function") return inspectTemplateFunction(argv);
    if (argc == 4 && std::string_view(argv[1]) == "--inspect-template-global") return inspectTemplateGlobal(argv);
    if (argc == 4 && std::string_view(argv[1]) == "--inspect-template-enumerator") return inspectTemplateEnumerator(argv);
    if (argc >= 3 && std::string_view(argv[1]) == "--catalogue-version-controls")
    {
        return runCatalogueVersionControls(argc, argv);
    }
    if (argc != 4)
    {
        std::cerr << "usage: nsc_cpp_c_identity_tests <clang-resource-dir> <work-dir> <clang-driver>\n"
                     "       nsc_cpp_c_identity_tests --verify-restored-c-usr <nsmeta> <header> "
                     "<clang-resource-dir> <clang-driver> <work-dir>\n"
                     "       nsc_cpp_c_identity_tests --verify-driver-restored-c-usr <producer.lib> <header> "
                     "<clang-resource-dir> <clang-driver>\n"
                     "       nsc_cpp_c_identity_tests --typedef-matrix <typedefs-dir> <clang-resource-dir> "
                     "<clang-driver>\n"
                     "       nsc_cpp_c_identity_tests --inspect-c-constants <producer.nsmeta> <forbidden-state> "
                     "<required-enumerator>\n"
                     "       nsc_cpp_c_identity_tests --catalogue-version-controls <fixtures-dir> "
                     "<producer.nsmeta>...\n";
        return 2;
    }
    return runProbes(argv[1], argv[2], argv[3]);
}
