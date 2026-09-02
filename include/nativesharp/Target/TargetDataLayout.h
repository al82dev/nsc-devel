#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace nativesharp::target
{
enum class BuiltinLayoutKind
{
    Bool,
    // ID 89. `sbyte` mide lo mismo que `byte` -- un byte -- y se distingue de el en el SIGNO,
    // que esta capa no modela: aqui solo viven tamano y alineacion. La distincion la hacen las
    // tablas de signo del frontend, del MIR y del backend.
    SByte,
    Byte,
    Char,
    Short,
    UShort,
    Int,
    UInt,
    Long,
    ULong,
    NInt,
    NUInt,
    Float,
    Double,
    Enum,
    Pointer
};

struct TypeLayout
{
    std::uint64_t size;
    std::uint64_t alignment;
};

struct ArrayLayout
{
    std::uint64_t size;
    std::uint64_t alignment;
    std::uint64_t stride;
    std::uint64_t count;
};

struct StructFieldLayout
{
    std::uint64_t offset;
    std::uint64_t size;
    std::uint64_t alignment;
};

struct StructLayout
{
    std::uint64_t size;
    std::uint64_t alignment;
    std::vector<StructFieldLayout> fields;
};

// C2.2 "union and explicit layout". Everything a C aggregate can say about WHERE its fields sit,
// expressed once, at the layer that owns the target's layout rules:
//
//   * isUnion       -- every field starts at the same place (offset 0 unless it names its own),
//                      and the aggregate is as big as its largest member. `union` in C.
//   * pack          -- an upper bound on every field's alignment, so a field is placed tighter
//                      than its type would naturally ask for. `#pragma pack(N)` in C. 0 = natural.
//   * minimumAlignment -- a lower bound on the AGGREGATE's alignment. `__declspec(align(N))` /
//                      `alignas(N)` in C. 0 = whatever the fields imply.
//
// A field may also name its own byte offset (StructFieldRequest::explicitOffset), which is how
// Win32's `INPUT` and every other hand-laid-out record is expressed. Overlap is not an error
// there: overlapping IS the feature.
struct StructLayoutOptions
{
    bool isUnion = false;
    std::uint64_t pack = 0;
    std::uint64_t minimumAlignment = 0;
};

struct StructFieldRequest
{
    TypeLayout layout;
    std::optional<std::uint64_t> explicitOffset;
};

class TargetDataLayout final
{
public:
    static TargetDataLayout WindowsX64();

    TypeLayout layoutOf(BuiltinLayoutKind kind) const noexcept;
    std::optional<std::uint64_t> strideOf(TypeLayout element) const noexcept;
    std::optional<ArrayLayout> layoutArray(TypeLayout element, std::uint64_t count) const noexcept;
    std::optional<StructLayout> layoutStruct(std::span<const TypeLayout> fields) const;
    // C2.2: the general form. The one-argument overload above is exactly this with default
    // options, kept because most callers have nothing to say about layout.
    std::optional<StructLayout> layoutStruct(
        std::span<const StructFieldRequest> fields, const StructLayoutOptions& options) const;

    std::uint64_t maximumNaturalAlignment() const noexcept;
    bool isLittleEndian() const noexcept;

private:
    static std::optional<std::uint64_t> checkedAdd(
        std::uint64_t left,
        std::uint64_t right) noexcept;

    static std::optional<std::uint64_t> checkedMultiply(
        std::uint64_t left,
        std::uint64_t right) noexcept;

    static std::optional<std::uint64_t> alignUp(
        std::uint64_t value,
        std::uint64_t alignment) noexcept;
};
}
