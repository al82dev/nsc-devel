#include "CompilationTestSupport.h"

namespace nsc::suite
{
namespace
{
using namespace compilation_support;

bool parseXml(std::string_view text, SimpleXmlElement& root, std::string& error)
{
    return SimpleXmlParser{}.parse(text, root, error);
}

void xmlDocumentContract(TestContext& context, std::size_t variant)
{
    SimpleXmlElement root;
    std::string error;
    const std::string source = variant == 1 ? "<Project/>" :
        variant == 2 ? "\xEF\xBB\xBF<?xml version=\"1.0\"?><!--before--><Project/><!--after-->" :
        variant == 3 ? "" :
        variant == 4 ? "<Project/><Other/>" :
        variant == 5 ? "text<Project/>" :
        variant == 6 ? "<?build deterministic?><Project/>" :
        variant == 7 ? "<!--a--><Project/><!--b-->" :
            "<Project>" + std::string(1024U, 'x') + "</Project>";
    const bool result = parseXml(source, root, error);
    const bool expected = variant != 3 && variant != 4 && variant != 5;
    context.expect(result == expected && (result ? root.name == "Project" : !error.empty()),
        "CF-COMPILATION-022 XML accepts BOM/miscellany around exactly one root and rejects other documents");
}

void xmlElementContract(TestContext& context, std::size_t variant)
{
    SimpleXmlElement root;
    std::string error;
    std::string source;
    if (variant == 1) source = "<A/>";
    else if (variant == 2) source = "<A><B/><C><D/></C></A>";
    else if (variant == 3) source = "<A>";
    else if (variant == 4) source = "<A></B>";
    else if (variant == 5) source = "<A><B></B>";
    else if (variant == 6) source = "<A> <B /> </A>";
    else if (variant == 7) source = "<A><B/></A>";
    else
    {
        for (std::size_t index = 0; index < 128U; ++index) source += "<N>";
        for (std::size_t index = 0; index < 128U; ++index) source += "</N>";
    }
    const bool result = parseXml(source, root, error);
    const bool expected = variant != 3 && variant != 4 && variant != 5;
    context.expect(result == expected && (result || !error.empty()),
        "CF-COMPILATION-023 element nesting, self-close, closing identity, and depth boundary are exact");
}

void xmlNameAttributeContract(TestContext& context, std::size_t variant)
{
    SimpleXmlElement root;
    std::string error;
    const std::string source = variant == 1 ? "<A x=\"1\"/>" :
        variant == 2 ? "<_A-1 x='one' y=\"two\"/>" :
        variant == 3 ? "<>" :
        variant == 4 ? "<A x=1/>" :
        variant == 5 ? "<A x=\"1\" x=\"2\"/>" :
        variant == 6 ? "<A ns:name=\"v\"/>" :
        variant == 7 ? "<A x=\"1\" y=\"2\"/>" :
            "<A " + std::string(256U, 'a') + "=\"v\"/>";
    const bool result = parseXml(source, root, error);
    const bool expected = variant != 3 && variant != 4 && variant != 5;
    bool exact = result == expected;
    if (result && variant == 2)
    {
        exact = root.attributes.size() == 2U && root.attributes[0].second == "one" &&
            root.attributes[1].second == "two";
    }
    context.expect(exact && (result || !error.empty()),
        "CF-COMPILATION-024 XML names and quoted attributes preserve order and reject duplicates");
}

void xmlTextEntityContract(TestContext& context, std::size_t variant)
{
    SimpleXmlElement root;
    std::string error;
    const std::string source = variant == 1 ? "<A>text</A>" :
        variant == 2 ? "<A>&amp;&lt;&gt;&quot;&apos;</A>" :
        variant == 3 ? "<A>&unknown;</A>" :
        variant == 4 ? "<A>&amp</A>" :
        variant == 5 ? "<A></A>" :
        variant == 6 ? "<A>left<B/>right</A>" :
        variant == 7 ? "<A>&amp;&amp;</A>" :
            "<A>" + std::string(65536U, 'x') + "</A>";
    const bool result = parseXml(source, root, error);
    const bool expected = variant != 3 && variant != 4;
    bool exact = result == expected;
    if (result && variant == 2)
    {
        exact = root.text == "&<>\"'";
    }
    if (result && variant == 6)
    {
        exact = root.text == "leftright" && root.children.size() == 1U;
    }
    context.expect(exact && (result || !error.empty()),
        "CF-COMPILATION-025 text and five predefined entities decode exactly; unknown entities fail closed");
}

void xmlMiscellaneousContract(TestContext& context, std::size_t variant)
{
    SimpleXmlElement root;
    std::string error;
    const std::string source = variant == 1 ? "<!--ok--><A/>" :
        variant == 2 ? "<?xml version=\"1.0\"?><A><?inside ok?></A>" :
        variant == 3 ? "<!--unterminated<A/>" :
        variant == 4 ? "<A><!--bad--comment--></A>" :
        variant == 5 ? "<!DOCTYPE A><A/>" :
        variant == 6 ? "<A><![CDATA[text]]></A>" :
        variant == 7 ? "<?before?><A/><?after?>" :
            "<A>" + std::string(128U, ' ') + "<!--ok--></A>";
    const bool result = parseXml(source, root, error);
    const bool expected = variant == 1 || variant == 2 || variant == 7 || variant == 8;
    context.expect(result == expected && (result || !error.empty()),
        "CF-COMPILATION-026 comments and processing instructions are supported; other declarations are rejected");
}

void xmlFirstErrorContract(TestContext& context, std::size_t variant)
{
    SimpleXmlElement root;
    std::string error;
    const std::string source = variant == 1 ? "<A></B>" :
        variant == 2 ? "\n\n<A>\n <B></C>\n</A>" :
        variant == 3 ? "<A x=\"1\" x=\"2\"></B>" :
        variant == 4 ? "<A>&bad;</A> trailing" :
        variant == 5 ? "<A><B>" :
        variant == 6 ? "<A\n x=\"1\"\n y=oops/>" :
        variant == 7 ? "<A></Wrong>" : "<A>" + std::string(8192U, 'x');
    const std::string expectedError = variant == 1 ?
        std::string("XML 1:8: closing element 'B' does not match 'A'") :
        variant == 2 ? "XML 4:9: closing element 'C' does not match 'B'" :
        variant == 3 ? "XML 1:11: duplicate XML attribute 'x'" :
        variant == 4 ? "XML 1:5: unsupported XML entity '&bad;'" :
        variant == 5 ? "XML 1:7: element 'B' is not closed" :
        variant == 6 ? "XML 3:4: expected a quoted XML attribute value" :
        variant == 7 ? "XML 1:12: closing element 'Wrong' does not match 'A'" :
            "XML 1:8196: element 'A' is not closed";
    const bool result = parseXml(source, root, error);
    context.expect(!result && error == expectedError,
        "CF-COMPILATION-027 parser preserves the exact first error and XML line-column location");
}
}

void registerCompilationXml169To216Module(TestRegistry& registry)
{
    addCompilationContract(registry, 22U, "unit", xmlDocumentContract);
    addCompilationContract(registry, 23U, "unit", xmlElementContract);
    addCompilationContract(registry, 24U, "unit", xmlNameAttributeContract);
    addCompilationContract(registry, 25U, "unit", xmlTextEntityContract);
    addCompilationContract(registry, 26U, "unit", xmlMiscellaneousContract);
    addCompilationContract(registry, 27U, "unit", xmlFirstErrorContract);
}
}
