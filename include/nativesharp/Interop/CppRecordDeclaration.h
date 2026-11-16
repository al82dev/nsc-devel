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
    // EN: T08-D14: non-empty when the aggregate is the unnamed type of a named member (`struct { ... } pair;`)
    //     instead of an anonymous carrier; its members are not promoted. Other members of the same parent declared
    //     with that same unnamed type (`struct { ... } a, b;`) share it.
    // ES: T08-D14: no vacio cuando el agregado es el tipo sin nombre de un miembro con nombre (`struct { ... }
    //     pair;`) en lugar de un portador anonimo; sus miembros no se promueven. Otros miembros del mismo padre
    //     declarados con ese mismo tipo sin nombre (`struct { ... } a, b;`) lo comparten.
    std::string memberName;
    std::vector<std::size_t> siblingMemberIndices;
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
    // T11-007: Clang's triviality of a special member; a trivial destructor has no effect to call.
    bool isTrivial = false;
    // EN: T12-004. A method declared in a base and reached through this record, and the symbol of the
    //     Clang adapter that calls an inherited or virtual method on this record (cppMemberThunkSymbol).
    // ES: T12-004. Un metodo declarado en una base y alcanzado por este record, y el simbolo del adaptador
    //     de Clang que llama a un metodo heredado o virtual sobre este record (cppMemberThunkSymbol).
    bool inherited = false;
    std::optional<std::string> dispatchSymbol;
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
    // EN: T19-036. A C++ thread_local whose access Clang guards with the CRT's on-demand TLS initialization
    //     (no constinit and no visible constant-initialized definition), as for a global (T10-004).
    // ES: T19-036. Un thread_local C++ cuyo acceso Clang protege con la inicializacion TLS bajo demanda del CRT
    //     (sin constinit ni definicion visible de inicializacion constante), como en una global (T10-004).
    bool cppTlsOnDemand = false;
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

// EN: T12-003. A base reached from the record through public non-virtual derivation only, by exactly one
//     path, with the offset Clang gives its subobject. A pointer converts to it as in C++.
// ES: T12-003. Una base alcanzada desde el record solo por derivacion publica no virtual, por un unico camino,
//     con el offset que Clang da a su subobjeto. Un puntero se convierte a ella como en C++.
struct CppRecordUpcast
{
    std::string baseClangUsr;
    std::uint64_t offsetBytes = 0;
};

struct CppImplicitLifetimeMembers
{
    bool defaultConstructor = false;
    bool copyConstructor = false;
    bool moveConstructor = false;
    bool destructor = false;
    bool operator==(const CppImplicitLifetimeMembers&) const = default;
};

// Clang observations about a complete C++ record. They do not by themselves
// establish a calling convention or make the record safe to pass by value.
struct CppRecordValueTraits
{
    bool triviallyCopyable = false;
    bool standardLayout = false;
    bool trivialDefaultConstructor = false;
    bool trivialDestructor = false;
    bool plainOldData = false;
    bool hasNonLiteralOrVolatileFieldsOrBases = false;
    bool operator==(const CppRecordValueTraits&) const = default;
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
    std::vector<CppRecordUpcast> upcasts;
    std::vector<CppRecordField> fields;
    std::vector<CppStaticField> staticFields;
    std::vector<CppMethodDeclaration> methods;
    // Identifies the language used to interpret this declaration; C and C++
    // unions have different member lifetime rules.
    bool isCxx = true;
    std::optional<std::string> clangUsr;
    std::vector<CppAnonymousCRecord> anonymousCRecords;
    bool isAnonymousCInternal = false;
    // Absent for incomplete declarations, C records, and unavailable traits.
    std::optional<CppRecordValueTraits> valueTraits;
    // T12-001: Clang's Microsoft ABI classification of this record returned by value from a free function,
    // observed for trivially copyable C++ records the C rules may not describe (bases, constructors, access).
    // T12-001: clasificacion del ABI de Microsoft de Clang de este record devuelto por valor desde una funcion
    // libre, observada para records C++ trivialmente copiables que las reglas de C quiza no describen.
    std::optional<bool> indirectReturn;
};
}
