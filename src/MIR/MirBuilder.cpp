#include "nativesharp/MIR/MirBuilder.h"

#include "nativesharp/HIR/HirNode.h"
#include "nativesharp/HIR/HirNodeKind.h"
#include "nativesharp/Lexer/TokenKind.h"
#include "nativesharp/MIR/MirBlock.h"
#include "nativesharp/MIR/MirFunction.h"
#include "nativesharp/MIR/MirGlobal.h"
#include "nativesharp/MIR/MirInstruction.h"
#include "nativesharp/MIR/MirModule.h"
#include "nativesharp/MIR/MirValue.h"
#include "nativesharp/Sema/Symbol.h"
#include "nativesharp/Sema/SymbolKind.h"
#include "nativesharp/Sema/Type.h"
#include "nativesharp/Sema/TypeKind.h"

#include <utility>

namespace nativesharp::mir
{
namespace
{
bool isVoid(const sema::Type* type) noexcept
{
    return type != nullptr && type->kind() == sema::TypeKind::Void;
}

bool isPointer(const sema::Type* type) noexcept
{
    return type != nullptr && type->kind() == sema::TypeKind::Pointer;
}
}

MirBuilder::MirBuilder(MirArena& arena) : arena_(arena) {}

MirModule* MirBuilder::build(const hir::HirNode* root)
{
    if (root == nullptr || root->kind() != hir::HirNodeKind::CompilationUnit)
    {
        fail("MIR builder requires a CompilationUnit HIR root");
        return nullptr;
    }

    module_ = arena_.makeModule();
    for (const hir::HirNode* child : root->children())
    {
        if (!lowerGlobal(child, module_))
        {
            return nullptr;
        }
    }
    return module_;
}

bool MirBuilder::succeeded() const noexcept { return error_.empty(); }
const std::string& MirBuilder::error() const noexcept { return error_; }

bool MirBuilder::lowerGlobal(const hir::HirNode* node, MirModule* module)
{
    switch (node->kind())
    {
        case hir::HirNodeKind::StructDeclaration:
        case hir::HirNodeKind::EnumDeclaration:
            return true;

        case hir::HirNodeKind::GlobalVariableDeclaration:
        {
            if (node->symbol() == nullptr)
            {
                return fail("global HIR node has no symbol");
            }
            MirGlobal* global = arena_.makeGlobal(node->symbol());
            if (node->first() != nullptr)
            {
                if (!node->symbol()->hasConstantValue())
                {
                    return fail("global initializer reached MIR without semantic constant value");
                }
                global->setExplicitInitializer(node->symbol()->constantValue());
            }
            module->addGlobal(global);
            return true;
        }

        case hir::HirNodeKind::FunctionDeclaration:
            return lowerFunction(node, module);

        default:
            return fail("unsupported HIR global reached MIR builder");
    }
}

bool MirBuilder::lowerFunction(const hir::HirNode* node, MirModule* module)
{
    if (node->symbol() == nullptr || node->symbol()->kind() != sema::SymbolKind::Function)
    {
        return fail("HIR function has no function symbol");
    }

    function_ = module->findFunction(node->symbol());
    if (function_ == nullptr)
    {
        function_ = arena_.makeFunction(node->symbol());
        module->addFunction(function_);
    }
    currentBlock_ = nullptr;
    loopStack_.clear();

    if (node->first() == nullptr)
    {
        return true;
    }
    if (function_->defined())
    {
        return fail("duplicate defined HIR function reached MIR builder");
    }
    function_->setDefined(true);

    setCurrentBlock(createBlock("entry"));
    for (std::size_t index = 0; index < node->children().size(); ++index)
    {
        const hir::HirNode* parameterNode = node->children()[index];
        if (parameterNode->symbol() == nullptr)
        {
            return fail("defined function parameter has no runtime symbol");
        }
        MirValue* incoming = arena_.makeValue(parameterNode->type());
        function_->addParameter(incoming);
        function_->addLocal(parameterNode->symbol());

        MirInstruction* parameter = emit(MirOpcode::Parameter, parameterNode->range());
        parameter->setResult(incoming);
        parameter->setSymbol(parameterNode->symbol());
        parameter->setImmediate(static_cast<std::uint64_t>(index));

        MirInstruction* store = emit(MirOpcode::StoreSymbol, parameterNode->range());
        store->setSymbol(parameterNode->symbol());
        store->addOperand(incoming);
    }

    if (!lowerBlock(node->first()))
    {
        return false;
    }

    if (currentBlock_ != nullptr && !currentBlock_->terminated())
    {
        sema::Type* returnType = node->symbol()->type()->returnType();
        if (isVoid(returnType))
        {
            emit(MirOpcode::Return, node->range());
        }
        else
        {
            emit(MirOpcode::Unreachable, node->range());
        }
    }
    return true;
}

bool MirBuilder::lowerBlock(const hir::HirNode* node)
{
    if (node == nullptr || node->kind() != hir::HirNodeKind::Block)
    {
        return fail("MIR block lowering requires a HIR Block node");
    }

    for (const hir::HirNode* statement : node->children())
    {
        if (!ensureWritableBlock(statement->range()) || !lowerStatement(statement))
        {
            return false;
        }
    }
    return true;
}

bool MirBuilder::lowerStatement(const hir::HirNode* node)
{
    switch (node->kind())
    {
        case hir::HirNodeKind::Block:
            return lowerBlock(node);

        case hir::HirNodeKind::LocalVariableDeclaration:
        {
            if (node->symbol() == nullptr)
            {
                return fail("local HIR declaration has no symbol");
            }
            function_->addLocal(node->symbol());
            if (node->first() != nullptr)
            {
                MirValue* initializer = lowerExpression(node->first());
                if (initializer == nullptr)
                {
                    return false;
                }
                MirInstruction* store = emit(MirOpcode::StoreSymbol, node->range());
                store->setSymbol(node->symbol());
                store->addOperand(initializer);
            }
            return true;
        }

        case hir::HirNodeKind::ExpressionStatement:
            if (node->first() == nullptr)
            {
                return fail("HIR expression statement has no expression");
            }
            static_cast<void>(lowerExpression(node->first()));
            return error_.empty();

        case hir::HirNodeKind::If:
            return lowerIf(node);
        case hir::HirNodeKind::While:
            return lowerWhile(node);
        case hir::HirNodeKind::For:
            return lowerFor(node);

        case hir::HirNodeKind::Break:
            if (loopStack_.empty())
            {
                return fail("HIR break reached MIR without loop target");
            }
            return emitJump(loopStack_.back().breakTarget, node->range());

        case hir::HirNodeKind::Continue:
            if (loopStack_.empty())
            {
                return fail("HIR continue reached MIR without loop target");
            }
            return emitJump(loopStack_.back().continueTarget, node->range());

        case hir::HirNodeKind::Return:
        {
            MirValue* value = nullptr;
            if (node->first() != nullptr)
            {
                value = lowerExpression(node->first());
                if (value == nullptr)
                {
                    return false;
                }
            }
            MirInstruction* instruction = emit(MirOpcode::Return, node->range());
            if (instruction == nullptr)
            {
                return false;
            }
            if (value != nullptr)
            {
                instruction->addOperand(value);
            }
            return true;
        }

        default:
            return fail("unsupported HIR statement reached MIR builder");
    }
}

bool MirBuilder::lowerIf(const hir::HirNode* node)
{
    MirValue* condition = lowerExpression(node->first());
    if (condition == nullptr)
    {
        return false;
    }

    MirBlock* thenBlock = createBlock("if.then");
    MirBlock* elseBlock = node->third() != nullptr ? createBlock("if.else") : nullptr;
    MirBlock* mergeBlock = createBlock("if.end");
    if (!emitBranch(condition, thenBlock, elseBlock != nullptr ? elseBlock : mergeBlock, node->range()))
    {
        return false;
    }

    setCurrentBlock(thenBlock);
    if (!lowerBlock(node->second()))
    {
        return false;
    }
    if (!currentBlock_->terminated() && !emitJump(mergeBlock, node->range()))
    {
        return false;
    }

    if (elseBlock != nullptr)
    {
        setCurrentBlock(elseBlock);
        if (node->third()->kind() == hir::HirNodeKind::Block)
        {
            if (!lowerBlock(node->third()))
            {
                return false;
            }
        }
        else if (!lowerStatement(node->third()))
        {
            return false;
        }
        if (!currentBlock_->terminated() && !emitJump(mergeBlock, node->range()))
        {
            return false;
        }
    }

    setCurrentBlock(mergeBlock);
    return true;
}

bool MirBuilder::lowerWhile(const hir::HirNode* node)
{
    MirBlock* conditionBlock = createBlock("while.cond");
    MirBlock* bodyBlock = createBlock("while.body");
    MirBlock* exitBlock = createBlock("while.end");
    if (!emitJump(conditionBlock, node->range()))
    {
        return false;
    }

    setCurrentBlock(conditionBlock);
    MirValue* condition = lowerExpression(node->first());
    if (condition == nullptr || !emitBranch(condition, bodyBlock, exitBlock, node->range()))
    {
        return false;
    }

    loopStack_.push_back({exitBlock, conditionBlock});
    setCurrentBlock(bodyBlock);
    const bool bodyOk = lowerBlock(node->second());
    if (bodyOk && !currentBlock_->terminated())
    {
        emitJump(conditionBlock, node->range());
    }
    loopStack_.pop_back();
    if (!bodyOk || !error_.empty())
    {
        return false;
    }

    setCurrentBlock(exitBlock);
    return true;
}

bool MirBuilder::lowerFor(const hir::HirNode* node)
{
    if (node->first() != nullptr)
    {
        if (node->first()->kind() == hir::HirNodeKind::LocalVariableDeclaration)
        {
            if (!lowerStatement(node->first()))
            {
                return false;
            }
        }
        else
        {
            static_cast<void>(lowerExpression(node->first()));
            if (!error_.empty())
            {
                return false;
            }
        }
    }

    MirBlock* conditionBlock = createBlock("for.cond");
    MirBlock* bodyBlock = createBlock("for.body");
    MirBlock* incrementBlock = createBlock("for.inc");
    MirBlock* exitBlock = createBlock("for.end");
    if (!emitJump(conditionBlock, node->range()))
    {
        return false;
    }

    setCurrentBlock(conditionBlock);
    if (node->second() != nullptr)
    {
        MirValue* condition = lowerExpression(node->second());
        if (condition == nullptr || !emitBranch(condition, bodyBlock, exitBlock, node->range()))
        {
            return false;
        }
    }
    else if (!emitJump(bodyBlock, node->range()))
    {
        return false;
    }

    loopStack_.push_back({exitBlock, incrementBlock});
    setCurrentBlock(bodyBlock);
    const bool bodyOk = !node->children().empty() && lowerBlock(node->children().front());
    if (bodyOk && !currentBlock_->terminated())
    {
        emitJump(incrementBlock, node->range());
    }
    loopStack_.pop_back();
    if (!bodyOk || !error_.empty())
    {
        return false;
    }

    setCurrentBlock(incrementBlock);
    if (node->third() != nullptr)
    {
        static_cast<void>(lowerExpression(node->third()));
        if (!error_.empty())
        {
            return false;
        }
    }
    if (!currentBlock_->terminated() && !emitJump(conditionBlock, node->range()))
    {
        return false;
    }

    setCurrentBlock(exitBlock);
    return true;
}

MirValue* MirBuilder::lowerExpression(const hir::HirNode* node)
{
    if (node == nullptr)
    {
        fail("null HIR expression reached MIR builder");
        return nullptr;
    }

    switch (node->kind())
    {
        case hir::HirNodeKind::Constant:
            return emitConstant(node->type(), node->constant(), node->range());

        case hir::HirNodeKind::SymbolAddress:
            return materializeAddress(lowerAddress(node), node->range());

        case hir::HirNodeKind::Load:
            return loadAddress(lowerAddress(node->first()), node->range());

        case hir::HirNodeKind::Store:
        {
            AddressRef address = lowerAddress(node->first());
            if (!error_.empty())
            {
                return nullptr;
            }
            MirValue* value = lowerExpression(node->second());
            if (value == nullptr || !storeAddress(address, value, node->range()))
            {
                return nullptr;
            }
            return value;
        }

        case hir::HirNodeKind::Conversion:
        {
            MirValue* value = lowerExpression(node->first());
            if (value == nullptr)
            {
                return nullptr;
            }
            return emitConvert(value, node->type(), node->conversionKind(), node->range());
        }

        case hir::HirNodeKind::Unary:
        {
            MirValue* operand = lowerExpression(node->first());
            if (operand == nullptr)
            {
                return nullptr;
            }
            const MirOpcode opcode = mapUnaryOpcode(node);
            if (!error_.empty())
            {
                return nullptr;
            }
            if (opcode == MirOpcode::Copy)
            {
                return operand;
            }
            return emitUnary(opcode, operand, node->type(), node->range());
        }

        case hir::HirNodeKind::Binary:
            if (node->operation() == lexer::TokenKind::AmpAmp || node->operation() == lexer::TokenKind::PipePipe)
            {
                return lowerShortCircuit(node);
            }
            else
            {
                MirValue* left = lowerExpression(node->first());
                MirValue* right = left != nullptr ? lowerExpression(node->second()) : nullptr;
                if (left == nullptr || right == nullptr)
                {
                    return nullptr;
                }
                const MirOpcode opcode = mapBinaryOpcode(node);
                if (!error_.empty())
                {
                    return nullptr;
                }
                if (opcode == MirOpcode::PointerAdd && !isPointer(left->type()) && isPointer(right->type()))
                {
                    return emitBinary(opcode, right, left, node->type(), node->range());
                }
                return emitBinary(opcode, left, right, node->type(), node->range());
            }

        case hir::HirNodeKind::Call:
        {
            std::vector<MirValue*> arguments;
            arguments.reserve(node->children().size());
            for (const hir::HirNode* argument : node->children())
            {
                MirValue* value = lowerExpression(argument);
                if (value == nullptr)
                {
                    return nullptr;
                }
                arguments.push_back(value);
            }
            MirInstruction* call = emit(MirOpcode::Call, node->range());
            call->setSymbol(node->symbol());
            for (MirValue* argument : arguments)
            {
                call->addOperand(argument);
            }
            if (isVoid(node->type()))
            {
                return nullptr;
            }
            MirValue* result = arena_.makeValue(node->type());
            call->setResult(result);
            return result;
        }

        case hir::HirNodeKind::IndexAddress:
        case hir::HirNodeKind::FieldAddress:
        case hir::HirNodeKind::DereferenceAddress:
            return materializeAddress(lowerAddress(node), node->range());

        case hir::HirNodeKind::ExtractField:
        {
            MirValue* base = lowerExpression(node->first());
            if (base == nullptr)
            {
                return nullptr;
            }
            MirInstruction* instruction = emit(MirOpcode::ExtractField, node->range());
            MirValue* result = arena_.makeValue(node->type());
            instruction->setResult(result);
            instruction->setSymbol(node->symbol());
            instruction->setOffset(node->offset());
            instruction->addOperand(base);
            return result;
        }

        case hir::HirNodeKind::CompoundAssignment:
            return lowerCompoundAssignment(node);
        case hir::HirNodeKind::Increment:
            return lowerIncrement(node);

        case hir::HirNodeKind::Allocation:
        {
            MirValue* count = nullptr;
            if (node->first() != nullptr)
            {
                count = lowerExpression(node->first());
                if (count == nullptr)
                {
                    return nullptr;
                }
            }
            MirInstruction* instruction = emit(
                node->arrayForm() ? MirOpcode::AllocateArray : MirOpcode::Allocate,
                node->range());
            instruction->setAuxiliaryType(node->operationType());
            if (count != nullptr)
            {
                instruction->addOperand(count);
            }
            MirValue* result = arena_.makeValue(node->type());
            instruction->setResult(result);
            return result;
        }

        case hir::HirNodeKind::Deallocation:
        {
            MirValue* pointer = lowerExpression(node->first());
            if (pointer == nullptr)
            {
                return nullptr;
            }
            MirInstruction* instruction = emit(
                node->arrayForm() ? MirOpcode::DeallocateArray : MirOpcode::Deallocate,
                node->range());
            instruction->addOperand(pointer);
            return nullptr;
        }

        case hir::HirNodeKind::StackAllocation:
        {
            MirValue* count = lowerExpression(node->first());
            if (count == nullptr)
            {
                return nullptr;
            }
            MirInstruction* instruction = emit(MirOpcode::StackAllocate, node->range());
            MirValue* result = arena_.makeValue(node->type());
            instruction->setResult(result);
            instruction->setAuxiliaryType(node->operationType());
            instruction->addOperand(count);
            return result;
        }

        default:
            fail("unsupported HIR expression reached MIR builder");
            return nullptr;
    }
}

MirValue* MirBuilder::lowerShortCircuit(const hir::HirNode* node)
{
    MirValue* left = lowerExpression(node->first());
    if (left == nullptr)
    {
        return nullptr;
    }

    MirValue* result = arena_.makeValue(node->type());
    MirBlock* rhsBlock = createBlock("logic.rhs");
    MirBlock* shortBlock = createBlock("logic.short");
    MirBlock* mergeBlock = createBlock("logic.end");

    const bool isAnd = node->operation() == lexer::TokenKind::AmpAmp;
    if (!emitBranch(left, isAnd ? rhsBlock : shortBlock, isAnd ? shortBlock : rhsBlock, node->range()))
    {
        return nullptr;
    }

    setCurrentBlock(shortBlock);
    MirInstruction* constant = emit(MirOpcode::Constant, node->range());
    constant->setResult(result);
    constant->setConstant(sema::ConstantValue::boolean(!isAnd));
    if (!emitJump(mergeBlock, node->range()))
    {
        return nullptr;
    }

    setCurrentBlock(rhsBlock);
    MirValue* right = lowerExpression(node->second());
    if (right == nullptr)
    {
        return nullptr;
    }
    MirInstruction* copy = emit(MirOpcode::Copy, node->range());
    copy->setResult(result);
    copy->addOperand(right);
    if (!emitJump(mergeBlock, node->range()))
    {
        return nullptr;
    }

    setCurrentBlock(mergeBlock);
    return result;
}

MirValue* MirBuilder::lowerCompoundAssignment(const hir::HirNode* node)
{
    AddressRef address = lowerAddress(node->first());
    if (!error_.empty())
    {
        return nullptr;
    }

    MirValue* current = loadAddress(address, node->range());
    if (current == nullptr)
    {
        return nullptr;
    }
    if (node->hasReadConversion())
    {
        current = emitConvert(current, node->operationType(), node->readConversionKind(), node->range());
        if (current == nullptr)
        {
            return nullptr;
        }
    }

    MirValue* right = lowerExpression(node->second());
    if (right == nullptr)
    {
        return nullptr;
    }

    const MirOpcode opcode = mapBinaryOpcode(node);
    if (!error_.empty())
    {
        return nullptr;
    }
    MirValue* operationResult = emitBinary(opcode, current, right, node->operationType(), node->range());
    MirValue* storedValue = operationResult;
    if (node->hasResultConversion())
    {
        storedValue = emitConvert(operationResult, node->type(), node->resultConversionKind(), node->range());
        if (storedValue == nullptr)
        {
            return nullptr;
        }
    }
    if (!storeAddress(address, storedValue, node->range()))
    {
        return nullptr;
    }
    return storedValue;
}

MirValue* MirBuilder::lowerIncrement(const hir::HirNode* node)
{
    AddressRef address = lowerAddress(node->first());
    if (!error_.empty())
    {
        return nullptr;
    }

    MirValue* oldValue = loadAddress(address, node->range());
    if (oldValue == nullptr)
    {
        return nullptr;
    }
    MirValue* newValue = nullptr;
    if (isPointer(node->operationType()))
    {
        const MirOpcode opcode = node->operation() == lexer::TokenKind::PlusPlus
            ? MirOpcode::PointerAdd
            : MirOpcode::PointerSubtract;
        MirInstruction* instruction = emit(opcode, node->range());
        newValue = arena_.makeValue(node->type());
        instruction->setResult(newValue);
        instruction->addOperand(oldValue);
        instruction->setImmediate(1);
    }
    else
    {
        MirValue* one = emitConstant(node->operationType(), oneValue(node->operationType()), node->range());
        const MirOpcode opcode = node->operation() == lexer::TokenKind::PlusPlus
            ? MirOpcode::Add
            : MirOpcode::Subtract;
        newValue = emitBinary(opcode, oldValue, one, node->type(), node->range());
    }
    if (newValue == nullptr || !storeAddress(address, newValue, node->range()))
    {
        return nullptr;
    }
    return node->postfix() ? oldValue : newValue;
}

MirBuilder::AddressRef MirBuilder::lowerAddress(const hir::HirNode* node)
{
    if (node == nullptr)
    {
        fail("null HIR address reached MIR builder");
        return {};
    }

    if (node->kind() == hir::HirNodeKind::SymbolAddress)
    {
        return {node->symbol(), nullptr, node->type(), node->type()->elementType()};
    }

    if (node->kind() == hir::HirNodeKind::DereferenceAddress)
    {
        MirValue* pointer = lowerExpression(node->first());
        return {nullptr, pointer, node->type(), node->type()->elementType()};
    }

    if (node->kind() == hir::HirNodeKind::FieldAddress)
    {
        MirValue* base = isAddressKind(node->first())
            ? materializeAddress(lowerAddress(node->first()), node->range())
            : lowerExpression(node->first());
        if (base == nullptr)
        {
            return {};
        }
        MirInstruction* instruction = emit(MirOpcode::FieldAddress, node->range());
        MirValue* pointer = arena_.makeValue(node->type());
        instruction->setResult(pointer);
        instruction->setSymbol(node->symbol());
        instruction->setOffset(node->offset());
        instruction->addOperand(base);
        return {nullptr, pointer, node->type(), node->type()->elementType()};
    }

    if (node->kind() == hir::HirNodeKind::IndexAddress)
    {
        MirValue* base = isAddressKind(node->first())
            ? materializeAddress(lowerAddress(node->first()), node->range())
            : lowerExpression(node->first());
        MirValue* index = base != nullptr ? lowerExpression(node->second()) : nullptr;
        if (base == nullptr || index == nullptr)
        {
            return {};
        }

        if (node->requiresBoundsCheck())
        {
            if (node->operationType() == nullptr ||
                (node->operationType()->kind() != sema::TypeKind::Array &&
                 node->operationType()->kind() != sema::TypeKind::StringLiteral))
            {
                fail("HIR bounds-check index has no fixed-length base type");
                return {};
            }
            MirInstruction* check = emit(MirOpcode::BoundsCheck, node->range());
            check->addOperand(index);
            check->setImmediate(node->operationType()->count());
        }

        MirInstruction* instruction = emit(MirOpcode::IndexAddress, node->range());
        MirValue* pointer = arena_.makeValue(node->type());
        instruction->setResult(pointer);
        instruction->setAuxiliaryType(node->operationType());
        instruction->addOperand(base);
        instruction->addOperand(index);
        return {nullptr, pointer, node->type(), node->type()->elementType()};
    }

    fail("HIR expression is not an address form in MIR lowering");
    return {};
}

MirValue* MirBuilder::materializeAddress(const AddressRef& address, SourceRange range)
{
    if (address.pointer != nullptr)
    {
        return address.pointer;
    }
    if (address.symbol == nullptr || address.pointerType == nullptr || address.pointeeType == nullptr)
    {
        fail("invalid MIR address reference");
        return nullptr;
    }

    MirInstruction* instruction = emit(MirOpcode::AddressOfSymbol, range);
    if (instruction == nullptr)
    {
        return nullptr;
    }
    MirValue* result = arena_.makeValue(address.pointerType);
    instruction->setResult(result);
    instruction->setSymbol(address.symbol);
    return result;
}

MirValue* MirBuilder::loadAddress(const AddressRef& address, SourceRange range)
{
    if (address.symbol != nullptr)
    {
        MirInstruction* instruction = emit(MirOpcode::LoadSymbol, range);
        MirValue* result = arena_.makeValue(address.pointeeType);
        instruction->setResult(result);
        instruction->setSymbol(address.symbol);
        return result;
    }

    if (address.pointer == nullptr || address.pointeeType == nullptr)
    {
        fail("invalid MIR load address");
        return nullptr;
    }
    MirInstruction* instruction = emit(MirOpcode::Load, range);
    MirValue* result = arena_.makeValue(address.pointeeType);
    instruction->setResult(result);
    instruction->addOperand(address.pointer);
    return result;
}

bool MirBuilder::storeAddress(const AddressRef& address, MirValue* value, SourceRange range)
{
    if (value == nullptr)
    {
        return fail("cannot store a null MIR value");
    }
    if (address.symbol != nullptr)
    {
        MirInstruction* instruction = emit(MirOpcode::StoreSymbol, range);
        instruction->setSymbol(address.symbol);
        instruction->addOperand(value);
        return true;
    }
    if (address.pointer == nullptr)
    {
        return fail("invalid MIR store address");
    }
    MirInstruction* instruction = emit(MirOpcode::Store, range);
    instruction->addOperand(address.pointer);
    instruction->addOperand(value);
    return true;
}

MirValue* MirBuilder::emitConstant(sema::Type* type, sema::ConstantValue value, SourceRange range)
{
    MirValue* result = arena_.makeValue(type);
    MirInstruction* instruction = emit(MirOpcode::Constant, range);
    instruction->setResult(result);
    instruction->setConstant(std::move(value));
    return result;
}

MirValue* MirBuilder::emitConvert(
    MirValue* value,
    sema::Type* destinationType,
    sema::ConversionKind kind,
    SourceRange range)
{
    if (value == nullptr || destinationType == nullptr)
    {
        fail("invalid MIR conversion input");
        return nullptr;
    }
    if (kind == sema::ConversionKind::Identity)
    {
        return value;
    }
    if (destinationType->kind() == sema::TypeKind::Void)
    {
        return nullptr;
    }
    MirInstruction* instruction = emit(MirOpcode::Convert, range);
    MirValue* result = arena_.makeValue(destinationType);
    instruction->setResult(result);
    instruction->setConversionKind(kind);
    instruction->addOperand(value);
    return result;
}

MirValue* MirBuilder::emitUnary(MirOpcode opcode, MirValue* operand, sema::Type* type, SourceRange range)
{
    MirInstruction* instruction = emit(opcode, range);
    MirValue* result = arena_.makeValue(type);
    instruction->setResult(result);
    instruction->addOperand(operand);
    return result;
}

MirValue* MirBuilder::emitBinary(
    MirOpcode opcode,
    MirValue* left,
    MirValue* right,
    sema::Type* type,
    SourceRange range)
{
    MirInstruction* instruction = emit(opcode, range);
    MirValue* result = arena_.makeValue(type);
    instruction->setResult(result);
    instruction->addOperand(left);
    instruction->addOperand(right);
    return result;
}

MirInstruction* MirBuilder::emit(MirOpcode opcode, SourceRange range)
{
    if (currentBlock_ == nullptr)
    {
        fail("attempted to emit MIR instruction without current block");
        return nullptr;
    }
    if (currentBlock_->terminated())
    {
        fail("attempted to emit MIR instruction after terminator");
        return nullptr;
    }
    MirInstruction* instruction = arena_.makeInstruction(opcode, range);
    currentBlock_->addInstruction(instruction);
    return instruction;
}

MirBlock* MirBuilder::createBlock(const std::string& stem)
{
    if (function_ == nullptr)
    {
        fail("attempted to create MIR block outside function");
        return nullptr;
    }
    MirBlock* block = arena_.makeBlock(stem + "." + std::to_string(function_->blocks().size()));
    function_->addBlock(block);
    return block;
}

void MirBuilder::setCurrentBlock(MirBlock* block) noexcept { currentBlock_ = block; }

bool MirBuilder::emitJump(MirBlock* target, SourceRange range)
{
    MirInstruction* instruction = emit(MirOpcode::Jump, range);
    if (instruction == nullptr || target == nullptr)
    {
        return fail("invalid MIR jump target");
    }
    instruction->setFirstTarget(target);
    return true;
}

bool MirBuilder::emitBranch(
    MirValue* condition,
    MirBlock* trueTarget,
    MirBlock* falseTarget,
    SourceRange range)
{
    MirInstruction* instruction = emit(MirOpcode::Branch, range);
    if (instruction == nullptr || condition == nullptr || trueTarget == nullptr || falseTarget == nullptr)
    {
        return fail("invalid MIR branch");
    }
    instruction->addOperand(condition);
    instruction->setFirstTarget(trueTarget);
    instruction->setSecondTarget(falseTarget);
    return true;
}

MirOpcode MirBuilder::mapUnaryOpcode(const hir::HirNode* node) const
{
    switch (node->operation())
    {
        case lexer::TokenKind::Plus: return MirOpcode::Copy;
        case lexer::TokenKind::Minus: return MirOpcode::Negate;
        case lexer::TokenKind::Bang: return MirOpcode::LogicalNot;
        case lexer::TokenKind::Tilde: return MirOpcode::BitNot;
        default: break;
    }
    const_cast<MirBuilder*>(this)->fail("unsupported HIR unary operator reached MIR");
    return MirOpcode::Copy;
}

MirOpcode MirBuilder::mapBinaryOpcode(const hir::HirNode* node) const
{
    const lexer::TokenKind operation = node->operation();
    const bool pointerOperation = node->operationType() != nullptr &&
        node->operationType()->kind() == sema::TypeKind::Pointer;

    if (operation == lexer::TokenKind::Plus && pointerOperation)
    {
        return MirOpcode::PointerAdd;
    }
    if (operation == lexer::TokenKind::Minus && pointerOperation)
    {
        if (node->kind() == hir::HirNodeKind::Binary && node->first() != nullptr && node->second() != nullptr &&
            isPointer(node->first()->type()) && isPointer(node->second()->type()))
        {
            return MirOpcode::PointerDifference;
        }
        return MirOpcode::PointerSubtract;
    }

    switch (operation)
    {
        case lexer::TokenKind::Plus: return MirOpcode::Add;
        case lexer::TokenKind::Minus: return MirOpcode::Subtract;
        case lexer::TokenKind::Star: return MirOpcode::Multiply;
        case lexer::TokenKind::Slash: return MirOpcode::Divide;
        case lexer::TokenKind::Percent: return MirOpcode::Remainder;
        case lexer::TokenKind::Amp: return MirOpcode::BitAnd;
        case lexer::TokenKind::Pipe: return MirOpcode::BitOr;
        case lexer::TokenKind::Caret: return MirOpcode::BitXor;
        case lexer::TokenKind::LessLess: return MirOpcode::ShiftLeft;
        case lexer::TokenKind::GreaterGreater: return MirOpcode::ShiftRight;
        case lexer::TokenKind::EqualEqual: return MirOpcode::CompareEqual;
        case lexer::TokenKind::BangEqual: return MirOpcode::CompareNotEqual;
        case lexer::TokenKind::Less: return MirOpcode::CompareLess;
        case lexer::TokenKind::LessEqual: return MirOpcode::CompareLessEqual;
        case lexer::TokenKind::Greater: return MirOpcode::CompareGreater;
        case lexer::TokenKind::GreaterEqual: return MirOpcode::CompareGreaterEqual;
        default: break;
    }
    const_cast<MirBuilder*>(this)->fail("unsupported HIR binary operator reached MIR");
    return MirOpcode::Copy;
}

sema::ConstantValue MirBuilder::oneValue(sema::Type* type) const
{
    if (type == nullptr)
    {
        return {};
    }
    switch (type->kind())
    {
        case sema::TypeKind::Float:
        case sema::TypeKind::Double:
            return sema::ConstantValue::floating(1.0);
        case sema::TypeKind::Byte:
        case sema::TypeKind::Char:
        case sema::TypeKind::UShort:
        case sema::TypeKind::UInt:
        case sema::TypeKind::ULong:
        case sema::TypeKind::NUInt:
            return sema::ConstantValue::unsignedInteger(1);
        case sema::TypeKind::Pointer:
        case sema::TypeKind::Short:
        case sema::TypeKind::Int:
        case sema::TypeKind::Long:
        case sema::TypeKind::NInt:
            return sema::ConstantValue::signedInteger(1);
        default:
            return {};
    }
}

bool MirBuilder::isAddressKind(const hir::HirNode* node) const noexcept
{
    if (node == nullptr)
    {
        return false;
    }
    return node->kind() == hir::HirNodeKind::SymbolAddress ||
        node->kind() == hir::HirNodeKind::FieldAddress ||
        node->kind() == hir::HirNodeKind::IndexAddress ||
        node->kind() == hir::HirNodeKind::DereferenceAddress;
}

bool MirBuilder::ensureWritableBlock(SourceRange range)
{
    if (currentBlock_ == nullptr)
    {
        return fail("MIR statement lowering has no current block");
    }
    if (!currentBlock_->terminated())
    {
        return true;
    }
    MirBlock* unreachable = createBlock("unreachable.code");
    if (unreachable == nullptr)
    {
        return false;
    }
    setCurrentBlock(unreachable);
    static_cast<void>(range);
    return true;
}

bool MirBuilder::fail(std::string message)
{
    if (error_.empty())
    {
        error_ = std::move(message);
    }
    return false;
}
}
