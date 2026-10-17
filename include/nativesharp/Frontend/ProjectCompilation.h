#pragma once

#include "nativesharp/Basic/CompilationEvent.h"
#include "nativesharp/Compilation/ProjectGraph.h"
#include "nativesharp/Compilation/ProjectLoader.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
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
#include "nativesharp/Source/SourceManager.h"

#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::frontend
{
struct PrecompiledSyntaxModule final
{
    std::filesystem::path ownerProjectFile;
    std::filesystem::path metadataPath;
    std::string moduleName;
    std::string sourceText;
    std::string templateSourceText;
    std::vector<PrecompiledMemberIdentity> members;
    std::vector<PrecompiledNamespaceFunctionIdentity> namespaceFunctions;
    std::vector<PrecompiledGenericSpecialization> genericSpecializations;
};

class ProjectCompilation final
{
public:
    using SourceBufferProvider =
        std::function<std::optional<std::string>(const std::filesystem::path&)>;

    void setCompilationJobs(std::size_t jobs) noexcept;
    void setProjectLoadOptions(compilation::ProjectLoadOptions options);
    void setCancellationCheck(std::function<bool()> check);
    // Called concurrently by syntax workers. Returning nullopt falls back to the file on disk.
    void setSourceBufferProvider(SourceBufferProvider provider);
    void setPhaseTimingSink(
        std::function<void(std::string_view, std::chrono::nanoseconds)> sink);
    void setProgressSink(std::function<void(const basic::CompilationEvent&)> sink);
    // Detailed, high-volume diagnostics are routed separately so normal --progress output keeps
    // its stable concise contract. The driver currently uses this for --generics-trace=<file>.
    void setDetailedProgressSink(std::function<void(const basic::CompilationEvent&)> sink);
    bool setPrecompiledModules(
        std::vector<PrecompiledSyntaxModule> modules,
        std::string& error);
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
    const source::SourceManager& sourceManager() const noexcept;
    diagnostics::DiagnosticEngine& diagnostics() noexcept;
    const diagnostics::DiagnosticEngine& diagnostics() const noexcept;
    std::span<const std::unique_ptr<ProjectSyntaxUnit>> units() const noexcept;
    std::span<const ProjectComptimeRecord> comptimeRecords() const noexcept;
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
    bool compileSyntaxCore(const std::filesystem::path& projectFile, std::string& loadError);
    bool enterStage(CompilationStage stage, std::string& error);

    std::optional<compilation::ProjectGraph> projectGraph_;
    std::vector<PrecompiledSyntaxModule> precompiledModules_;
    source::SourceManager sourceManager_;
    diagnostics::DiagnosticEngine diagnostics_;
    std::vector<std::unique_ptr<ProjectSyntaxUnit>> units_;
    std::vector<ProjectComptimeRecord> comptimeRecords_;
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
    std::function<void(const basic::CompilationEvent&)> detailedProgressSink_;
    compilation::ProjectLoadOptions projectLoadOptions_;
    std::size_t compilationJobs_ = 1;
    CompilationStageTracker stageTracker_;
};
}
