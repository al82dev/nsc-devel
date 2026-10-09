#include "nsc_suite/TestRegistry.h"

#include "nativesharp/Basic/ArtifactTransaction.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <array>
#include <string>
#include <string_view>
#include <utility>

namespace nsc::suite
{
namespace
{
using nativesharp::basic::OutputFailureKind;

void addCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{
        std::move(id),
        "CB-BAS-010",
        "Host.Basic",
        "unit",
        "PR",
        {"basic", "windows", "output-failure"},
        std::move(function),
    });
}

template<std::size_t Size>
void expectClassification(
    TestContext& context,
    const std::array<unsigned long, Size>& codes,
    OutputFailureKind expected,
    std::string_view expectedName,
    std::string_view description)
{
    for (const unsigned long code : codes)
    {
        const OutputFailureKind actual = nativesharp::basic::classifyWindowsOutputError(code);
        context.expect(actual == expected, std::string(description) + " must use the expected category");
        context.expect(
            nativesharp::basic::outputFailureKindName(actual) == expectedName,
            std::string(description) + " must use the expected stable name");
    }
}
}

void registerBasicOutputFailureModule(TestRegistry& registry)
{
    addCase(registry, "TB-BAS-041", [](TestContext& context) {
        expectClassification(
            context,
            std::array{static_cast<unsigned long>(ERROR_SUCCESS)},
            OutputFailureKind::None,
            "none",
            "successful Win32 result");
    });
    addCase(registry, "TB-BAS-042", [](TestContext& context) {
        expectClassification(
            context,
            std::array{
                static_cast<unsigned long>(ERROR_FILENAME_EXCED_RANGE),
                static_cast<unsigned long>(ERROR_BUFFER_OVERFLOW),
            },
            OutputFailureKind::PathLimit,
            "path-limit",
            "Win32 path limit result");
    });
    addCase(registry, "TB-BAS-043", [](TestContext& context) {
        expectClassification(
            context,
            std::array{
                static_cast<unsigned long>(ERROR_FILE_NOT_FOUND),
                static_cast<unsigned long>(ERROR_PATH_NOT_FOUND),
                static_cast<unsigned long>(ERROR_INVALID_DRIVE),
                static_cast<unsigned long>(ERROR_DIRECTORY),
            },
            OutputFailureKind::MissingComponent,
            "missing-component",
            "Win32 missing component result");
    });
    addCase(registry, "TB-BAS-044", [](TestContext& context) {
        expectClassification(
            context,
            std::array{
                static_cast<unsigned long>(ERROR_ACCESS_DENIED),
                static_cast<unsigned long>(ERROR_PRIVILEGE_NOT_HELD),
                static_cast<unsigned long>(ERROR_WRITE_PROTECT),
            },
            OutputFailureKind::PermissionDenied,
            "permission-denied",
            "Win32 permission result");
    });
    addCase(registry, "TB-BAS-045", [](TestContext& context) {
        expectClassification(
            context,
            std::array{
                static_cast<unsigned long>(ERROR_SHARING_VIOLATION),
                static_cast<unsigned long>(ERROR_LOCK_VIOLATION),
                static_cast<unsigned long>(ERROR_USER_MAPPED_FILE),
            },
            OutputFailureKind::FileLocked,
            "file-locked",
            "Win32 file lock result");
    });
    addCase(registry, "TB-BAS-046", [](TestContext& context) {
        expectClassification(
            context,
            std::array{
                static_cast<unsigned long>(ERROR_DISK_FULL),
                static_cast<unsigned long>(ERROR_HANDLE_DISK_FULL),
            },
            OutputFailureKind::InsufficientSpace,
            "insufficient-space",
            "Win32 insufficient space result");
    });
    addCase(registry, "TB-BAS-047", [](TestContext& context) {
        expectClassification(
            context,
            std::array{0xdeadbeefUL},
            OutputFailureKind::IoError,
            "io-error",
            "unclassified Win32 result");
    });
    addCase(registry, "TB-BAS-048", [](TestContext& context) {
        const auto invalid = static_cast<OutputFailureKind>(0xff);
        context.expect(
            nativesharp::basic::outputFailureKindName(invalid) == "io-error",
            "invalid output failure kind must use the io-error fallback name");
    });
}
}
