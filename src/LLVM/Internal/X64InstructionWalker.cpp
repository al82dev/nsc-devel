#include "X64InstructionWalker.h"

#include "../LlvmCApi.h"

#include <array>

namespace nativesharp::llvm_backend
{
namespace
{
constexpr const char* kTriple = "x86_64-pc-windows-msvc";
}

X64InstructionWalker::X64InstructionWalker()
{
    context_ = llvm_api::LLVMCreateDisasm(kTriple, nullptr, 0, nullptr, nullptr);
}

X64InstructionWalker::~X64InstructionWalker()
{
    if (context_ != nullptr) llvm_api::LLVMDisasmDispose(context_);
}

bool X64InstructionWalker::ready() const noexcept
{
    return context_ != nullptr;
}

bool X64InstructionWalker::lastEpilogueStart(const std::uint8_t* code, std::size_t size, std::size_t& start) const
{
    std::vector<std::size_t> starts;
    if (!instructionStarts(code, size, starts) || starts.empty()) return false;
    std::size_t returnIndex = starts.size();
    for (std::size_t index = starts.size(); index > 0; --index)
    {
        const std::size_t end = index < starts.size() ? starts[index] : size;
        if (isReturn(code + starts[index - 1], end - starts[index - 1]))
        {
            returnIndex = index - 1;
            break;
        }
    }
    if (returnIndex == starts.size())
    {
        start = starts.back();
        return true;
    }
    std::size_t first = returnIndex;
    while (first > 0 && isPop(code + starts[first - 1], starts[first] - starts[first - 1])) --first;
    if (first > 0 && isStackRestore(code + starts[first - 1], starts[first] - starts[first - 1])) --first;
    start = starts[first];
    return true;
}

bool X64InstructionWalker::instructionStarts(
    const std::uint8_t* code,
    std::size_t size,
    std::vector<std::size_t>& starts) const
{
    if (context_ == nullptr || code == nullptr) return false;
    std::array<char, 256> text{};
    std::size_t offset = 0;
    while (offset < size)
    {
        // ES: La API C pide bytes no constantes, pero no los modifica.
        // EN: The C API asks for non-const bytes, but it does not modify them.
        std::uint8_t* bytes = const_cast<std::uint8_t*>(code + offset);
        const std::size_t length = llvm_api::LLVMDisasmInstruction(
            context_, bytes, size - offset, offset, text.data(), text.size());
        if (length == 0) return false;
        starts.push_back(offset);
        offset += length;
    }
    return true;
}

bool X64InstructionWalker::isReturn(const std::uint8_t* instruction, std::size_t length) noexcept
{
    if (length == 1) return instruction[0] == 0xC3;
    if (length == 2) return (instruction[0] == 0xF3 || instruction[0] == 0x48) && instruction[1] == 0xC3;
    return length == 3 && instruction[0] == 0xC2;
}

bool X64InstructionWalker::isPop(const std::uint8_t* instruction, std::size_t length) noexcept
{
    if (length == 1) return instruction[0] >= 0x58 && instruction[0] <= 0x5F;
    return length == 2 && instruction[0] == 0x41 && instruction[1] >= 0x58 && instruction[1] <= 0x5F;
}

bool X64InstructionWalker::isStackRestore(const std::uint8_t* instruction, std::size_t length) noexcept
{
    // ES: add rsp, imm8 (48 83 C4 ib) y add rsp, imm32 (48 81 C4 id).
    // EN: add rsp, imm8 (48 83 C4 ib) and add rsp, imm32 (48 81 C4 id).
    if (length == 4 && instruction[0] == 0x48 && instruction[1] == 0x83 && instruction[2] == 0xC4) return true;
    if (length == 7 && instruction[0] == 0x48 && instruction[1] == 0x81 && instruction[2] == 0xC4) return true;
    // ES: mov rsp, rbp en sus dos codificaciones (48 89 EC y 48 8B E5).
    // EN: mov rsp, rbp in both encodings (48 89 EC and 48 8B E5).
    if (length == 3 && instruction[0] == 0x48 && instruction[1] == 0x89 && instruction[2] == 0xEC) return true;
    if (length == 3 && instruction[0] == 0x48 && instruction[1] == 0x8B && instruction[2] == 0xE5) return true;
    // ES: lea rsp, [reg + disp]: REX.W sin REX.R, opcode 8D, ModRM con reg = rsp (4) y operando en memoria.
    // EN: lea rsp, [reg + disp]: REX.W without REX.R, opcode 8D, ModRM with reg = rsp (4) and a memory operand.
    if (length < 3 || (instruction[0] != 0x48 && instruction[0] != 0x49) || instruction[1] != 0x8D) return false;
    const std::uint8_t modrm = instruction[2];
    return ((modrm >> 3U) & 7U) == 4U && (modrm >> 6U) != 3U;
}
}
