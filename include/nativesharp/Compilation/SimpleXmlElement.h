#pragma once

#include <string>
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
}
