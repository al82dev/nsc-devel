#include "nsc_suite/TestRegistry.h"

#include "nativesharp/Basic/Sha256.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nsc::suite
{
namespace
{
using nativesharp::basic::Sha256Digest;

void addCase(
    TestRegistry& registry,
    std::string id,
    std::string profile,
    TestFunction function)
{
    registry.add(TestCase{
        std::move(id),
        "CB-BAS-008",
        "Host.Basic",
        "component",
        std::move(profile),
        {"basic", "sha256", "filesystem"},
        std::move(function),
    });
}

class IsolatedCaseDirectory
{
public:
    explicit IsolatedCaseDirectory(std::string_view caseId)
        : root_(std::filesystem::current_path() /
                (".nsc-suite-" + std::string(caseId) + "-" +
                 std::to_string(GetCurrentProcessId())))
    {
        if (!std::filesystem::create_directory(root_))
        {
            throw std::runtime_error("cannot create isolated SHA-256 case directory: " + root_.string());
        }
    }

    IsolatedCaseDirectory(const IsolatedCaseDirectory&) = delete;
    IsolatedCaseDirectory& operator=(const IsolatedCaseDirectory&) = delete;

    ~IsolatedCaseDirectory()
    {
        std::error_code ignored;
        for (auto iterator = tracked_.rbegin(); iterator != tracked_.rend(); ++iterator)
        {
            std::filesystem::remove(*iterator, ignored);
            ignored.clear();
        }
        std::filesystem::remove(root_, ignored);
    }

    [[nodiscard]] std::filesystem::path file(std::string_view name)
    {
        const std::filesystem::path path = root_ / std::string(name);
        tracked_.push_back(path);
        return path;
    }

    [[nodiscard]] std::filesystem::path directory(std::string_view name)
    {
        const std::filesystem::path path = root_ / std::string(name);
        if (!std::filesystem::create_directory(path))
        {
            throw std::runtime_error("cannot create isolated SHA-256 fixture directory: " + path.string());
        }
        tracked_.push_back(path);
        return path;
    }

private:
    std::filesystem::path root_;
    std::vector<std::filesystem::path> tracked_;
};

void writeBytes(const std::filesystem::path& path, std::span<const std::uint8_t> bytes)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream)
    {
        throw std::runtime_error("cannot create SHA-256 fixture: " + path.string());
    }
    for (const std::uint8_t byte : bytes)
    {
        stream.put(static_cast<char>(byte));
    }
    stream.close();
    if (!stream)
    {
        throw std::runtime_error("cannot finish SHA-256 fixture: " + path.string());
    }
}

Sha256Digest sentinelDigest()
{
    Sha256Digest digest{};
    digest.fill(0xa5);
    return digest;
}

void expectFileDigest(
    TestContext& context,
    const std::filesystem::path& path,
    std::uintmax_t expectedSize,
    std::string_view expectedHex,
    std::string_view description)
{
    Sha256Digest digest = sentinelDigest();
    std::uintmax_t size = std::numeric_limits<std::uintmax_t>::max();
    std::string error = "stale error";
    const bool succeeded = nativesharp::basic::sha256File(path, digest, size, error);
    context.expect(succeeded, std::string(description) + " must be readable");
    context.expect(size == expectedSize, std::string(description) + " must report the exact byte count");
    context.expect(
        nativesharp::basic::sha256Hex(digest) == expectedHex,
        std::string(description) + " must match the independent digest");
    context.expect(error.empty(), std::string(description) + " must clear stale errors on success");
}

void expectControlledFailure(
    TestContext& context,
    const std::filesystem::path& path,
    const Sha256Digest& originalDigest,
    std::uintmax_t originalSize,
    bool succeeded,
    const Sha256Digest& digest,
    std::uintmax_t size,
    std::string_view error,
    std::string_view description)
{
    context.expect(!succeeded, std::string(description) + " must fail");
    context.expect(digest == originalDigest, std::string(description) + " must not publish a new digest");
    context.expect(size == originalSize, std::string(description) + " must not publish a new size");
    context.expect(
        error.find("cannot open file for SHA-256") != std::string_view::npos ||
            error.find("cannot read file for SHA-256") != std::string_view::npos,
        std::string(description) + " must classify the open or read failure");
    context.expect(
        error.find(path.string()) != std::string_view::npos,
        std::string(description) + " must identify the failing path");
}

void registerEmptyAndBinaryCases(TestRegistry& registry)
{
    addCase(registry, "TB-BAS-035", "PR", [](TestContext& context) {
        IsolatedCaseDirectory workspace("TB-BAS-035");
        const auto path = workspace.file("empty.bin");
        writeBytes(path, {});
        expectFileDigest(
            context,
            path,
            0,
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
            "empty file");
    });
    addCase(registry, "TB-BAS-036", "PR", [](TestContext& context) {
        IsolatedCaseDirectory workspace("TB-BAS-036");
        const auto path = workspace.file("binary.bin");
        constexpr std::array bytes{
            std::uint8_t{0x00}, std::uint8_t{0x01}, std::uint8_t{0x02},
            std::uint8_t{0x00}, std::uint8_t{0xff}, std::uint8_t{0x80},
            std::uint8_t{0x00},
        };
        writeBytes(path, bytes);
        expectFileDigest(
            context,
            path,
            bytes.size(),
            "faa242eb56b54b7896b0f272d7f5cd544cfd9e18ac9e48fff54743fcd9a0264b",
            "binary file with NUL bytes");
    });
}

void registerFailureCases(TestRegistry& registry)
{
    addCase(registry, "TB-BAS-037", "PR", [](TestContext& context) {
        IsolatedCaseDirectory workspace("TB-BAS-037");
        const auto path = workspace.file("missing.bin");
        const Sha256Digest originalDigest = sentinelDigest();
        Sha256Digest digest = originalDigest;
        constexpr std::uintmax_t originalSize = 777;
        std::uintmax_t size = originalSize;
        std::string error;
        const bool succeeded = nativesharp::basic::sha256File(path, digest, size, error);
        expectControlledFailure(
            context, path, originalDigest, originalSize, succeeded, digest, size, error, "missing file");
    });
    addCase(registry, "TB-BAS-038", "NIGHTLY", [](TestContext& context) {
        IsolatedCaseDirectory workspace("TB-BAS-038");
        const auto path = workspace.directory("directory-target");
        const Sha256Digest originalDigest = sentinelDigest();
        Sha256Digest digest = originalDigest;
        constexpr std::uintmax_t originalSize = 778;
        std::uintmax_t size = originalSize;
        std::string error;
        const bool succeeded = nativesharp::basic::sha256File(path, digest, size, error);
        expectControlledFailure(
            context, path, originalDigest, originalSize, succeeded, digest, size, error, "directory path");
    });
    addCase(registry, "TB-BAS-039", "NIGHTLY", [](TestContext& context) {
        IsolatedCaseDirectory workspace("TB-BAS-039");
        const auto path = workspace.file("locked.bin");
        constexpr std::array bytes{std::uint8_t{1}, std::uint8_t{2}, std::uint8_t{3}};
        writeBytes(path, bytes);
        const HANDLE lock = CreateFileW(
            path.c_str(),
            GENERIC_READ,
            0,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        context.expect(lock != INVALID_HANDLE_VALUE, "exclusive fixture handle must be acquired");
        if (lock == INVALID_HANDLE_VALUE)
        {
            return;
        }

        const Sha256Digest originalDigest = sentinelDigest();
        Sha256Digest digest = originalDigest;
        constexpr std::uintmax_t originalSize = 779;
        std::uintmax_t size = originalSize;
        std::string error;
        const bool succeeded = nativesharp::basic::sha256File(path, digest, size, error);
        CloseHandle(lock);
        expectControlledFailure(
            context, path, originalDigest, originalSize, succeeded, digest, size, error, "exclusively locked file");
    });
}

void registerMutationCase(TestRegistry& registry)
{
    addCase(registry, "TB-BAS-040", "WEEKLY", [](TestContext& context) {
        IsolatedCaseDirectory workspace("TB-BAS-040");
        const auto path = workspace.file("mutable.bin");
        constexpr std::array first{
            std::uint8_t{'f'}, std::uint8_t{'i'}, std::uint8_t{'r'}, std::uint8_t{'s'},
            std::uint8_t{'t'}, std::uint8_t{' '}, std::uint8_t{'m'}, std::uint8_t{'e'},
            std::uint8_t{'a'}, std::uint8_t{'s'}, std::uint8_t{'u'}, std::uint8_t{'r'},
            std::uint8_t{'e'}, std::uint8_t{'m'}, std::uint8_t{'e'}, std::uint8_t{'n'},
            std::uint8_t{'t'}, std::uint8_t{0x00},
        };
        constexpr std::array second{
            std::uint8_t{'s'}, std::uint8_t{'e'}, std::uint8_t{'c'}, std::uint8_t{'o'},
            std::uint8_t{'n'}, std::uint8_t{'d'}, std::uint8_t{' '}, std::uint8_t{'m'},
            std::uint8_t{'e'}, std::uint8_t{'a'}, std::uint8_t{'s'}, std::uint8_t{'u'},
            std::uint8_t{'r'}, std::uint8_t{'e'}, std::uint8_t{'m'}, std::uint8_t{'e'},
            std::uint8_t{'n'}, std::uint8_t{'t'}, std::uint8_t{0xff}, std::uint8_t{0x00},
        };
        writeBytes(path, first);
        expectFileDigest(
            context,
            path,
            first.size(),
            "da1987a56259b01f4b050a17c6c12804900ef2e07fc954b1ed2a56b2661da987",
            "first file measurement");
        writeBytes(path, second);
        expectFileDigest(
            context,
            path,
            second.size(),
            "faa61a5d12c379521789adc54e87b4a872b456ddf0d7a57248653c6de2bfea59",
            "second file measurement after mutation");
    });
}
}

void registerBasicSha256FileModule(TestRegistry& registry)
{
    registerEmptyAndBinaryCases(registry);
    registerFailureCases(registry);
    registerMutationCase(registry);
}
}
