#include "Internal/UnicodeDllLoader.h"

#include "nativesharp/LLVM/CoffObjectVerifier.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/Linker/LldLinker.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string_view>
#include <system_error>
#include <vector>

namespace nativesharp::linker
{
namespace
{
constexpr std::uint32_t kPathCapacity = 32768;

std::string hexDigits(std::uint64_t value)
{
    constexpr std::string_view digits = "0123456789abcdef";
    std::string text(16, '0');
    for (std::size_t index = 0; index < 16; ++index)
    {
        text[15 - index] = digits[value & 0xFU];
        value >>= 4U;
    }
    return text;
}

// EN: FNV-1a over the DLL name and the export list; it only keeps the helper symbols of two loader libraries
//     apart, it is not an identity.
// ES: FNV-1a sobre el nombre de la DLL y la lista de exportaciones; solo separa los simbolos auxiliares de dos
//     bibliotecas cargadoras, no es una identidad.
void mixHash(std::uint64_t& hash, std::string_view text)
{
    for (const char character : text)
    {
        hash ^= static_cast<unsigned char>(character);
        hash *= 1099511628211ULL;
    }
    hash ^= 0xFFU;
    hash *= 1099511628211ULL;
}

std::string loaderTag(std::string_view dllName, std::span<const std::string> exportSymbols)
{
    std::uint64_t hash = 14695981039346656037ULL;
    mixHash(hash, dllName);
    for (const std::string& symbol : exportSymbols) mixHash(hash, symbol);
    return "__nsc_udl_" + hexDigits(hash);
}

// EN: Quoted LLVM IR identifier: `\` and `"` and bytes outside printable ASCII are written as `\XX`.
// ES: Identificador LLVM IR entre comillas: `\`, `"` y los bytes fuera de ASCII imprimible van como `\XX`.
std::string irName(std::string_view name)
{
    constexpr std::string_view digits = "0123456789ABCDEF";
    std::string text = "@\"";
    for (const char character : name)
    {
        const auto byte = static_cast<unsigned char>(character);
        if (byte < 0x20U || byte >= 0x7FU || character == '"' || character == '\\')
        {
            text += '\\';
            text += digits[byte >> 4U];
            text += digits[byte & 0xFU];
            continue;
        }
        text += character;
    }
    text += '"';
    return text;
}

std::string irBytes(std::string_view bytes)
{
    constexpr std::string_view digits = "0123456789ABCDEF";
    std::string text = "c\"";
    for (const char character : bytes)
    {
        const auto byte = static_cast<unsigned char>(character);
        if (byte < 0x20U || byte >= 0x7FU || character == '"' || character == '\\')
        {
            text += '\\';
            text += digits[byte >> 4U];
            text += digits[byte & 0xFU];
            continue;
        }
        text += character;
    }
    text += "\\00\"";
    return text;
}

std::string utf8Name(const std::filesystem::path& path)
{
    const std::u8string value = path.filename().u8string();
    return std::string(reinterpret_cast<const char*>(value.data()), value.size());
}

std::vector<std::uint16_t> utf16Name(const std::filesystem::path& path)
{
    std::vector<std::uint16_t> units;
    const std::u16string value = path.filename().u16string();
    units.assign(value.begin(), value.end());
    units.push_back(0);
    return units;
}

// EN: The shared lazy tail. Entered by `jmp` from a per-export stub with the export index in eax and the
//     caller's arguments untouched; it saves rcx, rdx, r8, r9 and xmm0-xmm3, resolves the export and jumps to
//     it with the arguments restored. On entry rsp is 8 modulo 16, so after the 0x88-byte frame it is aligned
//     for movdqa and for the call.
// ES: La cola diferida compartida. Se entra por `jmp` desde el stub de cada exportacion con el indice en eax y
//     los argumentos del llamador intactos; guarda rcx, rdx, r8, r9 y xmm0-xmm3, resuelve la exportacion y
//     salta a ella con los argumentos restaurados. Al entrar rsp es 8 modulo 16, asi que con el marco de 0x88
//     bytes queda alineado para movdqa y para la llamada.
void appendCommonTail(std::string& text, const std::string& tag)
{
    const std::string name = tag + "_common";
    const std::vector<std::string> lines = {
        ".text",
        ".p2align 4",
        ".def " + name + "; .scl 3; .type 32; .endef",
        name + ":",
        ".seh_proc " + name,
        "subq $0x88, %rsp",
        ".seh_stackalloc 0x88",
        ".seh_endprologue",
        "movq %rcx, 32(%rsp)",
        "movq %rdx, 40(%rsp)",
        "movq %r8, 48(%rsp)",
        "movq %r9, 56(%rsp)",
        "movdqa %xmm0, 64(%rsp)",
        "movdqa %xmm1, 80(%rsp)",
        "movdqa %xmm2, 96(%rsp)",
        "movdqa %xmm3, 112(%rsp)",
        "movl %eax, %ecx",
        "callq " + tag + "_resolve",
        "movq 32(%rsp), %rcx",
        "movq 40(%rsp), %rdx",
        "movq 48(%rsp), %r8",
        "movq 56(%rsp), %r9",
        "movdqa 64(%rsp), %xmm0",
        "movdqa 80(%rsp), %xmm1",
        "movdqa 96(%rsp), %xmm2",
        "movdqa 112(%rsp), %xmm3",
        "addq $0x88, %rsp",
        "jmpq *%rax",
        ".seh_endproc"};
    for (const std::string& line : lines) text += "module asm \"" + line + "\"\n";
}

std::string loaderModuleText(const std::filesystem::path& dllPath, std::span<const std::string> exportSymbols)
{
    const std::string tag = loaderTag(utf8Name(dllPath), exportSymbols);
    const std::vector<std::uint16_t> name = utf16Name(dllPath);
    const std::size_t count = exportSymbols.size();
    std::string text = "target triple = \"x86_64-pc-windows-msvc\"\n\n";
    appendCommonTail(text, tag);
    for (std::size_t index = 0; index < count; ++index)
    {
        // EN: Leaf stub without a frame: no unwind data is needed.
        // ES: Stub hoja sin marco: no necesita datos de desenrollado.
        const std::string stub = tag + "_lazy" + std::to_string(index);
        text += "module asm \".p2align 4\"\n";
        text += "module asm \".globl " + stub + "\"\n";
        text += "module asm \".def " + stub + "; .scl 2; .type 32; .endef\"\n";
        text += "module asm \"" + stub + ":\"\n";
        text += "module asm \"movl $" + std::to_string(index) + ", %eax\"\n";
        text += "module asm \"jmp " + tag + "_common\"\n";
    }
    text += "\n@" + tag + "_name = private unnamed_addr constant [" + std::to_string(name.size()) + " x i16] [";
    for (std::size_t index = 0; index < name.size(); ++index)
    {
        if (index != 0) text += ", ";
        text += "i16 " + std::to_string(name[index]);
    }
    text += "], align 2\n";
    text += "@" + tag + "_module = internal global ptr null, align 8\n";
    for (std::size_t index = 0; index < count; ++index)
    {
        const std::string& symbol = exportSymbols[index];
        text += "@" + tag + "_s" + std::to_string(index) + " = private unnamed_addr constant [" +
            std::to_string(symbol.size() + 1) + " x i8] " + irBytes(symbol) + "\n";
        text += irName("__imp_" + symbol) + " = global ptr @" + tag + "_lazy" + std::to_string(index) +
            ", align 8\n";
        text += "declare void @" + tag + "_lazy" + std::to_string(index) + "()\n";
    }
    text += "@" + tag + "_symbols = private unnamed_addr constant [" + std::to_string(count) + " x ptr] [";
    for (std::size_t index = 0; index < count; ++index)
        text += std::string(index == 0 ? "" : ", ") + "ptr @" + tag + "_s" + std::to_string(index);
    text += "]\n@" + tag + "_slots = private unnamed_addr constant [" + std::to_string(count) + " x ptr] [";
    for (std::size_t index = 0; index < count; ++index)
        text += std::string(index == 0 ? "" : ", ") + "ptr " + irName("__imp_" + exportSymbols[index]);
    text += "]\n\n";
    for (const std::string& symbol : exportSymbols)
    {
        // EN: The import thunk the linker would have taken from an import library: `jmp *__imp_X(%rip)`.
        // ES: El thunk de importacion que el enlazador tomaria de una biblioteca de importacion.
        text += "define void " + irName(symbol) + "() #0 {\n"
            "  call void asm sideeffect \"jmpq *$0\", \"*m\"(ptr elementtype(ptr) " + irName("__imp_" + symbol) +
            ")\n  unreachable\n}\n";
    }
    const std::string suffixLimit = std::to_string(kPathCapacity - name.size());
    const std::string bytes = std::to_string(name.size() * 2);
    text +=
        "\ndefine ptr @" + tag + "_resolve(i32 %index) #1 {\n"
        "entry:\n"
        "  %owner = alloca ptr, align 8\n"
        "  %cached = load atomic ptr, ptr @" + tag + "_module acquire, align 8\n"
        "  %has = icmp ne ptr %cached, null\n"
        "  br i1 %has, label %lookup, label %locate\n"
        "locate:\n"
        "  %heap = call ptr @GetProcessHeap()\n"
        "  %path = call ptr @HeapAlloc(ptr %heap, i32 0, i64 " + std::to_string(kPathCapacity * 2) + ")\n"
        "  %path.ok = icmp ne ptr %path, null\n"
        "  br i1 %path.ok, label %owner.find, label %missing\n"
        "owner.find:\n"
        "  store ptr null, ptr %owner, align 8\n"
        "  %found = call i32 @GetModuleHandleExW(i32 6, ptr @" + tag + "_resolve, ptr %owner)\n"
        "  %found.ok = icmp ne i32 %found, 0\n"
        "  br i1 %found.ok, label %file, label %missing\n"
        "file:\n"
        "  %self = load ptr, ptr %owner, align 8\n"
        "  %length = call i32 @GetModuleFileNameW(ptr %self, ptr %path, i32 " +
            std::to_string(kPathCapacity) + ")\n"
        "  %length.some = icmp ne i32 %length, 0\n"
        "  %length.fits = icmp ult i32 %length, " + suffixLimit + "\n"
        "  %length.ok = and i1 %length.some, %length.fits\n"
        "  br i1 %length.ok, label %scan, label %missing\n"
        "scan:\n"
        "  %cursor = phi i32 [ %length, %file ], [ %previous, %next ]\n"
        "  %cursor.end = icmp eq i32 %cursor, 0\n"
        "  br i1 %cursor.end, label %missing, label %step\n"
        "step:\n"
        "  %previous = sub i32 %cursor, 1\n"
        "  %unit.ptr = getelementptr i16, ptr %path, i32 %previous\n"
        "  %unit = load i16, ptr %unit.ptr, align 2\n"
        "  %separator = icmp eq i16 %unit, 92\n"
        "  br i1 %separator, label %load, label %next\n"
        "next:\n"
        "  br label %scan\n"
        "load:\n"
        "  %suffix = getelementptr i16, ptr %path, i32 %cursor\n"
        "  call void @llvm.memcpy.p0.p0.i64(ptr align 2 %suffix, ptr align 2 @" + tag + "_name, i64 " + bytes +
            ", i1 false)\n"
        "  %library = call ptr @LoadLibraryExW(ptr %path, ptr null, i32 8)\n"
        "  %freed = call i32 @HeapFree(ptr %heap, i32 0, ptr %path)\n"
        "  %library.ok = icmp ne ptr %library, null\n"
        "  br i1 %library.ok, label %publish, label %missing\n"
        "publish:\n"
        "  store atomic ptr %library, ptr @" + tag + "_module release, align 8\n"
        "  br label %lookup\n"
        "lookup:\n"
        "  %module = phi ptr [ %cached, %entry ], [ %library, %publish ]\n"
        "  %symbol.ptr = getelementptr [" + std::to_string(count) + " x ptr], ptr @" + tag +
            "_symbols, i32 0, i32 %index\n"
        "  %symbol = load ptr, ptr %symbol.ptr, align 8\n"
        "  %address = call ptr @GetProcAddress(ptr %module, ptr %symbol)\n"
        "  %address.ok = icmp ne ptr %address, null\n"
        "  br i1 %address.ok, label %bind, label %absent\n"
        "bind:\n"
        "  %slot.ptr = getelementptr [" + std::to_string(count) + " x ptr], ptr @" + tag +
            "_slots, i32 0, i32 %index\n"
        "  %slot = load ptr, ptr %slot.ptr, align 8\n"
        "  store atomic ptr %address, ptr %slot release, align 8\n"
        "  ret ptr %address\n"
        "missing:\n"
        "  call void @RaiseException(i32 -1073741515, i32 1, i32 0, ptr null)\n"
        "  unreachable\n"
        "absent:\n"
        "  call void @RaiseException(i32 -1073741511, i32 1, i32 0, ptr null)\n"
        "  unreachable\n"
        "}\n\n"
        "declare dllimport i32 @GetModuleHandleExW(i32, ptr, ptr)\n"
        "declare dllimport i32 @GetModuleFileNameW(ptr, ptr, i32)\n"
        "declare dllimport ptr @LoadLibraryExW(ptr, ptr, i32)\n"
        "declare dllimport ptr @GetProcessHeap()\n"
        "declare dllimport ptr @HeapAlloc(ptr, i32, i64)\n"
        "declare dllimport i32 @HeapFree(ptr, i32, ptr)\n"
        "declare dllimport ptr @GetProcAddress(ptr, ptr)\n"
        "declare dllimport void @RaiseException(i32, i32, i32, ptr)\n"
        "declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)\n\n"
        "attributes #0 = { naked noinline nounwind }\n"
        "attributes #1 = { noinline uwtable }\n";
    // EN: The resolver and the common tail are referenced only from module asm; keep them.
    // ES: El resolvedor y la cola comun solo se usan desde module asm; se conservan.
    text += "@llvm.used = appending global [1 x ptr] [ptr @" + tag + "_resolve], section \"llvm.metadata\"\n";
    return text;
}
}

bool needsUnicodeDllLoader(const std::filesystem::path& dllPath)
{
    const std::u8string encoded = dllPath.filename().u8string();
    for (const char8_t character : encoded)
    {
        if (static_cast<unsigned char>(character) >= 0x80U) return true;
    }
    return false;
}

bool writeUnicodeDllLoaderLibrary(
    const LldLinker& linker,
    const llvm_backend::LlvmTargetMachine& targetMachine,
    const std::filesystem::path& directory,
    const std::filesystem::path& dllPath,
    std::span<const std::string> exportSymbols,
    const std::filesystem::path& importLibraryPath,
    std::string& error)
{
    error.clear();
    if (exportSymbols.empty()) return true;
    std::string moduleError;
    std::unique_ptr<llvm_backend::LlvmModule> module =
        llvm_backend::LlvmModule::parseText(loaderModuleText(dllPath, exportSymbols), moduleError);
    if (module == nullptr)
    {
        error = "could not build the Unicode DLL loader: " + moduleError;
        return false;
    }
    if (!module->verify(moduleError))
    {
        error = "Unicode DLL loader verification failed: " + moduleError;
        return false;
    }
    const std::filesystem::path object = directory / "unicode-dll-loader.obj";
    if (!targetMachine.emit(*module, object, llvm_backend::LlvmCodeGenFileType::Object, moduleError))
    {
        error = "Unicode DLL loader object emission failed: " + moduleError;
        return false;
    }
    llvm_backend::CoffObjectVerifier verifier;
    if (!verifier.verify(object))
    {
        error = "Unicode DLL loader COFF verification failed: " + verifier.error();
        return false;
    }
    std::error_code removeError;
    std::filesystem::remove(importLibraryPath, removeError);
    const std::array<std::filesystem::path, 1> inputs = {object};
    return linker.createStaticLibrary(inputs, importLibraryPath, error);
}
}
