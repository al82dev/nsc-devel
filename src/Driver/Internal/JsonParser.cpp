#include "JsonParser.h"

#include "Json.h"

#include <cctype>
#include <utility>

namespace nativesharp::driver
{
JsonParser::JsonParser(std::string_view text) : text_(text)
{
}

bool JsonParser::parse(Json &value, std::string &error)
{
    skip();
    if (!parseValue(value, error))
        return false;
    skip();
    if (position_ != text_.size())
        return fail(error, "unexpected trailing JSON content");
    return true;
}

bool JsonParser::fail(std::string &error, std::string_view message) const
{
    error = std::string(message) + " at byte " + std::to_string(position_);
    return false;
}

void JsonParser::skip()
{
    while (position_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[position_])))
        ++position_;
}

bool JsonParser::parseValue(Json &value, std::string &error)
{
    if (position_ >= text_.size())
        return fail(error, "expected JSON value");
    const char current = text_[position_];
    if (current == '{')
        return parseObject(value, error);
    if (current == '[')
        return parseArray(value, error);
    if (current == '"')
    {
        std::string parsed;
        if (!parseString(parsed, error))
            return false;
        value.value = std::move(parsed);
        return true;
    }
    if (text_.substr(position_, 4) == "true")
    {
        position_ += 4;
        value.value = true;
        return true;
    }
    if (text_.substr(position_, 5) == "false")
    {
        position_ += 5;
        value.value = false;
        return true;
    }
    if (text_.substr(position_, 4) == "null")
    {
        position_ += 4;
        value.value = nullptr;
        return true;
    }
    if (current == '-' || std::isdigit(static_cast<unsigned char>(current)))
    {
        const std::size_t start = position_++;
        while (position_ < text_.size() &&
               (std::isdigit(static_cast<unsigned char>(text_[position_])) || text_[position_] == '.' ||
                text_[position_] == 'e' || text_[position_] == 'E' || text_[position_] == '+' ||
                text_[position_] == '-'))
            ++position_;
        try
        {
            value.value = std::stod(std::string(text_.substr(start, position_ - start)));
        }
        catch (...)
        {
            return fail(error, "invalid JSON number");
        }
        return true;
    }
    return fail(error, "invalid JSON value");
}

bool JsonParser::parseString(std::string &value, std::string &error)
{
    if (text_[position_++] != '"')
        return fail(error, "expected string");
    while (position_ < text_.size())
    {
        const unsigned char current = static_cast<unsigned char>(text_[position_++]);
        if (current == '"')
            return true;
        if (current < 0x20U)
            return fail(error, "control character in string");
        if (current != '\\')
        {
            value.push_back(static_cast<char>(current));
            continue;
        }
        if (position_ >= text_.size())
            return fail(error, "incomplete string escape");
        const char escaped = text_[position_++];
        switch (escaped)
        {
        case '"':
            value.push_back('"');
            break;
        case '\\':
            value.push_back('\\');
            break;
        case '/':
            value.push_back('/');
            break;
        case 'b':
            value.push_back('\b');
            break;
        case 'f':
            value.push_back('\f');
            break;
        case 'n':
            value.push_back('\n');
            break;
        case 'r':
            value.push_back('\r');
            break;
        case 't':
            value.push_back('\t');
            break;
        case 'u':
        {
            if (position_ + 4U > text_.size())
                return fail(error, "incomplete unicode escape");
            unsigned codePoint = 0;
            for (unsigned index = 0; index < 4U; ++index)
            {
                const char digit = text_[position_++];
                codePoint <<= 4U;
                if (digit >= '0' && digit <= '9')
                    codePoint += static_cast<unsigned>(digit - '0');
                else if (digit >= 'a' && digit <= 'f')
                    codePoint += 10U + static_cast<unsigned>(digit - 'a');
                else if (digit >= 'A' && digit <= 'F')
                    codePoint += 10U + static_cast<unsigned>(digit - 'A');
                else
                    return fail(error, "invalid unicode escape");
            }
            if (codePoint <= 0x7fU)
                value.push_back(static_cast<char>(codePoint));
            else if (codePoint <= 0x7ffU)
            {
                value.push_back(static_cast<char>(0xc0U | (codePoint >> 6U)));
                value.push_back(static_cast<char>(0x80U | (codePoint & 0x3fU)));
            }
            else
            {
                value.push_back(static_cast<char>(0xe0U | (codePoint >> 12U)));
                value.push_back(static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3fU)));
                value.push_back(static_cast<char>(0x80U | (codePoint & 0x3fU)));
            }
            break;
        }
        default:
            return fail(error, "invalid string escape");
        }
    }
    return fail(error, "unterminated string");
}

bool JsonParser::parseObject(Json &value, std::string &error)
{
    ++position_;
    Json::Object object;
    skip();
    if (position_ < text_.size() && text_[position_] == '}')
    {
        ++position_;
        value.value = std::move(object);
        return true;
    }
    for (;;)
    {
        skip();
        if (position_ >= text_.size() || text_[position_] != '"')
            return fail(error, "expected object key");
        std::string key;
        if (!parseString(key, error))
            return false;
        skip();
        if (position_ >= text_.size() || text_[position_++] != ':')
            return fail(error, "expected ':'");
        skip();
        Json member;
        if (!parseValue(member, error))
            return false;
        if (!object.emplace(std::move(key), std::move(member)).second)
            return fail(error, "duplicate object key");
        skip();
        if (position_ >= text_.size())
            return fail(error, "unterminated object");
        if (text_[position_] == '}')
        {
            ++position_;
            value.value = std::move(object);
            return true;
        }
        if (text_[position_++] != ',')
            return fail(error, "expected ','");
    }
}

bool JsonParser::parseArray(Json &value, std::string &error)
{
    ++position_;
    Json::Array array;
    skip();
    if (position_ < text_.size() && text_[position_] == ']')
    {
        ++position_;
        value.value = std::move(array);
        return true;
    }
    for (;;)
    {
        skip();
        Json element;
        if (!parseValue(element, error))
            return false;
        array.push_back(std::move(element));
        skip();
        if (position_ >= text_.size())
            return fail(error, "unterminated array");
        if (text_[position_] == ']')
        {
            ++position_;
            value.value = std::move(array);
            return true;
        }
        if (text_[position_++] != ',')
            return fail(error, "expected ','");
    }
}
} // namespace nativesharp::driver
