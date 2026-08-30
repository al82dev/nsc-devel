#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nativesharp::compilation
{
struct SimpleXmlElement final
{
    std::string name;
    std::vector<std::pair<std::string, std::string>> attributes;
    std::string text;
    std::vector<SimpleXmlElement> children;
};

class SimpleXmlParser final
{
public:
    bool parse(std::string_view source, SimpleXmlElement& root, std::string& error);

private:
    bool parseElement(SimpleXmlElement& element);
    bool parseName(std::string& name);
    bool parseAttributeValue(std::string& value);
    bool parseText(std::string& text);
    bool skipMiscellaneous();
    bool skipComment();
    bool skipProcessingInstruction();
    bool decodeEntity(std::string& output);
    bool fail(std::string message);
    bool consume(char value) noexcept;
    bool consume(std::string_view value) noexcept;
    bool startsWith(std::string_view value) const noexcept;
    void skipWhitespace() noexcept;
    std::string location() const;

    std::string_view source_;
    std::size_t offset_ = 0;
    std::string* error_ = nullptr;
};
}
