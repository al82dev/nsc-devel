#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Diagnostics/Diagnostic.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectNamespaceFunctionModel.h"
#include "nativesharp/Frontend/ProjectOopHirModelDumper.h"
#include "nativesharp/Frontend/ProjectOopMirModelDumper.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"
#include "nativesharp/Interop/LibraryMetadata.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string_view>

namespace
{
void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}

void writeCoffArchive(
    const std::filesystem::path& path,
    std::string_view symbol)
{
    std::string payload(8, '\0');
    payload[3] = 1;
    payload.append(symbol);
    payload.push_back('\0');

    std::string archive = "!<arch>\n";
    const auto appendField = [&](std::string_view value, std::size_t width)
    {
        archive.append(value);
        archive.append(width - value.size(), ' ');
    };
    appendField("/", 16);
    appendField("0", 12);
    appendField("0", 6);
    appendField("0", 6);
    appendField("0", 8);
    appendField(std::to_string(payload.size()), 10);
    archive.push_back(static_cast<char>(96));
    archive.push_back('\n');
    archive.append(payload);
    if ((payload.size() & 1U) != 0U) archive.push_back('\n');
    writeFile(path, archive);
}

std::filesystem::path createProject(
    const std::filesystem::path& root,
    std::string_view source)
{
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root, error);
    writeFile(
        root / "State.nsproj",
        R"(<Project><PropertyGroup><Name>State</Name></PropertyGroup><ItemGroup><Compile Include="**/*.ns" /></ItemGroup></Project>)");
    writeFile(root / "State.ns", source);
    return root / "State.nsproj";
}

bool hasDiagnostic(
    const nativesharp::frontend::ProjectCompilation& compilation,
    nativesharp::diagnostics::DiagnosticId id)
{
    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
    {
        if (diagnostic.id() == id) return true;
    }
    return false;
}
}

int main()
{
    using nativesharp::diagnostics::DiagnosticId;
    using namespace nativesharp::frontend;
    nativesharp::tests::TestContext tests;
    const std::filesystem::path root =
        std::filesystem::current_path() / "_namespace_state_model_test";
    std::error_code errorCode;

    {
        ProjectCompilation compilation;
        std::string error;
        const auto project = createProject(
            root / "valid",
            "namespace Numeric { public const int Answer = 40 + 2; int Counter; "
            "internal bool Enabled = true; }");
        const bool ok = compilation.compileProjectMembers(project, error);
        tests.expect(ok && error.empty() && !compilation.diagnostics().hasErrors(),
            "constant and zero initialized namespace state is accepted");
        tests.expect(compilation.namespaceFunctionModel() != nullptr,
            "namespace model is available");
        if (compilation.namespaceFunctionModel() != nullptr)
        {
            const auto& model = *compilation.namespaceFunctionModel();
            tests.expect(model.states().size() == 3,
                "all namespace state declarations are modeled");
            const NamespaceStateSymbol* answer =
                model.findState(0, "Numeric", "Answer");
            const NamespaceStateSymbol* counter =
                model.findState(0, "Numeric", "Counter");
            tests.expect(answer != nullptr && answer->isConst &&
                    answer->initializerKind == NamespaceStateInitializerKind::Integer &&
                    answer->integerValue == 42,
                "integer constants are folded and retain const identity");
            tests.expect(counter != nullptr && !counter->isConst &&
                    counter->initializerKind == NamespaceStateInitializerKind::Zero,
                "declarations without initializer use zero initialization");
            tests.expect(answer != nullptr &&
                    answer->storageName.starts_with("__nsc_state_"),
                "state receives a deterministic physical storage name");
        }
    }

    {
        const std::filesystem::path projectRoot = root / "lowering";
        std::error_code ignored;
        std::filesystem::remove_all(projectRoot, ignored);
        std::filesystem::create_directories(projectRoot, ignored);
        writeFile(
            projectRoot / "State.nsproj",
            R"(<Project><PropertyGroup><Name>StateLowering</Name></PropertyGroup><ItemGroup><Compile Include="**/*.ns" /></ItemGroup></Project>)");
        writeFile(projectRoot / "Storage.ns",
            "namespace Shared { public int Counter = 7; public const int Bias = 5; }");
        writeFile(projectRoot / "Mutation.ns",
            "namespace Shared { int Increment() { Counter += Bias; return Counter; } }");
        writeFile(projectRoot / "Reader.ns",
            "namespace Consumer { int Read() { return Shared.Counter; } "
            "int* Address() { return &Shared.Counter; } }");

        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectOopMir(
            projectRoot / "State.nsproj", error);
        tests.expect(ok && error.empty() && !compilation.diagnostics().hasErrors(),
            "namespace state reads, writes, and addresses cross files through MIR");
        tests.expect(compilation.namespaceFunctionModel() != nullptr &&
                compilation.namespaceFunctionModel()->stateReferenceBindings().size() >= 5,
            "qualified and unqualified state references have explicit semantic bindings");

        if (ok && compilation.oopHirModel() != nullptr &&
            compilation.oopMirModel() != nullptr &&
            compilation.nameLookup() != nullptr)
        {
            const std::string hir =
                ProjectOopHirModelDumper::dump(*compilation.oopHirModel());
            tests.expect(hir.find("namespace-state-value") != std::string::npos &&
                    hir.find("namespace-state-address") != std::string::npos,
                "HIR distinguishes state values from stable state addresses");

            const auto fields = compilation.oopMirModel()->staticFields();
            tests.expect(std::ranges::count_if(fields, [](const auto& field)
            {
                return field.namespaceStateOrdinal.has_value();
            }) == 2, "MIR owns one storage record per namespace variable");
            tests.expect(std::ranges::any_of(fields, [](const auto& field)
            {
                return field.namespaceStateOrdinal.has_value() &&
                    field.name == "Counter" && !field.isConst &&
                    field.hasConstantInitializer && field.constantIntegerValue == 7;
            }), "mutable namespace storage keeps its constant initializer");

            const std::string mir =
                ProjectOopMirModelDumper::dump(*compilation.oopMirModel());
            tests.expect(mir.find("namespace-state-load") != std::string::npos &&
                    mir.find("namespace-state-address") != std::string::npos,
                "MIR retains namespace state identity on loads and stores");

            nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
            auto module = lowerer.lower(
                *compilation.nameLookup(), *compilation.oopMirModel());
            std::string verifyError;
            tests.expect(module != nullptr && lowerer.error().empty() &&
                    module->verify(verifyError),
                "namespace state lowers to verifier-clean LLVM");
            if (module != nullptr)
            {
                const std::string ir = module->text();
                tests.expect(ir.find("__nsc_state_") != std::string::npos &&
                        ir.find("store i32") != std::string::npos &&
                        ir.find("load i32") != std::string::npos,
                    "LLVM uses shared global storage for cross-file mutation and reads");
            }
        }
    }
    {
        ProjectCompilation compilation;
        std::string error;
        const std::filesystem::path projectRoot = root / "metadata";
        std::error_code ignored;
        std::filesystem::remove_all(projectRoot, ignored);
        std::filesystem::create_directories(projectRoot, ignored);
        writeFile(
            projectRoot / "State.nsproj",
            R"(<Project><PropertyGroup><Name>StateMetadata</Name><OutputType>StaticLibrary</OutputType></PropertyGroup><ItemGroup><Compile Include="**/*.ns" /></ItemGroup></Project>)");
        writeFile(projectRoot / "State.ns",
            "namespace Shared { public int Counter = 7; public const int Bias = 5; }");
        const bool ok = compilation.compileProjectOopMir(
            projectRoot / "State.nsproj", error);

        nativesharp::interop::LibraryMetadata metadata;
        nativesharp::interop::LibraryMetadataCodec codec;
        const std::filesystem::path metadataPath =
            projectRoot / "StateMetadata.nsmeta";
        error.clear();
        const bool wrote = ok && codec.write(
            compilation, metadataPath, metadata, error);
        tests.expect(wrote && error.empty() && metadata.formatMinor == 22 &&
                metadata.namespaceStates.size() == 2,
            "NSMETA 1.22 publishes public namespace state");
        nativesharp::interop::LibraryMetadata decoded;
        error.clear();
        tests.expect(wrote && codec.read(metadataPath, decoded, error) &&
                codec.verifyCurrentCompiler(decoded, error) &&
                decoded.namespaceStates.size() == 2,
            "public namespace state survives verified NSMETA round trip");
        const std::filesystem::path libraryPath =
            projectRoot / "StateMetadata.lib";
        const auto mutableState = std::ranges::find_if(
            decoded.namespaceStates,
            [](const auto& state) { return (state.flags & 1U) == 0U; });
        tests.expect(mutableState != decoded.namespaceStates.end() &&
                !mutableState->accessorName.empty(),
            "mutable public namespace state publishes its physical accessor");
        writeCoffArchive(
            libraryPath,
            mutableState != decoded.namespaceStates.end()
                ? std::string_view(mutableState->accessorName)
                : std::string_view("__missing_namespace_state_accessor"));
        const std::filesystem::path consumerRoot = root / "metadata-consumer";
        std::filesystem::remove_all(consumerRoot, ignored);
        std::filesystem::create_directories(consumerRoot, ignored);
        writeFile(
            consumerRoot / "Consumer.nsproj",
            "<Project><PropertyGroup><Name>StateConsumer</Name></PropertyGroup>"
            "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup><ItemGroup>"
            "<LibraryReference Include=\"" + libraryPath.generic_string() +
            "\" Metadata=\"" + metadataPath.generic_string() +
            "\" /></ItemGroup></Project>");
        writeFile(consumerRoot / "Consumer.ns",
            "namespace Consumer { int Read() { return Shared.Counter + Shared.Bias; } "
            "void Write() { Shared.Counter = 9; } }");
        const std::string consumerProject =
            (consumerRoot / "Consumer.nsproj").string();
        const std::array<std::string_view, 3> arguments = {
            "--dump-project-oop-llvm", consumerProject, "-O0"};
        nativesharp::driver::Driver driver;
        std::ostringstream output;
        std::ostringstream diagnostics;
        const nativesharp::ExitCode consumed =
            driver.run(arguments, output, diagnostics);
        tests.expect(consumed == nativesharp::ExitCode::Success &&
                diagnostics.str().empty(),
            "LibraryReference consumer binds public namespace state (" +
                diagnostics.str() + ")");
        tests.expect(consumed == nativesharp::ExitCode::Success &&
                mutableState != decoded.namespaceStates.end() &&
                output.str().find(mutableState->accessorName) != std::string::npos &&
                output.str().find("load i32") != std::string::npos &&
                output.str().find("store i32") != std::string::npos,
            "mutable imported namespace state calls the producer-owned accessor");
    }
    {
        ProjectCompilation compilation;
        std::string error;
        const auto project = createProject(
            root / "dynamic",
            "namespace Numeric { int Build() { return 7; } int Value = Build(); }");
        const bool ok = compilation.compileProjectMembers(project, error);
        tests.expect(!ok, "dynamic namespace initialization is rejected");
        tests.expect(hasDiagnostic(compilation, DiagnosticId::GlobalRequiresConstant),
            "dynamic initialization emits GlobalRequiresConstant");
    }

    {
        ProjectCompilation compilation;
        std::string error;
        const auto project = createProject(
            root / "duplicate",
            "namespace Numeric { int Value; int Value; }");
        const bool ok = compilation.compileProjectMembers(project, error);
        tests.expect(!ok, "duplicate namespace state is rejected");
        tests.expect(hasDiagnostic(compilation, DiagnosticId::DuplicateSymbol),
            "duplicate namespace state emits DuplicateSymbol");
        tests.expect(hasDiagnostic(compilation, DiagnosticId::PreviousDeclaration),
            "duplicate namespace state identifies its prior declaration");
    }

    std::filesystem::remove_all(root, errorCode);
    return tests.result();
}