#include "nativesharp/SSA/SsaFunction.h"

#include "nativesharp/MIR/MirBlock.h"

namespace nativesharp::ssa
{
SsaFunction::SsaFunction(
    const mir::MirFunction* sourceFunction,
    mir::MirFunction* function,
    const cfg::CfgFunction* cfgFunction,
    const dominance::DominanceFunction* dominanceFunction)
    : sourceFunction_(sourceFunction),
      function_(function),
      cfgFunction_(cfgFunction),
      dominanceFunction_(dominanceFunction)
{
}

const mir::MirFunction* SsaFunction::sourceFunction() const noexcept { return sourceFunction_; }
mir::MirFunction* SsaFunction::function() const noexcept { return function_; }
const cfg::CfgFunction* SsaFunction::cfgFunction() const noexcept { return cfgFunction_; }
const dominance::DominanceFunction* SsaFunction::dominanceFunction() const noexcept
{
    return dominanceFunction_;
}

void SsaFunction::addPromotedSymbol(sema::Symbol* symbol)
{
    promotedSymbols_.push_back(symbol);
}

const std::vector<sema::Symbol*>& SsaFunction::promotedSymbols() const noexcept
{
    return promotedSymbols_;
}

bool SsaFunction::isPromoted(const sema::Symbol* symbol) const noexcept
{
    for (const sema::Symbol* promoted : promotedSymbols_)
    {
        if (promoted == symbol)
        {
            return true;
        }
    }
    return false;
}

void SsaFunction::mapBlock(const mir::MirBlock* source, mir::MirBlock* output)
{
    outputBlocks_.emplace(source, output);
    sourceBlocks_.emplace(output, source);
}

mir::MirBlock* SsaFunction::findOutputBlock(const mir::MirBlock* source) const noexcept
{
    const auto found = outputBlocks_.find(source);
    return found == outputBlocks_.end() ? nullptr : found->second;
}

const mir::MirBlock* SsaFunction::findSourceBlock(const mir::MirBlock* output) const noexcept
{
    const auto found = sourceBlocks_.find(output);
    return found == sourceBlocks_.end() ? nullptr : found->second;
}

void SsaFunction::setExcludedUnreachableBlocks(std::size_t count) noexcept
{
    excludedUnreachableBlocks_ = count;
}

std::size_t SsaFunction::excludedUnreachableBlocks() const noexcept
{
    return excludedUnreachableBlocks_;
}
}
