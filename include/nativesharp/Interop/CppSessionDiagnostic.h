#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace nativesharp::interop
{
enum class CppSessionDiagnosticSeverity { Note, Remark, Warning, Error, Fatal };

struct CppSourcePoint
{
    std::filesystem::path file;
    std::uint32_t line = 0;
    std::uint32_t column = 0;
};

struct CppSessionDiagnostic
{
    CppSessionDiagnosticSeverity severity = CppSessionDiagnosticSeverity::Error;
    std::string message;
    std::optional<unsigned> originalId;
    std::optional<CppSourcePoint> location;
    std::vector<CppSessionDiagnostic> notes;
};
}
