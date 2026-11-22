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
    // C++ records that enclose the enum, outermost first; their names end qualifiedName before the enum name.
    std::vector<std::string> containingRecords = {};
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
    // EN: T14-007. A pointer to a data or function member; its kind stays Unsupported and the flag lets the
    //     import say why the declaration has no NativeSharp signature.
    // ES: T14-007. Un puntero a miembro de datos o de funcion; su tipo sigue sin soporte y la marca permite
    //     decir por que la declaracion no tiene firma NativeSharp.
    bool memberPointer = false;
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

// EN: A function type NativeSharp can call: a prototype, or an unprototyped C function (`R ()`, such as the
//     target of the Windows `FARPROC`) under the rule a direct unprototyped C declaration follows: cdecl, no
//     declared parameters, called with none. C makes `R (*)()` compatible with `R (*)(void)`.
// ES: Un tipo de funcion que NativeSharp puede llamar: un prototipo, o una funcion C sin prototipo (`R ()`, como
//     el destino del `FARPROC` de Windows) con la regla de una declaracion C directa sin prototipo: cdecl, sin
//     parametros declarados, llamada sin argumentos. C hace compatibles `R (*)()` y `R (*)(void)`.
inline bool cppFunctionTypeCallable(const CppFunctionType& signature) noexcept
{
    return signature.hasPrototype ||
        (signature.callingConvention == "cdecl" && signature.parameterTypes.empty() && !signature.variadic);
}

// EN: Item 2.1. NativeSharp writes `T const**`: the readonly mark of a pointer to pointer belongs to the base type
//     (T08-D17). For a pointer whose pointee is another pointer, the mark is the `const` of the base of the chain;
//     for a single level, the `const` of the pointee, as before.
// ES: Item 2.1. NativeSharp escribe `T const**`: la marca de solo lectura de un puntero a puntero es del tipo base
//     (T08-D17). Para un puntero cuyo apuntado es otro puntero, la marca es el `const` de la base de la cadena; para
//     un solo nivel, el `const` del apuntado, como antes.
inline bool cppPointeeReadonlyForNativeSharp(const CppType& pointer) noexcept
{
    if (pointer.kind != CppTypeKind::Pointer || !pointer.element) return false;
    const CppType* current = pointer.element.get();
    while (current->kind == CppTypeKind::Pointer && current->element) current = current->element.get();
    return current->isConst;
}

// EN: Item 2.1. A pointer to pointer with `const` below its top level (`const char**`, `T* const*`,
//     `const char* const*`) is carried without those `const`: the base `const` becomes the readonly mark, and the
//     `const` of an intermediate pointer only promises that the callee does not store through it. Null when the
//     chain has no such `const`, has `volatile` below the top, or ends in an array.
// ES: Item 2.1. Un puntero a puntero con `const` bajo su nivel superior (`const char**`, `T* const*`,
//     `const char* const*`) se transporta sin esos `const`: el `const` base pasa a ser la marca de solo lectura y el
//     `const` de un puntero intermedio solo promete que el llamado no escribe a traves de el. Nulo si la cadena no
//     tiene esos `const`, tiene `volatile` bajo el nivel superior o termina en un arreglo.
inline std::optional<CppType> cppPointerChainWithoutInnerConst(const CppType& pointer)
{
    if (pointer.kind != CppTypeKind::Pointer || !pointer.element || pointer.element->kind != CppTypeKind::Pointer)
        return std::nullopt;
    bool changed = false;
    for (const CppType* level = pointer.element.get(); level; level = level->element.get())
    {
        if (level->isVolatile || level->kind == CppTypeKind::Array) return std::nullopt;
        changed = changed || level->isConst;
        if (level->kind != CppTypeKind::Pointer) break;
    }
    if (!changed) return std::nullopt;
    CppType result = pointer;
    CppType* level = &result;
    while (level->kind == CppTypeKind::Pointer && level->element)
    {
        auto copy = std::make_shared<CppType>(*level->element);
        copy->isConst = false;
        level->element = copy;
        level = copy.get();
    }
    return result;
}
}
