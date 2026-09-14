#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Interop/LibraryMetadata.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"

#include "../support/TestContext.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace
{
void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}

std::filesystem::path writeProject(
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view source)
{
    const auto directory = root / name;
    writeFile(directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" />"
        "</ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
    return directory / (std::string(name) + ".nsproj");
}

nativesharp::ExitCode runDriver(
    std::span<const std::string_view> arguments,
    std::string& output,
    std::string& error)
{
    nativesharp::driver::Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const nativesharp::ExitCode code = driver.run(arguments, out, err);
    output = out.str();
    error = err.str();
    return code;
}

bool diagnosticContains(
    const nativesharp::frontend::ProjectCompilation& compilation,
    std::string_view text)
{
    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
    {
        if (diagnostic.message().find(text) != std::string::npos) return true;
    }
    return false;
}
}

int main()
{
    using nativesharp::frontend::ClassMemberKind;
    using nativesharp::frontend::ProjectCompilation;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const auto root = std::filesystem::path(NSC_BINARY_DIR) / "item308_volatile_mmio_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);

    const auto positive = writeProject(root, "VolatileMmio", R"NS(
public struct DeviceRegisters
{
    public volatile uint Status;
    public volatile ushort Control;
    public uint Ordinary;
}

public class Device
{
    private static volatile uint Doorbell;

    public static uint Touch(DeviceRegisters* registers)
    {
        registers->Status = registers->Status + 1u;
        registers->Control += 2;
        Doorbell = registers->Status;
        return Doorbell + registers->Ordinary;
    }
}
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(positive, error);
    tests.expect(ok && error.empty(), "volatile MMIO fixture reaches O0 OOP MIR");
    if (ok && compilation.memberModel() != nullptr && compilation.oopMirModel() != nullptr)
    {
        bool status = false;
        bool control = false;
        bool doorbell = false;
        bool ordinary = true;
        for (const auto& member : compilation.memberModel()->members())
        {
            if (member.kind != ClassMemberKind::Field) continue;
            if (member.name == "Status") status = member.isVolatile;
            if (member.name == "Control") control = member.isVolatile;
            if (member.name == "Doorbell") doorbell = member.isVolatile && member.isStatic;
            if (member.name == "Ordinary") ordinary = member.isVolatile;
        }
        tests.expect(status && control && doorbell,
            "instance and static fields retain their volatile declaration contract");
        tests.expect(!ordinary, "an ordinary neighboring field remains non-volatile");

        nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
        auto module = lowerer.lower(*compilation.nameLookup(), *compilation.oopMirModel());
        tests.expect(module != nullptr && lowerer.error().empty(),
            "volatile MMIO fixture lowers to LLVM");
        if (module != nullptr)
        {
            std::string verifyError;
            tests.expect(module->verify(verifyError),
                "volatile MMIO LLVM module passes the verifier");
            const std::string llvm = module->text();
            tests.expect(llvm.find("load volatile i32") != std::string::npos,
                "volatile uint reads become LLVM volatile loads");
            tests.expect(llvm.find("store volatile i32") != std::string::npos,
                "volatile uint writes become LLVM volatile stores");
            tests.expect(llvm.find("load volatile i16") != std::string::npos,
                "volatile ushort compound updates preserve volatile reads");
            tests.expect(llvm.find("store volatile i16") != std::string::npos,
                "volatile ushort compound updates preserve volatile writes");
            tests.expect(llvm.find("load atomic") == std::string::npos &&
                    llvm.find("store atomic") == std::string::npos &&
                    llvm.find("fence ") == std::string::npos,
                "volatile remains an MMIO visibility contract, not atomic synchronization");
        }

        const auto metadataPath = root / "VolatileMmio" / "VolatileMmio.nsmeta";
        nativesharp::interop::LibraryMetadataCodec codec;
        std::string metadataError;
        tests.expect(codec.write(compilation, metadataPath, metadataError),
            "volatile declarations serialize to NSMETA 1.14");
        nativesharp::interop::LibraryMetadata metadata;
        tests.expect(codec.read(metadataPath, metadata, metadataError),
            "volatile NSMETA round-trips through the current reader");
        tests.expect(codec.verifyCurrentCompiler(metadata, metadataError),
            "volatile NSMETA satisfies the current compiler contract");
        bool memberVolatile = false;
        bool layoutVolatile = false;
        for (const auto& member : metadata.members)
        {
            if (member.name == "Status" &&
                (member.flags & nativesharp::interop::kMetadataMemberVolatile) != 0U)
                memberVolatile = true;
        }
        for (const auto& type : metadata.types)
        {
            for (const auto& field : type.fields)
            {
                if (field.name == "Status" &&
                    (field.flags & nativesharp::interop::kMetadataMemberVolatile) != 0U)
                    layoutVolatile = true;
            }
        }
        tests.expect(memberVolatile && layoutVolatile,
            "NSMETA preserves volatile on both the public member and physical layout field");

        nativesharp::interop::LibraryMetadata downlevel = metadata;
        downlevel.formatMinor = 13;
        const auto downlevelPath = root / "VolatileMmio" / "VolatileMmio-1.13.nsmeta";
        tests.expect(codec.writeModel(downlevel, downlevelPath, metadataError),
            "a requested NSMETA 1.13 artifact is written with historical semantics");
        nativesharp::interop::LibraryMetadata decodedDownlevel;
        tests.expect(codec.read(downlevelPath, decodedDownlevel, metadataError),
            "the down-level volatile fixture remains readable");
        bool downlevelVolatile = false;
        for (const auto& member : decodedDownlevel.members)
        {
            downlevelVolatile = downlevelVolatile ||
                (member.flags & nativesharp::interop::kMetadataMemberVolatile) != 0U;
        }
        for (const auto& type : decodedDownlevel.types)
        {
            for (const auto& field : type.fields)
            {
                downlevelVolatile = downlevelVolatile ||
                    (field.flags & nativesharp::interop::kMetadataMemberVolatile) != 0U;
            }
        }
        tests.expect(!downlevelVolatile,
            "NSMETA 1.13 output strips the 1.14 volatile bit instead of misrepresenting it");
    }

    const auto producer = root / "PrecompiledProducer";
    writeFile(producer / "VolatileProducer.nsproj",
        "<Project><PropertyGroup><Name>VolatileProducer</Name>"
        "<OutputType>StaticLibrary</OutputType></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(producer / "Registers.ns", R"NS(
namespace Precompiled
{
    public struct Registers
    {
        public volatile uint Status;
    }

    public class Device
    {
        public static volatile uint Doorbell;
    }
}
)NS");
    std::string driverOutput;
    std::string driverError;
    const std::string producerProject = (producer / "VolatileProducer.nsproj").string();
    const std::string producerLibrary = (producer / "VolatileProducer.lib").string();
    const std::array<std::string_view, 5> producerArguments = {
        "-O0", "--emit-project-oop-lib", producerProject, "-o", producerLibrary};
    tests.expect(runDriver(producerArguments, driverOutput, driverError) ==
            nativesharp::ExitCode::Success,
        "O0 producer emits a volatile NSMETA/library pair (" + driverError + ")");

    const auto consumer = root / "PrecompiledConsumer";
    writeFile(consumer / "VolatileConsumer.nsproj",
        "<Project><PropertyGroup><Name>VolatileConsumer</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup><ItemGroup>"
        "<LibraryReference Include=\"" + (producer / "VolatileProducer.lib").generic_string() +
        "\" Metadata=\"" + (producer / "VolatileProducer.nsmeta").generic_string() +
        "\" /></ItemGroup></Project>");
    writeFile(consumer / "Main.ns", R"NS(
using Precompiled;
public class Consumer
{
    public static uint Read(Registers* registers)
    {
        Device.Doorbell = registers->Status;
        return Device.Doorbell;
    }
}
)NS");
    const std::string consumerProject = (consumer / "VolatileConsumer.nsproj").string();
    const std::array<std::string_view, 3> consumerArguments = {
        "--dump-project-oop-llvm", consumerProject, "-O0"};
    driverOutput.clear();
    driverError.clear();
    const bool consumerOk = runDriver(consumerArguments, driverOutput, driverError) ==
        nativesharp::ExitCode::Success;
    tests.expect(consumerOk, "LibraryReference consumer reconstructs volatile fields (" + driverError + ")");
    tests.expect(consumerOk && driverOutput.find("load volatile i32") != std::string::npos &&
            driverOutput.find("store volatile i32") != std::string::npos,
        "LibraryReference consumer preserves volatile loads and stores in O0 LLVM");

    for (const auto& [name, source] : {
        std::pair{"VolatileString", "public class Bad { public volatile string Value; }"},
        std::pair{"VolatileOwned", "public class Box { } public class Bad { public volatile owned Box Value; }"},
        std::pair{"VolatileReadonly", "public class Bad { public readonly volatile int Value; }"},
        std::pair{"VolatileMethod", "public class Bad { public volatile int Read() { return 0; } }"},
        std::pair{"VolatileProperty", "public class Bad { public volatile int Value { get; set; } }"}})
    {
        ProjectCompilation invalid;
        std::string invalidError;
        const auto project = writeProject(root, name, source);
        tests.expect(!invalid.compileProjectMembers(project, invalidError),
            std::string(name) + ": invalid volatile contract is rejected");
        tests.expect(diagnosticContains(invalid, "volatile"),
            std::string(name) + ": diagnostic identifies volatile");
    }

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
