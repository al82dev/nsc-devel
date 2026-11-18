#pragma once

#include <string_view>

namespace nativesharp
{
// La version visible del producto agrega metadata SemVer
// unica de cada build; sus dos valores se definen en Version.cpp, el unico consumidor del header
// generado por CMake. La sintaxis reconocida es acumulativa y no se selecciona por proyecto.
inline constexpr std::string_view kCompilerReleaseVersion = "1.0.0";
inline constexpr std::string_view kPreviousCompilerReleaseVersion = "none";
inline constexpr std::string_view kCompilerStage = "70";
extern const std::string_view kCompilerVersion;
extern const std::string_view kCompilerBuildId;
}
