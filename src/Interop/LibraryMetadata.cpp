#include "nativesharp/Interop/LibraryMetadata.h"

#include "nativesharp/Basic/Version.h"
#include "nativesharp/Frontend/EnumUnderlyingType.h"
#include "nativesharp/Compilation/Project.h"
#include "nativesharp/Compilation/ProjectGraph.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "nativesharp/Frontend/ProjectInheritanceModel.h"
#include "nativesharp/Frontend/ProjectInterfaceModel.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectObjectLayoutModel.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <set>
#include <span>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace nativesharp::interop
{
namespace
{
constexpr std::array<std::uint8_t, 8> kMagic = {'N', 'S', 'M', 'E', 'T', 'A', '1', 0};
constexpr std::uint16_t kFormatMajor = kMetadataFormatMajor;
constexpr std::uint16_t kFormatMinor = kMetadataFormatMinor;
constexpr std::uint32_t kEndianMarker = 0x01020304U;
constexpr std::uint32_t kWindowsX64Target = 1;
constexpr std::uint32_t kPointerSize = 8;
constexpr std::uint32_t kPointerAlignment = 8;
constexpr std::string_view kTargetTriple = "x86_64-pc-windows-msvc";
constexpr std::size_t kHeaderSize = 100;
constexpr std::uint64_t kMaximumPayloadSize = 128ULL * 1024ULL * 1024ULL;
constexpr std::uint32_t kMaximumRecordCount = 1'000'000U;
constexpr std::uint32_t kMaximumStringSize = 16U * 1024U * 1024U;

class ByteWriter final
{
public:
    void u8(std::uint8_t value) { bytes_.push_back(value); }

    void u16(std::uint16_t value)
    {
        u8(static_cast<std::uint8_t>(value & 0xffU));
        u8(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
    }

    void u32(std::uint32_t value)
    {
        for (unsigned shift = 0; shift < 32U; shift += 8U)
        {
            u8(static_cast<std::uint8_t>((value >> shift) & 0xffU));
        }
    }

    void u64(std::uint64_t value)
    {
        for (unsigned shift = 0; shift < 64U; shift += 8U)
        {
            u8(static_cast<std::uint8_t>((value >> shift) & 0xffU));
        }
    }

    void raw(std::span<const std::uint8_t> bytes)
    {
        bytes_.insert(bytes_.end(), bytes.begin(), bytes.end());
    }

    void text(std::string_view value)
    {
        u32(static_cast<std::uint32_t>(value.size()));
        bytes_.insert(bytes_.end(), value.begin(), value.end());
    }

    const std::vector<std::uint8_t>& bytes() const noexcept { return bytes_; }

private:
    std::vector<std::uint8_t> bytes_;
};

class ByteReader final
{
public:
    explicit ByteReader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

    bool u8(std::uint8_t& value)
    {
        if (position_ >= bytes_.size()) return false;
        value = bytes_[position_++];
        return true;
    }

    bool u16(std::uint16_t& value)
    {
        std::uint8_t a = 0;
        std::uint8_t b = 0;
        if (!u8(a) || !u8(b)) return false;
        value = static_cast<std::uint16_t>(a) |
            static_cast<std::uint16_t>(static_cast<std::uint16_t>(b) << 8U);
        return true;
    }

    bool u32(std::uint32_t& value)
    {
        value = 0;
        for (unsigned shift = 0; shift < 32U; shift += 8U)
        {
            std::uint8_t byte = 0;
            if (!u8(byte)) return false;
            value |= static_cast<std::uint32_t>(byte) << shift;
        }
        return true;
    }

    bool u64(std::uint64_t& value)
    {
        value = 0;
        for (unsigned shift = 0; shift < 64U; shift += 8U)
        {
            std::uint8_t byte = 0;
            if (!u8(byte)) return false;
            value |= static_cast<std::uint64_t>(byte) << shift;
        }
        return true;
    }

    bool text(std::string& value)
    {
        std::uint32_t size = 0;
        if (!u32(size) || size > kMaximumStringSize || remaining() < size) return false;
        value.assign(
            reinterpret_cast<const char*>(bytes_.data() + position_),
            static_cast<std::size_t>(size));
        position_ += size;
        return true;
    }

    bool raw(std::span<std::uint8_t> destination)
    {
        if (remaining() < destination.size()) return false;
        std::copy_n(bytes_.begin() + static_cast<std::ptrdiff_t>(position_), destination.size(), destination.begin());
        position_ += destination.size();
        return true;
    }

    std::size_t remaining() const noexcept { return bytes_.size() - position_; }
    bool atEnd() const noexcept { return position_ == bytes_.size(); }

private:
    std::span<const std::uint8_t> bytes_;
    std::size_t position_ = 0;
};

std::uint32_t rotateRight(std::uint32_t value, unsigned amount) noexcept
{
    return (value >> amount) | (value << (32U - amount));
}

std::array<std::uint8_t, 32> sha256(std::span<const std::uint8_t> input)
{
    static constexpr std::array<std::uint32_t, 64> constants = {
        0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
        0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
        0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
        0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
        0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
        0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
        0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
        0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U};

    std::array<std::uint32_t, 8> state = {
        0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
        0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};

    const std::uint64_t bitLength = static_cast<std::uint64_t>(input.size()) * 8ULL;
    const std::size_t paddedSize = ((input.size() + 9U + 63U) / 64U) * 64U;
    std::vector<std::uint8_t> padded(paddedSize, 0);
    std::copy(input.begin(), input.end(), padded.begin());
    padded[input.size()] = 0x80U;
    for (unsigned byte = 0; byte < 8U; ++byte)
    {
        padded[paddedSize - 1U - byte] = static_cast<std::uint8_t>((bitLength >> (byte * 8U)) & 0xffU);
    }

    for (std::size_t block = 0; block < padded.size(); block += 64U)
    {
        std::array<std::uint32_t, 64> words{};
        for (std::size_t index = 0; index < 16U; ++index)
        {
            const std::size_t offset = block + index * 4U;
            words[index] =
                (static_cast<std::uint32_t>(padded[offset]) << 24U) |
                (static_cast<std::uint32_t>(padded[offset + 1U]) << 16U) |
                (static_cast<std::uint32_t>(padded[offset + 2U]) << 8U) |
                static_cast<std::uint32_t>(padded[offset + 3U]);
        }
        for (std::size_t index = 16; index < words.size(); ++index)
        {
            const std::uint32_t x = words[index - 15U];
            const std::uint32_t y = words[index - 2U];
            const std::uint32_t s0 = rotateRight(x, 7U) ^ rotateRight(x, 18U) ^ (x >> 3U);
            const std::uint32_t s1 = rotateRight(y, 17U) ^ rotateRight(y, 19U) ^ (y >> 10U);
            words[index] = words[index - 16U] + s0 + words[index - 7U] + s1;
        }

        std::uint32_t a = state[0];
        std::uint32_t b = state[1];
        std::uint32_t c = state[2];
        std::uint32_t d = state[3];
        std::uint32_t e = state[4];
        std::uint32_t f = state[5];
        std::uint32_t g = state[6];
        std::uint32_t h = state[7];
        for (std::size_t index = 0; index < words.size(); ++index)
        {
            const std::uint32_t s1 = rotateRight(e, 6U) ^ rotateRight(e, 11U) ^ rotateRight(e, 25U);
            const std::uint32_t choice = (e & f) ^ ((~e) & g);
            const std::uint32_t temp1 = h + s1 + choice + constants[index] + words[index];
            const std::uint32_t s0 = rotateRight(a, 2U) ^ rotateRight(a, 13U) ^ rotateRight(a, 22U);
            const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t temp2 = s0 + majority;
            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }
        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
        state[5] += f;
        state[6] += g;
        state[7] += h;
    }

    std::array<std::uint8_t, 32> digest{};
    for (std::size_t index = 0; index < state.size(); ++index)
    {
        digest[index * 4U] = static_cast<std::uint8_t>((state[index] >> 24U) & 0xffU);
        digest[index * 4U + 1U] = static_cast<std::uint8_t>((state[index] >> 16U) & 0xffU);
        digest[index * 4U + 2U] = static_cast<std::uint8_t>((state[index] >> 8U) & 0xffU);
        digest[index * 4U + 3U] = static_cast<std::uint8_t>(state[index] & 0xffU);
    }
    return digest;
}

std::uint64_t stableTypeId(std::string_view value) noexcept
{
    std::uint64_t hash = 14695981039346656037ULL;
    for (const char character : value)
    {
        hash ^= static_cast<std::uint64_t>(static_cast<unsigned char>(character));
        hash *= 1099511628211ULL;
    }
    return hash;
}

std::string typeKeyForOrdinal(
    std::span<const frontend::NamedTypeDeclaration> types,
    std::size_t ordinal)
{
    if (ordinal >= types.size()) return {};
    return "type:" + types[ordinal].identity.key();
}

std::optional<std::size_t> rootProjectOrdinal(const frontend::ProjectCompilation& compilation)
{
    if (compilation.projectGraph() == nullptr || compilation.projectGraph()->rootProject() == nullptr)
    {
        return std::nullopt;
    }
    const auto projects = compilation.projectGraph()->projects();
    const compilation::Project* root = compilation.projectGraph()->rootProject();
    for (std::size_t ordinal = 0; ordinal < projects.size(); ++ordinal)
    {
        if (&projects[ordinal] == root) return ordinal;
    }
    return std::nullopt;
}

LibraryMetadataTypeKind metadataKind(frontend::NamedTypeDeclarationKind kind)
{
    switch (kind)
    {
        case frontend::NamedTypeDeclarationKind::Class: return LibraryMetadataTypeKind::Class;
        case frontend::NamedTypeDeclarationKind::Interface: return LibraryMetadataTypeKind::Interface;
        case frontend::NamedTypeDeclarationKind::Struct: return LibraryMetadataTypeKind::Struct;
        case frontend::NamedTypeDeclarationKind::Enum: return LibraryMetadataTypeKind::Enum;
        case frontend::NamedTypeDeclarationKind::Delegate: return LibraryMetadataTypeKind::Delegate;
    }
    return LibraryMetadataTypeKind::Struct;
}

LibraryMetadataOwnershipKind metadataOwnership(sema::OwnershipKind ownership) noexcept
{
    switch (ownership)
    {
        case sema::OwnershipKind::None: return LibraryMetadataOwnershipKind::None;
        case sema::OwnershipKind::Owned: return LibraryMetadataOwnershipKind::Owned;
        case sema::OwnershipKind::Borrowed: return LibraryMetadataOwnershipKind::Borrowed;
    }
    return LibraryMetadataOwnershipKind::None;
}

bool readOwnership(
    ByteReader& reader,
    std::uint16_t formatMinor,
    LibraryMetadataOwnershipKind& ownership)
{
    if (formatMinor == 0)
    {
        ownership = LibraryMetadataOwnershipKind::None;
        return true;
    }

    std::uint8_t raw = 0;
    if (!reader.u8(raw) || raw > static_cast<std::uint8_t>(LibraryMetadataOwnershipKind::Borrowed))
    {
        return false;
    }
    ownership = static_cast<LibraryMetadataOwnershipKind>(raw);
    return true;
}

std::uint16_t typeFlags(const frontend::NamedTypeDeclaration& type) noexcept
{
    std::uint16_t flags = 0;
    if (type.isPartial) flags |= 1U << 0U;
    if (type.isAbstract) flags |= 1U << 1U;
    if (type.isSealed) flags |= 1U << 2U;
    return flags;
}

std::uint32_t memberFlags(const frontend::ClassMemberSymbol& member) noexcept
{
    std::uint32_t flags = 0;
    if (member.isStatic) flags |= 1U << 0U;
    if (member.isReadonly) flags |= 1U << 1U;
    if (member.isConst) flags |= 1U << 2U;
    if (member.isVirtual) flags |= 1U << 3U;
    if (member.isOverride) flags |= 1U << 4U;
    if (member.isAbstract) flags |= 1U << 5U;
    if (member.isSealed) flags |= 1U << 6U;
    if (member.isExported) flags |= 1U << 7U;
    if (member.isExternC) flags |= 1U << 8U;
    if (member.isExportedC) flags |= 1U << 9U;
    if (member.isCompilerGenerated) flags |= 1U << 10U;
    if (member.isEvent) flags |= 1U << 11U;
    if (member.isPropertyAccessor) flags |= 1U << 12U;
    if (member.isIndexerAccessor) flags |= 1U << 13U;
    if (member.isGetter) flags |= 1U << 14U;
    if (member.isSetter) flags |= 1U << 15U;
    if (member.isUserOperator) flags |= 1U << 16U;
    if (member.isUserConversion) flags |= 1U << 17U;
    if (member.isImplicitUserConversion) flags |= 1U << 18U;
    return flags;
}

std::string signatureForMember(
    std::span<const frontend::ClassMemberSymbol> members,
    std::optional<std::size_t> ordinal)
{
    if (!ordinal.has_value() || *ordinal >= members.size()) return {};
    return members[*ordinal].signatureKey;
}

bool buildMetadataModel(
    const frontend::ProjectCompilation& compilation,
    LibraryMetadata& metadata,
    std::string& error)
{
    if (compilation.projectGraph() == nullptr || compilation.projectGraph()->rootProject() == nullptr ||
        compilation.nameLookup() == nullptr || compilation.genericModel() == nullptr ||
        compilation.memberModel() == nullptr || compilation.inheritanceModel() == nullptr ||
        compilation.interfaceModel() == nullptr || compilation.objectLayoutModel() == nullptr ||
        compilation.oopMirModel() == nullptr)
    {
        error = "Stage 48.2 metadata requires the complete project semantic/MIR model";
        return false;
    }
    const std::optional<std::size_t> rootOrdinal = rootProjectOrdinal(compilation);
    if (!rootOrdinal.has_value())
    {
        error = "cannot identify root project ordinal for metadata";
        return false;
    }

    metadata = {};
    metadata.formatMajor = kFormatMajor;
    metadata.formatMinor = kFormatMinor;
    metadata.targetId = kWindowsX64Target;
    metadata.pointerSize = kPointerSize;
    metadata.pointerAlignment = kPointerAlignment;
    metadata.producerVersion = std::string(kCompilerVersion);
    metadata.moduleName = compilation.projectGraph()->rootProject()->moduleName();
    metadata.targetTriple = std::string(kTargetTriple);

    const auto types = compilation.nameLookup()->namedTypes();
    const auto members = compilation.memberModel()->members();
    std::vector<std::size_t> rootTypeOrdinals;
    for (std::size_t ordinal = 0; ordinal < types.size(); ++ordinal)
    {
        if (types[ordinal].projectOrdinal == *rootOrdinal) rootTypeOrdinals.push_back(ordinal);
    }
    std::sort(rootTypeOrdinals.begin(), rootTypeOrdinals.end(), [&](std::size_t left, std::size_t right) {
        return types[left].identity.key() < types[right].identity.key();
    });

    for (const std::size_t typeOrdinal : rootTypeOrdinals)
    {
        const frontend::NamedTypeDeclaration& type = types[typeOrdinal];
        LibraryMetadataType record;
        record.kind = metadataKind(type.kind);
        record.accessibility = static_cast<std::uint8_t>(type.accessibility);
        record.flags = typeFlags(type);
        record.identityKey = type.identity.key();
        record.fullyQualifiedName = type.identity.fullyQualifiedName();
        const std::string canonicalTypeKey = typeKeyForOrdinal(types, typeOrdinal);
        record.stableTypeId = stableTypeId(canonicalTypeKey);

        if (const frontend::ClassInheritanceInfo* inheritance = compilation.inheritanceModel()->classInfo(typeOrdinal))
        {
            record.baseTypeKey = inheritance->baseTypeKey;
        }
        if (type.kind == frontend::NamedTypeDeclarationKind::Class)
        {
            for (const std::size_t interfaceOrdinal : compilation.interfaceModel()->interfacesOfClass(typeOrdinal))
            {
                record.interfaceTypeKeys.push_back(typeKeyForOrdinal(types, interfaceOrdinal));
            }
        }
        else if (type.kind == frontend::NamedTypeDeclarationKind::Interface)
        {
            if (const frontend::InterfaceTypeInfo* interfaceInfo = compilation.interfaceModel()->interfaceInfo(typeOrdinal))
            {
                for (const std::size_t interfaceOrdinal : interfaceInfo->directBaseInterfaceOrdinals)
                {
                    record.interfaceTypeKeys.push_back(typeKeyForOrdinal(types, interfaceOrdinal));
                }
            }
        }
        std::sort(record.interfaceTypeKeys.begin(), record.interfaceTypeKeys.end());

        const frontend::StructValueLayout* structLayout = compilation.objectLayoutModel()->structLayoutForType(typeOrdinal);
        const frontend::ClassObjectLayout* classLayout = compilation.objectLayoutModel()->layoutForType(typeOrdinal);
        if (structLayout != nullptr)
        {
            record.size = structLayout->size;
            record.alignment = structLayout->alignment;
            for (const frontend::ObjectFieldLayout& fieldLayout : structLayout->fields)
            {
                LibraryMetadataField field;
                if (fieldLayout.memberOrdinal < members.size())
                {
                    const frontend::ClassMemberSymbol& member = members[fieldLayout.memberOrdinal];
                    field.name = member.name;
                    field.flags = memberFlags(member);
                }
                field.typeKey = fieldLayout.typeKey;
                if (fieldLayout.memberOrdinal < members.size())
                {
                    field.ownership = metadataOwnership(members[fieldLayout.memberOrdinal].declaredOwnership);
                }
                field.offset = fieldLayout.offset;
                field.size = fieldLayout.size;
                field.alignment = fieldLayout.alignment;
                record.fields.push_back(std::move(field));
            }
        }
        else if (classLayout != nullptr)
        {
            record.size = classLayout->size;
            record.alignment = classLayout->alignment;
            record.headerSize = classLayout->headerSize;
            record.referenceSize = classLayout->referenceSize;
            record.referenceAlignment = classLayout->referenceAlignment;
            for (const frontend::ObjectFieldLayout& fieldLayout : classLayout->fields)
            {
                LibraryMetadataField field;
                if (fieldLayout.memberOrdinal < members.size())
                {
                    const frontend::ClassMemberSymbol& member = members[fieldLayout.memberOrdinal];
                    field.name = member.name;
                    field.flags = memberFlags(member);
                }
                field.typeKey = fieldLayout.typeKey;
                if (fieldLayout.memberOrdinal < members.size())
                {
                    field.ownership = metadataOwnership(members[fieldLayout.memberOrdinal].declaredOwnership);
                }
                field.offset = fieldLayout.offset;
                field.size = fieldLayout.size;
                field.alignment = fieldLayout.alignment;
                record.fields.push_back(std::move(field));
            }
        }
        else if (type.kind == frontend::NamedTypeDeclarationKind::Enum)
        {
            // EL TAMANO DEL ENUM SALE DEL MODELO DE LAYOUT, no de un 4 escrito aqui.
            //
            // Este era el SEPTIMO sitio con "un enum mide cuatro bytes" escrito a mano, y el mas
            // caro de los que quedaban: los metadatos alimentan a los generadores de host --el
            // enlace P/Invoke de C# y el de C++/CLI-- asi que un 4/4 fijo se propaga a codigo que
            // otro compilador genera y que ya nadie relaciona con este fichero. Un `enum : byte`
            // exportado hacia que el host reservara cuatro bytes donde el modulo pone uno.
            //
            // `valueLayoutForTypeKey` es la misma consulta que usan el modelo MIR y el HIR, asi que
            // los metadatos no pueden discrepar del layout que el propio modulo materializa.
            const std::optional<std::pair<std::uint64_t, std::uint64_t>> enumLayout =
                compilation.objectLayoutModel()->valueLayoutForTypeKey(
                    *compilation.nameLookup(), "type:" + type.identity.key());
            record.size = enumLayout.has_value() ? enumLayout->first : 4;
            record.alignment = enumLayout.has_value() ? enumLayout->second : 4;
            // EL SIGNO, que el tamano no lleva: `enum : byte` y `enum : sbyte` miden lo mismo.
            record.enumUnderlyingTypeKey =
                std::string(frontend::enumUnderlyingTypeKey(type.syntax));
        }
        else if (type.kind == frontend::NamedTypeDeclarationKind::Interface)
        {
            const target::TypeLayout referenceLayout = compilation.objectLayoutModel()->classReferenceLayout();
            record.referenceSize = referenceLayout.size;
            record.referenceAlignment = referenceLayout.alignment;
        }
        metadata.types.push_back(std::move(record));
    }

    std::unordered_set<std::size_t> rootTypes(rootTypeOrdinals.begin(), rootTypeOrdinals.end());
    std::unordered_map<std::size_t, const frontend::ProjectOopMirStaticField*> staticFields;
    for (const frontend::ProjectOopMirStaticField& field : compilation.oopMirModel()->staticFields())
    {
        staticFields.emplace(field.memberOrdinal, &field);
    }
    std::vector<std::size_t> memberOrdinals;
    for (std::size_t ordinal = 0; ordinal < members.size(); ++ordinal)
    {
        if (rootTypes.contains(members[ordinal].declaringTypeOrdinal)) memberOrdinals.push_back(ordinal);
    }
    std::sort(memberOrdinals.begin(), memberOrdinals.end(), [&](std::size_t left, std::size_t right) {
        const frontend::ClassMemberSymbol& a = members[left];
        const frontend::ClassMemberSymbol& b = members[right];
        const std::string aType = typeKeyForOrdinal(types, a.declaringTypeOrdinal);
        const std::string bType = typeKeyForOrdinal(types, b.declaringTypeOrdinal);
        if (aType != bType) return aType < bType;
        if (a.signatureKey != b.signatureKey) return a.signatureKey < b.signatureKey;
        return left < right;
    });

    for (const std::size_t ordinal : memberOrdinals)
    {
        const frontend::ClassMemberSymbol& member = members[ordinal];
        LibraryMetadataMember record;
        record.kind = static_cast<std::uint8_t>(member.kind);
        record.accessibility = static_cast<std::uint8_t>(member.accessibility);
        record.flags = memberFlags(member);
        record.declaringTypeKey = typeKeyForOrdinal(types, member.declaringTypeOrdinal);
        record.name = member.name;
        record.signatureKey = member.signatureKey;
        record.declaredTypeKey = member.declaredTypeKey;
        record.declaredOwnership = metadataOwnership(member.declaredOwnership);
        record.cAbiName = member.cAbiName;
        if (const frontend::ProjectOopMirFunction* function = compilation.oopMirModel()->functionForMember(ordinal))
        {
            record.mangledName = function->mangledName;
        }
        for (const frontend::MethodParameterSymbol& parameter : member.parameters)
        {
            LibraryMetadataParameter parameterRecord;
            parameterRecord.name = parameter.name;
            parameterRecord.typeKey = parameter.typeKey;
            parameterRecord.ownership = metadataOwnership(parameter.ownership);
            record.parameters.push_back(std::move(parameterRecord));
        }
        const auto staticField = staticFields.find(ordinal);
        if (staticField != staticFields.end() && staticField->second->isConst && staticField->second->hasConstantInitializer)
        {
            const frontend::ProjectOopMirStaticField& field = *staticField->second;
            if (field.typeKey == "builtin:bool")
            {
                record.constantKind = 3;
                record.constantBits = field.constantBooleanValue ? 1U : 0U;
            }
            else if (field.typeKey == "builtin:float")
            {
                record.constantKind = 4;
                record.constantBits = std::bit_cast<std::uint32_t>(static_cast<float>(field.constantFloatingValue));
            }
            else if (field.typeKey == "builtin:double")
            {
                record.constantKind = 5;
                record.constantBits = std::bit_cast<std::uint64_t>(field.constantFloatingValue);
            }
            else
            {
                record.constantKind = 1;
                record.constantBits = field.constantIntegerValue;
            }
        }
        metadata.members.push_back(std::move(record));
    }

    std::unordered_set<std::string> rootTemplateKeys;
    for (const frontend::GenericTemplateRecord& generic : compilation.genericModel()->templates())
    {
        if (generic.projectOrdinal != *rootOrdinal) continue;
        LibraryMetadataGenericTemplate record;
        record.kind = static_cast<std::uint8_t>(generic.kind);
        record.canonicalKey = generic.canonicalKey;
        record.moduleName = generic.moduleName;
        record.namespaceName = generic.namespaceName;
        record.ownerName = generic.ownerName;
        record.name = generic.name;
        record.parameters = generic.parameters;
        rootTemplateKeys.insert(record.canonicalKey);
        metadata.genericTemplates.push_back(std::move(record));
    }
    std::sort(metadata.genericTemplates.begin(), metadata.genericTemplates.end(), [](const auto& left, const auto& right) {
        return left.canonicalKey < right.canonicalKey;
    });
    for (const frontend::GenericSpecializationRecord& specialization : compilation.genericModel()->specializations())
    {
        if (!rootTemplateKeys.contains(specialization.templateKey)) continue;
        LibraryMetadataGenericSpecialization record;
        record.kind = static_cast<std::uint8_t>(specialization.kind);
        record.templateKey = specialization.templateKey;
        record.specializationKey = specialization.specializationKey;
        record.specializedName = specialization.specializedName;
        record.typeArguments = specialization.typeArguments;
        metadata.genericSpecializations.push_back(std::move(record));
    }
    std::sort(metadata.genericSpecializations.begin(), metadata.genericSpecializations.end(), [](const auto& left, const auto& right) {
        return left.specializationKey < right.specializationKey;
    });

    for (const frontend::ProjectOopMirDispatchTable& table : compilation.oopMirModel()->dispatchTables())
    {
        if (!rootTypes.contains(table.typeOrdinal)) continue;
        LibraryMetadataDispatch record;
        record.typeKey = table.typeKey;
        record.stableTypeId = table.stableTypeId;
        record.descriptorName = table.descriptorName;
        record.vtableName = table.vtableName;
        record.destructionEntryName = table.destructionEntryName;
        for (std::size_t index = 0; index < table.slotMemberOrdinals.size(); ++index)
        {
            record.slotSignatures.push_back(signatureForMember(members, table.slotMemberOrdinals[index]));
            record.slotCalleeNames.push_back(index < table.slotCalleeNames.size() ? table.slotCalleeNames[index] : std::string{});
        }
        for (const frontend::ProjectOopMirDispatchTable::InterfaceTable& tableInterface : table.interfaceTables)
        {
            LibraryMetadataDispatchInterface interfaceRecord;
            interfaceRecord.interfaceTypeKey = tableInterface.interfaceTypeKey;
            interfaceRecord.stableInterfaceTypeId = tableInterface.stableInterfaceTypeId;
            interfaceRecord.methodTableName = tableInterface.methodTableName;
            for (const std::size_t memberOrdinal : tableInterface.interfaceMemberOrdinals)
            {
                interfaceRecord.interfaceSignatures.push_back(signatureForMember(members, memberOrdinal));
            }
            for (const std::size_t memberOrdinal : tableInterface.implementationMemberOrdinals)
            {
                interfaceRecord.implementationSignatures.push_back(signatureForMember(members, memberOrdinal));
            }
            interfaceRecord.calleeNames = tableInterface.calleeNames;
            record.interfaces.push_back(std::move(interfaceRecord));
        }
        std::sort(record.interfaces.begin(), record.interfaces.end(), [](const auto& left, const auto& right) {
            return left.interfaceTypeKey < right.interfaceTypeKey;
        });
        metadata.dispatchTables.push_back(std::move(record));
    }
    std::sort(metadata.dispatchTables.begin(), metadata.dispatchTables.end(), [](const auto& left, const auto& right) {
        return left.typeKey < right.typeKey;
    });

    for (const frontend::ProjectOopMirFunction& function : compilation.oopMirModel()->functions())
    {
        if (function.isLambda || !function.isExportedC || !rootTypes.contains(function.declaringTypeOrdinal) ||
            function.memberOrdinal >= members.size())
        {
            continue;
        }
        const frontend::ClassMemberSymbol& member = members[function.memberOrdinal];
        LibraryMetadataExport record;
        record.cAbiName = member.cAbiName;
        record.declaringTypeKey = typeKeyForOrdinal(types, function.declaringTypeOrdinal);
        record.returnTypeKey = function.returnTypeKey;
        record.parameterTypeKeys = function.parameterTypeKeys;
        record.nativeMangledName = function.mangledName;
        metadata.exports.push_back(std::move(record));
    }
    std::sort(metadata.exports.begin(), metadata.exports.end(), [](const auto& left, const auto& right) {
        return left.cAbiName < right.cAbiName;
    });
    return true;
}

void writeStringVector(ByteWriter& writer, const std::vector<std::string>& values)
{
    writer.u32(static_cast<std::uint32_t>(values.size()));
    for (const std::string& value : values) writer.text(value);
}

bool readStringVector(ByteReader& reader, std::vector<std::string>& values)
{
    std::uint32_t count = 0;
    if (!reader.u32(count) || count > kMaximumRecordCount) return false;
    values.clear();
    values.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        std::string value;
        if (!reader.text(value)) return false;
        values.push_back(std::move(value));
    }
    return true;
}

std::vector<std::uint8_t> encodePayload(const LibraryMetadata& metadata)
{
    ByteWriter writer;
    writer.text(metadata.producerVersion);
    writer.text(metadata.moduleName);
    writer.text(metadata.targetTriple);

    writer.u32(static_cast<std::uint32_t>(metadata.types.size()));
    for (const LibraryMetadataType& type : metadata.types)
    {
        writer.u8(static_cast<std::uint8_t>(type.kind));
        writer.u8(type.accessibility);
        writer.u16(type.flags);
        writer.text(type.identityKey);
        writer.text(type.fullyQualifiedName);
        writer.text(type.baseTypeKey);
        writeStringVector(writer, type.interfaceTypeKeys);
        writer.u64(type.size);
        writer.u64(type.alignment);
        writer.u64(type.headerSize);
        writer.u64(type.referenceSize);
        writer.u64(type.referenceAlignment);
        writer.u64(type.stableTypeId);
        // Minor 2. Un lector de minor 1 no lo espera, asi que se escribe solo cuando el formato
        // que se esta produciendo lo incluye -- la misma condicion que ya usa `ownership`.
        if (metadata.formatMinor >= 2) writer.text(type.enumUnderlyingTypeKey);
        writer.u32(static_cast<std::uint32_t>(type.fields.size()));
        for (const LibraryMetadataField& field : type.fields)
        {
            writer.text(field.name);
            writer.text(field.typeKey);
            if (metadata.formatMinor >= 1)
            {
                writer.u8(static_cast<std::uint8_t>(field.ownership));
            }
            writer.u64(field.offset);
            writer.u64(field.size);
            writer.u64(field.alignment);
            writer.u32(field.flags);
        }
    }

    writer.u32(static_cast<std::uint32_t>(metadata.members.size()));
    for (const LibraryMetadataMember& member : metadata.members)
    {
        writer.u8(member.kind);
        writer.u8(member.accessibility);
        writer.u32(member.flags);
        writer.text(member.declaringTypeKey);
        writer.text(member.name);
        writer.text(member.signatureKey);
        writer.text(member.declaredTypeKey);
        if (metadata.formatMinor >= 1)
        {
            writer.u8(static_cast<std::uint8_t>(member.declaredOwnership));
        }
        writer.text(member.cAbiName);
        writer.text(member.mangledName);
        writer.u32(static_cast<std::uint32_t>(member.parameters.size()));
        for (const LibraryMetadataParameter& parameter : member.parameters)
        {
            writer.text(parameter.name);
            writer.text(parameter.typeKey);
            if (metadata.formatMinor >= 1)
            {
                writer.u8(static_cast<std::uint8_t>(parameter.ownership));
            }
        }
        writer.u8(member.constantKind);
        writer.u64(member.constantBits);
    }

    writer.u32(static_cast<std::uint32_t>(metadata.genericTemplates.size()));
    for (const LibraryMetadataGenericTemplate& generic : metadata.genericTemplates)
    {
        writer.u8(generic.kind);
        writer.text(generic.canonicalKey);
        writer.text(generic.moduleName);
        writer.text(generic.namespaceName);
        writer.text(generic.ownerName);
        writer.text(generic.name);
        writeStringVector(writer, generic.parameters);
    }

    writer.u32(static_cast<std::uint32_t>(metadata.genericSpecializations.size()));
    for (const LibraryMetadataGenericSpecialization& specialization : metadata.genericSpecializations)
    {
        writer.u8(specialization.kind);
        writer.text(specialization.templateKey);
        writer.text(specialization.specializationKey);
        writer.text(specialization.specializedName);
        writeStringVector(writer, specialization.typeArguments);
    }

    writer.u32(static_cast<std::uint32_t>(metadata.dispatchTables.size()));
    for (const LibraryMetadataDispatch& dispatch : metadata.dispatchTables)
    {
        writer.text(dispatch.typeKey);
        writer.u64(dispatch.stableTypeId);
        writer.text(dispatch.descriptorName);
        writer.text(dispatch.vtableName);
        writer.text(dispatch.destructionEntryName);
        writeStringVector(writer, dispatch.slotSignatures);
        writeStringVector(writer, dispatch.slotCalleeNames);
        writer.u32(static_cast<std::uint32_t>(dispatch.interfaces.size()));
        for (const LibraryMetadataDispatchInterface& interfaceRecord : dispatch.interfaces)
        {
            writer.text(interfaceRecord.interfaceTypeKey);
            writer.u64(interfaceRecord.stableInterfaceTypeId);
            writer.text(interfaceRecord.methodTableName);
            writeStringVector(writer, interfaceRecord.interfaceSignatures);
            writeStringVector(writer, interfaceRecord.implementationSignatures);
            writeStringVector(writer, interfaceRecord.calleeNames);
        }
    }

    writer.u32(static_cast<std::uint32_t>(metadata.exports.size()));
    for (const LibraryMetadataExport& exportRecord : metadata.exports)
    {
        writer.text(exportRecord.cAbiName);
        writer.text(exportRecord.declaringTypeKey);
        writer.text(exportRecord.returnTypeKey);
        writeStringVector(writer, exportRecord.parameterTypeKeys);
        writer.text(exportRecord.nativeMangledName);
    }
    return writer.bytes();
}

std::vector<std::uint8_t> encodeAbiProjection(const LibraryMetadata& metadata)
{
    LibraryMetadata projected = metadata;
    projected.producerVersion.clear();
    return encodePayload(projected);
}

bool readCount(ByteReader& reader, std::uint32_t& count)
{
    return reader.u32(count) && count <= kMaximumRecordCount;
}

bool decodePayload(
    std::span<const std::uint8_t> payload,
    LibraryMetadata& metadata,
    std::string& error)
{
    ByteReader reader(payload);
    if (!reader.text(metadata.producerVersion) || !reader.text(metadata.moduleName) || !reader.text(metadata.targetTriple))
    {
        error = "truncated metadata identity payload";
        return false;
    }

    std::uint32_t count = 0;
    if (!readCount(reader, count)) { error = "invalid metadata type count"; return false; }
    metadata.types.clear();
    metadata.types.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        LibraryMetadataType type;
        std::uint8_t kind = 0;
        if (!reader.u8(kind) || kind > static_cast<std::uint8_t>(LibraryMetadataTypeKind::Delegate) ||
            !reader.u8(type.accessibility) || !reader.u16(type.flags) ||
            !reader.text(type.identityKey) || !reader.text(type.fullyQualifiedName) || !reader.text(type.baseTypeKey) ||
            !readStringVector(reader, type.interfaceTypeKeys) ||
            !reader.u64(type.size) || !reader.u64(type.alignment) || !reader.u64(type.headerSize) ||
            !reader.u64(type.referenceSize) || !reader.u64(type.referenceAlignment) || !reader.u64(type.stableTypeId))
        {
            error = "truncated or invalid metadata type record";
            return false;
        }
        type.kind = static_cast<LibraryMetadataTypeKind>(kind);
        if (metadata.formatMinor >= 2 && !reader.text(type.enumUnderlyingTypeKey))
        {
            error = "truncated or invalid metadata enum underlying type";
            return false;
        }
        std::uint32_t fieldCount = 0;
        if (!readCount(reader, fieldCount)) { error = "invalid metadata field count"; return false; }
        type.fields.reserve(fieldCount);
        for (std::uint32_t fieldIndex = 0; fieldIndex < fieldCount; ++fieldIndex)
        {
            LibraryMetadataField field;
            if (!reader.text(field.name) || !reader.text(field.typeKey) ||
                !readOwnership(reader, metadata.formatMinor, field.ownership) ||
                !reader.u64(field.offset) || !reader.u64(field.size) ||
                !reader.u64(field.alignment) || !reader.u32(field.flags))
            {
                error = "truncated or invalid metadata field record";
                return false;
            }
            type.fields.push_back(std::move(field));
        }
        metadata.types.push_back(std::move(type));
    }

    if (!readCount(reader, count)) { error = "invalid metadata member count"; return false; }
    metadata.members.clear();
    metadata.members.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        LibraryMetadataMember member;
        if (!reader.u8(member.kind) || !reader.u8(member.accessibility) || !reader.u32(member.flags) ||
            !reader.text(member.declaringTypeKey) || !reader.text(member.name) || !reader.text(member.signatureKey) ||
            !reader.text(member.declaredTypeKey) ||
            !readOwnership(reader, metadata.formatMinor, member.declaredOwnership) ||
            !reader.text(member.cAbiName) || !reader.text(member.mangledName))
        {
            error = "truncated or invalid metadata member record";
            return false;
        }
        std::uint32_t parameterCount = 0;
        if (!readCount(reader, parameterCount)) { error = "invalid metadata parameter count"; return false; }
        member.parameters.reserve(parameterCount);
        for (std::uint32_t parameterIndex = 0; parameterIndex < parameterCount; ++parameterIndex)
        {
            LibraryMetadataParameter parameter;
            if (!reader.text(parameter.name) || !reader.text(parameter.typeKey) ||
                !readOwnership(reader, metadata.formatMinor, parameter.ownership))
            {
                error = "truncated or invalid metadata parameter record";
                return false;
            }
            member.parameters.push_back(std::move(parameter));
        }
        if (!reader.u8(member.constantKind) || !reader.u64(member.constantBits))
        {
            error = "truncated metadata constant record";
            return false;
        }
        metadata.members.push_back(std::move(member));
    }

    if (!readCount(reader, count)) { error = "invalid generic-template count"; return false; }
    metadata.genericTemplates.clear();
    metadata.genericTemplates.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        LibraryMetadataGenericTemplate generic;
        if (!reader.u8(generic.kind) || !reader.text(generic.canonicalKey) || !reader.text(generic.moduleName) ||
            !reader.text(generic.namespaceName) || !reader.text(generic.ownerName) || !reader.text(generic.name) ||
            !readStringVector(reader, generic.parameters))
        {
            error = "truncated generic-template metadata";
            return false;
        }
        metadata.genericTemplates.push_back(std::move(generic));
    }

    if (!readCount(reader, count)) { error = "invalid generic-specialization count"; return false; }
    metadata.genericSpecializations.clear();
    metadata.genericSpecializations.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        LibraryMetadataGenericSpecialization specialization;
        if (!reader.u8(specialization.kind) || !reader.text(specialization.templateKey) ||
            !reader.text(specialization.specializationKey) || !reader.text(specialization.specializedName) ||
            !readStringVector(reader, specialization.typeArguments))
        {
            error = "truncated generic-specialization metadata";
            return false;
        }
        metadata.genericSpecializations.push_back(std::move(specialization));
    }

    if (!readCount(reader, count)) { error = "invalid dispatch-table count"; return false; }
    metadata.dispatchTables.clear();
    metadata.dispatchTables.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        LibraryMetadataDispatch dispatch;
        if (!reader.text(dispatch.typeKey) || !reader.u64(dispatch.stableTypeId) ||
            !reader.text(dispatch.descriptorName) || !reader.text(dispatch.vtableName) ||
            !reader.text(dispatch.destructionEntryName) || !readStringVector(reader, dispatch.slotSignatures) ||
            !readStringVector(reader, dispatch.slotCalleeNames))
        {
            error = "truncated dispatch metadata";
            return false;
        }
        std::uint32_t interfaceCount = 0;
        if (!readCount(reader, interfaceCount)) { error = "invalid dispatch-interface count"; return false; }
        dispatch.interfaces.reserve(interfaceCount);
        for (std::uint32_t interfaceIndex = 0; interfaceIndex < interfaceCount; ++interfaceIndex)
        {
            LibraryMetadataDispatchInterface interfaceRecord;
            if (!reader.text(interfaceRecord.interfaceTypeKey) || !reader.u64(interfaceRecord.stableInterfaceTypeId) ||
                !reader.text(interfaceRecord.methodTableName) || !readStringVector(reader, interfaceRecord.interfaceSignatures) ||
                !readStringVector(reader, interfaceRecord.implementationSignatures) ||
                !readStringVector(reader, interfaceRecord.calleeNames))
            {
                error = "truncated dispatch-interface metadata";
                return false;
            }
            dispatch.interfaces.push_back(std::move(interfaceRecord));
        }
        metadata.dispatchTables.push_back(std::move(dispatch));
    }

    if (!readCount(reader, count)) { error = "invalid C-export count"; return false; }
    metadata.exports.clear();
    metadata.exports.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        LibraryMetadataExport exportRecord;
        if (!reader.text(exportRecord.cAbiName) || !reader.text(exportRecord.declaringTypeKey) ||
            !reader.text(exportRecord.returnTypeKey) || !readStringVector(reader, exportRecord.parameterTypeKeys) ||
            !reader.text(exportRecord.nativeMangledName))
        {
            error = "truncated C-export metadata";
            return false;
        }
        metadata.exports.push_back(std::move(exportRecord));
    }

    if (!reader.atEnd())
    {
        error = "metadata payload has trailing bytes";
        return false;
    }
    return true;
}

bool sameFingerprint(
    const std::array<std::uint8_t, 32>& left,
    const std::array<std::uint8_t, 32>& right) noexcept
{
    std::uint8_t difference = 0;
    for (std::size_t index = 0; index < left.size(); ++index)
    {
        difference = static_cast<std::uint8_t>(difference | (left[index] ^ right[index]));
    }
    return difference == 0;
}

bool writeBytesAtomically(
    const std::filesystem::path& destination,
    std::span<const std::uint8_t> bytes,
    std::string& error)
{
    std::error_code filesystemError;
    if (!destination.parent_path().empty())
    {
        std::filesystem::create_directories(destination.parent_path(), filesystemError);
        if (filesystemError)
        {
            error = "cannot create metadata output directory: " + filesystemError.message();
            return false;
        }
    }
    std::filesystem::path temporary = destination;
    temporary += ".tmp";
    std::filesystem::remove(temporary, filesystemError);
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            error = "cannot open temporary metadata file '" + temporary.string() + "'";
            return false;
        }
        stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!stream)
        {
            stream.close();
            std::filesystem::remove(temporary, filesystemError);
            error = "failed while writing NativeSharp metadata";
            return false;
        }
    }
    std::filesystem::remove(destination, filesystemError);
    filesystemError.clear();
    std::filesystem::rename(temporary, destination, filesystemError);
    if (filesystemError)
    {
        std::filesystem::remove(temporary, filesystemError);
        error = "cannot commit NativeSharp metadata: " + filesystemError.message();
        return false;
    }
    return true;
}
}

bool LibraryMetadataCodec::write(
    const frontend::ProjectCompilation& compilation,
    const std::filesystem::path& destination,
    std::string& error) const
{
    error.clear();
    LibraryMetadata metadata;
    if (!buildMetadataModel(compilation, metadata, error)) return false;
    return writeModel(metadata, destination, error);
}

// Escribe un MODELO ya construido, en la version menor que el propio modelo dice.
//
// POR QUE EXISTE, y no es andamiaje de pruebas. La compatibilidad hacia atras de este formato solo
// se puede MEDIR si se puede producir un fichero de una version anterior: sin esto, "un lector de
// 1.2 lee un fichero de 1.1" es una afirmacion sobre codigo que nadie ejecuto nunca -- y este
// proyecto no cuenta esas. Cada minor futuro hereda la prueba sin escribir nada nuevo.
bool LibraryMetadataCodec::writeModel(
    const LibraryMetadata& model,
    const std::filesystem::path& destination,
    std::string& error) const
{
    error.clear();
    LibraryMetadata metadata = model;
    if (metadata.formatMajor != kFormatMajor || metadata.formatMinor > kFormatMinor)
    {
        error = "cannot write NativeSharp metadata format " +
            std::to_string(metadata.formatMajor) + "." + std::to_string(metadata.formatMinor);
        return false;
    }
    // EL OBJETIVO SE VALIDA, NO SE SUSTITUYE. Escribir la constante por encima de lo que el modelo
    // diga es exactamente lo que hacia el encabezado con el minor: dos fuentes de verdad para el
    // mismo campo. Y aqui seria peor, porque esta funcion existe para MEDIR round-trips -- un
    // campo que se sobrescribe en silencio hace que la prueba diga "el objetivo sobrevivio" cuando
    // lo que sobrevivio es la constante.
    //
    // NativeSharp 0.1 solo tiene un objetivo, asi que lo unico correcto es rechazar cualquier otro
    // con su numero delante, y no producir un fichero que el lector rechazaria despues.
    if (metadata.targetId != kWindowsX64Target || metadata.pointerSize != kPointerSize ||
        metadata.pointerAlignment != kPointerAlignment)
    {
        error = "cannot write NativeSharp metadata for target " + std::to_string(metadata.targetId) +
            " with pointer " + std::to_string(metadata.pointerSize) + "/" +
            std::to_string(metadata.pointerAlignment) +
            "; NativeSharp 0.1 targets Windows x64 only";
        return false;
    }

    metadata.abiFingerprint = sha256(encodeAbiProjection(metadata));
    const std::vector<std::uint8_t> payload = encodePayload(metadata);
    if (payload.size() > kMaximumPayloadSize)
    {
        error = "NativeSharp metadata payload exceeds 128 MiB format limit";
        return false;
    }
    const std::array<std::uint8_t, 32> payloadHash = sha256(payload);

    ByteWriter file;
    file.raw(kMagic);
    // El encabezado escribe el minor DEL MODELO, no la constante. El cuerpo ya se codifica
    // segun `metadata.formatMinor` (`if (metadata.formatMinor >= N)`), asi que tomar el numero de
    // otro sitio serian dos fuentes de verdad para el mismo campo dentro del mismo fichero -- y un
    // encabezado que dijera 2 sobre un cuerpo escrito como 1 es un fichero que no se puede leer.
    file.u16(metadata.formatMajor);
    file.u16(metadata.formatMinor);
    // El marcador de endianidad es una propiedad de la CODIFICACION, no del modelo: no hay campo
    // que pueda decir otra cosa, y por eso este si es una constante. Los tres siguientes salen del
    // modelo, ya validado arriba.
    file.u32(kEndianMarker);
    file.u32(metadata.targetId);
    file.u32(metadata.pointerSize);
    file.u32(metadata.pointerAlignment);
    file.u64(static_cast<std::uint64_t>(payload.size()));
    file.raw(metadata.abiFingerprint);
    file.raw(payloadHash);
    file.raw(payload);
    return writeBytesAtomically(destination, file.bytes(), error);
}

bool LibraryMetadataCodec::read(
    const std::filesystem::path& source,
    LibraryMetadata& metadata,
    std::string& error) const
{
    error.clear();
    std::ifstream stream(source, std::ios::binary | std::ios::ate);
    if (!stream)
    {
        error = "cannot open NativeSharp metadata '" + source.string() + "'";
        return false;
    }
    const std::streampos end = stream.tellg();
    if (end < 0 || static_cast<std::uint64_t>(end) > kHeaderSize + kMaximumPayloadSize)
    {
        error = "NativeSharp metadata file has invalid size";
        return false;
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
    stream.seekg(0, std::ios::beg);
    stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!stream && !bytes.empty())
    {
        error = "failed while reading NativeSharp metadata";
        return false;
    }
    if (bytes.size() < kHeaderSize)
    {
        error = "NativeSharp metadata is truncated before its fixed header";
        return false;
    }

    ByteReader reader(bytes);
    std::array<std::uint8_t, 8> magic{};
    if (!reader.raw(magic) || magic != kMagic)
    {
        error = "invalid NativeSharp metadata magic";
        return false;
    }
    std::uint16_t major = 0;
    std::uint16_t minor = 0;
    std::uint32_t endianMarker = 0;
    std::uint32_t targetId = 0;
    std::uint32_t pointerSize = 0;
    std::uint32_t pointerAlignment = 0;
    std::uint64_t payloadSize = 0;
    std::array<std::uint8_t, 32> abiFingerprint{};
    std::array<std::uint8_t, 32> payloadHash{};
    if (!reader.u16(major) || !reader.u16(minor) || !reader.u32(endianMarker) ||
        !reader.u32(targetId) || !reader.u32(pointerSize) || !reader.u32(pointerAlignment) ||
        !reader.u64(payloadSize) || !reader.raw(abiFingerprint) || !reader.raw(payloadHash))
    {
        error = "truncated NativeSharp metadata header";
        return false;
    }
    if (major != kFormatMajor || minor > kFormatMinor)
    {
        error = "unsupported NativeSharp metadata format " + std::to_string(major) + "." + std::to_string(minor);
        return false;
    }
    if (endianMarker != kEndianMarker)
    {
        error = "NativeSharp metadata endian marker mismatch";
        return false;
    }
    if (payloadSize > kMaximumPayloadSize || payloadSize != reader.remaining())
    {
        error = "NativeSharp metadata payload length mismatch";
        return false;
    }
    const std::span<const std::uint8_t> payload(bytes.data() + kHeaderSize, static_cast<std::size_t>(payloadSize));
    if (!sameFingerprint(sha256(payload), payloadHash))
    {
        error = "NativeSharp metadata payload checksum mismatch";
        return false;
    }

    LibraryMetadata decoded;
    decoded.formatMajor = major;
    decoded.formatMinor = minor;
    decoded.targetId = targetId;
    decoded.pointerSize = pointerSize;
    decoded.pointerAlignment = pointerAlignment;
    decoded.abiFingerprint = abiFingerprint;
    if (!decodePayload(payload, decoded, error)) return false;
    const std::array<std::uint8_t, 32> recomputedAbi = sha256(encodeAbiProjection(decoded));
    if (!sameFingerprint(recomputedAbi, decoded.abiFingerprint))
    {
        error = "NativeSharp metadata ABI fingerprint mismatch";
        return false;
    }
    metadata = std::move(decoded);
    return true;
}

bool LibraryMetadataCodec::verifyCurrentCompiler(
    const LibraryMetadata& metadata,
    std::string& error) const
{
    error.clear();
    if (metadata.formatMajor != kFormatMajor || metadata.formatMinor > kFormatMinor)
    {
        error = "metadata format is not supported by this compiler";
        return false;
    }
    if (metadata.targetId != kWindowsX64Target || metadata.pointerSize != kPointerSize ||
        metadata.pointerAlignment != kPointerAlignment || metadata.targetTriple != kTargetTriple)
    {
        error = "metadata target ABI does not match NativeSharp 0.1 Windows x64";
        return false;
    }
    if (metadata.producerVersion != kCompilerVersion)
    {
        error = "metadata producer version '" + metadata.producerVersion +
            "' does not match compiler version '" + std::string(kCompilerVersion) + "'";
        return false;
    }
    return true;
}

std::string LibraryMetadataCodec::fingerprintHex(const std::array<std::uint8_t, 32>& fingerprint)
{
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (const std::uint8_t byte : fingerprint)
    {
        stream << std::setw(2) << static_cast<unsigned>(byte);
    }
    return stream.str();
}
}
