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
    return ok ? 0 : 1;
}
