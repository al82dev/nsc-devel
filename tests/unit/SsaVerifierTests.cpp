#include "nativesharp/Frontend/FrontendCompilation.h"
#include "nativesharp/MIR/MirBlock.h"
#include "nativesharp/MIR/MirFunction.h"
#include "nativesharp/MIR/MirInstruction.h"
#include "nativesharp/MIR/MirModule.h"
#include "nativesharp/MIR/MirOpcode.h"
#include "nativesharp/MIR/MirValue.h"
#include "nativesharp/SSA/SsaFunction.h"
#include "nativesharp/SSA/SsaModule.h"
#include "nativesharp/SSA/SsaVerifier.h"
#include "nativesharp/Sema/Symbol.h"
#include "../support/TestContext.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using nativesharp::frontend::FrontendCompilation;
using nativesharp::mir::MirBlock;
using nativesharp::mir::MirFunction;
using nativesharp::mir::MirInstruction;
using nativesharp::mir::MirOpcode;
using nativesharp::mir::MirPhiIncoming;
using nativesharp::mir::MirValue;
using nativesharp::ssa::SsaFunction;
using nativesharp::ssa::SsaModule;

const char* kSource =
    "int Main() {"
    "  int value = 0; int other = 3; int addressed = 7; int* pointer = &addressed;"
    "  if (value == 0) { value = 1; } else { value = 2; }"
    "  return value + *pointer;"
    "}";

bool compile(FrontendCompilation& compilation)
{
    return compilation.compileBuffer(
        std::filesystem::path("ssa_verifier_test.ns"),
        std::string(kSource),
        false);
}

SsaFunction* findFunction(FrontendCompilation& compilation, std::string_view name)
{
    SsaModule* module = const_cast<SsaModule*>(compilation.ssaModule());
    if (module == nullptr)
    {
        return nullptr;
    }
    for (SsaFunction* function : module->functions())
    {
        if (function != nullptr && function->function() != nullptr && function->function()->symbol() != nullptr &&
            function->function()->symbol()->name() == name)
        {
            return function;
        }
    }
    return nullptr;
}

MirInstruction* findPhi(SsaFunction* function)
{
    if (function == nullptr)
    {
        return nullptr;
    }
    for (MirBlock* block : function->function()->blocks())
    {
        for (MirInstruction* instruction : block->instructions())
        {
            if (instruction != nullptr && instruction->opcode() == MirOpcode::Phi)
            {
                return instruction;
            }
        }
    }
    return nullptr;
}

MirInstruction* findInstruction(SsaFunction* function, MirOpcode opcode, std::string_view symbolName)
{
    if (function == nullptr)
    {
        return nullptr;
    }
    for (MirBlock* block : function->function()->blocks())
    {
        for (MirInstruction* instruction : block->instructions())
        {
            if (instruction == nullptr || instruction->opcode() != opcode)
            {
                continue;
            }
            if (symbolName.empty() ||
                (instruction->symbol() != nullptr && instruction->symbol()->name() == symbolName))
            {
                return instruction;
            }
        }
    }
    return nullptr;
}

nativesharp::sema::Symbol* findPromoted(SsaFunction* function, std::string_view name)
{
    if (function == nullptr)
    {
        return nullptr;
    }
    for (nativesharp::sema::Symbol* symbol : function->promotedSymbols())
    {
        if (symbol != nullptr && symbol->name() == name)
        {
            return symbol;
        }
    }
    return nullptr;
}

bool verify(FrontendCompilation& compilation, std::string& error)
{
    nativesharp::ssa::SsaVerifier verifier;
    const bool result = verifier.verify(compilation.ssaModule());
    error = verifier.error();
    return result;
}
}

int main()
{
    using nativesharp::tests::TestContext;
    TestContext tests;

    {
        FrontendCompilation compilation;
        tests.expect(compile(compilation), "valid verifier fixture compiles through SSA");
        std::string error;
        tests.expect(verify(compilation, error), "SSA verifier accepts unmodified builder output");
        tests.expect(error.empty(), "successful SSA verification leaves empty error message");
    }

    {
        FrontendCompilation compilation;
        tests.expect(compile(compilation), "promotion-set corruption fixture compiles");
        SsaFunction* function = findFunction(compilation, "Main");
        MirInstruction* address = findInstruction(function, MirOpcode::AddressOfSymbol, "addressed");
        tests.expect(address != nullptr && address->symbol() != nullptr,
            "address-taken symbol exists for promotion-set corruption test");
        if (function != nullptr && address != nullptr)
        {
            function->addPromotedSymbol(address->symbol());
        }
        std::string error;
        tests.expect(!verify(compilation, error), "verifier rejects corrupted promoted-symbol set");
        tests.expect(error.find("promoted-symbol set") != std::string::npos,
            "promotion-set corruption fails for the intended invariant");
    }

    {
        FrontendCompilation compilation;
        tests.expect(compile(compilation), "duplicate-definition fixture compiles");
        SsaFunction* function = findFunction(compilation, "Main");
        MirInstruction* firstUndef = nullptr;
        MirInstruction* secondUndef = nullptr;
        if (function != nullptr)
        {
            for (MirBlock* block : function->function()->blocks())
            {
                for (MirInstruction* instruction : block->instructions())
                {
                    if (instruction != nullptr && instruction->opcode() == MirOpcode::Undef &&
                        instruction->result() != nullptr &&
                        instruction->result()->type()->displayName() == "int")
                    {
                        if (firstUndef == nullptr)
                        {
                            firstUndef = instruction;
                        }
                        else
                        {
                            secondUndef = instruction;
                            break;
                        }
                    }
                }
                if (secondUndef != nullptr)
                {
                    break;
                }
            }
        }
        tests.expect(firstUndef != nullptr && secondUndef != nullptr,
            "two dead same-typed undef seeds exist for duplicate-definition corruption");
        if (firstUndef != nullptr && secondUndef != nullptr)
        {
            secondUndef->setResult(firstUndef->result());
        }
        std::string error;
        tests.expect(!verify(compilation, error), "verifier rejects SSA value with two definitions");
        tests.expect(error.find("exactly one definition") != std::string::npos,
            "duplicate definition is rejected by SSA single-definition invariant");
    }

    {
        FrontendCompilation compilation;
        tests.expect(compile(compilation), "missing-PHI-incoming fixture compiles");
        SsaFunction* function = findFunction(compilation, "Main");
        MirInstruction* phi = findPhi(function);
        tests.expect(phi != nullptr && phi->phiIncoming().size() == 2,
            "diamond PHI has two incoming pairs before corruption");
        if (phi != nullptr)
        {
            std::vector<MirPhiIncoming>& incoming =
                const_cast<std::vector<MirPhiIncoming>&>(phi->phiIncoming());
            incoming.pop_back();
        }
        std::string error;
        tests.expect(!verify(compilation, error), "verifier rejects PHI missing predecessor input");
        tests.expect(error.find("one incoming value per unique reachable predecessor") != std::string::npos,
            "missing PHI input fails predecessor-completeness invariant");
    }

    {
        FrontendCompilation compilation;
        tests.expect(compile(compilation), "duplicate-PHI-predecessor fixture compiles");
        SsaFunction* function = findFunction(compilation, "Main");
        MirInstruction* phi = findPhi(function);
        tests.expect(phi != nullptr && phi->phiIncoming().size() == 2,
            "diamond PHI has two predecessor entries before duplicate corruption");
        if (phi != nullptr)
        {
            std::vector<MirPhiIncoming>& incoming =
                const_cast<std::vector<MirPhiIncoming>&>(phi->phiIncoming());
            incoming[1].block = incoming[0].block;
        }
        std::string error;
        tests.expect(!verify(compilation, error), "verifier rejects duplicate PHI predecessor");
        tests.expect(error.find("invalid or duplicated") != std::string::npos,
            "duplicate PHI predecessor fails exact predecessor-set invariant");
    }

    {
        FrontendCompilation compilation;
        tests.expect(compile(compilation), "promoted-storage-op fixture compiles");
        SsaFunction* function = findFunction(compilation, "Main");
        MirInstruction* addressedLoad = findInstruction(function, MirOpcode::StoreSymbol, "addressed");
        nativesharp::sema::Symbol* promoted = findPromoted(function, "value");
        tests.expect(addressedLoad != nullptr && promoted != nullptr,
            "fixture contains unpromoted load and promoted same-typed local");
        if (addressedLoad != nullptr && promoted != nullptr)
        {
            addressedLoad->setSymbol(promoted);
        }
        std::string error;
        tests.expect(!verify(compilation, error), "verifier rejects storage operation on promoted symbol");
        tests.expect(error.find("must not retain load/store/address.symbol") != std::string::npos,
            "promoted storage operation fails intended elimination invariant");
    }

    {
        FrontendCompilation compilation;
        const char* parameterSource =
            "int Identity(int x) { int y; return x + y; }"
            "int Main() { return Identity(3); }";
        tests.expect(
            compilation.compileBuffer(
                std::filesystem::path("ssa_parameter_undef.ns"),
                std::string(parameterSource),
                false),
            "parameter-undef corruption fixture compiles");
        SsaFunction* function = findFunction(compilation, "Identity");
        MirInstruction* localUndef = findInstruction(function, MirOpcode::Undef, "y");
        nativesharp::sema::Symbol* parameter = findPromoted(function, "x");
        tests.expect(localUndef != nullptr && parameter != nullptr,
            "fixture exposes local undef seed and promoted parameter");
        if (localUndef != nullptr && parameter != nullptr)
        {
            localUndef->setSymbol(parameter);
        }
        std::string error;
        tests.expect(!verify(compilation, error), "verifier rejects artificial undef seed for parameter");
        tests.expect(error.find("promoted parameters must not receive artificial undef seeds") != std::string::npos,
            "parameter undef corruption fails the intended initialization invariant");
    }

    {
        FrontendCompilation compilation;
        tests.expect(compile(compilation), "non-dominating-use fixture compiles");
        SsaFunction* function = findFunction(compilation, "Main");
        MirInstruction* phi = findPhi(function);
        MirInstruction* returnInstruction = findInstruction(function, MirOpcode::Return, "");
        tests.expect(phi != nullptr && returnInstruction != nullptr && !phi->phiIncoming().empty(),
            "fixture exposes branch-local PHI input and return use");
        if (phi != nullptr && returnInstruction != nullptr && !phi->phiIncoming().empty() &&
            !returnInstruction->operands().empty())
        {
            std::vector<MirValue*>& operands =
                const_cast<std::vector<MirValue*>&>(returnInstruction->operands());
            operands[0] = phi->phiIncoming()[0].value;
        }
        std::string error;
        tests.expect(!verify(compilation, error), "verifier rejects use not dominated by its definition");
        tests.expect(error.find("definition must dominate every ordinary use") != std::string::npos,
            "non-dominating use fails dominance invariant rather than unrelated shape check");
    }

    return tests.result();
}
