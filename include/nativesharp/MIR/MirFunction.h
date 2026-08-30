#pragma once

#include <vector>

namespace nativesharp::sema
{
class Symbol;
}

namespace nativesharp::mir
{
class MirBlock;
class MirValue;

class MirFunction final
{
public:
    explicit MirFunction(sema::Symbol* symbol);

    sema::Symbol* symbol() const noexcept;
    bool defined() const noexcept;
    void setDefined(bool value) noexcept;

    void addParameter(MirValue* value);
    const std::vector<MirValue*>& parameters() const noexcept;

    void addLocal(sema::Symbol* symbol);
    const std::vector<sema::Symbol*>& locals() const noexcept;

    void addBlock(MirBlock* block);
    void replaceBlocks(std::vector<MirBlock*> blocks);
    const std::vector<MirBlock*>& blocks() const noexcept;
    MirBlock* entryBlock() const noexcept;

private:
    sema::Symbol* symbol_;
    bool defined_ = false;
    std::vector<MirValue*> parameters_;
    std::vector<sema::Symbol*> locals_;
    std::vector<MirBlock*> blocks_;
};
}
