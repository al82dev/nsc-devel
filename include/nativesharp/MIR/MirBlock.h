#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nativesharp::mir
{
class MirInstruction;

class MirBlock final
{
public:
    MirBlock(std::uint32_t id, std::string name);

    std::uint32_t id() const noexcept;
    const std::string& name() const noexcept;
    void addInstruction(MirInstruction* instruction);
    void replaceInstructions(std::vector<MirInstruction*> instructions);
    const std::vector<MirInstruction*>& instructions() const noexcept;
    bool terminated() const noexcept;

private:
    std::uint32_t id_;
    std::string name_;
    std::vector<MirInstruction*> instructions_;
};
}
