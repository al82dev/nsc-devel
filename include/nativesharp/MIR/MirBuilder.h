#pragma once

#include "nativesharp/MIR/MirArena.h"
#include "nativesharp/Sema/ConstantValue.h"
#include "nativesharp/Sema/ConversionKind.h"

#include <string>
#include <vector>

namespace nativesharp::hir
{
class HirNode;
}

namespace nativesharp::sema
{
class Symbol;
class Type;
}

namespace nativesharp::mir
{
class MirBlock;
class MirFunction;
class MirInstruction;
class MirModule;
class MirValue;

class MirBuilder final
{
public:
    explicit MirBuilder(MirArena& arena);

    MirModule* build(const hir::HirNode* root);
    bool succeeded() const noexcept;
    const std::string& error() const noexcept;

private:
    struct AddressRef
    {
        sema::Symbol* symbol = nullptr;
        MirValue* pointer = nullptr;
        sema::Type* pointerType = nullptr;
        sema::Type* pointeeType = nullptr;
    };

    struct LoopTargets
    {
        MirBlock* breakTarget = nullptr;
        MirBlock* continueTarget = nullptr;
    };

    bool lowerGlobal(const hir::HirNode* node, MirModule* module);
    bool lowerFunction(const hir::HirNode* node, MirModule* module);
    bool lowerBlock(const hir::HirNode* node);
    bool lowerStatement(const hir::HirNode* node);
    bool lowerIf(const hir::HirNode* node);
    bool lowerWhile(const hir::HirNode* node);
    bool lowerFor(const hir::HirNode* node);

    MirValue* lowerExpression(const hir::HirNode* node);
    MirValue* lowerShortCircuit(const hir::HirNode* node);
    MirValue* lowerCompoundAssignment(const hir::HirNode* node);
    MirValue* lowerIncrement(const hir::HirNode* node);

    AddressRef lowerAddress(const hir::HirNode* node);
    MirValue* materializeAddress(const AddressRef& address, SourceRange range);
    MirValue* loadAddress(const AddressRef& address, SourceRange range);
    bool storeAddress(const AddressRef& address, MirValue* value, SourceRange range);

    MirValue* emitConstant(sema::Type* type, sema::ConstantValue value, SourceRange range);
    MirValue* emitConvert(
        MirValue* value,
        sema::Type* destinationType,
        sema::ConversionKind kind,
        SourceRange range);
    MirValue* emitUnary(MirOpcode opcode, MirValue* operand, sema::Type* type, SourceRange range);
    MirValue* emitBinary(
        MirOpcode opcode,
        MirValue* left,
        MirValue* right,
        sema::Type* type,
        SourceRange range);
    MirInstruction* emit(MirOpcode opcode, SourceRange range);
    MirBlock* createBlock(const std::string& stem);
    void setCurrentBlock(MirBlock* block) noexcept;
    bool emitJump(MirBlock* target, SourceRange range);
    bool emitBranch(MirValue* condition, MirBlock* trueTarget, MirBlock* falseTarget, SourceRange range);
    MirOpcode mapUnaryOpcode(const hir::HirNode* node) const;
    MirOpcode mapBinaryOpcode(const hir::HirNode* node) const;
    sema::ConstantValue oneValue(sema::Type* type) const;
    bool isAddressKind(const hir::HirNode* node) const noexcept;
    bool ensureWritableBlock(SourceRange range);
    bool fail(std::string message);

    MirArena& arena_;
    MirModule* module_ = nullptr;
    MirFunction* function_ = nullptr;
    MirBlock* currentBlock_ = nullptr;
    std::vector<LoopTargets> loopStack_;
    std::string error_;
};
}
