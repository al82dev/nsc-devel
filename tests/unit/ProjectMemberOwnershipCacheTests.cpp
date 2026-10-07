#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"

#include "../support/TestContext.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

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
    using nativesharp::tests::TestContext;

    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_member_ownership_cache";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);
    writeFile(
        root / "OwnershipCache.nsproj",
        "<Project><PropertyGroup><Name>OwnershipCache</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(
        root / "Main.ns",
        "class Resource {}\n"
        "struct Leaf { public owned Resource Value; }\n"
        "struct Middle { public Leaf Value; }\n"
        "struct Root { public Middle Value; }\n"
        "struct Plain { public int Value; }\n");

    ProjectCompilation compilation;
    std::string error;
    const bool compiled = compilation.compileProjectMembers(root / "OwnershipCache.nsproj", error);
    tests.expect(compiled, "ownership cache fixture compiles its member graph");
    tests.expect(error.empty(), "ownership cache fixture has no loader error");
    tests.expect(compilation.memberModel() != nullptr, "member compilation publishes its model");

    if (compilation.memberModel() != nullptr)
    {
        const auto& model = *compilation.memberModel();
        constexpr std::string_view leaf = "OwnershipCache::Leaf";
        constexpr std::string_view middle = "OwnershipCache::Middle";
        constexpr std::string_view rootType = "OwnershipCache::Root";
        constexpr std::string_view plain = "OwnershipCache::Plain";
        tests.expect(model.typeTransitivelyOwnsStorage(leaf), "direct owned field marks its struct");
        tests.expect(model.typeTransitivelyOwnsStorage(middle), "ownership reaches one outer struct");
        tests.expect(model.typeTransitivelyOwnsStorage(rootType), "ownership reaches the complete chain");
        tests.expect(!model.typeTransitivelyOwnsStorage(plain), "plain value struct remains non-owning");

        std::atomic<bool> stable{true};
        std::vector<std::thread> readers;
        for (std::size_t reader = 0; reader < 8; ++reader)
        {
            readers.emplace_back([&model, &stable, leaf, middle, rootType, plain]()
            {
                for (std::size_t iteration = 0; iteration < 10000; ++iteration)
                {
                    const bool expected =
                        model.typeTransitivelyOwnsStorage(leaf) &&
                        model.typeTransitivelyOwnsStorage(middle) &&
                        model.typeTransitivelyOwnsStorage(rootType) &&
                        !model.typeTransitivelyOwnsStorage(plain);
                    if (!expected)
                    {
                        stable.store(false, std::memory_order_relaxed);
                        break;
                    }
                }
            });
        }
        for (std::thread& reader : readers) reader.join();
        tests.expect(stable.load(std::memory_order_relaxed),
            "concurrent ownership queries remain stable and read-only");
    }

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
