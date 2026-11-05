#include "nativesharp/Interop/CppClangSessionFactory.h"
#include "nativesharp/Basic/ArtifactTransaction.h"
#include "nativesharp/Parser/ScopedKnownTypeNames.h"
#include "nativesharp/Parser/LexicalNamespaceScopes.h"
#include "nativesharp/Frontend/QualifiedNamespaceTarget.h"
#include "nativesharp/Frontend/LexicalNamespaceEnvironment.h"
#include "nativesharp/Frontend/CppScopedTypeNames.h"
#include "nativesharp/Frontend/CppParserProjectInventory.h"
#include "nativesharp/Frontend/ProjectSyntaxUnit.h"
#include "nativesharp/Source/SourceFile.h"
#include "nativesharp/Interop/CppProjectSessions.h"
#include "nativesharp/Interop/CppBinaryDependencies.h"
#include "nativesharp/Interop/LibraryMetadataCodec.h"
#include "nativesharp/Interop/CppScalarTypeMapping.h"
#include "nativesharp/Interop/CppSemanticFunction.h"
#include "nativesharp/Interop/CppSemanticRecord.h"
#include "nativesharp/Interop/CppSemanticInventory.h"
#include "nativesharp/Interop/CppSemanticFunctionGroups.h"
#include "nativesharp/Interop/CppNamespaceInventory.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectConstructionModel.h"
#include "nativesharp/Frontend/ProjectObjectLayoutModel.h"
#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Frontend/ProjectOopHirNode.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/Frontend/ProjectOopMirNode.h"
#include "nativesharp/Frontend/ObjectConstructionBinding.h"
#include "nativesharp/Frontend/ClassDestructionPlan.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/CppLifetimeObjectEmitter.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"
#include "nativesharp/LLVM/ProjectOopObjectGenerator.h"
#include "nativesharp/LLVM/ProjectOopObjectBundle.h"
#include "nativesharp/Linker/WindowsExecutableBuilder.h"
#include "nativesharp/Linker/WindowsLibraryBuilder.h"
#include "nativesharp/Compilation/ProjectLoader.h"
#include "nativesharp/Compilation/ProjectGraphLoader.h"
#include "nativesharp/Compilation/PrecompiledProjectPreparation.h"
#include "nativesharp/Compilation/PrecompiledProductSidecarCodec.h"
#include "nativesharp/Compilation/CompilationInputManifest.h"
#include "../../src/Driver/Internal/DriverExecutionScope.h"
#include "../../src/Driver/Internal/DriverInternal.h"
#include "../../src/Compilation/PrecompiledSurfaceReachability.h"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_set>
#include <variant>

using namespace nativesharp::interop;

namespace
{
class RejectUnexpectedOpen final : public CppInteropSessionFactory
{
public:
    CppSessionOpenResult open(CppSessionOptions, CppCancellationToken) override
    {
        ++calls;
        return CppSessionOpenFailure{"test", "unexpected C/C++ session", {}};
    }
    unsigned calls = 0;
};

class FailThirdOpen final : public CppInteropSessionFactory
{
public:
    explicit FailThirdOpen(CppInteropSessionFactory& delegate) : delegate_(delegate) {}
    CppSessionOpenResult open(CppSessionOptions options, CppCancellationToken cancellation) override
    {
        if (++calls_ == 3) return CppSessionOpenFailure{"test", "controlled failure", {}};
        return delegate_.open(std::move(options), cancellation);
    }
private:
    CppInteropSessionFactory& delegate_;
    unsigned calls_ = 0;
};

class NoImplicitSession final : public CppInteropSession
{
public:
    explicit NoImplicitSession(std::unique_ptr<CppInteropSession> inner) : inner_(std::move(inner)) {}
    CppSessionState state() const noexcept override { return inner_->state(); }
    CppSessionOptions optionsSnapshot() const override { return inner_->optionsSnapshot(); }
    std::vector<CppSessionDiagnostic> diagnosticsSnapshot() const override { return inner_->diagnosticsSnapshot(); }
    std::optional<std::size_t> topLevelDeclarationCount() const override { return inner_->topLevelDeclarationCount(); }
    std::optional<std::vector<CppFunctionDeclaration>> freeFunctionsSnapshot() const override { return inner_->freeFunctionsSnapshot(); }
    std::optional<std::vector<CppRecordDeclaration>> recordsSnapshot() const override { return inner_->recordsSnapshot(); }
    std::optional<std::vector<CppGlobalDeclaration>> globalsSnapshot() const override { return inner_->globalsSnapshot(); }
    std::optional<std::vector<CppNamespaceDeclaration>> namespacesSnapshot() const override { return inner_->namespacesSnapshot(); }
    std::optional<std::vector<std::filesystem::path>> sourceFilesSnapshot() const override { return inner_->sourceFilesSnapshot(); }
    std::optional<std::vector<CppSourceFileContent>> sourceFileContentsSnapshot() const override { return inner_->sourceFileContentsSnapshot(); }
    CppSessionCloseResult close() override { return inner_->close(); }
private:
    std::unique_ptr<CppInteropSession> inner_;
};

CppSessionOptions options(const char* resource, const char* working, const char* driver,
                          CppLanguageMode mode, std::string source)
{
    CppSessionOptions result;
    result.language = mode;
    result.languageStandard = mode == CppLanguageMode::C ? "c11" : "c++20";
    result.targetTriple = "x86_64-pc-windows-msvc";
    result.driverReferencePath = driver;
    result.resourceDirectory = resource;
    result.workingDirectory = working;
    result.input = CppBufferInput{mode == CppLanguageMode::C ? "probe.c" : "probe.cpp", std::move(source)};
    return result;
}

bool expect(bool condition, const char* message)
{
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}

const CppFunctionDeclaration* functionByName(const std::vector<CppFunctionDeclaration>& values,
                                              const std::string& name)
{
    const auto it = std::find_if(values.begin(), values.end(), [&](const auto& value) {
        return value.qualifiedName == name;
    });
    return it == values.end() ? nullptr : &*it;
}

const CppRecordDeclaration* recordByName(const std::vector<CppRecordDeclaration>& values,
                                         const std::string& name)
{
    const auto it = std::find_if(values.begin(), values.end(), [&](const auto& value) {
        return value.qualifiedName == name;
    });
    return it == values.end() ? nullptr : &*it;
}

const CppGlobalDeclaration* globalByName(const std::vector<CppGlobalDeclaration>& values,
                                         const std::string& name)
{
    const auto it = std::find_if(values.begin(), values.end(), [&](const auto& value) {
        return value.qualifiedName == name;
    });
    return it == values.end() ? nullptr : &*it;
}

std::unique_ptr<CppInteropSession> open(CppClangSessionFactory& factory, CppSessionOptions opts)
{
    auto result = factory.open(std::move(opts), {});
    if (auto* failure = std::get_if<CppSessionOpenFailure>(&result))
    {
        std::cerr << "Open failed in " << failure->phase << ": " << failure->message << '\n';
        for (const auto& diagnostic : failure->diagnostics)
            std::cerr << diagnostic.message << '\n';
        return {};
    }
    return std::move(std::get<std::unique_ptr<CppInteropSession>>(result));
}
}

int main(int argc, char** argv)
{
    if (argc != 4 && argc != 5) return 2;
    CppClangSessionFactory factory;
    bool ok = true;
    {
        CppSessionOptions base;
        base.language = CppLanguageMode::Cpp;
        base.languageStandard = "c++20";
        base.targetTriple = "x86_64-pc-windows-msvc";
        base.driverReferencePath = "A:/tools/clang.exe";
        base.resourceDirectory = "A:/tools/resource";
        base.workingDirectory = "A:/work";
        base.includePaths = {{"A:/include/one", CppIncludeKind::User},
                             {"A:/include/two", CppIncludeKind::System}};
        base.macros = {{CppMacroAction::Define, "A=1"}, {CppMacroAction::Undefine, "B"}};
        base.input = CppBufferInput{"A:/work/main.cpp", "int value;"};
        const auto identity = cppSessionConfigurationIdentity(base);
        auto changed = [&](auto mutate) {
            auto copy = base;
            mutate(copy);
            return cppSessionConfigurationIdentity(copy) != identity;
        };
        ok &= expect(identity.size() == 64 && identity == cppSessionConfigurationIdentity(base) &&
            changed([](auto& o) { o.language = CppLanguageMode::C; }) &&
            changed([](auto& o) { o.languageStandard = "c++23"; }) &&
            changed([](auto& o) { o.targetTriple = "other"; }) &&
            changed([](auto& o) { o.driverReferencePath = "A:/other/clang.exe"; }) &&
            changed([](auto& o) { o.resourceDirectory = "A:/other/resource"; }) &&
            changed([](auto& o) { o.workingDirectory = "A:/other/work"; }) &&
            changed([](auto& o) { o.includePaths[0].path = "A:/other/include"; }) &&
            changed([](auto& o) { o.includePaths[0].path = "A:/include/dir/../one"; }) &&
            changed([](auto& o) { o.includePaths[0].kind = CppIncludeKind::System; }) &&
            changed([](auto& o) { std::swap(o.includePaths[0], o.includePaths[1]); }) &&
            changed([](auto& o) { o.macros[0].action = CppMacroAction::Undefine; }) &&
            changed([](auto& o) { o.macros[0].text = "A=2"; }) &&
            changed([](auto& o) { std::swap(o.macros[0], o.macros[1]); }) &&
            changed([](auto& o) { std::get<CppBufferInput>(o.input).virtualPath = "A:/work/other.cpp"; }) &&
            changed([](auto& o) { std::get<CppBufferInput>(o.input).contents = "int other;"; }) &&
            changed([](auto& o) { o.input = CppFileInput{"A:/work/main.cpp"}; }),
            "session configuration identity is stable and distinguishes every option and order");
        auto file = base;
        file.input = CppFileInput{"A:/work/main.cpp"};
        auto otherFile = file;
        otherFile.input = CppFileInput{"A:/work/other.cpp"};
        ok &= expect(cppSessionConfigurationIdentity(file) != cppSessionConfigurationIdentity(otherFile),
                     "session configuration identity distinguishes file input paths");
    }
    {
        namespace fs = std::filesystem;
        const auto root = fs::absolute(fs::path(argv[2]) / "cpp-fixtures" / "no-replace-transaction");
        std::error_code ec;
        fs::create_directories(root, ec);
        const auto primary = root / "primary.txt";
        const auto protectedPath = root / "protected.dll";
        const auto stagePrimary = root / "primary.stage";
        const auto stageProtected = root / "protected.stage";
        { std::ofstream file(primary, std::ios::binary); file << "original"; }
        { std::ofstream file(stagePrimary, std::ios::binary); file << "replacement"; }
        { std::ofstream file(stageProtected, std::ios::binary); file << "new-runtime"; }
        fs::remove(protectedPath, ec);
        nativesharp::basic::ArtifactTransaction transaction;
        std::string error;
        const bool staged = !ec && transaction.addStaged(primary, stagePrimary, error) &&
            transaction.addStagedNoReplace(protectedPath, stageProtected, error);
        { std::ofstream file(protectedPath, std::ios::binary); file << "concurrent"; }
        const bool rejected = staged && !transaction.commit(error);
        const auto read = [](const fs::path& path) {
            std::ifstream file(path, std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        };
        ok &= expect(rejected && error.find("rollback-complete") != std::string::npos &&
                         read(primary) == "original" && read(protectedPath) == "concurrent",
                     "no-replace transaction rejects late destination and rolls back earlier artifact");
        fs::remove(protectedPath, ec);
        nativesharp::basic::ArtifactTransaction freeTransaction;
        const bool installed = !ec && freeTransaction.addStagedNoReplace(protectedPath, stageProtected, error) &&
            freeTransaction.commit(error);
        ok &= expect(installed && read(protectedPath) == "new-runtime",
                     "no-replace transaction installs when destination remains absent");
    }
    {
        using namespace nativesharp::interop;
        auto model = std::make_shared<LibraryMetadata>();
        LibraryMetadataType keep;
        keep.identityKey = "Example::Keep";
        keep.fullyQualifiedName = "Keep";
        LibraryMetadataType drop;
        drop.identityKey = "Example::Drop";
        drop.fullyQualifiedName = "Drop";
        model->types = {keep, drop};
        LibraryMetadataCppBinaryDependency first;
        first.libraryFileName = "one.lib";
        LibraryMetadataCppBinaryDependency second;
        second.libraryFileName = "two.lib";
        model->cppBinaryDependencies = {first, second, first};
        std::vector<std::shared_ptr<const LibraryMetadata>> modules = {model};
        nativesharp::compilation::PrecompiledSurfaceReachability reachability(modules, {false});
        reachability.reach({"Keep"});
        const auto reduced = reachability.prunedMetadata(0);
        ok &= expect(reduced != model && reduced->types.size() == 1 &&
                         reduced->types[0].identityKey == keep.identityKey &&
                         reduced->cppBinaryDependencies == model->cppBinaryDependencies,
                     "precompiled surface pruning retains C++ binary dependency order and repetitions");
        nativesharp::compilation::PrecompiledSurfaceReachability full(modules, {true});
        ok &= expect(full.prunedMetadata(0) == model &&
                         full.prunedMetadata(0)->cppBinaryDependencies == model->cppBinaryDependencies,
                     "full precompiled surface keeps the original C++ binary dependencies");
    }
    {
        namespace fs = std::filesystem;
        const fs::path root = fs::absolute(fs::path(argv[2]) / "cpp-fixtures" / "source-files");
        std::error_code ec;
        fs::create_directories(root, ec);
        const auto mainHeader = root / "root.h";
        const auto nestedHeader = root / "nested.h";
        const auto skippedHeader = root / "skipped.h";
        { std::ofstream file(mainHeader); file << "#include \"nested.h\"\n#if 0\n#include \"skipped.h\"\n#endif\n"; }
        { std::ofstream file(nestedHeader, std::ios::binary); file << "struct NestedDependency {};\n"; }
        { std::ofstream file(skippedHeader); file << "struct SkippedDependency {};\n"; }
        auto config = options(argv[1], argv[2], argv[3], CppLanguageMode::Cpp, "");
        config.input = CppFileInput{mainHeader};
        auto session = open(factory, std::move(config));
        ok &= expect(!ec && static_cast<bool>(session), "file-dependency fixture opens");
        if (session)
        {
            auto files = session->sourceFilesSnapshot();
            auto contents = session->sourceFileContentsSnapshot();
            const auto contains = [&](const fs::path& path) {
                return files && std::find(files->begin(), files->end(), path.lexically_normal()) != files->end();
            };
            ok &= expect(files && contains(mainHeader) && contains(nestedHeader) && !contains(skippedHeader) &&
                             std::is_sorted(files->begin(), files->end()) &&
                             std::adjacent_find(files->begin(), files->end()) == files->end(),
                         "Clang source-file snapshot includes observed transitive headers only once");
            const auto expectedNested = nativesharp::basic::sha256(std::span<const std::uint8_t>(
                reinterpret_cast<const std::uint8_t*>("struct NestedDependency {};\n"), 28));
            const auto nested = contents ? std::find_if(contents->begin(), contents->end(), [&](const auto& item) {
                return item.path == nestedHeader.lexically_normal();
            }) : std::vector<CppSourceFileContent>::const_iterator{};
            nativesharp::basic::Sha256Digest rootDigest{};
            std::uintmax_t rootSize = 0;
            std::string rootError;
            const bool rootHashed = nativesharp::basic::sha256File(mainHeader, rootDigest, rootSize, rootError);
            const auto rootEntry = contents ? std::find_if(contents->begin(), contents->end(), [&](const auto& item) {
                return item.path == mainHeader.lexically_normal();
            }) : std::vector<CppSourceFileContent>::const_iterator{};
            ok &= expect(rootHashed && contents && rootEntry != contents->end() &&
                             rootEntry->size == rootSize && rootEntry->digest == rootDigest,
                         "Clang source-file content snapshot hashes the physical root header");
            ok &= expect(contents && nested != contents->end() && nested->size == 28 && nested->digest == expectedNested,
                         "Clang source-file content snapshot hashes the observed transitive header");
            session->close();
            ok &= expect(!session->sourceFilesSnapshot() && !session->sourceFileContentsSnapshot() &&
                             contains(nestedHeader) && contents && nested != contents->end() && nested->digest == expectedNested,
                         "copied source-file snapshot survives session closure");
        }
    }
    {
        namespace fs = std::filesystem;
        const fs::path root = fs::absolute(fs::path(argv[2]) / "cpp-fixtures" / "binary-catalog");
        std::error_code ec;
        fs::create_directories(root / "first", ec);
        if (!ec) fs::create_directories(root / "second", ec);
        const fs::path library = root / "first" / "api.lib";
        const fs::path duplicate = root / "second" / "API.lib";
        const fs::path runtime = root / "first" / "api.dll";
        { std::ofstream file(library, std::ios::binary); file << "first"; }
        { std::ofstream file(duplicate, std::ios::binary); file << "different"; }
        { std::ofstream file(runtime, std::ios::binary); file << "runtime"; }
        nativesharp::compilation::CppLibraryReference first;
        first.libraryPath = library; first.runtimePath = runtime;
        nativesharp::compilation::CppLibraryReference headerOnly;
        std::vector<nativesharp::interop::CppBinaryDependency> catalog;
        std::string catalogError;
        const std::vector<nativesharp::compilation::CppLibraryReference> references = {headerOnly, first, first};
        const bool built = !ec && nativesharp::interop::collectCppBinaryDependencies(references, catalog, catalogError);
        const std::string expected = "a7937b64b8caa58f03721bb6bacf5c78cb235febe0e70b1b84cd99541461a08e";
        ok &= expect(built && catalog.size() == 2 && catalog[0].library.fileName == "api.lib" &&
                         catalog[0].library.size == 5 && nativesharp::basic::sha256Hex(catalog[0].library.digest) == expected &&
                         catalog[0].runtime.fileName == "api.dll" && catalog[0].runtime.size == 7 &&
                         catalog[1].library.digest == catalog[0].library.digest,
                     "C++ binary catalog hashes artifacts and preserves link-order duplicates");
        std::vector<nativesharp::interop::ResolvedCppBinaryDependency> resolved;
        const bool resolvedOk = built && nativesharp::interop::resolveCppBinaryDependencies(
            catalog, root / "first", resolved, catalogError);
        ok &= expect(resolvedOk && resolved.size() == 2 && resolved[0].libraryPath == library &&
                         resolved[0].runtimePath == runtime && resolved[1].libraryPath == library,
                     "C++ binary consumer resolves verified libraries and runtimes in link order");
        { std::ofstream file(library, std::ios::binary | std::ios::trunc); file << "other"; }
        ok &= expect(!nativesharp::interop::resolveCppBinaryDependencies(catalog, root / "first", resolved, catalogError) &&
                         catalogError.find("identity mismatch") != std::string::npos && resolved.size() == 2 &&
                         resolved[0].libraryPath == library,
                     "C++ binary consumer rejects modified same-size library without changing output");
        { std::ofstream file(library, std::ios::binary | std::ios::trunc); file << "first"; }
        auto unsafeCatalog = catalog;
        if (!unsafeCatalog.empty()) unsafeCatalog[0].library.fileName = "../api.lib";
        ok &= expect(!nativesharp::interop::resolveCppBinaryDependencies(unsafeCatalog, root / "first", resolved, catalogError) &&
                         resolved.size() == 2 && resolved[0].libraryPath == library,
                     "C++ binary consumer rejects parent traversal without changing output");
        nativesharp::compilation::CppLibraryReference collision;
        collision.libraryPath = duplicate;
        const std::vector<nativesharp::compilation::CppLibraryReference> conflicting = {first, collision};
        const bool collisionRejected = !nativesharp::interop::collectCppBinaryDependencies(conflicting, catalog, catalogError);
        ok &= expect(collisionRejected && catalog.size() == 2 && nativesharp::basic::sha256Hex(catalog[0].library.digest) == expected,
                     "C++ binary catalog rejects conflicting Windows names without changing output");
        collision.libraryPath = root / "missing.lib";
        const std::vector<nativesharp::compilation::CppLibraryReference> missing = {first, collision};
        ok &= expect(!nativesharp::interop::collectCppBinaryDependencies(missing, catalog, catalogError) &&
                         catalog.size() == 2 && nativesharp::basic::sha256Hex(catalog[0].library.digest) == expected,
                     "C++ binary catalog rejects missing files without changing output");
        {
            using namespace nativesharp::compilation;
            const auto otherPath = root / "first" / "other.lib";
            { std::ofstream file(otherPath, std::ios::binary); file << "second"; }
            CppLibraryReference other;
            other.libraryPath = otherPath;
            const auto makeProject = [&](const char* name, std::vector<CppLibraryReference> refs) {
                return Project(root / name, name, ProjectOutputKind::Library, ProjectSourceSet(root),
                    {}, {}, {}, nativesharp::language::PanicProfile::Abort, {}, {}, {}, std::move(refs));
            };
            std::vector<Project> projects;
            projects.push_back(makeProject("dep.nsproj", {headerOnly, first}));
            projects.push_back(makeProject("root.nsproj", {other, first}));
            ProjectGraph graph(root / "root.nsproj", std::move(projects));
            std::vector<LibraryMetadataCppBinaryDependency> graphCatalog;
            const bool graphBuilt = collectCppGraphBinaryMetadataDependencies(graph, graphCatalog, catalogError);
            ok &= expect(graphBuilt && graphCatalog.size() == 3 && graphCatalog[0].libraryFileName == "api.lib" &&
                             graphCatalog[1].libraryFileName == "other.lib" && graphCatalog[2].libraryFileName == "api.lib",
                         "C++ binary catalog includes references from all projects in graph order");
            collision.libraryPath = duplicate;
            std::vector<Project> conflictProjects;
            conflictProjects.push_back(makeProject("dep.nsproj", {first}));
            conflictProjects.push_back(makeProject("root.nsproj", {collision}));
            ProjectGraph conflictingGraph(root / "root.nsproj", std::move(conflictProjects));
            ok &= expect(!collectCppGraphBinaryMetadataDependencies(conflictingGraph, graphCatalog, catalogError) &&
                             graphCatalog.size() == 3 && graphCatalog[1].libraryFileName == "other.lib",
                         "C++ graph catalog rejects cross-project name conflicts without changing output");
        }
        if (built && !catalog.empty())
        {
        nativesharp::interop::LibraryMetadata model;
        model.moduleName = "BinaryCatalogTest";
        model.producerVersion = "test";
        model.targetTriple = "x86_64-pc-windows-msvc";
        std::string metadataError;
        auto libraryOnly = first;
        libraryOnly.runtimePath.clear();
        const std::vector<nativesharp::compilation::CppLibraryReference> metadataReferences = {first, libraryOnly, first};
        const bool metadataCollected = nativesharp::interop::collectCppBinaryMetadataDependencies(
            metadataReferences, model.cppBinaryDependencies, metadataError);
        nativesharp::interop::LibraryMetadataCodec codec;
        const auto metadataPath = root / "cpp-closure.nsmeta";
        nativesharp::interop::LibraryMetadata decoded;
        const bool encoded = metadataCollected && codec.writeModel(model, metadataPath, metadataError) &&
            codec.read(metadataPath, decoded, metadataError);
        ok &= expect(encoded && decoded.formatMinor == 41 && decoded.cppBinaryDependencies == model.cppBinaryDependencies,
                     "NSMETA preserves ordered C++ binary dependencies and repeated link inputs");
        std::vector<nativesharp::interop::ResolvedCppBinaryDependency> decodedPaths;
        const bool decodedResolved = encoded && nativesharp::interop::resolveCppBinaryMetadataDependencies(
            decoded.cppBinaryDependencies, root / "first", decodedPaths, metadataError);
        ok &= expect(decodedResolved && decodedPaths.size() == 3 && decodedPaths[0].libraryPath == library &&
                         decodedPaths[0].runtimePath == runtime && decodedPaths[1].runtimePath.empty() &&
                         decodedPaths[2].libraryPath == library,
                     "serialized C++ binary closure resolves in order with an optional runtime");
        { std::ofstream file(library, std::ios::binary | std::ios::trunc); file << "other"; }
        ok &= expect(!nativesharp::interop::resolveCppBinaryMetadataDependencies(
                         decoded.cppBinaryDependencies, root / "first", decodedPaths, metadataError) &&
                         metadataError.find("identity mismatch") != std::string::npos &&
                         decodedPaths.size() == 3 && decodedPaths[0].libraryPath == library,
                     "serialized C++ closure rejects modified files without changing resolved output");
        { std::ofstream file(library, std::ios::binary | std::ios::trunc); file << "first"; }
        ok &= expect(encoded && codec.verifyCurrentCompiler(decoded, metadataError),
                     "NSMETA accepts structurally valid C++ binary and runtime dependencies");
        auto oldModel = model;
        oldModel.formatMinor = 40;
        ok &= expect(!codec.writeModel(oldModel, root / "invalid-old.nsmeta", metadataError),
                     "NSMETA 1.40 refuses to silently discard C++ dependencies");
        oldModel.cppBinaryDependencies.clear();
        nativesharp::interop::LibraryMetadata oldDecoded;
        const bool oldRoundTrip = codec.writeModel(oldModel, root / "old.nsmeta", metadataError) &&
            codec.read(root / "old.nsmeta", oldDecoded, metadataError) &&
            codec.verifyCurrentCompiler(oldDecoded, metadataError);
        ok &= expect(oldRoundTrip && oldDecoded.formatMinor == 40 && oldDecoded.cppBinaryDependencies.empty(),
                     "NSMETA 1.40 without C++ closure remains readable and valid");
        auto baseline = model;
        baseline.cppBinaryDependencies.clear();
        nativesharp::interop::LibraryMetadata baselineDecoded;
        const bool baselineRoundTrip = codec.writeModel(baseline, root / "baseline.nsmeta", metadataError) &&
            codec.read(root / "baseline.nsmeta", baselineDecoded, metadataError);
        ok &= expect(encoded && baselineRoundTrip && decoded.abiFingerprint == baselineDecoded.abiFingerprint,
                     "private C++ binary closure does not change the public ABI fingerprint");
        }
    }
    {
        const std::vector<std::string> current{"A", "B"};
        std::vector<std::string> visited;
        const auto relative = nativesharp::frontend::resolveQualifiedNamespaceTarget(
            current, "C", [&](const std::string& candidate) {
                visited.push_back(candidate);
                return candidate == "A.C" || candidate == "C";
            });
        const bool relativeOrder = relative == "A.C" && visited == std::vector<std::string>({"A.B.C", "A.C"});
        visited.clear();
        const auto global = nativesharp::frontend::resolveQualifiedNamespaceTarget(
            current, "C", [&](const std::string& candidate) {
                visited.push_back(candidate);
                return candidate == "C";
            });
        const bool globalOrder = global == "C" && visited == std::vector<std::string>({"A.B.C", "A.C", "C"});
        const auto missing = nativesharp::frontend::resolveQualifiedNamespaceTarget(
            current, "Missing", [](const std::string&) { return false; });
        ok &= expect(relativeOrder && globalOrder && !missing,
                     "namespace target lookup prefers nearest visible scope, then global, and reports absence");
    }
    {
        using nativesharp::FileId;
        using nativesharp::SourceLocation;
        using nativesharp::parser::ScopedKnownTypeNames;
        const FileId file{1};
        const ScopedKnownTypeNames names({
            {file, 0, 100, {"Global"}},
            {file, 10, 30, {"Left"}},
            {file, 15, 20, {"Nested"}},
            {file, 30, 50, {"Right"}}});
        ok &= expect(names.contains(SourceLocation(file, 15), "Global") &&
                     names.contains(SourceLocation(file, 15), "Left") &&
                     names.contains(SourceLocation(file, 15), "Nested") &&
                     !names.contains(SourceLocation(file, 20), "Nested") &&
                     !names.contains(SourceLocation(file, 30), "Left") &&
                     names.contains(SourceLocation(file, 30), "Right") &&
                     !names.contains(SourceLocation(file, 100), "Global") &&
                     !names.contains(SourceLocation(FileId{2}, 15), "Left") &&
                     !names.contains(SourceLocation{}, "Global") &&
                     !names.contains(SourceLocation(file, 15), ""),
                     "scoped C++ parser names respect files, nested scopes, siblings and half-open boundaries");
        const ScopedKnownTypeNames effective({{file, 0, 100, {"Parent"}}, {file, 10, 20, {"Child"}}}, true);
        ok &= expect(effective.contains(SourceLocation(file, 5), "Parent") &&
                     !effective.contains(SourceLocation(file, 15), "Parent") &&
                     effective.contains(SourceLocation(file, 15), "Child") &&
                     effective.contains(SourceLocation(file, 20), "Parent"),
                     "effective scoped name sets use the innermost complete environment");
    }
    {
        using nativesharp::FileId;
        using nativesharp::parser::ScopedKnownTypeNames;
        const FileId file{77};
        const std::string source = "namespace Left { int F() { Item* p = null; return 0; } } "
            "namespace Right { int G() { int Item = 2; int p = 3; Item * p; return Item; } }";
        const auto right = static_cast<std::uint32_t>(source.find("namespace Right"));
        const ScopedKnownTypeNames scopes({{file, 0, right, {"Item"}}});
        nativesharp::source::SourceFile input(file, "scoped-test.ns", source);
        nativesharp::frontend::ProjectSyntaxUnit unit(0, 0, "ScopeTest", file);
        nativesharp::diagnostics::DiagnosticEngine diagnostics;
        const auto known = nativesharp::parser::buildKnownTypeNameSet();
        const bool parsed = unit.lex(input, diagnostics) && unit.parse(diagnostics, known, &scopes);
        bool pointer = false;
        bool multiply = false;
        std::vector<const nativesharp::ast::AstNode*> pending{unit.syntaxTree()};
        std::unordered_set<const nativesharp::ast::AstNode*> seen;
        while (!pending.empty())
        {
            const auto* node = pending.back(); pending.pop_back();
            if (!node || !seen.insert(node).second) continue;
            pointer |= node->kind() == nativesharp::ast::AstNodeKind::VariableDeclarationStatement &&
                node->typeSyntax() && node->typeSyntax()->baseName() == "Item";
            multiply |= node->kind() == nativesharp::ast::AstNodeKind::BinaryExpression &&
                node->tokenKind() == nativesharp::lexer::TokenKind::Star;
            pending.push_back(node->first()); pending.push_back(node->second()); pending.push_back(node->third());
            for (const auto* child : node->children()) pending.push_back(child);
        }
        ok &= expect(parsed && pointer && multiply && !diagnostics.hasErrors(),
                     "parser consumes scoped names only at the source location of each statement");
    }
    {
        const nativesharp::FileId file{78};
        const std::string source = "using Root; using Root; namespace A.B { using Alias = Shared.Token; "
            "namespace Inner { using Shared; int F() { using Fake; return 0; } } } "
            "namespace Other { int G() { return 0; } }";
        nativesharp::source::SourceFile input(file, "scope-scan.ns", source);
        nativesharp::frontend::ProjectSyntaxUnit unit(0, 0, "ScopeTest", file);
        nativesharp::diagnostics::DiagnosticEngine diagnostics;
        const bool lexed = unit.lex(input, diagnostics);
        const auto scopes = nativesharp::parser::lexicalNamespaceScopes(unit.tokens());
        ok &= expect(lexed && scopes.size() == 4 && scopes[0].directives.size() == 2 &&
                     scopes[0].directives[0].target == std::vector<std::string>{"Root"} &&
                     scopes[1].namespaceSegments == std::vector<std::string>({"A", "B"}) &&
                     scopes[1].parent == 0 && scopes[1].directives.size() == 1 &&
                     scopes[1].directives[0].alias == "Alias" &&
                     scopes[1].directives[0].target == std::vector<std::string>({"Shared", "Token"}) &&
                     scopes[2].parent == 1 && scopes[2].directives.size() == 1 &&
                     scopes[2].namespaceSegments == std::vector<std::string>({"A", "B", "Inner"}) &&
                     scopes[3].parent == 0 && scopes[2].end <= scopes[3].begin,
                     "lexical namespace scopes preserve parents, directives and skip function bodies");
        const std::vector<std::string> visibleNamespaces{"Root", "Shared", "A.B.Inner.Shared"};
        const auto environments = nativesharp::frontend::lexicalNamespaceEnvironments(scopes, visibleNamespaces);
        ok &= expect(environments && environments->size() == 4 &&
                     (*environments)[0].importedNamespaces == std::vector<std::string>{"Root"} &&
                     (*environments)[1].aliases.size() == 1 &&
                     (*environments)[2].importedNamespaces ==
                         std::vector<std::string>({"Root", "A.B.Inner.Shared"}) &&
                     (*environments)[2].aliases.size() == 1 &&
                     (*environments)[3].importedNamespaces == std::vector<std::string>{"Root"} &&
                     (*environments)[3].aliases.empty(),
                     "namespace environments use relative priority and inherit only through parents");
        const std::vector<std::string> missingNamespaces{"Root"};
        const auto missingEnvironments = nativesharp::frontend::lexicalNamespaceEnvironments(scopes, missingNamespaces);
        ok &= expect(missingEnvironments && (*missingEnvironments)[2].unresolvedNamespaces.size() == 1 &&
                     (*missingEnvironments)[3].unresolvedNamespaces.empty(),
                     "unresolved namespace directives stay visible in their own scope");
        const std::string fileSource = "namespace A.B; using Shared; int F() { return 0; }";
        nativesharp::source::SourceFile fileInput(file, "file-scope.ns", fileSource);
        nativesharp::frontend::ProjectSyntaxUnit fileUnit(0, 0, "ScopeTest", file);
        nativesharp::diagnostics::DiagnosticEngine fileDiagnostics;
        const bool fileLexed = fileUnit.lex(fileInput, fileDiagnostics);
        const auto fileScopes = nativesharp::parser::lexicalNamespaceScopes(fileUnit.tokens());
        ok &= expect(fileLexed && fileScopes.size() == 2 && fileScopes[1].parent == 0 &&
                     fileScopes[1].namespaceSegments == std::vector<std::string>({"A", "B"}) &&
                     fileScopes[1].directives.size() == 1 && fileScopes[1].end == fileScopes[0].end,
                     "file-scoped namespace retains its using until the end of the file");
    }
    {
        const nativesharp::FileId file{79};
        const std::string source = "namespace Left { using Shared; int F() { Item* p = null; return 0; } } "
            "namespace Right { int G() { Shared.Item* q = null; int Item = 2; int p = 3; "
            "Item * p; return Item; } }";
        nativesharp::source::SourceFile input(file, "cpp-scope-seeds.ns", source);
        nativesharp::frontend::ProjectSyntaxUnit unit(0, 0, "ScopeTest", file);
        nativesharp::diagnostics::DiagnosticEngine diagnostics;
        const bool lexed = unit.lex(input, diagnostics);
        const auto lexical = nativesharp::parser::lexicalNamespaceScopes(unit.tokens());
        const std::vector<std::string> namespaces{"Shared"};
        const std::vector<std::string> types{"Shared.Item"};
        const auto scoped = nativesharp::frontend::cppScopedTypeNames(lexical, namespaces, types);
        const auto leftPos = static_cast<std::uint32_t>(source.find("Item* p"));
        const auto rightPos = static_cast<std::uint32_t>(source.find("Item * p"));
        const bool visibility = scoped && scoped->contains(nativesharp::SourceLocation(file, leftPos), "Item") &&
            !scoped->contains(nativesharp::SourceLocation(file, rightPos), "Item") &&
            scoped->contains(nativesharp::SourceLocation(file, rightPos), "Shared.Item");
        const auto known = nativesharp::parser::buildKnownTypeNameSet();
        const bool parsed = lexed && scoped && unit.parse(diagnostics, known, &*scoped);
        bool pointer = false;
        bool multiply = false;
        std::vector<const nativesharp::ast::AstNode*> pending{unit.syntaxTree()};
        std::unordered_set<const nativesharp::ast::AstNode*> seen;
        while (!pending.empty())
        {
            const auto* node = pending.back(); pending.pop_back();
            if (!node || !seen.insert(node).second) continue;
            pointer |= node->kind() == nativesharp::ast::AstNodeKind::VariableDeclarationStatement &&
                node->typeSyntax() && node->typeSyntax()->baseName() == "Item";
            multiply |= node->kind() == nativesharp::ast::AstNodeKind::BinaryExpression &&
                node->tokenKind() == nativesharp::lexer::TokenKind::Star;
            pending.push_back(node->first()); pending.push_back(node->second()); pending.push_back(node->third());
            for (const auto* child : node->children()) pending.push_back(child);
        }
        ok &= expect(visibility && parsed && pointer && multiply && !diagnostics.hasErrors(),
                     "C++ scoped seeds follow namespace imports without affecting sibling expressions");
        auto unsupported = lexical;
        unsupported[0].directives.push_back({"Alias", {"Shared", "Item"}});
        const auto simpleAlias = nativesharp::frontend::cppScopedTypeNames(unsupported, namespaces, types);
        ok &= expect(simpleAlias && simpleAlias->contains(nativesharp::SourceLocation(file, leftPos), "Alias"),
                     "scoped seed preparation recognizes a simple alias to a C++ type");
        const std::string aliasSource = "using P = Shared.Item*; using Count = int; "
            "int F() { P p = null; Count n = 1; return n; }";
        nativesharp::source::SourceFile aliasInput(file, "cpp-complex-alias.ns", aliasSource);
        nativesharp::frontend::ProjectSyntaxUnit aliasUnit(0, 0, "ScopeTest", file);
        nativesharp::diagnostics::DiagnosticEngine aliasDiagnostics;
        const bool aliasLexed = aliasUnit.lex(aliasInput, aliasDiagnostics);
        const auto aliasScopes = nativesharp::parser::lexicalNamespaceScopes(aliasUnit.tokens());
        const auto aliasEnvironment = nativesharp::frontend::lexicalNamespaceEnvironments(aliasScopes, namespaces);
        const auto aliasNames = nativesharp::frontend::cppScopedTypeNames(aliasScopes, namespaces, types);
        const auto aliasKnown = nativesharp::parser::buildKnownTypeNameSet();
        const bool aliasParsed = aliasLexed && aliasNames && aliasUnit.parse(aliasDiagnostics, aliasKnown, &*aliasNames);
        ok &= expect(aliasLexed && aliasEnvironment &&
                     (*aliasEnvironment)[0].unsupportedDirectives.empty() &&
                     (*aliasEnvironment)[0].aliases.size() == 2 &&
                     (*aliasEnvironment)[0].aliases[0].directive.explicitTypeSyntax &&
                     (*aliasEnvironment)[0].aliases[1].directive.explicitTypeSyntax &&
                     aliasParsed && !aliasDiagnostics.hasErrors(),
                     "record pointer and builtin aliases remain type names and parse with their full syntax");
        const std::string invalidSource = "using Bad = Shared.Item + 1; int F() { return 0; }";
        nativesharp::source::SourceFile invalidInput(file, "cpp-invalid-alias.ns", invalidSource);
        nativesharp::frontend::ProjectSyntaxUnit invalidUnit(0, 0, "ScopeTest", file);
        nativesharp::diagnostics::DiagnosticEngine invalidDiagnostics;
        const bool invalidLexed = invalidUnit.lex(invalidInput, invalidDiagnostics);
        const auto invalidScopes = nativesharp::parser::lexicalNamespaceScopes(invalidUnit.tokens());
        const auto invalidEnvironment = nativesharp::frontend::lexicalNamespaceEnvironments(invalidScopes, namespaces);
        ok &= expect(invalidLexed && invalidEnvironment &&
                     (*invalidEnvironment)[0].unsupportedDirectives.size() == 1 &&
                     !nativesharp::frontend::cppScopedTypeNames(invalidScopes, namespaces, types) &&
                     !invalidUnit.parse(invalidDiagnostics, aliasKnown) && invalidDiagnostics.hasErrors(),
                     "invalid alias remains unsupported and the ordinary parser reports its syntax error");
    }
    {
        const nativesharp::FileId file{80};
        const std::string source = "namespace Left { using N = Shared; using T = Shared.Item; using U = T; "
            "int F() { N.Item* n = null; T* t = null; U* u = null; return 0; } "
            "namespace Child { int H() { U* p = null; return 0; } } } "
            "namespace Right { int G() { int T = 1; int p = 2; T * p; return T; } }";
        nativesharp::source::SourceFile input(file, "cpp-scope-aliases.ns", source);
        nativesharp::frontend::ProjectSyntaxUnit unit(0, 0, "ScopeTest", file);
        nativesharp::diagnostics::DiagnosticEngine diagnostics;
        const bool lexed = unit.lex(input, diagnostics);
        const auto scopes = nativesharp::parser::lexicalNamespaceScopes(unit.tokens());
        const std::vector<std::string> namespaces{"Shared"};
        const std::vector<std::string> types{"Shared.Item"};
        const auto scoped = nativesharp::frontend::cppScopedTypeNames(scopes, namespaces, types);
        const auto left = static_cast<std::uint32_t>(source.find("N.Item*"));
        const auto right = static_cast<std::uint32_t>(source.find("T * p"));
        const auto child = static_cast<std::uint32_t>(source.find("U* p"));
        const bool correctNames = scoped &&
            scoped->contains(nativesharp::SourceLocation(file, left), "N.Item") &&
            scoped->contains(nativesharp::SourceLocation(file, left), "T") &&
            scoped->contains(nativesharp::SourceLocation(file, left), "U") &&
            scoped->contains(nativesharp::SourceLocation(file, child), "U") &&
            !scoped->contains(nativesharp::SourceLocation(file, right), "T");
        const auto known = nativesharp::parser::buildKnownTypeNameSet();
        const bool parsed = lexed && scoped && unit.parse(diagnostics, known, &*scoped);
        ok &= expect(correctNames && parsed && !diagnostics.hasErrors(),
                     "scoped C++ names resolve namespace and chained type aliases without leaking to siblings");
    }
    {
        using namespace nativesharp::compilation;
        const auto fixture = std::filesystem::path(argv[2]) / "cpp-fixtures";
        const auto projectPath = fixture / "project" / "CppProbe.nsproj";
        std::string error;
        ProjectLoader loader;
        auto project = loader.load(projectPath, error);
        if (!project) std::cerr << "Project load failed: " << error << '\n';
        ok &= expect(project && project->cppLibraryReferences().size() == 2,
                     "project loads C and C++ references");
        if (project && project->cppLibraryReferences().size() == 2)
        {
            const auto& cRef = project->cppLibraryReferences()[0];
            const auto& cppRef = project->cppLibraryReferences()[1];
            ok &= expect(cRef.language == CppLanguageMode::C && cRef.languageStandard == "c11" &&
                         !cRef.libraryPath.empty() && cRef.includePaths.size() == 1 &&
                         cRef.macros.size() == 3 && cRef.macros[0].action == CppMacroAction::Define &&
                         cRef.macros[1].action == CppMacroAction::Undefine &&
                         cRef.macros[2].action == CppMacroAction::Define,
                         "C library options and macro ordering survive loading");
            ok &= expect(cppRef.language == CppLanguageMode::Cpp && cppRef.libraryPath.empty(),
                         "C++ header-only references need no library");
            const CppProjectSessionEnvironment environment{
                "x86_64-pc-windows-msvc", argv[3], argv[1], argv[2], {fixture}};
            auto opened = openCppProjectSessions(project->cppLibraryReferences(), environment, factory, {});
            auto* sessions = std::get_if<CppProjectSessions>(&opened);
            ok &= expect(sessions && sessions->sessions.size() == 2,
                         "project references each open a real Clang session");
            if (sessions && sessions->sessions.size() == 2)
            {
                const auto cOptions = sessions->sessions[0]->optionsSnapshot();
                const auto cppOptions = sessions->sessions[1]->optionsSnapshot();
                const auto* file = std::get_if<CppFileInput>(&cOptions.input);
                ok &= expect(cOptions.language == CppLanguageMode::C && cOptions.macros.size() == 3 &&
                             cOptions.includePaths.size() == 2 &&
                             cOptions.includePaths[1].kind == CppIncludeKind::System &&
                             cOptions.includePaths[1].path == fixture && file && file->path == cRef.headerPath &&
                             cppOptions.language == CppLanguageMode::Cpp && cppOptions.macros.empty() &&
                             cppOptions.languageStandard == "c++20" && cppOptions.includePaths.size() == 1 &&
                             cppOptions.includePaths[0].kind == CppIncludeKind::System,
                             "project session options retain each reference configuration and order");
            }
            std::vector<CppLibraryReference> broken(project->cppLibraryReferences().begin(),
                                                     project->cppLibraryReferences().end());
            broken[1].headerPath = fixture / "missing-session-header.h";
            auto failed = openCppProjectSessions(broken, environment, factory, {});
            const auto* failure = std::get_if<CppProjectSessionFailure>(&failed);
            ok &= expect(failure && failure->referenceIndex == 1 && failure->failure.phase == "input",
                         "failed second session returns its index without a partial session set");
            CppCancellationToken cancelledToken;
            cancelledToken.cancel();
            auto cancelledProject = openCppProjectSessions(project->cppLibraryReferences(), environment,
                                                            factory, cancelledToken);
            const auto* cancelledProjectFailure = std::get_if<CppProjectSessionFailure>(&cancelledProject);
            ok &= expect(cancelledProjectFailure && cancelledProjectFailure->referenceIndex == 0 &&
                         cancelledProjectFailure->failure.phase == "cancel",
                         "project session opening propagates cancellation");
            auto systemReference = std::vector<CppLibraryReference>{cRef};
            systemReference[0].headerPath = fixture / "system-probe.h";
            auto systemEnvironment = environment;
            systemEnvironment.systemIncludePaths = {fixture / "system"};
            auto systemOpened = openCppProjectSessions(systemReference, systemEnvironment, factory, {});
            const auto* systemSessions = std::get_if<CppProjectSessions>(&systemOpened);
            bool foundSystemFunction = false;
            if (systemSessions && systemSessions->sessions.size() == 1)
            {
                const auto functions = systemSessions->sessions[0]->freeFunctionsSnapshot();
                foundSystemFunction = functions && functionByName(*functions, "common_system_function");
            }
            ok &= expect(foundSystemFunction, "common system include paths are used to parse headers");

            auto projectOpened = openCppProjectSessions(*project, environment, factory, {});
            auto* ownedSessions = std::get_if<CppProjectSessions>(&projectOpened);
            ok &= expect(ownedSessions && ownedSessions->projectFile == project->projectFile(),
                         "project session set retains its owner identity");
            auto otherProject = loader.load(fixture / "other" / "Other.nsproj", error);
            ok &= expect(static_cast<bool>(otherProject), "second project loads");
            if (otherProject)
            {
                auto plainProject = loader.load(fixture / "plain" / "Plain.nsproj", error);
                ok &= expect(static_cast<bool>(plainProject), "project without C/C++ references loads");
                std::vector<Project> graphProjects;
                graphProjects.push_back(*otherProject);
                if (plainProject) graphProjects.push_back(*plainProject);
                graphProjects.push_back(*project);
                ProjectGraph combinedGraph(project->projectFile(), std::move(graphProjects));
                {
                    std::vector<CppProjectSessions> groups(2);
                    groups[0].projectFile = otherProject->projectFile();
                    groups[0].sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                        CppLanguageMode::C, "struct Opaque; int other_value(void); int pointer_value(struct Opaque* p); extern int c_global; static int internal_global;\n")));
                    groups[0].sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                        CppLanguageMode::C, "int other_value(void);\n")));
                    groups[0].sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                        CppLanguageMode::Cpp, "namespace other {} namespace { namespace hidden {} }\n")));
                    groups[0].sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                        CppLanguageMode::Cpp, "namespace other { namespace deep {} }\n")));
                    groups[1].projectFile = project->projectFile();
                    groups[1].sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                        CppLanguageMode::Cpp, "namespace feature { int value(int); double value(double); extern int shared; extern const int limit; extern volatile int io; extern thread_local int tls; static int local; }\n")));
                    groups[1].sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                        CppLanguageMode::C, "int other_value(void);\n")));
                    std::vector<Project> originalOrder{*otherProject, *project};
                    ProjectGraph finalGraph(project->projectFile(), std::move(originalOrder));
                    const std::vector<PrecompiledProjectDescriptor> addedModules{{
                        project->projectFile(), fixture / "invented-metadata.nsmeta", "InventoryMetadata"}};
                    ok &= expect(finalGraph.addPrecompiledProjects(addedModules, error),
                                 "test inserts a precompiled module before the source projects");
                    {
                        const auto globals = collectCppSemanticGlobals(finalGraph, groups);
                        const auto* entries = std::get_if<std::vector<CppSemanticGlobalEntry>>(&globals);
                        bool cGlobal = false, cppGlobal = false, internalRejected = false;
                        bool volatileProjected = false, constProjected = false, tlsRejected = false;
                        if (entries)
                            for (const auto& entry : *entries)
                            {
                                const auto* symbol = std::get_if<CppSemanticGlobal>(&entry.projection);
                                const auto* rejected = std::get_if<CppSemanticGlobalFailure>(&entry.projection);
                                if (entry.declaration.name == "c_global")
                                    cGlobal = symbol && entry.projectOrdinal == 1 && entry.referenceIndex == 0 &&
                                        symbol->externalSymbol == "c_global" && symbol->typeKey == "builtin:int" &&
                                        symbol->namespaceName.empty() && entry.targetTriple == "x86_64-pc-windows-msvc";
                                if (entry.declaration.name == "shared")
                                    cppGlobal = symbol && entry.projectOrdinal == 2 && entry.referenceIndex == 0 &&
                                        symbol->externalSymbol == "?shared@feature@@3HA" && symbol->namespaceName == "feature" &&
                                        symbol->typeKey == "builtin:int";
                                if (entry.declaration.name == "local" || entry.declaration.name == "internal_global")
                                    internalRejected |= rejected && !entry.declaration.externalLinkage;
                                if (entry.declaration.name == "io") volatileProjected = symbol && symbol->isVolatile;
                                if (entry.declaration.name == "limit") constProjected = symbol && symbol->isConst;
                                if (entry.declaration.name == "tls") tlsRejected = rejected && entry.declaration.threadLocal;
                            }
                        ok &= expect(entries && entries->size() == 7 && cGlobal && cppGlobal && internalRejected &&
                                     volatileProjected && constProjected && tlsRejected,
                                     "global inventory preserves project, exact symbol, type and unsupported storage contracts");
                        if (entries)
                        {
                            nativesharp::frontend::CppFunctionDemand globalDemand;
                            globalDemand.lookupName = "shared";
                            globalDemand.lookupNamespaces = {"feature"};
                            globalDemand.visibleProjects = {2};
                            auto unsupportedDemand = globalDemand;
                            unsupportedDemand.lookupName = "tls";
                            auto invisibleDemand = globalDemand;
                            invisibleDemand.visibleProjects = {1};
                            const std::vector demands{globalDemand, globalDemand, unsupportedDemand, invisibleDemand};
                            std::vector<nativesharp::frontend::NamespaceStateSymbol> provided;
                            std::string providerError;
                            const bool providedOk = provideCppSemanticGlobals(finalGraph, groups, demands, provided, providerError);
                            bool foundShared = false, foundDeferred = false;
                            for (const auto& state : provided)
                            {
                                foundShared |= state.name == "shared" && state.projectOrdinal == 2 &&
                                    state.storageName == "?shared@feature@@3HA" && state.cppImportError.empty();
                                foundDeferred |= state.name == "tls" && state.projectOrdinal == 2 &&
                                    state.cppImportError.find("thread-local") != std::string::npos;
                            }
                            ok &= expect(providedOk && provided.size() == 2 && foundShared && foundDeferred,
                                         "global provider deduplicates demands, keeps project visibility and defers unsupported storage");
                            const auto selected = selectCppSemanticGlobal(*entries, 2, "feature::shared");
                            const auto internal = selectCppSemanticGlobal(*entries, 2, "feature::local");
                            const auto unsupported = selectCppSemanticGlobal(*entries, 2, "feature::tls");
                            const auto invisible = selectCppSemanticGlobal(*entries, 1, "feature::shared");
                            ok &= expect(selected.candidate && selected.declarationIndices.size() == 1 &&
                                         internal.internalIndices.size() == 1 && !internal.candidate &&
                                         unsupported.unsupportedIndices.size() == 1 && !unsupported.candidate &&
                                         invisible.declarationIndices.empty(),
                                         "global selection retains project, internal and unsupported declarations");
                            if (selected.candidate && !selected.declarationIndices.empty())
                            {
                            auto repeated = *entries;
                            repeated.push_back((*entries)[selected.declarationIndices[0]]);
                            repeated.back().referenceIndex += 10;
                            const auto same = selectCppSemanticGlobal(repeated, 2, "feature::shared");
                            ok &= expect(same.candidate && same.declarationIndices.size() == 2 && same.conflict.empty(),
                                         "identical external global declarations retain both origins");
                            repeated.back().declaration.type.canonicalSpelling = "double";
                            const auto conflict = selectCppSemanticGlobal(repeated, 2, "feature::shared");
                            ok &= expect(!conflict.candidate && !conflict.conflict.empty() &&
                                         conflict.declarationIndices.size() == 2,
                                         "different types on repeated global declarations conflict");
                            repeated.back() = (*entries)[selected.declarationIndices[0]];
                            repeated.back().declaration.dllImport = !repeated.back().declaration.dllImport;
                            const auto dllConflict = selectCppSemanticGlobal(repeated, 2, "feature::shared");
                            ok &= expect(!dllConflict.candidate && !dllConflict.conflict.empty(),
                                         "different DLL import attributes on repeated global declarations conflict");
                            repeated.back() = (*entries)[selected.declarationIndices[0]];
                            repeated.back().declaration.qualifiedName = "feature::alias";
                            repeated.back().declaration.type.isVolatile = true;
                            const auto alias = selectCppSemanticGlobal(repeated, 2, "feature::shared");
                            ok &= expect(!alias.candidate && !alias.conflict.empty() && alias.aliasIndices.size() == 1,
                                         "an incompatible alias of the same data symbol conflicts");
                            repeated.back().declaration.type.isVolatile = false;
                            repeated.back().targetTriple = "other-target";
                            const auto targetConflict = selectCppSemanticGlobal(repeated, 2, "feature::shared");
                            ok &= expect(!targetConflict.candidate && !targetConflict.conflict.empty(),
                                         "a symbol alias from a different target cannot be merged");
                            }
                        }
                    }
                    {
                        std::vector<CppProjectSessions> recordGroups(2);
                        recordGroups[0].projectFile = otherProject->projectFile();
                        recordGroups[0].sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                            CppLanguageMode::Cpp, "struct Shared { int x; }; struct Same { int x; }; struct Equal { int x; }; struct Forward; struct OnlyForward; "
                            "Shared* use(Shared*); OnlyForward* store(OnlyForward*); int nestedPtr(Shared**); "
                            "void observe(const OnlyForward*); Shared value(Shared); "
                            "template<class T> struct Box {}; int specialized(Box<int>*);\n")));
                        recordGroups[0].sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                            CppLanguageMode::Cpp, "struct Shared { double x; }; struct Same { float x; }; struct Equal { int x; }; struct Forward { int x; };\n")));
                        recordGroups[0].sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                            CppLanguageMode::Cpp, "struct Shared { int x; }; struct Same { int x; };\n")));
                        recordGroups[1].projectFile = project->projectFile();
                        recordGroups[1].sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                            CppLanguageMode::Cpp, "namespace { struct Hidden {}; } struct Item {}; struct Shared {}; Shared* own(Shared*);\n")));
                        auto inventory = collectCppSemanticRecords(finalGraph, recordGroups);
                        const auto* entries = std::get_if<std::vector<CppSemanticRecordEntry>>(&inventory);
                        bool first = false, second = false, item = false, anonymous = false, forward = false;
                        std::optional<std::uint32_t> hashA, hashB, hashAgain;
                        if (entries)
                            for (const auto& entry : *entries)
                            {
                                first |= entry.projectOrdinal == 1 && entry.referenceIndex == 0 &&
                                    entry.declaration.name == "Shared" && entry.declaration.sizeBits == 32 && entry.identity;
                                second |= entry.projectOrdinal == 1 && entry.referenceIndex == 1 &&
                                    entry.declaration.name == "Shared" && entry.declaration.sizeBits == 64 && entry.identity;
                                item |= entry.projectOrdinal == 2 && entry.declaration.name == "Item" && entry.identity;
                                anonymous |= entry.projectOrdinal == 2 && entry.declaration.name == "Hidden" && !entry.identity;
                                forward |= entry.projectOrdinal == 1 && entry.declaration.name == "Forward" &&
                                    !entry.declaration.complete && !entry.declaration.odrHash;
                                if (entry.projectOrdinal == 1 && entry.declaration.name == "Same")
                                {
                                    if (entry.referenceIndex == 0) hashA = entry.declaration.odrHash;
                                    if (entry.referenceIndex == 1) hashB = entry.declaration.odrHash;
                                    if (entry.referenceIndex == 2) hashAgain = entry.declaration.odrHash;
                                }
                            }
                        ok &= expect(first && second && item && anonymous,
                                     "record inventory retains conflicting definitions, origins and anonymous entries");
                        ok &= expect(forward && hashA && hashB && hashAgain && *hashA != *hashB && *hashA == *hashAgain,
                                     "Clang ODR fingerprints distinguish equal-size fields and remain stable across sessions");
                        if (entries)
                        {
                            const auto grouped = groupCppSemanticRecords(*entries);
                            bool sharedConflict = false, sameConflict = false, equalUnverified = false;
                            bool oneDefinition = false, forwardOnly = false, forwardAndDefinition = false, anonymousRetained = false;
                            for (const auto& group : grouped.groups)
                            {
                                sharedConflict |= group.identity.name() == "Shared" && group.projectOrdinal == 1 &&
                                    group.status == CppRecordGroupStatus::Conflict && group.inventoryIndices.size() == 3;
                                sameConflict |= group.identity.name() == "Same" && group.projectOrdinal == 1 &&
                                    group.status == CppRecordGroupStatus::Conflict && group.inventoryIndices.size() == 3;
                                equalUnverified |= group.identity.name() == "Equal" &&
                                    group.status == CppRecordGroupStatus::MultipleDefinitionsUnverified &&
                                    group.inventoryIndices.size() == 2;
                                oneDefinition |= group.identity.name() == "Item" && group.projectOrdinal == 2 &&
                                    group.status == CppRecordGroupStatus::OneDefinition;
                                forwardOnly |= group.identity.name() == "OnlyForward" &&
                                    group.status == CppRecordGroupStatus::ForwardOnly;
                                forwardAndDefinition |= group.identity.name() == "Forward" &&
                                    group.status == CppRecordGroupStatus::OneDefinition && group.inventoryIndices.size() == 2;
                            }
                            for (const auto index : grouped.unprojectedIndices)
                                anonymousRetained |= index < entries->size() && (*entries)[index].declaration.name == "Hidden";
                            ok &= expect(sharedConflict && sameConflict && equalUnverified && oneDefinition &&
                                         forwardOnly && forwardAndDefinition && anonymousRetained,
                                         "record groups retain origins, proven conflicts and unverified matches");
                            const auto comparison = compareCppRecordDefinitions(*entries, grouped, recordGroups);
                            const auto* compared = std::get_if<std::vector<CppRecordStructuralComparison>>(&comparison);
                            bool sameDifferent = false, sharedDifferent = false, equalEquivalent = false;
                            if (compared)
                                for (const auto& item : *compared)
                                {
                                    const auto& record = grouped.groups[item.groupIndex];
                                    sameDifferent |= record.identity.name() == "Same" && item.comparedDefinitions == 3 &&
                                        item.verdict == CppRecordStructuralVerdict::Different;
                                    sharedDifferent |= record.identity.name() == "Shared" && item.comparedDefinitions == 3 &&
                                        item.verdict == CppRecordStructuralVerdict::Different;
                                    equalEquivalent |= record.identity.name() == "Equal" && item.comparedDefinitions == 2 &&
                                        item.verdict == CppRecordStructuralVerdict::Equivalent;
                                }
                            ok &= expect(sameDifferent && sharedDifferent && equalEquivalent,
                                         "record-group comparison retains structural evidence for each owner and definition");
                            const auto refinement = refineCppRecordGroupsForPointerTransport(*entries, grouped, recordGroups);
                            const auto* refined = std::get_if<CppRecordGroups>(&refinement);
                            bool equalPointerOnly = false, conflictsPreserved = false;
                            if (refined)
                                for (const auto& record : refined->groups)
                                {
                                    equalPointerOnly |= record.identity.name() == "Equal" &&
                                        record.status == CppRecordGroupStatus::MultipleDefinitionsPointerCompatible;
                                    conflictsPreserved |= record.identity.name() == "Same" &&
                                        record.status == CppRecordGroupStatus::Conflict;
                                    if (record.identity.name() == "Equal" && record.inventoryIndices.size() == 2)
                                    {
                                        auto candidateEntries = *entries;
                                        for (const auto i : record.inventoryIndices)
                                            candidateEntries[i].declaration.location = CppSourcePoint{
                                                std::filesystem::path(argv[3]), 1, 1};
                                        const bool selected = selectCppRecordMemberDefinition(candidateEntries, record).has_value();
                                        auto& changed = candidateEntries[record.inventoryIndices.back()].declaration;
                                        if (!changed.fields.empty()) changed.fields[0].access = CppAccess::Private;
                                        const bool rejected = !selectCppRecordMemberDefinition(candidateEntries, record);
                                        ok &= expect(selected && rejected,
                                                     "repeated members require equal captured contracts at one physical source location");
                                    }
                                }
                            const auto refinedPrepared = refined ? prepareCppRecordObservation(*entries, *refined) :
                                CppRecordObservationResult{CppRecordObservationFailure{"refinement failed"}};
                            bool noRepeatedLayout = false;
                            if (const auto* prepared = std::get_if<CppRecordObservationInputs>(&refinedPrepared))
                                for (std::size_t i = 0; i < prepared->types.size(); ++i)
                                    noRepeatedLayout |= refined->groups[prepared->groupByImportedType[i]].identity.name() == "Equal" &&
                                        !prepared->types[i].objectLayout;
                            auto noHashEntries = *entries;
                            for (auto& entry : noHashEntries)
                                if (entry.declaration.name == "Equal" && entry.referenceIndex == 1) entry.declaration.odrHash.reset();
                            const auto noHashRefinement = refineCppRecordGroupsForPointerTransport(noHashEntries, grouped, recordGroups);
                            bool missingEvidenceRetained = false;
                            if (const auto* missing = std::get_if<CppRecordGroups>(&noHashRefinement))
                                for (const auto& record : missing->groups)
                                    missingEvidenceRetained |= record.identity.name() == "Equal" &&
                                        record.status == CppRecordGroupStatus::MultipleDefinitionsUnverified;
                            ok &= expect(equalPointerOnly && conflictsPreserved && noRepeatedLayout && missingEvidenceRetained,
                                         "structural refinement admits only pointer transport with complete matching evidence");
                            auto invalidComparisonGroups = grouped;
                            invalidComparisonGroups.groups.push_back(grouped.groups.front());
                            ok &= expect(std::holds_alternative<CppRecordStructuralComparisonFailure>(
                                             compareCppRecordDefinitions(*entries, invalidComparisonGroups, recordGroups)),
                                         "record-group comparison rejects duplicate groups");
                            auto invalidEntries = *entries;
                            invalidEntries[grouped.groups.front().inventoryIndices.front()].referenceIndex = recordGroups[0].sessions.size() + 10;
                            ok &= expect(std::holds_alternative<CppRecordStructuralComparisonFailure>(
                                             compareCppRecordDefinitions(invalidEntries, grouped, recordGroups)),
                                         "record-group comparison rejects missing session references");
                            invalidEntries = *entries;
                            invalidEntries[grouped.groups.front().inventoryIndices.front()].targetTriple = "different-target";
                            ok &= expect(std::holds_alternative<CppRecordStructuralComparisonFailure>(
                                             compareCppRecordDefinitions(invalidEntries, grouped, recordGroups)),
                                         "record-group comparison rejects a target inconsistent with its session");
                            auto signatures = collectCppSemanticFunctions(finalGraph, recordGroups);
                            const auto* signatureEntries = std::get_if<std::vector<CppSemanticInventoryEntry>>(&signatures);
                            bool signatureUseReturn = false, signatureUseParameter = false, signatureNested = false;
                            bool signatureForward = false, signatureOtherOwner = false, signatureSpecialized = false;
                            if (signatureEntries)
                            {
                                bool projectedForward = false, projectedReadonly = false, projectedOther = false;
                                bool rejectedConflict = false, rejectedValue = false, rejectedSpecialization = false;
                                bool rejectedWrongOwner = false, rejectedDuplicate = false, rejectedUnverified = false;
                                for (const auto& entry : *signatureEntries)
                                {
                                    const auto& moduleName = finalGraph.projects()[entry.projectOrdinal].moduleName();
                                    const auto projection = projectCppSemanticFunctionWithRecords(
                                        entry.declaration, entry.projectOrdinal, moduleName, grouped);
                                    const auto* symbol = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(&projection);
                                    const bool failed = std::holds_alternative<CppSemanticFunctionFailure>(projection);
                                    if (entry.declaration.name == "store")
                                    {
                                        const auto identity = projectCppRecordTypeIdentity(*entry.declaration.returnType.element, moduleName);
                                        projectedForward = symbol && identity && symbol->declaredTypeKey == "type:" + identity->key() + "*" &&
                                            symbol->parameters.size() == 1 && symbol->parameters[0].typeKey == "type:" + identity->key() + "*";
                                        auto duplicate = grouped;
                                        for (const auto& group : grouped.groups)
                                            if (group.projectOrdinal == entry.projectOrdinal && group.identity.name() == "OnlyForward")
                                            { duplicate.groups.push_back(group); break; }
                                        rejectedDuplicate = std::holds_alternative<CppSemanticFunctionFailure>(
                                            projectCppSemanticFunctionWithRecords(entry.declaration, entry.projectOrdinal, moduleName, duplicate));
                                        auto unverified = grouped;
                                        for (auto& group : unverified.groups)
                                            if (group.projectOrdinal == entry.projectOrdinal && group.identity.name() == "OnlyForward")
                                                group.status = CppRecordGroupStatus::MultipleDefinitionsUnverified;
                                        rejectedUnverified = std::holds_alternative<CppSemanticFunctionFailure>(
                                            projectCppSemanticFunctionWithRecords(entry.declaration, entry.projectOrdinal, moduleName, unverified));
                                        rejectedWrongOwner = std::holds_alternative<CppSemanticFunctionFailure>(
                                            projectCppSemanticFunctionWithRecords(entry.declaration, 2, moduleName, grouped));
                                    }
                                    if (entry.declaration.name == "observe")
                                        projectedReadonly = symbol && symbol->parameters.size() == 1 && symbol->parameters[0].pointerPointeeReadonly;
                                    if (entry.declaration.name == "own") projectedOther = symbol && symbol->projectOrdinal == 2;
                                    if (entry.declaration.name == "use") rejectedConflict = failed;
                                    if (entry.declaration.name == "value") rejectedValue = failed;
                                    if (entry.declaration.name == "specialized") rejectedSpecialization = failed;
                                }
                                ok &= expect(projectedForward && projectedReadonly && projectedOther && rejectedConflict &&
                                             rejectedValue && rejectedSpecialization && rejectedWrongOwner && rejectedDuplicate && rejectedUnverified,
                                             "isolated record pointer projection preserves identity and rejects unsafe or ambiguous records");
                                const auto dependencyResult = collectCppSignatureRecordDependencies(finalGraph, *signatureEntries, grouped);
                                if (const auto* dependencies = std::get_if<std::vector<CppSignatureRecordDependency>>(&dependencyResult))
                                    for (const auto& dependency : *dependencies)
                                    {
                                        const auto& function = (*signatureEntries)[dependency.functionIndex];
                                        const CppRecordGroup* group = dependency.groupIndex ? &grouped.groups[*dependency.groupIndex] : nullptr;
                                        const bool conflict = group && group->projectOrdinal == 1 &&
                                            group->identity.name() == "Shared" && group->status == CppRecordGroupStatus::Conflict;
                                        if (function.declaration.name == "use" && dependency.indirectionDepth == 1 && conflict)
                                        {
                                            signatureUseReturn |= !dependency.parameterIndex;
                                            signatureUseParameter |= dependency.parameterIndex == std::optional<std::size_t>(0);
                                        }
                                        signatureNested |= function.declaration.name == "nestedPtr" && dependency.indirectionDepth == 2 && conflict;
                                        signatureForward |= function.declaration.name == "store" && group &&
                                            group->status == CppRecordGroupStatus::ForwardOnly;
                                        signatureOtherOwner |= function.declaration.name == "own" && group &&
                                            group->projectOrdinal == 2 && group->identity.name() == "Shared";
                                        signatureSpecialized |= function.declaration.name == "specialized" &&
                                            dependency.status == CppSignatureRecordStatus::Unprojectable && !dependency.groupIndex;
                                    }
                                auto invalidSignatureGroups = grouped;
                                invalidSignatureGroups.groups.push_back(grouped.groups[0]);
                                ok &= expect(std::holds_alternative<CppSignatureRecordFailure>(
                                                 collectCppSignatureRecordDependencies(finalGraph, *signatureEntries, invalidSignatureGroups)),
                                             "signature record association rejects duplicate groups");
                                auto absentGroups = grouped;
                                std::erase_if(absentGroups.groups, [](const auto& item) {
                                    return item.projectOrdinal == 1 && item.identity.name() == "Shared";
                                });
                                const auto missingResult = collectCppSignatureRecordDependencies(finalGraph, *signatureEntries, absentGroups);
                                bool missingReported = false;
                                if (const auto* missing = std::get_if<std::vector<CppSignatureRecordDependency>>(&missingResult))
                                    for (const auto& dependency : *missing)
                                        missingReported |= (*signatureEntries)[dependency.functionIndex].declaration.name == "use" &&
                                            dependency.status == CppSignatureRecordStatus::Missing && !dependency.groupIndex;
                                auto malformedSignatures = *signatureEntries;
                                for (auto& entry : malformedSignatures)
                                    if (entry.declaration.name == "use") entry.declaration.returnType.element.reset();
                                ok &= expect(missingReported && std::holds_alternative<CppSignatureRecordFailure>(
                                                 collectCppSignatureRecordDependencies(finalGraph, malformedSignatures, grouped)),
                                             "signature association reports missing records and rejects incomplete type descriptions");
                            }
                            ok &= expect(signatureUseReturn && signatureUseParameter && signatureNested && signatureForward &&
                                         signatureOtherOwner && signatureSpecialized,
                                         "signature records retain function position, owning project, conflicts and unprojectable types");
                            const auto prepared = prepareCppRecordObservation(*entries, grouped);
                            const auto* inputs = std::get_if<CppRecordObservationInputs>(&prepared);
                            bool preparedConflict = false, preparedOther = false, preparedForwardNoLayout = false;
                            bool preparedUnverifiedNoLayout = false, originsMatch = inputs &&
                                inputs->types.size() == inputs->groupByImportedType.size();
                            if (inputs)
                                for (std::size_t i = 0; i < inputs->types.size(); ++i)
                                {
                                    const auto groupIndex = inputs->groupByImportedType[i];
                                    if (groupIndex >= grouped.groups.size()) { originsMatch = false; continue; }
                                    const auto& group = grouped.groups[groupIndex];
                                    originsMatch &= inputs->types[i].projectOrdinal == group.projectOrdinal &&
                                        inputs->types[i].identity.key() == group.identity.key() &&
                                        !inputs->types[i].sourcePath.empty();
                                    preparedConflict |= group.identity.name() == "Shared" &&
                                        group.status == CppRecordGroupStatus::Conflict && group.inventoryIndices.size() == 3 &&
                                        !inputs->types[i].objectLayout;
                                    preparedOther |= group.identity.name() == "Item" && group.projectOrdinal == 2;
                                    preparedForwardNoLayout |= group.identity.name() == "OnlyForward" && !inputs->types[i].objectLayout;
                                    preparedUnverifiedNoLayout |= group.identity.name() == "Equal" && !inputs->types[i].objectLayout;
                                }
                            ok &= expect(inputs && originsMatch && preparedConflict && preparedOther &&
                                         preparedForwardNoLayout && preparedUnverifiedNoLayout,
                                         "record observation inputs retain origins and do not invent uncertain layouts");
                            auto invalidGroups = grouped;
                            invalidGroups.groups[0].inventoryIndices.push_back(entries->size());
                            ok &= expect(std::holds_alternative<CppRecordObservationFailure>(
                                             prepareCppRecordObservation(*entries, invalidGroups)),
                                         "record observation rejects invalid inventory indices");
                            auto changedTarget = *entries;
                            for (auto& entry : changedTarget)
                                if (entry.declaration.name == "Equal" && entry.referenceIndex == 1)
                                    entry.targetTriple = "different-target";
                            const auto changedGroups = groupCppSemanticRecords(changedTarget);
                            bool targetConflict = false;
                            for (const auto& group : changedGroups.groups)
                                targetConflict |= group.identity.name() == "Equal" && group.status == CppRecordGroupStatus::Conflict;
                            ok &= expect(targetConflict, "record grouping detects target mismatches");
                        }
                        recordGroups[1].sessions[0]->close();
                        auto closed = collectCppSemanticRecords(finalGraph, recordGroups);
                        const auto* closedError = std::get_if<CppSemanticRecordFailure>(&closed);
                        ok &= expect(closedError && closedError->projectFile == project->projectFile() &&
                                     closedError->referenceIndex == 0,
                                     "record inventory reports the owner and index of a closed session");
                        if (entries)
                        {
                            const auto grouped = groupCppSemanticRecords(*entries);
                            recordGroups[0].sessions[2]->close();
                            const auto partlyClosed = compareCppRecordDefinitions(*entries, grouped, recordGroups);
                            bool differenceWins = false;
                            if (const auto* results = std::get_if<std::vector<CppRecordStructuralComparison>>(&partlyClosed))
                                for (const auto& item : *results)
                                    differenceWins |= grouped.groups[item.groupIndex].identity.name() == "Same" &&
                                        item.verdict == CppRecordStructuralVerdict::Different;
                            recordGroups[0].sessions[1]->close();
                            const auto closedComparison = compareCppRecordDefinitions(*entries, grouped, recordGroups);
                            bool unavailable = false;
                            if (const auto* results = std::get_if<std::vector<CppRecordStructuralComparison>>(&closedComparison))
                                for (const auto& item : *results)
                                    unavailable |= grouped.groups[item.groupIndex].identity.name() == "Equal" &&
                                        item.verdict == CppRecordStructuralVerdict::Unavailable;
                            const auto closedRefinement = refineCppRecordGroupsForPointerTransport(*entries, grouped, recordGroups);
                            bool closedNotPromoted = false;
                            if (const auto* result = std::get_if<CppRecordGroups>(&closedRefinement))
                                for (const auto& record : result->groups)
                                    closedNotPromoted |= record.identity.name() == "Equal" &&
                                        record.status == CppRecordGroupStatus::MultipleDefinitionsUnverified;
                            ok &= expect(differenceWins && unavailable && closedNotPromoted,
                                         "record comparison preserves differences and reports closed-session uncertainty");
                        }
                        recordGroups[0].projectFile = fixture / "missing-owner.nsproj";
                        auto missingOwner = collectCppSemanticRecords(finalGraph, recordGroups);
                        ok &= expect(std::holds_alternative<CppSemanticRecordFailure>(missingOwner),
                                     "record inventory rejects a missing project owner");
                    }
                    auto namespaceInventory = collectCppNamespaces(finalGraph, groups);
                    const auto* namespaceEntries = std::get_if<std::vector<CppNamespaceInventoryEntry>>(
                        &namespaceInventory);
                    bool otherNamespace = false, featureNamespace = false, anonymousChild = false;
                    if (namespaceEntries)
                        for (const auto& entry : *namespaceEntries)
                        {
                            otherNamespace |= entry.declaration.qualifiedName == "other" &&
                                entry.projectOrdinal == 1 && entry.referenceIndex == 2 &&
                                entry.moduleName == otherProject->moduleName();
                            featureNamespace |= entry.declaration.qualifiedName == "feature" &&
                                entry.projectOrdinal == 2 && entry.referenceIndex == 0 &&
                                entry.moduleName == project->moduleName();
                            anonymousChild |= entry.projectOrdinal == 1 && entry.declaration.insideAnonymous;
                        }
                    ok &= expect(namespaceEntries && otherNamespace && featureNamespace && anonymousChild,
                                 "namespace inventory preserves project, reference and anonymous provenance");
                    if (namespaceEntries)
                    {
                        const auto visible = visibleCppNamespaces(*namespaceEntries);
                        const auto* names = std::get_if<std::vector<std::pair<std::size_t, std::string>>>(&visible);
                        const auto contains = [&](std::size_t owner, std::string_view name) {
                            return names && std::any_of(names->begin(), names->end(), [&](const auto& item) {
                                return item.first == owner && item.second == name;
                            });
                        };
                        ok &= expect(names && names->size() == 3 && contains(1, "other") &&
                                     contains(1, "other.deep") && contains(2, "feature"),
                                     "visible namespaces deduplicate reopened names and exclude anonymous descendants");
                    }
                    auto inventory = collectCppSemanticFunctions(finalGraph, groups);
                    const auto* entries = std::get_if<std::vector<CppSemanticInventoryEntry>>(&inventory);
                    bool otherFound = false, rejectedPointer = false, cppFound = false;
                    if (entries)
                    {
                        for (const auto& entry : *entries)
                        {
                            const auto* symbol = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(
                                &entry.projection);
                            if (entry.declaration.name == "other_value")
                                otherFound |= symbol && symbol->projectOrdinal == 1 &&
                                    symbol->moduleName == otherProject->moduleName() &&
                                    entry.projectFile == otherProject->projectFile() && entry.referenceIndex == 0;
                            if (entry.declaration.name == "pointer_value")
                                rejectedPointer = std::holds_alternative<CppSemanticFunctionFailure>(entry.projection);
                            if (entry.declaration.name == "value")
                                cppFound = symbol && symbol->projectOrdinal == 2 &&
                                    symbol->moduleName == project->moduleName() && symbol->namespaceName == "feature";
                        }
                    }
                    ok &= expect(entries && entries->size() == 6 && otherFound && rejectedPointer && cppFound,
                                 "inventory uses final graph owners and retains unsupported declarations");
                    if (entries)
                    {
                        auto grouped = groupCppSemanticFunctions(*entries);
                        const auto* groupedValue = std::get_if<CppSemanticFunctionGroups>(&grouped);
                        bool duplicateRetained = false, separateOwner = false;
                        if (groupedValue)
                        {
                            for (const auto& group : groupedValue->groups)
                            {
                                if (group.symbol.name == "other_value" && group.symbol.projectOrdinal == 1)
                                    duplicateRetained = group.inventoryIndices.size() == 2 &&
                                        (*entries)[group.inventoryIndices[0]].referenceIndex == 0 &&
                                        (*entries)[group.inventoryIndices[1]].referenceIndex == 1;
                                else if (group.symbol.name == "other_value" && group.symbol.projectOrdinal == 2)
                                    separateOwner = group.inventoryIndices.size() == 1;
                            }
                        }
                        ok &= expect(groupedValue && groupedValue->groups.size() == 4 &&
                                     groupedValue->unprojectedIndices.size() == 1 && duplicateRetained && separateOwner,
                                     "matching declarations group while origins and overloads remain distinct");
                        if (groupedValue && !groupedValue->groups.empty())
                        {
                            auto conflictingDll = *entries;
                            conflictingDll.push_back((*entries)[groupedValue->groups[0].inventoryIndices[0]]);
                            conflictingDll.back().declaration.dllImport = !conflictingDll.back().declaration.dllImport;
                            const auto conflict = selectCppSemanticFunctions(conflictingDll,
                                conflictingDll.back().projectOrdinal, conflictingDll.back().declaration.qualifiedName);
                            ok &= expect(std::holds_alternative<CppSemanticFunctionGroupFailure>(conflict),
                                         "function redeclarations with conflicting DLL import attributes are rejected");
                        }
                        const auto absent = selectCppSemanticFunctions(*entries, 1, "missing_function");
                        const auto* absentValue = std::get_if<CppSemanticFunctionGroups>(&absent);
                        ok &= expect(absentValue && absentValue->groups.empty() &&
                                     absentValue->unprojectedIndices.empty(),
                                     "lookup of an absent name returns no candidates");
                        const auto otherOwner = selectCppSemanticFunctions(*entries, 2, "other_value");
                        const auto* ownerValue = std::get_if<CppSemanticFunctionGroups>(&otherOwner);
                        ok &= expect(ownerValue && ownerValue->groups.size() == 1 &&
                                     ownerValue->groups[0].symbol.projectOrdinal == 2,
                                     "lookup does not mix owners of the same name");
                        bool everyLookupMatches = true;
                        for (const auto& entry : *entries)
                        {
                            const auto& qualified = entry.declaration.qualifiedName;
                            const auto separator = qualified.rfind("::");
                            nativesharp::frontend::CppFunctionDemand demand;
                            demand.lookupName = separator == std::string::npos ? qualified : qualified.substr(separator + 2);
                            std::string ns = separator == std::string::npos ? "" : qualified.substr(0, separator);
                            for (std::size_t p = 0; (p = ns.find("::", p)) != std::string::npos; ++p)
                                ns.replace(p, 2, ".");
                            demand.lookupNamespaces = {ns};
                            demand.visibleProjects = {entry.projectOrdinal};
                            const std::vector demands{demand};
                            const auto reference = selectCppSemanticFunctions(*entries, entry.projectOrdinal, qualified);
                            const auto indexed = selectCppSemanticDemands(*entries, demands);
                            const auto* referenceGroups = std::get_if<CppSemanticFunctionGroups>(&reference);
                            const auto* indexedGroups = std::get_if<std::vector<CppSemanticDemandCandidates>>(&indexed);
                            if (!referenceGroups || !indexedGroups || indexedGroups->size() != 1 ||
                                (*indexedGroups)[0].candidates.unprojectedIndices != referenceGroups->unprojectedIndices ||
                                (*indexedGroups)[0].candidates.groups.size() != referenceGroups->groups.size())
                            { everyLookupMatches = false; continue; }
                            for (std::size_t i = 0; i < referenceGroups->groups.size(); ++i)
                                everyLookupMatches &= (*indexedGroups)[0].candidates.groups[i].inventoryIndices ==
                                    referenceGroups->groups[i].inventoryIndices &&
                                    (*indexedGroups)[0].candidates.groups[i].symbol.mangledName ==
                                        referenceGroups->groups[i].symbol.mangledName;
                        }
                        ok &= expect(everyLookupMatches, "indexed lookup agrees for every real inventory declaration");
                    }
                    groups[1].sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                        CppLanguageMode::Cpp, "namespace feature { struct Opaque; int value(Opaque*); } namespace top { namespace inner { int f(); } }\n")));
                    auto overloadedInventory = collectCppSemanticFunctions(finalGraph, groups);
                    const auto* overloadedEntries = std::get_if<std::vector<CppSemanticInventoryEntry>>(
                        &overloadedInventory);
                    if (overloadedEntries)
                    {
                        auto selected = selectCppSemanticFunctions(*overloadedEntries, 2, "feature::value");
                        const auto* selectedValue = std::get_if<CppSemanticFunctionGroups>(&selected);
                        ok &= expect(selectedValue && selectedValue->groups.size() == 2 &&
                                     selectedValue->unprojectedIndices.size() == 1 &&
                                     (*overloadedEntries)[selectedValue->unprojectedIndices[0]].declaration.name == "value",
                                     "lookup retains representable and unprojected overloads with original indices");
                        nativesharp::frontend::CppFunctionDemand demand;
                        demand.lookupName = "value";
                        demand.lookupNamespaces = {"feature"};
                        demand.visibleProjects = {2};
                        auto inaccessible = demand;
                        inaccessible.visibleProjects = {1};
                        auto nested = demand;
                        nested.lookupName = "f";
                        nested.lookupNamespaces = {"top.inner"};
                        const std::vector demands{demand, demand, inaccessible, nested};
                        const auto selection = selectCppSemanticDemands(*overloadedEntries, demands);
                        const auto* matches = std::get_if<std::vector<CppSemanticDemandCandidates>>(&selection);
                        bool agreesWithReference = selectedValue && matches && !matches->empty() &&
                            (*matches)[0].candidates.unprojectedIndices == selectedValue->unprojectedIndices &&
                            (*matches)[0].candidates.groups.size() == selectedValue->groups.size();
                        if (agreesWithReference)
                            for (std::size_t i = 0; i < selectedValue->groups.size(); ++i)
                                agreesWithReference &= (*matches)[0].candidates.groups[i].inventoryIndices ==
                                    selectedValue->groups[i].inventoryIndices &&
                                    (*matches)[0].candidates.groups[i].symbol.mangledName ==
                                        selectedValue->groups[i].symbol.mangledName;
                        ok &= expect(matches && matches->size() == 3 && (*matches)[0].demandIndex == 0 &&
                                     (*matches)[1].demandIndex == 1 && (*matches)[0].projectOrdinal == 2 &&
                                     (*matches)[0].namespaceName == "feature" &&
                                     (*matches)[0].candidates.groups.size() == 2 &&
                                     (*matches)[0].candidates.unprojectedIndices.size() == 1 &&
                                     (*matches)[2].demandIndex == 3 && (*matches)[2].namespaceName == "top.inner" &&
                                     (*matches)[2].candidates.groups.size() == 1 &&
                                     (*matches)[2].candidates.groups[0].symbol.name == "f",
                                     "demand selection preserves overloads, visibility and demand provenance");
                        ok &= expect(agreesWithReference, "indexed demand selection agrees with the reference lookup");
                    }
                    else ok &= expect(false, "overloaded inventory is available");
                    groups[1].sessions.pop_back();
                    groups[0].sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                        CppLanguageMode::C, "int pointer_value(int p);\n")));
                    auto pointerConflict = collectCppSemanticFunctions(finalGraph, groups);
                    const auto* pointerConflictEntries = std::get_if<std::vector<CppSemanticInventoryEntry>>(
                        &pointerConflict);
                    ok &= expect(pointerConflictEntries && std::holds_alternative<CppSemanticFunctionGroupFailure>(
                                     groupCppSemanticFunctions(*pointerConflictEntries)),
                                 "conflict is detected even when one C signature is unprojected");
                    if (pointerConflictEntries)
                    {
                        const auto related = selectCppSemanticFunctions(*pointerConflictEntries, 1, "pointer_value");
                        const auto* conflict = std::get_if<CppSemanticFunctionGroupFailure>(&related);
                        const auto unrelated = selectCppSemanticFunctions(*pointerConflictEntries, 2, "feature::value");
                        const auto* unrelatedValue = std::get_if<CppSemanticFunctionGroups>(&unrelated);
                        ok &= expect(conflict && conflict->firstIndex < pointerConflictEntries->size() &&
                                     conflict->conflictingIndex < pointerConflictEntries->size() &&
                                     (*pointerConflictEntries)[conflict->firstIndex].declaration.name == "pointer_value" &&
                                     unrelatedValue && unrelatedValue->groups.size() == 2,
                                     "lookup reports relevant conflict by original indices without blocking other names");
                        nativesharp::frontend::CppFunctionDemand demand;
                        demand.lookupName = "pointer_value";
                        demand.lookupNamespaces = {""};
                        demand.visibleProjects = {1};
                        const std::vector demands{demand};
                        const auto demandSelection = selectCppSemanticDemands(*pointerConflictEntries, demands);
                        const auto* demandFailure = std::get_if<CppSemanticDemandFailure>(&demandSelection);
                        ok &= expect(conflict && demandFailure && demandFailure->demandIndex == 0 &&
                                     demandFailure->conflict.firstIndex == conflict->firstIndex &&
                                     demandFailure->conflict.conflictingIndex == conflict->conflictingIndex,
                                     "demand selection reports the originating conflict without losing indices");
                    }
                    groups[0].sessions.pop_back();
                    groups[0].sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                        CppLanguageMode::Cpp, "namespace alpha { extern \"C\" int alias(int); }\n")));
                    groups[0].sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                        CppLanguageMode::Cpp, "namespace beta { extern \"C\" double alias(int); }\n")));
                    auto aliasInventory = collectCppSemanticFunctions(finalGraph, groups);
                    const auto* aliasEntries = std::get_if<std::vector<CppSemanticInventoryEntry>>(&aliasInventory);
                    if (aliasEntries)
                    {
                        const auto aliasResult = selectCppSemanticFunctions(*aliasEntries, 1, "alpha::alias");
                        const auto* aliasFailure = std::get_if<CppSemanticFunctionGroupFailure>(&aliasResult);
                        ok &= expect(aliasFailure && aliasFailure->firstIndex < aliasEntries->size() &&
                                     aliasFailure->conflictingIndex < aliasEntries->size() &&
                                     (*aliasEntries)[aliasFailure->conflictingIndex].declaration.qualifiedName ==
                                         "beta::alias",
                                     "selection checks incompatible aliases of the same C symbol");
                        nativesharp::frontend::CppFunctionDemand demand;
                        demand.lookupName = "alias";
                        demand.lookupNamespaces = {"alpha"};
                        demand.visibleProjects = {1};
                        const std::vector demands{demand};
                        const auto indexed = selectCppSemanticDemands(*aliasEntries, demands);
                        const auto* indexedFailure = std::get_if<CppSemanticDemandFailure>(&indexed);
                        ok &= expect(aliasFailure && indexedFailure &&
                                     indexedFailure->conflict.firstIndex == aliasFailure->firstIndex &&
                                     indexedFailure->conflict.conflictingIndex == aliasFailure->conflictingIndex,
                                     "indexed selection preserves conflicts between names sharing a C symbol");
                        const auto delayed = selectCppSemanticDemands(*aliasEntries, demands, true);
                        const auto* delayedMatches = std::get_if<std::vector<CppSemanticDemandCandidates>>(&delayed);
                        ok &= expect(aliasFailure && delayedMatches && delayedMatches->size() == 1 &&
                                     delayedMatches->front().deferredConflict &&
                                     delayedMatches->front().deferredConflict->firstIndex == aliasFailure->firstIndex &&
                                     delayedMatches->front().deferredConflict->conflictingIndex == aliasFailure->conflictingIndex,
                                     "deferred selection preserves conflicts between aliases of one C symbol");
                    }
                    else ok &= expect(false, "alias inventory is available");
                    groups[0].sessions.pop_back();
                    groups[0].sessions.pop_back();
                    groups[0].sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                        CppLanguageMode::C, "double other_value(void);\n")));
                    auto conflictInventory = collectCppSemanticFunctions(finalGraph, groups);
                    const auto* conflictEntries = std::get_if<std::vector<CppSemanticInventoryEntry>>(
                        &conflictInventory);
                    ok &= expect(conflictEntries && std::holds_alternative<CppSemanticFunctionGroupFailure>(
                                     groupCppSemanticFunctions(*conflictEntries)),
                                 "separate Clang sessions with conflicting C signatures are not grouped");
                    std::vector<Project> duplicateOwners{*otherProject, *otherProject, *project};
                    ProjectGraph ambiguousGraph(project->projectFile(), std::move(duplicateOwners));
                    ok &= expect(std::holds_alternative<CppSemanticInventoryFailure>(
                                     collectCppSemanticFunctions(ambiguousGraph, groups)),
                                 "inventory rejects ambiguous project owners");
                    ok &= expect(std::holds_alternative<CppNamespaceInventoryFailure>(
                                     collectCppNamespaces(ambiguousGraph, groups)),
                                 "namespace inventory rejects ambiguous project owners");
                    std::vector<CppProjectSessions> invalidGroup(1);
                    invalidGroup[0].projectFile = fixture / "missing-owner.nsproj";
                    ok &= expect(std::holds_alternative<CppSemanticInventoryFailure>(
                                     collectCppSemanticFunctions(finalGraph, invalidGroup)),
                                 "inventory rejects missing project owners");
                    ok &= expect(std::holds_alternative<CppNamespaceInventoryFailure>(
                                     collectCppNamespaces(finalGraph, invalidGroup)),
                                 "namespace inventory rejects missing project owners");
                    invalidGroup[0].projectFile = project->projectFile();
                    invalidGroup[0].sessions.push_back(nullptr);
                    const auto nullResult = collectCppSemanticFunctions(finalGraph, invalidGroup);
                    const auto* nullFailure = std::get_if<CppSemanticInventoryFailure>(&nullResult);
                    ok &= expect(nullFailure && nullFailure->referenceIndex == 0 &&
                                 nullFailure->message.find("null") != std::string::npos,
                                 "inventory identifies a null session");
                    const auto nullNamespaceResult = collectCppNamespaces(finalGraph, invalidGroup);
                    const auto* nullNamespaceFailure = std::get_if<CppNamespaceInventoryFailure>(&nullNamespaceResult);
                    ok &= expect(nullNamespaceFailure && nullNamespaceFailure->referenceIndex == 0,
                                 "namespace inventory identifies a null session");
                    if (groups[0].sessions[0]) groups[0].sessions[0]->close();
                    ok &= expect(std::holds_alternative<CppSemanticInventoryFailure>(
                                     collectCppSemanticFunctions(finalGraph, groups)),
                                 "inventory rejects unavailable sessions without partial results");
                    ok &= expect(std::holds_alternative<CppNamespaceInventoryFailure>(
                                     collectCppNamespaces(finalGraph, groups)),
                                 "namespace inventory rejects unavailable sessions without partial results");
                }
                {
                    std::ostringstream driverOutput, driverErrors;
                    nativesharp::driver::DriverExecutionScope scope(driverOutput, driverErrors);
                    auto* previousScope = nativesharp::driver::activeExecutionScope;
                    nativesharp::driver::activeExecutionScope = &scope;
                    scope.cppSessionFactory = &factory;
                    CompilationInputManifest ordinaryManifest(combinedGraph, {});
                    nativesharp::frontend::ProjectCompilation ordinaryCompilation;
                    error.clear();
                    const bool ordinaryPrepared = nativesharp::driver::configurePrecompiledModules(
                        ordinaryManifest, ordinaryCompilation, error, false);
                    ok &= expect(ordinaryPrepared && ordinaryCompilation.cppProjectSessions().empty(),
                                 "driver without explicit environment does not create C/C++ sessions");
                    if (plainProject)
                    {
                        ProjectGraph plainGraph(plainProject->projectFile(), std::vector<Project>{*plainProject});
                        CompilationInputManifest plainManifest(plainGraph, {});
                        nativesharp::frontend::ProjectCompilation plainCompilation;
                        RejectUnexpectedOpen unexpectedFactory;
                        scope.cppSessionEnvironment = environment;
                        scope.cppSessionFactory = &unexpectedFactory;
                        error.clear();
                        const bool plainPrepared = nativesharp::driver::configurePrecompiledModules(
                            plainManifest, plainCompilation, error, false);
                        ok &= expect(plainPrepared && unexpectedFactory.calls == 0 &&
                                     plainCompilation.cppProjectSessions().empty(),
                                     "driver with C/C++ support and no references opens no Clang session");
                    }
                    scope.cppSessionEnvironment = environment;
                    scope.cppSessionFactory = nullptr;
                    CompilationInputManifest noFactoryManifest(combinedGraph, {});
                    nativesharp::frontend::ProjectCompilation noFactoryCompilation;
                    error.clear();
                    ok &= expect(!nativesharp::driver::configurePrecompiledModules(
                                     noFactoryManifest, noFactoryCompilation, error, false) &&
                                 error.find("requires a session factory") != std::string::npos,
                                 "driver rejects an explicit environment without a factory");
                    scope.cppSessionFactory = &factory;
                    CompilationInputManifest driverManifest(combinedGraph, {});
                    nativesharp::frontend::ProjectCompilation driverCompilation;
                    error.clear();
                    const bool prepared = nativesharp::driver::configurePrecompiledModules(
                        driverManifest, driverCompilation, error, false);
                    if (!prepared) std::cerr << "Driver preparation failed: " << error << '\n';
                    ok &= expect(prepared && driverCompilation.cppProjectSessions().size() == 2,
                                 "driver prepares real project sessions before compilation");
                    if (prepared)
                    {
                        error.clear();
                        ok &= expect(!driverCompilation.setCppImportedFunctions({}, error) &&
                                     error.find("function provider") != std::string::npos,
                                     "driver installs the C/C++ function provider with real sessions");
                    }
                    scope.cppSessionEnvironment->resourceDirectory /= "nonexistent-resource";
                    CompilationInputManifest failedManifest(combinedGraph, {});
                    nativesharp::frontend::ProjectCompilation failedCompilation;
                    error.clear();
                    ok &= expect(!nativesharp::driver::configurePrecompiledModules(
                                     failedManifest, failedCompilation, error, false) &&
                                 error.find("C/C++ session failed") != std::string::npos &&
                                 error.find("reference 0") != std::string::npos,
                                 "driver reports the project and reference when Clang preparation fails");
                    nativesharp::driver::activeExecutionScope = previousScope;
                }
                auto graphOpened = openCppGraphSessions(combinedGraph, environment, factory, {});
                auto* graphSessions = std::get_if<CppGraphSessions>(&graphOpened);
                ok &= expect(graphSessions && graphSessions->projects.size() == 2 &&
                             graphSessions->projects[0].projectFile == otherProject->projectFile() &&
                             graphSessions->projects[1].projectFile == project->projectFile() &&
                             graphSessions->projects[0].sessions.size() == 1 &&
                             graphSessions->projects[1].sessions.size() == 2,
                             "graph session groups retain project and reference order");
                if (graphSessions)
                {
                    nativesharp::frontend::ProjectCompilation graphOwner;
                    ok &= expect(graphOwner.setCppProjectSessions(std::move(graphSessions->projects), error) &&
                                 graphOwner.cppProjectSessions().size() == 2,
                                 "graph sessions transfer directly into frontend ownership");
                }
                FailThirdOpen failingFactory(factory);
                auto graphFailed = openCppGraphSessions(combinedGraph, environment, failingFactory, {});
                const auto* graphFailure = std::get_if<CppGraphSessionFailure>(&graphFailed);
                ok &= expect(graphFailure && graphFailure->projectFile == project->projectFile() &&
                             graphFailure->projectFailure.referenceIndex == 1 &&
                             graphFailure->projectFailure.failure.phase == "test",
                             "graph failure identifies the exact project and reference without partial output");
                CppCancellationToken graphCancelled;
                graphCancelled.cancel();
                auto graphCancelledResult = openCppGraphSessions(combinedGraph, environment, factory, graphCancelled);
                const auto* cancelledGraphFailure = std::get_if<CppGraphSessionFailure>(&graphCancelledResult);
                ok &= expect(cancelledGraphFailure && cancelledGraphFailure->projectFile == otherProject->projectFile() &&
                             cancelledGraphFailure->projectFailure.failure.phase == "cancel",
                             "graph session creation propagates cancellation at the owning project");
                auto first = openCppProjectSessions(*project, environment, factory, {});
                auto second = openCppProjectSessions(*otherProject, environment, factory, {});
                auto* firstGroup = std::get_if<CppProjectSessions>(&first);
                auto* secondGroup = std::get_if<CppProjectSessions>(&second);
                ok &= expect(firstGroup && secondGroup, "sessions for separate projects open");
                if (firstGroup && secondGroup)
                {
                    std::vector<CppProjectSessions> multiple;
                    multiple.push_back(std::move(*firstGroup));
                    multiple.push_back(std::move(*secondGroup));
                    nativesharp::frontend::ProjectCompilation multiOwner;
                    ok &= expect(multiOwner.setCppProjectSessions(std::move(multiple), error) &&
                                 multiOwner.cppProjectSessions().size() == 2 &&
                                 multiOwner.cppProjectSessions()[0].projectFile !=
                                     multiOwner.cppProjectSessions()[1].projectFile &&
                                 multiOwner.cppProjectSessions()[1].sessions[0]->optionsSnapshot().macros.size() == 1,
                                 "frontend keeps sessions and options separate for two projects");
                }
            }
            if (ownedSessions)
            {
                nativesharp::frontend::ProjectCompilation owner;
                std::vector<CppProjectSessions> groups;
                groups.push_back(std::move(*ownedSessions));
                error.clear();
                ok &= expect(owner.setCppProjectSessions(std::move(groups), error) && error.empty() &&
                             owner.cppProjectSessions().size() == 1 &&
                             owner.cppProjectSessions()[0].sessions.size() == 2 &&
                             owner.cppProjectSessions()[0].sessions[0]->topLevelDeclarationCount().has_value(),
                             "frontend retains real sessions for their project");
                std::vector<CppProjectSessions> duplicateGroups(2);
                duplicateGroups[0].projectFile = project->projectFile();
                duplicateGroups[1].projectFile = project->projectFile();
                ok &= expect(!owner.setCppProjectSessions(std::move(duplicateGroups), error) &&
                             owner.cppProjectSessions().size() == 1,
                             "duplicate owners are rejected without replacing existing sessions");
                std::vector<CppProjectSessions> missingOwner(1);
                ok &= expect(!owner.setCppProjectSessions(std::move(missingOwner), error) &&
                             owner.cppProjectSessions().size() == 1,
                             "missing owner is rejected without replacing existing sessions");
                std::vector<CppProjectSessions> nullSession(1);
                nullSession[0].projectFile = project->projectFile();
                nullSession[0].sessions.push_back(nullptr);
                ok &= expect(!owner.setCppProjectSessions(std::move(nullSession), error) &&
                             owner.cppProjectSessions().size() == 1,
                             "null session is rejected without replacing existing sessions");
                owner.compileProjectSyntax(projectPath, error);
                ok &= expect(!owner.setCppProjectSessions({}, error) && !error.empty() &&
                             owner.cppProjectSessions().size() == 1,
                             "project session sets cannot be replaced after compilation begins");
            }
        }
        ProjectGraphLoader graphLoader;
        auto graph = graphLoader.load(projectPath, error);
        if (!graph) std::cerr << "Graph load failed: " << error << '\n';
        ok &= expect(graph && graph->rootProject() && graph->rootProject()->cppLibraryReferences().size() == 2,
                     "project graph retains C/C++ references");
        if (graph && graph->rootProject() && graph->rootProject()->cppLibraryReferences().size() == 2)
        {
            const auto owner = graph->rootProject()->projectFile();
            const std::vector<LibraryReference> additional{{fixture / "record-symbols.lib", {}, {}, {}}};
            ok &= expect(graph->addLibraryReferences(owner, additional, error) && graph->rootProject() &&
                         graph->rootProject()->cppLibraryReferences().size() == 2,
                         "reconstructing a graph project preserves C/C++ references");
            const auto header = graph->rootProject()->cppLibraryReferences()[0].headerPath;
            const auto library = graph->rootProject()->cppLibraryReferences()[0].libraryPath;
            CompilationInputManifest manifest(std::move(*graph), {});
            const auto inputs = manifest.inputFiles();
            ok &= expect(std::find(inputs.begin(), inputs.end(), header) != inputs.end() &&
                         std::find(inputs.begin(), inputs.end(), library) != inputs.end(),
                         "manifest includes the header and binary library");
            LibraryMetadataRepository metadata;
            error.clear();
            ok &= expect(!manifest.calculateIdentity(metadata, error) &&
                         error.find("not yet connected") != std::string::npos,
                         "unconnected references cannot reuse a compilation cache");
        }
        for (const auto& [name, expected] : std::vector<std::pair<const char*, const char*>>{
                 {"BadLanguage.nsproj", "Language must be"},
                 {"MissingHeader.nsproj", "header does not exist"},
                 {"MissingLibrary.nsproj", "library"},
                 {"RuntimeWithoutLibrary.nsproj", "Runtime requires"}})
        {
            error.clear();
            const auto invalidProject = loader.load(fixture / "invalid" / name, error);
            if (invalidProject || error.find(expected) == std::string::npos)
                std::cerr << "Unexpected result for " << name << ": " << error << '\n';
            ok &= expect(!invalidProject && error.find(expected) != std::string::npos,
                         "invalid C/C++ reference is rejected for the expected reason");
        }
        nativesharp::frontend::ProjectCompilation compilation;
        error.clear();
        ok &= expect(!compilation.compileProjectSyntax(projectPath, error) &&
                     error.find("not yet connected") != std::string::npos,
                     "project compilation rejects references until integration is complete");
        if (project && project->cppLibraryReferences().size() == 2)
        {
            std::vector<std::filesystem::path> libraries;
            std::vector<std::filesystem::path> runtimes;
            const bool collected = nativesharp::driver::appendValidatedLibraryReferences(
                compilation, libraries, runtimes, error, nullptr);
            const bool collectedAgain = nativesharp::driver::appendValidatedLibraryReferences(
                compilation, libraries, runtimes, error, nullptr);
            ok &= expect(collected && collectedAgain && libraries.size() == 1 &&
                         libraries[0] == project->cppLibraryReferences()[0].libraryPath && runtimes.empty(),
                         "link inputs include each C/C++ archive once and omit header-only references");
        }
    }
    {
        const auto projectPath = std::filesystem::path("A:/llvm-project-al82dev/nativesharp-interop/pruebas-proyecto/CppProject.nsproj");
        nativesharp::compilation::ProjectGraphLoader loader;
        std::string error;
        auto graph = loader.load(projectPath, error);
        const CppProjectSessionEnvironment environment{
            "x86_64-pc-windows-msvc", argv[3], argv[1], argv[2], {}};
        auto opened = graph ? openCppGraphSessions(*graph, environment, factory, {}) :
            CppGraphSessionsResult{CppGraphSessionFailure{}};
        auto* sessions = std::get_if<CppGraphSessions>(&opened);
        nativesharp::frontend::ProjectCompilation compilation;
        const bool configured = sessions && compilation.setCppProjectSessions(std::move(sessions->projects), error) &&
            compilation.setCppNamespaceProvider([](const auto& graph, auto sessions, auto& output, auto& why) {
                auto inventory = collectCppNamespaces(graph, sessions);
                if (!std::holds_alternative<std::vector<CppNamespaceInventoryEntry>>(inventory)) {
                    why = "namespace inventory failed"; return false;
                }
                auto names = visibleCppNamespaces(std::get<std::vector<CppNamespaceInventoryEntry>>(inventory));
                if (!std::holds_alternative<std::vector<std::pair<std::size_t, std::string>>>(names)) {
                    why = "namespace projection failed"; return false;
                }
                output = std::move(std::get<std::vector<std::pair<std::size_t, std::string>>>(names));
                return true;
            }, error) && compilation.setCppFunctionProvider(
                [](const auto& graph, auto sessions, auto, auto demands, auto& projected, auto& deferred, auto& why) {
                    return provideCppSemanticFunctions(graph, sessions, demands, projected, deferred, why);
                }, error) && compilation.setCppGlobalProvider(
                [](const auto& graph, auto sessions, auto demands, auto& states, auto& why) {
                    return provideCppSemanticGlobals(graph, sessions, demands, states, why);
                }, error);
        const bool ready = configured && compilation.compileProjectOopMir(projectPath, error);
        if (!ready) std::cerr << "project reference MIR failed: " << error << '\n';
        bool imported = false;
        if (ready && compilation.oopMirModel())
            for (const auto& function : compilation.oopMirModel()->functions())
                imported |= function.isImportedC && function.mangledName == "project_sum";
        ok &= expect(ready && imported,
                     "actual project reference with real Clang sessions reaches MIR");
        if (graph)
        {
            auto missing = openCppGraphSessions(*graph, environment, factory, {});
            auto* available = std::get_if<CppGraphSessions>(&missing);
            nativesharp::frontend::ProjectCompilation noProviders;
            const bool installed = available && noProviders.setCppProjectSessions(std::move(available->projects), error);
            ok &= expect(installed && !noProviders.compileProjectSyntax(projectPath, error) &&
                         error.find("not yet connected") != std::string::npos,
                         "project references require both semantic providers");
            CppProjectSessions wrongGroup;
            wrongGroup.projectFile = std::filesystem::absolute(projectPath);
            auto wrongOptions = options(argv[1], argv[2], argv[3], CppLanguageMode::C, "int other(void);\n");
            wrongGroup.sessions.push_back(open(factory, std::move(wrongOptions)));
            std::vector<CppProjectSessions> wrongGroups;
            wrongGroups.push_back(std::move(wrongGroup));
            nativesharp::frontend::ProjectCompilation wrongHeader;
            const bool wrongConfigured = wrongHeader.setCppProjectSessions(std::move(wrongGroups), error) &&
                wrongHeader.setCppNamespaceProvider([](const auto&, auto, auto&, auto&) { return true; }, error) &&
                wrongHeader.setCppFunctionProvider([](const auto&, auto, auto, auto, auto&, auto&, auto&) {
                    return true;
                }, error);
            ok &= expect(wrongConfigured && !wrongHeader.compileProjectSyntax(projectPath, error) &&
                         error.find("not yet connected") != std::string::npos,
                         "project reference rejects a session for a different input");
        }
    }
    auto c = open(factory, options(argv[1], argv[2], argv[3], CppLanguageMode::C,
                                  "#include <stddef.h>\nint bridge_probe(size_t n, ...);\n"
                                  "int c_add(int left, int right);\n"
                                  "int ptr_read(int* p);\n"
                                  "int ptr_const(const int* p); int ptr_volatile(volatile int* p); int** ptr_nested(int** p);\n"
                                  "void* ptr_void(void* p); int ptr_nested_const(const int** p);\n"
                                  "int bridge_probe(size_t n, ...);\n"
                                  "struct Pair { char a; int b; }; struct Incomplete;\n"
                                  "extern int c_global; extern int c_global; static const int c_local = 3; "
                                  "extern volatile int c_volatile; extern const volatile float c_float;\n"
                                  "struct Flags { unsigned a:3; unsigned b:5; }; "
                                  "struct Later; struct Later { int value; };\n"));
    std::optional<std::vector<CppFunctionDeclaration>> cFunctions;
    std::optional<std::vector<CppRecordDeclaration>> cRecords;
    std::optional<std::vector<CppGlobalDeclaration>> cGlobals;
    ok &= expect(static_cast<bool>(c), "valid C translation unit opens");
    if (c)
    {
        const auto namespaces = c->namespacesSnapshot();
        ok &= expect(namespaces && namespaces->empty(), "C translation unit has no C++ namespaces");
        ok &= expect(c->state() == CppSessionState::Ready, "C session is ready");
        auto count = c->topLevelDeclarationCount();
        ok &= expect(count && *count > 0, "C AST is available after parsing");
        cFunctions = c->freeFunctionsSnapshot();
        const auto* probe = cFunctions ? functionByName(*cFunctions, "bridge_probe") : nullptr;
        ok &= expect(probe && probe->externalSymbol == "bridge_probe" && probe->variadic &&
                     probe->parameters.size() == 1 && !probe->parameters[0].canonicalTypeSpelling.empty() &&
                     probe->location.has_value() && !probe->canonicalFunctionType.empty() &&
                     probe->callingConvention == "cdecl" && probe->languageLinkage == CppLanguageLinkage::C,
                     "C function projection preserves symbol, signature and source");
        if (probe)
        {
            auto projected = projectCppSemanticFunction(*probe, 0, "Semantic");
            const auto* symbol = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(&projected);
            ok &= expect(symbol && symbol->isVariadic && symbol->parameters.size() == 1 &&
                         symbol->mangledName == "bridge_probe" &&
                         symbol->cppCanonicalIdentity.find("...") != std::string::npos,
                         "Clang C variadic signature projects with its external identity");
            if (symbol)
            {
                nativesharp::frontend::ProjectCompilation compilation;
                compilation.setSourceBufferProvider([](const std::filesystem::path& path)
                    -> std::optional<std::string> {
                    if (path.filename() == "Main.ns")
                        return "int Main() { byte x = 3; float y = 1.5f; return bridge_probe(2, x, y); }\n";
                    return std::nullopt;
                });
                std::string error;
                const auto path = std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/Semantic.nsproj";
                const bool ready = compilation.setCppImportedFunctions({*symbol}, error) &&
                    compilation.compileProjectOopMir(path, error);
                if (!ready)
                {
                    std::cerr << "C variadic MIR failed: " << error << '\n';
                    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
                        std::cerr << diagnostic.message() << '\n';
                }
                bool emitted = false;
                if (ready && compilation.oopMirModel() && compilation.nameLookup())
                {
                    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
                    auto module = lowerer.lower(*compilation.nameLookup(), *compilation.oopMirModel());
                    const bool valid = module && module->verify(error);
                    const auto ir = module ? module->text() : std::string{};
                    ok &= expect(valid && ir.find("@bridge_probe(i64, ...)") != std::string::npos,
                                 "C variadic declaration reaches LLVM with its fixed argument");
                    auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(error);
                    emitted = valid && target && target->emit(*module,
                        std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/variadic-call.obj",
                        nativesharp::llvm_backend::LlvmCodeGenFileType::Object, error);
                    if (!emitted) std::cerr << "C variadic object failed: " << lowerer.error() << ' ' << error << '\n';
                }
                ok &= expect(emitted, "C variadic call emits a Windows x64 object");
            }
        }
        if (cFunctions)
            ok &= expect(std::count_if(cFunctions->begin(), cFunctions->end(), [](const auto& value) {
                return value.qualifiedName == "bridge_probe";
            }) == 1, "repeated declarations share one canonical function");
        cRecords = c->recordsSnapshot();
        cGlobals = c->globalsSnapshot();
        const auto* global = cGlobals ? globalByName(*cGlobals, "c_global") : nullptr;
        const auto* local = cGlobals ? globalByName(*cGlobals, "c_local") : nullptr;
        const auto* volatileGlobal = cGlobals ? globalByName(*cGlobals, "c_volatile") : nullptr;
        const auto* qualifiedFloat = cGlobals ? globalByName(*cGlobals, "c_float") : nullptr;
        ok &= expect(global && global->externalSymbol == "c_global" && global->type.sizeBits == 32 &&
                     global->location && std::count_if(cGlobals->begin(), cGlobals->end(), [](const auto& value) {
                         return value.qualifiedName == "c_global";
                     }) == 1, "C global declaration preserves symbol, type and canonical identity");
        ok &= expect(local && !local->externalLinkage && !local->externalSymbol && local->type.isConst,
                     "internal C global preserves const without an external symbol");
        const auto globalMapping = global ? mapCppScalarType(global->type) : std::nullopt;
        const auto constMapping = local ? mapCppScalarType(local->type) : std::nullopt;
        const auto volatileMapping = volatileGlobal ? mapCppScalarType(volatileGlobal->type) : std::nullopt;
        const auto floatMapping = qualifiedFloat ? mapCppScalarType(qualifiedFloat->type) : std::nullopt;
        ok &= expect(globalMapping && globalMapping->nativeSharpTypeKey == "builtin:int" &&
                     !globalMapping->isConst && !globalMapping->isVolatile && constMapping &&
                     constMapping->nativeSharpTypeKey == "builtin:int" && constMapping->isConst &&
                     volatileMapping && volatileMapping->nativeSharpTypeKey == "builtin:int" &&
                     volatileMapping->isVolatile && floatMapping && floatMapping->isConst &&
                     floatMapping->isVolatile && floatMapping->nativeSharpTypeKey == "builtin:float",
                     "scalar representation mapping retains const and volatile from Clang");
        const auto* pointerRead = cFunctions ? functionByName(*cFunctions, "ptr_read") : nullptr;
        const auto* pointerConst = cFunctions ? functionByName(*cFunctions, "ptr_const") : nullptr;
        const auto* pointerVolatile = cFunctions ? functionByName(*cFunctions, "ptr_volatile") : nullptr;
        const auto* pointerNested = cFunctions ? functionByName(*cFunctions, "ptr_nested") : nullptr;
        const auto* pointerVoid = cFunctions ? functionByName(*cFunctions, "ptr_void") : nullptr;
        const auto* pointerNestedConst = cFunctions ? functionByName(*cFunctions, "ptr_nested_const") : nullptr;
        ok &= expect(pointerVolatile && pointerNestedConst &&
                     std::holds_alternative<CppSemanticFunctionFailure>(projectCppSemanticFunction(*pointerVolatile, 0, "Semantic")) &&
                     std::holds_alternative<CppSemanticFunctionFailure>(projectCppSemanticFunction(*pointerNestedConst, 0, "Semantic")),
                     "qualified pointees are not silently mapped to mutable pointers");
        if (pointerConst)
        {
            const auto projectedConst = projectCppSemanticFunction(*pointerConst, 0, "Semantic");
            const auto* constSymbol = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(&projectedConst);
            ok &= expect(constSymbol && constSymbol->parameters.size() == 1 &&
                         constSymbol->parameters[0].typeKey == "builtin:int*" &&
                         constSymbol->parameters[0].pointerPointeeReadonly,
                         "C const pointer parameter preserves pointee readonly");
            if (constSymbol && pointerRead)
            {
                const auto mutableProjection = projectCppSemanticFunction(*pointerRead, 0, "Semantic");
                const auto* mutableSymbol = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(&mutableProjection);
                for (const bool readonlyTarget : {true, false})
                {
                    nativesharp::frontend::ProjectCompilation compilation;
                    compilation.setSourceBufferProvider([readonlyTarget](const std::filesystem::path& path)
                        -> std::optional<std::string> {
                        if (path.filename() == "Main.ns")
                            return readonlyTarget
                                ? "int Main() { int value = 41; int const* p = &value; return ptr_const(p); }\n"
                                : "int Main() { int value = 41; int const* p = &value; return ptr_read(p); }\n";
                        return std::nullopt;
                    });
                    std::string error;
                    const auto path = std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/Semantic.nsproj";
                    const bool configured = mutableSymbol && compilation.setCppImportedFunctions(
                        {*constSymbol, *mutableSymbol}, error);
                    const bool ready = configured && (readonlyTarget
                        ? compilation.compileProjectOopMir(path, error)
                        : compilation.compileProjectMethodBodies(path, error));
                    bool expectedDiagnostic = false;
                    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
                        expectedDiagnostic |= diagnostic.message().find("no viable overload of namespace function 'ptr_read'") !=
                            std::string::npos;
                    ok &= expect(configured && (readonlyTarget ? ready && !compilation.diagnostics().hasErrors()
                                                                : !ready && expectedDiagnostic),
                                 readonlyTarget ? "readonly pointer argument calls a const C parameter"
                                                : "readonly pointer argument cannot call a mutable C parameter");
                    if (readonlyTarget && ready && compilation.nameLookup() && compilation.oopMirModel())
                    {
                        nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
                        auto module = lowerer.lower(*compilation.nameLookup(), *compilation.oopMirModel());
                        std::string llvmError;
                        const bool valid = module && module->verify(llvmError);
                        auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(llvmError);
                        const bool emitted = valid && target && target->emit(*module,
                            std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/const-pointer-call.obj",
                            nativesharp::llvm_backend::LlvmCodeGenFileType::Object, llvmError);
                        if (!emitted) std::cerr << "Const pointer LLVM/object failed: " << lowerer.error() << ' ' << llvmError << '\n';
                        ok &= expect(emitted, "readonly pointer call emits a verified Windows x64 object");
                    }
                }
            }
        }
        else ok &= expect(false, "Clang exposes the const pointer parameter fixture");
        if (pointerVoid)
        {
            const auto projectedVoid = projectCppSemanticFunction(*pointerVoid, 0, "Semantic");
            const auto* voidSymbol = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(&projectedVoid);
            ok &= expect(voidSymbol && voidSymbol->declaredTypeKey == "builtin:void*" &&
                         voidSymbol->parameters.size() == 1 && voidSymbol->parameters[0].typeKey == "builtin:void*",
                         "void pointer return and parameter retain pointer type");
        }
        else ok &= expect(false, "Clang exposes the void pointer fixture");
        if (pointerNested)
        {
            const auto nested = projectCppSemanticFunction(*pointerNested, 0, "Semantic");
            const auto* nestedSymbol = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(&nested);
            ok &= expect(nestedSymbol && nestedSymbol->declaredTypeKey == "builtin:int**" &&
                         nestedSymbol->parameters.size() == 1 && nestedSymbol->parameters[0].typeKey == "builtin:int**",
                         "nested pointer return and parameter retain both levels");
        }
        else ok &= expect(false, "Clang exposes the nested pointer fixture");
        if (pointerRead)
        {
            const auto projectedPointer = projectCppSemanticFunction(*pointerRead, 0, "Semantic");
            const auto* symbol = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(&projectedPointer);
            ok &= expect(symbol && symbol->parameters.size() == 1 &&
                         symbol->parameters[0].typeKey == "builtin:int*",
                         "C pointer to int maps to a NativeSharp pointer parameter");
            if (symbol)
            {
                nativesharp::frontend::ProjectCompilation pointerCompilation;
                pointerCompilation.setSourceBufferProvider([](const std::filesystem::path& path)
                    -> std::optional<std::string> {
                    if (path.filename() == "Main.ns")
                        return "int Main() { int value = 41; return ptr_read(&value); }\n";
                    return std::nullopt;
                });
                std::string error;
                const auto path = std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/Semantic.nsproj";
                const bool ready = pointerCompilation.setCppImportedFunctions({*symbol}, error) &&
                    pointerCompilation.compileProjectOopMir(path, error);
                if (!ready) std::cerr << "C pointer MIR failed: " << error << '\n';
                ok &= expect(ready && pointerCompilation.oopMirModel(),
                             "C pointer call reaches MIR from NativeSharp");
                if (ready && pointerCompilation.oopMirModel() && pointerCompilation.nameLookup())
                {
                    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
                    auto module = lowerer.lower(*pointerCompilation.nameLookup(), *pointerCompilation.oopMirModel());
                    std::string llvmError;
                    const bool valid = module && module->verify(llvmError);
                    const std::string ir = module ? module->text() : std::string{};
                    ok &= expect(valid && ir.find("declare i32 @ptr_read(ptr)") != std::string::npos &&
                                 ir.find("call i32 @ptr_read(ptr") != std::string::npos,
                                 "C pointer call reaches verified LLVM with pointer ABI");
                    auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(llvmError);
                    const bool emitted = valid && target && target->emit(*module,
                        std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/pointer-call.obj",
                        nativesharp::llvm_backend::LlvmCodeGenFileType::Object, llvmError);
                    if (!emitted) std::cerr << "C pointer LLVM/object failed: " << lowerer.error() << ' ' << llvmError << '\n';
                    ok &= expect(emitted, "C pointer call emits a Windows x64 object");
                }
            }
        }
        else ok &= expect(false, "Clang exposes the pointer parameter fixture");
        const auto* add = cFunctions ? functionByName(*cFunctions, "c_add") : nullptr;
        if (add && add->externalSymbol && add->parameters.size() == 2)
        {
            ok &= expect(std::holds_alternative<CppSemanticFunctionFailure>(
                             projectCppSemanticFunction(*add, 0, "")),
                         "semantic conversion requires an owning module");
            auto projected = projectCppSemanticFunction(*add, 0, "Semantic");
            auto* projectedSymbol = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(&projected);
            ok &= expect(projectedSymbol != nullptr, "C function converts automatically for semantic indexing");
            if (projectedSymbol)
            {
                const auto imported = *projectedSymbol;
                {
                    nativesharp::frontend::ProjectCompilation bodies;
                    std::string bodyError;
                    const auto path = std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/Semantic.nsproj";
                    const bool configured = bodies.setCppImportedFunctions({imported}, bodyError);
                    const bool ready = configured && bodies.compileProjectMethodBodies(path, bodyError);
                    if (!ready)
                        for (const auto& diagnostic : bodies.diagnostics().diagnostics())
                            std::cerr << "C++ import body diagnostic: " << diagnostic.message() << '\n';
                    ok &= expect(configured && ready && !bodies.diagnostics().hasErrors(),
                                 "Clang function is recognized as external in body analysis");
                }
                {
                    nativesharp::frontend::ProjectCompilation hir;
                    std::string hirError;
                    const auto path = std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/Semantic.nsproj";
                    const bool ready = hir.setCppImportedFunctions({imported}, hirError) &&
                        hir.compileProjectOopHir(path, hirError);
                    bool found = false;
                    if (ready && hir.oopHirModel())
                        for (const auto& function : hir.oopHirModel()->functions())
                            found |= function.isImportedC && function.mangledName == "c_add" &&
                                function.cAbiName == "c_add";
                    if (!ready) std::cerr << "Clang C HIR setup failed: " << hirError << '\n';
                    ok &= expect(found, "Clang C external symbol reaches HIR as a declaration");
                    nativesharp::frontend::ProjectCompilation mir;
                    const bool mirReady = mir.setCppImportedFunctions({imported}, hirError) &&
                        mir.compileProjectOopMir(path, hirError);
                    bool mirFound = false;
                    if (mirReady && mir.oopMirModel())
                        for (const auto& function : mir.oopMirModel()->functions())
                            mirFound |= function.isImportedC && function.mangledName == "c_add" &&
                                function.cAbiName == "c_add";
                    if (!mirReady) std::cerr << "Clang C MIR setup failed: " << hirError << '\n';
                    ok &= expect(mirFound, "Clang C external symbol reaches MIR as a declaration");
                    if (mirFound && mir.nameLookup())
                    {
                        nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
                        auto module = lowerer.lower(*mir.nameLookup(), *mir.oopMirModel());
                        std::string verifyError;
                        const bool valid = module && module->verify(verifyError);
                        const std::string ir = module ? module->text() : std::string{};
                        if (!valid) std::cerr << "Clang C LLVM failed: " << lowerer.error() << ' ' << verifyError << '\n';
                        ok &= expect(valid && ir.find("declare i32 @c_add(i32, i32)") != std::string::npos &&
                                     ir.find("call i32 @c_add(") != std::string::npos,
                                     "Clang C external declaration and call reach verified LLVM");
                        auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(verifyError);
                        const bool emitted = valid && target && target->emit(*module,
                            std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/c-call.obj",
                            nativesharp::llvm_backend::LlvmCodeGenFileType::Object, verifyError);
                        if (!emitted) std::cerr << "Clang C object failed: " << verifyError << '\n';
                        ok &= expect(emitted, "Clang C call emits a Windows x64 object");
                    }
                }
                {
                    nativesharp::frontend::ProjectCompilation ordinaryPrototype;
                    ordinaryPrototype.setSourceBufferProvider([](const std::filesystem::path& path)
                        -> std::optional<std::string> {
                        if (path.filename() == "Main.ns")
                            return "int missing(int value); int Main() { return missing(1); }\n";
                        return std::nullopt;
                    });
                    std::string bodyError;
                    const auto path = std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/Semantic.nsproj";
                    const bool ready = ordinaryPrototype.compileProjectMethodBodies(path, bodyError);
                    bool missingDefinition = false;
                    for (const auto& diagnostic : ordinaryPrototype.diagnostics().diagnostics())
                        missingDefinition |= diagnostic.id() == nativesharp::diagnostics::DiagnosticId::MissingFunctionDefinition;
                    ok &= expect(!ready && missingDefinition,
                                 "ordinary NativeSharp prototype still requires a definition when called");
                }
                nativesharp::frontend::ProjectCompilation compilation;
                std::string error;
                const auto semanticPath = std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/Semantic.nsproj";
                auto incompleteProjection = imported;
                incompleteProjection.mangledName.clear();
                ok &= expect(!compilation.setCppImportedFunctions({incompleteProjection}, error) &&
                             !compilation.setCppImportedFunctions({imported, imported}, error) &&
                             compilation.setCppImportedFunctions({imported}, error),
                             "compilation validates imported symbols before starting");
                const bool nativeReady = compilation.compileProjectMembers(semanticPath, error);
                if (!nativeReady) std::cerr << "Native semantic setup failed: " << error << '\n';
                ok &= expect(nativeReady && compilation.nameLookup() && compilation.memberModel(),
                             "ordinary project provides semantic models for C import test");
                if (nativeReady && compilation.nameLookup() && compilation.memberModel())
                {
                    const auto* compiledModel = compilation.namespaceFunctionModel();
                    ok &= expect(compiledModel && compiledModel->findFunctions(0, "", "c_add").size() == 1 &&
                                 !compilation.setCppImportedFunctions({}, error),
                                 "project compilation installs imported functions and rejects late changes");
                    nativesharp::frontend::ProjectNamespaceFunctionModel model;
                    auto incomplete = imported; incomplete.mangledName.clear();
                    ok &= expect(!model.setCppImportedFunctions({incomplete}, error),
                                 "semantic index rejects incomplete imported symbols");
                    ok &= expect(!model.setCppImportedFunctions({imported, imported}, error),
                                 "semantic index rejects duplicate Clang identities");
                    ok &= expect(model.setCppImportedFunctions({imported}, error) &&
                                 model.build(*compilation.nameLookup(), *compilation.memberModel(),
                                             compilation.units(), compilation.diagnostics(), compilation.genericModel()),
                                 "semantic model builds with a function projected from Clang");
                    const auto ordinals = model.findFunctions(0, "", "c_add");
                    ok &= expect(ordinals.size() == 1 && ordinals[0] < model.functions().size() &&
                                 model.functions()[ordinals[0]].mangledName == "c_add" &&
                                 model.functions()[ordinals[0]].cppCallingConvention == "cdecl" &&
                                 model.functions()[ordinals[0]].cppCanonicalIdentity.starts_with("c_add|"),
                                 "semantic lookup finds imported C function and retains its symbol");
                    ok &= expect(model.findFunctions(0, "", "Main").size() == 1,
                                 "importing a function preserves ordinary NativeSharp functions");
                    const auto bindings = model.referenceBindings();
                    const auto bound = std::find_if(bindings.begin(), bindings.end(), [](const auto& binding) {
                        return binding.writtenName == "c_add";
                    });
                    ok &= expect(bound != bindings.end() && bound->candidateFunctionOrdinals.size() == 1 &&
                                 ordinals.size() == 1 && bound->candidateFunctionOrdinals[0] == ordinals[0],
                                 "NativeSharp call name binds to the Clang-projected candidate");
                    nativesharp::frontend::ProjectNamespaceFunctionModel observer;
                    const bool observed = observer.setCaptureCppFunctionDemands(true) &&
                        observer.setCppImportedFunctions({imported}, error) &&
                        observer.build(*compilation.nameLookup(), *compilation.memberModel(), compilation.units(),
                                       compilation.diagnostics(), compilation.genericModel());
                    bool directDemand = false;
                    if (observed)
                        for (const auto& demand : observer.cppFunctionDemands())
                            directDemand |= demand.directCall && demand.writtenName == "c_add" &&
                                demand.projectOrdinal == 0 && demand.syntax != nullptr;
                    ok &= expect(directDemand, "demand capture includes an unqualified direct C call");
                    ok &= expect(!model.setCppImportedFunctions({}, error),
                                 "semantic imports cannot change after model construction");
                    nativesharp::frontend::ProjectNamespaceFunctionModel badOwnerModel;
                    nativesharp::diagnostics::DiagnosticEngine badOwnerDiagnostics;
                    auto badOwner = imported;
                    badOwner.projectOrdinal = 999999;
                    ok &= expect(badOwnerModel.setCppImportedFunctions({badOwner}, error) &&
                                 !badOwnerModel.build(*compilation.nameLookup(), *compilation.memberModel(),
                                     compilation.units(), badOwnerDiagnostics, compilation.genericModel()) &&
                                 badOwnerDiagnostics.hasErrors() &&
                                 badOwnerModel.findFunctions(badOwner.projectOrdinal, "", "c_add").empty(),
                                 "semantic index rejects an owner outside the project graph");
                }
                {
                    nativesharp::frontend::ProjectCompilation deferred;
                    CppProjectSessions group;
                    group.projectFile = std::filesystem::absolute(semanticPath);
                    group.sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                        CppLanguageMode::C, "int c_add(int left, int right);\n")));
                    std::vector<CppProjectSessions> groups;
                    groups.push_back(std::move(group));
                    std::size_t invocations = 0;
                    bool sawSyntax = false;
                    bool sawDemand = false;
                    auto provider = [&invocations, &sawSyntax, &sawDemand](const auto& graph, auto sessions, auto units,
                                                               auto demands, auto& output, auto&, auto& why) {
                        ++invocations;
                        sawSyntax = !units.empty() && units.front() && units.front()->syntaxTree();
                        for (const auto& demand : demands)
                            sawDemand |= demand.directCall && demand.writtenName == "c_add" &&
                                demand.projectOrdinal == 0 && demand.syntax != nullptr &&
                                demand.lookupName == "c_add" && demand.lookupNamespaces.size() == 1 &&
                                demand.lookupNamespaces[0].empty() &&
                                std::find(demand.visibleProjects.begin(), demand.visibleProjects.end(), 0) !=
                                    demand.visibleProjects.end();
                        auto inventory = collectCppSemanticFunctions(graph, sessions);
                        if (const auto* failure = std::get_if<CppSemanticInventoryFailure>(&inventory))
                        { why = failure->message; return false; }
                        for (auto& entry : std::get<std::vector<CppSemanticInventoryEntry>>(inventory))
                        {
                            auto* symbol = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(
                                &entry.projection);
                            if (!symbol) { why = "unrepresented declaration in test"; return false; }
                            output.push_back(std::move(*symbol));
                        }
                        return true;
                    };
                    nativesharp::frontend::ProjectCompilation syntaxOnly;
                    ok &= expect(syntaxOnly.setCppFunctionProvider(provider, error) &&
                                 syntaxOnly.compileProjectSyntax(semanticPath, error) && invocations == 0,
                                 "syntax-only compilation does not invoke the semantic function provider");
                    ok &= expect(deferred.setCppProjectSessions(std::move(groups), error) &&
                                 deferred.setCppFunctionProvider(provider, error) &&
                                 !deferred.setCppImportedFunctions({imported}, error),
                                 "deferred provider and direct symbols are mutually exclusive");
                    error.clear();
                    const bool deferredReady = deferred.compileProjectMembers(semanticPath, error);
                    if (!deferredReady) std::cerr << "Deferred semantic setup failed: " << error << '\n';
                    ok &= expect(deferredReady && invocations == 1 && sawSyntax && sawDemand && deferred.namespaceFunctionModel() &&
                                 deferred.namespaceFunctionModel()->findFunctions(0, "", "c_add").size() == 1 &&
                                 !deferred.setCppFunctionProvider({}, error),
                                 "provider projects real Clang functions after the graph is prepared");
                    nativesharp::frontend::ProjectCompilation failedProvider;
                    ok &= expect(failedProvider.setCppFunctionProvider(
                        [](const auto&, auto, auto, auto, auto&, auto&, auto& why) { why = "provider test failure"; return false; },
                        error), "failing provider installs before compilation");
                    error.clear();
                    ok &= expect(!failedProvider.compileProjectMembers(semanticPath, error) &&
                                 error == "provider test failure",
                                 "provider failure stops compilation and preserves its diagnostic");
                    nativesharp::frontend::ProjectCompilation duplicateDefinition;
                    duplicateDefinition.setSourceBufferProvider([](const std::filesystem::path& path)
                        -> std::optional<std::string> {
                        if (path.filename() == "Main.ns")
                            return "int duplicate() { return 1; } int duplicate() { return 2; } int Main() { return 0; }\n";
                        return std::nullopt;
                    });
                    bool duplicateProviderCalled = false;
                    ok &= expect(duplicateDefinition.setCppFunctionProvider(
                        [&duplicateProviderCalled](const auto&, auto, auto, auto, auto&, auto&, auto&) {
                            duplicateProviderCalled = true; return true;
                        }, error), "duplicate-definition test installs a provider");
                    error.clear();
                    const bool duplicateReady = duplicateDefinition.compileProjectMembers(semanticPath, error);
                    bool duplicateReported = false;
                    for (const auto& diagnostic : duplicateDefinition.diagnostics().diagnostics())
                        duplicateReported |= diagnostic.message().find("already has a definition") != std::string::npos;
                    ok &= expect(!duplicateReady && !duplicateProviderCalled && duplicateReported,
                                 "observer failure preserves real diagnostics without invoking provider");
                    nativesharp::frontend::ProjectCompilation invalidProvider;
                    ok &= expect(invalidProvider.setCppFunctionProvider(
                        [incompleteProjection](const auto&, auto, auto, auto, auto& output, auto&, auto&) {
                            output.push_back(incompleteProjection); return true;
                        }, error), "provider of invalid symbols installs before compilation");
                    error.clear();
                    ok &= expect(!invalidProvider.compileProjectMembers(semanticPath, error) &&
                                 error.find("incomplete imported") != std::string::npos,
                                 "provider output is validated before semantic installation");
                    nativesharp::frontend::ProjectCompilation directFirst;
                    ok &= expect(directFirst.setCppImportedFunctions({imported}, error) &&
                                 !directFirst.setCppFunctionProvider(provider, error),
                                 "direct symbols also prevent a conflicting provider");
                }
            }
        }
        else ok &= expect(false, "Clang exposes the C function used in semantic test");
        const auto* pair = cRecords ? recordByName(*cRecords, "Pair") : nullptr;
        const auto* incomplete = cRecords ? recordByName(*cRecords, "Incomplete") : nullptr;
        const auto* flags = cRecords ? recordByName(*cRecords, "Flags") : nullptr;
        const auto* later = cRecords ? recordByName(*cRecords, "Later") : nullptr;
        ok &= expect(pair && pair->complete && pair->odrHash && pair->sizeBits == 64 && pair->fields.size() == 2 &&
                     pair->fields[0].offsetBits == 0 && pair->fields[1].offsetBits == 32,
                     "C struct size and field offsets match the target");
        ok &= expect(incomplete && !incomplete->complete && !incomplete->sizeBits && !incomplete->odrHash,
                     "incomplete record has no invented layout");
        ok &= expect(flags && flags->fields.size() == 2 && flags->fields[0].offsetBits == 0 &&
                     flags->fields[0].bitWidth == 3 && flags->fields[1].offsetBits == 3 &&
                     flags->fields[1].bitWidth == 5, "bitfield widths and offsets come from Clang");
        ok &= expect(later && later->complete && later->sizeBits == 32 &&
                     std::count_if(cRecords->begin(), cRecords->end(), [](const auto& value) {
                         return value.qualifiedName == "Later";
                     }) == 1, "forward declaration resolves once to its definition");
    }
    auto namespaceSession = open(factory, options(argv[1], argv[2], argv[3], CppLanguageMode::Cpp,
        "namespace empty {} namespace reopened { namespace first {} } "
        "namespace reopened { namespace second {} } inline namespace versioned {} "
        "namespace { namespace hidden {} }\n"));
    ok &= expect(static_cast<bool>(namespaceSession), "namespace fixture opens");
    if (namespaceSession)
    {
        const auto namespaces = namespaceSession->namespacesSnapshot();
        auto count = [&](std::string_view name) {
            return namespaces ? std::count_if(namespaces->begin(), namespaces->end(), [&](const auto& value) {
                return value.qualifiedName == name;
            }) : 0;
        };
        bool inlineFound = false, anonymousFound = false, hiddenFound = false, allLocated = namespaces.has_value();
        if (namespaces)
            for (const auto& value : *namespaces)
            {
                inlineFound |= value.qualifiedName == "versioned" && value.isInline;
                anonymousFound |= value.isAnonymous && !value.insideAnonymous;
                hiddenFound |= !value.isAnonymous && value.insideAnonymous &&
                    value.qualifiedName.find("hidden") != std::string::npos;
                allLocated &= value.location.has_value();
            }
        ok &= expect(namespaces && count("empty") == 1 && count("reopened") == 1 &&
                     count("reopened::first") == 1 && count("reopened::second") == 1 &&
                     inlineFound && anonymousFound && hiddenFound && allLocated,
                     "namespace snapshot keeps empty, reopened, nested, inline and anonymous declarations");
        namespaceSession->close();
        ok &= expect(!namespaceSession->namespacesSnapshot() && namespaces && count("reopened::second") == 1,
                     "copied namespace declarations survive session closure");
    }
    return ok ? 0 : 1;
}
