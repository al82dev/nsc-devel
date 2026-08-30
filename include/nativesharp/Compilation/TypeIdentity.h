#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::compilation
{
struct TypeNameSegment final
{
    std::string name;
    std::uint32_t arity = 0;

    bool operator==(const TypeNameSegment&) const noexcept = default;
};

class TypeIdentity final
{
public:
    TypeIdentity(
        std::string moduleName,
        std::vector<std::string> namespaceSegments,
        std::vector<TypeNameSegment> containingTypes,
        std::string name,
        std::uint32_t arity = 0);

    const std::string& moduleName() const noexcept;
    const std::vector<std::string>& namespaceSegments() const noexcept;
    const std::vector<TypeNameSegment>& containingTypes() const noexcept;
    const std::string& name() const noexcept;
    std::uint32_t arity() const noexcept;

    bool isValid() const noexcept;
    const std::string& validationError() const noexcept;
    std::string fullyQualifiedName() const;
    std::string key() const;

    bool operator==(const TypeIdentity& other) const noexcept;

private:
    static bool isIdentifier(std::string_view value) noexcept;
    static void appendTypeSegment(std::string& output, const TypeNameSegment& segment);
    void validate();

    std::string moduleName_;
    std::vector<std::string> namespaceSegments_;
    std::vector<TypeNameSegment> containingTypes_;
    std::string name_;
    std::uint32_t arity_;
    std::string validationError_;
};
}
