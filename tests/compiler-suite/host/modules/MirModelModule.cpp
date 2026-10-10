#include "MirContractSupport.h"

#include "nativesharp/Sema/ConstantValue.h"
#include "nativesharp/Sema/ConversionKind.h"

#include <array>
#include <string_view>
#include <vector>

namespace nsc::suite
{
namespace
{
using namespace mir_contract_support;
using nativesharp::mir::MirGlobal;
using nativesharp::mir::MirPhiIncoming;
using nativesharp::mir::isMirTerminator;
using nativesharp::mir::mirOpcodeName;
using nativesharp::sema::ConstantValue;
using nativesharp::sema::ConversionKind;

void arenaOracle(TestContext& context, std::size_t variant)
{
    TypeContext types;
    Symbol globalSymbol(SymbolKind::Variable, "g", types.builtin(TypeKind::Int), nullptr);
    Type* signature = types.functionType(types.builtin(TypeKind::Void), {});
    Symbol functionSymbol(SymbolKind::Function, "F", signature, nullptr);
    MirArena arena;
    MirModule* module = arena.makeModule();
    MirGlobal* global = arena.makeGlobal(&globalSymbol);
    MirFunction* function = arena.makeFunction(&functionSymbol);
    MirBlock* first = arena.makeBlock("first");
    MirBlock* second = arena.makeBlock("second");
    MirInstruction* instruction = arena.makeInstruction(MirOpcode::Copy, SourceRange());
    MirValue* firstValue = arena.makeValue(types.builtin(TypeKind::Int));
    MirValue* secondValue = arena.makeValue(types.builtin(TypeKind::Long));
    context.expect(module != nullptr && global != nullptr && function != nullptr && instruction != nullptr,
        "arena must return every requested object");
    context.expect(first->id() == 0 && second->id() == 1, "block IDs must be monotone and zero based");
    context.expect(firstValue->id() == 0 && secondValue->id() == 1, "value IDs must be independent and monotone");
    context.expect(arena.values().size() == 2 && arena.values()[variant % 2] != nullptr,
        "value registry must retain allocation order");
}

void valueOracle(TestContext& context, std::size_t variant)
{
    TypeContext types;
    MirArena arena;
    Type* type = variant % 2 == 0 ? types.builtin(TypeKind::Int) : types.builtin(TypeKind::Long);
    MirValue* value = arena.makeValue(type);
    context.expect(value->id() == 0, "first virtual value must have ID zero");
    context.expect(value->type() == type, "virtual value must preserve semantic type identity");
    if (variant >= 4)
    {
        MirValue* next = arena.makeValue(type);
        context.expect(next->id() == value->id() + 1, "virtual IDs must remain monotone");
    }
}

void blockOracle(TestContext& context, std::size_t variant)
{
    MirArena arena;
    MirBlock* block = arena.makeBlock("entry");
    MirInstruction* copy = arena.makeInstruction(MirOpcode::Copy, SourceRange());
    MirInstruction* ret = arena.makeInstruction(MirOpcode::Return, SourceRange());
    block->addInstruction(copy);
    block->addInstruction(ret);
    context.expect(block->id() == 0 && block->name() == "entry", "block must preserve identity and name");
    context.expect(block->terminated(), "final return must terminate the block");
    if (variant >= 2)
    {
        block->replaceInstructions({copy});
        context.expect(!block->terminated() && block->instructions().front() == copy,
            "replacement must preserve supplied order and recompute termination");
    }
}

void functionOracle(TestContext& context, std::size_t variant)
{
    TypeContext types;
    Type* signature = types.functionType(types.builtin(TypeKind::Void), {});
    Symbol functionSymbol(SymbolKind::Function, "F", signature, nullptr);
    Symbol local(SymbolKind::Variable, "x", types.builtin(TypeKind::Int), nullptr);
    MirArena arena;
    MirFunction* function = arena.makeFunction(&functionSymbol);
    MirValue* parameter = arena.makeValue(types.builtin(TypeKind::Int));
    MirBlock* entry = arena.makeBlock("entry");
    function->setDefined(variant != 2);
    function->addParameter(parameter);
    function->addLocal(&local);
    function->addBlock(entry);
    context.expect(function->symbol() == &functionSymbol, "function must preserve symbol identity");
    context.expect(function->parameters().front() == parameter && function->locals().front() == &local,
        "function collections must preserve insertion order");
    context.expect(function->entryBlock() == entry, "first block must be the entry block");
    function->replaceBlocks({entry});
    context.expect(function->blocks().size() == 1, "block replacement must replace the complete sequence");
}

void globalOracle(TestContext& context, std::size_t variant)
{
    TypeContext types;
    Symbol symbol(SymbolKind::Variable, "G", types.builtin(TypeKind::Int), nullptr);
    MirArena arena;
    MirGlobal* global = arena.makeGlobal(&symbol);
    context.expect(global->symbol() == &symbol && !global->hasExplicitInitializer(),
        "global starts with its symbol and no explicit initializer");
    global->setExplicitInitializer(ConstantValue::signedInteger(static_cast<std::int64_t>(variant + 1)));
    context.expect(global->hasExplicitInitializer(), "setting a constant must mark the initializer present");
    context.expect(global->initializer().signedInteger() == static_cast<std::int64_t>(variant + 1),
        "global must preserve the exact initializer payload");
}

void moduleOracle(TestContext& context, std::size_t variant)
{
    TypeContext types;
    Symbol globalSymbol(SymbolKind::Variable, "G", types.builtin(TypeKind::Int), nullptr);
    Type* signature = types.functionType(types.builtin(TypeKind::Void), {});
    Symbol firstSymbol(SymbolKind::Function, "A", signature, nullptr);
    Symbol secondSymbol(SymbolKind::Function, "B", signature, nullptr);
    MirArena arena;
    MirModule* module = arena.makeModule();
    MirGlobal* global = arena.makeGlobal(&globalSymbol);
    MirFunction* first = arena.makeFunction(&firstSymbol);
    MirFunction* second = arena.makeFunction(&secondSymbol);
    module->addGlobal(global);
    module->addFunction(first);
    module->addFunction(second);
    context.expect(module->globals().size() == 1 && module->globals().front() == global,
        "module must preserve global insertion order");
    context.expect(module->functions().size() == 2 && module->functions()[variant % 2] != nullptr,
        "module must preserve function insertion order");
    context.expect(module->findFunction(&firstSymbol) == first && module->findFunction(&secondSymbol) == second,
        "function lookup must use symbol identity");
}

constexpr std::array<MirOpcode, 47> kOpcodes = {
    MirOpcode::Parameter, MirOpcode::Undef, MirOpcode::Phi, MirOpcode::Constant, MirOpcode::Copy,
    MirOpcode::LoadSymbol, MirOpcode::StoreSymbol, MirOpcode::AddressOfSymbol, MirOpcode::Load, MirOpcode::Store,
    MirOpcode::Convert, MirOpcode::Negate, MirOpcode::BitNot, MirOpcode::LogicalNot, MirOpcode::Add,
    MirOpcode::Subtract, MirOpcode::Multiply, MirOpcode::Divide, MirOpcode::Remainder, MirOpcode::BitAnd,
    MirOpcode::BitOr, MirOpcode::BitXor, MirOpcode::ShiftLeft, MirOpcode::ShiftRight, MirOpcode::CompareEqual,
    MirOpcode::CompareNotEqual, MirOpcode::CompareLess, MirOpcode::CompareLessEqual, MirOpcode::CompareGreater,
    MirOpcode::CompareGreaterEqual, MirOpcode::PointerAdd, MirOpcode::PointerSubtract,
    MirOpcode::PointerDifference, MirOpcode::Call, MirOpcode::BoundsCheck, MirOpcode::FieldAddress,
    MirOpcode::IndexAddress, MirOpcode::ExtractField, MirOpcode::Allocate, MirOpcode::AllocateArray,
    MirOpcode::Deallocate, MirOpcode::DeallocateArray, MirOpcode::StackAllocate, MirOpcode::Jump,
    MirOpcode::Branch, MirOpcode::Return, MirOpcode::Unreachable,
};

void opcodeOracle(TestContext& context, std::size_t variant)
{
    for (MirOpcode opcode : kOpcodes)
    {
        context.expect(mirOpcodeName(opcode) != "invalid", "every declared MIR opcode must have a stable name");
    }
    context.expect(mirOpcodeName(static_cast<MirOpcode>(999)) == "invalid", "unknown opcodes must report invalid");
    for (MirOpcode opcode : kOpcodes)
    {
        const bool expected = opcode == MirOpcode::Jump || opcode == MirOpcode::Branch ||
            opcode == MirOpcode::Return || opcode == MirOpcode::Unreachable;
        context.expect(isMirTerminator(opcode) == expected, "terminator classification must be exhaustive");
    }
    context.expect(!mirOpcodeName(kOpcodes[variant % kOpcodes.size()]).empty(), "selected opcode name is nonempty");
}

void resetOracle(TestContext& context, std::size_t variant)
{
    TypeContext types;
    Symbol symbol(SymbolKind::Variable, "x", types.builtin(TypeKind::Int), nullptr);
    MirArena arena;
    MirInstruction* instruction = arena.makeInstruction(MirOpcode::Call, SourceRange());
    MirValue* result = arena.makeValue(types.builtin(TypeKind::Int));
    MirValue* operand = arena.makeValue(types.builtin(TypeKind::Int));
    MirBlock* target = arena.makeBlock("target");
    instruction->setResult(result);
    instruction->addOperand(operand);
    instruction->addPhiIncoming(target, operand);
    instruction->setSymbol(&symbol);
    instruction->setAuxiliaryType(types.builtin(TypeKind::Long));
    instruction->setConstant(ConstantValue::signedInteger(7));
    instruction->setConversionKind(ConversionKind::ExplicitNumeric);
    instruction->setImmediate(3);
    instruction->setOffset(5);
    instruction->setFirstTarget(target);
    instruction->setSecondTarget(target);
    instruction->resetTo(kOpcodes[variant % kOpcodes.size()]);
    context.expect(instruction->result() == result, "resetTo must retain result identity");
    context.expect(!instruction->range().isValid(), "resetTo must retain the original empty source range");
    context.expect(instruction->operands().empty() && instruction->phiIncoming().empty(),
        "resetTo must clear value payloads");
    context.expect(instruction->symbol() == nullptr && instruction->auxiliaryType() == nullptr,
        "resetTo must clear symbol and type payloads");
    context.expect(!instruction->hasConstant() && !instruction->hasConversion() && !instruction->hasImmediate() &&
            !instruction->hasOffset() && instruction->firstTarget() == nullptr && instruction->secondTarget() == nullptr,
        "resetTo must clear every optional payload channel");
}

void operandOracle(TestContext& context, std::size_t variant)
{
    TypeContext types;
    MirArena arena;
    MirInstruction* instruction = arena.makeInstruction(MirOpcode::Add, SourceRange());
    MirValue* first = arena.makeValue(types.builtin(TypeKind::Int));
    MirValue* second = arena.makeValue(types.builtin(TypeKind::Int));
    MirValue* replacement = arena.makeValue(types.builtin(TypeKind::Int));
    instruction->addOperand(first);
    instruction->addOperand(second);
    instruction->addPhiIncoming(arena.makeBlock("left"), first);
    instruction->setOperand(variant % 2, replacement);
    context.expect(instruction->operands()[variant % 2] == replacement, "setOperand must update the selected slot");
    instruction->replaceValueUse(first, replacement);
    context.expect(instruction->phiIncoming().front().value == replacement,
        "replaceValueUse must cover ordinary operands and PHI incoming values");
    instruction->clearOperands();
    context.expect(instruction->operands().empty(), "clearOperands must remove the complete operand sequence");
}

void phiOracle(TestContext& context, std::size_t variant)
{
    TypeContext types;
    MirArena arena;
    MirInstruction* instruction = arena.makeInstruction(MirOpcode::Phi, SourceRange());
    MirBlock* firstBlock = arena.makeBlock("first");
    MirBlock* secondBlock = arena.makeBlock("second");
    MirValue* firstValue = arena.makeValue(types.builtin(TypeKind::Int));
    MirValue* secondValue = arena.makeValue(types.builtin(TypeKind::Int));
    instruction->addPhiIncoming(firstBlock, firstValue);
    instruction->addPhiIncoming(secondBlock, secondValue);
    const std::vector<MirPhiIncoming>& incoming = instruction->phiIncoming();
    context.expect(incoming.size() == 2 && incoming[0].block == firstBlock && incoming[1].value == secondValue,
        "PHI incoming pairs must preserve insertion order and pointer identity");
    if (variant >= 2)
    {
        instruction->clearPhiIncoming();
        context.expect(instruction->phiIncoming().empty(), "clearPhiIncoming must remove all pairs");
    }
}

void payloadOracle(TestContext& context, std::size_t variant)
{
    TypeContext types;
    Symbol symbol(SymbolKind::Variable, "x", types.builtin(TypeKind::Int), nullptr);
    MirArena arena;
    MirInstruction* instruction = arena.makeInstruction(MirOpcode::Convert, SourceRange());
    instruction->setSymbol(&symbol);
    instruction->setAuxiliaryType(types.builtin(TypeKind::Long));
    instruction->setConstant(ConstantValue::signedInteger(static_cast<std::int64_t>(variant)));
    instruction->setConversionKind(ConversionKind::ExplicitNumeric);
    instruction->setImmediate(variant);
    instruction->setOffset(variant + 1);
    context.expect(instruction->symbol() == &symbol, "symbol payload must preserve identity");
    context.expect(instruction->auxiliaryType() == types.builtin(TypeKind::Long), "auxiliary type must preserve identity");
    context.expect(instruction->hasConstant() && instruction->constant().signedInteger() ==
            static_cast<std::int64_t>(variant), "constant channel must preserve presence and payload");
    context.expect(instruction->hasConversion() && instruction->conversionKind() == ConversionKind::ExplicitNumeric,
        "conversion channel must preserve presence and kind");
    context.expect(instruction->hasImmediate() && instruction->immediate() == variant,
        "immediate channel must preserve presence and value");
    context.expect(instruction->hasOffset() && instruction->offset() == variant + 1,
        "offset channel must preserve presence and value");
}

void targetOracle(TestContext& context, std::size_t variant)
{
    MirArena arena;
    MirInstruction* instruction = arena.makeInstruction(MirOpcode::Branch, SourceRange());
    MirBlock* first = arena.makeBlock("true");
    MirBlock* second = arena.makeBlock("false");
    instruction->setFirstTarget(first);
    instruction->setSecondTarget(second);
    context.expect(instruction->firstTarget() == first && instruction->secondTarget() == second,
        "control targets must preserve exact block identities");
    if (variant >= 4)
    {
        instruction->setSecondTarget(nullptr);
        context.expect(instruction->secondTarget() == nullptr, "target setters must preserve explicit absence");
    }
}
}

void registerMirModelModule(TestRegistry& registry)
{
    addMatrix(registry, 1, "component", arenaOracle);
    addMatrix(registry, 2, "unit", valueOracle);
    addMatrix(registry, 3, "unit", blockOracle);
    addMatrix(registry, 4, "component", functionOracle);
    addMatrix(registry, 5, "unit", globalOracle);
    addMatrix(registry, 6, "unit", moduleOracle);
    addMatrix(registry, 7, "unit", opcodeOracle);
    addMatrix(registry, 8, "unit", resetOracle);
    addMatrix(registry, 9, "unit", operandOracle);
    addMatrix(registry, 10, "unit", phiOracle);
    addMatrix(registry, 11, "unit", payloadOracle);
    addMatrix(registry, 12, "unit", targetOracle);
}
}
