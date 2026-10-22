#include "nativesharp/Compilation/TypeIdentity.h"

#include <cctype>
#include <utility>

namespace nativesharp::compilation
{
std::string symbolFragmentForIdentityKey(std::string_view identityKey)
{
    static constexpr char kHex[] = "0123456789ABCDEF";

    std::string result;
    result.reserve(identityKey.size() * 3);
    for (const char character : identityKey)
    {
        const auto byte = static_cast<unsigned char>(character);
        // Se compara contra los rangos ASCII a mano y no con `std::isalnum`, que depende del
        // locale: el nombre de un simbolo no puede cambiar segun la maquina que compile.
        const bool asciiAlphaNumeric =
            (byte >= 'a' && byte <= 'z') ||
            (byte >= 'A' && byte <= 'Z') ||
            (byte >= '0' && byte <= '9');
        if (asciiAlphaNumeric || byte == '_')
        {
            result.push_back(static_cast<char>(byte));
            continue;
        }
        result.push_back('$');
        result.push_back(kHex[byte >> 4]);
        result.push_back(kHex[byte & 0x0F]);
    }
    return result;
}

bool symbolNeedsIrTextQuotes(std::string_view symbolName) noexcept
{
    if (symbolName.empty()) return true;
    // La misma condicion que AsmWriter::PrintLLVMName, y por los mismos motivos: se comprueba a
    // mano contra los rangos ASCII y no con `std::isalnum`, que depende del locale.
    const auto first = static_cast<unsigned char>(symbolName.front());
    if (first >= '0' && first <= '9') return true;
    for (const char raw : symbolName)
    {
        const auto byte = static_cast<unsigned char>(raw);
        const bool plain =
            (byte >= 'a' && byte <= 'z') ||
            (byte >= 'A' && byte <= 'Z') ||
            (byte >= '0' && byte <= '9') ||
            byte == '-' || byte == '.' || byte == '_';
        if (!plain) return true;
    }
    return false;
}

std::string irTextSymbolReference(std::string_view symbolName)
{
    std::string result("@");
    if (!symbolNeedsIrTextQuotes(symbolName))
    {
        result.append(symbolName);
        return result;
    }
    result.push_back('"');
    result.append(symbolName);
    result.push_back('"');
    return result;
}

std::string irTextIdentitySymbolPrefix(std::string_view prefix)
{
    std::string result("@\"");
    result.append(prefix);
    return result;
}

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
    initializeNames();
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
    return fullyQualifiedName_;
}

std::string TypeIdentity::key() const
{
    return key_;
}

std::string_view TypeIdentity::fullyQualifiedNameView() const noexcept
{
    return fullyQualifiedName_;
}

std::string_view TypeIdentity::keyView() const noexcept
{
    return key_;
}

void TypeIdentity::initializeNames()
{
    for (const std::string& segment : namespaceSegments_)
    {
        if (!fullyQualifiedName_.empty())
        {
            fullyQualifiedName_.push_back('.');
        }
        fullyQualifiedName_ += segment;
    }
    bool firstContainingType = true;
    for (const TypeNameSegment& segment : containingTypes_)
    {
        if (!fullyQualifiedName_.empty())
        {
            fullyQualifiedName_.push_back(firstContainingType ? '.' : '+');
        }
        appendTypeSegment(fullyQualifiedName_, segment);
        firstContainingType = false;
    }
    if (!fullyQualifiedName_.empty())
    {
        fullyQualifiedName_.push_back(containingTypes_.empty() ? '.' : '+');
    }
    appendTypeSegment(fullyQualifiedName_, TypeNameSegment{name_, arity_});
    key_.reserve(moduleName_.size() + 2U + fullyQualifiedName_.size());
    key_ = moduleName_;
    key_ += "::";
    key_ += fullyQualifiedName_;
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
