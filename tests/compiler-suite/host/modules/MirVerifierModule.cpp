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
using nativesharp::mir::MirGlobal;
using nativesharp::mir::MirVerificationMode;
using nativesharp::mir::MirVerifier;
using nativesharp::sema::ConstantValue;
using nativesharp::sema::ConversionKind;

void expectRejected(
    TestContext& context, MirVerifier& verifier, const MirModule* module,
    MirVerificationMode mode, std::string_view expected)
{
    context.expect(!verifier.verify(module, mode), "corrupt MIR must be rejected");
    context.expect(verifier.error().find(expected) != std::string::npos,
        "MIR verifier must retain the diagnostic for the violated contract");
}

void expectCompiledValid(TestContext& context, std::string_view label, std::string source)
{
    auto compilation = compileMir(context, label, std::move(source));
    MirVerifier verifier;
    context.expect(verifier.verify(compilation->mirModule(), MirVerificationMode::PreSsa),
        std::string(label) + " must satisfy the PreSsa MIR verifier");
    context.expect(verifier.error().empty(), std::string(label) + " must leave no verifier diagnostic");
}

MirValue* addIntegerConstant(FunctionFixture& fixture, std::int64_t value, TypeKind kind = TypeKind::Int)
{
    MirValue* result = fixture.arena.makeValue(fixture.types.builtin(kind));
    MirInstruction* instruction = fixture.arena.makeInstruction(MirOpcode::Constant, SourceRange());
    instruction->setResult(result);
    instruction->setConstant(ConstantValue::signedInteger(value));
    fixture.entry->addInstruction(instruction);
    return result;
}

void moduleVerifierOracle(TestContext& context, std::size_t variant)
{
    MirVerifier verifier;
    if (variant == 0)
    {
        MirArena arena;
        context.expect(verifier.verify(arena.makeModule()), "empty MIR module is structurally valid");
        return;
    }
    if (variant == 1 || variant == 7)
    {
        FunctionFixture fixture;
        fixture.addReturn();
        context.expect(verifier.verify(fixture.module), "minimal defined function must verify");
        if (variant == 7) context.expect(verifier.verify(fixture.module), "repeated verification must be deterministic");
        return;
    }
    if (variant == 2)
    {
        expectRejected(context, verifier, nullptr, MirVerificationMode::PreSsa, "module is null");
        return;
    }

    TypeContext types;
    MirArena arena;
    MirModule* module = arena.makeModule();
    if (variant == 3)
    {
        Symbol wrong(SymbolKind::Variable, "G", types.builtin(TypeKind::Int), nullptr);
        module->addGlobal(arena.makeGlobal(&wrong));
        expectRejected(context, verifier, module, MirVerificationMode::PreSsa, "global-variable symbol");
    }
    else if (variant == 4)
    {
        Symbol global(SymbolKind::GlobalVariable, "G", types.builtin(TypeKind::Int), nullptr);
        MirGlobal* first = arena.makeGlobal(&global);
        MirGlobal* second = arena.makeGlobal(&global);
        module->addGlobal(first);
        module->addGlobal(second);
        expectRejected(context, verifier, module, MirVerificationMode::PreSsa, "duplicate global/function symbol");
    }
    else if (variant == 5)
    {
        Symbol global(SymbolKind::GlobalVariable, "G", types.builtin(TypeKind::Int), nullptr);
        MirGlobal* value = arena.makeGlobal(&global);
        value->setExplicitInitializer(ConstantValue::signedInteger(1));
        module->addGlobal(value);
        expectRejected(context, verifier, module, MirVerificationMode::PreSsa, "semantic constant value");
    }
    else
    {
        Type* signature = types.functionType(types.builtin(TypeKind::Void), {});
        Symbol functionSymbol(SymbolKind::Function, "F", signature, nullptr);
        module->addFunction(arena.makeFunction(&functionSymbol));
        module->addFunction(arena.makeFunction(&functionSymbol));
        expectRejected(context, verifier, module, MirVerificationMode::PreSsa, "duplicate function symbol");
    }
}

void functionVerifierOracle(TestContext& context, std::size_t variant)
{
    MirVerifier verifier;
    if (variant == 0)
    {
        FunctionFixture fixture(TypeKind::Void, false);
        context.expect(verifier.verify(fixture.module), "empty prototype must verify");
        return;
    }
    if (variant == 1)
    {
        FunctionFixture fixture;
        fixture.addReturn();
        context.expect(verifier.verify(fixture.module), "defined void function must verify");
        return;
    }
    if (variant == 7)
    {
        expectCompiledValid(context, "mir-function-verifier",
            "int F(int x) { return x; } int Main() { return F(1); }");
        return;
    }
    if (variant == 2)
    {
        TypeContext types;
        Symbol wrong(SymbolKind::Variable, "F", types.builtin(TypeKind::Int), nullptr);
        MirArena arena;
        MirModule* module = arena.makeModule();
        module->addFunction(arena.makeFunction(&wrong));
        expectRejected(context, verifier, module, MirVerificationMode::PreSsa, "semantic function symbol");
        return;
    }
    FunctionFixture fixture;
    if (variant == 3)
    {
        fixture.function->setDefined(false);
        expectRejected(context, verifier, fixture.module, MirVerificationMode::PreSsa, "prototype cannot contain");
    }
    else if (variant == 4)
    {
        fixture.function->replaceBlocks({});
        expectRejected(context, verifier, fixture.module, MirVerificationMode::PreSsa, "entry block");
    }
    else if (variant == 5)
    {
        Symbol local(SymbolKind::Variable, "x", fixture.types.builtin(TypeKind::Int), nullptr);
        fixture.function->addLocal(&local);
        fixture.function->addLocal(&local);
        fixture.addReturn();
        expectRejected(context, verifier, fixture.module, MirVerificationMode::PreSsa, "locals must be unique");
    }
    else
    {
        fixture.function->addBlock(fixture.entry);
        fixture.addReturn();
        expectRejected(context, verifier, fixture.module, MirVerificationMode::PreSsa, "duplicate block pointer");
    }
}

void blockVerifierOracle(TestContext& context, std::size_t variant)
{
    MirVerifier verifier;
    FunctionFixture fixture;
    if (variant == 0)
    {
        fixture.addReturn();
        context.expect(verifier.verify(fixture.module), "return-only block must verify");
    }
    else if (variant == 1)
    {
        fixture.entry->addInstruction(fixture.arena.makeInstruction(MirOpcode::Unreachable, SourceRange()));
        context.expect(verifier.verify(fixture.module), "unreachable-only block must verify");
    }
    else if (variant == 2)
    {
        expectRejected(context, verifier, fixture.module, MirVerificationMode::PreSsa, "cannot be empty");
    }
    else if (variant == 3)
    {
        fixture.entry->addInstruction(nullptr);
        expectRejected(context, verifier, fixture.module, MirVerificationMode::PreSsa, "null instruction");
    }
    else if (variant == 4)
    {
        fixture.addReturn();
        fixture.entry->addInstruction(fixture.arena.makeInstruction(MirOpcode::Return, SourceRange()));
        expectRejected(context, verifier, fixture.module, MirVerificationMode::PreSsa, "final instruction");
    }
    else if (variant == 5)
    {
        MirInstruction* value = fixture.arena.makeInstruction(MirOpcode::Constant, SourceRange());
        value->setResult(fixture.arena.makeValue(fixture.types.builtin(TypeKind::Int)));
        value->setConstant(ConstantValue::signedInteger(1));
        fixture.entry->addInstruction(value);
        expectRejected(context, verifier, fixture.module, MirVerificationMode::PreSsa, "exactly one terminator");
    }
    else
    {
        expectCompiledValid(context, "mir-block-verifier",
            variant == 6 ? "int Main() { if (true) return 1; return 0; }" :
                           "int Main() { int i = 0; while (i < 4) i++; return i; }");
    }
}

void ssaInstructionOracle(TestContext& context, std::size_t variant)
{
    MirVerifier verifier;
    FunctionFixture fixture;
    Type* integer = fixture.types.builtin(TypeKind::Int);
    if (variant == 0 || variant == 7)
    {
        const std::size_t count = variant == 7 ? 32 : 1;
        for (std::size_t index = 0; index < count; ++index)
        {
            MirInstruction* undef = fixture.arena.makeInstruction(MirOpcode::Undef, SourceRange());
            undef->setResult(fixture.arena.makeValue(integer));
            fixture.entry->addInstruction(undef);
        }
        fixture.addReturn();
        context.expect(verifier.verify(fixture.module, MirVerificationMode::Ssa),
            "typed SSA undef prefix must verify in SSA mode");
    }
    else if (variant == 1)
    {
        MirValue* source = fixture.arena.makeValue(integer);
        MirInstruction* undef = fixture.arena.makeInstruction(MirOpcode::Undef, SourceRange());
        undef->setResult(source);
        fixture.entry->addInstruction(undef);
        MirInstruction* phi = fixture.arena.makeInstruction(MirOpcode::Phi, SourceRange());
        phi->setResult(fixture.arena.makeValue(integer));
        phi->addPhiIncoming(fixture.entry, source);
        fixture.entry->replaceInstructions({phi, undef, fixture.arena.makeInstruction(MirOpcode::Return, SourceRange())});
        context.expect(verifier.verify(fixture.module, MirVerificationMode::Ssa),
            "PHI prefix with typed in-function incoming must verify");
    }
    else if (variant == 2)
    {
        MirInstruction* undef = fixture.arena.makeInstruction(MirOpcode::Undef, SourceRange());
        undef->setResult(fixture.arena.makeValue(integer));
        fixture.entry->addInstruction(undef);
        fixture.addReturn();
        expectRejected(context, verifier, fixture.module, MirVerificationMode::PreSsa, "pre-SSA MIR");
    }
    else if (variant == 3)
    {
        MirInstruction* phi = fixture.arena.makeInstruction(MirOpcode::Phi, SourceRange());
        phi->setResult(fixture.arena.makeValue(integer));
        fixture.entry->addInstruction(phi);
        fixture.addReturn();
        expectRejected(context, verifier, fixture.module, MirVerificationMode::Ssa, "one or more incoming");
    }
    else
    {
        expectCompiledValid(context, "mir-ssa-form", variant == 4 ?
            "int F(int x) { return x; } int Main() { return F(1); }" :
            "int Main() { int x = 1; return x; }");
    }
}

void storageVerifierOracle(TestContext& context, std::size_t variant)
{
    if (variant != 2 && variant != 3)
    {
        static const std::array<std::string_view, 8> programs = {
            "int Main() { return 1; }",
            "int Main() { int x = 1; return x; }",
            "", "",
            "int G = 1; int Main() { return G; }",
            "int Main() { int x = 1; int* p = &x; return *p; }",
            "int Main() { int x = 1; x = 2; return x; }",
            "int Main() { int x = 0; int* p = &x; *p = 4; return x; }",
        };
        expectCompiledValid(context, "mir-storage-verifier", std::string(programs[variant]));
        return;
    }
    FunctionFixture fixture;
    MirVerifier verifier;
    if (variant == 2)
    {
        MirInstruction* constant = fixture.arena.makeInstruction(MirOpcode::Constant, SourceRange());
        constant->setResult(fixture.arena.makeValue(fixture.types.builtin(TypeKind::Bool)));
        constant->setConstant(ConstantValue::signedInteger(1));
        fixture.entry->addInstruction(constant);
        fixture.addReturn();
        expectRejected(context, verifier, fixture.module, MirVerificationMode::PreSsa, "constant payload");
    }
    else
    {
        Symbol local(SymbolKind::Variable, "x", fixture.types.builtin(TypeKind::Int), nullptr);
        fixture.function->addLocal(&local);
        MirValue* wrong = addIntegerConstant(fixture, 1, TypeKind::Long);
        MirInstruction* store = fixture.arena.makeInstruction(MirOpcode::StoreSymbol, SourceRange());
        store->setSymbol(&local);
        store->addOperand(wrong);
        fixture.entry->addInstruction(store);
        fixture.addReturn();
        expectRejected(context, verifier, fixture.module, MirVerificationMode::PreSsa, "matching in-scope storage type");
    }
}

void arithmeticVerifierOracle(TestContext& context, std::size_t variant)
{
    if (variant != 2 && variant != 3)
    {
        static const std::array<std::string_view, 8> programs = {
            "int Main() { return -1; }",
            "int Main() { return 1 + 2; }",
            "", "",
            "int Main() { return 8 >> 1; }",
            "int Main() { return 1 < 2 ? 1 : 0; }",
            "long Main() { int x = 1; return (long)x; }",
            "int Main() { int x = 1; return ((x + 2) * 3) / 2; }",
        };
        expectCompiledValid(context, "mir-arithmetic-verifier", std::string(programs[variant]));
        return;
    }
    FunctionFixture fixture;
    MirVerifier verifier;
    MirValue* source = addIntegerConstant(fixture, 1);
    MirInstruction* operation = fixture.arena.makeInstruction(
        variant == 2 ? MirOpcode::Add : MirOpcode::Convert, SourceRange());
    operation->setResult(fixture.arena.makeValue(fixture.types.builtin(TypeKind::Int)));
    operation->addOperand(source);
    if (variant == 2)
    {
        operation->addOperand(fixture.arena.makeValue(fixture.types.builtin(TypeKind::Float)));
    }
    else
    {
        operation->setConversionKind(ConversionKind::Identity);
    }
    fixture.entry->addInstruction(operation);
    fixture.addReturn();
    expectRejected(context, verifier, fixture.module, MirVerificationMode::PreSsa,
        variant == 2 ? "arithmetic" : "identity or to-void");
}

void pointerCallOracle(TestContext& context, std::size_t variant)
{
    if (variant != 2 && variant != 3)
    {
        static const std::array<std::string_view, 8> programs = {
            "int Main() { int x = 0; int* p = &x; p = p + 1; return 0; }",
            "int F(int x) { return x; } int Main() { return F(1); }",
            "", "",
            "int Main() { int x = 0; int* p = &x; p++; return 0; }",
            "int Main() { int x = 0; int* a = &x; int* b = &x; return (int)(b - a); }",
            "void F() {} int Main() { F(); return 0; }",
            "int F(int x, int y) { return x + y; } int Main() { return F(1, 2); }",
        };
        expectCompiledValid(context, "mir-pointer-call", std::string(programs[variant]));
        return;
    }
    FunctionFixture fixture;
    MirVerifier verifier;
    Type* pointer = fixture.types.pointerTo(fixture.types.builtin(TypeKind::Int));
    MirValue* base = fixture.arena.makeValue(pointer);
    MirInstruction* seed = fixture.arena.makeInstruction(MirOpcode::Undef, SourceRange());
    seed->setResult(base);
    fixture.entry->addInstruction(seed);
    MirInstruction* add = fixture.arena.makeInstruction(MirOpcode::PointerAdd, SourceRange());
    add->setResult(fixture.arena.makeValue(pointer));
    add->addOperand(base);
    add->setImmediate(1);
    if (variant == 2) add->addOperand(addIntegerConstant(fixture, 1));
    fixture.entry->addInstruction(add);
    fixture.addReturn();
    expectRejected(context, verifier, fixture.module,
        variant == 2 ? MirVerificationMode::Ssa : MirVerificationMode::PreSsa,
        variant == 2 ? "immediate pointer step" : "pre-SSA MIR");
}

void memoryVerifierOracle(TestContext& context, std::size_t variant)
{
    if (variant != 2 && variant != 3)
    {
        static const std::array<std::string_view, 8> programs = {
            "int Main() { int[2] a; return a[0]; }",
            "int Main() { int* p = new int; delete p; return 0; }",
            "", "",
            "int Main() { int* p = new int[4]; delete[] p; return 0; }",
            "struct P { int x; } int Main() { P p; p.x = 1; return p.x; }",
            "int Main() { int[4] a; a[1] = 2; return a[1]; }",
            "int Main() { int[16] a; int i = 0; a[i] = 1; return a[i]; }",
        };
        expectCompiledValid(context, "mir-memory-verifier", std::string(programs[variant]));
        return;
    }
    FunctionFixture fixture;
    MirVerifier verifier;
    MirValue* index = addIntegerConstant(fixture, 0);
    MirInstruction* bounds = fixture.arena.makeInstruction(MirOpcode::BoundsCheck, SourceRange());
    bounds->addOperand(index);
    bounds->setImmediate(variant == 2 ? 0 : 4);
    if (variant == 3) bounds->setResult(fixture.arena.makeValue(fixture.types.builtin(TypeKind::Int)));
    fixture.entry->addInstruction(bounds);
    fixture.addReturn();
    expectRejected(context, verifier, fixture.module, MirVerificationMode::PreSsa, "bounds.check");
}

void terminatorVerifierOracle(TestContext& context, std::size_t variant)
{
    if (variant != 2 && variant != 3)
    {
        static const std::array<std::string_view, 8> programs = {
            "void F() { return; } int Main() { return 0; }",
            "int Main() { if (true) return 1; return 0; }",
            "", "",
            "int Main() { while (true) break; return 0; }",
            "int Main() { return 1; int x = 2; }",
            "int Main() { int x = 0; if (x == 0) x = 1; return x; }",
            "int Main() { int i = 0; while (i < 8) i++; return i; }",
        };
        expectCompiledValid(context, "mir-terminator-verifier", std::string(programs[variant]));
        return;
    }
    FunctionFixture fixture;
    MirVerifier verifier;
    MirBlock* target = fixture.arena.makeBlock("target");
    fixture.function->addBlock(target);
    target->addInstruction(fixture.arena.makeInstruction(MirOpcode::Return, SourceRange()));
    MirInstruction* branch = fixture.arena.makeInstruction(MirOpcode::Branch, SourceRange());
    branch->addOperand(addIntegerConstant(fixture, 1));
    branch->setFirstTarget(target);
    branch->setSecondTarget(target);
    if (variant == 3) branch->setSecondTarget(nullptr);
    fixture.entry->addInstruction(branch);
    expectRejected(context, verifier, fixture.module, MirVerificationMode::PreSsa, "branch requires bool");
}

void helperVerifierOracle(TestContext& context, std::size_t variant)
{
    MirVerifier verifier;
    if (variant == 0 || variant == 2)
    {
        context.expect(!verifier.verify(nullptr), "null module must fail");
        context.expect(verifier.error() == "MIR module is null", "null module must publish exact first diagnostic");
    }
    FunctionFixture valid;
    valid.addReturn();
    context.expect(verifier.verify(valid.module), "verification must reset state before a valid module");
    context.expect(verifier.error().empty(), "successful reuse must clear the previous diagnostic");
    if (variant >= 3)
    {
        FunctionFixture invalid;
        context.expect(!verifier.verify(invalid.module), "unterminated empty block must fail");
        const std::string first = verifier.error();
        context.expect(!first.empty(), "first structural failure must be retained");
        context.expect(verifier.verify(valid.module), "later valid verification must reset latched state");
    }
    if (variant == 7)
    {
        for (std::size_t repetition = 0; repetition < 32; ++repetition)
        {
            context.expect(verifier.verify(valid.module), "repeated valid verification must remain deterministic");
        }
    }
}
}

void registerMirVerifierModule(TestRegistry& registry)
{
    addMatrix(registry, 29, "component", moduleVerifierOracle);
    addMatrix(registry, 30, "component", functionVerifierOracle);
    addMatrix(registry, 31, "component", blockVerifierOracle);
    addMatrix(registry, 32, "component", ssaInstructionOracle);
    addMatrix(registry, 33, "component", storageVerifierOracle);
    addMatrix(registry, 34, "component", arithmeticVerifierOracle);
    addMatrix(registry, 35, "component", pointerCallOracle);
    addMatrix(registry, 36, "component", memoryVerifierOracle);
    addMatrix(registry, 37, "component", terminatorVerifierOracle);
    addMatrix(registry, 38, "unit", helperVerifierOracle);
}
}
