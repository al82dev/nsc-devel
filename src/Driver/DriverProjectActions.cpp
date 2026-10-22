#include "Internal/DriverInternal.h"

namespace nativesharp::driver
{
using target::BuiltinLayoutKind;

std::optional<ExitCode> runBasicProjectAction(std::string_view action, std::string_view path, std::ostream &output,
                                              std::ostream &error)
{
    if (action == "--check-project" || action == "--dump-project")
    {
        std::string loadError;
        const std::optional<compilation::Project> project =
            activeProjectLoader().load(std::filesystem::path(path), activeProjectLoadOptions(), loadError);
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
    if (action != "--check-project-graph" && action != "--dump-project-graph")
        return std::nullopt;

    const CompilationJobPolicy *const jobs = activeCompilationJobPolicy();
    compilation::ProjectLoader &projectLoader = activeProjectLoader();
    const compilation::ProjectLoadOptions loadOptions = activeProjectLoadOptions();
    compilation::ProjectGraphLoader loader(
        jobs != nullptr ? jobs->effectiveJobs() : 1,
        [&projectLoader, loadOptions](const std::filesystem::path &projectPath, std::string &loadError)
        { return projectLoader.load(projectPath, loadOptions, loadError); });
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

bool compileProjectAction(std::string_view action, std::string_view path,
                          frontend::ProjectCompilation &projectCompilation, std::string &loadError, bool &oopLlvmAction,
                          bool &oopHirAction)
{
    oopLlvmAction = action == "--verify-project-oop-llvm" || action == "--dump-project-oop-llvm";
    const bool oopMirAction = oopLlvmAction || action == "--check-project-oop-mir" ||
                              action == "--dump-project-oop-mir" || action == "--explain-project-costs" ||
                              action == "--explain-project-costs-json";
    oopHirAction = oopMirAction || action == "--check-project-oop-hir" || action == "--dump-project-oop-hir";
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
    const std::filesystem::path projectPath(path);
    return oopMirAction              ? projectCompilation.compileProjectOopMir(projectPath, loadError)
           : oopHirAction            ? projectCompilation.compileProjectOopHir(projectPath, loadError)
           : layoutAction            ? projectCompilation.compileProjectObjectLayouts(projectPath, loadError)
           : constructionAction      ? projectCompilation.compileProjectConstructions(projectPath, loadError)
           : destructionAction       ? projectCompilation.compileProjectDestruction(projectPath, loadError)
           : virtualDispatchAction   ? projectCompilation.compileProjectVirtualDispatch(projectPath, loadError)
           : methodBodyAction        ? projectCompilation.compileProjectMethodBodies(projectPath, loadError)
           : genericConstraintAction ? projectCompilation.compileProjectGenericConstraints(projectPath, loadError)
           : interfaceAction         ? projectCompilation.compileProjectInterfaces(projectPath, loadError)
           : inheritanceAction       ? projectCompilation.compileProjectInheritance(projectPath, loadError)
           : memberBindingAction     ? projectCompilation.compileProjectMembers(projectPath, loadError)
           : nameBindingAction       ? projectCompilation.compileProjectNames(projectPath, loadError)
           : genericAction           ? projectCompilation.compileProjectGenerics(projectPath, loadError)
                                     : projectCompilation.compileProjectSyntax(projectPath, loadError);
}

void dumpProjectFrontendModels(std::string_view action, frontend::ProjectCompilation &projectCompilation,
                               std::ostream &output)
{
    if (action == "--dump-project-compilation")
    {
        frontend::ProjectCompilationDumper dumper;
        dumper.dump(projectCompilation, output);
    }

    if (action == "--dump-project-comptime")
    {
        frontend::ProjectCompilationDumper dumper;
        dumper.dumpComptime(projectCompilation, output);
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
}

void dumpProjectSemanticModels(std::string_view action, frontend::ProjectCompilation &projectCompilation,
                               std::ostream &output)
{
    if (action == "--dump-project-virtual-dispatch" && projectCompilation.nameLookup() != nullptr &&
        projectCompilation.memberModel() != nullptr && projectCompilation.virtualDispatchModel() != nullptr)
    {
        frontend::ProjectVirtualDispatchModelDumper dumper;
        dumper.dump(*projectCompilation.virtualDispatchModel(), *projectCompilation.memberModel(),
                    *projectCompilation.nameLookup(), output);
    }

    if (action == "--dump-project-method-bodies" && projectCompilation.nameLookup() != nullptr &&
        projectCompilation.memberModel() != nullptr && projectCompilation.methodBodyModel() != nullptr)
    {
        frontend::ProjectMethodBodyModelDumper dumper;
        dumper.dump(*projectCompilation.methodBodyModel(), *projectCompilation.memberModel(),
                    *projectCompilation.nameLookup(), output);
    }

    if (action == "--dump-project-destruction" && projectCompilation.nameLookup() != nullptr &&
        projectCompilation.memberModel() != nullptr && projectCompilation.destructionModel() != nullptr)
    {
        frontend::ProjectDestructionModelDumper::dump(*projectCompilation.destructionModel(),
                                                      *projectCompilation.nameLookup(),
                                                      *projectCompilation.memberModel(), output);
    }

    if (action == "--dump-project-constructions" && projectCompilation.nameLookup() != nullptr &&
        projectCompilation.memberModel() != nullptr && projectCompilation.constructionModel() != nullptr)
    {
        frontend::ProjectConstructionModelDumper dumper;
        dumper.dump(*projectCompilation.constructionModel(), *projectCompilation.memberModel(),
                    *projectCompilation.nameLookup(), output);
    }

    if (action == "--dump-project-layouts" && projectCompilation.nameLookup() != nullptr &&
        projectCompilation.memberModel() != nullptr && projectCompilation.objectLayoutModel() != nullptr)
    {
        frontend::ProjectObjectLayoutModelDumper dumper;
        dumper.dump(*projectCompilation.objectLayoutModel(), *projectCompilation.memberModel(),
                    *projectCompilation.nameLookup(), output);
    }

    if (action == "--dump-project-oop-hir" && projectCompilation.oopHirModel() != nullptr)
    {
        output << frontend::ProjectOopHirModelDumper::dump(*projectCompilation.oopHirModel());
    }

    if (action == "--dump-project-oop-mir" && projectCompilation.oopMirModel() != nullptr)
    {
        output << frontend::ProjectOopMirModelDumper::dump(*projectCompilation.oopMirModel());
    }

    if ((action == "--explain-project-costs" || action == "--explain-project-costs-json") &&
        projectCompilation.oopMirModel() != nullptr)
    {
        const frontend::ProjectCostReport costs =
            frontend::exploreProjectCosts(projectCompilation.oopMirModel()->functions());
        output << (action == "--explain-project-costs-json" ? frontend::projectCostReportJson(costs)
                                                            : frontend::projectCostReportText(costs));
    }
}

bool lowerProjectLlvm(std::string_view action, bool oopLlvmAction, bool success,
                      frontend::ProjectCompilation &projectCompilation,
                      std::unique_ptr<llvm_backend::LlvmModule> &oopLlvmModule, std::ostream &output,
                      std::ostream &error)
{
    if (!oopLlvmAction || !success || projectCompilation.nameLookup() == nullptr ||
        projectCompilation.memberModel() == nullptr || projectCompilation.objectLayoutModel() == nullptr ||
        projectCompilation.oopMirModel() == nullptr)
        return true;
    llvm_backend::ProjectOopLlvmLowerer lowerer;
    oopLlvmModule =
        measureDriverPhase("llvm-lowering",
                           [&]
                           {
                               return lowerer.lower(llvm_backend::ProjectOopLlvmLoweringRequest::withoutDebug(
                                   *projectCompilation.nameLookup(), *projectCompilation.oopMirModel()));
                           });
    if (oopLlvmModule == nullptr)
    {
        error << "nsc: error: " << lowerer.error() << '\n';
        return false;
    }
    if (action == "--dump-project-oop-llvm")
        output << oopLlvmModule->text();
    return true;
}

bool validateProjectCompilationResult(bool success, bool oopHirAction, frontend::ProjectCompilation &projectCompilation,
                                      std::ostream &error)
{
    if (projectCompilation.diagnostics().hasErrors())
    {
        diagnostics::DiagnosticRenderer renderer(projectCompilation.sourceManager());
        renderer.renderAll(projectCompilation.diagnostics(), error);
    }
    if (!success)
        return false;

    // HIR is the first consumer-facing checkpoint at which every referenced
    // precompiled public contract has been materialized.  Validate the same
    // NSMETA/archive/runtime triplets used by emission here, without linking.
    // With no MIR the helper deliberately stops after artifact validation;
    // reachability-driven runtime packaging remains an emission concern.
    if (!oopHirAction)
        return true;
    std::vector<std::filesystem::path> referencedLibraries;
    std::vector<std::filesystem::path> referencedRuntimes;
    std::string referenceError;
    if (!appendValidatedLibraryReferences(projectCompilation, referencedLibraries, referencedRuntimes, referenceError))
    {
        error << "nsc: error: precompiled library contract validation failed: " << referenceError << '\n';
        return false;
    }
    return true;
}

void printProjectCheckPart1(std::string_view action, frontend::ProjectCompilation &projectCompilation,
                            std::ostream &output)
{
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
}

void printProjectCheckPart2(std::string_view action, frontend::ProjectCompilation &projectCompilation,
                            std::ostream &output)
{
    if (action == "--check-project-virtual-dispatch" && projectCompilation.virtualDispatchModel() != nullptr)
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
}

void printProjectCheckPart3(std::string_view action, frontend::ProjectCompilation &projectCompilation,
                            const std::unique_ptr<llvm_backend::LlvmModule> &oopLlvmModule, std::ostream &output)
{
    if (action == "--check-project-destruction" && projectCompilation.destructionModel() != nullptr)
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
}

ExitCode Driver::runProject(std::string_view action, std::string_view path, std::ostream &output, std::ostream &error)
{
    if (const std::optional<ExitCode> basic = runBasicProjectAction(action, path, output, error))
        return *basic;

    std::string loadError;
    std::optional<compilation::CompilationInputManifest> manifest =
        loadCompilationInputManifest(std::filesystem::path(path), loadError);
    if (!manifest.has_value())
    {
        error << "nsc: error: " << loadError << '\n';
        return ExitCode::CompilationError;
    }
    frontend::ProjectCompilation projectCompilation;
    configureProjectExecution(projectCompilation);
    if (!measureDriverPhase(
            "precompiled-metadata",
            [&] { return configurePrecompiledModules(*manifest, projectCompilation, loadError); }))
    {
        error << "nsc: error: " << loadError << '\n';
        return ExitCode::CompilationError;
    }
    bool oopLlvmAction = false;
    bool oopHirAction = false;
    const bool success = compileProjectAction(action, path, projectCompilation, loadError, oopLlvmAction, oopHirAction);
    if (!loadError.empty())
    {
        error << "nsc: error: " << loadError << '\n';
        return ExitCode::CompilationError;
    }

    dumpProjectFrontendModels(action, projectCompilation, output);
    dumpProjectSemanticModels(action, projectCompilation, output);

    std::unique_ptr<llvm_backend::LlvmModule> oopLlvmModule;
    if (!lowerProjectLlvm(action, oopLlvmAction, success, projectCompilation, oopLlvmModule, output, error) ||
        !validateProjectCompilationResult(success, oopHirAction, projectCompilation, error))
        return ExitCode::CompilationError;

    printProjectCheckPart1(action, projectCompilation, output);
    printProjectCheckPart2(action, projectCompilation, output);
    printProjectCheckPart3(action, projectCompilation, oopLlvmModule, output);
    return ExitCode::Success;
}

bool prepareOopCodeCompilation(std::string_view inputPath,
                               compilation::CompilationInputManifest &manifest,
                               frontend::ProjectCompilation &compilation,
                               std::string &loadError, std::ostream &error)
{
    configureProjectExecution(compilation);
    if (!measureDriverPhase(
            "precompiled-metadata",
            [&] { return configurePrecompiledModules(manifest, compilation, loadError); }))
    {
        error << "nsc: error: " << loadError << '\n';
        return false;
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
        return false;
    }
    if (compilation.nameLookup() == nullptr || compilation.memberModel() == nullptr ||
        compilation.objectLayoutModel() == nullptr || compilation.oopMirModel() == nullptr)
    {
        error << "nsc: error: Stage 44 OOP project compilation did not produce "
                 "required lowering models\n";
        return false;
    }
    return true;
}

bool lowerOopCodeModule(frontend::ProjectCompilation &compilation,
                        llvm_backend::LlvmOptimizationLevel optimizationLevel,
                        llvm_backend::DebugInformationKind debugInformation,
                        const std::vector<llvm_backend::DebugPrefixMap> &debugPrefixMaps,
                        std::unique_ptr<llvm_backend::LlvmModule> &module,
                        std::unique_ptr<llvm_backend::LlvmTargetMachine> &targetMachine, std::ostream &error)
{
    llvm_backend::ProjectOopLlvmLowerer lowerer;
    module =
        measureDriverPhase("llvm-lowering",
                           [&]
                           {
                               return lowerer.lower(llvm_backend::ProjectOopLlvmLoweringRequest::withDebug(
                                   *compilation.nameLookup(), *compilation.memberModel(), *compilation.oopMirModel(),
                                   compilation.sourceManager(), debugInformation, optimizationLevel, debugPrefixMaps));
                           });
    if (module == nullptr)
    {
        error << "nsc: error: " << lowerer.error() << '\n';
        return false;
    }

    std::string targetError;
    const bool targetConfigured = measureDriverPhase(
        "target-configuration",
        [&]
        {
            targetMachine = llvm_backend::LlvmTargetMachine::createWindowsX64(optimizationLevel, targetError);
            return targetMachine != nullptr && targetMachine->configureModuleForTarget(*module, targetError);
        });
    if (!targetConfigured)
    {
        error << "nsc: error: " << targetError << '\n';
        return false;
    }

    // NSC-LIMIT-005: observada, como las demas llamadas al optimizador. Ver
    // LlvmOptimizer.h.
    llvm_backend::LlvmOptimizer optimizer;
    llvm_backend::LlvmOptimizationObservation observation;
    if (!measureDriverPhase(
            "llvm-optimization",
            [&] { return optimizer.optimize(*module, *targetMachine, optimizationLevel, &observation, targetError); }))
    {
        error << "nsc: error: " << targetError << '\n';
        return false;
    }
    return true;
}

bool publishOopCodeObject(std::string_view outputPath, frontend::ProjectCompilation &compilation,
                          llvm_backend::LlvmModule &module, llvm_backend::LlvmTargetMachine &targetMachine,
                          llvm_backend::LlvmOptimizationLevel optimizationLevel,
                          llvm_backend::DebugInformationKind debugInformation, std::string &loadError,
                          std::ostream &output, std::ostream &error)
{
    std::string targetError;
    const std::filesystem::path destination = pathFromUtf8(outputPath);
    ArtifactPublisher artifactPublisher;
    ObjectPublication publication;
    if (!artifactPublisher.prepareObject(destination, publication, targetError))
    {
        error << "nsc: error: output preflight failed: " << targetError << '\n';
        return false;
    }
    if (!measureDriverPhase("object-emission",
                            [&]
                            {
                                return targetMachine.emit(module, publication.stage,
                                                          llvm_backend::LlvmCodeGenFileType::Object, targetError);
                            }))
    {
        error << "nsc: error: " << targetError << '\n';
        return false;
    }
    llvm_backend::CoffObjectVerifier verifier;
    if (!measureDriverPhase("coff-verification", [&] { return verifier.verify(publication.stage); }))
    {
        error << "nsc: error: Stage 44 emitted invalid COFF object: " << verifier.error() << '\n';
        return false;
    }
    if (!compilation.completeCodeGeneration(loadError))
    {
        error << "nsc: error: " << loadError << '\n';
        return false;
    }
    if (!artifactPublisher.commit(targetError))
    {
        error << "nsc: error: object publication failed: " << targetError << '\n';
        return false;
    }
    output << "project OOP object emission: ok (machine=0x" << std::hex << verifier.info().machine << std::dec
           << ", sections=" << verifier.info().sectionCount << ")\n"
           << "optimization: " << llvm_backend::llvmOptimizationLevelName(optimizationLevel) << '\n'
           << "debug-information: " << llvm_backend::debugInformationKindName(debugInformation) << '\n';
    return true;
}

ExitCode Driver::runProjectOopCodeGeneration(std::string_view inputPath, std::string_view outputPath,
                                             llvm_backend::LlvmOptimizationLevel optimizationLevel,
                                             llvm_backend::DebugInformationKind debugInformation,
                                             const std::vector<llvm_backend::DebugPrefixMap> &debugPrefixMaps,
                                             std::ostream &output, std::ostream &error)
{
    std::string loadError;
    std::optional<compilation::CompilationInputManifest> manifest =
        loadCompilationInputManifest(std::filesystem::path(inputPath), loadError);
    if (!manifest.has_value())
    {
        error << "nsc: error: " << loadError << '\n';
        return ExitCode::CompilationError;
    }
    frontend::ProjectCompilation compilation;
    if (!prepareOopCodeCompilation(inputPath, *manifest, compilation, loadError, error))
        return ExitCode::CompilationError;

    std::unique_ptr<llvm_backend::LlvmModule> module;
    std::unique_ptr<llvm_backend::LlvmTargetMachine> targetMachine;
    if (!lowerOopCodeModule(compilation, optimizationLevel, debugInformation, debugPrefixMaps, module, targetMachine,
                            error))
        return ExitCode::CompilationError;

    if (!publishOopCodeObject(outputPath, compilation, *module, *targetMachine, optimizationLevel, debugInformation,
                              loadError, output, error))
        return ExitCode::CompilationError;
    return ExitCode::Success;
}
} // namespace nativesharp::driver
