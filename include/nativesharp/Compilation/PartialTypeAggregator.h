#pragma once

#include "nativesharp/Compilation/TypeIdentity.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace nativesharp::compilation
{
enum class TypeDeclarationKind
{
    Class,
    Struct,
    Enum,
    Interface,
    Delegate
};

enum class DeclaredAccessibility
{
    Unspecified,
    Internal,
    Public
};

struct TypePartDescriptor final
{
    TypeIdentity identity;
    TypeDeclarationKind kind = TypeDeclarationKind::Class;
    DeclaredAccessibility accessibility = DeclaredAccessibility::Unspecified;
    bool isPartial = false;
    bool isAbstract = false;
    bool isSealed = false;
    bool isStatic = false;
    std::optional<std::string> baseTypeKey;
    std::size_t sourceOrdinal = 0;
    std::uint32_t sourceOffset = 0;
    std::string sourcePath;
};

struct MergedTypeDescriptor final
{
    TypeIdentity identity;
    TypeDeclarationKind kind = TypeDeclarationKind::Class;
    DeclaredAccessibility accessibility = DeclaredAccessibility::Internal;
    bool isAbstract = false;
    bool isSealed = false;
    bool isStatic = false;
    std::optional<std::string> baseTypeKey;
    std::vector<TypePartDescriptor> parts;
};

class PartialTypeAggregator final
{
public:
    bool addPart(TypePartDescriptor part, std::string& error);
    bool finalize(std::string& error);

    std::span<const MergedTypeDescriptor> types() const noexcept;
    bool isFinalized() const noexcept;

private:
    static bool mergeGroup(MergedTypeDescriptor& merged, std::string& error);

    std::vector<TypePartDescriptor> parts_;
    std::vector<MergedTypeDescriptor> types_;
    bool finalized_ = false;
};
}
