#pragma once

#include "nativesharp/Basic/CompilationEvent.h"
#include "nativesharp/Compilation/LibraryMetadataRepository.h"
#include "nativesharp/Compilation/ProjectGraph.h"
#include "nativesharp/Compilation/ProjectLoader.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Interop/CppInteropSession.h"
#include "nativesharp/Interop/CppProjectSessions.h"
#include "nativesharp/Frontend/CppImportedTypeAlias.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectCompilationLifetime.h"
#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "nativesharp/Frontend/ProjectGenericConstraintModel.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNamespaceFunctionModel.h"
#include "nativesharp/Frontend/ProjectInheritanceModel.h"
#include "nativesharp/Frontend/ProjectInterfaceModel.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/Frontend/ProjectMetaprogrammingModel.h"
#include "nativesharp/Frontend/ProjectVirtualDispatchModel.h"
#include "nativesharp/Frontend/ProjectDestructionModel.h"
#include "nativesharp/Frontend/ProjectConstructionModel.h"
#include "nativesharp/Frontend/CompilationStage.h"
#include "nativesharp/Frontend/ComptimeEngine.h"
#include "nativesharp/Frontend/ProjectObjectLayoutModel.h"
#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/Frontend/ProjectSyntaxUnit.h"
#include "nativesharp/Frontend/PrecompiledSnapshotUnit.h"
#include "nativesharp/Frontend/PrecompiledSyntaxModule.h"
#include "nativesharp/Source/SourceManager.h"
#include "Internal/ProjectSyntaxSourceRead.h"

#include <unordered_set>
#include <chrono>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::driver
{
class CompilationWorkerPool;
}

namespace nativesharp::interop { struct LibraryMetadata; }

namespace nativesharp::frontend
{
class ProjectCompilation final
{
public:
    using SourceBufferProvider =
        std::function<std::optional<std::string>(const std::filesystem::path&)>;
    using CppFunctionProvider = std::function<bool(
        const compilation::ProjectGraph&, std::span<const interop::CppProjectSessions>,
        std::span<const std::unique_ptr<ProjectSyntaxUnit>>,
        std::span<const CppFunctionDemand>,
        std::vector<NamespaceFunctionSymbol>&,
        std::vector<interop::CppDeferredFunctionCandidate>&, std::string&)>;
    using CppNamespaceProvider = std::function<bool(
        const compilation::ProjectGraph&, std::span<const interop::CppProjectSessions>,
        std::vector<std::pair<std::size_t, std::string>>&, std::string&)>;
    using CppGlobalProvider = std::function<bool(
        const compilation::ProjectGraph&, std::span<const interop::CppProjectSessions>,
        std::span<const CppFunctionDemand>, std::vector<NamespaceStateSymbol>&, std::string&)>;
    using ExperimentalCppTypeProvider = std::function<bool(
        const compilation::ProjectGraph&, std::span<const interop::CppProjectSessions>,
        std::vector<CppImportedTypeDeclaration>&, std::string&)>;
    using CppTypeAliasProvider = std::function<bool(
        const compilation::ProjectGraph&, std::span<const interop::CppProjectSessions>,
        std::vector<CppImportedTypeAlias>&, std::string&)>;
    using ExperimentalCppTypeSelector = std::function<bool(
        const ProjectCompilation&, std::vector<CppImportedTypeDeclaration>&, std::string&)>;
    using CppPrecompiledParserTypeProvider = std::function<bool(
        const ProjectCompilation&, std::vector<CppImportedTypeDeclaration>&, std::string&)>;
    using ExperimentalCppInstanceMethodProvider = std::function<bool(
        const compilation::ProjectGraph&, std::span<const interop::CppProjectSessions>,
        const ProjectNameLookup&, std::vector<CppImportedInstanceMethod>&, std::string&)>;
    using ExperimentalCppLifetimeMethodProvider = std::function<bool(
        const compilation::ProjectGraph&, std::span<const interop::CppProjectSessions>,
        const ProjectNameLookup&, std::vector<CppImportedLifetimeMethod>&, std::string&)>;
    using ExperimentalCppInstanceFieldProvider = std::function<bool(
        const compilation::ProjectGraph&, std::span<const interop::CppProjectSessions>,
        const ProjectNameLookup&, std::vector<CppImportedInstanceField>&, std::string&)>;
    using ExperimentalCppStaticMethodProvider = std::function<bool(
        const compilation::ProjectGraph&, std::span<const interop::CppProjectSessions>,
        const ProjectNameLookup&, std::vector<CppImportedStaticMethod>&, std::string&)>;
    using ExperimentalCppStaticFieldProvider = std::function<bool(
        const compilation::ProjectGraph&, std::span<const interop::CppProjectSessions>,
        const ProjectNameLookup&, std::vector<CppImportedStaticField>&, std::string&)>;
    using CppRecordObserver = std::function<void(const ProjectCompilation&)>;
    using CppMetadataCatalogProvider = std::function<bool(
        const ProjectCompilation&, const interop::LibraryMetadata&, std::vector<std::uint8_t>&, std::string&)>;

    void setLifetime(ProjectCompilationLifetime& lifetime) noexcept;
    // Must be called before the first compilation attempt. Ownership is transferred.
    bool setCppInteropSession(std::unique_ptr<interop::CppInteropSession> session, std::string& error);
    interop::CppInteropSession* cppInteropSession() const noexcept;
    bool setCppProjectSessions(std::vector<interop::CppProjectSessions> sessions, std::string& error);
    // EN: T19-003. Adds the macro expansion sessions of precompiled projects, before compilation starts.
    // ES: T19-003. Agrega las sesiones de expansion de macros de proyectos precompilados, antes de compilar.
    bool appendCppProjectSessions(std::vector<interop::CppProjectSessions> sessions, std::string& error);
    std::span<const interop::CppProjectSessions> cppProjectSessions() const noexcept;
    struct CppTemplateDemand
    {
        std::string templateKey;
        CppFunctionDemand demand;
    };
    // Candidates retain the producer's lookup context; they are not resolved C bindings.
    std::span<const CppTemplateDemand> cppTemplateDemands() const noexcept;
    bool setCppImportedFunctions(std::vector<NamespaceFunctionSymbol> functions, std::string& error);
    bool setCppVariadicCallPlanProvider(CppVariadicCallPlanProvider provider, std::string& error);
    bool setCppFunctionPointerPlanProvider(CppFunctionPointerPlanProvider provider, std::string& error);
    bool setCppVariadicFunctionPointerPlanProvider(CppVariadicFunctionPointerPlanProvider provider, std::string& error);
    bool setExperimentalCppImportedStates(std::vector<NamespaceStateSymbol> states, std::string& error);
    // Experimental injection for isolated compiler tests. Imported records do not yet have
    // object-layout or lifetime semantics and this must not be enabled by the production driver.
    bool setExperimentalCppImportedTypes(std::vector<CppImportedTypeDeclaration> types, std::string& error);
    bool setExperimentalCppTypeProvider(ExperimentalCppTypeProvider provider, std::string& error);
    bool setCppTypeAliasProvider(CppTypeAliasProvider provider, std::string& error);
    bool setExperimentalCppTypeSelector(ExperimentalCppTypeSelector selector, std::string& error);
    bool setCppPrecompiledParserTypeProvider(CppPrecompiledParserTypeProvider provider, std::string& error);
    bool setExperimentalCppInstanceMethodProvider(ExperimentalCppInstanceMethodProvider provider, std::string& error);
    bool setExperimentalCppLifetimeMethodProvider(ExperimentalCppLifetimeMethodProvider provider, std::string& error);
    const std::set<std::pair<std::size_t, const interop::CppInteropSession*>>& cppClassTemplateSessions() const noexcept
    { return cppClassTemplateSessions_; }
    bool hasExperimentalCppLifetimeMethodProvider() const noexcept { return static_cast<bool>(experimentalCppLifetimeMethodProvider_); }
    bool setExperimentalCppInstanceFieldProvider(ExperimentalCppInstanceFieldProvider provider, std::string& error);
    bool setExperimentalCppStaticMethodProvider(ExperimentalCppStaticMethodProvider provider, std::string& error);
    bool setExperimentalCppStaticFieldProvider(ExperimentalCppStaticFieldProvider provider, std::string& error);
    // Lowering for the subset currently represented by the C++ member providers.
    bool enableCppImportedMethodLowering(std::string& error);
    bool enableCppImportedFieldLowering(std::string& error);
    // Compatibility entry points for isolated compiler experiments.
    bool enableExperimentalCppInstanceMethodLoweringForTests(std::string& error);
    bool enableExperimentalCppInstanceFieldLoweringForTests(std::string& error);
    bool enableExperimentalCppLifetimeAnalysisForTests(std::string& error);
    bool enableExperimentalCppLocalLifetimeAnalysisForTests(std::string& error);
    bool enableCppImportedNoexceptLocalLifetime(std::string& error);
    bool enableExperimentalCppDiscardedLifetimeLoweringForTests(std::string& error);
    bool setCppFunctionProvider(CppFunctionProvider provider, std::string& error);
    bool setCppNamespaceProvider(CppNamespaceProvider provider, std::string& error);
    bool setCppGlobalProvider(CppGlobalProvider provider, std::string& error);
    bool setCppRecordObserver(CppRecordObserver observer, std::string& error);
    bool setCppMetadataCatalogProvider(CppMetadataCatalogProvider provider, std::string& error);
    bool collectCppMetadataCatalog(const interop::LibraryMetadata& metadata,
                                   std::vector<std::uint8_t>& output, std::string& error) const;
    void setCompilationJobs(std::size_t jobs) noexcept;
    void setProjectLoadOptions(compilation::ProjectLoadOptions options);
    const compilation::ProjectLoadOptions& projectLoadOptions() const noexcept;
    std::size_t compilationJobs() const noexcept;
    void setTemplateCacheRoot(std::filesystem::path root);
    const std::filesystem::path& precompiledProductCacheRoot() const noexcept;
    void setCancellationCheck(std::function<bool()> check);
    // Called concurrently by syntax workers. Returning nullopt falls back to the file on disk.
    void setSourceBufferProvider(SourceBufferProvider provider);
    void setPhaseTimingSink(
        std::function<void(std::string_view, std::chrono::nanoseconds)> sink);
    void setProgressSink(std::function<void(const basic::CompilationEvent&)> sink);
    // EN: Receives the report lines of --time that are not phase timings (for example the
    //     precompiled snapshot counts). Unset, nothing is reported.
    // ES: Recibe las líneas del reporte de --time que no son tiempos de fase (por ejemplo, las
    //     cuentas de snapshots precompilados). Sin asignar, no se reporta nada.
    void setReportSink(std::function<void(std::string_view)> sink);
    // EN: T08-D05b. Returns the DLL identities (`name:sha256`) of C runtimes whose dependency closure holds a
    //     homonymous DLL with other contents in another runtime's closure. HIR binds the functions of a TLS alias
    //     of such a DLL at run time, so the alias loader can isolate its dependencies. Unset, nothing is isolated.
    // ES: T08-D05b. Devuelve las identidades (`nombre:sha256`) de los runtimes C cuyo cierre de dependencias tiene
    //     una DLL homonima con otro contenido en el cierre de otro runtime. HIR enlaza en ejecucion las funciones
    //     de un alias TLS de esa DLL, para que el cargador de alias aisle sus dependencias. Sin asignar, no se
    //     aisla nada.
    using CppRuntimeIsolationResolver = std::function<bool(
        const ProjectCompilation&, std::unordered_set<std::string>&, std::string&)>;
    void setCppRuntimeIsolationResolver(CppRuntimeIsolationResolver resolver);
#if 0 // INSTRUMENTATION CODE: detailed generics trace support.
    // Detailed, high-volume diagnostics are routed separately so normal --progress output keeps
    // its stable concise contract. The driver currently uses this for --generics-trace=<file>.
    void setDetailedProgressSink(std::function<void(const basic::CompilationEvent&)> sink);
#endif // INSTRUMENTATION CODE
    bool setPrecompiledModules(
        std::vector<PrecompiledSyntaxModule> modules,
        std::string& error);
    // The driver keeps this compilation alive through linking or bundle restoration.
    void retainReceiverProductGuard(std::shared_ptr<void> guard) {
        receiverProductGuards_.push_back(std::move(guard));
    }
    void setReceiverProductIdentity(std::string identity) { receiverProductIdentity_ = std::move(identity); }
    std::string_view receiverProductIdentity() const noexcept { return receiverProductIdentity_; }
    void setReceiverRequiresThinLtoSidecar(bool required) noexcept { receiverRequiresThinLtoSidecar_ = required; }
    bool receiverRequiresThinLtoSidecar() const noexcept { return receiverRequiresThinLtoSidecar_; }
    bool setPreloadedProjectGraph(
        compilation::ProjectGraph graph,
        std::string& error);

    bool compileProjectSyntax(const std::filesystem::path& projectFile, std::string& loadError);
    bool compileProjectNames(const std::filesystem::path& projectFile, std::string& loadError);
    bool compileProjectGenerics(const std::filesystem::path& projectFile, std::string& loadError);
    bool compileProjectMembers(const std::filesystem::path& projectFile, std::string& loadError);
    bool compileProjectInheritance(const std::filesystem::path& projectFile, std::string& loadError);
    bool compileProjectInterfaces(const std::filesystem::path& projectFile, std::string& loadError);
    // NSC-LIMIT-007. FASE PROPIA, y no un trozo de la de cuerpos.
    //
    // Estaba incrustada dentro de `compileProjectMethodBodies`, de modo que pedir «comprueba los
    // constraints» analizaba ademas todos los cuerpos del proyecto y podia fallar por un error
    // que no tenia nada que ver con un `where`. Aqui la cadena para justo despues de las
    // interfaces, que es lo ultimo que la validacion necesita.
    bool compileProjectGenericConstraints(
        const std::filesystem::path& projectFile,
        std::string& loadError);
    // Construye el modelo de constraints dando por hecho que las fases anteriores YA estan.
    // Existe para que la fase propia y la de cuerpos compartan la construccion sin que la
    // segunda tenga que encadenarse por el valor de retorno de la primera.
    bool buildGenericConstraintModel();
    bool compileProjectMethodBodies(const std::filesystem::path& projectFile, std::string& loadError);
    bool compileProjectVirtualDispatch(const std::filesystem::path& projectFile, std::string& loadError);
    bool compileProjectDestruction(const std::filesystem::path& projectFile, std::string& loadError);
    bool compileProjectConstructions(const std::filesystem::path& projectFile, std::string& loadError);
    bool compileProjectObjectLayouts(const std::filesystem::path& projectFile, std::string& loadError);
    bool compileProjectOopHir(const std::filesystem::path& projectFile, std::string& loadError);
    bool compileProjectOopMir(const std::filesystem::path& projectFile, std::string& loadError);
    bool completeCodeGeneration(std::string& error);
    std::span<const CompilationStage> completedStages() const noexcept;

    const compilation::ProjectGraph* projectGraph() const noexcept;
    compilation::LibraryMetadataRepository& libraryMetadataRepository() noexcept;
    const compilation::LibraryMetadataRepository& libraryMetadataRepository() const noexcept;
    ProjectCompilationLifetime& lifetime() noexcept;
    const ProjectCompilationLifetime& lifetime() const noexcept;
    ProjectCompilationIndexes& indexes() noexcept;
    const ProjectCompilationIndexes& indexes() const noexcept;
    const source::SourceManager& sourceManager() const noexcept;
    bool cloneSyntaxForObservation(
        std::vector<std::unique_ptr<ProjectSyntaxUnit>>& output,
        diagnostics::DiagnosticEngine& diagnostics,
        std::string& error) const;
    // EN: The C/C++ type aliases given to the name lookup, live and from precompiled catalogs.
    // ES: Los alias de tipos C/C++ que recibe la busqueda de nombres, vivos y de catalogos precompilados.
    std::span<const CppImportedTypeAlias> cppImportedTypeAliases() const noexcept;
    diagnostics::DiagnosticEngine& diagnostics() noexcept;
    const diagnostics::DiagnosticEngine& diagnostics() const noexcept;
    std::span<const std::unique_ptr<ProjectSyntaxUnit>> units() const noexcept;
    std::span<const ProjectComptimeRecord> comptimeRecords() const noexcept;
    // EN: Semantic snapshots accepted for restored public API units, in module order.
    // ES: Snapshots semánticos aceptados para unidades de API pública restauradas, en orden de
    //     módulo.
    std::span<const PrecompiledSnapshotUnit> precompiledSnapshots() const noexcept;
    std::span<const PrecompiledSyntaxModule> precompiledModules() const noexcept;
    // EN: Producer side (plan item 2.2). The reference compilation of a library names the library
    //     here before compiling; the subphase `precompiled-snapshot` then indexes the restored API
    //     unit of that module, before any phase mutates it, and keeps it for capture.
    // ES: Lado productor (punto 2.2 del plan). La compilación de referencia de una biblioteca nombra
    //     aquí la biblioteca antes de compilar; la subfase `precompiled-snapshot` indexa entonces la
    //     unidad de API restaurada de ese módulo, antes de que la mute cualquier fase, y la conserva
    //     para la captura.
    void setPrecompiledSnapshotCaptureModule(std::string moduleName);
    const PrecompiledSnapshotUnit* precompiledSnapshotCapture() const noexcept;
    const ProjectNameLookup* nameLookup() const noexcept;
    const ProjectGenericModel* genericModel() const noexcept;
    const ProjectGenericConstraintModel* genericConstraintModel() const noexcept;
    const ProjectMemberModel* memberModel() const noexcept;
    const ProjectNamespaceFunctionModel* namespaceFunctionModel() const noexcept;
    const ProjectInheritanceModel* inheritanceModel() const noexcept;
    const ProjectInterfaceModel* interfaceModel() const noexcept;
    const ProjectMethodBodyModel* methodBodyModel() const noexcept;
    const ProjectVirtualDispatchModel* virtualDispatchModel() const noexcept;
    const ProjectDestructionModel* destructionModel() const noexcept;
    const ProjectConstructionModel* constructionModel() const noexcept;
    const ProjectObjectLayoutModel* objectLayoutModel() const noexcept;
    const ProjectMetaprogrammingModel* metaprogrammingModel() const noexcept;
    const ProjectOopHirModel* oopHirModel() const noexcept;
    const ProjectOopMirModel* oopMirModel() const noexcept;

private:
    bool cancellationRequested(std::string& loadError, std::string_view nextPhase) const;
    bool isSyntaxCompilationFresh(std::string& loadError) const;
    bool ensureProjectGraph(
        const std::filesystem::path& projectFile,
        std::string& loadError);
    bool addPrecompiledProjects(std::string& loadError);
    std::vector<ProjectSyntaxSourceRead> collectSyntaxSourceReads() const;
    bool readSyntaxSources(
        std::vector<ProjectSyntaxSourceRead>& sourceReads,
        driver::CompilationWorkerPool& syntaxWorkers,
        std::string& loadError);
    void appendComptimeRecord(
        const ProjectSyntaxSourceRead& read,
        const std::filesystem::path& relativePath);
    void reportComptimeDiagnostics(
        const ProjectSyntaxSourceRead& read,
        FileId fileId);
    bool materializeComptimeSource(
        ProjectSyntaxSourceRead& read,
        std::string& loadError);
    bool executeSyntaxComptime(
        std::vector<ProjectSyntaxSourceRead>& sourceReads,
        driver::CompilationWorkerPool& syntaxWorkers,
        std::string& loadError);
    std::optional<std::size_t> precompiledProjectOrdinal(
        const std::filesystem::path& canonicalMetadataPath) const;
    bool materializePrecompiledUnit(
        const PrecompiledSyntaxModule& module,
        std::size_t projectOrdinal,
        std::size_t sourceOrdinal,
        std::string_view displaySuffix,
        const std::string& sourceText,
        std::string_view publishedSyntaxProduct,
        bool isPrecompiled,
        std::string_view productName,
        std::string& loadError);
    bool restorePublishedPrecompiledSyntaxUnit(
        ProjectSyntaxUnit& unit,
        std::string_view payload,
        std::string_view productName,
        std::string& loadError);
    bool restorePrecompiledSyntaxUnit(
        ProjectSyntaxUnit& unit,
        std::string_view cacheKey);
    bool materializePrecompiledModule(
        const PrecompiledSyntaxModule& module,
        std::set<std::string>& materializedMetadata,
        std::string& loadError);
    bool materializePrecompiledTemplateUnit(
        const PrecompiledSyntaxModule& module,
        std::size_t projectOrdinal,
        std::string& loadError);
    bool materializePrecompiledSyntax(std::string& loadError);
    void loadPrecompiledSnapshots();
    bool loadPrecompiledSnapshot(
        const PrecompiledSyntaxModule& module,
        std::size_t unitOrdinal,
        std::span<const compilation::PrecompiledSnapshotDependency> loadedDependencies,
        std::string& reason);
    bool buildMemberModel(std::string& error);
    bool buildNamespaceFunctionModel(std::string& error);
    bool buildInheritanceModel();
    bool buildInterfaceModel();
    bool buildVirtualDispatchModel();
    bool buildDestructionModel();
    bool buildConstructionModel();
    bool buildObjectLayoutModel();
    bool calculateTemplateParseCacheKey(std::string& loadError);
    bool restoreTemplateParseProduct(std::string& loadError);
    void publishTemplateParseProduct();
    void publishPrecompiledSyntaxProducts();
    bool precompiledSyntaxProductPending(std::size_t index) const noexcept;
    void capturePristineSyntaxProducts(driver::CompilationWorkerPool& syntaxWorkers);
    std::string_view pristineSyntaxProduct(std::size_t index) const noexcept;
    void releasePristineSyntaxProducts() noexcept;
    bool produceSyntaxCorpus(
        driver::CompilationWorkerPool& syntaxWorkers,
        std::string& loadError);
    bool lexSyntaxUnits(
        driver::CompilationWorkerPool& syntaxWorkers,
        bool& lexicalOk,
        std::string& loadError);
    bool parseSyntaxUnits(
        driver::CompilationWorkerPool& syntaxWorkers,
        bool lexicalOk,
        std::string& loadError);
    bool refreshCppImportedTypes(std::string& loadError);
    bool applyCppTemplateUses(std::string& loadError);
    bool buildNamespaceFunctions(std::string& loadError);
    bool deduceCppTemplateCalls(std::span<const CppTemplateDeductionRequest> requests);
    // T5: the class template specialization descriptions of the record arguments of a deduction request.
    void describeCppRecordArguments(
        std::size_t projectOrdinal,
        std::vector<interop::CppTemplateArgument>& arguments,
        std::vector<interop::CppTemplateArgument>& explicitArguments) const;
    bool compileSyntaxCore(const std::filesystem::path& projectFile, std::string& loadError);
    bool enterStage(CompilationStage stage, std::string& error);

    // Declared before the project models so that it outlives their destruction.
    std::unique_ptr<interop::CppInteropSession> cppInteropSession_;
    std::vector<interop::CppProjectSessions> cppProjectSessions_;
    std::vector<CppTemplateDemand> cppTemplateDemands_;
    std::vector<NamespaceFunctionSymbol> cppImportedFunctions_;
    std::vector<NamespaceStateSymbol> experimentalCppImportedStates_;
    std::vector<CppImportedTypeDeclaration> experimentalCppImportedTypes_;
    std::vector<CppImportedTypeAlias> cppImportedTypeAliases_;
    // T13-006: the names of the specializations written in NativeSharp, referenced by the rewritten tokens.
    // T13-006: los nombres de las especializaciones escritas en NativeSharp, que usan los tokens reescritos.
    std::deque<std::string> cppTemplateLexemes_;
    // T13-005: the C++ function templates of each project, and whether their calls were already deduced.
    // T13-005: las plantillas de funcion C++ de cada proyecto, y si sus llamadas ya se dedujeron.
    std::map<std::size_t, std::set<std::string>> cppFunctionTemplates_;
    // EN: T19-003. Deduction runs in rounds: a call whose argument is another template or macro call is deduced
    //     once the inner call has a type. The calls already deduced are not deduced again.
    // ES: T19-003. La deduccion corre en rondas: una llamada cuyo argumento es otra llamada a una plantilla o
    //     macro se deduce cuando la interior ya tiene tipo. Las llamadas ya deducidas no se deducen otra vez.
    static constexpr std::size_t kCppTemplateDeductionRounds = 8;
    std::size_t cppTemplateDeductionRound_ = 0;
    std::set<const ast::AstNode*> cppDeducedTemplateCalls_;
    // X2: the demands of the member function template specializations deduced so far. / Las demandas de las
    //     especializaciones de plantillas de funcion miembro deducidas hasta ahora.
    std::vector<CppFunctionDemand> cppMemberTemplateDemands_;
    std::unordered_map<const ast::AstNode*, std::string> cppMemberTemplateSelections_;
    // T19-040. / T19-040.
    std::string argumentSpelling(const ast::AstNode* argument) const;
    // C2-05: whether a macro argument has no side effects (see CppTemplateArgument::sideEffectFree). / Si un
    //     argumento de macro no tiene efectos.
    bool macroArgumentSideEffectFree(const ast::AstNode* argument) const;
    bool cppTemplateCallPending(const ast::AstNode* syntax) const
    {
        return cppTemplateDeductionRound_ < kCppTemplateDeductionRounds && !cppDeducedTemplateCalls_.contains(syntax);
    }
    // T13-007: the NativeSharp structs mirrored for C++ templates. / Los structs reflejados para plantillas C++.
    std::set<std::string, std::less<>> cppNativeMirrorTypes_;
    std::map<std::string, interop::CppNativeStructLayout, std::less<>> cppNativeMirrorLayouts_;
    // T14-003: the macros of each project NativeSharp does not import, with the reason. / Las macros no importadas.
    std::map<std::size_t, std::map<std::string, std::string, std::less<>>> cppUnimportedMacros_;
    // EN: T19-003. The C function-like macros of each project, which a call expands through a Clang wrapper.
    // ES: T19-003. Las macros de funcion C de cada proyecto, que una llamada expande con un envoltorio de Clang.
    std::map<std::size_t, std::set<std::string, std::less<>>> cppFunctionMacros_;
    // C2-02: argument-list macros of each project and their element counts. / Macros lista de cada proyecto y su
    //     numero de elementos.
    std::map<std::size_t, std::map<std::string, std::size_t, std::less<>>> cppArgumentListMacros_;
    // C2-03: brace-initializer macros of each project and the record types Clang accepts them for. / Macros de
    //     inicializacion entre llaves de cada proyecto y los tipos record para los que Clang las acepta.
    std::map<std::size_t, std::map<std::string, std::vector<std::string>, std::less<>>> cppBraceInitializerMacros_;
    // T19-040: the token parameters of each function-like macro, and the per-call wrappers. / Los parametros de
    //     tokens de cada macro de funcion, y los envoltorios por llamada.
    std::map<std::size_t, std::map<std::string, std::vector<unsigned>, std::less<>>> cppFunctionMacroTokens_;
    std::unordered_map<const ast::AstNode*, CppMacroCallWrapper> cppMacroCallWrappers_;
    std::size_t namespaceDiagnosticsBegin_ = 0;
    std::size_t namespaceDiagnosticsEnd_ = 0;
    CppTypeAliasProvider cppTypeAliasProvider_;
    ExperimentalCppTypeProvider experimentalCppTypeProvider_;
    ExperimentalCppTypeSelector experimentalCppTypeSelector_;
    CppPrecompiledParserTypeProvider cppPrecompiledParserTypeProvider_;
    ExperimentalCppInstanceMethodProvider experimentalCppInstanceMethodProvider_;
    ExperimentalCppLifetimeMethodProvider experimentalCppLifetimeMethodProvider_;
    // NSMeta X4-06: (project, session) of each class template a source instantiated. / Plantillas de clase usadas.
    std::set<std::pair<std::size_t, const interop::CppInteropSession*>> cppClassTemplateSessions_;
    ExperimentalCppInstanceFieldProvider experimentalCppInstanceFieldProvider_;
    ExperimentalCppStaticMethodProvider experimentalCppStaticMethodProvider_;
    ExperimentalCppStaticFieldProvider experimentalCppStaticFieldProvider_;
    bool experimentalCppInstanceMethodLoweringForTests_ = false;
    bool experimentalCppInstanceFieldLoweringForTests_ = false;
    bool experimentalCppLifetimeAnalysisForTests_ = false;
    bool experimentalCppLocalLifetimeAnalysisForTests_ = false;
    bool cppNoexceptLocalLifetimeOnly_ = false;
    bool experimentalCppDiscardedLifetimeLoweringForTests_ = false;
    bool experimentalCppImportedTypesConfigured_ = false;
    CppFunctionProvider cppFunctionProvider_;
    CppVariadicCallPlanProvider cppVariadicCallPlanProvider_;
    CppFunctionPointerPlanProvider cppFunctionPointerPlanProvider_;
    CppVariadicFunctionPointerPlanProvider cppVariadicFunctionPointerPlanProvider_;
    CppNamespaceProvider cppNamespaceProvider_;
    CppGlobalProvider cppGlobalProvider_;
    CppRecordObserver cppRecordObserver_;
    CppMetadataCatalogProvider cppMetadataCatalogProvider_;
    bool cppImportedFunctionsConfigured_ = false;
    bool compilationAttempted_ = false;
    std::optional<compilation::ProjectGraph> projectGraph_;
    std::vector<std::shared_ptr<void>> receiverProductGuards_;
    std::string receiverProductIdentity_;
    bool receiverRequiresThinLtoSidecar_ = false;
    std::vector<PrecompiledSyntaxModule> precompiledModules_;
    // Canonical project file -> first project-graph ordinal with that file. Built once per
    // compilation by materializePrecompiledSyntax; read by precompiledProjectOrdinal.
    std::map<std::filesystem::path, std::size_t> precompiledProjectOrdinals_;
    // EN: Unit ordinal of the public API unit of each precompiled module (index of
    //     precompiledModules_), or no value for a module materialized under another entry.
    // ES: Ordinal de la unidad de API pública de cada módulo precompilado (índice de
    //     precompiledModules_), o sin valor para un módulo materializado bajo otra entrada.
    std::vector<std::optional<std::size_t>> precompiledApiUnits_;
    std::vector<PrecompiledSnapshotUnit> precompiledSnapshots_;
    std::string precompiledSnapshotCaptureModule_;
    std::optional<PrecompiledSnapshotUnit> precompiledSnapshotCapture_;
    source::SourceManager sourceManager_;
    diagnostics::DiagnosticEngine diagnostics_;
    std::vector<std::unique_ptr<ProjectSyntaxUnit>> units_;
    std::vector<ProjectComptimeRecord> comptimeRecords_;
    ProjectCompilationLifetime ownedLifetime_;
    ProjectCompilationLifetime* lifetime_ = &ownedLifetime_;
    std::unique_ptr<ProjectNameLookup> nameLookup_;
    std::unique_ptr<ProjectGenericModel> genericModel_;
    std::unique_ptr<ProjectMemberModel> memberModel_;
    std::unique_ptr<ProjectNamespaceFunctionModel> namespaceFunctionModel_;
    std::unique_ptr<ProjectInheritanceModel> inheritanceModel_;
    std::unique_ptr<ProjectInterfaceModel> interfaceModel_;
    // NSC-LIMIT-007. Va entre las interfaces y los cuerpos, que es el unico sitio donde se puede:
    // antes no existe con que responder «este tipo implementa esa interfaz», y despues los cuerpos
    // ya se habrian analizado sobre una especializacion invalida.
    std::unique_ptr<ProjectGenericConstraintModel> genericConstraintModel_;
    std::unique_ptr<ProjectMethodBodyModel> methodBodyModel_;
    std::unique_ptr<ProjectVirtualDispatchModel> virtualDispatchModel_;
    std::unique_ptr<ProjectDestructionModel> destructionModel_;
    std::unique_ptr<ProjectConstructionModel> constructionModel_;
    std::unique_ptr<ProjectObjectLayoutModel> objectLayoutModel_;
    std::unique_ptr<ProjectMetaprogrammingModel> metaprogrammingModel_;
    std::unique_ptr<ProjectOopHirModel> oopHirModel_;
    std::unique_ptr<ProjectOopMirModel> oopMirModel_;
    std::function<bool()> cancellationCheck_;
    SourceBufferProvider sourceBufferProvider_;
    std::function<void(std::string_view, std::chrono::nanoseconds)> phaseTimingSink_;
    std::function<void(const basic::CompilationEvent&)> progressSink_;
    std::function<void(std::string_view)> reportSink_;
    CppRuntimeIsolationResolver cppRuntimeIsolationResolver_;
#if 0 // INSTRUMENTATION CODE
    std::function<void(const basic::CompilationEvent&)> detailedProgressSink_;
#endif // INSTRUMENTATION CODE
    compilation::ProjectLoadOptions projectLoadOptions_;
    std::filesystem::path templateCacheRoot_;
    std::filesystem::path precompiledProductCacheRoot_;
    std::vector<std::string> precompiledSyntaxCacheKeysByUnit_;
    std::vector<bool> precompiledSyntaxCacheHitsByUnit_;
    std::string templateParseCacheKey_;
    bool templateParseCacheHit_ = false;
    // EN: Parse products serialized before the generic pass mutates the trees; indexed like units_.
    // ES: Productos del parser serializados antes de que el pase genérico mute los árboles; mismo
    //     índice que units_.
    std::vector<std::string> pristineSyntaxProductsByUnit_;
    std::size_t compilationJobs_ = 1;
    CompilationStageTracker stageTracker_;
};
}
