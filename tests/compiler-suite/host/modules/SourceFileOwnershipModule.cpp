#include "nsc_suite/TestRegistry.h"

#include "nativesharp/Basic/FileId.h"
#include "nativesharp/Source/SourceFile.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace nsc::suite
{
// Registers the bounded ownership cases and preserves the UINT32_MAX saturation case as infrastructure-pending.
void registerSourceFileOwnershipModule(TestRegistry& registry)
{
    registry.add(TestCase{
        "TS-SRC-003",
        "CS-SRC-002",
        "Host.Source",
        "unit",
        "PR",
        {"source", "file", "ownership", "accessor"},
        [](TestContext& context) {
            const std::filesystem::path path = R"(A:\source\main.ns)";
            const std::string buffer = "namespace Example {}";
            const nativesharp::source::SourceFile file(nativesharp::FileId{37}, path, buffer);

            context.expect(file.id() == nativesharp::FileId{37}, "source file must preserve its file ID");
            context.expect(file.path() == path, "source file must preserve its filesystem path");
            context.expect(file.displayPath() == path.string(), "display path must preserve path.string bytes");
            context.expect(file.buffer() == buffer, "source file must preserve its ASCII buffer");
        },
    });
    registry.add(TestCase{
        "TS-SRC-004",
        "CS-SRC-002",
        "Host.Source",
        "unit",
        "NIGHTLY",
        {"source", "file", "ownership", "unicode-path"},
        [](TestContext& context) {
            const std::filesystem::path path = L"A:\\fuente\\m\u00F3dulo-\u00F1.ns";
            const std::string expectedDisplay = path.string();
            const nativesharp::source::SourceFile file(nativesharp::FileId{41}, path, "source");

            context.expect(file.path() == path, "source file must preserve its representable Unicode path");
            context.expect(
                file.displayPath() == expectedDisplay,
                "display path must equal the narrow representation captured at construction");
        },
    });
    registry.add(TestCase{
        "TS-SRC-005",
        "CS-SRC-002",
        "Host.Source",
        "unit",
        "PR",
        {"source", "file", "ownership", "binary-buffer"},
        [](TestContext& context) {
            const std::string bytes({'\0', static_cast<char>(0xff), 'x', '\0'});
            const nativesharp::source::SourceFile file(
                nativesharp::FileId{53}, R"(A:\source\binary.ns)", bytes);
            const std::string_view actual = file.buffer();

            context.expect(file.size() == 4, "source size must count embedded NUL and high bytes");
            context.expect(actual.size() == bytes.size(), "source buffer view must preserve the complete byte count");
            for (std::size_t index = 0; index < bytes.size(); ++index)
            {
                const auto expected = static_cast<std::uint8_t>(bytes[index]);
                const auto observed = static_cast<std::uint8_t>(actual[index]);
                context.expect(observed == expected, "source buffer view must preserve every byte");
            }
        },
    });
    registry.add(TestCase{
        "TS-SRC-006",
        "CS-SRC-002",
        "Host.Source",
        "unit",
        "PR",
        {"source", "file", "ownership", "move"},
        [](TestContext& context) {
            std::optional<nativesharp::source::SourceFile> file;
            const std::filesystem::path expectedPath = R"(A:\source\owned-after-inputs-die.ns)";
            const std::string expectedBuffer(96, 'q');
            {
                std::filesystem::path inputPath = expectedPath;
                std::string inputBuffer = expectedBuffer;
                file.emplace(
                    nativesharp::FileId{61}, std::move(inputPath), std::move(inputBuffer));
            }

            context.expect(file->path() == expectedPath, "source file must own the path after moved inputs die");
            context.expect(file->buffer() == expectedBuffer, "source file must own the buffer after moved inputs die");
        },
    });
    registry.add(TestCase{
        "TS-SRC-007",
        "CS-SRC-002",
        "Host.Source",
        "unit",
        "PR",
        {"source", "file", "ownership", "view"},
        [](TestContext& context) {
            const std::filesystem::path path = R"(A:\source\stable-owned-view-with-a-long-name.ns)";
            const std::string bytes(128, 'v');
            const nativesharp::source::SourceFile file(nativesharp::FileId{67}, path, bytes);
            const std::string_view firstDisplay = file.displayPath();
            const std::string_view firstBuffer = file.buffer();
            const std::string_view secondDisplay = file.displayPath();
            const std::string_view secondBuffer = file.buffer();

            context.expect(
                firstDisplay.data() == secondDisplay.data(),
                "unchanged display-path views must refer to stable file-owned bytes");
            context.expect(
                firstBuffer.data() == secondBuffer.data(),
                "unchanged buffer views must refer to stable file-owned bytes");
            context.expect(firstDisplay == path.string(), "display-path view must preserve owned bytes");
            context.expect(firstBuffer == bytes, "buffer view must preserve owned bytes");
        },
    });
    registry.add(TestCase{
        "TS-SRC-008",
        "CS-SRC-002",
        "Host.Source",
        "unit",
        "WEEKLY",
        {"source", "file", "size", "infrastructure-pending", "large-memory"},
        [](TestContext& context) {
            context.expect(
                false,
                "TS-SRC-008 requires a bounded fixture above UINT32_MAX; the current SourceFile API "
                "must own and scan the complete buffer and exposes no virtual-size seam");
        },
        TestDisposition::InfrastructurePending,
    });
}
}
