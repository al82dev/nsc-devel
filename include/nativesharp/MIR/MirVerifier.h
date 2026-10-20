#pragma once

#include "nativesharp/MIR/MirVerificationMode.h"

#include <string>
#include <unordered_set>

namespace nativesharp::sema
{
class Symbol;
}

namespace nativesharp::mir
{
class MirBlock;
class MirFunction;
class MirInstruction;
class MirModule;
class MirValue;

class MirVerifier final
{
public:
    bool verify(const MirModule* module, MirVerificationMode mode = MirVerificationMode::PreSsa);
    const std::string& error() const noexcept;

private:
    bool verifyFunction(const MirFunction* function);
    bool verifyBlock(const MirBlock* block, const MirFunction* function);
    bool verifyInstruction(const MirInstruction* instruction, const MirFunction* function);
    bool verifyUndefInstruction(const MirInstruction* instruction, const MirFunction* function);
    bool verifyPhiInstruction(const MirInstruction* instruction, const MirFunction* function);
    bool verifyParameterInstruction(const MirInstruction* instruction, const MirFunction* function);
    bool verifySimpleStorageInstruction(
        const MirInstruction* instruction,
        const MirFunction* function);
    bool verifyMemoryInstruction(const MirInstruction* instruction);
    bool verifyUnaryInstruction(const MirInstruction* instruction);
    bool verifyArithmeticInstruction(const MirInstruction* instruction);
    bool verifyComparisonInstruction(const MirInstruction* instruction);
    bool verifyPointerInstruction(const MirInstruction* instruction);
    bool verifyCallInstruction(const MirInstruction* instruction);
    bool verifyAggregateInstruction(const MirInstruction* instruction);
    bool verifyAllocationInstruction(const MirInstruction* instruction);
    bool verifyTerminatorInstruction(
        const MirInstruction* instruction,
        const MirFunction* function);
    bool verifyStorageSymbol(const sema::Symbol* symbol, const MirFunction* function) const noexcept;
    bool verifyTarget(const MirBlock* target, const MirFunction* function);
    bool verifyValue(const MirValue* value) const noexcept;
    bool fail(std::string message);

    MirVerificationMode mode_ = MirVerificationMode::PreSsa;
    std::unordered_set<const sema::Symbol*> moduleSymbols_;
    std::unordered_set<const MirValue*> knownValues_;
    std::string error_;
};
}
