#include "SsaContractSupport.h"

#include "nativesharp/SSA/SsaDumper.h"

#include <array>
#include <sstream>
#include <string_view>

namespace nsc::suite
{
namespace
{
using ssa_contract_support::addMatrix;
using ssa_contract_support::compileSsa;
using nativesharp::ssa::SsaDumper;

std::string dump(const nativesharp::ssa::SsaModule* module)
{
    std::ostringstream output;
    SsaDumper().dump(module, output);
    return output.str();
}

void moduleDumpOracle(TestContext& context, std::size_t variant)
{
    constexpr std::array<std::string_view, 8> sources{"", "int G;", "int G=42;", "int F();",
        "extern C int F(int x);", "int A;int B=2;int F();", "int G;int F(){return G;}",
        "int A;int B;int C;int D;int E;int F;int G;int H;"};
    auto compilation = compileSsa(context, "CF-SSA-031", std::string(sources[variant]));
    const std::string first = dump(compilation->ssaModule());
    const std::string second = dump(compilation->ssaModule());
    context.expect(first == second, "CF-SSA-031 module dump must be byte-deterministic");
    context.expect(first.starts_with("ssa.module\n"), "CF-SSA-031 valid module must begin with exact header");
    if (variant == 1) context.expect(first.find("global @G : int = zeroinit\n") != std::string::npos,
        "CF-SSA-031 implicit global initializer must dump as zeroinit");
    if (variant == 2) context.expect(first.find("global @G : int = 42\n") != std::string::npos,
        "CF-SSA-031 explicit global initializer must preserve constant text");
    if (variant == 4) context.expect(first.find("extern-C prototype\n") != std::string::npos,
        "CF-SSA-031 extern C prototype marker must be byte-exact");
}

void functionDumpOracle(TestContext& context, std::size_t variant)
{
    constexpr std::array<std::string_view, 8> sources{"void F(){}", "int F(){return 1;}",
        "int F(int x){return x;}", "int F(int x){int y=x;return y;}",
        "int F(int x){if(x==0){return 1;}return 2;}",
        "int F(){int x=0;while(x<2){x=x+1;}return x;}",
        "int F(){int addressed=1;int* p=&addressed;return *p;}",
        "int F(){int a=1;int b=2;int c=3;int d=4;return a+b+c+d;}"};
    auto compilation = compileSsa(context, "CF-SSA-032", std::string(sources[variant]));
    const std::string text = dump(compilation->ssaModule());
    context.expect(text.find("function @F : ") != std::string::npos,
        "CF-SSA-032 function header must retain symbol and type");
    context.expect(text.find("  promoted:") != std::string::npos,
        "CF-SSA-032 function dump must expose the ordered promoted set");
    context.expect(text.find("  excluded-unreachable: ") != std::string::npos,
        "CF-SSA-032 function dump must expose excluded block accounting");
    context.expect(text.find("bb") != std::string::npos,
        "CF-SSA-032 function dump must retain block order and labels");
}

void instructionDumpOracle(TestContext& context, std::size_t variant)
{
    constexpr std::array<std::string_view, 8> sources{"int F(){return -7;}",
        "uint F(){return 9u;}", "double F(){return 1.25;}",
        "bool F(){return true;}", "int* F(){return null;}",
        "extern C int puts(char* p);int F(){return puts(\"a\\n\\t\\0b\");}",
        "int F(int x){return x+1;}",
        "int F(int x){int y=0;if(x==0){y=1;}else{y=2;}return y;}"};
    auto compilation = compileSsa(context, "CF-SSA-033", std::string(sources[variant]));
    const std::string first = dump(compilation->ssaModule());
    const std::string second = dump(compilation->ssaModule());
    context.expect(first == second, "CF-SSA-033 instruction dump must be byte-deterministic");
    context.expect(first.find("%v") != std::string::npos,
        "CF-SSA-033 typed values must use the stable percent-value spelling");
    if (variant == 5)
    {
        context.expect(first.find("\\n") != std::string::npos && first.find("\\t") != std::string::npos &&
                first.find("\\0") != std::string::npos,
            "CF-SSA-033 string constants must escape control bytes exactly");
    }
    if (variant == 7)
    {
        context.expect(first.find("phi") != std::string::npos && first.find("[bb") != std::string::npos,
            "CF-SSA-033 PHI dump must expose predecessor-value pairs");
    }
}
}

// Registers CF-SSA-031..033 module, function, and instruction dump matrices.
void registerSsaDumpModule(TestRegistry& registry)
{
    addMatrix(registry, 31, "unit", moduleDumpOracle);
    addMatrix(registry, 32, "unit", functionDumpOracle);
    addMatrix(registry, 33, "unit", instructionDumpOracle);
}
}
