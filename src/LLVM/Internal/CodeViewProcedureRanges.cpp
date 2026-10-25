#include "CodeViewProcedureRanges.h"

#include "CoffObjectFile.h"
#include "X64InstructionWalker.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <tuple>
#include <utility>
#include <vector>

namespace nativesharp::llvm_backend
{
namespace
{
constexpr std::uint32_t kCodeViewSignature = 4;
constexpr std::uint32_t kSymbolsSubsection = 0xF1;
constexpr std::uint32_t kLinesSubsection = 0xF2;
constexpr std::uint16_t kLocalProcedure = 0x110F;
constexpr std::uint16_t kGlobalProcedure = 0x1110;
constexpr std::uint16_t kLocalProcedureId = 0x1146;
constexpr std::uint16_t kGlobalProcedureId = 0x1147;
constexpr std::uint32_t kRuntimeFunctionSize = 12;
constexpr std::uint32_t kLineNumberMask = 0x00FFFFFF;
constexpr std::uint32_t kHiddenLine = 0x00FEEFEE;
constexpr std::uint32_t kAlwaysStepIntoLine = 0x00F00F00;

// ES: Campos de un registro de procedimiento, relativos al inicio del registro (antes de su longitud).
// EN: Procedure record fields, relative to the start of the record (before its length).
constexpr std::uint32_t kProcedureCodeSize = 16;
constexpr std::uint32_t kProcedureDebugStart = 20;
constexpr std::uint32_t kProcedureDebugEnd = 24;
constexpr std::uint32_t kProcedureCodeOffset = 32;
constexpr std::uint32_t kProcedureMinimumLength = 38;

using FunctionKey = std::uint64_t;
using LineEntry = std::pair<std::uint32_t, std::uint32_t>;

FunctionKey functionKey(std::int32_t sectionNumber, std::uint32_t offset)
{
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(sectionNumber)) << 32U) | offset;
}

bool isProcedure(std::uint16_t kind)
{
    return kind == kLocalProcedure || kind == kGlobalProcedure || kind == kLocalProcedureId ||
        kind == kGlobalProcedureId;
}

bool isRealLine(std::uint32_t line)
{
    return line != 0 && line != kHiddenLine && line != kAlwaysStepIntoLine;
}

bool isSection(const CoffSection& section, const std::string& name)
{
    return section.name == name || section.name.rfind(name + "$", 0) == 0;
}

// ES: Prologo y fin del cuerpo principal de cada funcion segun .pdata. La primera entrada que empieza en la
// funcion es la principal; las de los funclets empiezan en otro lugar.
// EN: Prologue and end of the main body of each function according to .pdata. The first entry that starts at
// the function is the main one; funclet entries start elsewhere.
void collectRuntimeFunctions(
    const CoffObjectFile& object,
    std::map<FunctionKey, std::uint32_t>& prologSizes,
    std::map<FunctionKey, std::uint32_t>& bodySizes)
{
    for (const CoffSection& section : object.sections())
    {
        if (!isSection(section, ".pdata")) continue;
        for (std::uint32_t entry = 0; entry + kRuntimeFunctionSize <= section.dataSize;
             entry += kRuntimeFunctionSize)
        {
            std::int32_t beginSection = 0;
            std::uint32_t beginOffset = 0;
            std::int32_t endSection = 0;
            std::uint32_t endOffset = 0;
            std::int32_t unwindSection = 0;
            std::uint32_t unwindOffset = 0;
            if (!object.relocationTarget(section, entry, beginSection, beginOffset) ||
                !object.relocationTarget(section, entry + 4, endSection, endOffset) ||
                !object.relocationTarget(section, entry + 8, unwindSection, unwindOffset) ||
                endSection != beginSection || endOffset < beginOffset)
                continue;
            const CoffSection& unwind = object.sections()[static_cast<std::size_t>(unwindSection) - 1];
            if (unwindOffset > unwind.dataSize || unwind.dataSize - unwindOffset < 4) continue;
            const std::uint8_t* info = object.data(static_cast<std::size_t>(unwind.dataOffset) + unwindOffset, 4);
            if (info == nullptr) continue;
            const FunctionKey key = functionKey(beginSection, beginOffset);
            prologSizes.emplace(key, info[1]);
            bodySizes.emplace(key, endOffset - beginOffset);
        }
    }
}

void collectLineBlocks(
    const CoffObjectFile& object,
    const CoffSection& section,
    std::uint32_t body,
    std::uint32_t length,
    std::map<FunctionKey, std::vector<LineEntry>>& lines)
{
    std::int32_t functionSection = 0;
    std::uint32_t functionOffset = 0;
    std::uint16_t flags = 0;
    if (length < 12 || !object.relocationTarget(section, body, functionSection, functionOffset) ||
        !object.readU16(static_cast<std::size_t>(section.dataOffset) + body + 6, flags))
        return;
    std::vector<LineEntry>& entries = lines[functionKey(functionSection, functionOffset)];
    std::uint32_t block = body + 12;
    while (block + 12 <= body + length)
    {
        std::uint32_t lineCount = 0;
        std::uint32_t blockSize = 0;
        const std::size_t blockStart = static_cast<std::size_t>(section.dataOffset) + block;
        if (!object.readU32(blockStart + 4, lineCount) || !object.readU32(blockStart + 8, blockSize) ||
            blockSize < 12 || block + blockSize > body + length)
            return;
        for (std::uint32_t index = 0; index < lineCount && 12 + (index + 1) * 8 <= blockSize; ++index)
        {
            std::uint32_t offset = 0;
            std::uint32_t lineFlags = 0;
            if (!object.readU32(blockStart + 12 + index * 8, offset) ||
                !object.readU32(blockStart + 16 + index * 8, lineFlags))
                return;
            entries.emplace_back(offset, lineFlags & kLineNumberMask);
        }
        block += blockSize;
    }
}

// ES: Una subseccion CodeView: indice de su seccion .debug$S, inicio de su cuerpo y longitud.
// EN: One CodeView subsection: index of its .debug$S section, start of its body and length.
using Subsection = std::tuple<std::size_t, std::uint32_t, std::uint32_t>;

// ES: Subsecciones de tipo `wanted` de todas las secciones .debug$S, en orden.
// EN: Subsections of kind `wanted` from every .debug$S section, in order.
std::vector<Subsection> subsections(const CoffObjectFile& object, std::uint32_t wanted)
{
    std::vector<Subsection> found;
    for (std::size_t index = 0; index < object.sections().size(); ++index)
    {
        const CoffSection& section = object.sections()[index];
        std::uint32_t signature = 0;
        if (!isSection(section, ".debug$S") || section.dataSize < 4 ||
            !object.readU32(section.dataOffset, signature) || signature != kCodeViewSignature)
            continue;
        std::uint32_t position = 4;
        while (position + 8 <= section.dataSize)
        {
            std::uint32_t kind = 0;
            std::uint32_t length = 0;
            const std::size_t header = static_cast<std::size_t>(section.dataOffset) + position;
            if (!object.readU32(header, kind) || !object.readU32(header + 4, length) ||
                length > section.dataSize - position - 8)
                break;
            if (kind == wanted) found.emplace_back(index, position + 8, length);
            position += 8 + length;
            position = (position + 3U) & ~3U;
        }
    }
    return found;
}

std::uint32_t firstBodyLineOffset(std::vector<LineEntry> entries, std::uint32_t bodySize, bool& found)
{
    found = false;
    std::stable_sort(entries.begin(), entries.end());
    std::uint32_t openingLine = 0;
    for (const LineEntry& entry : entries)
    {
        if (!isRealLine(entry.second) || entry.first >= bodySize) continue;
        if (openingLine == 0)
        {
            openingLine = entry.second;
            continue;
        }
        if (entry.second != openingLine)
        {
            found = true;
            return entry.first;
        }
    }
    return 0;
}

// ES: Calcula y escribe DbgStart y DbgEnd del registro de procedimiento en `record` de `section`. Devuelve
// verdadero si escribio. Un registro cuya funcion no se resuelve o no se decodifica queda como estaba.
// EN: Computes and writes DbgStart and DbgEnd of the procedure record at `record` of `section`. Returns true
// when it wrote. A record whose function does not resolve or does not decode stays as it was.
bool patchProcedure(
    CoffObjectFile& object,
    std::size_t sectionIndex,
    std::uint32_t record,
    const X64InstructionWalker& walker,
    const std::map<FunctionKey, std::uint32_t>& prologSizes,
    const std::map<FunctionKey, std::uint32_t>& bodySizes,
    const std::map<FunctionKey, std::vector<LineEntry>>& lines)
{
    const CoffSection& section = object.sections()[sectionIndex];
    const std::size_t start = static_cast<std::size_t>(section.dataOffset) + record;
    std::uint32_t codeSize = 0;
    std::int32_t functionSection = 0;
    std::uint32_t functionOffset = 0;
    if (!object.readU32(start + kProcedureCodeSize, codeSize) || codeSize == 0 ||
        !object.relocationTarget(section, record + kProcedureCodeOffset, functionSection, functionOffset))
        return false;
    const CoffSection& code = object.sections()[static_cast<std::size_t>(functionSection) - 1];
    if (functionOffset > code.dataSize || code.dataSize - functionOffset < codeSize) return false;
    const FunctionKey key = functionKey(functionSection, functionOffset);
    std::uint32_t bodySize = codeSize;
    const auto body = bodySizes.find(key);
    if (body != bodySizes.end() && body->second != 0 && body->second <= codeSize) bodySize = body->second;
    const std::uint8_t* bytes = object.data(static_cast<std::size_t>(code.dataOffset) + functionOffset, bodySize);
    std::size_t epilogue = 0;
    if (bytes == nullptr || !walker.lastEpilogueStart(bytes, bodySize, epilogue)) return false;

    std::uint32_t debugStart = 0;
    const auto prolog = prologSizes.find(key);
    if (prolog != prologSizes.end()) debugStart = prolog->second;
    const auto functionLines = lines.find(key);
    if (functionLines != lines.end())
    {
        bool found = false;
        const std::uint32_t firstBodyLine = firstBodyLineOffset(functionLines->second, bodySize, found);
        if (found) debugStart = std::max(debugStart, firstBodyLine);
    }
    const std::uint32_t debugEnd = static_cast<std::uint32_t>(epilogue);
    debugStart = std::min(debugStart, debugEnd);
    return object.writeU32(start + kProcedureDebugStart, debugStart) &&
        object.writeU32(start + kProcedureDebugEnd, debugEnd);
}

// ES: Recorre los registros de una subseccion de simbolos y corrige cada procedimiento.
// EN: Walks the records of a symbols subsection and patches every procedure.
bool patchSymbols(
    CoffObjectFile& object,
    const Subsection& subsection,
    const X64InstructionWalker& walker,
    const std::map<FunctionKey, std::uint32_t>& prologSizes,
    const std::map<FunctionKey, std::uint32_t>& bodySizes,
    const std::map<FunctionKey, std::vector<LineEntry>>& lines)
{
    const std::size_t sectionIndex = std::get<0>(subsection);
    const std::uint32_t body = std::get<1>(subsection);
    const std::uint32_t length = std::get<2>(subsection);
    bool changed = false;
    std::uint32_t record = body;
    while (record + 4 <= body + length)
    {
        const std::size_t start = static_cast<std::size_t>(object.sections()[sectionIndex].dataOffset) + record;
        std::uint16_t recordLength = 0;
        std::uint16_t recordKind = 0;
        if (!object.readU16(start, recordLength) || !object.readU16(start + 2, recordKind) ||
            record + 2U + recordLength > body + length)
            break;
        if (isProcedure(recordKind) && recordLength >= kProcedureMinimumLength &&
            patchProcedure(object, sectionIndex, record, walker, prologSizes, bodySizes, lines))
            changed = true;
        record += 2U + recordLength;
    }
    return changed;
}
}

bool CodeViewProcedureRanges::apply(const std::filesystem::path& objectPath, std::string& error)
{
    CoffObjectFile object;
    if (!object.load(objectPath, error)) return false;
    if (!object.isAmd64()) return true;
    const std::vector<Subsection> symbolSubsections = subsections(object, kSymbolsSubsection);
    if (symbolSubsections.empty()) return true;
    X64InstructionWalker walker;
    if (!walker.ready())
    {
        error = "the LLVM x86-64 disassembler is not available to compute CodeView procedure ranges";
        return false;
    }
    std::map<FunctionKey, std::uint32_t> prologSizes;
    std::map<FunctionKey, std::uint32_t> bodySizes;
    std::map<FunctionKey, std::vector<LineEntry>> lines;
    collectRuntimeFunctions(object, prologSizes, bodySizes);
    for (const Subsection& subsection : subsections(object, kLinesSubsection))
    {
        const CoffSection& section = object.sections()[std::get<0>(subsection)];
        collectLineBlocks(object, section, std::get<1>(subsection), std::get<2>(subsection), lines);
    }
    bool changed = false;
    for (const Subsection& subsection : symbolSubsections)
    {
        if (patchSymbols(object, subsection, walker, prologSizes, bodySizes, lines)) changed = true;
    }
    if (!changed) return true;
    return object.save(objectPath, error);
}
}
