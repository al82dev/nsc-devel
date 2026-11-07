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
//
// Usage / Uso:
//   nsc_cpp_c_identity_tests <clang-resource-dir> <work-dir> <clang-driver>
//   nsc_cpp_c_identity_tests --verify-restored-c-usr <producer.nsmeta> <header> <clang-resource-dir>
//                            <clang-driver> <work-dir>

#include "nativesharp/Interop/CppClangSessionFactory.h"
#include "nativesharp/Interop/CppProjectSessions.h"
#include "nativesharp/Interop/CppSemanticFunctionGroups.h"
#include "nativesharp/Interop/CppSemanticInventory.h"
#include "nativesharp/Interop/CppSemanticRecord.h"
#include "nativesharp/Interop/LibraryMetadata.h"
#include "nativesharp/Interop/LibraryMetadataCodec.h"
#include "nativesharp/Frontend/PrecompiledSyntaxModule.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNamespaceFunctionModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "../../src/Interop/Internal/CppCRecordMetadataCodec.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
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
    const auto againTable = collectUsrs(*again);
    const auto renamedTable = collectUsrs(*renamed);
    printTables(firstTable, againTable, renamedTable);
    first->close();
    again->close();
    renamed->close();
    return checkUsrTables(firstTable, againTable, renamedTable);
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

// EN: Opens the a.h and b.h sessions as two references of one project.
// ES: Abre las sesiones de a.h y b.h como dos referencias de un mismo proyecto.
std::vector<CppProjectSessions> twoHeaderSessions(CppClangSessionFactory& factory, const std::string& resource,
                                                  const std::filesystem::path& directory, const std::string& driver,
                                                  const std::filesystem::path& project)
{
    CppProjectSessions group;
    group.projectFile = project;
    group.sessions.push_back(openSession(factory, headerOptions(resource, directory, driver, "a.h")));
    group.sessions.push_back(openSession(factory, headerOptions(resource, directory, driver, "b.h")));
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
        auto inlineSelected = selectCppSemanticFunctions(*entries, 0, "cid_inline");
        const auto* inlineGroups = std::get_if<CppSemanticFunctionGroups>(&inlineSelected);
        std::cout << "OBSERVATION static inline cid_inline from a.h and b.h: "
                  << (inlineGroups ? std::to_string(inlineGroups->groups.size()) + " group(s)"
                                   : "selection failure: " +
                                     std::get<CppSemanticFunctionGroupFailure>(inlineSelected).message) << '\n';
    }
    auto globals = collectCppSemanticGlobals(*graph, compilation.cppProjectSessions());
    const auto* globalEntries = std::get_if<std::vector<CppSemanticGlobalEntry>>(&globals);
    ok &= expect(globalEntries != nullptr, "two-header global inventory is collected");
    if (globalEntries)
    {
        const auto selection = selectCppSemanticGlobal(*globalEntries, 0, "cid_global");
        ok &= expect(selection.candidate && selection.conflict.empty() && selection.candidate->clangUsr,
                     "one external C global declared by two headers is selected with its USR");
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
                            twoHeaderSessions(factory, resource, directory, driver, project), error) &&
        compilation.compileProjectNames(project, error);
    if (!loaded) std::cerr << "two-header project: " << error << '\n';
    if (!expect(loaded, "two-header project loads its names")) return false;
    const bool external = checkExternalSelection(compilation);
    const bool tagged = checkTaggedTypes(compilation);
    return external && tagged;
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

// EN: Binds the typedef name CIdInt declared by two sessions whose headers are `first` and
//     `second` (identical text). Returns true when the project names compile without errors.
// ES: Enlaza el typedef CIdInt declarado por dos sesiones cuyos encabezados son `first` y
//     `second` (texto idéntico). Devuelve true si los nombres del proyecto compilan sin errores.
bool bindTwoSessionAlias(CppClangSessionFactory& factory, const std::string& resource,
                         const std::filesystem::path& directory, const std::string& driver,
                         const std::string& first, const std::string& second)
{
    const auto project = writeProject(directory / ("alias-" + first + "-" + second), "CIdentityAlias",
                                      "int Use(CIdInt value) { return value; } int Main() { return 0; }\n");
    CppProjectSessions group;
    group.projectFile = project;
    group.sessions.push_back(openSession(factory, headerOptions(resource, directory, driver, first)));
    group.sessions.push_back(openSession(factory, headerOptions(resource, directory, driver, second)));
    std::vector<CppProjectSessions> groups;
    groups.push_back(std::move(group));
    nativesharp::frontend::ProjectCompilation compilation;
    std::string error;
    const bool configured = !project.empty() && compilation.setCppProjectSessions(std::move(groups), error) &&
        compilation.setCppTypeAliasProvider(provideCppTypeAliases, error);
    const bool bound = configured && compilation.compileProjectNames(project, error) &&
        !compilation.diagnostics().hasErrors();
    std::cout << "OBSERVATION typedef CIdInt from " << first << " and " << second << ": bound="
              << (bound ? "yes" : "no") << " diagnostics=" << diagnosticText(compilation)
              << " error='" << error << "'\n";
    return bound;
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
    const bool control = bindTwoSessionAlias(factory, resource, directory, driver, "a.h", "a.h");
    bindTwoSessionAlias(factory, resource, directory, driver, "a.h", "b.h");
    return expect(control, "one typedef observed by two sessions of the same header binds");
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

// EN: Entry of the probe mode. Writes a.h and b.h with identical text and runs probes 1-4.
// ES: Entrada del modo de sondas. Escribe a.h y b.h con texto idéntico y ejecuta las sondas 1-4.
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
        const auto clang = std::find_if(live.begin(), live.end(), [&](const auto& item) { return item.name == name; });
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
    if (!metadata_detail::decodeVersionedCTypeCatalog(metadata.cppCRecordCatalog, records, enums, error))
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
}

int main(int argc, char** argv)
{
    if (argc == 7 && std::string_view(argv[1]) == "--verify-restored-c-usr") return verifyRestoredUsrs(argv);
    if (argc != 4)
    {
        std::cerr << "usage: nsc_cpp_c_identity_tests <clang-resource-dir> <work-dir> <clang-driver>\n"
                     "       nsc_cpp_c_identity_tests --verify-restored-c-usr <nsmeta> <header> "
                     "<clang-resource-dir> <clang-driver> <work-dir>\n";
        return 2;
    }
    return runProbes(argv[1], argv[2], argv[3]);
}
