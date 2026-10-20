#include "nativesharp/Runtime/RuntimeAbiContract.h"
#include "Internal/ActualSymbol.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

namespace nativesharp::runtime
{
namespace
{
std::string_view trim(std::string_view value)
{
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
    {
        value.remove_prefix(1);
    }
    while (!value.empty() &&
           (value.back() == ' ' || value.back() == '\t' || value.back() == '\r'))
    {
        value.remove_suffix(1);
    }
    return value;
}

std::string canonicalFunctionType(std::string_view line, std::size_t nameMarker)
{
    std::string_view prefix = trim(line.substr(7, nameMarker - 7));
    const std::size_t returnSeparator = prefix.rfind(' ');
    const std::string_view returnType = returnSeparator == std::string_view::npos
        ? prefix
        : prefix.substr(returnSeparator + 1);

    const std::size_t open = line.find('(', nameMarker);
    const std::size_t close = line.find(')', open);
    if (open == std::string_view::npos || close == std::string_view::npos)
    {
        return {};
    }

    std::string result(returnType);
    result.push_back('(');
    std::string_view parameters = line.substr(open + 1, close - open - 1);
    bool first = true;
    while (!parameters.empty())
    {
        const std::size_t comma = parameters.find(',');
        std::string_view parameter = trim(parameters.substr(0, comma));
        const std::size_t typeEnd = parameter.find_first_of(" \t");
        parameter = parameter.substr(0, typeEnd);
        if (!first)
        {
            result.push_back(',');
        }
        result.append(parameter);
        first = false;
        if (comma == std::string_view::npos)
        {
            break;
        }
        parameters.remove_prefix(comma + 1);
    }
    result.push_back(')');
    return result;
}

bool collectActualSymbols(
    std::string_view llvmIr,
    std::unordered_map<std::string, ActualSymbol>& symbols,
    std::string& error)
{
    constexpr std::string_view prefix = "__nativesharp_";
    while (!llvmIr.empty())
    {
        const std::size_t newline = llvmIr.find('\n');
        const std::string_view line = trim(llvmIr.substr(0, newline));
        if (newline == std::string_view::npos)
        {
            llvmIr = {};
        }
        else
        {
            llvmIr.remove_prefix(newline + 1);
        }

        RuntimeAbiSymbolKind kind = RuntimeAbiSymbolKind::Function;
        std::size_t nameBegin = std::string_view::npos;
        std::string type;
        if (line.starts_with("define "))
        {
            nameBegin = line.find("@" + std::string(prefix));
            if (nameBegin != std::string_view::npos)
            {
                type = canonicalFunctionType(line, nameBegin);
            }
        }
        else if (line.starts_with("@" + std::string(prefix)))
        {
            kind = RuntimeAbiSymbolKind::Data;
            nameBegin = 0;
            const std::size_t global = line.find(" = global ");
            if (global != std::string_view::npos)
            {
                const std::string_view tail = trim(line.substr(global + 10));
                type.assign(tail.substr(0, tail.find_first_of(" \t")));
            }
        }

        if (nameBegin == std::string_view::npos)
        {
            continue;
        }
        const std::size_t nameEnd = line.find_first_of("( \t=", nameBegin + 1);
        const std::string name(line.substr(nameBegin + 1, nameEnd - nameBegin - 1));
        if (type.empty())
        {
            error = "cannot determine LLVM type of runtime symbol '" + name + "'";
            return false;
        }
        if (!symbols.emplace(name, ActualSymbol{kind, std::move(type)}).second)
        {
            error = "duplicate runtime symbol '" + name + "'";
            return false;
        }
    }
    return true;
}

bool isEntryPoint(const RuntimeAbiSymbol& symbol)
{
    return symbol.name == "__nativesharp_start";
}
}

std::string runtimeAbiContractManifest()
{
    std::string result = "NSRUNTIMEABI " + std::to_string(kRuntimeAbiMajor) + "." +
        std::to_string(kRuntimeAbiMinor) + "\n";
    result += "symbol_set=" + std::to_string(kRuntimeSymbolSetVersion) + "\n";
    result += "target=x86_64-pc-windows-msvc\n";
    result += "pointer_model=llvm-opaque\n";
    result += "calling_convention=win64-c\n";
    for (const RuntimeAbiSymbol& symbol : kRuntimeAbiSymbols)
    {
        result += symbol.kind == RuntimeAbiSymbolKind::Function ? "function " : "data ";
        result += symbol.name;
        result.push_back(' ');
        result += symbol.llvmType;
        result.push_back('\n');
    }
    return result;
}

std::uint64_t runtimeAbiContractFingerprint()
{
    constexpr std::uint64_t offsetBasis = 14695981039346656037ULL;
    constexpr std::uint64_t prime = 1099511628211ULL;
    std::uint64_t value = offsetBasis;
    for (const char byte : runtimeAbiContractManifest())
    {
        value ^= static_cast<unsigned char>(byte);
        value *= prime;
    }
    return value;
}

std::string runtimeAbiContractFingerprintHex()
{
    std::ostringstream stream;
    stream << std::hex << std::setfill('0') << std::setw(16)
           << runtimeAbiContractFingerprint();
    return stream.str();
}

bool writeRuntimeAbiContract(const std::filesystem::path& destination, std::string& error)
{
    error.clear();
    if (destination.empty())
    {
        error = "runtime ABI contract destination is empty";
        return false;
    }
    const std::filesystem::path parent = destination.parent_path();
    if (!parent.empty())
    {
        std::error_code filesystemError;
        std::filesystem::create_directories(parent, filesystemError);
        if (filesystemError)
        {
            error = "cannot create runtime ABI contract directory '" + parent.string() +
                "': " + filesystemError.message();
            return false;
        }
    }
    std::ofstream stream(destination, std::ios::binary | std::ios::trunc);
    const std::string manifest = runtimeAbiContractManifest();
    if (!stream || !stream.write(manifest.data(), static_cast<std::streamsize>(manifest.size())))
    {
        error = "cannot write runtime ABI contract '" + destination.string() + "'";
        return false;
    }
    return true;
}

bool verifyRuntimeAbiContractFile(const std::filesystem::path& source, std::string& error)
{
    error.clear();
    std::ifstream stream(source, std::ios::binary);
    if (!stream)
    {
        error = "cannot open runtime ABI contract '" + source.string() + "'";
        return false;
    }
    std::string actual{
        std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    actual.erase(std::remove(actual.begin(), actual.end(), '\r'), actual.end());
    const std::string expected = runtimeAbiContractManifest();
    if (actual != expected)
    {
        error = "incompatible runtime ABI contract '" + source.string() +
            "': expected fingerprint " + runtimeAbiContractFingerprintHex();
        return false;
    }
    return true;
}

bool verifyRuntimeAbiContract(
    std::string_view llvmIr,
    RuntimeAbiImageKind imageKind,
    std::string& error)
{
    error.clear();
    std::unordered_map<std::string, ActualSymbol> actual;
    if (!collectActualSymbols(llvmIr, actual, error))
    {
        return false;
    }

    for (const RuntimeAbiSymbol& expected : kRuntimeAbiSymbols)
    {
        const bool required = imageKind == RuntimeAbiImageKind::Executable ||
            (imageKind == RuntimeAbiImageKind::SupportLibrary && !isEntryPoint(expected)) ||
            (imageKind == RuntimeAbiImageKind::EntryPoint && isEntryPoint(expected));
        const auto found = actual.find(std::string(expected.name));
        if (!required)
        {
            if (found != actual.end() && imageKind != RuntimeAbiImageKind::Executable)
            {
                error = "runtime image unexpectedly defines symbol '" +
                    std::string(expected.name) + "'";
                return false;
            }
            continue;
        }
        if (found == actual.end())
        {
            error = "runtime ABI symbol is missing: '" + std::string(expected.name) + "'";
            return false;
        }
        if (found->second.kind != expected.kind)
        {
            error = "runtime ABI symbol kind mismatch for '" + std::string(expected.name) + "'";
            return false;
        }
        if (found->second.llvmType != expected.llvmType)
        {
            error = "runtime ABI type mismatch for '" + std::string(expected.name) +
                "': found '" + found->second.llvmType + "', expected '" +
                std::string(expected.llvmType) + "'";
            return false;
        }
        actual.erase(found);
    }

    if (!actual.empty())
    {
        auto first = std::min_element(actual.begin(), actual.end(), [](const auto& left, const auto& right) {
            return left.first < right.first;
        });
        error = "uncontracted runtime ABI symbol: '" + first->first + "'";
        return false;
    }
    return true;
}
}
