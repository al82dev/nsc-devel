#pragma once

#include "nativesharp/Interop/CppFunctionDeclaration.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nativesharp::interop
{
enum class CppAccess { Public, Protected, Private, None };
enum class CppRecordKind { Struct, Class, Union };
enum class CppMethodKind { Method, Constructor, Destructor };
enum class CppMethodRefQualifier { None, LValue, RValue };

struct CppRecordField
{
    std::string name;
    CppType type;
    CppAccess access = CppAccess::None;
    std::optional<std::uint64_t> offsetBits;
    std::optional<unsigned> bitWidth;
    std::optional<std::string> clangUsr;
    // C11 anonymous struct/union carrier, distinct from an unnamed bit-field.
    bool isAnonymousCarrier = false;
};

// Physical anonymous C11 aggregates, addressed by field indices from their named owner.
struct CppAnonymousCRecord
{
    std::vector<std::size_t> carrierPath;
    CppRecordKind kind = CppRecordKind::Struct;
    std::uint64_t sizeBits = 0;
    std::uint64_t alignmentBits = 0;
    std::vector<CppRecordField> fields;
};

struct CppMethodDeclaration
{
    CppMethodKind kind = CppMethodKind::Method;
    std::string name;
    std::string qualifiedName;
    std::string canonicalFunctionType;
    std::string callingConvention;
    CppMethodRefQualifier refQualifier = CppMethodRefQualifier::None;
    CppType returnType;
    std::vector<CppFunctionParameter> parameters;
    CppAccess access = CppAccess::None;
    bool isStatic = false;
    bool isConst = false;
    bool isVolatile = false;
    // True only when Clang has resolved a non-throwing exception specification.
    bool isNonThrowing = false;
    bool dllImport = false;
    bool isVirtual = false;
    bool isPureVirtual = false;
    bool isDeleted = false;
    bool variadic = false;
    // Constructor/destructor names are selected for complete-object use; on the MS ABI
    // Clang uses the base destructor when the class has no virtual bases.
    // A symbol name does not guarantee a particular library defines it.
    std::optional<std::string> externalSymbol;
    std::optional<std::string> clangUsr;
};

struct CppStaticField
{
    std::string name;
    CppType type;
    CppAccess access = CppAccess::None;
    bool threadLocal = false;
    bool dllImport = false;
    std::optional<std::string> externalSymbol;
    std::optional<std::string> clangUsr;
};

struct CppBaseDeclaration
{
    CppType type;
    CppAccess access = CppAccess::None;
    bool isVirtual = false;
    std::optional<std::uint64_t> offsetBits;
};

struct CppImplicitLifetimeMembers
{
    bool defaultConstructor = false;
    bool copyConstructor = false;
    bool moveConstructor = false;
    bool destructor = false;
    bool operator==(const CppImplicitLifetimeMembers&) const = default;
};

struct CppRecordDeclaration
{
    struct SourceProvenance
    {
        std::filesystem::path sourcePath;
        std::string bufferSha256;
        std::uint32_t byteOffset = 0;
        bool operator==(const SourceProvenance&) const = default;
    };
    std::string name;
    std::string qualifiedName;
    std::vector<std::string> namespaceSegments;
    std::vector<std::string> containingRecords;
    bool anonymousContext = false;
    bool specializationContext = false;
    CppRecordKind kind = CppRecordKind::Struct;
    CppAccess access = CppAccess::None;
    bool complete = false;
    std::optional<std::uint64_t> sizeBits;
    std::optional<std::uint64_t> alignmentBits;
    // Clang ODRHash is a diagnostic fingerprint, not proof of ABI or ODR equivalence.
    std::optional<std::uint32_t> odrHash;
    // Presence only, including lazily declared members. Does not prove viability or symbol emission.
    std::optional<CppImplicitLifetimeMembers> implicitLifetimeMembers;
    std::optional<CppSourcePoint> location;
    // Captured from Clang's parsed buffer; unavailable for macro locations.
    std::optional<SourceProvenance> sourceProvenance;
    std::vector<CppBaseDeclaration> bases;
    std::vector<CppRecordField> fields;
    std::vector<CppStaticField> staticFields;
    std::vector<CppMethodDeclaration> methods;
    // Identifies the language used to interpret this declaration; C and C++
    // unions have different member lifetime rules.
    bool isCxx = true;
    std::optional<std::string> clangUsr;
    std::vector<CppAnonymousCRecord> anonymousCRecords;
    bool isAnonymousCInternal = false;
};
}
