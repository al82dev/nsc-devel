#pragma once

#include "DriverCompilationUnit.h"

namespace nativesharp::driver
{
std::string foldedPathKey(const std::filesystem::path &path);
std::string foldedName(std::string value);
std::string escapedStringLiteral(std::string_view value);
bool sourceTypeForMetadataKey(std::string typeKey, std::string &sourceType, std::string &error);
bool sourceTypeForMetadataKey(const interop::LibraryMetadata &metadata, const std::string &typeKey,
                              std::string &sourceType, std::string &error);
std::string ownershipPrefix(interop::LibraryMetadataOwnershipKind ownership);
std::string ownershipPrefix(interop::LibraryMetadataOwnershipKind ownership, std::string_view lifetime);
std::string lifetimeAnnotation(std::string_view lifetime);
std::string metadataBaseTypeKey(std::string typeKey);
void appendParserKnownTypeNames(const interop::LibraryMetadata &metadata, const std::string &writtenTypeKey,
                                std::vector<std::string> &knownTypeNames);
const interop::LibraryMetadataType *metadataTypeForKey(const interop::LibraryMetadata &metadata,
                                                       const std::string &typeKey);
std::optional<std::string> metadataSpecializedTypeKey(
    const interop::LibraryMetadataGenericSpecialization &specialization);
bool validatePrecompiledExternalTypeClosure(
    std::span<const LoadedPrecompiledMetadata> loaded,
    const std::unordered_map<std::string, std::filesystem::path> &sourceProjects,
    std::string &error);
bool isPubliclyReachableMetadataType(const interop::LibraryMetadata &metadata,
                                     const interop::LibraryMetadataType &type) noexcept;
bool canMaterializePrecompiledDeclaration(const interop::LibraryMetadata &metadata,
                                          const interop::LibraryMetadataMember &member);
std::string_view sourceAccessibility(std::uint8_t accessibility) noexcept;
const interop::LibraryMetadataMember *metadataFieldMember(const interop::LibraryMetadata &metadata,
                                                          const interop::LibraryMetadataType &type,
                                                          std::string_view name);
const interop::LibraryMetadataMember *metadataEventForBacking(const interop::LibraryMetadata &metadata,
                                                              const interop::LibraryMetadataType &type,
                                                              const interop::LibraryMetadataField &field);
std::string enumConstantSource(const interop::LibraryMetadataType &type, std::uint64_t bits);
bool metadataConstantSource(const interop::LibraryMetadataMember &member, std::string &source, std::string &error);
std::string_view metadataOperatorToken(std::string_view name) noexcept;
std::string parameterPassPrefix(interop::LibraryMetadataParameterPassKind passKind, std::string_view lifetime);
bool sourceDefaultValue(const interop::LibraryMetadataParameter &parameter, std::string &source, std::string &error);
std::vector<std::string> publishedNativeSymbols(const interop::LibraryMetadata &metadata);
std::vector<std::string> missingSymbols(std::span<const std::string> expected, std::span<const std::string> actual);
bool buildPrecompiledPublicSource(const interop::LibraryMetadata &metadata, std::string &source, std::string &error);
bool buildPrecompiledGenericTemplateSource(const interop::LibraryMetadata &metadata, std::string &source,
                                           std::string &error);
bool verifyPrecompiledGenericTemplateSource(const interop::LibraryMetadata &metadata, std::string &error);

const compilation::ProjectLoadOptions &activeProjectLoadOptions() noexcept;
compilation::ProjectLoader &activeProjectLoader() noexcept;

template <typename Callback> auto measureDriverPhase(std::string_view phase, Callback callback)
{
    const DriverPhaseTimer timing(phase);
    return callback();
}

void appendCodeUnitTelemetry(std::ostream &output, const llvm_backend::ProjectOopObjectBundle &bundle);
void configureProjectExecution(frontend::ProjectCompilation &compilation);
std::size_t effectiveCompilationJobs() noexcept;

std::string_view subsystemDisplayName(linker::WindowsExecutableSubsystem subsystem);
bool hasConsumerCallableAccessibility(std::uint8_t accessibility) noexcept;
bool hasConsumerCallableAccessibility(frontend::ClassMemberAccessibility accessibility) noexcept;
bool isConsumerCallableNativeMethod(const interop::LibraryMetadataMember &member) noexcept;
bool isPubliclyReachableNamedType(std::span<const frontend::NamedTypeDeclaration> types,
                                  std::size_t typeOrdinal) noexcept;
std::vector<std::string> metadataRequiredSymbols(const interop::LibraryMetadata &metadata);
std::string describeSymbols(std::span<const std::string> symbols);
void printMetadataAbiContract(const interop::LibraryMetadata &metadata, std::ostream &output);
bool verifyPrecompiledPublicSource(const interop::LibraryMetadata &metadata, std::string &error);
bool writeVerifiedLibraryMetadata(const frontend::ProjectCompilation &compilation,
                                  const std::filesystem::path &destination, std::string &error,
                                  interop::LibraryMetadata *writtenModel = nullptr);
// EN: Plan items 2.1-2.4. After the verified NSMETA and its `.nsproducts` are written, compiles an
//     empty consumer of the library in memory and publishes the semantic snapshot of its public
//     API unit. Never fails the build: without a snapshot consumers take today's path.
// ES: Puntos 2.1 a 2.4 del plan. Después de escribir la NSMETA verificada y su `.nsproducts`,
//     compila en memoria un consumidor vacío de la biblioteca y publica el snapshot semántico de su
//     unidad de API pública. Nunca hace fallar el build: sin snapshot los consumidores van por la
//     ruta de hoy.
void publishPrecompiledSnapshot(const frontend::ProjectCompilation &compilation,
                                const std::filesystem::path &metadataPath);
bool buildPrecompiledSnapshotReference(const frontend::ProjectCompilation &producer,
                                       const std::filesystem::path &metadataPath,
                                       frontend::ProjectCompilation &reference, std::string &error);

std::optional<compilation::CompilationInputManifest> loadCompilationInputManifest(
    const std::filesystem::path &projectFile, std::string &error);
void addCompilationProductIdentity(compilation::CompilationInputManifest &manifest,
                                   std::string_view productKind,
                                   const std::filesystem::path &outputPath = {});
void addCompilationCodegenIdentity(
    compilation::CompilationInputManifest &manifest,
    llvm_backend::LlvmOptimizationLevel optimizationLevel,
    llvm_backend::DebugInformationKind debugInformation,
    std::span<const llvm_backend::DebugPrefixMap> debugPrefixMaps,
    llvm_backend::LinkTimeOptimizationMode linkTimeOptimization);
void addCompilationLinkIdentity(
    compilation::CompilationInputManifest &manifest, bool incrementalLink,
    linker::WindowsExecutableSubsystem subsystem,
    std::span<const std::filesystem::path> extraLinkLibraries);
// `materializeDemandedSurface` is true for code generation: each precompiled module outside the
// standard library then materializes only the declarations its sources can reach. Analysis dumps
// and checks pass false and keep the complete surfaces.
bool configurePrecompiledModules(compilation::CompilationInputManifest &manifest,
                                 frontend::ProjectCompilation &compilation, std::string &error,
                                 bool materializeDemandedSurface = false);
bool restoreCompilationBundle(
    const compilation::CompilationInputManifest &manifest,
    bool &restored,
    std::string &error);
void publishCompilationBundle(
    const compilation::CompilationInputManifest &manifest) noexcept;
void configureObjectGeneratorProgress(llvm_backend::ProjectOopObjectGenerator &generator);
void appendExternalImport(std::vector<linker::ExternalImportModule> &modules, const std::string &dllName,
                          const std::string &symbol);
// Appends to a list of external import modules with the result of appendExternalImport (one
// module per DLL, matched with foldedName; symbols in first-appearance order without duplicates),
// using indexes that live while the list is built. Modules already in the list are indexed
// first. The list must not change by other means while the builder appends to it.
class ExternalImportListBuilder final
{
public:
    explicit ExternalImportListBuilder(std::vector<linker::ExternalImportModule> &modules);
    void append(const std::string &dllName, const std::string &symbol);

private:
    std::vector<linker::ExternalImportModule> &modules_;
    std::unordered_map<std::string, std::size_t> moduleByFoldedName_;
    // Symbols of each module, by module position, for the duplicate check.
    std::vector<std::unordered_set<std::string>> symbolsByModule_;
};
bool appendValidatedLibraryReferences(const frontend::ProjectCompilation &compilation,
                                      std::vector<std::filesystem::path> &libraries,
                                      std::vector<std::filesystem::path> &runtimeLibraries, std::string &error,
                                      std::vector<linker::ExternalImportModule> *inheritedImports = nullptr);
bool verifyLinkedRuntimeLibraries(std::span<const std::filesystem::path> runtimeLibraries,
                                  const linker::PeExecutableInfo &image, std::string &error);
std::filesystem::path pathFromUtf8(std::string_view value);
bool packageRuntimeClosure(std::span<const std::filesystem::path> runtimeLibraries,
                           const std::filesystem::path &destination, std::vector<std::filesystem::path> &copiedFiles,
                           std::string &error);
void removePackagedFiles(std::span<const std::filesystem::path> copiedFiles);

ExitCode runCompilerSession(const Driver &driver, std::span<const std::string_view> arguments, std::ostream &output,
                            std::ostream &error,
                            compilation::CompilationInputManifest *preparedInputManifest);
} // namespace nativesharp::driver
