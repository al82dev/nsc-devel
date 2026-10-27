#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace nativesharp::llvm_backend
{
// ES: Recorre el codigo x64 de una funcion instruccion por instruccion con el desensamblador de LLVM y
// ubica su ultimo epilogo. El epilogo es el que define el ABI de Windows x64 (y el que reconoce el
// desenrollador de Windows): un `add rsp, imm`, `lea rsp, [reg + disp]` o `mov rsp, rbp` opcional, cero o mas
// `pop` de registros de 64 bits y un `ret`. Las fronteras de instruccion salen del desensamblador, asi que un
// byte 0xC3 o 0x58 dentro de otra instruccion no se confunde con un `ret` o un `pop`.
// EN: Walks a function's x64 code instruction by instruction with LLVM's disassembler and locates its last
// epilogue. The epilogue is the one the Windows x64 ABI defines (and the Windows unwinder recognizes): an
// optional `add rsp, imm`, `lea rsp, [reg + disp]` or `mov rsp, rbp`, zero or more 64-bit register `pop`s
// and a `ret`. Instruction boundaries come from the disassembler, so a 0xC3 or 0x58 byte inside another
// instruction is not taken for a `ret` or a `pop`.
class X64InstructionWalker final
{
public:
    X64InstructionWalker();
    ~X64InstructionWalker();
    X64InstructionWalker(const X64InstructionWalker&) = delete;
    X64InstructionWalker& operator=(const X64InstructionWalker&) = delete;

    bool ready() const noexcept;

    // ES: Desplazamiento donde empieza el ultimo epilogo de `code`. Si la funcion no tiene `ret` (no
    // retorna), es el inicio de su ultima instruccion. Falso si el codigo no se puede decodificar.
    // EN: Offset where the last epilogue of `code` starts. When the function has no `ret` (it does not
    // return), it is the start of its last instruction. False when the code cannot be decoded.
    bool lastEpilogueStart(const std::uint8_t* code, std::size_t size, std::size_t& start) const;

private:
    bool instructionStarts(const std::uint8_t* code, std::size_t size, std::vector<std::size_t>& starts) const;
    static bool isReturn(const std::uint8_t* instruction, std::size_t length) noexcept;
    static bool isPop(const std::uint8_t* instruction, std::size_t length) noexcept;
    static bool isStackRestore(const std::uint8_t* instruction, std::size_t length) noexcept;

    void* context_ = nullptr;
};
}
