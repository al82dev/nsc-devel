#pragma once

#include "nativesharp/Compilation/SimpleXmlElement.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nativesharp::compilation
{
class SimpleXmlParser final
{
public:
    bool parse(std::string_view source, SimpleXmlElement& root, std::string& error);

private:
    bool parseElement(SimpleXmlElement& element, std::size_t depth);
    bool parseElementStart(SimpleXmlElement& element, std::size_t depth);
    bool parseElementAttributes(SimpleXmlElement& element, bool& selfClosing);
    bool parseElementContent(SimpleXmlElement& element, std::size_t depth);
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
