#include "nativesharp/LLVM/LlvmLowerer.h"

#include "LlvmCApi.h"
#include "SignedDivisionCorrection.h"

#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmTrapReason.h"
#include "nativesharp/MIR/MirBlock.h"
#include "nativesharp/MIR/MirFunction.h"
#include "nativesharp/MIR/MirGlobal.h"
#include "nativesharp/MIR/MirInstruction.h"
#include "nativesharp/MIR/MirModule.h"
#include "nativesharp/MIR/MirOpcode.h"
#include "nativesharp/MIR/MirValue.h"
#include "nativesharp/Passes/OptimizationModule.h"
#include "nativesharp/Passes/OptimizationVerifier.h"
#include "nativesharp/Sema/ConstantValue.h"
#include "nativesharp/Sema/Symbol.h"
#include "nativesharp/Sema/SymbolKind.h"
#include "nativesharp/Sema/Type.h"
#include "nativesharp/Sema/TypeKind.h"
#include "nativesharp/Target/TargetDataLayout.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

#include "Internal/Implementation.h"

LlvmLowerer::LlvmLowerer(std::span<sema::Symbol* const> globalSymbols) noexcept
    : implementation_(std::make_unique<Implementation>(globalSymbols))
{
}

LlvmLowerer::~LlvmLowerer() = default;

std::unique_ptr<LlvmModule> LlvmLowerer::lower(const passes::OptimizationModule* optimizedModule)
{
    return implementation_->lower(optimizedModule, error_);
}

const std::string& LlvmLowerer::error() const noexcept
{
    return error_;
}
}
