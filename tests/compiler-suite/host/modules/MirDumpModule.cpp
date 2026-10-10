#include "MirContractSupport.h"

#include "nativesharp/Sema/ConstantValue.h"
#include "nativesharp/Sema/ConversionKind.h"

#include <array>
#include <string>
#include <string_view>

namespace nsc::suite
{
namespace
{
using namespace mir_contract_support;
using nativesharp::sema::ConstantValue;
using nativesharp::sema::ConversionKind;

void constantDumpOracle(TestContext& context, std::size_t variant)
{
    TypeContext types;
    Type* signature = types.functionType(types.builtin(TypeKind::Void), {});
    Symbol functionSymbol(SymbolKind::Function, "F", signature, nullptr);
    MirArena arena;
    MirModule* module = arena.makeModule();
    MirFunction* function = arena.makeFunction(&functionSymbol);
    function->setDefined(true);
    module->addFunction(function);
    MirBlock* entry = arena.makeBlock("entry");
    function->addBlock(entry);
    MirInstruction* constant = arena.makeInstruction(MirOpcode::Constant, SourceRange());
    static const std::array<ConstantValue, 8> values = {
        ConstantValue::signedInteger(-7),
        ConstantValue::unsignedInteger(9),
        ConstantValue::floating(1.25),
        ConstantValue::boolean(true),
        ConstantValue::nullValue(),
        ConstantValue::stringValue("a\\\"\n\r\t"),
        ConstantValue::stringValue(std::string("x\0y", 3)),
        ConstantValue::stringValue(std::string("\x01\x7f", 2)),
    };
    constant->setResult(arena.makeValue(types.builtin(TypeKind::Int)));
    constant->setConstant(values[variant]);
    entry->addInstruction(constant);
    MirInstruction* ret = arena.makeInstruction(MirOpcode::Return, SourceRange());
    entry->addInstruction(ret);
    const std::string first = dumpModule(module);
    const std::string second = dumpModule(module);
    context.expect(first == second, "constant dump must be byte deterministic");
    context.expect(first.find("const") != std::string::npos, "constant dump must identify the opcode");
    if (variant == 5)
    {
        context.expect(first.find("\\\\") != std::string::npos && first.find("\\\"") != std::string::npos &&
                first.find("\\n") != std::string::npos && first.find("\\r") != std::string::npos &&
                first.find("\\t") != std::string::npos,
            "byte string dump must escape slash quote and control characters");
    }
    if (variant == 6) context.expect(first.find("\\0") != std::string::npos, "NUL byte must use the stable escape");
    if (variant == 7) context.expect(first.find("\\x01") != std::string::npos, "nonprintable byte must use hex");
}

void moduleDumpOracle(TestContext& context, std::size_t variant)
{
    constexpr std::array<std::string_view, 8> programs = {
        "int Main() { return 0; }",
        "int G = 3; int Main() { return G; }",
        "void F(); int Main() { return 0; }",
        "int F(int x) { return x; } int Main() { return F(1); }",
        "int Main() { int x = 1; return x; }",
        "int Main() { int x = 1; { int x = 2; x = 3; } return x; }",
        "int B() { return 2; } int A() { return 1; } int Main() { return A() + B(); }",
        "int G = 1; int H = 2; int Main() { return G + H; }",
    };
    auto compilation = compileMir(context, "mir-module-dump", std::string(programs[variant]));
    const std::string first = dumpModule(compilation->mirModule());
    const std::string second = dumpModule(compilation->mirModule());
    context.expect(first == second, "module dump must be byte deterministic across repetition");
    context.expect(first.starts_with("mir.module\n"), "module dump must publish its stable header");
    context.expect(first.find("function @Main") != std::string::npos, "module dump must include Main");
    if (variant == 1 || variant == 7)
    {
        context.expect(first.find("global @G : int =") != std::string::npos,
            "module dump must retain ordered global declarations and initializer text");
    }
    if (variant == 5)
    {
        context.expect(first.find("@x#0:int") != std::string::npos &&
                first.find("@x#1:int") != std::string::npos,
            "local dump must disambiguate shadowed symbols by stable ordinal");
    }
}

void instructionDumpOracle(TestContext& context, std::size_t variant)
{
    TypeContext types;
    Type* signature = types.functionType(types.builtin(TypeKind::Void), {});
    Symbol functionSymbol(SymbolKind::Function, "F", signature, nullptr);
    Symbol local(SymbolKind::Variable, "x", types.builtin(TypeKind::Int), nullptr);
    MirArena arena;
    MirModule* module = arena.makeModule();
    MirFunction* function = arena.makeFunction(&functionSymbol);
    function->setDefined(true);
    function->addLocal(&local);
    module->addFunction(function);
    MirBlock* entry = arena.makeBlock("entry");
    MirBlock* target = arena.makeBlock("target");
    function->addBlock(entry);
    function->addBlock(target);
    MirInstruction* instruction = arena.makeInstruction(MirOpcode::Convert, SourceRange());
    MirValue* input = arena.makeValue(types.builtin(TypeKind::Int));
    MirValue* result = arena.makeValue(types.builtin(TypeKind::Long));
    instruction->setResult(result);
    instruction->addOperand(input);
    instruction->setSymbol(&local);
    instruction->setAuxiliaryType(types.builtin(TypeKind::Long));
    instruction->setConstant(ConstantValue::signedInteger(variant));
    instruction->setConversionKind(ConversionKind::ExplicitNumeric);
    instruction->setImmediate(variant + 1);
    instruction->setOffset(variant + 2);
    instruction->setFirstTarget(target);
    instruction->setSecondTarget(entry);
    entry->addInstruction(instruction);
    entry->addInstruction(arena.makeInstruction(MirOpcode::Return, SourceRange()));
    target->addInstruction(arena.makeInstruction(MirOpcode::Return, SourceRange()));
    const std::string text = dumpModule(module);
    context.expect(text.find("%v1:long = convert") != std::string::npos,
        "instruction dump must print result identity type and opcode");
    context.expect(text.find("@x#0") != std::string::npos && text.find("<long>") != std::string::npos,
        "instruction dump must print symbol and auxiliary type payloads");
    context.expect(text.find("imm=") != std::string::npos && text.find("offset=") != std::string::npos,
        "instruction dump must print immediate and offset presence");
    context.expect(text.find("-> bb1, bb0") != std::string::npos,
        "instruction dump must print both block targets in stable order");
}
}

void registerMirDumpModule(TestRegistry& registry)
{
    addMatrix(registry, 26, "unit", constantDumpOracle);
    addMatrix(registry, 27, "unit", moduleDumpOracle);
    addMatrix(registry, 28, "unit", instructionDumpOracle);
}
}
