#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"

#include "../support/TestContext.h"

#include <array>
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
}

int main()
{
    using nativesharp::frontend::ProjectCompilation;
    using nativesharp::frontend::ProjectTypeConversion;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_member_type_worklists";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);
    writeFile(
        root / "TypeWorklists.nsproj",
        "<Project><PropertyGroup><Name>TypeWorklists</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(
        root / "Main.ns",
        "class Resource {}\n"
        "copy struct CopyLeaf { public int Value; }\n"
        "struct Owner { public owned Resource Value; }\n"
        "struct DropLeaf { public int Value; ~DropLeaf() {} }\n"
        "struct DropOuter { public DropLeaf Value; }\n");

    ProjectCompilation compilation;
    std::string error;
    const bool compiled = compilation.compileProjectMembers(root / "TypeWorklists.nsproj", error);
    tests.expect(compiled, "type worklist fixture compiles its member graph");
    tests.expect(error.empty(), "type worklist fixture has no loader error");
    tests.expect(compilation.memberModel() != nullptr, "member compilation publishes its model");

    if (compilation.memberModel() != nullptr)
    {
        const auto& model = *compilation.memberModel();
        const std::string copyLeaf = "type:TypeWorklists::CopyLeaf";
        const std::string owner = "type:TypeWorklists::Owner";
        const std::string dropOuter = "type:TypeWorklists::DropOuter";
        const std::array<std::string, 2> copyElements = {"builtin:int", copyLeaf};
        const std::array<std::string, 2> moveElements = {"builtin:int", owner};
        const std::array<std::string, 2> dropElements = {"builtin:int", dropOuter};
        const std::string copyTuple = ProjectTypeConversion::tupleTypeKey(copyElements);
        const std::string moveTuple = ProjectTypeConversion::tupleTypeKey(moveElements);
        const std::string dropTuple = ProjectTypeConversion::tupleTypeKey(dropElements);

        tests.expect(model.typeIsImplicitlyCopyable(copyLeaf), "explicit Copy struct is copyable");
        tests.expect(model.typeIsImplicitlyCopyable(copyTuple), "Copy tuple is copyable");
        tests.expect(model.typeIsImplicitlyCopyable(copyLeaf + "[4]"),
            "fixed array of Copy values is copyable");
        tests.expect(!model.typeIsImplicitlyCopyable(owner), "owned field makes struct non-Copy");
        tests.expect(!model.typeIsImplicitlyCopyable(moveTuple),
            "tuple containing an owner is non-Copy");
        tests.expect(model.typeIsMoveOnly(owner), "owned struct is move-only");
        tests.expect(model.typeIsMoveOnly(moveTuple), "tuple propagates move-only");
        tests.expect(model.typeIsMoveOnly(owner + "[4]"), "fixed array propagates move-only");
        tests.expect(model.typeTransitivelyHasCustomDestructor(dropOuter),
            "struct field propagates custom destructor");
        tests.expect(model.typeTransitivelyHasCustomDestructor(dropTuple),
            "tuple propagates custom destructor");
        tests.expect(model.typeTransitivelyHasCustomDestructor(dropOuter + "[4]"),
            "fixed array propagates custom destructor");
        tests.expect(!model.typeTransitivelyHasCustomDestructor(copyTuple),
            "Copy-only tuple has no custom destructor");
    }

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
