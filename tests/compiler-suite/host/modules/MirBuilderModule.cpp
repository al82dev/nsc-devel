#include "MirContractSupport.h"

#include "nativesharp/MIR/MirBuilder.h"
#include "nativesharp/Sema/Type.h"

#include <array>
#include <string>
#include <string_view>

namespace nsc::suite
{
namespace
{
using namespace mir_contract_support;
using nativesharp::mir::MirBuilder;

constexpr std::array<std::string_view, 8> kSimplePrograms = {
    "int Main() { return 0; }",
    "int G = 3; int Main() { return G; }",
    "void F(); int Main() { return 0; }",
    "int F(int x) { return x; } int Main() { return F(1); }",
    "int Main() { int x = 1; return x; }",
    "long F() { long x = 7; return x; } int Main() { return (int)F(); }",
    "int A() { return 1; } int B() { return 2; } int Main() { return A() + B(); }",
    "int Main() { int x = 0; x = x + 1; return x; }",
};

std::unique_ptr<FrontendCompilation> compileVariant(
    TestContext& context, std::string_view label, const std::array<std::string_view, 8>& programs,
    std::size_t variant)
{
    return compileMir(context, label, std::string(programs[variant]));
}

void rootOracle(TestContext& context, std::size_t variant)
{
    if (variant == 2 || variant == 3)
    {
        MirArena arena;
        MirBuilder builder(arena);
        context.expect(builder.build(nullptr) == nullptr, "MIR builder must reject a missing HIR root");
        context.expect(!builder.succeeded() && !builder.error().empty(),
            "missing HIR root must retain the first builder diagnostic");
        return;
    }
    auto compilation = compileVariant(context, "mir-root", kSimplePrograms, variant);
    context.expect(compilation->hirRoot() != nullptr, "source must produce the HIR root consumed by MIR");
    context.expect(compilation->mirModule() != nullptr, "valid CompilationUnit HIR must produce one MIR module");
}

void functionOracle(TestContext& context, std::size_t variant)
{
    constexpr std::array<std::string_view, 8> programs = {
        "void F(); int Main() { return 0; }",
        "int F(int x) { return x; } int Main() { return F(1); }",
        "int F(int x); int Main() { return 0; }",
        "int F(int x); int F(int value) { return value; } int Main() { return F(2); }",
        "void F() {} int Main() { F(); return 0; }",
        "long F(long a, long b) { return a + b; } int Main() { return 0; }",
        "int A() { return 1; } int B() { return 2; } int Main() { return A() + B(); }",
        "int F(int x) { int y = x + 1; return y; } int Main() { return F(3); }",
    };
    auto compilation = compileVariant(context, "mir-function", programs, variant);
    const MirModule* module = compilation->mirModule();
    const MirFunction* mainFunction = findFunction(module, "Main");
    context.expect(mainFunction != nullptr && mainFunction->defined(), "Main must lower as a definition");
    context.expect(mainFunction != nullptr && mainFunction->entryBlock() != nullptr,
        "defined function must publish its entry block");
    if (variant == 3)
    {
        const MirFunction* function = findFunction(module, "F");
        context.expect(function != nullptr && function->defined() && function->parameters().size() == 1,
            "prototype and definition must coalesce with complete parameters");
    }
}

void emissionOracle(TestContext& context, std::size_t variant)
{
    constexpr std::array<std::string_view, 8> programs = {
        "int Main() { return 1; }",
        "int Main() { int x = 1; return x + 2; }",
        "int Main() { return 1; int x = 2; }",
        "void F() { return; int x = 2; } int Main() { F(); return 0; }",
        "int Main() { int x; x = 4; return x; }",
        "int Main() { int x = 1; x++; return x; }",
        "int Main() { int x = 1; { int y = 2; x += y; } return x; }",
        "int Main() { int x = 0; x = x + 1; x = x + 2; return x; }",
    };
    auto compilation = compileVariant(context, "mir-emission", programs, variant);
    const MirFunction* function = findFunction(compilation->mirModule(), variant == 3 ? "F" : "Main");
    context.expect(function != nullptr && !function->blocks().empty(), "emission must retain ordered blocks");
    if (variant == 2 || variant == 3)
    {
        const MirBlock* detached = findBlock(function, "unreachable.code");
        context.expect(detached != nullptr && detached->terminated(),
            "post-terminator source must move to an explicitly terminated detached block");
    }
}

void ifOracle(TestContext& context, std::size_t variant)
{
    constexpr std::array<std::string_view, 8> programs = {
        "int Main() { if (true) return 1; return 0; }",
        "int Main() { int x = 0; if (x == 0) x = 1; else x = 2; return x; }",
        "int Main() { if (false) { return 1; } return 2; }",
        "int Main() { if (true) { return 1; } else { return 2; } }",
        "int Main() { int x = 0; if (true) {} return x; }",
        "int Main() { if (true) { if (false) return 1; } return 0; }",
        "int Main() { bool a = true; if (a) return 1; return 0; }",
        "int Main() { int x = 0; if (x == 0) x = 1; if (x == 1) x = 2; return x; }",
    };
    auto compilation = compileVariant(context, "mir-if", programs, variant);
    const MirFunction* function = findFunction(compilation->mirModule(), "Main");
    context.expect(countOpcode(function, MirOpcode::Branch) >= 1, "if lowering must emit a branch");
    context.expect(findBlock(function, "if.then") != nullptr, "if lowering must create a then block");
    if (variant == 1 || variant == 3)
    {
        context.expect(findBlock(function, "if.else") != nullptr, "else syntax must create an else block");
    }
}

void loopOracle(TestContext& context, std::size_t variant)
{
    constexpr std::array<std::string_view, 8> programs = {
        "int Main() { int i = 0; while (i < 1) { i++; } return i; }",
        "int Main() { int i = 0; for (i = 0; i < 2; i++) {} return i; }",
        "int Main() { int i = 0; while (true) { break; } return i; }",
        "int Main() { int i = 0; while (i < 2) { i++; continue; } return i; }",
        "int Main() { for (;;) { break; } return 0; }",
        "int Main() { int i = 0; for (; i < 2;) { i++; continue; } return i; }",
        "int Main() { int i = 0; while (i < 2) { for (;;) { break; } i++; } return i; }",
        "int Main() { int i = 0; while (i < 8) { i++; if (i == 4) continue; } return i; }",
    };
    auto compilation = compileVariant(context, "mir-loop", programs, variant);
    const MirFunction* function = findFunction(compilation->mirModule(), "Main");
    context.expect(countOpcode(function, MirOpcode::Jump) >= 1, "loop lowering must emit explicit jump edges");
    if (variant == 0 || variant == 2 || variant == 3 || variant == 6 || variant == 7)
    {
        context.expect(findBlock(function, "while.cond") != nullptr, "while lowering must expose its condition block");
    }
}

void statementOracle(TestContext& context, std::size_t variant)
{
    constexpr std::array<std::string_view, 8> programs = {
        "void F() { return; } int Main() { F(); return 0; }",
        "int Main() { int x = 1; x = 2; return x; }",
        "int Main() { { int x = 1; } return 0; }",
        "int Main() { int x = 0; if (true) x = 1; return x; }",
        "int Main() { int x; return 0; }",
        "int Main() { int x = 1; x++; x--; return x; }",
        "int Main() { int x = 1; while (x < 2) { x++; } return x; }",
        "int Main() { int x = 0; for (x = 0; x < 3; x++) {} return x; }",
    };
    auto compilation = compileVariant(context, "mir-statement", programs, variant);
    const MirFunction* function = findFunction(compilation->mirModule(), "Main");
    context.expect(function != nullptr && function->entryBlock() != nullptr,
        "statement lowering must retain the containing function and entry block");
    context.expect(function != nullptr && function->blocks().back()->terminated(),
        "lowered function must end every final path explicitly");
}

void expressionOracle(TestContext& context, std::size_t variant)
{
    constexpr std::array<std::string_view, 8> programs = {
        "int Main() { return 1; }",
        "int Main() { int x = 2; return x; }",
        "int Main() { return -1; }",
        "int Main() { return 1 + 2 * 3; }",
        "int F(int x) { return x; } int Main() { return F(4); }",
        "int Main() { int x = 1; int* p = &x; return *p; }",
        "int Main() { int[2] a; a[0] = 5; return a[0]; }",
        "int Main() { int x = 1; return (long)x == 1 ? 1 : 0; }",
    };
    auto compilation = compileVariant(context, "mir-expression", programs, variant);
    const MirFunction* function = findFunction(compilation->mirModule(), "Main");
    context.expect(function != nullptr && countOpcode(function, MirOpcode::Return) >= 1,
        "typed expression graph must feed an explicit return");
}

void shortCircuitOracle(TestContext& context, std::size_t variant)
{
    constexpr std::array<std::string_view, 8> programs = {
        "int Main() { bool x = true && false; return 0; }",
        "int Main() { bool x = true || false; return 0; }",
        "int Main() { bool a = true; bool x = a && a; return 0; }",
        "int Main() { bool a = false; bool x = a || !a; return 0; }",
        "int Main() { bool x = (true && false) || true; return 0; }",
        "int Main() { bool a = true; if (a && true) return 1; return 0; }",
        "int Main() { bool a = false; if (a || true) return 1; return 0; }",
        "int Main() { bool a = true; bool b = false; bool c = a && (b || a); return 0; }",
    };
    auto compilation = compileVariant(context, "mir-short-circuit", programs, variant);
    const MirFunction* function = findFunction(compilation->mirModule(), "Main");
    context.expect(findBlock(function, "logic.rhs") != nullptr, "short circuit must create a conditional RHS block");
    context.expect(findBlock(function, "logic.short") != nullptr, "short circuit must create a short path block");
    context.expect(findBlock(function, "logic.end") != nullptr, "short circuit must create a merge block");
    context.expect(countOpcode(function, MirOpcode::Branch) >= 1, "short circuit must use CFG branching");
}

void compoundOracle(TestContext& context, std::size_t variant)
{
    constexpr std::array<std::string_view, 8> programs = {
        "int Main() { int x = 1; x += 2; return x; }",
        "int Main() { int x = 1; x -= 2; return x; }",
        "int Main() { int x = 1; int y = x++; return y; }",
        "int Main() { int x = 1; int y = ++x; return y; }",
        "int Main() { int[4] a; int i = 0; a[i++] += 2; return i; }",
        "int Main() { int x = 0; int* p = &x; p++; return 0; }",
        "int Main() { int x = 0; int* p = &x; ++p; return 0; }",
        "int Main() { int x = 1; x *= 2; x /= 2; return x; }",
    };
    auto compilation = compileVariant(context, "mir-compound", programs, variant);
    const MirFunction* function = findFunction(compilation->mirModule(), "Main");
    context.expect(function != nullptr && countOpcode(function, MirOpcode::StoreSymbol) >= 1,
        "compound and increment lowering must store their updated value");
    if (variant == 5 || variant == 6)
    {
        const MirInstruction* pointerAdd = findInstruction(function, MirOpcode::PointerAdd);
        context.expect(pointerAdd != nullptr && pointerAdd->hasImmediate() && pointerAdd->immediate() == 1,
            "pointer increment must carry canonical immediate element step");
    }
}

void addressOracle(TestContext& context, std::size_t variant)
{
    constexpr std::array<std::string_view, 8> programs = {
        "int Main() { int x = 1; int* p = &x; return *p; }",
        "int Main() { int[4] a; a[1] = 2; return a[1]; }",
        "struct P { int x; } int Main() { P p; p.x = 2; return p.x; }",
        "int Main() { int x = 1; int* p = &x; *p = 3; return x; }",
        "int Main() { int[1] a; return a[0]; }",
        "int Main() { int x = 1; int* p = &x; int* q = p + 0; return *q; }",
        "int Main() { int[3] a; int i = 2; return a[i]; }",
        "int Main() { int[8] a; int i = 0; a[i] = i; return a[i]; }",
    };
    auto compilation = compileVariant(context, "mir-address", programs, variant);
    const MirFunction* function = findFunction(compilation->mirModule(), "Main");
    const bool hasAddress = findInstruction(function, MirOpcode::AddressOfSymbol) != nullptr ||
        findInstruction(function, MirOpcode::IndexAddress) != nullptr ||
        findInstruction(function, MirOpcode::FieldAddress) != nullptr;
    context.expect(hasAddress, "address lowering must materialize the address form required by the source");
    if (variant == 1 || variant == 4 || variant == 6 || variant == 7)
    {
        context.expect(countOpcode(function, MirOpcode::BoundsCheck) >= 1,
            "fixed-array indexing must emit bounds checks for its access expressions");
    }
}

void primitiveOracle(TestContext& context, std::size_t variant)
{
    constexpr std::array<std::string_view, 8> programs = {
        "int Main() { return 42; }",
        "long F(int x) { return (long)x; } int Main() { return (int)F(1); }",
        "int Main() { return -1; }",
        "int Main() { return ~1; }",
        "int Main() { return 1 + 2; }",
        "int Main() { return 4 << 1; }",
        "int Main() { return 1 < 2 ? 1 : 0; }",
        "int Main() { int x = 1; return ((x * 8) / 2) % 3; }",
    };
    auto compilation = compileVariant(context, "mir-primitive", programs, variant);
    const MirFunction* function = findFunction(compilation->mirModule(), "Main");
    context.expect(function != nullptr && function->entryBlock() != nullptr,
        "primitive lowering must retain a complete function");
    context.expect(countOpcode(function, MirOpcode::Return) >= 1, "primitive result must reach an explicit return");
}

void blockTopologyOracle(TestContext& context, std::size_t variant)
{
    constexpr std::array<std::string_view, 8> programs = {
        "int Main() { if (true) return 1; return 0; }",
        "int Main() { int i = 0; while (i < 1) { i++; } return i; }",
        "int Main() { for (;;) { break; } return 0; }",
        "int Main() { bool x = true && false; return 0; }",
        "int Main() { return 1; int x = 2; }",
        "int Main() { int x = 0; if (x == 0) x = 1; else x = 2; return x; }",
        "int Main() { int i = 0; for (i = 0; i < 2; i++) {} return i; }",
        "int Main() { int i = 0; while (i < 4) { if (i == 2) break; i++; } return i; }",
    };
    auto compilation = compileVariant(context, "mir-topology", programs, variant);
    const MirFunction* function = findFunction(compilation->mirModule(), "Main");
    if (function == nullptr || function->blocks().empty())
    {
        context.expect(false, "CFG lowering must publish ordered blocks");
        return;
    }
    for (const MirBlock* block : function->blocks())
    {
        context.expect(block != nullptr && block->terminated(), "every emitted MIR block must terminate explicitly");
    }
}

void mappingOracle(TestContext& context, std::size_t variant)
{
    constexpr std::array<std::string_view, 8> programs = {
        "int Main() { return -1; }",
        "int Main() { return ~1; }",
        "int Main() { return !false ? 1 : 0; }",
        "int Main() { return 1 + 2 * 3; }",
        "int Main() { return (8 >> 1) ^ 3; }",
        "int Main() { return 1 <= 2 ? 1 : 0; }",
        "int Main() { int x = 0; int* p = &x; p = p + 1; return 0; }",
        "int Main() { int x = 1; int y = 2; return (x != y) ? x - y : x / y; }",
    };
    auto compilation = compileVariant(context, "mir-opcode-map", programs, variant);
    const MirFunction* function = findFunction(compilation->mirModule(), "Main");
    context.expect(function != nullptr && countOpcode(function, MirOpcode::Return) == 1,
        "operator mapping must produce a complete expression and one return terminator");
}
}

void registerMirBuilderModule(TestRegistry& registry)
{
    addMatrix(registry, 13, "component", rootOracle);
    addMatrix(registry, 14, "component", functionOracle);
    addMatrix(registry, 15, "component", emissionOracle);
    addMatrix(registry, 16, "component", ifOracle);
    addMatrix(registry, 17, "component", loopOracle);
    addMatrix(registry, 18, "component", statementOracle);
    addMatrix(registry, 19, "component", expressionOracle);
    addMatrix(registry, 20, "component", shortCircuitOracle);
    addMatrix(registry, 21, "component", compoundOracle);
    addMatrix(registry, 22, "component", addressOracle);
    addMatrix(registry, 23, "component", primitiveOracle);
    addMatrix(registry, 24, "component", blockTopologyOracle);
    addMatrix(registry, 25, "component", mappingOracle);
}
}
