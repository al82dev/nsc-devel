#include "nativesharp/Target/TargetDataLayout.h"
#include "nativesharp/Target/TargetInfo.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

namespace
{
using nativesharp::target::ArrayLayout;
using nativesharp::target::BuiltinLayoutKind;
using nativesharp::target::StructLayout;
using nativesharp::target::TargetDataLayout;
using nativesharp::target::TargetInfo;
using nativesharp::target::TypeLayout;

int failures = 0;

void expect(bool condition, std::string_view message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void expectLayout(TypeLayout actual, std::uint64_t size, std::uint64_t alignment, std::string_view name)
{
    expect(actual.size == size, std::string(name) + " size");
    expect(actual.alignment == alignment, std::string(name) + " alignment");
}

void testTargetInfo()
{
    const TargetInfo target = TargetInfo::WindowsX64();
    expect(target.architectureName() == "x86-64", "target architecture");
    expect(target.operatingSystemName() == "windows", "target operating system");
    expect(target.objectFormatName() == "COFF", "target object format");
    expect(target.llvmTriple() == "x86_64-pc-windows-msvc", "target LLVM triple");
}

void testScalarLayouts()
{
    const TargetDataLayout layout = TargetDataLayout::WindowsX64();

    expectLayout(layout.layoutOf(BuiltinLayoutKind::Bool), 1, 1, "bool");
    expectLayout(layout.layoutOf(BuiltinLayoutKind::Byte), 1, 1, "byte");
    expectLayout(layout.layoutOf(BuiltinLayoutKind::Char), 1, 1, "char");
    expectLayout(layout.layoutOf(BuiltinLayoutKind::Short), 2, 2, "short");
    expectLayout(layout.layoutOf(BuiltinLayoutKind::UShort), 2, 2, "ushort");
    expectLayout(layout.layoutOf(BuiltinLayoutKind::Int), 4, 4, "int");
    expectLayout(layout.layoutOf(BuiltinLayoutKind::UInt), 4, 4, "uint");
    expectLayout(layout.layoutOf(BuiltinLayoutKind::Long), 8, 8, "long");
    expectLayout(layout.layoutOf(BuiltinLayoutKind::ULong), 8, 8, "ulong");
    expectLayout(layout.layoutOf(BuiltinLayoutKind::NInt), 8, 8, "nint");
    expectLayout(layout.layoutOf(BuiltinLayoutKind::NUInt), 8, 8, "nuint");
    expectLayout(layout.layoutOf(BuiltinLayoutKind::Float), 4, 4, "float");
    expectLayout(layout.layoutOf(BuiltinLayoutKind::Double), 8, 8, "double");
    expectLayout(layout.layoutOf(BuiltinLayoutKind::Enum), 4, 4, "enum");
    expectLayout(layout.layoutOf(BuiltinLayoutKind::Pointer), 8, 8, "pointer");
    expect(layout.isLittleEndian(), "little endian target");
    expect(layout.maximumNaturalAlignment() == 8, "maximum natural alignment");
}

void testArrays()
{
    const TargetDataLayout layout = TargetDataLayout::WindowsX64();
    const TypeLayout intLayout = layout.layoutOf(BuiltinLayoutKind::Int);

    const std::optional<ArrayLayout> array = layout.layoutArray(intLayout, 100);
    expect(array.has_value(), "int[100] layout exists");

    if (array.has_value())
    {
        expect(array->size == 400, "int[100] size");
        expect(array->alignment == 4, "int[100] alignment");
        expect(array->stride == 4, "int[100] stride");
        expect(array->count == 100, "int[100] count");
    }

    expect(!layout.layoutArray(intLayout, 0).has_value(), "fixed array count zero rejected");

    const TypeLayout hugeElement{std::numeric_limits<std::uint64_t>::max(), 1};
    expect(!layout.layoutArray(hugeElement, 2).has_value(), "array size overflow rejected");
}

void testStructs()
{
    const TargetDataLayout layout = TargetDataLayout::WindowsX64();
    const TypeLayout intLayout = layout.layoutOf(BuiltinLayoutKind::Int);
    const TypeLayout byteLayout = layout.layoutOf(BuiltinLayoutKind::Byte);
    const TypeLayout shortLayout = layout.layoutOf(BuiltinLayoutKind::Short);

    const std::array<TypeLayout, 2> pointFields = {intLayout, intLayout};
    const std::optional<StructLayout> point = layout.layoutStruct(pointFields);
    expect(point.has_value(), "Point layout exists");

    if (point.has_value())
    {
        expect(point->size == 8, "Point size");
        expect(point->alignment == 4, "Point alignment");
        expect(point->fields.size() == 2, "Point field count");
        expect(point->fields[0].offset == 0, "Point.X offset");
        expect(point->fields[1].offset == 4, "Point.Y offset");
    }

    const std::array<TypeLayout, 3> mixedFields = {byteLayout, intLayout, shortLayout};
    const std::optional<StructLayout> mixed = layout.layoutStruct(mixedFields);
    expect(mixed.has_value(), "Mixed layout exists");

    if (mixed.has_value())
    {
        expect(mixed->size == 12, "Mixed size");
        expect(mixed->alignment == 4, "Mixed alignment");
        expect(mixed->fields[0].offset == 0, "Mixed.A offset");
        expect(mixed->fields[1].offset == 4, "Mixed.B offset");
        expect(mixed->fields[2].offset == 8, "Mixed.C offset");
    }

    const std::array<TypeLayout, 0> noFields = {};
    const std::optional<StructLayout> empty = layout.layoutStruct(noFields);
    expect(empty.has_value(), "empty struct layout exists");

    if (empty.has_value())
    {
        expect(empty->size == 1, "empty struct size");
        expect(empty->alignment == 1, "empty struct alignment");
    }
}
}

int main()
{
    testTargetInfo();
    testScalarLayouts();
    testArrays();
    testStructs();

    if (failures != 0)
    {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "all TargetDataLayout tests passed\n";
    return EXIT_SUCCESS;
}
