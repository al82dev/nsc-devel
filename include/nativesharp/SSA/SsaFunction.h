#pragma once

#include <cstddef>
#include <unordered_map>
#include <vector>

namespace nativesharp::cfg
{
class CfgFunction;
}

namespace nativesharp::dominance
{
class DominanceFunction;
}

namespace nativesharp::mir
{
class MirBlock;
class MirFunction;
}

namespace nativesharp::sema
{
class Symbol;
}

namespace nativesharp::ssa
{
class SsaFunction final
{
public:
    SsaFunction(
        const mir::MirFunction* sourceFunction,
        mir::MirFunction* function,
        const cfg::CfgFunction* cfgFunction,
        const dominance::DominanceFunction* dominanceFunction);

    const mir::MirFunction* sourceFunction() const noexcept;
    mir::MirFunction* function() const noexcept;
    const cfg::CfgFunction* cfgFunction() const noexcept;
    const dominance::DominanceFunction* dominanceFunction() const noexcept;

    void addPromotedSymbol(sema::Symbol* symbol);
    const std::vector<sema::Symbol*>& promotedSymbols() const noexcept;
    bool isPromoted(const sema::Symbol* symbol) const noexcept;

    void mapBlock(const mir::MirBlock* source, mir::MirBlock* output);
    mir::MirBlock* findOutputBlock(const mir::MirBlock* source) const noexcept;
    const mir::MirBlock* findSourceBlock(const mir::MirBlock* output) const noexcept;

    void setExcludedUnreachableBlocks(std::size_t count) noexcept;
    std::size_t excludedUnreachableBlocks() const noexcept;

private:
    const mir::MirFunction* sourceFunction_;
    mir::MirFunction* function_;
    const cfg::CfgFunction* cfgFunction_;
    const dominance::DominanceFunction* dominanceFunction_;
    std::vector<sema::Symbol*> promotedSymbols_;
    std::unordered_map<const mir::MirBlock*, mir::MirBlock*> outputBlocks_;
    std::unordered_map<const mir::MirBlock*, const mir::MirBlock*> sourceBlocks_;
    std::size_t excludedUnreachableBlocks_ = 0;
};
}
