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
    return ok ? 0 : 1;
}
