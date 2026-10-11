#pragma once

#include "nativesharp/Source/SourceManager.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nsc::suite::source_test_support
{
enum class SourceReadMutationMode
{
    TruncateBeforeRead,
    GrowAfterRead,
};

struct SourceReadMutationPlan final
{
    std::filesystem::path path;
    SourceReadMutationMode mode = SourceReadMutationMode::TruncateBeforeRead;
    std::uintmax_t expectedBytes = 0;
    std::uintmax_t replacementBytes = 0;
    bool triggered = false;
    std::string error;
};

inline thread_local SourceReadMutationPlan* activeSourceReadMutation = nullptr;

class ScopedSourceReadMutation final
{
public:
    explicit ScopedSourceReadMutation(SourceReadMutationPlan& plan) : plan_(plan)
    {
        activeSourceReadMutation = &plan_;
    }

    ~ScopedSourceReadMutation()
    {
        if (activeSourceReadMutation == &plan_) activeSourceReadMutation = nullptr;
    }

    ScopedSourceReadMutation(const ScopedSourceReadMutation&) = delete;
    ScopedSourceReadMutation& operator=(const ScopedSourceReadMutation&) = delete;

private:
    SourceReadMutationPlan& plan_;
};
}

// A narrow standard-library facade lets the canonical SourceManager.cpp body be instantiated as
// a test-only peer. Only ifstream is adapted, and only for the current thread. All data types and
// filesystem operations remain the real standard-library types used by the product body.
namespace nsc_source_test_std
{
using ::std::char_traits;
using ::std::error_code;
using ::std::ios;
using ::std::move;
using ::std::nullopt;
using ::std::numeric_limits;
using ::std::optional;
using ::std::size_t;
using ::std::streamsize;
using ::std::string;
using ::std::string_view;
using ::std::uint32_t;
using ::std::uintmax_t;
namespace filesystem = ::std::filesystem;

class ifstream final
{
public:
    ifstream(const filesystem::path& path, ios::openmode mode)
        : path_(path), stream_(path, mode)
    {
    }

    explicit operator bool() const { return static_cast<bool>(stream_); }
    bool operator!() const { return !stream_; }

    void read(char* destination, streamsize bytes)
    {
        using namespace nsc::suite::source_test_support;
        SourceReadMutationPlan* plan = activeSourceReadMutation;
        if (plan != nullptr && plan->path == path_ && !plan->triggered &&
            plan->mode == SourceReadMutationMode::TruncateBeforeRead)
        {
            std::error_code error;
            std::filesystem::resize_file(path_, plan->replacementBytes, error);
            if (error) plan->error = error.message();
            else plan->triggered = true;
        }

        stream_.read(destination, bytes);

        if (plan != nullptr && plan->path == path_ && !plan->triggered &&
            plan->mode == SourceReadMutationMode::GrowAfterRead &&
            static_cast<std::uintmax_t>(stream_.gcount()) == plan->expectedBytes)
        {
            std::error_code error;
            std::filesystem::resize_file(path_, plan->replacementBytes, error);
            if (error) plan->error = error.message();
            else plan->triggered = true;
        }
    }

    streamsize gcount() const { return stream_.gcount(); }
    char_traits<char>::int_type peek() { return stream_.peek(); }

private:
    filesystem::path path_;
    ::std::ifstream stream_;
};
}

namespace nativesharp::source
{
// Same public surface and storage as SourceManager, with definitions instantiated from the
// canonical SourceManager.cpp body under the test-only stream facade.
class CoordinatedSourceManager final
{
public:
    static std::optional<std::string> readFileBuffer(
        const std::filesystem::path& path,
        std::string& errorMessage);
    std::optional<FileId> loadFile(const std::filesystem::path& path, std::string& errorMessage);
    FileId addBuffer(std::filesystem::path path, std::string buffer);
    const SourceFile* getFile(FileId id) const noexcept;
    SourcePosition getPosition(SourceLocation location) const noexcept;
    std::string_view getText(SourceRange range) const noexcept;
    std::string_view getLineText(SourceLocation location) const noexcept;
    std::string_view getPath(SourceLocation location) const noexcept;

private:
    std::vector<SourceFile> files_;
};
}
