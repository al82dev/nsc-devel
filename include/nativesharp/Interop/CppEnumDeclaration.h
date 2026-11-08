#pragma once

#include "nativesharp/Interop/CppSessionDiagnostic.h"
#include "nativesharp/Interop/CppType.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace nativesharp::interop
{
struct CppEnumeratorDeclaration
{
    std::string name;
    // Exact decimal spelling preserves values outside the int64_t range.
    std::string decimalValue;
    std::uint32_t valueBits = 0;
    bool valueIsUnsigned = false;
    std::optional<CppSourcePoint> location;
    // Type of the enumerator expression, which need not be the enum's storage type.
    CppType valueType;
    std::optional<std::string> clangUsr;
};

struct CppEnumDeclaration
{
    struct SourceProvenance
    {
        std::filesystem::path sourcePath;
        std::string bufferSha256;
        std::uint32_t byteOffset = 0;
        bool operator==(const SourceProvenance&) const = default;
    };
    // Anonymous enums have empty names but may still contain usable constants.
    std::string name;
    std::string qualifiedName;
    bool complete = false;
    bool scoped = false;
    std::optional<CppType> underlyingType;
    std::optional<CppSourcePoint> location;
    std::vector<CppEnumeratorDeclaration> enumerators;
    bool isCxx = true;
    std::optional<std::uint64_t> sizeBits;
    std::optional<std::uint64_t> alignmentBits;
    std::optional<std::string> clangUsr;
    // From Clang's parsed buffer; unavailable for macro locations.
    std::optional<SourceProvenance> sourceProvenance;
    // Session-owned evidence for a virtual main input; never serialized.
    std::shared_ptr<const std::string> transientSourceBuffer;
};
}
