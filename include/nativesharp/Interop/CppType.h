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
    Pointer, LValueReference, RValueReference, Array, Enumeration, Record, Function, Unsupported
};

struct CppRecordTypeIdentity
{
    std::string name;
    std::vector<std::string> namespaceSegments;
    std::vector<std::string> containingRecords;
    bool anonymousContext = false;
    bool specializationContext = false;
};

struct CppEnumTypeIdentity
{
    std::string name;
    std::string qualifiedName;
    bool anonymousContext = false;
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
    std::optional<std::uint64_t> sizeBits;
    std::optional<std::uint64_t> alignmentBits;
    std::optional<std::uint64_t> arrayCount;
    std::optional<CppRecordTypeIdentity> recordIdentity;
    std::optional<CppEnumTypeIdentity> enumIdentity;
    std::shared_ptr<const CppType> enumUnderlying;
    std::shared_ptr<const CppType> element;
    // Present for a captured function type when the recursion limit permits it.
    std::shared_ptr<const CppFunctionType> function;
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
