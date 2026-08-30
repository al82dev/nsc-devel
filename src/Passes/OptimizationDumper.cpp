#include "nativesharp/Passes/OptimizationDumper.h"

#include "nativesharp/MIR/MirBlock.h"
#include "nativesharp/MIR/MirFunction.h"
#include "nativesharp/MIR/MirGlobal.h"
#include "nativesharp/MIR/MirInstruction.h"
#include "nativesharp/MIR/MirModule.h"
#include "nativesharp/MIR/MirOpcode.h"
#include "nativesharp/MIR/MirValue.h"
#include "nativesharp/Passes/OptimizationModule.h"
#include "nativesharp/Sema/ConstantValue.h"
#include "nativesharp/Sema/ConversionKind.h"
#include "nativesharp/Sema/Symbol.h"
#include "nativesharp/Sema/Type.h"

#include <iomanip>
#include <ostream>
#include <string>

namespace nativesharp::passes
{
namespace
{
void dumpEscapedString(
    const std::string& value,
    std::ostream& output)
{
    output << '"';
    for (char rawCharacter : value)
    {
        const unsigned char character =
            static_cast<unsigned char>(rawCharacter);
        switch (character)
        {
            case '\\': output << "\\\\"; break;
            case '"': output << "\\\""; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            case '\0': output << "\\0"; break;
            default:
                if (character >= 0x20 && character <= 0x7E)
                {
                    output << static_cast<char>(character);
                }
                else
                {
                    const std::ios_base::fmtflags flags = output.flags();
                    const char fill = output.fill();
                    output << "\\x" << std::hex << std::uppercase
                           << std::setw(2) << std::setfill('0')
                           << static_cast<unsigned int>(character);
                    output.flags(flags);
                    output.fill(fill);
                }
                break;
        }
    }
    output << '"';
}

void dumpConstant(
    const sema::ConstantValue& value,
    std::ostream& output)
{
    switch (value.kind())
    {
        case sema::ConstantKind::None:
            output << "<none>";
            break;
        case sema::ConstantKind::SignedInteger:
            output << value.signedInteger();
            break;
        case sema::ConstantKind::UnsignedInteger:
            output << value.unsignedInteger() << 'u';
            break;
        case sema::ConstantKind::Floating:
            output << std::setprecision(17) << value.floating();
            break;
        case sema::ConstantKind::Boolean:
            output << (value.boolean() ? "true" : "false");
            break;
        case sema::ConstantKind::Null:
            output << "null";
            break;
        case sema::ConstantKind::String:
            dumpEscapedString(value.string(), output);
            break;
    }
}

void dumpSymbol(
    const sema::Symbol* symbol,
    const mir::MirFunction* function,
    std::ostream& output)
{
    output << '@' << symbol->name();
    if (function == nullptr)
    {
        return;
    }

    const std::vector<sema::Symbol*>& locals =
        function->locals();
    for (std::size_t index = 0; index < locals.size(); ++index)
    {
        if (locals[index] == symbol)
        {
            output << '#' << index;
            return;
        }
    }
}
}

void OptimizationDumper::dump(
    const OptimizationModule* module,
    std::ostream& output) const
{
    if (module == nullptr || module->module() == nullptr)
    {
        return;
    }

    output << "optimized.ssa.module\n";
    for (const mir::MirGlobal* global :
         module->module()->globals())
    {
        output << "global @" << global->symbol()->name()
               << " : " << global->symbol()->type()->displayName();
        if (global->hasExplicitInitializer())
        {
            output << " = ";
            dumpConstant(global->initializer(), output);
        }
        else
        {
            output << " = zeroinit";
        }
        output << '\n';
    }

    for (const mir::MirFunction* function :
         module->module()->functions())
    {
        dumpFunction(function, output);
    }
}

void OptimizationDumper::dumpFunction(
    const mir::MirFunction* function,
    std::ostream& output) const
{
    output << "function @" << function->symbol()->name()
           << " : " << function->symbol()->type()->displayName();
    if (function->symbol()->isExternC())
    {
        output << " extern-C";
    }
    if (!function->defined())
    {
        output << " prototype\n";
        return;
    }
    output << '\n';

    for (const mir::MirBlock* block : function->blocks())
    {
        dumpBlock(block, function, output);
    }
}

void OptimizationDumper::dumpBlock(
    const mir::MirBlock* block,
    const mir::MirFunction* function,
    std::ostream& output) const
{
    output << "bb" << block->id() << ' ' << block->name() << ":\n";
    for (const mir::MirInstruction* instruction :
         block->instructions())
    {
        output << "  ";
        dumpInstruction(instruction, function, output);
        output << '\n';
    }
}

void OptimizationDumper::dumpInstruction(
    const mir::MirInstruction* instruction,
    const mir::MirFunction* function,
    std::ostream& output) const
{
    if (instruction->result() != nullptr)
    {
        dumpValue(instruction->result(), output);
        output << " = ";
    }

    output << mir::mirOpcodeName(instruction->opcode());

    if (instruction->symbol() != nullptr)
    {
        output << ' ';
        dumpSymbol(instruction->symbol(), function, output);
    }
    if (instruction->auxiliaryType() != nullptr)
    {
        output << " <"
               << instruction->auxiliaryType()->displayName()
               << '>';
    }
    if (instruction->hasConstant())
    {
        output << ' ';
        dumpConstant(instruction->constant(), output);
    }
    if (instruction->hasConversion())
    {
        output << " ["
               << sema::ToString(instruction->conversionKind())
               << ']';
    }
    if (instruction->hasImmediate())
    {
        output << " imm=" << instruction->immediate();
    }
    if (instruction->hasOffset())
    {
        output << " offset=" << instruction->offset();
    }

    if (instruction->opcode() == mir::MirOpcode::Phi)
    {
        for (const mir::MirPhiIncoming& incoming :
             instruction->phiIncoming())
        {
            output << " [bb" << incoming.block->id() << ": ";
            dumpValue(incoming.value, output);
            output << ']';
        }
    }
    else
    {
        for (const mir::MirValue* operand :
             instruction->operands())
        {
            output << ' ';
            dumpValue(operand, output);
        }
    }

    if (instruction->firstTarget() != nullptr)
    {
        output << " -> bb" << instruction->firstTarget()->id();
    }
    if (instruction->secondTarget() != nullptr)
    {
        output << ", bb" << instruction->secondTarget()->id();
    }
}

void OptimizationDumper::dumpValue(
    const mir::MirValue* value,
    std::ostream& output) const
{
    output << "%v" << value->id()
           << ':' << value->type()->displayName();
}
}
