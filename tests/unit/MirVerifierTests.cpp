#include "nativesharp/MIR/MirArena.h"
#include "nativesharp/MIR/MirBlock.h"
#include "nativesharp/MIR/MirFunction.h"
#include "nativesharp/MIR/MirInstruction.h"
#include "nativesharp/MIR/MirModule.h"
#include "nativesharp/MIR/MirVerifier.h"
#include "nativesharp/MIR/MirValue.h"
#include "nativesharp/Sema/Symbol.h"
#include "nativesharp/Sema/SymbolKind.h"
#include "nativesharp/Sema/TypeContext.h"
#include "../support/TestContext.h"

using nativesharp::SourceRange;
using nativesharp::mir::MirArena;
using nativesharp::mir::MirBlock;
using nativesharp::mir::MirFunction;
using nativesharp::mir::MirInstruction;
using nativesharp::mir::MirModule;
using nativesharp::mir::MirOpcode;
using nativesharp::mir::MirVerifier;
using nativesharp::mir::MirValue;
using nativesharp::sema::Symbol;
using nativesharp::sema::SymbolKind;
using nativesharp::sema::Type;
using nativesharp::sema::TypeContext;
using nativesharp::sema::TypeKind;

namespace
{
struct FunctionFixture
{
    MirArena arena;
    MirModule* module = nullptr;
    MirFunction* function = nullptr;
    MirBlock* entry = nullptr;

    FunctionFixture(Symbol* symbol, bool defined)
    {
        module = arena.makeModule();
        function = arena.makeFunction(symbol);
        function->setDefined(defined);
        module->addFunction(function);
        if (defined)
        {
            entry = arena.makeBlock("entry");
            function->addBlock(entry);
        }
    }
};

MirInstruction* addReturnVoid(FunctionFixture& fixture)
{
    MirInstruction* instruction = fixture.arena.makeInstruction(MirOpcode::Return, SourceRange());
    fixture.entry->addInstruction(instruction);
    return instruction;
}
}

int main()
{
    using nativesharp::tests::TestContext;
    TestContext tests;
    TypeContext types;

    {
        Type* type = types.functionType(types.builtin(TypeKind::Void), {});
        Symbol symbol(SymbolKind::Function, "F", type, nullptr);
        FunctionFixture fixture(&symbol, true);
        addReturnVoid(fixture);
        MirVerifier verifier;
        tests.expect(verifier.verify(fixture.module), "verifier accepts minimal defined void function");
    }

    {
        Type* type = types.functionType(types.builtin(TypeKind::Void), {});
        Symbol symbol(SymbolKind::Function, "F", type, nullptr);
        FunctionFixture fixture(&symbol, true);
        MirVerifier verifier;
        tests.expect(!verifier.verify(fixture.module), "verifier rejects block without terminator");
    }

    {
        Type* type = types.functionType(types.builtin(TypeKind::Void), {});
        Symbol symbol(SymbolKind::Function, "F", type, nullptr);
        FunctionFixture fixture(&symbol, true);
        addReturnVoid(fixture);
        MirInstruction* constant = fixture.arena.makeInstruction(MirOpcode::Constant, SourceRange());
        MirValue* value = fixture.arena.makeValue(types.builtin(TypeKind::Int));
        constant->setResult(value);
        constant->setConstant(nativesharp::sema::ConstantValue::signedInteger(1));
        fixture.entry->addInstruction(constant);
        MirVerifier verifier;
        tests.expect(!verifier.verify(fixture.module), "verifier rejects instruction after terminator");
    }

    {
        Type* type = types.functionType(types.builtin(TypeKind::Void), {});
        Symbol symbol(SymbolKind::Function, "F", type, nullptr);
        FunctionFixture fixture(&symbol, true);
        MirValue* condition = fixture.arena.makeValue(types.builtin(TypeKind::Int));
        MirInstruction* constant = fixture.arena.makeInstruction(MirOpcode::Constant, SourceRange());
        constant->setResult(condition);
        constant->setConstant(nativesharp::sema::ConstantValue::signedInteger(1));
        fixture.entry->addInstruction(constant);
        MirBlock* target = fixture.arena.makeBlock("target");
        fixture.function->addBlock(target);
        MirInstruction* branch = fixture.arena.makeInstruction(MirOpcode::Branch, SourceRange());
        branch->addOperand(condition);
        branch->setFirstTarget(target);
        branch->setSecondTarget(target);
        fixture.entry->addInstruction(branch);
        MirInstruction* ret = fixture.arena.makeInstruction(MirOpcode::Return, SourceRange());
        target->addInstruction(ret);
        MirVerifier verifier;
        tests.expect(!verifier.verify(fixture.module), "verifier rejects non-bool branch condition");
    }

    {
        Type* type = types.functionType(types.builtin(TypeKind::Int), {});
        Symbol symbol(SymbolKind::Function, "F", type, nullptr);
        FunctionFixture fixture(&symbol, true);
        MirValue* value = fixture.arena.makeValue(types.builtin(TypeKind::Long));
        MirInstruction* constant = fixture.arena.makeInstruction(MirOpcode::Constant, SourceRange());
        constant->setResult(value);
        constant->setConstant(nativesharp::sema::ConstantValue::signedInteger(1));
        fixture.entry->addInstruction(constant);
        MirInstruction* ret = fixture.arena.makeInstruction(MirOpcode::Return, SourceRange());
        ret->addOperand(value);
        fixture.entry->addInstruction(ret);
        MirVerifier verifier;
        tests.expect(!verifier.verify(fixture.module), "verifier rejects return value with wrong type");
    }

    {
        Type* type = types.functionType(types.builtin(TypeKind::Void), {});
        Symbol symbol(SymbolKind::Function, "F", type, nullptr);
        FunctionFixture fixture(&symbol, false);
        MirBlock* illegal = fixture.arena.makeBlock("illegal");
        fixture.function->addBlock(illegal);
        MirVerifier verifier;
        tests.expect(!verifier.verify(fixture.module), "verifier rejects blocks attached to a prototype");
    }

    {
        Type* type = types.functionType(types.builtin(TypeKind::Void), {});
        Symbol symbolA(SymbolKind::Function, "A", type, nullptr);
        Symbol symbolB(SymbolKind::Function, "B", type, nullptr);
        FunctionFixture fixtureA(&symbolA, true);
        MirFunction* other = fixtureA.arena.makeFunction(&symbolB);
        other->setDefined(true);
        fixtureA.module->addFunction(other);
        MirBlock* otherBlock = fixtureA.arena.makeBlock("other.entry");
        other->addBlock(otherBlock);
        MirInstruction* otherReturn = fixtureA.arena.makeInstruction(MirOpcode::Return, SourceRange());
        otherBlock->addInstruction(otherReturn);
        MirInstruction* jump = fixtureA.arena.makeInstruction(MirOpcode::Jump, SourceRange());
        jump->setFirstTarget(otherBlock);
        fixtureA.entry->addInstruction(jump);
        MirVerifier verifier;
        tests.expect(!verifier.verify(fixtureA.module), "verifier rejects jump target owned by another function");
    }

    {
        Type* type = types.functionType(types.builtin(TypeKind::Void), {});
        Symbol functionSymbol(SymbolKind::Function, "F", type, nullptr);
        Symbol localSymbol(SymbolKind::Variable, "x", types.builtin(TypeKind::Int), nullptr);
        FunctionFixture fixture(&functionSymbol, true);
        MirValue* wrong = fixture.arena.makeValue(types.builtin(TypeKind::Long));
        MirInstruction* constant = fixture.arena.makeInstruction(MirOpcode::Constant, SourceRange());
        constant->setResult(wrong);
        constant->setConstant(nativesharp::sema::ConstantValue::signedInteger(1));
        fixture.entry->addInstruction(constant);
        MirInstruction* store = fixture.arena.makeInstruction(MirOpcode::StoreSymbol, SourceRange());
        store->setSymbol(&localSymbol);
        store->addOperand(wrong);
        fixture.entry->addInstruction(store);
        addReturnVoid(fixture);
        MirVerifier verifier;
        tests.expect(!verifier.verify(fixture.module), "verifier rejects store.symbol type mismatch");
    }

    {
        Type* type = types.functionType(types.builtin(TypeKind::Void), {});
        Symbol functionSymbol(SymbolKind::Function, "F", type, nullptr);
        FunctionFixture fixture(&functionSymbol, true);
        MirValue* value = fixture.arena.makeValue(types.builtin(TypeKind::Int));
        MirInstruction* constantA = fixture.arena.makeInstruction(MirOpcode::Constant, SourceRange());
        constantA->setResult(value);
        constantA->setConstant(nativesharp::sema::ConstantValue::signedInteger(1));
        fixture.entry->addInstruction(constantA);
        MirInstruction* constantB = fixture.arena.makeInstruction(MirOpcode::Constant, SourceRange());
        constantB->setResult(value);
        constantB->setConstant(nativesharp::sema::ConstantValue::signedInteger(2));
        fixture.entry->addInstruction(constantB);
        addReturnVoid(fixture);
        MirVerifier verifier;
        tests.expect(verifier.verify(fixture.module),
            "verifier deliberately permits multiple definitions before SSA construction");
    }

    {
        Type* type = types.functionType(types.builtin(TypeKind::Void), {});
        Symbol symbolA(SymbolKind::Function, "A", type, nullptr);
        Symbol symbolB(SymbolKind::Function, "B", type, nullptr);
        MirArena arena;
        MirModule* module = arena.makeModule();
        MirFunction* a = arena.makeFunction(&symbolA);
        MirFunction* b = arena.makeFunction(&symbolB);
        a->setDefined(true);
        b->setDefined(true);
        module->addFunction(a);
        module->addFunction(b);
        MirBlock* aEntry = arena.makeBlock("a.entry");
        MirBlock* bEntry = arena.makeBlock("b.entry");
        a->addBlock(aEntry);
        b->addBlock(bEntry);
        MirValue* foreign = arena.makeValue(types.builtin(TypeKind::Int));
        MirInstruction* constant = arena.makeInstruction(MirOpcode::Constant, SourceRange());
        constant->setResult(foreign);
        constant->setConstant(nativesharp::sema::ConstantValue::signedInteger(1));
        aEntry->addInstruction(constant);
        MirInstruction* aReturn = arena.makeInstruction(MirOpcode::Return, SourceRange());
        aEntry->addInstruction(aReturn);
        MirInstruction* copy = arena.makeInstruction(MirOpcode::Copy, SourceRange());
        MirValue* localResult = arena.makeValue(types.builtin(TypeKind::Int));
        copy->setResult(localResult);
        copy->addOperand(foreign);
        bEntry->addInstruction(copy);
        MirInstruction* bReturn = arena.makeInstruction(MirOpcode::Return, SourceRange());
        bEntry->addInstruction(bReturn);
        MirVerifier verifier;
        tests.expect(!verifier.verify(module), "verifier rejects virtual values crossing function ownership");
    }


    {
        Type* type = types.functionType(types.builtin(TypeKind::Void), {});
        Symbol functionSymbol(SymbolKind::Function, "F", type, nullptr);
        FunctionFixture fixture(&functionSymbol, true);
        MirValue* left = fixture.arena.makeValue(types.builtin(TypeKind::Int));
        MirInstruction* leftConstant = fixture.arena.makeInstruction(MirOpcode::Constant, SourceRange());
        leftConstant->setResult(left);
        leftConstant->setConstant(nativesharp::sema::ConstantValue::signedInteger(1));
        fixture.entry->addInstruction(leftConstant);
        MirValue* right = fixture.arena.makeValue(types.builtin(TypeKind::Float));
        MirInstruction* rightConstant = fixture.arena.makeInstruction(MirOpcode::Constant, SourceRange());
        rightConstant->setResult(right);
        rightConstant->setConstant(nativesharp::sema::ConstantValue::floating(2.0));
        fixture.entry->addInstruction(rightConstant);
        MirInstruction* add = fixture.arena.makeInstruction(MirOpcode::Add, SourceRange());
        add->setResult(fixture.arena.makeValue(types.builtin(TypeKind::Int)));
        add->addOperand(left);
        add->addOperand(right);
        fixture.entry->addInstruction(add);
        addReturnVoid(fixture);
        MirVerifier verifier;
        tests.expect(!verifier.verify(fixture.module), "verifier rejects arithmetic with mismatched operand types");
    }

    {
        Type* type = types.functionType(types.builtin(TypeKind::Void), {});
        Symbol functionSymbol(SymbolKind::Function, "F", type, nullptr);
        FunctionFixture fixture(&functionSymbol, true);
        MirValue* value = fixture.arena.makeValue(types.builtin(TypeKind::Float));
        MirInstruction* constant = fixture.arena.makeInstruction(MirOpcode::Constant, SourceRange());
        constant->setResult(value);
        constant->setConstant(nativesharp::sema::ConstantValue::floating(1.0));
        fixture.entry->addInstruction(constant);
        MirInstruction* bitNot = fixture.arena.makeInstruction(MirOpcode::BitNot, SourceRange());
        bitNot->setResult(fixture.arena.makeValue(types.builtin(TypeKind::Float)));
        bitNot->addOperand(value);
        fixture.entry->addInstruction(bitNot);
        addReturnVoid(fixture);
        MirVerifier verifier;
        tests.expect(!verifier.verify(fixture.module), "verifier rejects bit.not on floating-point values");
    }

    {
        Type* type = types.functionType(types.builtin(TypeKind::Void), {});
        Symbol functionSymbol(SymbolKind::Function, "F", type, nullptr);
        FunctionFixture fixture(&functionSymbol, true);
        MirValue* source = fixture.arena.makeValue(types.builtin(TypeKind::Int));
        MirInstruction* constant = fixture.arena.makeInstruction(MirOpcode::Constant, SourceRange());
        constant->setResult(source);
        constant->setConstant(nativesharp::sema::ConstantValue::signedInteger(1));
        fixture.entry->addInstruction(constant);
        MirInstruction* convert = fixture.arena.makeInstruction(MirOpcode::Convert, SourceRange());
        convert->setResult(fixture.arena.makeValue(types.builtin(TypeKind::Int)));
        convert->addOperand(source);
        convert->setConversionKind(nativesharp::sema::ConversionKind::Identity);
        fixture.entry->addInstruction(convert);
        addReturnVoid(fixture);
        MirVerifier verifier;
        tests.expect(!verifier.verify(fixture.module), "verifier rejects explicit MIR identity conversion nodes");
    }

    {
        Type* type = types.functionType(types.builtin(TypeKind::Void), {});
        Symbol functionSymbol(SymbolKind::Function, "F", type, nullptr);
        FunctionFixture fixture(&functionSymbol, true);
        MirValue* index = fixture.arena.makeValue(types.builtin(TypeKind::Int));
        MirInstruction* constant = fixture.arena.makeInstruction(MirOpcode::Constant, SourceRange());
        constant->setResult(index);
        constant->setConstant(nativesharp::sema::ConstantValue::signedInteger(0));
        fixture.entry->addInstruction(constant);
        MirInstruction* bounds = fixture.arena.makeInstruction(MirOpcode::BoundsCheck, SourceRange());
        bounds->addOperand(index);
        bounds->setImmediate(0);
        fixture.entry->addInstruction(bounds);
        addReturnVoid(fixture);
        MirVerifier verifier;
        tests.expect(!verifier.verify(fixture.module), "verifier rejects zero-sized fixed bounds check");
    }

    {
        Type* type = types.functionType(types.builtin(TypeKind::Void), {});
        Symbol functionSymbol(SymbolKind::Function, "F", type, nullptr);
        FunctionFixture fixture(&functionSymbol, true);
        Type* pointerToInt = types.pointerTo(types.builtin(TypeKind::Int));
        Symbol localSymbol(SymbolKind::Variable, "base", types.builtin(TypeKind::Int), nullptr);
        fixture.function->addLocal(&localSymbol);
        MirValue* base = fixture.arena.makeValue(pointerToInt);
        MirInstruction* baseAddress = fixture.arena.makeInstruction(MirOpcode::AddressOfSymbol, SourceRange());
        baseAddress->setResult(base);
        baseAddress->setSymbol(&localSymbol);
        fixture.entry->addInstruction(baseAddress);
        MirValue* index = fixture.arena.makeValue(types.builtin(TypeKind::Int));
        MirInstruction* indexConstant = fixture.arena.makeInstruction(MirOpcode::Constant, SourceRange());
        indexConstant->setResult(index);
        indexConstant->setConstant(nativesharp::sema::ConstantValue::signedInteger(0));
        fixture.entry->addInstruction(indexConstant);
        MirInstruction* address = fixture.arena.makeInstruction(MirOpcode::IndexAddress, SourceRange());
        address->setAuxiliaryType(types.arrayOf(types.builtin(TypeKind::Int), 4));
        address->setResult(fixture.arena.makeValue(pointerToInt));
        address->addOperand(base);
        address->addOperand(index);
        fixture.entry->addInstruction(address);
        addReturnVoid(fixture);
        MirVerifier verifier;
        tests.expect(
            !verifier.verify(fixture.module),
            "verifier rejects array index.address with pointer base mismatch");
        tests.expect(verifier.error().find("array index.address") != std::string::npos,
            "array index.address negative test fails for the intended base/result relationship");
    }


    {
        Type* type = types.functionType(types.builtin(TypeKind::Void), {});
        Symbol functionSymbol(SymbolKind::Function, "F", type, nullptr);
        FunctionFixture fixture(&functionSymbol, true);
        MirInstruction* constant = fixture.arena.makeInstruction(MirOpcode::Constant, SourceRange());
        MirValue* value = fixture.arena.makeValue(types.builtin(TypeKind::Bool));
        constant->setResult(value);
        constant->setConstant(nativesharp::sema::ConstantValue::signedInteger(1));
        fixture.entry->addInstruction(constant);
        addReturnVoid(fixture);
        MirVerifier verifier;
        tests.expect(
            !verifier.verify(fixture.module),
            "verifier rejects constant payload incompatible with result type");
    }

    {
        Type* type = types.functionType(types.builtin(TypeKind::Void), {});
        Symbol functionSymbol(SymbolKind::Function, "F", type, nullptr);
        Symbol localSymbol(SymbolKind::Variable, "x", types.builtin(TypeKind::Int), nullptr);
        FunctionFixture fixture(&functionSymbol, true);
        fixture.function->addLocal(&localSymbol);
        fixture.function->addLocal(&localSymbol);
        addReturnVoid(fixture);
        MirVerifier verifier;
        tests.expect(!verifier.verify(fixture.module), "verifier rejects duplicate local symbol entries");
    }

    {
        Type* type = types.functionType(types.builtin(TypeKind::Void), {});
        Symbol functionSymbol(SymbolKind::Function, "F", type, nullptr);
        MirArena arena;
        MirModule* module = arena.makeModule();
        MirFunction* first = arena.makeFunction(&functionSymbol);
        MirFunction* second = arena.makeFunction(&functionSymbol);
        module->addFunction(first);
        module->addFunction(second);
        MirVerifier verifier;
        tests.expect(!verifier.verify(module), "verifier rejects duplicate function symbols at module level");
    }


    {
        Type* type = types.functionType(types.builtin(TypeKind::Void), {});
        Symbol functionSymbol(SymbolKind::Function, "F", type, nullptr);
        Symbol foreignLocal(SymbolKind::Variable, "foreign", types.builtin(TypeKind::Int), nullptr);
        FunctionFixture fixture(&functionSymbol, true);
        MirInstruction* load = fixture.arena.makeInstruction(MirOpcode::LoadSymbol, SourceRange());
        load->setResult(fixture.arena.makeValue(types.builtin(TypeKind::Int)));
        load->setSymbol(&foreignLocal);
        fixture.entry->addInstruction(load);
        addReturnVoid(fixture);
        MirVerifier verifier;
        tests.expect(!verifier.verify(fixture.module),
            "verifier rejects load.symbol for local storage not owned by the current function");
    }


    {
        Type* type = types.functionType(types.builtin(TypeKind::Void), {});
        Symbol functionSymbol(SymbolKind::Function, "F", type, nullptr);
        FunctionFixture fixture(&functionSymbol, true);
        Type* pointerType = types.pointerTo(types.builtin(TypeKind::Int));
        Symbol leftSymbol(SymbolKind::Variable, "left", types.builtin(TypeKind::Int), nullptr);
        Symbol rightSymbol(SymbolKind::Variable, "right", types.builtin(TypeKind::Int), nullptr);
        fixture.function->addLocal(&leftSymbol);
        fixture.function->addLocal(&rightSymbol);
        MirValue* left = fixture.arena.makeValue(pointerType);
        MirValue* right = fixture.arena.makeValue(pointerType);
        MirInstruction* leftAddress = fixture.arena.makeInstruction(MirOpcode::AddressOfSymbol, SourceRange());
        leftAddress->setResult(left);
        leftAddress->setSymbol(&leftSymbol);
        fixture.entry->addInstruction(leftAddress);
        MirInstruction* rightAddress = fixture.arena.makeInstruction(MirOpcode::AddressOfSymbol, SourceRange());
        rightAddress->setResult(right);
        rightAddress->setSymbol(&rightSymbol);
        fixture.entry->addInstruction(rightAddress);
        MirInstruction* compare = fixture.arena.makeInstruction(MirOpcode::CompareLess, SourceRange());
        compare->setResult(fixture.arena.makeValue(types.builtin(TypeKind::Bool)));
        compare->addOperand(left);
        compare->addOperand(right);
        fixture.entry->addInstruction(compare);
        addReturnVoid(fixture);
        MirVerifier verifier;
        tests.expect(!verifier.verify(fixture.module), "verifier rejects relational comparison on pointers");
    }


    {
        Type* parameterType = types.builtin(TypeKind::Int);
        Type* functionType = types.functionType(types.builtin(TypeKind::Void), {parameterType});
        Symbol functionSymbol(SymbolKind::Function, "F", functionType, nullptr);
        Symbol parameterSymbol(SymbolKind::Parameter, "value", parameterType, nullptr);
        FunctionFixture fixture(&functionSymbol, true);
        fixture.function->addLocal(&parameterSymbol);
        fixture.function->addParameter(fixture.arena.makeValue(parameterType));
        addReturnVoid(fixture);
        MirVerifier verifier;
        tests.expect(!verifier.verify(fixture.module),
            "verifier rejects runtime parameter values without an explicit positional param instruction");
    }


    {
        Type* parameterType = types.builtin(TypeKind::Int);
        Type* functionType = types.functionType(types.builtin(TypeKind::Void), {parameterType});
        Symbol functionSymbol(SymbolKind::Function, "F", functionType, nullptr);
        Symbol parameterSymbol(SymbolKind::Parameter, "value", parameterType, nullptr);
        FunctionFixture fixture(&functionSymbol, true);
        fixture.function->addLocal(&parameterSymbol);
        MirValue* parameterValue = fixture.arena.makeValue(parameterType);
        fixture.function->addParameter(parameterValue);
        MirInstruction* entryJump = fixture.arena.makeInstruction(MirOpcode::Jump, SourceRange());
        MirBlock* second = fixture.arena.makeBlock("second");
        fixture.function->addBlock(second);
        entryJump->setFirstTarget(second);
        fixture.entry->addInstruction(entryJump);
        MirInstruction* parameter = fixture.arena.makeInstruction(MirOpcode::Parameter, SourceRange());
        parameter->setResult(parameterValue);
        parameter->setSymbol(&parameterSymbol);
        parameter->setImmediate(0);
        second->addInstruction(parameter);
        MirInstruction* ret = fixture.arena.makeInstruction(MirOpcode::Return, SourceRange());
        second->addInstruction(ret);
        MirVerifier verifier;
        tests.expect(!verifier.verify(fixture.module), "verifier rejects param instruction outside entry block");
    }

    return tests.result();
}
