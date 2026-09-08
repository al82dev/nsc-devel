// Stage 60 / 1.0-E7 -- the specification is a product contract, not an essay.
//
// This test deliberately does NOT claim to prove integer, ownership, EH, memory
// or ABI semantics. Their focused suites do that. It proves the traceability
// invariant E7 introduces: every clause in the normative specification is
// unique, has an explicit disposition, names implementation and executable
// evidence, and every named path exists. Conversely, every map row must occur
// as a normative heading. A new rule therefore cannot live only in prose, a
// test, or a code comment.

#include "nativesharp/Basic/Version.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using nativesharp::tests::TestContext;

std::string readFile(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

std::vector<std::string> split(std::string_view text, char delimiter)
{
    std::vector<std::string> fields;
    std::size_t begin = 0;
    while (begin <= text.size())
    {
        const std::size_t end = text.find(delimiter, begin);
        fields.emplace_back(text.substr(begin, end == std::string_view::npos ? text.size() - begin : end - begin));
        // A conforming checkout may use CRLF. std::getline removes '\n' but preserves '\r',
        // which used to turn the final evidence path in every map row into a nonexistent file.
        if (!fields.back().empty() && fields.back().back() == '\r')
            fields.back().pop_back();
        if (end == std::string_view::npos)
            break;
        begin = end + 1;
    }
    return fields;
}

void checkPathList(TestContext& tests, const std::filesystem::path& sourceRoot, std::string_view clause,
                   std::string_view kind, std::string_view value)
{
    const auto paths = split(value, ';');
    tests.expect(!paths.empty() && !paths.front().empty(), std::string(clause) + " has " + std::string(kind));
    for (const std::string& relative : paths)
    {
        tests.expect(relative.find('\\') == std::string::npos, relative + ": map paths use portable separators");
        tests.expect(!std::filesystem::path(relative).is_absolute(), relative + ": map path is repository-relative");
        tests.expect(std::filesystem::is_regular_file(sourceRoot / relative), relative + ": referenced file exists");
    }
}
} // namespace

int main()
{
    TestContext tests;
    const std::filesystem::path sourceRoot = NSC_SOURCE_DIR;
    const auto specificationPath = sourceRoot / "spec/NativeSharp_Language_Specification_1_0.md";
    const auto grammarPath = sourceRoot / "spec/NativeSharp_Grammar_1_0.ebnf";
    const auto mapPath = sourceRoot / "spec/NativeSharp_1_0_conformance.map";

    tests.expect(std::filesystem::is_regular_file(specificationPath), "normative specification exists");
    tests.expect(std::filesystem::is_regular_file(mapPath), "conformance map exists");
    tests.expect(std::filesystem::is_regular_file(grammarPath), "normative grammar exists");

    const std::string specification = readFile(specificationPath);
    const std::string grammar = readFile(grammarPath);
    const std::string map = readFile(mapPath);
    tests.expect(!specification.empty(), "normative specification is readable");
    tests.expect(!map.empty(), "conformance map is readable");
    tests.expect(!grammar.empty(), "normative grammar is readable");
    unsigned compilerStage = 0;
    const std::string_view compilerStageText = nativesharp::kCompilerStage;
    const auto stageParse =
        std::from_chars(compilerStageText.data(), compilerStageText.data() + compilerStageText.size(), compilerStage);
    tests.expect(stageParse.ec == std::errc() &&
                     stageParse.ptr == compilerStageText.data() + compilerStageText.size() && compilerStage >= 60,
                 "E7 remains present at Stage 60 and later snapshots");

    std::set<std::string> mappedIds;
    std::istringstream mapLines(map);
    std::string line;
    std::size_t row = 0;
    while (std::getline(mapLines, line))
    {
        ++row;
        if (line.empty() || line.starts_with('#'))
            continue;

        const auto fields = split(line, '|');
        tests.expect(fields.size() == 5, "map row " + std::to_string(row) + " has five fields");
        if (fields.size() != 5)
            continue;

        const std::string& id = fields[0];
        const std::string& status = fields[1];
        tests.expect(id.starts_with("NS-"), id + ": clause id has NS prefix");
        tests.expect(mappedIds.insert(id).second, id + ": map id is unique");
        tests.expect(status == "implemented" || status == "excluded", id + ": status is contractual");
        tests.expect(!fields[2].empty(), id + ": title is not empty");
        tests.expect(specification.find("### " + id + " —") != std::string::npos, id + ": normative heading exists");
        checkPathList(tests, sourceRoot, id, "implementation paths", fields[3]);
        checkPathList(tests, sourceRoot, id, "evidence paths", fields[4]);
    }

    std::set<std::string> specificationIds;
    std::istringstream specificationLines(specification);
    while (std::getline(specificationLines, line))
    {
        constexpr std::string_view prefix = "### NS-";
        if (!line.starts_with(prefix))
            continue;
        const std::size_t separator = line.find(" —");
        tests.expect(separator != std::string::npos, line + ": normative heading has title separator");
        if (separator == std::string::npos)
            continue;
        const std::string id = line.substr(4, separator - 4);
        tests.expect(specificationIds.insert(id).second, id + ": specification id is unique");
        tests.expect(mappedIds.contains(id), id + ": specification clause is mapped");
    }

    tests.expect(mappedIds == specificationIds, "specification and conformance map contain the same clauses");
    tests.expect(mappedIds.size() >= 40, "contract covers at least 40 independently traceable rules");
    tests.expect(specification.find("debe") != std::string::npos, "specification uses normative language");
    tests.expect(specification.find("x86_64-pc-windows-msvc") != std::string::npos,
                 "specification names the exact supported target");
    tests.expect(grammar.find("compilation-unit =") != std::string::npos, "grammar defines the compilation unit");
    tests.expect(grammar.find("assignment-expression =") != std::string::npos,
                 "grammar fixes expression precedence through assignment");
    tests.expect(grammar.find("ownership = \"owned\" | \"borrowed\" | \"raii\"") != std::string::npos,
                 "grammar contains NativeSharp ownership syntax");

    const std::string cmake = readFile(sourceRoot / "CMakeLists.txt");
    tests.expect(cmake.find("install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/spec/ "
                            "DESTINATION share/nativesharp/spec)") != std::string::npos,
                 "the normative contract is installed with the compiler");

    return tests.result();
}
