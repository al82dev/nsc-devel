#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/Frontend/ProjectOopMirModelDumper.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace
{
void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}

const nativesharp::frontend::ProjectOopMirFunction* findFunction(
    const nativesharp::frontend::ProjectOopMirModel& model, std::string_view fragment)
{
    for (const auto& function : model.functions())
        if (function.mangledName.find(fragment) != std::string::npos) return &function;
    return nullptr;
}
}

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::frontend;
    const std::filesystem::path root =
        nativesharp::tests::evidenceRoot("nativesharp_item395_mir_layout_abi_tests");
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    writeFile(root / "Item395.nsproj",
        "<Project><PropertyGroup><Name>Item395</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(root / "Main.ns",
        "namespace Item395; "
        "public struct Three { public byte A; public byte B; public byte C; } "
        "public struct Eight { public long Value; } "
        "public class Probe { "
        "public static Three Echo(Three value) { return value; } "
        "public static Eight Direct(Eight value) { return value; } }");

    ProjectCompilation compilation;
    std::string error;
    const bool built = compilation.compileProjectOopMir(root / "Item395.nsproj", error);
    tests.expect(built && compilation.oopMirModel() != nullptr,
        "layout/ABI fixture reaches project MIR: " + error);
    if (!built || compilation.oopMirModel() == nullptr) return tests.resultKeeping(root);
    const ProjectOopMirModel& model = *compilation.oopMirModel();

    const ProjectOopMirStructLayout* three = nullptr;
    const ProjectOopMirStructLayout* eight = nullptr;
    for (const auto& layout : model.structLayouts())
    {
        if (layout.typeKey.ends_with(".Three")) three = &layout;
        if (layout.typeKey.ends_with(".Eight")) eight = &layout;
    }
    tests.expect(three != nullptr && three->size == 3 && three->alignment == 1 &&
            three->abiPassing == ProjectOopMirAbiPassing::Indirect,
        "MIR owns the complete three-byte layout and indirect Win64 classification");
    tests.expect(eight != nullptr && eight->size == 8 &&
            eight->abiPassing == ProjectOopMirAbiPassing::Direct,
        "MIR owns the direct eight-byte aggregate classification");

    const ProjectOopMirFunction* echo = findFunction(model, "Echo");
    const ProjectOopMirFunction* direct = findFunction(model, "Direct");
    tests.expect(echo != nullptr &&
            echo->abi.returnPassing == ProjectOopMirAbiPassing::Indirect &&
            echo->abi.hiddenReturnParameterOrdinal == 0 &&
            echo->abi.parameterPassing.size() == 1 &&
            echo->abi.parameterPassing[0] == ProjectOopMirAbiPassing::Indirect &&
            echo->abi.parameterPhysicalOrdinals[0] == 1 &&
            echo->abi.physicalParameterCount == 2,
        "MIR freezes the complete hidden-return and parameter ABI for an indirect aggregate");
    tests.expect(direct != nullptr &&
            direct->abi.returnPassing == ProjectOopMirAbiPassing::Direct &&
            !direct->abi.hiddenReturnParameterOrdinal.has_value() &&
            direct->abi.parameterPassing.size() == 1 &&
            direct->abi.parameterPassing[0] == ProjectOopMirAbiPassing::Direct,
        "MIR freezes the direct aggregate function ABI without a hidden slot");

    const std::string dump = ProjectOopMirModelDumper::dump(model);
    tests.expect(dump.find("schema=1.11") != std::string::npos &&
            dump.find("abi=win64-indirect") != std::string::npos,
        "versioned MIR dumps expose layout and ABI decisions");

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*compilation.nameLookup(), model);
    tests.expect(module != nullptr, "LLVM consumes the MIR ABI contract: " + lowerer.error());
    return tests.resultKeeping(root);
}
