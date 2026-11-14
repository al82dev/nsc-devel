#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace nativesharp::interop
{
struct CppFunctionType;
enum class CppTypeKind
{
    Void, Boolean, SignedInteger, UnsignedInteger, Floating,
    Pointer, LValueReference, RValueReference, Array, Enumeration, Record, Function, Unsupported,
    // Captured for analysis only; older metadata codecs deliberately reject this kind.
    Atomic,
    // A fixed Clang vector retains its lane type, count and physical layout.
    FixedVector
};

struct CppRecordTypeIdentity
{
    std::string name;
    std::vector<std::string> namespaceSegments;
    std::vector<std::string> containingRecords;
    bool anonymousContext = false;
    bool specializationContext = false;
    std::optional<std::string> clangUsr = std::nullopt;
};

struct CppEnumTypeIdentity
{
    std::string name;
    std::string qualifiedName;
    bool anonymousContext = false;
    std::optional<std::string> clangUsr = std::nullopt;
};

// Sizes describe a C/C++ object type, not the ABI rules for passing or returning it.
struct CppType
{
    CppTypeKind kind = CppTypeKind::Unsupported;
    std::string spelling;
    std::string canonicalSpelling;
    std::string unqualifiedCanonicalSpelling;
    bool isConst = false;
    bool isVolatile = false;
    bool isRestrict = false;
    std::optional<std::uint64_t> sizeBits;
    std::optional<std::uint64_t> alignmentBits;
    std::optional<std::uint64_t> arrayCount;
    // True for T[], distinct from variable or dependent bounds and T[0].
    bool isIncompleteArray = false;
    std::optional<CppRecordTypeIdentity> recordIdentity;
    std::optional<CppEnumTypeIdentity> enumIdentity;
    std::shared_ptr<const CppType> enumUnderlying;
    std::shared_ptr<const CppType> element;
    // Present for a captured function type when the recursion limit permits it.
    std::shared_ptr<const CppFunctionType> function;
    // Set for long double only when Clang confirms IEEE binary64 semantics.
    bool ieeeBinary64 = false;
};

// A description of the Clang function type, not a promise that NSC can call it.
struct CppFunctionType
{
    CppType returnType;
    std::vector<CppType> parameterTypes;
    std::string callingConvention;
    bool hasPrototype = false;
    bool variadic = false;
};
}
