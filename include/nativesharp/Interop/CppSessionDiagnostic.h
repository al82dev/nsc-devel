#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
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

// EN: T17-001, T17-004. "file:line:column" of a C/C++ source point, the path in UTF-8 with forward slashes.
// ES: T17-001, T17-004. "archivo:linea:columna" de un punto C/C++, con la ruta en UTF-8 y barras normales.
inline std::string describeCppSourcePoint(const CppSourcePoint& point)
{
    const auto encoded = point.file.generic_u8string();
    return std::string(reinterpret_cast<const char*>(encoded.data()), encoded.size()) + ":" +
        std::to_string(point.line) + ":" + std::to_string(point.column);
}

// EN: T17-004. A Clang diagnostic as one line, preceded by its location when Clang gave one.
// ES: T17-004. Un diagnostico de Clang en una linea, precedido de su ubicacion si Clang la dio.
inline std::string describeCppDiagnostic(const CppSessionDiagnostic& diagnostic)
{
    return diagnostic.location ? describeCppSourcePoint(*diagnostic.location) + ": " + diagnostic.message
                               : diagnostic.message;
}
}
