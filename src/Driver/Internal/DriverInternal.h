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

bool configurePrecompiledModules(const std::filesystem::path &projectFile, frontend::ProjectCompilation &compilation,
                                 std::string &error);
void configureObjectGeneratorProgress(llvm_backend::ProjectOopObjectGenerator &generator);
void appendExternalImport(std::vector<linker::ExternalImportModule> &modules, const std::string &dllName,
                          const std::string &symbol);
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
                            std::ostream &error);
} // namespace nativesharp::driver
