#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Frontend/ProjectOopHirModelDumper.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/Frontend/ProjectOopMirModelDumper.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}

template <typename Node>
void collectNodes(const Node* node, std::vector<const Node*>& output)
{
    if (node == nullptr) return;
    output.push_back(node);
    for (const auto& child : node->children) collectNodes(child.get(), output);
}
}

int main()
{
    using namespace nativesharp::frontend;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const auto root = std::filesystem::temp_directory_path() /
        "nativesharp_integrated_assembly_mir_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    writeFile(root / "AssemblyMir.nsproj",
        "<Project><PropertyGroup><Name>AssemblyMir</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(root / "Main.ns", R"NS(
public class Program
{
    public static int Main()
    {
        int source = 40;
        int result;
        asm {
            mov $out0, $0
            add $out0, $1
        } out(result : "={rax}") in(source : "m", 2 : "i")
          clobber("cc", "memory", "rbx");
        return result;
    }
}
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool built = compilation.compileProjectOopMir(root / "AssemblyMir.nsproj", error);
    tests.expect(built && error.empty(), "validated asm lowers through project OOP MIR");

    if (built && compilation.oopHirModel() != nullptr && compilation.oopMirModel() != nullptr)
    {
        std::vector<const ProjectOopHirNode*> hir;
        std::vector<const ProjectOopMirNode*> mir;
        for (const auto& function : compilation.oopHirModel()->functions())
            collectNodes(function.body.get(), hir);
        for (const auto& function : compilation.oopMirModel()->functions())
            collectNodes(function.body.get(), mir);

        const auto hirOf = [&](ProjectOopHirNodeKind kind)
        {
            std::vector<const ProjectOopHirNode*> found;
            for (const auto* node : hir) if (node->kind == kind) found.push_back(node);
            return found;
        };
        const auto mirOf = [&](ProjectOopMirNodeKind kind)
        {
            std::vector<const ProjectOopMirNode*> found;
            for (const auto* node : mir) if (node->kind == kind) found.push_back(node);
            return found;
        };

        const auto hirAsm = hirOf(ProjectOopHirNodeKind::InlineAssembly);
        const auto mirAsm = mirOf(ProjectOopMirNodeKind::InlineAssembly);
        tests.expect(hirAsm.size() == 1 && mirAsm.size() == 1,
            "HIR and MIR each contain one first-class inline-assembly operation");
        tests.expect(!hirAsm.empty() && !mirAsm.empty() &&
                hirAsm[0]->stringValue == mirAsm[0]->stringValue &&
                hirAsm[0]->stringValue.find("mov $out0, $0") != std::string::npos,
            "the exact assembly source bytes survive HIR-to-MIR lowering");

        const auto hirLines = hirOf(ProjectOopHirNodeKind::InlineAssemblyLine);
        const auto mirLines = mirOf(ProjectOopMirNodeKind::InlineAssemblyLine);
        tests.expect(hirLines.size() == 2 && mirLines.size() == 2 &&
                mirLines[0]->stringValue == "mov $out0, $0" &&
                mirLines[1]->stringValue == "add $out0, $1" &&
                mirLines[0]->syntax != nullptr && mirLines[0]->syntax->range().isValid(),
            "instruction records retain order, exact spelling, and source identity");

        const auto inputs = mirOf(ProjectOopMirNodeKind::InlineAssemblyInput);
        const auto outputs = mirOf(ProjectOopMirNodeKind::InlineAssemblyOutput);
        tests.expect(inputs.size() == 2 && outputs.size() == 1 &&
                inputs[0]->integerValue == 0 && inputs[0]->stringValue == "m" &&
                inputs[0]->children.size() == 1 &&
                inputs[0]->children[0]->kind == ProjectOopMirNodeKind::LocalAddress &&
                inputs[1]->integerValue == 1 && inputs[1]->stringValue == "i" &&
                inputs[1]->children.size() == 1 &&
                inputs[1]->children[0]->kind == ProjectOopMirNodeKind::IntegerConstant &&
                outputs[0]->integerValue == 0 && outputs[0]->stringValue == "={rax}" &&
                outputs[0]->booleanValue && outputs[0]->children.size() == 1 &&
                outputs[0]->children[0]->kind == ProjectOopMirNodeKind::LocalAddress,
            "MIR operands preserve ordinals, constraints, explicitness, values, and addresses");

        const auto clobbers = mirOf(ProjectOopMirNodeKind::InlineAssemblyClobber);
        tests.expect(clobbers.size() == 3 &&
                clobbers[0]->stringValue == "cc" && clobbers[0]->elementTypeKey == "flags" &&
                clobbers[1]->stringValue == "memory" &&
                clobbers[1]->elementTypeKey == "memory" &&
                clobbers[2]->stringValue == "rbx" &&
                clobbers[2]->elementTypeKey == "register" && clobbers[2]->booleanValue,
            "MIR clobbers preserve effect categories and nonvolatile preservation requirements");

        const std::string hirDump = ProjectOopHirModelDumper::dump(*compilation.oopHirModel());
        const std::string mirDump = ProjectOopMirModelDumper::dump(*compilation.oopMirModel());
        tests.expect(hirDump.find("inline-assembly-output") != std::string::npos &&
                hirDump.find("constraint=={rax}") != std::string::npos &&
                mirDump.find("schema=1.1") != std::string::npos &&
                mirDump.find("category=memory") != std::string::npos,
            "HIR/MIR dumps expose the complete assembly contract and schema revision");
    }

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
