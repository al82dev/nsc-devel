#include "nativesharp/Frontend/FrontendCompilation.h"
#include "nativesharp/MIR/MirBlock.h"
#include "nativesharp/MIR/MirDumper.h"
#include "nativesharp/MIR/MirFunction.h"
#include "nativesharp/MIR/MirGlobal.h"
#include "nativesharp/MIR/MirInstruction.h"
#include "nativesharp/MIR/MirModule.h"
#include "nativesharp/MIR/MirOpcode.h"
#include "nativesharp/MIR/MirValue.h"
#include "nativesharp/Sema/Symbol.h"
#include "../support/TestContext.h"

#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

using nativesharp::frontend::FrontendCompilation;
using nativesharp::mir::MirBlock;
using nativesharp::mir::MirFunction;
using nativesharp::mir::MirInstruction;
using nativesharp::mir::MirModule;
using nativesharp::mir::MirOpcode;
using nativesharp::mir::MirValue;

namespace
{
bool compile(std::string_view source, FrontendCompilation& compilation)
{
    return compilation.compileBuffer(std::filesystem::path("mir_test.ns"), std::string(source), false);
}

const MirFunction* findFunction(const MirModule* module, std::string_view name)
{
    if (module == nullptr)
    {
        return nullptr;
    }
    for (const MirFunction* function : module->functions())
    {
        if (function != nullptr && function->symbol() != nullptr && function->symbol()->name() == name)
        {
            return function;
        }
    }
    return nullptr;
}

const MirBlock* findBlock(const MirFunction* function, std::string_view prefix)
{
    if (function == nullptr)
    {
        return nullptr;
    }
    for (const MirBlock* block : function->blocks())
    {
        if (block != nullptr && block->name().starts_with(prefix))
        {
            return block;
        }
    }
    return nullptr;
}

const MirInstruction* findInstruction(const MirFunction* function, MirOpcode opcode)
{
    if (function == nullptr)
    {
        return nullptr;
    }
    for (const MirBlock* block : function->blocks())
    {
        for (const MirInstruction* instruction : block->instructions())
        {
            if (instruction->opcode() == opcode)
            {
                return instruction;
            }
        }
    }
    return nullptr;
}

int countOpcode(const MirFunction* function, MirOpcode opcode)
{
    int count = 0;
    if (function == nullptr)
    {
        return count;
    }
    for (const MirBlock* block : function->blocks())
    {
        for (const MirInstruction* instruction : block->instructions())
        {
            if (instruction->opcode() == opcode)
            {
                ++count;
            }
        }
    }
    return count;
}

int countStoreTo(const MirFunction* function, std::string_view name)
{
    int count = 0;
    if (function == nullptr)
    {
        return count;
    }
    for (const MirBlock* block : function->blocks())
    {
        for (const MirInstruction* instruction : block->instructions())
        {
            if (instruction->opcode() == MirOpcode::StoreSymbol && instruction->symbol() != nullptr &&
                instruction->symbol()->name() == name)
            {
                ++count;
            }
        }
    }
    return count;
}
}

int main()
{
    using nativesharp::tests::TestContext;
    TestContext tests;

    {
        FrontendCompilation compilation;
        tests.expect(compile("int Main() { int x = 10; return x + 32; }", compilation),
            "simple source lowers through MIR");
        const MirFunction* mainFunction = findFunction(compilation.mirModule(), "Main");
        tests.expect(mainFunction != nullptr && mainFunction->defined(), "Main is a defined MIR function");
        tests.expect(mainFunction != nullptr && mainFunction->entryBlock() != nullptr,
            "defined MIR function has an entry block");
        tests.expect(findInstruction(mainFunction, MirOpcode::StoreSymbol) != nullptr,
            "local initializer becomes store.symbol");
        tests.expect(findInstruction(mainFunction, MirOpcode::LoadSymbol) != nullptr,
            "local read becomes load.symbol");
        tests.expect(findInstruction(mainFunction, MirOpcode::Add) != nullptr,
            "resolved integer addition becomes MIR add opcode");
        tests.expect(mainFunction != nullptr && mainFunction->entryBlock()->terminated(),
            "entry block ends in an explicit terminator");
    }

    {
        FrontendCompilation compilation;
        const char* source = "int Main() { int x = 0; if (x == 0) { x = 1; } else { x = 2; } return x; }";
        tests.expect(compile(source, compilation), "if/else lowers through MIR");
        const MirFunction* function = findFunction(compilation.mirModule(), "Main");
        tests.expect(countOpcode(function, MirOpcode::Branch) >= 1, "if condition lowers to branch");
        tests.expect(findBlock(function, "if.then") != nullptr, "if lowering creates then block");
        tests.expect(findBlock(function, "if.else") != nullptr, "if lowering creates else block");
        tests.expect(findBlock(function, "if.end") != nullptr, "if lowering creates merge block");
    }

    {
        FrontendCompilation compilation;
        const char* source =
            "int Main() { int i = 0; while (i < 4) { i++; continue; } "
            "for (i = 0; i < 4; i++) { continue; } return i; }";
        tests.expect(compile(source, compilation), "loop and continue sample lowers through MIR");
        const MirFunction* function = findFunction(compilation.mirModule(), "Main");
        const MirBlock* whileBody = findBlock(function, "while.body");
        const MirBlock* forBody = findBlock(function, "for.body");
        tests.expect(whileBody != nullptr && !whileBody->instructions().empty() &&
                whileBody->instructions().back()->opcode() == MirOpcode::Jump &&
                whileBody->instructions().back()->firstTarget() != nullptr &&
                whileBody->instructions().back()->firstTarget()->name().starts_with("while.cond"),
            "continue in while jumps to while condition");
        tests.expect(forBody != nullptr && !forBody->instructions().empty() &&
                forBody->instructions().back()->opcode() == MirOpcode::Jump &&
                forBody->instructions().back()->firstTarget() != nullptr &&
                forBody->instructions().back()->firstTarget()->name().starts_with("for.inc"),
            "continue in for jumps to increment block");
    }

    {
        FrontendCompilation compilation;
        tests.expect(compile("int Main() { bool a = true; bool b = false; bool c = a && b; return 0; }", compilation),
            "short-circuit expression lowers through MIR");
        const MirFunction* function = findFunction(compilation.mirModule(), "Main");
        tests.expect(findBlock(function, "logic.rhs") != nullptr, "short-circuit lowering creates RHS block");
        tests.expect(findBlock(function, "logic.short") != nullptr, "short-circuit lowering creates short block");
        tests.expect(findBlock(function, "logic.end") != nullptr, "short-circuit lowering creates merge block");

        std::unordered_map<const MirValue*, int> definitions;
        for (const MirBlock* block : function->blocks())
        {
            for (const MirInstruction* instruction : block->instructions())
            {
                if (instruction->result() != nullptr)
                {
                    ++definitions[instruction->result()];
                }
            }
        }
        bool foundMultipleDefinition = false;
        for (const auto& entry : definitions)
        {
            if (entry.second > 1)
            {
                foundMultipleDefinition = true;
            }
        }
        tests.expect(foundMultipleDefinition,
            "short-circuit merge result deliberately has multiple definitions in non-SSA MIR");
    }

    {
        FrontendCompilation compilation;
        const char* source = "int Main() { int[4] a; int i = 0; a[i++] += 2; return i; }";
        tests.expect(compile(source, compilation), "compound indexed assignment lowers through MIR");
        const MirFunction* function = findFunction(compilation.mirModule(), "Main");
        tests.expect(countStoreTo(function, "i") == 2,
            "compound assignment evaluates i++ exactly once in its lvalue address");
        tests.expect(countOpcode(function, MirOpcode::BoundsCheck) == 1,
            "fixed-array compound assignment emits one bounds check");
    }

    {
        FrontendCompilation compilation;
        tests.expect(compile("int Main() { int[4] a; int i = 1; return a[i]; }", compilation),
            "fixed-array indexing lowers through MIR");
        const MirFunction* function = findFunction(compilation.mirModule(), "Main");
        bool ordered = false;
        for (const MirBlock* block : function->blocks())
        {
            for (std::size_t index = 1; index < block->instructions().size(); ++index)
            {
                if (block->instructions()[index]->opcode() == MirOpcode::IndexAddress &&
                    block->instructions()[index - 1]->opcode() == MirOpcode::BoundsCheck)
                {
                    ordered = true;
                }
            }
        }
        tests.expect(ordered, "bounds.check is emitted immediately before fixed-array index.address");
    }

    {
        FrontendCompilation compilation;
        tests.expect(compile("int Main() { int* p = new int[4]; delete[] p; return 0; }", compilation),
            "array allocation lowers through MIR");
        const MirFunction* function = findFunction(compilation.mirModule(), "Main");
        bool ordered = false;
        for (const MirBlock* block : function->blocks())
        {
            for (std::size_t index = 1; index < block->instructions().size(); ++index)
            {
                if (block->instructions()[index]->opcode() == MirOpcode::AllocateArray &&
                    block->instructions()[index - 1]->opcode() == MirOpcode::Constant)
                {
                    ordered = true;
                }
            }
        }
        tests.expect(ordered, "new[] evaluates its count before emitting alloc.array");
        tests.expect(findInstruction(function, MirOpcode::DeallocateArray) != nullptr,
            "delete[] becomes free.array");
    }

    {
        FrontendCompilation compilation;
        const char* source = "int F(int x); int F(int value) { return value; } int Main() { return F(1); }";
        tests.expect(compile(source, compilation), "prototype plus definition lowers through MIR");
        int functionCount = 0;
        const MirFunction* f = nullptr;
        for (const MirFunction* function : compilation.mirModule()->functions())
        {
            if (function->symbol()->name() == "F")
            {
                ++functionCount;
                f = function;
            }
        }
        tests.expect(functionCount == 1, "prototype and definition coalesce into one MIR function");
        tests.expect(f != nullptr && f->defined() && !f->blocks().empty(),
            "coalesced MIR function carries the definition body");
    }

    {
        FrontendCompilation compilation;
        tests.expect(compile("int Counter = 42; int Main() { return Counter; }", compilation),
            "mutable global initializer lowers through MIR");
        tests.expect(compilation.mirModule() != nullptr && compilation.mirModule()->globals().size() == 1,
            "MIR module records one global");
        if (compilation.mirModule() != nullptr && !compilation.mirModule()->globals().empty())
        {
            tests.expect(compilation.mirModule()->globals().front()->hasExplicitInitializer(),
                "mutable global retains its static constant initializer in MIR");
        }
    }


    {
        FrontendCompilation compilation;
        const char* source =
            "int Marker = 0; "
            "int NextIndex() { Marker = Marker * 10 + 1; return 1; } "
            "int* NextPointer(int* p) { Marker = Marker * 10 + 2; return p; } "
            "int Main() { int value = 0; int* p = NextIndex() + NextPointer(&value); return Marker; }";
        tests.expect(compile(source, compilation), "integer plus pointer lowers through MIR");
        const MirFunction* function = findFunction(compilation.mirModule(), "Main");
        const MirInstruction* pointerAdd = findInstruction(function, MirOpcode::PointerAdd);
        tests.expect(pointerAdd != nullptr && pointerAdd->operands().size() == 2,
            "integer plus pointer becomes canonical ptr.add(pointer, index)");
        if (pointerAdd != nullptr && pointerAdd->operands().size() == 2)
        {
            tests.expect(pointerAdd->operands()[0]->type()->kind() == nativesharp::sema::TypeKind::Pointer &&
                    pointerAdd->operands()[1]->type()->kind() == nativesharp::sema::TypeKind::Int,
                "ptr.add stores pointer base before integer index regardless of source operand order");
        }

        std::vector<std::string> callOrder;
        if (function != nullptr)
        {
            for (const MirBlock* block : function->blocks())
            {
                for (const MirInstruction* instruction : block->instructions())
                {
                    if (instruction->opcode() == MirOpcode::Call && instruction->symbol() != nullptr)
                    {
                        callOrder.push_back(instruction->symbol()->name());
                    }
                }
            }
        }
        tests.expect(callOrder.size() >= 2 && callOrder[0] == "NextIndex" && callOrder[1] == "NextPointer",
            "integer plus pointer preserves left-to-right source evaluation before canonical operand placement");
    }


    {
        FrontendCompilation compilation;
        const char* source = "int SideEffect() { return 7; } int Main() { (void)SideEffect(); return 0; }";
        tests.expect(compile(source, compilation), "cast-to-void expression lowers through MIR");
        const MirFunction* function = findFunction(compilation.mirModule(), "Main");
        tests.expect(countOpcode(function, MirOpcode::Call) == 1,
            "cast-to-void preserves evaluation of its side-effecting operand");
        tests.expect(countOpcode(function, MirOpcode::Convert) == 0,
            "cast-to-void does not materialize a meaningless void MIR value");
    }

    {
        FrontendCompilation compilation;
        const char* source =
            "int Main() { int x = 0; bool choose = true; "
            "if (choose) { x = 1; } else { x = 2; } return x; }";
        tests.expect(compile(source, compilation), "mutable local across branches lowers through MIR");
        const MirFunction* function = findFunction(compilation.mirModule(), "Main");
        tests.expect(countStoreTo(function, "x") == 3,
            "pre-SSA MIR preserves initializer plus both branch writes to the same mutable symbol");
        tests.expect(countOpcode(function, MirOpcode::LoadSymbol) >= 2,
            "pre-SSA MIR reads mutable symbols explicitly instead of inventing PHI nodes in stage 12");
    }

    {
        FrontendCompilation compilation;
        tests.expect(compile("int Main() { return 1; int unreachable = 2; }", compilation),
            "source statements after terminator remain representable in MIR");
        const MirFunction* function = findFunction(compilation.mirModule(), "Main");
        const MirBlock* unreachable = findBlock(function, "unreachable.code");
        tests.expect(unreachable != nullptr,
            "lowering preserves post-terminator source in a detached unreachable.code block");
        tests.expect(unreachable != nullptr && unreachable->terminated() &&
                unreachable->instructions().back()->opcode() == MirOpcode::Unreachable,
            "detached unreachable source block receives an explicit unreachable terminator");
    }

    {
        FrontendCompilation compilation;
        tests.expect(compile("int Main() { int value = 0; int* pointer = &value; pointer++; return 0; }", compilation),
            "pointer postfix increment lowers through MIR");
        const MirFunction* function = findFunction(compilation.mirModule(), "Main");
        const MirInstruction* increment = findInstruction(function, MirOpcode::PointerAdd);
        tests.expect(increment != nullptr && increment->hasImmediate() && increment->immediate() == 1 &&
                increment->operands().size() == 1,
            "pointer increment uses canonical immediate element step instead of fake pointer constant");
    }


    {
        FrontendCompilation compilation;
        const char* source =
            "int Main() { int x = 1; { int x = 2; x = 3; } return x; }";
        tests.expect(compile(source, compilation), "shadowed locals lower through MIR");
        std::ostringstream output;
        nativesharp::mir::MirDumper dumper;
        dumper.dump(compilation.mirModule(), output);
        tests.expect(output.str().find("@x#0:int") != std::string::npos &&
                output.str().find("@x#1:int") != std::string::npos,
            "MIR dump assigns distinct local slot suffixes to shadowed symbols");
    }

    return tests.result();
}
