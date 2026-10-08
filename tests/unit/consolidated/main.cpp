#include "nsc_test/TestRegistry.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{
struct Options
{
    bool list = false;
    bool all = false;
    std::optional<std::string> caseId;
    std::optional<std::string> component;
    std::optional<std::filesystem::path> reportJson;
};

std::string escapeJson(std::string_view value)
{
    std::string escaped;
    escaped.reserve(value.size());
    for (const char character : value)
    {
        switch (character)
        {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped += character;
            break;
        }
    }
    return escaped;
}

Options parseOptions(int argumentCount, char** arguments)
{
    Options options;
    for (int index = 1; index < argumentCount; ++index)
    {
        const std::string_view argument(arguments[index]);
        if (argument == "--list")
        {
            options.list = true;
        }
        else if (argument == "--all")
        {
            options.all = true;
        }
        else if (argument == "--case" && index + 1 < argumentCount)
        {
            options.caseId = arguments[++index];
        }
        else if (argument == "--component" && index + 1 < argumentCount)
        {
            options.component = arguments[++index];
        }
        else if (argument == "--report-json" && index + 1 < argumentCount)
        {
            options.reportJson = arguments[++index];
        }
        else
        {
            throw std::invalid_argument("unknown or incomplete argument: " + std::string(argument));
        }
    }
    const int selectors = static_cast<int>(options.list) + static_cast<int>(options.all) +
                          static_cast<int>(options.caseId.has_value()) +
                          static_cast<int>(options.component.has_value());
    if (selectors == 0)
    {
        options.all = true;
    }
    else if (selectors != 1)
    {
        throw std::invalid_argument("select exactly one of --list, --all, --case, or --component");
    }
    return options;
}

void writeReport(
    const std::filesystem::path& path,
    const std::vector<nsc::test::TestResult>& results)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        throw std::runtime_error("cannot open JSON report: " + path.string());
    }
    std::size_t passed = 0;
    for (const auto& result : results)
    {
        passed += static_cast<std::size_t>(result.passed);
    }
    output << "{\n  \"selected\": " << results.size() << ",\n"
           << "  \"passed\": " << passed << ",\n"
           << "  \"failed\": " << (results.size() - passed) << ",\n"
           << "  \"results\": [";
    for (std::size_t index = 0; index < results.size(); ++index)
    {
        const auto& result = results[index];
        output << (index == 0 ? "\n" : ",\n")
               << "    {\"id\": \"" << escapeJson(result.id) << "\", "
               << "\"component\": \"" << escapeJson(result.component) << "\", "
               << "\"passed\": " << (result.passed ? "true" : "false") << ", "
               << "\"duration_us\": " << result.duration.count() << ", "
               << "\"failures\": [";
        for (std::size_t failure = 0; failure < result.failures.size(); ++failure)
        {
            output << (failure == 0 ? "" : ", ") << "\""
                   << escapeJson(result.failures[failure]) << "\"";
        }
        output << "]}";
    }
    output << (results.empty() ? "" : "\n") << "  ]\n}\n";
}
}

int main(int argumentCount, char** arguments)
{
    try
    {
        const Options options = parseOptions(argumentCount, arguments);
        nsc::test::TestRegistry registry;
        if (options.list)
        {
            for (const auto& testCase : registry.cases())
            {
                std::cout << testCase.id << '\t' << testCase.component << '\t' << testCase.name
                          << '\n';
            }
            std::cout << "cases=" << registry.cases().size() << '\n';
            return 0;
        }

        std::vector<const nsc::test::TestCase*> selected;
        if (options.caseId)
        {
            const auto* testCase = registry.find(*options.caseId);
            if (testCase == nullptr)
            {
                std::cerr << "unknown test case: " << *options.caseId << '\n';
                return 2;
            }
            selected.push_back(testCase);
        }
        else if (options.component)
        {
            selected = registry.byComponent(*options.component);
            if (selected.empty())
            {
                std::cerr << "component has no tests: " << *options.component << '\n';
                return 2;
            }
        }
        else
        {
            selected = registry.all();
        }

        const auto results = registry.run(selected);
        std::size_t passed = 0;
        for (const auto& result : results)
        {
            passed += static_cast<std::size_t>(result.passed);
            std::cout << (result.passed ? "PASS" : "FAIL") << ' ' << result.id << '\n';
            for (const auto& failure : result.failures)
            {
                std::cout << "  " << failure << '\n';
            }
        }
        if (options.reportJson)
        {
            writeReport(*options.reportJson, results);
        }
        std::cout << "selected=" << results.size() << " passed=" << passed
                  << " failed=" << (results.size() - passed) << '\n';
        return passed == results.size() ? 0 : 1;
    }
    catch (const std::exception& error)
    {
        std::cerr << "nsc_unit_tests: error: " << error.what() << '\n';
        return 2;
    }
}
