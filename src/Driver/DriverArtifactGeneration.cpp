#include "Internal/DriverInternal.h"
#include "Internal/ProjectCommandSupport.h"
#include "ThinLtoLibraryLinking.h"
#include "../LLVM/Internal/CppTlsDllAlias.h"

#include "nativesharp/Linker/ExternalImportValidation.h"
#include "nativesharp/Interop/CppBinaryDependencies.h"
#include "nativesharp/Basic/ArtifactTransaction.h"
#include "nativesharp/Basic/Sha256.h"
#include <algorithm>
#include <iterator>
#include <set>
#ifdef NSC_ENABLE_CPP_INTEROP
#include "nativesharp/Frontend/ProjectConstructionModel.h"
#include "nativesharp/Frontend/ProjectNamespaceFunctionModel.h"
#include "nativesharp/Frontend/PrecompiledSyntaxModule.h"
#include "nativesharp/Interop/CppSemanticInventory.h"
#include "nativesharp/Interop/CppSemanticFunction.h"
#include "nativesharp/Interop/CppMemberThunk.h"
#include "nativesharp/LLVM/CppLifetimeObjectEmitter.h"
#include "../Interop/Internal/CppCRecordMetadataCodec.h"
#endif
// EN: T08-D19 template dependencies are NativeSharp surface, needed with or without C/C++ interoperability.
// ES: Las dependencias de plantilla de T08-D19 son superficie NativeSharp, necesarias con o sin interop C/C++.
#include "../Interop/Internal/TemplateDependencyMembers.h"
#if defined(_WIN32)
#include "ProductContractLock.h"
#include "nativesharp/Frontend/ClassImplicitLifecycleProof.h"
#include "nativesharp/Frontend/ClassReceiverContractManifest.h"
#include <algorithm>
#include <fstream>
#endif

namespace nativesharp::driver
{
// Direct linked products have no project-command selection; inspect static inputs before caching.
static std::string selectDirectLinkedProductLinker(
    const compilation::CompilationInputManifest& manifest, std::string_view requested,
    const std::vector<std::filesystem::path>& extraLinkLibraries)
{
    if (!requested.empty()) return std::string(requested);
    // EN: T15. NSLink does not link the MSVC C runtime (its COMDAT rules reject libucrt), so a product started
    //     through it is linked with lld-link unless a linker is requested on the command line.
    // ES: T15. NSLink no enlaza el runtime de C de MSVC (sus reglas COMDAT rechazan libucrt), asi que un producto
    //     que arranca por el se enlaza con lld-link salvo que la linea de ordenes pida un enlazador.
    if (activeExecutionScope != nullptr && activeExecutionScope->crtStartup) return "LLD";
    if (environmentValue("NSC_LINKER")) return std::string(requested);
    std::set<std::filesystem::path> inspected;
    const auto requiresLld = [&](const std::filesystem::path& path) {
        if (path.empty() || !inspected.insert(path).second) return false;
        bool required = false;
        std::string inspectionError;
        if (!llvm_backend::ProjectOopObjectGenerator::requiresLldForStaticInput(path, required, inspectionError))
            return true;
        return required;
    };
    for (const auto& project : manifest.projectGraph().projects())
    {
        for (const auto& reference : project.cppLibraryReferences())
            if (reference.runtimePath.empty() && requiresLld(reference.libraryPath)) return "LLD";
        for (const auto& reference : project.libraryReferences())
            if ((reference.runtimePath.empty() && requiresLld(reference.libraryPath)) ||
                precompiledReferenceRequiresLld(reference, manifest.loadOptions())) return "LLD";
    }
    for (const auto& library : extraLinkLibraries)
        if (requiresLld(library)) return "LLD";
    return {};
}

static bool appendCStaticInlineObjects(frontend::ProjectCompilation &compilation,
                                       llvm_backend::ProjectOopObjectBundle &bundle,
                                       llvm_backend::LlvmOptimizationLevel level, std::string &error)
{
#ifdef NSC_ENABLE_CPP_INTEROP
    if (compilation.cppProjectSessions().empty()) { error.clear(); return true; }
    if (!compilation.projectGraph() || !compilation.namespaceFunctionModel())
    { error = "C inline emission requires completed project models"; return false; }
    auto inventory = interop::collectCppSemanticFunctions(*compilation.projectGraph(), compilation.cppProjectSessions());
    if (const auto* failure = std::get_if<interop::CppSemanticInventoryFailure>(&inventory))
    { error = "C inline inventory failed: " + failure->message; return false; }
    struct Group {
        const interop::CppInteropSession* session;
        std::vector<interop::CppCInlineIRExport> exports;
        std::vector<interop::CppCInlineIRExport> globals;
    };
    std::vector<Group> groups;
    // T19-039: the symbols of the C complex functions that MIR calls. / Los simbolos de las funciones C complejas
    //     que llama MIR.
    std::optional<std::set<std::string>> calledComplexSymbols;
    for (const auto& entry : std::get<std::vector<interop::CppSemanticInventoryEntry>>(inventory))
    {
        if (!entry.declaration.isCStaticInlineDefinition || !entry.declaration.externalSymbol) continue;
        // EN: T19-039. A C operator or conversion of a `_Complex` carrier is imported whenever an expression could
        //     use it, so it is emitted only when the program calls it: the multiplication and division need
        //     compiler-rt, which a program without complex arithmetic does not link.
        // ES: T19-039. Un operador o conversion C de un portador `_Complex` se importa siempre que una expresion
        //     podria usarlo, asi que se emite solo si el programa lo llama: la multiplicacion y la division necesitan
        //     compiler-rt, que un programa sin aritmetica compleja no enlaza.
        if (entry.declaration.cMacroWrapper.starts_with("__nsc_complex_"))
        {
            if (!calledComplexSymbols)
            {
                calledComplexSymbols.emplace();
                const auto* mir = compilation.oopMirModel();
                const auto functions = compilation.namespaceFunctionModel()->functions();
                std::vector<const frontend::ProjectOopMirNode*> pending;
                if (mir)
                    for (const auto& function : mir->functions())
                        if (function.body) pending.push_back(function.body.get());
                while (!pending.empty())
                {
                    const auto* node = pending.back();
                    pending.pop_back();
                    if (node->namespaceFunctionOrdinal && *node->namespaceFunctionOrdinal < functions.size())
                        calledComplexSymbols->insert(functions[*node->namespaceFunctionOrdinal].mangledName);
                    for (const auto& child : node->children)
                        if (child) pending.push_back(child.get());
                }
            }
            if (!calledComplexSymbols->contains(*entry.declaration.externalSymbol)) continue;
        }
        bool selected = false;
        for (const auto& function : compilation.namespaceFunctionModel()->functions())
            if (function.projectOrdinal == entry.projectOrdinal && function.mangledName == *entry.declaration.externalSymbol)
            { selected = true; break; }
        if (!selected)
            for (const auto& request : compilation.cppTemplateDemands())
                if (request.demand.lookupName == entry.declaration.name &&
                    std::find(request.demand.visibleProjects.begin(), request.demand.visibleProjects.end(), entry.projectOrdinal) != request.demand.visibleProjects.end() &&
                    std::find(request.demand.lookupNamespaces.begin(), request.demand.lookupNamespaces.end(), "") != request.demand.lookupNamespaces.end())
                { selected = true; break; }
        if (!selected) continue;
        const interop::CppInteropSession* session = nullptr;
        for (const auto& project : compilation.cppProjectSessions())
            if (project.projectFile == entry.projectFile && entry.referenceIndex < project.sessions.size())
            { session = project.sessions[entry.referenceIndex].get(); break; }
        if (!session) { error = "C inline function has no matching Clang session"; return false; }
        auto found = std::find_if(groups.begin(), groups.end(),
            [&](const Group& group) { return group.session == session; });
        if (found == groups.end()) { groups.push_back({session, {}, {}}); found = std::prev(groups.end()); }
        // T19-003: a macro wrapper is found by its own C name. / Un envoltorio de macro se busca por su nombre C.
        found->exports.push_back({entry.declaration.cMacroWrapper.empty() ? entry.declaration.name
                                                                          : entry.declaration.cMacroWrapper,
                                  *entry.declaration.externalSymbol});
    }
    auto globalInventory = interop::collectCppSemanticGlobals(*compilation.projectGraph(), compilation.cppProjectSessions());
    if (const auto* failure = std::get_if<interop::CppSemanticInventoryFailure>(&globalInventory))
    { error = "C internal global inventory failed: " + failure->message; return false; }
    for (const auto& entry : std::get<std::vector<interop::CppSemanticGlobalEntry>>(globalInventory))
    {
        if (!entry.cLanguage || entry.declaration.externalLinkage) continue;
        const auto symbol = interop::cppCInlineLinkSymbol(entry.projectFile, entry.referenceIndex,
                                                        "global:" + entry.declaration.name);
        const auto states = compilation.namespaceFunctionModel()->states();
        bool selected = std::any_of(states.begin(), states.end(), [&](const auto& state) {
            return state.projectOrdinal == entry.projectOrdinal && state.isCppExternal && state.storageName == symbol;
        });
        if (!selected)
            for (const auto& request : compilation.cppTemplateDemands())
                if (request.demand.lookupName == entry.declaration.name &&
                    std::find(request.demand.visibleProjects.begin(), request.demand.visibleProjects.end(), entry.projectOrdinal) != request.demand.visibleProjects.end() &&
                    std::find(request.demand.lookupNamespaces.begin(), request.demand.lookupNamespaces.end(), "") != request.demand.lookupNamespaces.end())
                { selected = true; break; }
        if (!selected) continue;
        const interop::CppInteropSession* session = nullptr;
        for (const auto& project : compilation.cppProjectSessions())
            if (project.projectFile == entry.projectFile && entry.referenceIndex < project.sessions.size())
            { session = project.sessions[entry.referenceIndex].get(); break; }
        if (!session) { error = "C internal global has no matching Clang session"; return false; }
        auto found = std::find_if(groups.begin(), groups.end(),
            [&](const Group& group) { return group.session == session; });
        if (found == groups.end()) { groups.push_back({session, {}, {}}); found = std::prev(groups.end()); }
        // EN: T19-041. A macro that is not a constant exports its accessor function under the global's symbol.
        // ES: T19-041. Una macro no constante exporta su funcion accesora con el simbolo de la global.
        if (!entry.declaration.cMacroAccessor.empty())
        {
            found->exports.push_back({entry.declaration.cMacroAccessor, symbol});
            continue;
        }
        // T19-003: a macro value is found by its own C name. / El valor de una macro se busca por su nombre C.
        found->globals.push_back({entry.declaration.cMacroStorage.empty() ? entry.declaration.name
                                                                          : entry.declaration.cMacroStorage, symbol});
    }
    if (groups.empty()) { error.clear(); return true; }
    return bundle.appendGeneratedObjects(groups.size(),
        [&](std::span<const std::filesystem::path> paths, std::string& emitError)
        {
            for (std::size_t i = 0; i < groups.size(); ++i)
                if (!llvm_backend::emitCStaticInlineObject(*groups[i].session, groups[i].exports, groups[i].globals,
                        level, paths[i], emitError)) return false;
            return true;
        }, error);
#else
    (void)compilation; (void)bundle; (void)level;
    error.clear(); return true;
#endif
}

// EN: T10-012. Every C++ symbol the program imports is offered to the Clang sessions of the project; a
//     session emits the ones it defines inline (functions and inline variables), which a library need
//     not export. A symbol is emitted once, by the first session that defines it.
// ES: T10-012. Cada simbolo C++ que importa el programa se ofrece a las sesiones Clang del proyecto; una
//     sesion emite los que define inline (funciones y variables inline), que una biblioteca no tiene
//     por que exportar. Cada simbolo se emite una vez, en la primera sesion que lo define.
static bool appendCppInlineObjects(frontend::ProjectCompilation &compilation,
                                   const frontend::ProjectOopMirModel &mir,
                                   llvm_backend::ProjectOopObjectBundle &bundle,
                                   llvm_backend::LlvmOptimizationLevel level,
                                   llvm_backend::DebugInformationKind debugInformation, std::string &error)
{
#ifdef NSC_ENABLE_CPP_INTEROP
    error.clear();
    if (compilation.cppProjectSessions().empty()) return true;
    std::vector<std::string> remaining;
    for (const auto& function : mir.functions())
    {
        if ((!function.isImportedC && !function.isCppExternal) || function.cppDllImport ||
            !function.importDllName.empty()) continue;
        const auto& symbol = function.cAbiName.empty() ? function.mangledName : function.cAbiName;
        if (!symbol.empty()) remaining.push_back(symbol);
    }
    if (compilation.namespaceFunctionModel())
        for (const auto& state : compilation.namespaceFunctionModel()->states())
            if (state.isCppExternal && !state.storageName.empty()) remaining.push_back(state.storageName);
    if (compilation.memberModel())
        for (const auto& member : compilation.memberModel()->members())
            if (member.isCppExternal && !member.cppExternalSymbol.empty()) remaining.push_back(member.cppExternalSymbol);
    std::sort(remaining.begin(), remaining.end());
    remaining.erase(std::unique(remaining.begin(), remaining.end()), remaining.end());
    std::vector<std::string> modules;
    const interop::CppDebugInformation cppDebugInformation =
        debugInformation == llvm_backend::DebugInformationKind::Full ? interop::CppDebugInformation::Full :
        debugInformation == llvm_backend::DebugInformationKind::LineTablesOnly ? interop::CppDebugInformation::LineTables :
        interop::CppDebugInformation::None;
    for (const auto& project : compilation.cppProjectSessions())
        for (const auto& session : project.sessions)
        {
            if (remaining.empty() || !session) break;
            std::vector<std::string> emitted;
            auto ir = session->emitCppInlineIR(remaining, emitted, cppDebugInformation);
            if (!ir || emitted.empty()) continue;
            modules.push_back(std::move(*ir));
            const std::set<std::string> done(emitted.begin(), emitted.end());
            std::erase_if(remaining, [&](const std::string& symbol) { return done.contains(symbol); });
        }
    // EN: T12-004. The member adapters still requested are generated by the session whose translation unit
    //     defines the derived record.
    // ES: T12-004. Los adaptadores de miembro que siguen pedidos los genera la sesion cuya unidad de
    //     traduccion define el record derivado.
    for (const auto& project : compilation.cppProjectSessions())
        for (const auto& session : project.sessions)
        {
            if (!session || std::none_of(remaining.begin(), remaining.end(), [](const std::string& symbol) {
                    return std::string_view(symbol).starts_with(interop::cppMemberThunkPrefix); })) break;
            std::vector<std::string> emitted;
            auto irs = session->emitCppMemberThunkIR(remaining, emitted);
            for (auto& ir : irs) modules.push_back(std::move(ir));
            const std::set<std::string> done(emitted.begin(), emitted.end());
            std::erase_if(remaining, [&](const std::string& symbol) { return done.contains(symbol); });
        }
    if (modules.empty()) return true;
    return bundle.appendGeneratedObjects(modules.size(),
        [&](std::span<const std::filesystem::path> paths, std::string& emitError)
        {
            for (std::size_t i = 0; i < modules.size(); ++i)
                if (!llvm_backend::emitCppInlineObject(modules[i], level, paths[i], emitError)) return false;
            return true;
        }, error);
#else
    (void)compilation; (void)mir; (void)bundle; (void)level; (void)debugInformation;
    error.clear(); return true;
#endif
}

bool appendCppImplicitLifetimeObjects(frontend::ProjectCompilation &compilation,
                                      const frontend::ProjectOopMirModel &mir,
                                      llvm_backend::ProjectOopObjectBundle &bundle,
                                      llvm_backend::LlvmOptimizationLevel level, std::string &error)
{
#ifdef NSC_ENABLE_CPP_INTEROP
    if (compilation.cppProjectSessions().empty()) { error.clear(); return true; }
    if (!compilation.projectGraph() || !compilation.nameLookup() || !compilation.memberModel() ||
        !compilation.constructionModel())
    { error = "C++ lifetime emission requires completed project models"; return false; }
    std::vector<interop::CppImplicitLifetimeEmissionRequest> plan;
    if (!interop::planCppImplicitLifetimeFromValues(*compilation.projectGraph(),
            compilation.cppProjectSessions(), *compilation.nameLookup(), *compilation.memberModel(),
            compilation.constructionModel()->constructions(), mir, plan, error)) return false;
    if (plan.empty()) { error.clear(); return true; }
    return bundle.appendGeneratedObjects(plan.size(),
        [&](std::span<const std::filesystem::path> paths, std::string &emitError)
        {
            std::vector<std::filesystem::path> generated;
            return llvm_backend::emitCppImplicitLifetimeObjects(plan, paths, level, generated, emitError);
        }, error);
#else
    (void)compilation; (void)mir; (void)bundle; (void)level;
    error.clear();
    return true;
#endif
}
using target::BuiltinLayoutKind;

// Interop Foundation: gather the project's `extern C from "<dll>"` declarations
// as (dll -> its symbols), in first-appearance order, for the linker's
// import-library step. The compiler learned no API by name at any point -- this
// is purely the shape the frontend recorded.
std::vector<linker::ExternalImportModule> collectExternalImports(const frontend::ProjectOopMirModel &mirModel)
{
    std::vector<linker::ExternalImportModule> modules;
    ExternalImportListBuilder moduleList(modules);
    bool anyRuntimeBoundImport = false;
    const auto addSymbol = [&moduleList](const std::string &dllName, const std::string &symbol)
    { moduleList.append(dllName, symbol); };
    for (const frontend::ProjectOopMirFunction &function : mirModel.functions())
    {
        if (!function.isImportedC || function.importDllName.empty())
            continue;
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

// EN: T08-D11/D12. A successful compilation still reports the C interop warnings: the integer operations whose
//     NativeSharp result differs from C carry the explicit correction. Other warnings keep their current behavior.
// ES: T08-D11/D12. Una compilacion correcta informa igual los avisos de interoperabilidad C: las operaciones
//     enteras cuyo resultado NativeSharp difiere del de C llevan la correccion explicita. Los demas avisos
//     conservan su comportamiento actual.
static void renderCppInteropWarnings(const frontend::ProjectCompilation &compilation, std::ostream &output)
{
    const diagnostics::DiagnosticRenderer renderer(compilation.sourceManager());
    for (const auto &diagnostic : compilation.diagnostics().diagnostics())
        if (diagnostic.severity() == diagnostics::DiagnosticSeverity::Warning &&
            diagnostic.id() == diagnostics::DiagnosticId::CppInteropDiagnostic)
            renderer.render(diagnostic, output);
}

// An import library for the packaged name must precede the original import archive.
// Only declarations with an unambiguous Clang origin are added here; other cases
// retain the existing post-link diagnostic instead of selecting an arbitrary DLL.
static bool appendCppTlsAliasedFunctionImports(
    const frontend::ProjectCompilation &compilation,
    const std::vector<std::filesystem::path> &runtimeLibraries,
    std::vector<linker::ExternalImportModule> &imports, std::string &error)
{
#ifdef NSC_ENABLE_CPP_INTEROP
    if (!compilation.projectGraph() || !compilation.oopMirModel())
        return true;
    auto runtimeCandidates = runtimeLibraries;
    std::vector<std::filesystem::path> dynamicRuntimes;
    linker::PeExecutableInfo beforeLink;
    if (!appendReferencedCppRuntimes(compilation, beforeLink, runtimeCandidates, dynamicRuntimes, error))
        return false;
    CppTlsRuntimeAliases aliases;
    if (!selectCppTlsRuntimeAliases(compilation, dynamicRuntimes, aliases, error)) return false;
    if (aliases.empty()) return true;
    auto inventory = interop::collectCppSemanticFunctions(*compilation.projectGraph(), compilation.cppProjectSessions());
    if (const auto *failure = std::get_if<interop::CppSemanticInventoryFailure>(&inventory))
    { error = "C/C++ import inventory failed: " + failure->message; return false; }
    std::unordered_set<std::string> required;
    for (const auto &function : compilation.oopMirModel()->functions())
        if (function.isImportedC && function.cppDllImport)
            required.insert(function.mangledName);
    std::unordered_map<std::string, std::string> origins;
    const auto addOrigin = [&](const std::string &name, const std::string &identity) {
        const auto [it, inserted] = origins.emplace(name, identity);
        if (!inserted && it->second != identity) it->second.clear();
    };
    for (const auto &entry : std::get<std::vector<interop::CppSemanticInventoryEntry>>(inventory))
    {
        const auto *symbol = std::get_if<frontend::NamespaceFunctionSymbol>(&entry.projection);
        const std::string name = symbol ? symbol->mangledName : entry.declaration.externalSymbol.value_or("");
        if (!required.contains(name)) continue;
        const auto &references = compilation.projectGraph()->projects()[entry.projectOrdinal].cppLibraryReferences();
        std::string identity;
        if (symbol && entry.referenceIndex < references.size() && !references[entry.referenceIndex].runtimePath.empty())
        {
            const auto &path = references[entry.referenceIndex].runtimePath;
            basic::Sha256Digest digest{};
            std::uintmax_t size = 0;
            if (!basic::sha256File(path, digest, size, error)) return false;
            identity = pathToUtf8(path.filename()) + ":" + basic::sha256Hex(digest);
        }
        addOrigin(name, identity);
    }
    for (const auto &module : compilation.precompiledModules())
    {
        if (module.cppCTypeCatalog.empty()) continue;
        std::vector<interop::CppCRecordSnapshot> records;
        std::vector<interop::CppCEnumSnapshot> enums;
        interop::metadata_detail::CDependencyCatalog dependencies;
        if (!interop::metadata_detail::decodeCTypeCatalogAndDependencies(
                module.cppCTypeCatalog, records, enums, dependencies, error)) return false;
        for (const auto &function : dependencies.functions)
        {
            if (!required.contains(function.linkSymbol)) continue;
            const auto marker = function.referenceIdentity.rfind("#dll:");
            addOrigin(function.linkSymbol, marker == std::string::npos ? std::string{} :
                function.referenceIdentity.substr(marker + 5));
        }
    }
    ExternalImportListBuilder builder(imports);
    for (const auto &[symbol, identity] : origins)
    {
        if (identity.empty()) continue;
        const std::string alias = interop::cppTlsDllAlias(identity);
        if (alias.empty() || std::none_of(aliases.begin(), aliases.end(),
                [&](const auto &item) { return item.second == alias; })) continue;
        const bool claimed = std::any_of(imports.begin(), imports.end(), [&](const auto &module) {
            return foldedName(module.dllName) != foldedName(alias) &&
                std::find(module.symbols.begin(), module.symbols.end(), symbol) != module.symbols.end();
        });
        if (!claimed) builder.append(alias, symbol);
    }
#else
    (void)compilation; (void)runtimeLibraries; (void)imports; (void)error;
#endif
    return true;
}

std::unique_ptr<llvm_backend::ProjectOopObjectBundle> generateProjectObjects(
    frontend::ProjectCompilation &compilation, llvm_backend::DebugInformationKind debugInformation,
    llvm_backend::LlvmOptimizationLevel optimizationLevel,
    const std::vector<llvm_backend::DebugPrefixMap> &debugPrefixMaps,
    llvm_backend::LinkTimeOptimizationMode linkTimeOptimization, std::string &error,
    bool captureThinLtoUnits = false, std::vector<llvm_backend::ThinLtoLibraryUnits> thinLtoLibraries = {},
    std::optional<std::vector<std::string>> thinLtoPreservedSymbols = std::nullopt)
{
    llvm_backend::ProjectOopObjectGenerator objectGenerator;
    configureObjectGeneratorProgress(objectGenerator);
    if (activeExecutionScope != nullptr && !activeExecutionScope->addressSanitizerClang.empty())
        objectGenerator.setAddressSanitizer(true);
    objectGenerator.setThinLtoUnitBitcodeCapture(captureThinLtoUnits);
    objectGenerator.setThinLtoLibraryUnits(std::move(thinLtoLibraries));
    // Native objects emitted from Clang sessions are appended after ThinLTO.
    // Their undefined references cannot be scanned before the backend runs,
    // so retain all externally visible definitions for mixed compilations.
    if (!compilation.cppProjectSessions().empty()) thinLtoPreservedSymbols.reset();
    objectGenerator.setThinLtoPreservedSymbols(std::move(thinLtoPreservedSymbols));
    std::unique_ptr<llvm_backend::ProjectOopObjectBundle> objectBundle =
        measureDriverPhase("llvm-object-generation",
                           [&]
                           {
                               return objectGenerator.generate(
                                   *compilation.nameLookup(), *compilation.memberModel(), *compilation.oopMirModel(),
                                   compilation.sourceManager(), debugInformation, optimizationLevel,
                                   effectiveCompilationJobs(), debugPrefixMaps, linkTimeOptimization, error,
                                   &compilation.lifetime().workerPool());
                           });
    if (objectBundle == nullptr)
        return nullptr;
    if (!appendCppImplicitLifetimeObjects(compilation, *compilation.oopMirModel(),
                                          *objectBundle, optimizationLevel, error))
        return nullptr;
    if (!appendCStaticInlineObjects(compilation, *objectBundle, optimizationLevel, error))
        return nullptr;
    if (!appendCppInlineObjects(compilation, *compilation.oopMirModel(), *objectBundle, optimizationLevel,
                                debugInformation, error))
        return nullptr;
    if (!compilation.completeCodeGeneration(error))
        return nullptr;
    return objectBundle;
}

bool prepareExecutableProject(std::string_view inputPath,
                              compilation::CompilationInputManifest &manifest,
                              frontend::ProjectCompilation &compilation,
                              const frontend::ProjectOopMirFunction *&entryFunction,
                               bool &restored, std::ostream &error)
{
    configureProjectExecution(compilation);
    std::string loadError;
    if (!measureDriverPhase(
            "precompiled-metadata",
            [&] { return configurePrecompiledModules(manifest, compilation, loadError, true); }))
    {
        error << "nsc: error: " << loadError << '\n';
        if (!compilation.diagnostics().diagnostics().empty())
            diagnostics::DiagnosticRenderer(compilation.sourceManager()).renderAll(compilation.diagnostics(), error);
        return false;
    }
    if (!restoreCompilationBundle(manifest, restored, loadError))
    {
        error << "nsc: error: compilation bundle restore failed: "
              << loadError << '\n';
        return false;
    }
    if (restored) return true;
    if (!compilation.compileProjectOopMir(pathFromUtf8(inputPath), loadError))
    {
        if (!loadError.empty())
            error << "nsc: error: " << loadError << '\n';
        if (compilation.diagnostics().hasErrors())
        {
            diagnostics::DiagnosticRenderer renderer(compilation.sourceManager());
            renderer.renderAll(compilation.diagnostics(), error);
        }
        return false;
    }
    renderCppInteropWarnings(compilation, error);
    // EN: LKF-108. A successful build still shows its warnings (for example stackalloc in a loop);
    //     only a failed one rendered diagnostics before.
    // ES: LKF-108. Una compilacion correcta tambien muestra sus advertencias (por ejemplo stackalloc
    //     en un bucle); antes solo una fallida mostraba diagnosticos.
    if (compilation.diagnostics().warningCount() != 0U)
    {
        diagnostics::DiagnosticRenderer renderer(compilation.sourceManager());
        renderer.renderAll(compilation.diagnostics(), error);
    }
    if (compilation.nameLookup() == nullptr || compilation.memberModel() == nullptr ||
        compilation.oopMirModel() == nullptr)
    {
        error << "nsc: error: Stage 45.12 OOP executable compilation did not "
                 "produce required lowering models\n";
        return false;
    }
    if (compilation.namespaceFunctionModel() == nullptr)
    {
        error << "nsc: internal error: namespace function model is unavailable" << std::endl;
        return false;
    }
    bool ambiguousEntryPoint = false;
    entryFunction =
        compilation.oopMirModel()->executableEntryPoint(*compilation.namespaceFunctionModel(), ambiguousEntryPoint);
    if (ambiguousEntryPoint)
    {
        error << "nsc: error: OOP executable has multiple int Main() entry points" << std::endl;
        return false;
    }
    if (entryFunction == nullptr)
    {
        error << "nsc: error: OOP executable requires exactly one int Main() "
                 "or int Main(string[] args) function or static method"
              << std::endl;
        return false;
    }
    return true;
}

void printExecutableGenerationResult(const frontend::ProjectOopMirFunction &entryFunction,
                                     const llvm_backend::ProjectOopObjectBundle &objectBundle,
                                     const linker::WindowsExecutableBuilder &builder,
                                     llvm_backend::LlvmOptimizationLevel optimizationLevel,
                                     llvm_backend::DebugInformationKind debugInformation,
                                     llvm_backend::LinkTimeOptimizationMode linkTimeOptimization, bool incrementalLink,
                                     linker::WindowsExecutableSubsystem subsystem, std::size_t linkedLibraryCount,
                                     std::size_t packagedRuntimeCount, std::ostream &output)
{
    output << "project OOP PE executable emission: ok\n"
           << "entry-symbol: " << entryFunction.mangledName << '\n'
           << "machine: AMD64 (0x8664)\n"
           << "format: PE32+\n"
           << "subsystem: " << subsystemDisplayName(subsystem) << '\n'
           << "optimization: " << llvm_backend::llvmOptimizationLevelName(optimizationLevel) << '\n'
           << "debug-information: " << llvm_backend::debugInformationKindName(debugInformation) << '\n'
           << "link-time-optimization: " << llvm_backend::linkTimeOptimizationModeName(linkTimeOptimization) << '\n'
           << "code-units: " << objectBundle.codeUnitCount() << '\n'
           << "artifact-cache-hits: " << objectBundle.cacheHitCount() << '\n'
           << "artifact-cache-misses: " << objectBundle.cacheMissCount() << '\n'
           << "thinlto-summaries: " << objectBundle.thinLtoSummaryCount() << '\n'
           << "thinlto-index: "
           << (objectBundle.thinLtoIndexDigest().empty() ? "none" : objectBundle.thinLtoIndexDigest()) << '\n'
           << "thinlto-imports: " << objectBundle.thinLtoImportCount() << '\n'
           << "thinlto-cached-backends: " << objectBundle.thinLtoCachedBackendCount() << '\n'
           << "incremental-link: "
           << (incrementalLink ? (builder.reusedIncrementalLink() ? "hit" : "miss") : "disabled") << '\n'
           << "linked-libraries: " << linkedLibraryCount << '\n'
           << "packaged-runtimes: " << packagedRuntimeCount << '\n'
           << "sections: " << builder.info().sectionCount << '\n'
           << "entry-rva: " << builder.info().entryPointRva << '\n'
           << "exception-directory-rva: " << builder.info().exceptionDirectoryRva << '\n'
           << "exception-directory-size: " << builder.info().exceptionDirectorySize << '\n'
           << "imports:";
    for (const std::string &library : builder.info().importedLibraries)
        output << ' ' << library;
    output << '\n';
    if (activeExecutionScope != nullptr && activeExecutionScope->reportTiming)
        appendCodeUnitTelemetry(output, objectBundle);
}

/// EN: Freeze DEC-13. Under --validate-system-imports, checks the complete import list (own and
///     inherited) against the DLLs found on this host; without the option it does nothing, so the
///     artifact never depends on the host. Returns false and reports on `error` when a symbol is not
///     exported by its DLL.
/// ES: Congelamiento DEC-13. Con --validate-system-imports comprueba la lista completa de
///     imports (propios y heredados) contra las DLL que se encuentran en este host; sin la opcion no
///     hace nada, asi que el artefacto nunca depende del host. Devuelve false y lo reporta en `error`
///     cuando la DLL no exporta un simbolo.
bool validateSystemImportsWhenRequested(std::span<const linker::ExternalImportModule> externalImports,
                                        std::ostream &error)
{
    if (activeExecutionScope == nullptr || !activeExecutionScope->validateSystemImports) return true;
    std::string validationError;
    if (linker::validateExternalImportExports(externalImports, validationError)) return true;
    error << "nsc: error: system import validation failed: " << validationError << '\n';
    return false;
}

// EN: T16-003. A C or C++ dynamic initializer (`.CRT$XCU`, `.CRT$XIU`) that the link takes, from a static library
//     member or from the C++ code emitted for the program, relies on the C runtime startup to call it. The entry
//     of NativeSharp does not, so the program would read the objects before their construction: the link is
//     rejected, naming the input, unless an executable starts through the C runtime (--crt-startup). A C or C++
//     DLL runs its own initializers.
// ES: T16-003. Un inicializador dinamico de C o C++ (`.CRT$XCU`, `.CRT$XIU`) que toma el enlace, de un miembro de
//     una biblioteca estatica o del codigo C++ emitido para el programa, depende de que el arranque del runtime
//     de C lo llame. La entrada de NativeSharp no lo hace, asi que el programa leeria los objetos antes de
//     construirlos: el enlace se rechaza nombrando la entrada, salvo en un ejecutable que arranca por el runtime
//     de C (--crt-startup). Una DLL C o C++ ejecuta sus propios inicializadores.
static bool rejectUnstartedCrtInitializers(std::span<const std::filesystem::path> objects,
                                           std::span<const std::filesystem::path> linkLibraries, bool executable,
                                           std::ostream &error)
{
    if (executable && activeExecutionScope != nullptr && activeExecutionScope->crtStartup) return true;
    std::string input;
    std::string member;
    std::string inspectionError;
    if (!llvm_backend::ProjectOopObjectGenerator::findLinkedCrtInitializers(objects, linkLibraries, input, member,
                                                                           inspectionError))
    {
        error << "nsc: error: C/C++ initializer inspection failed: " << inspectionError << '\n';
        return false;
    }
    if (input.empty()) return true;
    error << "nsc: error: C/C++ library '" << input << "' has dynamic initializers";
    if (!member.empty()) error << " in its linked member '" << member << "'";
    error << ", which only the C runtime startup runs; "
          << (executable ? "build the executable with --crt-startup and the C runtime libraries (--link-lib), or "
                           "reference the library as a DLL\n"
                         : "a NativeSharp DLL does not start the C runtime, so reference the library as a DLL\n");
    return false;
}

// EN: T17-005, T17-006. After a failed link, each C/C++ function the program calls that no object defines and
//     no library provides is named with its declaration. When a library defines the same C++ name with another
//     decoration, the header and the library disagree on the declaration (types, convention or qualifiers),
//     which the linker alone reports as a missing symbol.
// ES: T17-005, T17-006. Tras un enlace fallido, cada funcion C/C++ que llama el programa y que ningun objeto
//     define ni ninguna biblioteca da se nombra con su declaracion. Si una biblioteca define el mismo nombre C++
//     con otra decoracion, el encabezado y la biblioteca discrepan en la declaracion (tipos, convencion o
//     calificadores), lo que el enlazador solo informa como simbolo ausente.
static void explainMissingCppSymbols(const frontend::ProjectCompilation &compilation,
                                     std::span<const std::filesystem::path> objects,
                                     std::span<const std::filesystem::path> linkLibraries, std::ostream &error)
{
    if (compilation.oopMirModel() == nullptr) return;
    std::vector<std::string> symbols;
    for (const auto &function : compilation.oopMirModel()->functions())
    {
        if ((!function.isImportedC && !function.isCppExternal) || !function.importDllName.empty()) continue;
        const auto &symbol = function.cAbiName.empty() ? function.mangledName : function.cAbiName;
        if (!symbol.empty()) symbols.push_back(symbol);
    }
    std::sort(symbols.begin(), symbols.end());
    symbols.erase(std::unique(symbols.begin(), symbols.end()), symbols.end());
    if (symbols.empty()) return;
    std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::string>>>> missing;
    std::string inspectionError;
    if (!llvm_backend::ProjectOopObjectGenerator::findMissingLinkSymbols(objects, linkLibraries, symbols, missing,
                                                                        inspectionError))
        return;
    for (const auto &[symbol, sameName] : missing)
    {
        std::string location;
        if (compilation.namespaceFunctionModel())
            for (const auto &function : compilation.namespaceFunctionModel()->functions())
                if (function.mangledName == symbol && function.cppLocation)
                {
                    location = interop::describeCppSourcePoint(*function.cppLocation);
                    break;
                }
        error << "nsc: error: C/C++ function '" << llvm_backend::ProjectOopObjectGenerator::demangleSymbol(symbol)
              << "' (symbol '" << symbol << "')";
        if (!location.empty()) error << ", declared at " << location << ",";
        if (sameName.empty())
        {
            error << " is not defined by any linked library\n";
            continue;
        }
        error << " is not defined with this declaration: the header and the library disagree on it\n";
        for (const auto &[library, other] : sameName)
            error << "nsc: note: '" << library << "' defines '"
                  << llvm_backend::ProjectOopObjectGenerator::demangleSymbol(other) << "' (symbol '" << other << "')\n";
    }
}

bool prepareExecutableBuilder(const frontend::ProjectCompilation &compilation,
                              const std::vector<std::filesystem::path> &extraLinkLibraries,
                              llvm_backend::DebugInformationKind debugInformation, bool incrementalLink,
                              linker::WindowsExecutableSubsystem subsystem,
                              std::vector<std::filesystem::path> &linkLibraries,
                              std::vector<std::filesystem::path> &runtimeLibraries,
                              linker::WindowsExecutableBuilder &builder, std::ostream &error)
{
    linkLibraries = extraLinkLibraries;
    std::vector<linker::ExternalImportModule> inheritedImports;
    std::string referenceError;
    if (!appendValidatedLibraryReferences(compilation, linkLibraries, runtimeLibraries, referenceError,
                                          &inheritedImports))
    {
        error << "nsc: error: precompiled library reference validation failed: " << referenceError << '\n';
        return false;
    }
    builder.setProgramObjectsVerified(true);
    builder.setRuntimeLibrary(linker::configuredWindowsRuntimeLibrary());
    builder.setSubsystem(subsystem);
    const bool crtStartup = activeExecutionScope != nullptr && activeExecutionScope->crtStartup;
    builder.setCrtStartup(crtStartup);
    builder.setDebugInformation(debugInformation);
    builder.setIncrementalLink(incrementalLink);
    std::vector<linker::ExternalImportModule> externalImports = collectExternalImports(*compilation.oopMirModel());
    ExternalImportListBuilder externalImportList(externalImports);
    for (const linker::ExternalImportModule &module : inheritedImports)
        for (const std::string &symbol : module.symbols)
            externalImportList.append(module.dllName, symbol);
    if (!validateSystemImportsWhenRequested(externalImports, error)) return false;
    if (!appendCppTlsAliasedFunctionImports(compilation, runtimeLibraries, externalImports, referenceError))
    {
        error << "nsc: error: C/C++ alias imports failed: " << referenceError << '\n';
        return false;
    }
    builder.setExternalImports(std::move(externalImports));
    builder.setExtraLinkLibraries(linkLibraries);
    return true;
}

// EN: The packaged runtimes a cached bundle must restore. A runtime already present with identical contents is
//     not staged again, but it is still part of the output. A name packaged only inside private assemblies
//     (T08-D05) has no copy next to the product.
// ES: Los runtimes empaquetados que un bundle de cache debe restaurar. Un runtime ya presente con el mismo
//     contenido no se vuelve a preparar, pero sigue formando parte de la salida. Un nombre empaquetado solo
//     dentro de ensamblados privados (T08-D05) no tiene copia junto al producto.
static void addRuntimeBundleOutputs(compilation::CompilationInputManifest &manifest,
                                    std::span<const std::filesystem::path> packagedRuntimes,
                                    std::span<const std::filesystem::path> runtimeLibraries,
                                    const CppTlsRuntimeAliases &aliases)
{
    std::unordered_set<std::string> rootNames;
    std::unordered_set<std::string> isolatedNames;
    for (const std::filesystem::path &runtime : packagedRuntimes)
    {
        const std::filesystem::path relative = runtimePackageRelativePath(runtime);
        manifest.addRequiredBundleRelativeOutput(relative);
        (relative.has_parent_path() ? isolatedNames : rootNames).insert(foldedPathKey(relative.filename()));
    }
    for (const std::filesystem::path &runtime : runtimeLibraries)
    {
        const auto alias = aliases.find(foldedPathKey(runtime));
        const std::filesystem::path name = alias == aliases.end() ? runtime.filename() : pathFromUtf8(alias->second);
        const std::string key = foldedPathKey(name);
        if (isolatedNames.contains(key) && !rootNames.contains(key)) continue;
        manifest.addRequiredBundleOutput(name);
    }
}

bool publishExecutableArtifact(std::string_view outputPath, const frontend::ProjectCompilation &compilation,
                               const frontend::ProjectOopMirFunction &entryFunction,
                               const llvm_backend::ProjectOopObjectBundle &objectBundle,
                               llvm_backend::LlvmOptimizationLevel optimizationLevel,
                               llvm_backend::DebugInformationKind debugInformation, bool incrementalLink,
                               std::vector<std::filesystem::path> &runtimeLibraries,
                               linker::WindowsExecutableBuilder &builder,
                               std::vector<std::filesystem::path> &packagedRuntimes,
                               CppTlsRuntimeAliases &aliases, std::ostream &error)
{
    std::string buildError;
    std::string referenceError;
    const std::filesystem::path destination = pathFromUtf8(outputPath);
    ArtifactPublisher artifactPublisher;
    ExecutablePublication publication;
    const bool publishPdb = debugInformation != llvm_backend::DebugInformationKind::None;
    if (!artifactPublisher.prepareExecutable(destination, publishPdb, incrementalLink, publication, buildError))
    {
        error << "nsc: error: executable output preflight failed: " << buildError << '\n';
        return false;
    }
    if (incrementalLink && !ArtifactPublisher::seedExecutable(publication, buildError))
    {
        error << "nsc: error: incremental-link staging failed: " << buildError << '\n';
        return false;
    }
    if (incrementalLink && publishPdb && !ArtifactPublisher::seedExecutablePdb(publication, buildError))
    {
        error << "nsc: error: incremental-link PDB staging failed: " << buildError << '\n';
        return false;
    }
    if (!measureDriverPhase("executable-build",
                            [&]
                            {
                                return builder.buildObjects(objectBundle.objectFiles(), objectBundle.combinedLlvmText(),
                                                            publication.stage, optimizationLevel,
                                                            entryFunction.mangledName, buildError);
                            }))
    {
        error << "nsc: error: OOP Windows executable generation failed: " << buildError << '\n';
        return false;
    }
    std::vector<std::filesystem::path> dynamicallyLoadedRuntimes;
    if (!appendReferencedCppRuntimes(compilation, builder.info(), runtimeLibraries, dynamicallyLoadedRuntimes, referenceError))
    {
        error << "nsc: error: C/C++ runtime selection failed: " << referenceError << '\n';
        return false;
    }
    if (!computeCppTlsRuntimeAliases(compilation, dynamicallyLoadedRuntimes, runtimeLibraries,
                                     builder.info(), aliases, referenceError))
    {
        error << "nsc: error: C/C++ runtime alias selection failed: " << referenceError << '\n';
        return false;
    }
    if (!verifyLinkedRuntimeLibraries(runtimeLibraries, builder.info(), publication.stage, dynamicallyLoadedRuntimes,
                                      referenceError))
    {
        error << "nsc: error: precompiled DLL/import-library mismatch: " << referenceError << '\n';
        return false;
    }
    std::vector<std::filesystem::path> runtimeClosure(runtimeLibraries.begin(), runtimeLibraries.end());
    if (activeExecutionScope != nullptr && !activeExecutionScope->addressSanitizerRuntime.empty())
        runtimeClosure.push_back(activeExecutionScope->addressSanitizerRuntime);
    if (!validateCppRuntimeDestinations(compilation, runtimeLibraries, aliases,
                                        destination.parent_path(), referenceError))
    {
        error << "nsc: error: C/C++ runtime output preflight failed: " << referenceError << '\n';
        return false;
    }
    if (!packageRuntimeClosure(runtimeClosure, dynamicallyLoadedRuntimes, aliases, publication.stage,
                               packagedRuntimes, referenceError))
    {
        removePackagedFiles(packagedRuntimes);
        error << "nsc: error: runtime packaging failed: " << referenceError << '\n';
        return false;
    }
    bool protectExistingRuntimes = false;
    if (!validatePackagedCppRuntimeDestinations(compilation, runtimeLibraries, packagedRuntimes,
                                                destination.parent_path(), protectExistingRuntimes, referenceError))
    {
        removePackagedFiles(packagedRuntimes);
        error << "nsc: error: C/C++ runtime output preflight failed: " << referenceError << '\n';
        return false;
    }
    if (publishPdb && !artifactPublisher.addExecutablePdb(publication, buildError))
    {
        error << "nsc: error: PDB output preflight failed: " << buildError << '\n';
        return false;
    }
    if (incrementalLink && !artifactPublisher.addExecutableState(publication, buildError))
    {
        error << "nsc: error: incremental-state output preflight failed: " << buildError << '\n';
        return false;
    }
    if (!artifactPublisher.addRuntimeArtifacts(destination.parent_path(), packagedRuntimes, buildError, protectExistingRuntimes))
    {
        error << "nsc: error: runtime output preflight failed: " << buildError << '\n';
        return false;
    }
    if (!artifactPublisher.commit(buildError))
    {
        error << "nsc: error: executable publication failed: " << buildError << '\n';
        return false;
    }
    return true;
}

ExitCode Driver::runProjectOopExecutableGeneration(std::string_view inputPath, std::string_view outputPath,
                                                   llvm_backend::LlvmOptimizationLevel optimizationLevel,
                                                   llvm_backend::DebugInformationKind debugInformation,
                                                   const std::vector<llvm_backend::DebugPrefixMap> &debugPrefixMaps,
                                                   llvm_backend::LinkTimeOptimizationMode linkTimeOptimization,
                                                   bool incrementalLink, linker::WindowsExecutableSubsystem subsystem,
                                                   const std::vector<std::filesystem::path> &extraLinkLibraries,
                                                   std::string_view linkerSelection,
                                                   std::ostream &output, std::ostream &error)
{
    std::string loadError;
    std::optional<compilation::CompilationInputManifest> manifest =
        loadCompilationInputManifest(pathFromUtf8(inputPath), loadError);
    if (!manifest.has_value())
    {
        error << "nsc: error: " << loadError << '\n';
        return ExitCode::CompilationError;
    }
    const std::string selectedLinker = selectDirectLinkedProductLinker(*manifest, linkerSelection, extraLinkLibraries);
    addCompilationProductIdentity(
        *manifest, "executable", pathFromUtf8(outputPath));
    addCompilationCodegenIdentity(
        *manifest, optimizationLevel, debugInformation,
        debugPrefixMaps, linkTimeOptimization);
    addCompilationLinkIdentity(
        *manifest, incrementalLink, subsystem, extraLinkLibraries, selectedLinker);
    frontend::ProjectCompilation compilation;
    compilation.setReceiverRequiresThinLtoSidecar(
        llvm_backend::ProjectOopObjectGenerator::thinLtoRuns(linkTimeOptimization, optimizationLevel));
    const frontend::ProjectOopMirFunction *entryFunction = nullptr;
    bool restored = false;
    if (!prepareExecutableProject(
            inputPath, *manifest, compilation, entryFunction, restored, error))
        return ExitCode::CompilationError;
    if (restored)
    {
        output << manifest->bundleLookup().resultOutput;
        return ExitCode::Success;
    }
    std::string codeGenerationError;
    ThinLtoLibraryLinking thinLtoLibraries;
    if (!thinLtoLibraries.collect(
            compilation, linkTimeOptimization, optimizationLevel, debugInformation, codeGenerationError))
    {
        error << "nsc: error: " << codeGenerationError << '\n';
        return ExitCode::CompilationError;
    }
    // The link inputs are known before code generation, so ThinLTO can keep visible exactly the
    // program symbols they need and internalize the rest (item 50).
    // Las entradas del enlace se conocen antes de generar codigo, asi que ThinLTO puede dejar visibles
    // exactamente los simbolos del programa que necesitan e internalizar el resto (punto 50).
    std::vector<std::filesystem::path> linkLibraries;
    std::vector<std::filesystem::path> runtimeLibraries;
    linker::WindowsExecutableBuilder builder;
    builder.setLinkerSelection(selectedLinker);
    std::vector<std::filesystem::path> packagedRuntimes;
    if (!prepareExecutableBuilder(compilation, extraLinkLibraries, debugInformation, incrementalLink, subsystem,
                                  linkLibraries, runtimeLibraries, builder, error))
        return ExitCode::CompilationError;
    builder.setMainArguments(!entryFunction->parameterTypeKeys.empty());
    if (activeExecutionScope != nullptr && !activeExecutionScope->addressSanitizerClang.empty())
        builder.setAddressSanitizerClang(activeExecutionScope->addressSanitizerClang);
    const std::vector<std::string> entrySymbols = {entryFunction->mangledName};
    std::optional<std::vector<std::string>> preservedSymbols =
        thinLtoLibraries.preservedSymbols(linkLibraries, entrySymbols);
    std::unique_ptr<llvm_backend::ProjectOopObjectBundle> objectBundle = generateProjectObjects(
        compilation, debugInformation, optimizationLevel, debugPrefixMaps, linkTimeOptimization, codeGenerationError,
        false, thinLtoLibraries.takeLibraries(), std::move(preservedSymbols));
    if (objectBundle == nullptr)
    {
        error << "nsc: error: " << codeGenerationError << '\n';
        return ExitCode::CompilationError;
    }
    if (!thinLtoLibraries.replaceLinkLibraries(*objectBundle, linkLibraries, codeGenerationError))
    {
        error << "nsc: error: ThinLTO library linking failed: " << codeGenerationError << '\n';
        return ExitCode::CompilationError;
    }
    builder.setExtraLinkLibraries(linkLibraries);
    if (!rejectUnstartedCrtInitializers(objectBundle->objectFiles(), linkLibraries, true, error))
        return ExitCode::CompilationError;
    builder.setRemoveUnreferencedSections(objectBundle->thinLtoInternalized());
    CppTlsRuntimeAliases aliases;
    if (!publishExecutableArtifact(outputPath, compilation, *entryFunction, *objectBundle, optimizationLevel, debugInformation,
                                   incrementalLink, runtimeLibraries, builder, packagedRuntimes, aliases, error))
    {
        explainMissingCppSymbols(compilation, objectBundle->objectFiles(), linkLibraries, error);
        return ExitCode::CompilationError;
    }
    addRuntimeBundleOutputs(*manifest, packagedRuntimes, runtimeLibraries, aliases);
    std::ostringstream resultOutput;
    printExecutableGenerationResult(*entryFunction, *objectBundle, builder, optimizationLevel, debugInformation,
                                    linkTimeOptimization, incrementalLink, subsystem, linkLibraries.size(),
                                    packagedRuntimes.size(), resultOutput);
    thinLtoLibraries.printResult(resultOutput);
    if (activeExecutionScope != nullptr && !activeExecutionScope->addressSanitizerClang.empty())
        resultOutput << "address-sanitizer: enabled (precompiled libraries are not instrumented)\n";
    manifest->setBundleResultOutput(resultOutput.str());
    publishCompilationBundle(*manifest);
    output << resultOutput.str();
    return ExitCode::Success;
}

#if defined(_WIN32)
namespace
{
constexpr std::string_view kReceiverContractUnknown = "NSC-RECEIVER-UNKNOWN\n";

std::filesystem::path receiverContractPath(const std::filesystem::path& library)
{
    std::filesystem::path result = library;
    result += ".nsreceiver";
    return result;
}

bool receiverContractState(const std::filesystem::path& path, bool& exists, bool& marker, std::string& error)
{
    std::error_code ec;
    exists = std::filesystem::exists(path, ec);
    if (ec) { error = "cannot inspect previous receiver contract: " + ec.message(); return false; }
    marker = false;
    if (!exists) return true;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec) { error = "cannot inspect previous receiver contract: " + ec.message(); return false; }
    if (size != kReceiverContractUnknown.size()) return true;
    std::ifstream input(path, std::ios::binary);
    if (!input) { error = "cannot read previous receiver contract"; return false; }
    std::string bytes(kReceiverContractUnknown.size(), '\0');
    input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!input) { error = "cannot read previous receiver contract"; return false; }
    marker = bytes == kReceiverContractUnknown;
    return true;
}

bool stageReceiverContract(const frontend::ProjectCompilation& compilation,
                           const StaticLibraryPublication& publication,
                           const linker::CoffArchiveInfo& archive, bool previousExists,
                           bool& staged, std::string& error)
{
    staged = false;
    frontend::ReceiverContractManifest contract;
    const auto* graph = compilation.projectGraph();
    const auto* names = compilation.nameLookup();
    if (graph != nullptr && names != nullptr && graph->rootProject() != nullptr)
    {
        const auto projects = graph->projects();
        std::optional<std::size_t> root;
        for (std::size_t i = 0; i < projects.size(); ++i)
            if (&projects[i] == graph->rootProject()) { root = i; break; }
        if (root.has_value())
            for (std::size_t i = 0; i < names->namedTypes().size(); ++i)
            {
                const auto proof = frontend::inspectImplicitLifecycleEvidence(compilation, i, *root);
                if (!proof.has_value()) continue;
                if (std::ranges::find(archive.symbolNames, proof->constructorSymbol) == archive.symbolNames.end() ||
                    std::ranges::find(archive.symbolNames, proof->objectDestructionSymbol) == archive.symbolNames.end())
                    continue;
                frontend::ReceiverContractRecord constructor{
                    frontend::ReceiverContractKind::Constructor, proof->typeIdentity};
                frontend::ReceiverContractRecord destructor{
                    frontend::ReceiverContractKind::Destructor, proof->typeIdentity};
                if (frontend::validReceiverContractRecord(constructor) &&
                    frontend::validReceiverContractRecord(destructor))
                {
                    contract.records.push_back(std::move(constructor));
                    contract.records.push_back(std::move(destructor));
                }
            }
    }
    std::ranges::sort(contract.records, frontend::receiverContractRecordLess);
    std::string bytes;
    if (!contract.records.empty())
    {
        std::uintmax_t size = 0;
        if (!basic::sha256File(publication.stage, contract.libraryDigest, size, error) ||
            !basic::sha256File(publication.metadataStage, contract.metadataDigest, size, error)) return false;
        const auto encoded = frontend::encodeReceiverContractManifest(contract);
        if (encoded.has_value()) bytes.assign(encoded->begin(), encoded->end());
    }
    // No complete representable proof: invalidate only an existing sidecar.
    if (bytes.empty() && previousExists) bytes = kReceiverContractUnknown;
    if (bytes.empty()) return true;
    std::ofstream output(publication.receiverContractStage, std::ios::binary | std::ios::trunc);
    if (!output) { error = "cannot stage receiver contract"; return false; }
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    output.close();
    if (!output) { error = "cannot finish receiver contract"; return false; }
    staged = true;
    return true;
}
}
#endif

bool prepareStaticLibraryProject(std::string_view inputPath,
                                 compilation::CompilationInputManifest &manifest,
                                 frontend::ProjectCompilation &compilation,
                                 std::vector<std::filesystem::path> &referencedLibraries,
                                 std::vector<std::filesystem::path> &runtimeLibraries,
                                 std::vector<linker::ExternalImportModule> &inheritedImports,
                                 bool &restored, std::ostream &error)
{
    configureProjectExecution(compilation);
    std::string loadError;
    if (!measureDriverPhase(
            "precompiled-metadata",
            [&] { return configurePrecompiledModules(manifest, compilation, loadError, true); }))
    {
        error << "nsc: error: " << loadError << '\n';
        if (!compilation.diagnostics().diagnostics().empty())
            diagnostics::DiagnosticRenderer(compilation.sourceManager()).renderAll(compilation.diagnostics(), error);
        return false;
    }
    if (!restoreCompilationBundle(manifest, restored, loadError))
    {
        error << "nsc: error: compilation bundle restore failed: "
              << loadError << '\n';
        return false;
    }
    if (restored) return true;
    if (!compilation.compileProjectOopMir(pathFromUtf8(inputPath), loadError))
    {
        if (!loadError.empty())
            error << "nsc: error: " << loadError << '\n';
        if (compilation.diagnostics().hasErrors())
        {
            diagnostics::DiagnosticRenderer renderer(compilation.sourceManager());
            renderer.renderAll(compilation.diagnostics(), error);
        }
        return false;
    }
    renderCppInteropWarnings(compilation, error);
    // EN: LKF-108. A successful build still shows its warnings (for example stackalloc in a loop);
    //     only a failed one rendered diagnostics before.
    // ES: LKF-108. Una compilacion correcta tambien muestra sus advertencias (por ejemplo stackalloc
    //     en un bucle); antes solo una fallida mostraba diagnosticos.
    if (compilation.diagnostics().warningCount() != 0U)
    {
        diagnostics::DiagnosticRenderer renderer(compilation.sourceManager());
        renderer.renderAll(compilation.diagnostics(), error);
    }
    if (compilation.projectGraph() == nullptr || compilation.nameLookup() == nullptr ||
        compilation.memberModel() == nullptr || compilation.oopMirModel() == nullptr)
    {
        error << "nsc: error: Stage 46 static-library compilation did not produce required models\n";
        return false;
    }
    const compilation::Project *rootProject = compilation.projectGraph()->rootProject();
    if (rootProject == nullptr || rootProject->outputKind() != compilation::ProjectOutputKind::Library)
    {
        error << "nsc: error: --emit-project-oop-lib requires root OutputType Library or StaticLibrary\n";
        return false;
    }
    std::string referenceError;
    if (!appendValidatedLibraryReferences(compilation, referencedLibraries, runtimeLibraries, referenceError,
                                          &inheritedImports))
    {
        error << "nsc: error: precompiled library reference validation failed: " << referenceError << '\n';
        return false;
    }
    return true;
}

bool stageCppStaticDependencies(const frontend::ProjectCompilation &compilation,
                                const StaticLibraryPublication &publication,
                                ArtifactPublisher &publisher, std::string &error)
{
    interop::LibraryMetadataCodec codec;
    interop::LibraryMetadata metadata;
    if (!codec.read(publication.metadataStage, metadata, error)) return false;
    if (metadata.cppBinaryDependencies.empty()) return true;
    std::vector<interop::LibraryMetadataCppBinaryDependency> current;
    if (!interop::collectCppGraphBinaryMetadataDependencies(*compilation.projectGraph(), current, error)) return false;
    if (current != metadata.cppBinaryDependencies)
    {
        error = "C/C++ binary dependencies changed after metadata generation";
        return false;
    }
    std::unordered_map<std::string, const interop::LibraryMetadataCppBinaryDependency *> declaredRuntimes;
    for (const auto &record : current)
        if (!record.runtimeFileName.empty()) declaredRuntimes.emplace(foldedName(record.runtimeFileName), &record);
    for (const auto &project : compilation.projectGraph()->projects())
        for (const auto &reference : project.cppLibraryReferences())
        {
            if (reference.runtimePath.empty()) continue;
            linker::PeExecutableVerifier verifier;
            if (!verifier.verifyDynamicLibrary(reference.runtimePath))
            {
                error = "invalid C/C++ runtime '" + reference.runtimePath.string() + "': " + verifier.error();
                return false;
            }
            for (const auto &imported : verifier.info().importedLibraries)
            {
                const auto leaf = pathFromUtf8(imported);
                if (leaf.empty() || leaf != leaf.filename() || leaf.has_root_name())
                {
                    error = "invalid DLL import name in C/C++ runtime: " + imported;
                    return false;
                }
                const auto sibling = reference.runtimePath.parent_path() / leaf;
                std::error_code siblingError;
                const bool present = std::filesystem::is_regular_file(sibling, siblingError);
                if (siblingError && siblingError != std::errc::no_such_file_or_directory)
                { error = "cannot inspect C/C++ runtime dependency: " + siblingError.message(); return false; }
                if (!present) continue;
                const auto found = declaredRuntimes.find(foldedName(imported));
                if (found == declaredRuntimes.end())
                {
                    error = "C/C++ runtime dependency must be declared for publication: " + imported;
                    return false;
                }
                basic::Sha256Digest digest{};
                std::uintmax_t size = 0;
                if (!basic::sha256File(sibling, digest, size, error)) return false;
                if (size != found->second->runtimeSize || digest != found->second->runtimeDigest)
                {
                    error = "C/C++ runtime dependency differs from declared runtime: " + imported;
                    return false;
                }
            }
        }
    std::error_code ec;
    const auto outputDirectory = std::filesystem::absolute(publication.destination, ec).parent_path();
    if (ec) { error = "cannot resolve static-library output directory: " + ec.message(); return false; }
    std::unordered_set<std::string> reserved;
    for (const auto &path : {publication.destination, publication.headerDestination, publication.metadataDestination,
                             publication.productSidecarDestination, publication.thinLtoSidecarDestination})
        reserved.insert(foldedPathKey(outputDirectory / path.filename()));
    std::unordered_set<std::string> stagedNames;
    std::vector<std::filesystem::path> staged;
    const auto stageArtifact = [&](const std::filesystem::path &source, std::uint64_t expectedSize,
                                   const basic::Sha256Digest &expectedDigest) {
        const auto destination = outputDirectory / source.filename();
        const auto key = foldedPathKey(destination);
        if (reserved.contains(key))
        {
            error = "C/C++ binary dependency conflicts with a NativeSharp product: " + destination.string();
            return false;
        }
        if (!stagedNames.insert(key).second) return true;
        const auto stage = publication.stage.parent_path() / "cpp-static-dependencies" / source.filename();
        if (!basic::seedArtifactStage(source, stage, error)) return false;
        basic::Sha256Digest digest{};
        std::uintmax_t size = 0;
        if (!basic::sha256File(stage, digest, size, error)) return false;
        if (size != expectedSize || digest != expectedDigest)
        {
            error = "C/C++ binary dependency changed while being staged: " + source.string();
            return false;
        }
        staged.push_back(stage);
        return true;
    };
    std::size_t index = 0;
    for (const auto &project : compilation.projectGraph()->projects())
        for (const auto &reference : project.cppLibraryReferences())
        {
            if (reference.libraryPath.empty()) continue;
            const auto &record = current[index++];
            if (!stageArtifact(reference.libraryPath, record.librarySize, record.libraryDigest)) return false;
            if (!reference.runtimePath.empty() &&
                !stageArtifact(reference.runtimePath, record.runtimeSize, record.runtimeDigest)) return false;
        }
    return publisher.addRuntimeArtifacts(outputDirectory, staged, error, true);
}

bool publishStaticLibraryArtifact(std::string_view outputPath, const frontend::ProjectCompilation &compilation,
                                  const llvm_backend::ProjectOopObjectBundle &objectBundle,
                                  std::span<const std::filesystem::path> referencedLibraries,
                                  std::span<const linker::ExternalImportModule> inheritedImports,
                                  llvm_backend::LlvmOptimizationLevel optimizationLevel,
                                  linker::WindowsLibraryBuilder &builder, StaticLibraryPublication &publication,
                                  bool previousReceiverContract,
                                  std::ostream &error)
{
    builder.setProgramObjectsVerified(true);
    builder.setRuntimeLibrary(linker::configuredWindowsRuntimeLibrary());
    std::vector<linker::ExternalImportModule> externalImports = collectExternalImports(*compilation.oopMirModel());
    ExternalImportListBuilder externalImportList(externalImports);
    for (const linker::ExternalImportModule &module : inheritedImports)
        for (const std::string &symbol : module.symbols)
            externalImportList.append(module.dllName, symbol);
    builder.setExternalImports(std::move(externalImports));
    builder.setExtraLinkLibraries(
        std::vector<std::filesystem::path>(referencedLibraries.begin(), referencedLibraries.end()));
    std::string buildError;
    ArtifactPublisher artifactPublisher;
    if (!artifactPublisher.prepareStaticLibrary(pathFromUtf8(outputPath), publication, buildError))
    {
        error << "nsc: error: static-library output preflight failed: " << buildError << '\n';
        return false;
    }
    if (!measureDriverPhase("static-library-build",
                            [&]
                            {
                                return builder.buildStaticObjects(objectBundle.objectFiles(), publication.stage,
                                                                  optimizationLevel, buildError);
                            }))
    {
        error << "nsc: error: NativeSharp static-library generation failed: " << buildError << '\n';
        return false;
    }
    interop::CHeaderGenerator headerGenerator;
    std::string headerError;
    if (!measureDriverPhase("host-header", [&]
                            { return headerGenerator.writeHeader(compilation, publication.headerStage, headerError); }))
    {
        error << "nsc: error: Stage 48.1 static-library C header generation failed: " << headerError << '\n';
        return false;
    }
    std::string metadataError;
    if (!measureDriverPhase(
            "library-metadata",
            [&] { return writeVerifiedLibraryMetadata(compilation, publication.metadataStage, metadataError); }))
    {
        error << "nsc: error: Stage 48.2 static-library metadata generation failed: " << metadataError << '\n';
        return false;
    }
    publishPrecompiledSnapshot(compilation, publication.metadataStage);
    // Sidecar v3 carries generated native objects alongside bitcode; otherwise
    // the writer keeps the existing v2 representation.
#if defined(_WIN32)
    if (!stageReceiverContract(compilation, publication, builder.archiveInfo(), previousReceiverContract,
                               publication.receiverContractStaged, buildError) ||
        (publication.receiverContractStaged &&
         !artifactPublisher.addStaticLibraryReceiverContract(publication, buildError)))
    {
        error << "nsc: error: static-library receiver contract staging failed: " << buildError << '\n';
        return false;
    }
#else
    (void)previousReceiverContract;
#endif
    if (!objectBundle.thinLtoUnitBitcode().empty())
    {
        // The sidecar binds the staged .lib and .nsmeta, so it is written after both (item 48).
        // El sidecar liga la .lib y la .nsmeta en staging, asi que se escribe despues de ambas.
        if (!ThinLtoLibraryLinking::writeSidecar(objectBundle, compilation, publication.stage,
                                                 publication.metadataStage, publication.thinLtoSidecarStage,
                                                 buildError) ||
            !artifactPublisher.addStaticLibraryThinLtoSidecar(publication, buildError))
        {
            error << "nsc: error: static-library ThinLTO sidecar generation failed: " << buildError << '\n';
            return false;
        }
        publication.thinLtoSidecarStaged = true;
    }
    if (!stageCppStaticDependencies(compilation, publication, artifactPublisher, buildError))
    {
        error << "nsc: error: C/C++ static dependency staging failed: " << buildError << '\n';
        return false;
    }
    if (!artifactPublisher.commit(buildError))
    {
        error << "nsc: error: static-library publication failed: " << buildError << '\n';
        return false;
    }
    if (!publication.thinLtoSidecarStaged)
    {
        // A sidecar left by an earlier ThinLTO build would only be rejected; remove it anyway.
        // Un sidecar de un build ThinLTO anterior solo seria rechazado; se borra de todos modos.
        std::error_code staleError;
        std::filesystem::remove(publication.thinLtoSidecarDestination, staleError);
        // EN: LKF-246: the stale sidecar is harmless (its fingerprints reject it) but a removal
        //     that failed is reported instead of silently ignored.
        // ES: LKF-246: el sidecar viejo no hace daño (sus huellas lo rechazan), pero una eliminación
        //     fallida se informa en lugar de ignorarse.
        if (staleError)
            error << "nsc: warning: stale ThinLTO sidecar '"
                  << publication.thinLtoSidecarDestination.string() << "' could not be removed: "
                  << staleError.message() << '\n';
    }
    return true;
}

void printStaticLibraryGenerationResult(const llvm_backend::ProjectOopObjectBundle &objectBundle,
                                        const linker::WindowsLibraryBuilder &builder,
                                        const StaticLibraryPublication &publication,
                                        llvm_backend::LlvmOptimizationLevel optimizationLevel,
                                        llvm_backend::DebugInformationKind debugInformation,
                                        llvm_backend::LinkTimeOptimizationMode linkTimeOptimization,
                                        std::size_t referencedLibraryCount, std::ostream &output)
{
    output << "project OOP static library emission: ok\n"
           << "format: COFF archive (.lib)\n"
           << "machine-members: AMD64 COFF\n"
           << "optimization: " << llvm_backend::llvmOptimizationLevelName(optimizationLevel) << '\n'
           << "debug-information: " << llvm_backend::debugInformationKindName(debugInformation) << '\n'
           << "link-time-optimization: " << llvm_backend::linkTimeOptimizationModeName(linkTimeOptimization) << '\n'
           << "code-units: " << objectBundle.codeUnitCount() << '\n'
           << "artifact-cache-hits: " << objectBundle.cacheHitCount() << '\n'
           << "artifact-cache-misses: " << objectBundle.cacheMissCount() << '\n'
           << "runtime-object-cache: "
           << (!builder.runtimeObjectCacheAttempted() ? "disabled-or-external"
                                                      : (builder.reusedRuntimeObject() ? "hit" : "miss"))
           << '\n'
           << "static-archive-cache: "
           << (!builder.staticArchiveCacheAttempted() ? "disabled" : (builder.reusedStaticArchive() ? "hit" : "miss"))
           << '\n'
           << "thinlto-summaries: " << objectBundle.thinLtoSummaryCount() << '\n'
           << "thinlto-index: "
           << (objectBundle.thinLtoIndexDigest().empty() ? "none" : objectBundle.thinLtoIndexDigest()) << '\n'
           << "thinlto-imports: " << objectBundle.thinLtoImportCount() << '\n'
           << "thinlto-cached-backends: " << objectBundle.thinLtoCachedBackendCount() << '\n'
           << "debug-companion: "
           << (debugInformation == llvm_backend::DebugInformationKind::None ? "none"
                                                                            : "CodeView in COFF archive members")
           << '\n'
           << "library-references: " << referencedLibraryCount << '\n'
           << "archive-members: " << builder.archiveInfo().memberCount << '\n'
           << "host-header: " << pathToUtf8(publication.headerDestination) << '\n'
           << "library-metadata: " << pathToUtf8(publication.metadataDestination) << '\n';
    if (publication.thinLtoSidecarStaged)
        output << "thinlto-library-sidecar: " << pathToUtf8(publication.thinLtoSidecarDestination) << '\n';
    if (activeExecutionScope != nullptr && activeExecutionScope->reportTiming)
        appendCodeUnitTelemetry(output, objectBundle);
}

ExitCode Driver::runProjectOopStaticLibraryGeneration(std::string_view inputPath, std::string_view outputPath,
                                                      llvm_backend::LlvmOptimizationLevel optimizationLevel,
                                                      llvm_backend::DebugInformationKind debugInformation,
                                                      const std::vector<llvm_backend::DebugPrefixMap> &debugPrefixMaps,
                                                      llvm_backend::LinkTimeOptimizationMode linkTimeOptimization,
                                                      std::string_view linkerSelection,
                                                      std::ostream &output, std::ostream &error)
{
    bool previousReceiverContract = false;
#if defined(_WIN32)
    const std::filesystem::path receiverOutput = pathFromUtf8(outputPath);
    std::error_code receiverError;
    const auto absoluteOutput = std::filesystem::absolute(receiverOutput, receiverError);
    if (receiverError)
    {
        error << "nsc: error: cannot resolve static-library destination: " << receiverError.message() << '\n';
        return ExitCode::CompilationError;
    }
    std::filesystem::create_directories(absoluteOutput.parent_path(), receiverError);
    if (receiverError)
    {
        error << "nsc: error: cannot prepare static-library destination: " << receiverError.message() << '\n';
        return ExitCode::CompilationError;
    }
    contract_lock_prototype::ProductLock receiverLock;
    if (!receiverLock.acquire(receiverOutput, true))
    {
        error << "nsc: error: cannot acquire static-library receiver-contract lock\n";
        return ExitCode::CompilationError;
    }
    bool previousReceiverMarker = false;
    std::string receiverStateError;
    if (!receiverContractState(receiverContractPath(receiverOutput), previousReceiverContract,
                               previousReceiverMarker, receiverStateError))
    {
        error << "nsc: error: " << receiverStateError << '\n';
        return ExitCode::CompilationError;
    }
#endif
    std::string loadError;
    std::optional<compilation::CompilationInputManifest> manifest =
        loadCompilationInputManifest(pathFromUtf8(inputPath), loadError);
    if (!manifest.has_value())
    {
        error << "nsc: error: " << loadError << '\n';
        return ExitCode::CompilationError;
    }
    addCompilationProductIdentity(
        *manifest, "static-library", pathFromUtf8(outputPath));
#if defined(_WIN32)
    // The bundle must not reuse entries created before receiver sidecars were
    // part of the product. This is a cache identity, not a language version.
    manifest->addIdentityField("receiver-contract-transport", "implicit-root-v1");
    if (previousReceiverContract && !previousReceiverMarker)
        manifest->addRequiredBundleOutput(receiverContractPath(receiverOutput));
#endif
    addCompilationCodegenIdentity(
        *manifest, optimizationLevel, debugInformation,
        debugPrefixMaps, linkTimeOptimization);
    addCompilationLinkToolIdentity(*manifest, linkerSelection);
    frontend::ProjectCompilation compilation;
    compilation.setReceiverRequiresThinLtoSidecar(
        llvm_backend::ProjectOopObjectGenerator::thinLtoRuns(linkTimeOptimization, optimizationLevel));
    std::vector<std::filesystem::path> referencedLibraries;
    std::vector<std::filesystem::path> runtimeLibraries;
    std::vector<linker::ExternalImportModule> inheritedImports;
    bool restored = false;
    if (!prepareStaticLibraryProject(inputPath, *manifest, compilation, referencedLibraries,
                                     runtimeLibraries, inheritedImports, restored, error))
        return ExitCode::CompilationError;
    if (restored)
    {
        output << manifest->bundleLookup().resultOutput;
        return ExitCode::Success;
    }

    // A static library publishes a ThinLTO sidecar only when its archive holds exactly the objects of
    // its units, that is, with the NativeSharp runtime configured as a separate library (item 48).
    // Una biblioteca estatica publica sidecar ThinLTO solo cuando su archivo contiene exactamente los
    // objetos de sus unidades, es decir, con el runtime de NativeSharp configurado aparte (punto 48).
    const bool captureThinLtoUnits =
        llvm_backend::ProjectOopObjectGenerator::thinLtoRuns(linkTimeOptimization, optimizationLevel) &&
        !linker::configuredWindowsRuntimeLibrary().empty();
    std::string codeGenerationError;
    std::unique_ptr<llvm_backend::ProjectOopObjectBundle> objectBundle = generateProjectObjects(
        compilation, debugInformation, optimizationLevel, debugPrefixMaps, linkTimeOptimization, codeGenerationError,
        captureThinLtoUnits);
    if (objectBundle == nullptr)
    {
        error << "nsc: error: " << codeGenerationError << '\n';
        return ExitCode::CompilationError;
    }

    linker::WindowsLibraryBuilder builder;
    builder.setLinkerSelection(std::string(linkerSelection));
    StaticLibraryPublication publication;
    if (!publishStaticLibraryArtifact(outputPath, compilation, *objectBundle, referencedLibraries, inheritedImports,
                                      optimizationLevel, builder, publication, previousReceiverContract, error))
        return ExitCode::CompilationError;
#if defined(_WIN32)
    if (publication.receiverContractStaged)
        manifest->addRequiredBundleOutput(publication.receiverContractDestination);
#endif
    std::ostringstream resultOutput;
    printStaticLibraryGenerationResult(*objectBundle, builder, publication, optimizationLevel, debugInformation,
                                       linkTimeOptimization, referencedLibraries.size(), resultOutput);
    manifest->setBundleResultOutput(resultOutput.str());
    publishCompilationBundle(*manifest);
    output << resultOutput.str();
    return ExitCode::Success;
}

bool prepareDynamicLibraryProject(std::string_view inputPath,
                                  compilation::CompilationInputManifest &manifest,
                                  frontend::ProjectCompilation &compilation,
                                  bool &restored, std::ostream &error)
{
    configureProjectExecution(compilation);
    std::string loadError;
    if (!measureDriverPhase(
            "precompiled-metadata",
            [&] { return configurePrecompiledModules(manifest, compilation, loadError, true); }))
    {
        error << "nsc: error: " << loadError << '\n';
        if (!compilation.diagnostics().diagnostics().empty())
            diagnostics::DiagnosticRenderer(compilation.sourceManager()).renderAll(compilation.diagnostics(), error);
        return false;
    }
    if (!restoreCompilationBundle(manifest, restored, loadError))
    {
        error << "nsc: error: compilation bundle restore failed: "
              << loadError << '\n';
        return false;
    }
    if (restored) return true;
    if (!compilation.compileProjectOopMir(pathFromUtf8(inputPath), loadError))
    {
        if (!loadError.empty())
            error << "nsc: error: " << loadError << '\n';
        if (compilation.diagnostics().hasErrors())
        {
            diagnostics::DiagnosticRenderer renderer(compilation.sourceManager());
            renderer.renderAll(compilation.diagnostics(), error);
        }
        return false;
    }
    renderCppInteropWarnings(compilation, error);
    // EN: LKF-108. A successful build still shows its warnings (for example stackalloc in a loop);
    //     only a failed one rendered diagnostics before.
    // ES: LKF-108. Una compilacion correcta tambien muestra sus advertencias (por ejemplo stackalloc
    //     en un bucle); antes solo una fallida mostraba diagnosticos.
    if (compilation.diagnostics().warningCount() != 0U)
    {
        diagnostics::DiagnosticRenderer renderer(compilation.sourceManager());
        renderer.renderAll(compilation.diagnostics(), error);
    }
    if (compilation.projectGraph() == nullptr || compilation.nameLookup() == nullptr ||
        compilation.memberModel() == nullptr || compilation.oopMirModel() == nullptr)
    {
        error << "nsc: error: Stage 47 DLL compilation did not produce required models\n";
        return false;
    }
    const compilation::Project *rootProject = compilation.projectGraph()->rootProject();
    if (rootProject == nullptr || rootProject->outputKind() != compilation::ProjectOutputKind::SharedLibrary)
    {
        error << "nsc: error: --emit-project-oop-dll requires root OutputType SharedLibrary\n";
        return false;
    }
    return true;
}

void collectDynamicMethodExports(const frontend::ProjectCompilation &compilation,
                                 std::span<const frontend::NamedTypeDeclaration> types,
                                 std::span<const frontend::ClassMemberSymbol> members, std::size_t rootProjectOrdinal,
                                 std::vector<std::string> &exportSymbols)
{
    // EN: T08-D19: non-public members named by published templates are part of the consumable surface.
    // ES: T08-D19: los miembros no publicos que nombran las plantillas publicadas son parte de la superficie.
    const auto templateDependencies =
        interop::metadata_detail::templateDependencyMemberOrdinals(compilation, rootProjectOrdinal);
    for (const frontend::ProjectOopMirFunction &function : compilation.oopMirModel()->functions())
    {
        if (function.isLambda || function.destructor || function.declaringTypeOrdinal >= types.size())
            continue;
        const frontend::NamedTypeDeclaration &type = types[function.declaringTypeOrdinal];
        if (function.isSyntheticImplicitConstructor)
        {
            if (type.projectOrdinal == rootProjectOrdinal &&
                isPubliclyReachableNamedType(types, function.declaringTypeOrdinal))
                exportSymbols.push_back(function.mangledName);
            continue;
        }
        if (function.memberOrdinal >= members.size())
            continue;
        const frontend::ClassMemberSymbol &member = members[function.memberOrdinal];
        if (type.projectOrdinal != rootProjectOrdinal ||
            !isPubliclyReachableNamedType(types, function.declaringTypeOrdinal) ||
            (!hasConsumerCallableAccessibility(member.accessibility) &&
             !templateDependencies.contains(function.memberOrdinal)))
            continue;
        if (function.constructor)
        {
            exportSymbols.push_back(function.mangledName);
            continue;
        }
        if (member.kind != frontend::ClassMemberKind::Method ||
            (!member.isStatic && type.kind != frontend::NamedTypeDeclarationKind::Struct &&
             type.kind != frontend::NamedTypeDeclarationKind::Class &&
             type.kind != frontend::NamedTypeDeclarationKind::Interface) ||
            member.isAbstract || member.isCompilerGenerated || (member.isExternC && !member.isExportedC))
            continue;
        exportSymbols.push_back(function.mangledName);
        if (member.isExportedC)
            exportSymbols.push_back(member.cAbiName);
    }
}

void collectDynamicNamespaceExports(const frontend::ProjectCompilation &compilation, std::size_t rootProjectOrdinal,
                                    std::vector<std::string> &exportSymbols)
{
    if (compilation.namespaceFunctionModel() == nullptr)
        return;
    const auto namespaceFunctions = compilation.namespaceFunctionModel()->functions();
    for (const frontend::ProjectOopMirFunction &function : compilation.oopMirModel()->functions())
    {
        if (!function.namespaceFunctionOrdinal.has_value() ||
            *function.namespaceFunctionOrdinal >= namespaceFunctions.size())
            continue;
        const frontend::NamespaceFunctionSymbol &symbol = namespaceFunctions[*function.namespaceFunctionOrdinal];
        if (symbol.projectOrdinal != rootProjectOrdinal ||
            symbol.accessibility != frontend::NamespaceFunctionAccessibility::Public || function.isImportedC ||
            function.isImportedNative)
            continue;
        exportSymbols.push_back(function.mangledName);
        if (function.isExportedC && !function.cAbiName.empty())
            exportSymbols.push_back(function.cAbiName);
    }
}

void collectDynamicLayoutExports(const frontend::ProjectCompilation &compilation,
                                 std::span<const frontend::NamedTypeDeclaration> types, std::size_t rootProjectOrdinal,
                                 std::vector<std::string> &exportSymbols)
{
    for (const frontend::ProjectOopMirDispatchTable &table : compilation.oopMirModel()->dispatchTables())
    {
        if (table.isImported || table.typeOrdinal >= types.size())
            continue;
        const frontend::NamedTypeDeclaration &type = types[table.typeOrdinal];
        if (type.projectOrdinal == rootProjectOrdinal && !type.isStatic &&
            isPubliclyReachableNamedType(types, table.typeOrdinal))
            exportSymbols.push_back(table.descriptorAccessorName);
    }
    for (const frontend::ProjectOopMirStructLayout &layout : compilation.oopMirModel()->structLayouts())
    {
        if (layout.isImported || !layout.requiresDrop || layout.dropGlueEntryName.empty() ||
            layout.typeOrdinal >= types.size())
            continue;
        const frontend::NamedTypeDeclaration &type = types[layout.typeOrdinal];
        if (type.projectOrdinal == rootProjectOrdinal && isPubliclyReachableNamedType(types, layout.typeOrdinal))
            exportSymbols.push_back(layout.dropGlueEntryName);
    }
}

void collectDynamicStateExports(const frontend::ProjectCompilation &compilation,
                                std::span<const frontend::NamedTypeDeclaration> types,
                                std::span<const frontend::ClassMemberSymbol> members, std::size_t rootProjectOrdinal,
                                std::vector<std::string> &exportSymbols)
{
    const auto templateDependencies =
        interop::metadata_detail::templateDependencyMemberOrdinals(compilation, rootProjectOrdinal);
    for (const frontend::ProjectOopMirStaticField &field : compilation.oopMirModel()->staticFields())
    {
        if (field.isImported || field.isConst || field.declaringTypeOrdinal >= types.size() ||
            field.memberOrdinal >= members.size())
            continue;
        const frontend::NamedTypeDeclaration &type = types[field.declaringTypeOrdinal];
        const frontend::ClassMemberSymbol &member = members[field.memberOrdinal];
        if (type.projectOrdinal == rootProjectOrdinal &&
            isPubliclyReachableNamedType(types, field.declaringTypeOrdinal) &&
            (hasConsumerCallableAccessibility(member.accessibility) ||
             templateDependencies.contains(field.memberOrdinal)))
            exportSymbols.push_back(field.accessorName);
    }
    if (compilation.namespaceFunctionModel() == nullptr)
        return;
    for (const frontend::NamespaceStateSymbol &state : compilation.namespaceFunctionModel()->states())
    {
        if (state.projectOrdinal == rootProjectOrdinal &&
            state.accessibility == frontend::NamespaceFunctionAccessibility::Public && !state.isConst &&
            !state.isPrecompiled && !state.isCppExternal)
            exportSymbols.push_back("__nsc_get_" + state.storageName);
    }
}

bool hasDynamicPublicMetadataSurface(const frontend::ProjectCompilation &compilation,
                                     std::span<const frontend::NamedTypeDeclaration> types,
                                     std::size_t rootProjectOrdinal)
{
    const bool hasPublicType = std::ranges::any_of(
        types,
        [&](const auto &type)
        {
            return type.projectOrdinal == rootProjectOrdinal &&
                   isPubliclyReachableNamedType(types, static_cast<std::size_t>(&type - types.data()));
        });
    const bool hasPublicFunction =
        compilation.namespaceFunctionModel() != nullptr &&
        std::ranges::any_of(compilation.namespaceFunctionModel()->functions(),
                            [&](const auto &function)
                            {
                                return function.projectOrdinal == rootProjectOrdinal &&
                                       function.accessibility == frontend::NamespaceFunctionAccessibility::Public &&
                                       !function.isPrecompiled;
                            });
    const bool hasPublicState =
        compilation.namespaceFunctionModel() != nullptr &&
        std::ranges::any_of(compilation.namespaceFunctionModel()->states(),
                            [&](const auto &state)
                            {
                                return state.projectOrdinal == rootProjectOrdinal &&
                                       state.accessibility == frontend::NamespaceFunctionAccessibility::Public &&
                                       !state.isPrecompiled;
                            });
    const bool hasPublicGeneric =
        compilation.genericModel() != nullptr &&
        std::ranges::any_of(compilation.genericModel()->templates(),
                            [&](const auto &generic)
                            {
                                if (generic.projectOrdinal != rootProjectOrdinal || generic.syntax == nullptr)
                                    return false;
                                return generic.kind == frontend::GenericTemplateKind::Method
                                           ? generic.syntax->hasModifier(lexer::TokenKind::KwPublic)
                                           : generic.syntax->tokenKind() == lexer::TokenKind::KwPublic;
                            });
    return hasPublicType || hasPublicFunction || hasPublicState || hasPublicGeneric;
}

bool collectDynamicLibraryExports(const frontend::ProjectCompilation &compilation,
                                  std::vector<std::string> &exportSymbols, std::ostream &error)
{
    const auto types = compilation.nameLookup()->namedTypes();
    const auto members = compilation.memberModel()->members();
    const std::size_t rootProjectOrdinal = compilation.projectGraph()->projects().size() - 1;
    collectDynamicMethodExports(compilation, types, members, rootProjectOrdinal, exportSymbols);
    collectDynamicNamespaceExports(compilation, rootProjectOrdinal, exportSymbols);
    collectDynamicLayoutExports(compilation, types, rootProjectOrdinal, exportSymbols);
    collectDynamicStateExports(compilation, types, members, rootProjectOrdinal, exportSymbols);
    std::sort(exportSymbols.begin(), exportSymbols.end());
    exportSymbols.erase(std::unique(exportSymbols.begin(), exportSymbols.end()), exportSymbols.end());
    if (!exportSymbols.empty() || hasDynamicPublicMetadataSurface(compilation, types, rootProjectOrdinal))
        return true;
    error << "nsc: error: NativeSharp DLL requires at least one publishable "
             "public type or method in the root project\n";
    return false;
}

bool prepareDynamicLibraryBuilder(const frontend::ProjectCompilation &compilation,
                                  const std::vector<std::filesystem::path> &extraLinkLibraries,
                                  std::vector<std::filesystem::path> &linkLibraries,
                                  std::vector<std::filesystem::path> &runtimeLibraries,
                                  linker::WindowsLibraryBuilder &builder, std::ostream &error)
{
    linkLibraries = extraLinkLibraries;
    std::vector<linker::ExternalImportModule> inheritedImports;
    std::string referenceError;
    if (!appendValidatedLibraryReferences(compilation, linkLibraries, runtimeLibraries, referenceError,
                                          &inheritedImports))
    {
        error << "nsc: error: precompiled library reference validation failed: " << referenceError << '\n';
        return false;
    }
    builder.setProgramObjectsVerified(true);
    builder.setRuntimeLibrary(linker::configuredWindowsRuntimeLibrary());
    std::vector<linker::ExternalImportModule> externalImports = collectExternalImports(*compilation.oopMirModel());
    ExternalImportListBuilder externalImportList(externalImports);
    for (const linker::ExternalImportModule &module : inheritedImports)
        for (const std::string &symbol : module.symbols)
            externalImportList.append(module.dllName, symbol);
    if (!validateSystemImportsWhenRequested(externalImports, error)) return false;
    if (!appendCppTlsAliasedFunctionImports(compilation, runtimeLibraries, externalImports, referenceError))
    {
        error << "nsc: error: C/C++ alias imports failed: " << referenceError << '\n';
        return false;
    }
    builder.setExternalImports(std::move(externalImports));
    builder.setExtraLinkLibraries(linkLibraries);
    return true;
}

bool buildDynamicLibraryProduct(const std::filesystem::path &destination,
                                const frontend::ProjectCompilation &compilation,
                                const llvm_backend::ProjectOopObjectBundle &objectBundle,
                                std::span<const std::string> exportSymbols,
                                std::vector<std::filesystem::path> &runtimeLibraries,
                                CppTlsRuntimeAliases &aliases,
                                llvm_backend::LlvmOptimizationLevel optimizationLevel,
                                llvm_backend::DebugInformationKind debugInformation,
                                linker::WindowsLibraryBuilder &builder, ArtifactPublisher &artifactPublisher,
                                DynamicLibraryPublication &publication, std::string &buildError,
                                std::vector<std::filesystem::path> &dynamicallyLoadedRuntimes, std::ostream &error)
{
    const bool publishPdb = debugInformation != llvm_backend::DebugInformationKind::None;
    if (!artifactPublisher.prepareDynamicLibrary(destination, publishPdb, publication, buildError))
    {
        error << "nsc: error: DLL output preflight failed: " << buildError << '\n';
        return false;
    }
    if (!measureDriverPhase("dynamic-library-build",
                            [&]
                            {
                                return builder.buildDynamicObjects(objectBundle.objectFiles(),
                                                                   objectBundle.combinedLlvmText(), exportSymbols,
                                                                   publication.stage, publication.importLibraryStage,
                                                                   optimizationLevel, debugInformation, buildError);
                            }))
    {
        error << "nsc: error: NativeSharp DLL generation failed: " << buildError << '\n';
        return false;
    }
    std::string referenceError;
    dynamicallyLoadedRuntimes.clear();
    if (!appendReferencedCppRuntimes(compilation, builder.dynamicLibraryInfo(), runtimeLibraries, dynamicallyLoadedRuntimes, referenceError))
    {
        error << "nsc: error: C/C++ runtime selection failed: " << referenceError << '\n';
        return false;
    }
    if (!computeCppTlsRuntimeAliases(compilation, dynamicallyLoadedRuntimes, runtimeLibraries,
                                     builder.dynamicLibraryInfo(), aliases, referenceError))
    {
        error << "nsc: error: C/C++ runtime alias selection failed: " << referenceError << '\n';
        return false;
    }
    if (!validateCppRuntimeDestinations(compilation, runtimeLibraries, aliases,
                                        destination.parent_path(), referenceError))
    {
        error << "nsc: error: C/C++ runtime output preflight failed: " << referenceError << '\n';
        return false;
    }
    if (!verifyLinkedRuntimeLibraries(runtimeLibraries, builder.dynamicLibraryInfo(), publication.stage,
                                      dynamicallyLoadedRuntimes, referenceError))
    {
        error << "nsc: error: precompiled DLL/import-library mismatch: " << referenceError << '\n';
        return false;
    }
    interop::CHeaderGenerator headerGenerator;
    std::string headerError;
    if (!measureDriverPhase("host-header", [&]
                            { return headerGenerator.writeHeader(compilation, publication.headerStage, headerError); }))
    {
        error << "nsc: error: Stage 48.1 DLL C header generation failed: " << headerError << '\n';
        return false;
    }
    std::string metadataError;
    if (!measureDriverPhase(
            "library-metadata",
            [&] { return writeVerifiedLibraryMetadata(compilation, publication.metadataStage, metadataError); }))
    {
        error << "nsc: error: Stage 48.2 DLL metadata generation failed: " << metadataError << '\n';
        return false;
    }
    publishPrecompiledSnapshot(compilation, publication.metadataStage);
    return true;
}

bool commitDynamicLibraryProduct(const std::filesystem::path &destination,
                                 const frontend::ProjectCompilation &compilation,
                                 std::span<const std::filesystem::path> runtimeLibraries,
                                 std::span<const std::filesystem::path> dynamicallyLoadedRuntimes,
                                 const CppTlsRuntimeAliases &aliases, bool publishPdb,
                                 ArtifactPublisher &artifactPublisher, DynamicLibraryPublication &publication,
                                 std::string &buildError, std::vector<std::filesystem::path> &packagedRuntimes,
                                 std::ostream &error)
{
    std::string referenceError;
    if (!packageRuntimeClosure(runtimeLibraries, dynamicallyLoadedRuntimes, aliases, publication.stage,
                               packagedRuntimes, referenceError))
    {
        removePackagedFiles(packagedRuntimes);
        error << "nsc: error: runtime packaging failed: " << referenceError << '\n';
        return false;
    }
    bool protectExistingRuntimes = false;
    if (!validatePackagedCppRuntimeDestinations(compilation, runtimeLibraries, packagedRuntimes,
                                                destination.parent_path(), protectExistingRuntimes, referenceError))
    {
        removePackagedFiles(packagedRuntimes);
        error << "nsc: error: C/C++ runtime output preflight failed: " << referenceError << '\n';
        return false;
    }
    if (publishPdb && !artifactPublisher.addDynamicLibraryPdb(publication, buildError))
    {
        error << "nsc: error: DLL PDB output preflight failed: " << buildError << '\n';
        return false;
    }
    if (!artifactPublisher.addRuntimeArtifacts(destination.parent_path(), packagedRuntimes, buildError, protectExistingRuntimes))
    {
        error << "nsc: error: DLL runtime output preflight failed: " << buildError << '\n';
        return false;
    }
    if (!artifactPublisher.commit(buildError))
    {
        error << "nsc: error: DLL publication failed: " << buildError << '\n';
        return false;
    }
    return true;
}

void printDynamicLibraryGenerationResult(const llvm_backend::ProjectOopObjectBundle &objectBundle,
                                         const linker::WindowsLibraryBuilder &builder,
                                         const DynamicLibraryPublication &publication,
                                         llvm_backend::LlvmOptimizationLevel optimizationLevel,
                                         llvm_backend::DebugInformationKind debugInformation,
                                         std::size_t linkLibraryCount, std::size_t packagedRuntimeCount,
                                         std::ostream &output)
{
    output << "project OOP DLL emission: ok\n"
           << "format: PE32+ DLL\n"
           << "machine: AMD64 (0x8664)\n"
           << "optimization: " << llvm_backend::llvmOptimizationLevelName(optimizationLevel) << '\n'
           << "debug-information: " << llvm_backend::debugInformationKindName(debugInformation) << '\n'
           << "code-units: " << objectBundle.codeUnitCount() << '\n'
           << "artifact-cache-hits: " << objectBundle.cacheHitCount() << '\n'
           << "artifact-cache-misses: " << objectBundle.cacheMissCount() << '\n'
           << "program-database: "
           << (debugInformation == llvm_backend::DebugInformationKind::None
                   ? "none"
                   : pathToUtf8(publication.pdbDestination))
           << '\n'
           << "linked-libraries: " << linkLibraryCount << '\n'
           << "packaged-runtimes: " << packagedRuntimeCount << '\n'
           << "import-library: " << pathToUtf8(publication.importLibraryDestination) << '\n'
           << "host-header: " << pathToUtf8(publication.headerDestination) << '\n'
           << "library-metadata: " << pathToUtf8(publication.metadataDestination) << '\n'
           << "exports: " << builder.dynamicLibraryInfo().exportedSymbols.size() << '\n';
    for (const std::string &symbol : builder.dynamicLibraryInfo().exportedSymbols)
        output << "  " << symbol << '\n';
    output << "exception-directory-rva: " << builder.dynamicLibraryInfo().exceptionDirectoryRva << '\n'
           << "exception-directory-size: " << builder.dynamicLibraryInfo().exceptionDirectorySize << '\n'
           << "clr-header-rva: " << builder.dynamicLibraryInfo().clrDirectoryRva << '\n';
    if (activeExecutionScope != nullptr && activeExecutionScope->reportTiming)
        appendCodeUnitTelemetry(output, objectBundle);
}

ExitCode Driver::runProjectOopDynamicLibraryGeneration(std::string_view inputPath, std::string_view outputPath,
                                                       llvm_backend::LlvmOptimizationLevel optimizationLevel,
                                                       llvm_backend::DebugInformationKind debugInformation,
                                                       const std::vector<llvm_backend::DebugPrefixMap> &debugPrefixMaps,
                                                       llvm_backend::LinkTimeOptimizationMode linkTimeOptimization,
                                                       const std::vector<std::filesystem::path> &extraLinkLibraries,
                                                       std::string_view linkerSelection,
                                                       std::ostream &output, std::ostream &error)
{
    std::string loadError;
    std::optional<compilation::CompilationInputManifest> manifest =
        loadCompilationInputManifest(pathFromUtf8(inputPath), loadError);
    if (!manifest.has_value())
    {
        error << "nsc: error: " << loadError << '\n';
        return ExitCode::CompilationError;
    }
    const std::string selectedLinker = selectDirectLinkedProductLinker(*manifest, linkerSelection, extraLinkLibraries);
    addCompilationProductIdentity(
        *manifest, "dynamic-library", pathFromUtf8(outputPath));
    addCompilationCodegenIdentity(
        *manifest, optimizationLevel, debugInformation,
        debugPrefixMaps, linkTimeOptimization);
    addCompilationLinkIdentity(
        *manifest, false, linker::WindowsExecutableSubsystem::Console,
        extraLinkLibraries, selectedLinker);
    frontend::ProjectCompilation compilation;
    compilation.setReceiverRequiresThinLtoSidecar(
        llvm_backend::ProjectOopObjectGenerator::thinLtoRuns(linkTimeOptimization, optimizationLevel));
    bool restored = false;
    if (!prepareDynamicLibraryProject(
            inputPath, *manifest, compilation, restored, error))
        return ExitCode::CompilationError;
    if (restored)
    {
        output << manifest->bundleLookup().resultOutput;
        return ExitCode::Success;
    }

    std::vector<std::string> exportSymbols;
    if (!collectDynamicLibraryExports(compilation, exportSymbols, error))
        return ExitCode::CompilationError;
    std::string codeGenerationError;
    ThinLtoLibraryLinking thinLtoLibraries;
    if (!thinLtoLibraries.collect(
            compilation, linkTimeOptimization, optimizationLevel, debugInformation, codeGenerationError))
    {
        error << "nsc: error: " << codeGenerationError << '\n';
        return ExitCode::CompilationError;
    }
    // As for an executable, the link inputs come first so ThinLTO knows what to keep visible (item 50).
    // Igual que en un ejecutable, las entradas del enlace van primero para que ThinLTO sepa que dejar
    // visible (punto 50).
    const std::filesystem::path destination = pathFromUtf8(outputPath);
    std::vector<std::filesystem::path> linkLibraries;
    std::vector<std::filesystem::path> runtimeLibraries;
    linker::WindowsLibraryBuilder builder;
    builder.setLinkerSelection(selectedLinker);
    if (!prepareDynamicLibraryBuilder(compilation, extraLinkLibraries, linkLibraries, runtimeLibraries, builder, error))
        return ExitCode::CompilationError;
    std::optional<std::vector<std::string>> preservedSymbols =
        thinLtoLibraries.preservedSymbols(linkLibraries, exportSymbols);
    std::unique_ptr<llvm_backend::ProjectOopObjectBundle> objectBundle = generateProjectObjects(
        compilation, debugInformation, optimizationLevel, debugPrefixMaps, linkTimeOptimization, codeGenerationError,
        false, thinLtoLibraries.takeLibraries(), std::move(preservedSymbols));
    if (objectBundle == nullptr)
    {
        error << "nsc: error: " << codeGenerationError << '\n';
        return ExitCode::CompilationError;
    }
    if (!thinLtoLibraries.replaceLinkLibraries(*objectBundle, linkLibraries, codeGenerationError))
    {
        error << "nsc: error: ThinLTO library linking failed: " << codeGenerationError << '\n';
        return ExitCode::CompilationError;
    }
    builder.setExtraLinkLibraries(linkLibraries);
    if (!rejectUnstartedCrtInitializers(objectBundle->objectFiles(), linkLibraries, false, error))
        return ExitCode::CompilationError;
    builder.setRemoveUnreferencedSections(objectBundle->thinLtoInternalized());

    std::string buildError;
    ArtifactPublisher artifactPublisher;
    DynamicLibraryPublication publication;
    CppTlsRuntimeAliases aliases;
    std::vector<std::filesystem::path> dynamicallyLoadedRuntimes;
    if (!buildDynamicLibraryProduct(destination, compilation, *objectBundle, exportSymbols, runtimeLibraries,
                                    aliases, optimizationLevel, debugInformation, builder, artifactPublisher, publication,
                                    buildError, dynamicallyLoadedRuntimes, error))
    {
        explainMissingCppSymbols(compilation, objectBundle->objectFiles(), linkLibraries, error);
        return ExitCode::CompilationError;
    }

    const bool publishPdb = debugInformation != llvm_backend::DebugInformationKind::None;
    std::vector<std::filesystem::path> packagedRuntimes;
    if (!commitDynamicLibraryProduct(destination, compilation, runtimeLibraries, dynamicallyLoadedRuntimes, aliases,
                                     publishPdb, artifactPublisher, publication, buildError, packagedRuntimes, error))
        return ExitCode::CompilationError;
    addRuntimeBundleOutputs(*manifest, packagedRuntimes, runtimeLibraries, aliases);
    std::ostringstream resultOutput;
    printDynamicLibraryGenerationResult(*objectBundle, builder, publication, optimizationLevel, debugInformation,
                                        linkLibraries.size(), packagedRuntimes.size(), resultOutput);
    thinLtoLibraries.printResult(resultOutput);
    manifest->setBundleResultOutput(resultOutput.str());
    publishCompilationBundle(*manifest);
    output << resultOutput.str();
    return ExitCode::Success;
}
} // namespace nativesharp::driver
