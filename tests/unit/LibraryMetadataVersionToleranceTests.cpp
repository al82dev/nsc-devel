#include "nativesharp/Basic/Version.h"
#include "nativesharp/Driver/CompatibilityPolicy.h"
#include "nativesharp/Interop/LibraryMetadata.h"

#include "../support/TestContext.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace
{
using nativesharp::interop::LibraryMetadata;
using nativesharp::interop::LibraryMetadataCodec;
using nativesharp::tests::TestContext;

std::vector<std::uint8_t> readBytes(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

bool writeBytes(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    return stream.good();
}

void expectReadable(
    TestContext& tests,
    const LibraryMetadataCodec& codec,
    const std::filesystem::path& path,
    std::string_view description)
{
    LibraryMetadata decoded;
    std::string error;
    const bool readable = codec.read(path, decoded, error);
    const bool compatible = readable && codec.verifyCurrentCompiler(decoded, error);
    tests.expect(compatible, std::string(description) + " (" + error + ")");
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::current_path() / "_targeted" / "nsmeta-version-tolerance";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    LibraryMetadata model;
    model.producerVersion = std::string(nativesharp::kCompilerVersion);
    model.moduleName = "VersionTolerance";
    model.targetTriple = "x86_64-pc-windows-msvc";

    LibraryMetadataCodec codec;
    std::string error;
    const std::filesystem::path currentPath = root / "current.nsmeta";
    tests.expect(codec.writeModel(model, currentPath, error),
        "current metadata fixture writes (" + error + ")");
    expectReadable(tests, codec, currentPath, "current metadata is readable");

    const std::vector<std::uint8_t> current = readBytes(currentPath);
    tests.expect(current.size() >= 12U, "metadata fixture contains the fixed header");
    if (current.size() >= 12U)
    {
        std::vector<std::uint8_t> differentMajor = current;
        differentMajor[8] = 2U;
        differentMajor[9] = 0U;
        const std::filesystem::path majorPath = root / "different-major.nsmeta";
        tests.expect(writeBytes(majorPath, differentMajor), "different-major fixture writes");
        expectReadable(tests, codec, majorPath,
            "a compatible payload is not rejected by its major label");

        std::vector<std::uint8_t> futureMinor = current;
        const std::uint16_t minor =
            static_cast<std::uint16_t>(nativesharp::interop::kMetadataFormatMinor + 1U);
        futureMinor[10] = static_cast<std::uint8_t>(minor & 0xffU);
        futureMinor[11] = static_cast<std::uint8_t>(minor >> 8U);
        const std::filesystem::path minorPath = root / "future-minor.nsmeta";
        tests.expect(writeBytes(minorPath, futureMinor), "future-minor fixture writes");
        expectReadable(tests, codec, minorPath,
            "a compatible payload is not rejected by its minor label");
    }

    model.producerVersion = "0.2.0-dev-old";
    error.clear();
    tests.expect(codec.verifyCurrentCompiler(model, error),
        "producer identity does not override compatible metadata contents (" + error + ")");

    std::string explanation;
    tests.expect(nativesharp::driver::checkCompilerCompatibility(
                     "99.37-experimental", explanation, error) &&
                     explanation.find("informational") != std::string::npos,
        "compiler release labels do not override capability validation");

    return tests.resultKeeping(root);
}
