#include "nativesharp/Compilation/TypeIdentity.h"

#include <cctype>
#include <utility>

namespace nativesharp::compilation
{
TypeIdentity::TypeIdentity(
    std::string moduleName,
    std::vector<std::string> namespaceSegments,
    std::vector<TypeNameSegment> containingTypes,
    std::string name,
    std::uint32_t arity)
    : moduleName_(std::move(moduleName)),
      namespaceSegments_(std::move(namespaceSegments)),
      containingTypes_(std::move(containingTypes)),
      name_(std::move(name)),
      arity_(arity)
{
    validate();
}

const std::string& TypeIdentity::moduleName() const noexcept
{
    return moduleName_;
}

const std::vector<std::string>& TypeIdentity::namespaceSegments() const noexcept
{
    return namespaceSegments_;
}

const std::vector<TypeNameSegment>& TypeIdentity::containingTypes() const noexcept
{
    return containingTypes_;
}

const std::string& TypeIdentity::name() const noexcept
{
    return name_;
}

std::uint32_t TypeIdentity::arity() const noexcept
{
    return arity_;
}

bool TypeIdentity::isValid() const noexcept
{
    return validationError_.empty();
}

const std::string& TypeIdentity::validationError() const noexcept
{
    return validationError_;
}

std::string TypeIdentity::fullyQualifiedName() const
{
    std::string result;
    for (const std::string& segment : namespaceSegments_)
    {
        if (!result.empty())
        {
            result.push_back('.');
        }
        result += segment;
    }
    bool firstContainingType = true;
    for (const TypeNameSegment& segment : containingTypes_)
    {
        if (!result.empty())
        {
            result.push_back(firstContainingType ? '.' : '+');
        }
        appendTypeSegment(result, segment);
        firstContainingType = false;
    }
    if (!result.empty())
    {
        result.push_back(containingTypes_.empty() ? '.' : '+');
    }
    appendTypeSegment(result, TypeNameSegment{name_, arity_});
    return result;
}

std::string TypeIdentity::key() const
{
    return moduleName_ + "::" + fullyQualifiedName();
}

bool TypeIdentity::operator==(const TypeIdentity& other) const noexcept
{
    return moduleName_ == other.moduleName_ &&
        namespaceSegments_ == other.namespaceSegments_ &&
        containingTypes_ == other.containingTypes_ &&
        name_ == other.name_ &&
        arity_ == other.arity_;
}

bool TypeIdentity::isIdentifier(std::string_view value) noexcept
{
    if (value.empty())
    {
        return false;
    }
    const auto isStart = [](unsigned char character)
    {
        return std::isalpha(character) != 0 || character == '_';
    };
    const auto isContinue = [&isStart](unsigned char character)
    {
        return isStart(character) || std::isdigit(character) != 0;
    };

    if (!isStart(static_cast<unsigned char>(value.front())))
    {
        return false;
    }
    for (const char character : value.substr(1))
    {
        if (!isContinue(static_cast<unsigned char>(character)))
        {
            return false;
        }
    }
    return true;
}

void TypeIdentity::appendTypeSegment(std::string& output, const TypeNameSegment& segment)
{
    output += segment.name;
    if (segment.arity != 0)
    {
        output.push_back('`');
        output += std::to_string(segment.arity);
    }
}

void TypeIdentity::validate()
{
    if (!isIdentifier(moduleName_))
    {
        validationError_ = "module name is not a valid NativeSharp identifier";
        return;
    }
    for (const std::string& segment : namespaceSegments_)
    {
        if (!isIdentifier(segment))
        {
            validationError_ = "namespace contains an invalid NativeSharp identifier";
            return;
        }
    }
    for (const TypeNameSegment& segment : containingTypes_)
    {
        if (!isIdentifier(segment.name))
        {
            validationError_ = "containing type contains an invalid NativeSharp identifier";
            return;
        }
    }
    if (!isIdentifier(name_))
    {
        validationError_ = "type name is not a valid NativeSharp identifier";
    }
}
}
