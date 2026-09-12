#include "nativesharp/Compilation/SimpleXmlParser.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace nativesharp::compilation
{
bool SimpleXmlParser::parse(std::string_view source, SimpleXmlElement& root, std::string& error)
{
    source_ = source;
    offset_ = 0;
    error_ = &error;
    error.clear();
    root = {};

    if (startsWith("\xEF\xBB\xBF"))
    {
        offset_ += 3;
    }
    if (!skipMiscellaneous())
    {
        return false;
    }
    if (offset_ == source_.size())
    {
        return fail("XML document is empty");
    }
    if (!parseElement(root, 1U))
    {
        return false;
    }
    if (!skipMiscellaneous())
    {
        return false;
    }
    if (offset_ != source_.size())
    {
        return fail("unexpected content after the root element");
    }
    return true;
}

bool SimpleXmlParser::parseElement(SimpleXmlElement& element, std::size_t depth)
{
    constexpr std::size_t kMaximumXmlDepth = 128U;
    if (depth > kMaximumXmlDepth)
        return fail("XML nesting exceeds the 128-element safety limit");
    if (!consume('<'))
    {
        return fail("expected '<' to begin an element");
    }
    if (startsWith("/") || startsWith("!") || startsWith("?"))
    {
        return fail("expected an element name");
    }
    if (!parseName(element.name))
    {
        return false;
    }

    while (true)
    {
        skipWhitespace();
        if (consume("/>"))
        {
            return true;
        }
        if (consume('>'))
        {
            break;
        }

        std::string attributeName;
        if (!parseName(attributeName))
        {
            return false;
        }
        const auto duplicate = std::find_if(
            element.attributes.begin(),
            element.attributes.end(),
            [&attributeName](const auto& attribute)
            {
                return attribute.first == attributeName;
            });
        if (duplicate != element.attributes.end())
        {
            return fail("duplicate XML attribute '" + attributeName + "'");
        }

        skipWhitespace();
        if (!consume('='))
        {
            return fail("expected '=' after XML attribute '" + attributeName + "'");
        }
        skipWhitespace();
        std::string attributeValue;
        if (!parseAttributeValue(attributeValue))
        {
            return false;
        }
        element.attributes.emplace_back(std::move(attributeName), std::move(attributeValue));
    }

    while (offset_ < source_.size())
    {
        if (startsWith("<!--"))
        {
            if (!skipComment())
            {
                return false;
            }
            continue;
        }
        if (startsWith("</"))
        {
            offset_ += 2;
            std::string closingName;
            if (!parseName(closingName))
            {
                return false;
            }
            skipWhitespace();
            if (!consume('>'))
            {
                return fail("expected '>' after closing element '" + closingName + "'");
            }
            if (closingName != element.name)
            {
                return fail(
                    "closing element '" + closingName + "' does not match '" + element.name + "'");
            }
            return true;
        }
        if (startsWith("<?"))
        {
            if (!skipProcessingInstruction())
            {
                return false;
            }
            continue;
        }
        if (startsWith("<!"))
        {
            return fail("DOCTYPE, CDATA, and XML declarations other than comments are not supported");
        }
        if (startsWith("<"))
        {
            SimpleXmlElement child;
            if (!parseElement(child, depth + 1U))
            {
                return false;
            }
            element.children.push_back(std::move(child));
            continue;
        }
        if (!parseText(element.text))
        {
            return false;
        }
    }

    return fail("element '" + element.name + "' is not closed");
}

bool SimpleXmlParser::parseName(std::string& name)
{
    if (offset_ >= source_.size())
    {
        return fail("expected an XML name");
    }
    const auto isStart = [](unsigned char value)
    {
        return std::isalpha(value) != 0 || value == '_' || value == ':';
    };
    const auto isContinue = [&isStart](unsigned char value)
    {
        return isStart(value) || std::isdigit(value) != 0 || value == '-' || value == '.';
    };
    if (!isStart(static_cast<unsigned char>(source_[offset_])))
    {
        return fail("expected an XML name");
    }

    const std::size_t begin = offset_++;
    while (offset_ < source_.size() && isContinue(static_cast<unsigned char>(source_[offset_])))
    {
        ++offset_;
    }
    name.assign(source_.substr(begin, offset_ - begin));
    return true;
}

bool SimpleXmlParser::parseAttributeValue(std::string& value)
{
    if (offset_ >= source_.size() || (source_[offset_] != '"' && source_[offset_] != '\''))
    {
        return fail("expected a quoted XML attribute value");
    }
    const char quote = source_[offset_++];
    while (offset_ < source_.size() && source_[offset_] != quote)
    {
        if (source_[offset_] == '<')
        {
            return fail("'<' is not valid inside an XML attribute value");
        }
        if (source_[offset_] == '&')
        {
            if (!decodeEntity(value))
            {
                return false;
            }
        }
        else
        {
            value.push_back(source_[offset_++]);
        }
    }
    if (!consume(quote))
    {
        return fail("unterminated XML attribute value");
    }
    return true;
}

bool SimpleXmlParser::parseText(std::string& text)
{
    while (offset_ < source_.size() && source_[offset_] != '<')
    {
        if (source_[offset_] == '&')
        {
            if (!decodeEntity(text))
            {
                return false;
            }
        }
        else
        {
            text.push_back(source_[offset_++]);
        }
    }
    return true;
}

bool SimpleXmlParser::skipMiscellaneous()
{
    while (true)
    {
        skipWhitespace();
        if (startsWith("<!--"))
        {
            if (!skipComment())
            {
                return false;
            }
            continue;
        }
        if (startsWith("<?"))
        {
            if (!skipProcessingInstruction())
            {
                return false;
            }
            continue;
        }
        return true;
    }
}

bool SimpleXmlParser::skipComment()
{
    if (!consume("<!--"))
    {
        return fail("expected XML comment");
    }
    const std::size_t end = source_.find("-->", offset_);
    if (end == std::string_view::npos)
    {
        return fail("unterminated XML comment");
    }
    if (source_.substr(offset_, end - offset_).find("--") != std::string_view::npos)
    {
        return fail("'--' is not valid inside an XML comment");
    }
    offset_ = end + 3;
    return true;
}

bool SimpleXmlParser::skipProcessingInstruction()
{
    if (!consume("<?"))
    {
        return fail("expected XML processing instruction");
    }
    const std::size_t end = source_.find("?>", offset_);
    if (end == std::string_view::npos)
    {
        return fail("unterminated XML processing instruction");
    }
    offset_ = end + 2;
    return true;
}

bool SimpleXmlParser::decodeEntity(std::string& output)
{
    if (!consume('&'))
    {
        return fail("expected XML entity");
    }
    const std::size_t end = source_.find(';', offset_);
    if (end == std::string_view::npos)
    {
        return fail("unterminated XML entity");
    }
    const std::string_view entity = source_.substr(offset_, end - offset_);
    if (entity == "amp") output.push_back('&');
    else if (entity == "lt") output.push_back('<');
    else if (entity == "gt") output.push_back('>');
    else if (entity == "quot") output.push_back('"');
    else if (entity == "apos") output.push_back('\'');
    else return fail("unsupported XML entity '&" + std::string(entity) + ";'");
    offset_ = end + 1;
    return true;
}

bool SimpleXmlParser::fail(std::string message)
{
    if (error_ != nullptr && error_->empty())
    {
        *error_ = location() + ": " + std::move(message);
    }
    return false;
}

bool SimpleXmlParser::consume(char value) noexcept
{
    if (offset_ >= source_.size() || source_[offset_] != value)
    {
        return false;
    }
    ++offset_;
    return true;
}

bool SimpleXmlParser::consume(std::string_view value) noexcept
{
    if (!startsWith(value))
    {
        return false;
    }
    offset_ += value.size();
    return true;
}

bool SimpleXmlParser::startsWith(std::string_view value) const noexcept
{
    return source_.substr(offset_, value.size()) == value;
}

void SimpleXmlParser::skipWhitespace() noexcept
{
    while (offset_ < source_.size() && std::isspace(static_cast<unsigned char>(source_[offset_])) != 0)
    {
        ++offset_;
    }
}

std::string SimpleXmlParser::location() const
{
    std::size_t line = 1;
    std::size_t column = 1;
    for (std::size_t index = 0; index < offset_ && index < source_.size(); ++index)
    {
        if (source_[index] == '\n')
        {
            ++line;
            column = 1;
        }
        else
        {
            ++column;
        }
    }
    return "XML " + std::to_string(line) + ":" + std::to_string(column);
}
}
