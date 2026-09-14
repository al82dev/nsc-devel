#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Frontend/InliningPolicy.h"
#include "nativesharp/Interop/LibraryMetadata.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using nativesharp::ExitCode;
using nativesharp::driver::Driver;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

ExitCode runDriver(
    std::span<const std::string_view> arguments,
    std::string& output,
    std::string& error)
{
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode code = driver.run(arguments, out, err);
    output = out.str();
    error = err.str();
    return code;
}

bool hasCallTo(std::string_view ir, std::string_view physicalName)
{
    std::size_t begin = 0;
    while (begin < ir.size())
    {
        const std::size_t end = ir.find('\n', begin);
        const std::string_view line = ir.substr(
            begin, end == std::string_view::npos ? ir.size() - begin : end - begin);
        if (line.find("call ") != std::string_view::npos &&
            line.find(physicalName) != std::string_view::npos)
        {
            return true;
        }
        if (end == std::string_view::npos) break;
        begin = end + 1U;
    }
    return false;
}

const nativesharp::interop::LibraryMetadataMember* findMember(
    const nativesharp::interop::LibraryMetadata& metadata,
    std::string_view name)
{
    const auto found = std::ranges::find_if(metadata.members, [&](const auto& member) {
        return member.name == name;
    });
    return found == metadata.members.end() ? nullptr : &*found;
}
}

int main()
{
    using nativesharp::frontend::InliningPolicy;
    using nativesharp::interop::LibraryMetadata;
    using nativesharp::interop::LibraryMetadataCodec;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const std::filesystem::path root =
        nativesharp::tests::evidenceRoot("nativesharp_cross_library_inlining_tests");
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);

    const std::filesystem::path producer = root / "Producer";
    writeFile(producer / "InlineProducer.nsproj",
        "<Project><PropertyGroup><Name>InlineProducer</Name><OutputType>StaticLibrary</OutputType>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(producer / "Api.ns", R"NS(
namespace CrossLibrary
{
    public static class Arithmetic
    {
        public static forceinline int Required(int value) { return value + 1; }
        public static inline int Preferred(int value) { return value + 2; }
        public static noinline int Boundary(int value) { return value + 3; }
    }

    public struct Cell
    {
        public int Value;
        public Cell(int value) { Value = value; }
        public forceinline int Read() { return Value; }
    }
}
)NS");

    std::string output;
    std::string error;
    const std::string producerProject = (producer / "InlineProducer.nsproj").string();
    const std::string producerLibrary = (producer / "InlineProducer.lib").string();
    const std::array<std::string_view, 5> producerArguments = {
        "-O0", "--emit-project-oop-lib", producerProject, "-o", producerLibrary};
    tests.expect(runDriver(producerArguments, output, error) == ExitCode::Success,
        "O0 producer emits a precompiled library with ordinary inline methods (" + error + ")");

    LibraryMetadata metadata;
    LibraryMetadataCodec codec;
    error.clear();
    const bool metadataRead = codec.read(producer / "InlineProducer.nsmeta", metadata, error);
    tests.expect(metadataRead &&
                     metadata.formatMinor == nativesharp::interop::kMetadataFormatMinor &&
                     metadata.formatMinor >= 13U,
        "producer writes the current NSMETA cross-library inline contract (" + error + ")");
    const auto* required = findMember(metadata, "Required");
    const auto* preferred = findMember(metadata, "Preferred");
    const auto* boundary = findMember(metadata, "Boundary");
    const auto* read = findMember(metadata, "Read");
    const auto policyOf = [](const nativesharp::interop::LibraryMetadataMember* member) {
        return member == nullptr ? 99U :
            (member->flags & nativesharp::interop::kMetadataMemberInliningMask) >>
                nativesharp::interop::kMetadataMemberInliningShift;
    };
    tests.expect(required != nullptr && !required->inlineSourceText.empty() &&
                     policyOf(required) == static_cast<unsigned>(InliningPolicy::Required),
        "forceinline policy and source body survive metadata emission");
    tests.expect(preferred != nullptr && !preferred->inlineSourceText.empty() &&
                     policyOf(preferred) == static_cast<unsigned>(InliningPolicy::Preferred),
        "preferred inline policy and source body survive metadata emission");
    tests.expect(read != nullptr && !read->inlineSourceText.empty() &&
                     policyOf(read) == static_cast<unsigned>(InliningPolicy::Required),
        "a struct instance method transports the same reusable inline contract");
    tests.expect(boundary != nullptr && boundary->inlineSourceText.empty() &&
                     policyOf(boundary) == static_cast<unsigned>(InliningPolicy::Forbidden),
        "noinline crosses metadata as policy without duplicating its implementation body");

    LibraryMetadata downLevel = metadata;
    downLevel.formatMinor = 12U;
    error.clear();
    const bool wroteDownLevel = codec.writeModel(
        downLevel, producer / "InlineProducer-1.12.nsmeta", error);
    tests.expect(wroteDownLevel,
        "down-level writer can deliberately produce historical NSMETA 1.12 (" + error + ")");
    LibraryMetadata decodedDownLevel;
    error.clear();
    const bool readDownLevel = codec.read(
        producer / "InlineProducer-1.12.nsmeta", decodedDownLevel, error);
    tests.expect(readDownLevel && std::ranges::all_of(
            decodedDownLevel.members, [](const auto& member) {
                return member.inlineSourceText.empty() &&
                    (member.flags & nativesharp::interop::kMetadataMemberInliningMask) == 0U;
            }),
        "NSMETA 1.12 output drops 1.13 policy bits and inline bodies (" + error + ")");

    const std::filesystem::path consumer = root / "Consumer";
    writeFile(consumer / "InlineConsumer.nsproj",
        "<Project><PropertyGroup><Name>InlineConsumer</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup><ItemGroup>"
        "<LibraryReference Include=\"" + (producer / "InlineProducer.lib").generic_string() +
        "\" Metadata=\"" + (producer / "InlineProducer.nsmeta").generic_string() +
        "\" /></ItemGroup></Project>");
    writeFile(consumer / "Program.ns", R"NS(
using CrossLibrary;

class Program
{
    public static int Main()
    {
        Cell cell = new Cell(38);
        int required = Arithmetic.Required(cell.Read());
        int preferred = Arithmetic.Preferred(1);
        int boundary = Arithmetic.Boundary(2);
        return required == 39 && preferred == 3 && boundary == 5 ? 0 : 1;
    }
}
)NS");

    const std::string consumerProject = (consumer / "InlineConsumer.nsproj").string();
    const std::array<std::string_view, 3> dumpArguments = {
        "--dump-project-oop-llvm", consumerProject, "-O0"};
    output.clear();
    error.clear();
    const bool dumped = runDriver(dumpArguments, output, error) == ExitCode::Success;
    tests.expect(dumped, "LibraryReference consumer materializes transported inline bodies (" + error + ")");
    if (dumped && required != nullptr && preferred != nullptr && read != nullptr && boundary != nullptr)
    {
        writeFile(root / "consumer.ll", output);
        tests.expect(output.find("alwaysinline") != std::string::npos &&
                         output.find("inlinehint") != std::string::npos &&
                         output.find("noinline") != std::string::npos,
            "consumer LLVM retains all three producer policies");
        tests.expect(output.find("weak_odr") != std::string::npos &&
                         output.find("comdat any") != std::string::npos,
            "transported definitions use symbol-scoped ODR/COMDAT linkage");
        tests.expect(hasCallTo(output, required->mangledName) &&
                         hasCallTo(output, preferred->mangledName) &&
                         hasCallTo(output, read->mangledName) &&
                         hasCallTo(output, boundary->mangledName),
            "raw O0 IR resolves calls to producer physical identities before optimization");
    }

    const std::string executable = (consumer / "InlineConsumer.exe").string();
    const std::array<std::string_view, 5> executableArguments = {
        "-O0", "--emit-project-oop-exe", consumerProject, "-o", executable};
    output.clear();
    error.clear();
    const bool emitted = runDriver(executableArguments, output, error) == ExitCode::Success;
    tests.expect(emitted,
        "O0 consumer links producer and consumer ODR copies without duplicate symbols (" + error + ")");
    if (emitted)
    {
        const std::string command = "\"" + executable + "\"";
        tests.expect(std::system(command.c_str()) == 0,
            "cross-library inline executable preserves the producer's result");
    }

    return tests.resultKeeping(root);
}
