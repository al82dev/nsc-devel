#include "ThinLtoBackend.h"
#include "WinEhCatchSwitchPhiLegalization.h"

#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"

#if !defined(NSC_THINLTO_UNAVAILABLE)
#include "llvm-c/Core.h"
#include "llvm-c/Error.h"
#include "llvm-c/TargetMachine.h"
#include "llvm-c/Transforms/PassBuilder.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/ModuleSummaryAnalysis.h"
#include "llvm/Analysis/ProfileSummaryInfo.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/Demangle/Demangle.h"
#if __has_include("llvm/Support/VCSRevision.h")
#include "llvm/Support/VCSRevision.h"
#endif
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/DiagnosticPrinter.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/ModuleSummaryIndex.h"
#include "llvm/LTO/Config.h"
#include "llvm/LTO/LTO.h"
#include "llvm/Object/Archive.h"
#include "llvm/Object/Binary.h"
#include "llvm/Object/COFF.h"
#include "llvm/Object/COFFImportFile.h"
#include "llvm/Object/IRObjectFile.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/CachePruning.h"
#include "llvm/Support/Caching.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Threading.h"
#include "llvm/Support/raw_ostream.h"
#endif

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <fstream>
#if defined(_WIN32)
#include <process.h>
#endif

namespace
{
// EN: LKF-437: a backend cache entry is a raw COFF object for x64. An entry that is empty, truncated or
//     not an AMD64 (or bigobj) COFF object is a miss, not a hit copied into the link.
// ES: LKF-437: una entrada del caché de backends es un objeto COFF crudo de x64. Una entrada vacía,
//     truncada o que no es un objeto COFF AMD64 (o bigobj) es un fallo, no un acierto copiado al enlace.
bool usableCachedThinLtoObject(const std::filesystem::path& entry)
{
    std::ifstream stream(entry, std::ios::binary);
    unsigned char header[20] = {};
    if (!stream.read(reinterpret_cast<char*>(header), sizeof(header))) return false;
    const bool amd64 = header[0] == 0x64U && header[1] == 0x86U;
    const bool bigObject = header[0] == 0x00U && header[1] == 0x00U && header[2] == 0xFFU && header[3] == 0xFFU;
    return amd64 || bigObject;
}

// LKF-437: the temporary of a published entry carries the process id, like the artifact transaction.
// LKF-437: el temporal de una entrada publicada lleva el id del proceso, como la transacción de artefactos.
std::string thinLtoCacheProcessTag()
{
#if defined(_WIN32)
    return std::to_string(static_cast<unsigned long>(_getpid()));
#else
    return "0";
#endif
}
}

namespace nativesharp::llvm_backend
{
#if !defined(NSC_THINLTO_UNAVAILABLE)
namespace
{
// EN: LLVM reads paths as UTF-8; the narrow `path::string()` threw for directories the active code page
//     cannot spell (T08-002-09m7c0g2b). ES: LLVM lee las rutas como UTF-8; `path::string()` lanzaba con
//     directorios que la pagina de codigos activa no puede escribir (T08-002-09m7c0g2b).
std::string thinLtoPathUtf8(const std::filesystem::path& path)
{
    const std::u8string encoded = path.u8string();
    return std::string(reinterpret_cast<const char*>(encoded.data()), encoded.size());
}
}
#endif

#if defined(NSC_THINLTO_UNAVAILABLE)
ThinLtoBackend::ThinLtoBackend(std::size_t moduleCount)
    : bitcode_(moduleCount),
      identifiers_(moduleCount),
      finalModuleTexts_(moduleCount)
{
}

bool ThinLtoBackend::available() noexcept
{
    return false;
}

bool ThinLtoBackend::addModule(
    std::size_t, LlvmModule&, const LlvmTargetMachine&, LlvmOptimizationLevel, std::string& error)
{
    error = "ThinLTO needs the STATIC LLVM host";
    return false;
}

bool ThinLtoBackend::run(
    std::span<const std::filesystem::path>,
    const LlvmTargetMachine&,
    LlvmOptimizationLevel,
    std::size_t,
    const std::filesystem::path&,
    std::string& error)
{
    error = "ThinLTO needs the STATIC LLVM host";
    return false;
}

std::string ThinLtoBackend::compatibilityIdentity(const LlvmTargetMachine&, LlvmOptimizationLevel)
{
    return {};
}

bool ThinLtoBackend::collectUndefinedSymbols(
    const std::filesystem::path&, std::vector<std::string>&, std::string& error)
{
    error = "ThinLTO needs the STATIC LLVM host";
    return false;
}
bool ThinLtoBackend::validateNativeObject(std::string_view, std::string& error)
{
    error = "ThinLTO needs the STATIC LLVM host";
    return false;
}
bool ThinLtoBackend::requiresLldForStaticInput(
    const std::filesystem::path&, bool& found, std::string& error)
{
    found = false;
    error = "TLS inspection needs the STATIC LLVM host";
    return false;
}
bool ThinLtoBackend::findLinkedCrtInitializers(std::span<const std::filesystem::path>,
                                               std::span<const std::filesystem::path>, std::string& input,
                                               std::string& member, std::string& error)
{
    input.clear();
    member.clear();
    error = "initializer inspection needs the STATIC LLVM host";
    return false;
}
bool ThinLtoBackend::findMissingLinkSymbols(
    std::span<const std::filesystem::path>, std::span<const std::filesystem::path>, std::span<const std::string>,
    std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::string>>>>& missing,
    std::string& error)
{
    missing.clear();
    error = "symbol inspection needs the STATIC LLVM host";
    return false;
}
std::string ThinLtoBackend::demangleSymbol(std::string_view symbol) { return std::string(symbol); }
#else
namespace
{
unsigned levelNumber(LlvmOptimizationLevel level) noexcept
{
    switch (level)
    {
    case LlvmOptimizationLevel::O0: return 0;
    case LlvmOptimizationLevel::O1: return 1;
    case LlvmOptimizationLevel::O2: return 2;
    case LlvmOptimizationLevel::O3: return 3;
    }
    return 0;
}

// The code generation level LlvmTargetMachine::createWindowsX64 uses for `level`.
// El nivel de generacion de codigo que usa LlvmTargetMachine::createWindowsX64 para `level`.
llvm::CodeGenOptLevel codeGenerationLevel(LlvmOptimizationLevel level) noexcept
{
    switch (level)
    {
    case LlvmOptimizationLevel::O0: return llvm::CodeGenOptLevel::None;
    case LlvmOptimizationLevel::O1: return llvm::CodeGenOptLevel::Less;
    case LlvmOptimizationLevel::O2: return llvm::CodeGenOptLevel::Default;
    case LlvmOptimizationLevel::O3: return llvm::CodeGenOptLevel::Aggressive;
    }
    return llvm::CodeGenOptLevel::None;
}

std::vector<std::string> splitFeatures(const std::string& features)
{
    std::vector<std::string> result;
    llvm::SmallVector<llvm::StringRef, 16> parts;
    llvm::StringRef(features).split(parts, ',', -1, false);
    for (const llvm::StringRef part : parts) result.emplace_back(part.str());
    return result;
}

// Appends the undefined symbols of one COFF object. Agrega los simbolos indefinidos de un objeto COFF.
bool appendUndefinedSymbols(
    const llvm::object::ObjectFile& object,
    std::vector<std::string>& symbols,
    std::string& error)
{
    for (const llvm::object::SymbolRef& symbol : object.symbols())
    {
        llvm::Expected<std::uint32_t> flags = symbol.getFlags();
        if (!flags)
        {
            error = llvm::toString(flags.takeError());
            return false;
        }
        if ((*flags & llvm::object::SymbolRef::SF_Undefined) == 0) continue;
        llvm::Expected<llvm::StringRef> name = symbol.getName();
        if (!name)
        {
            error = llvm::toString(name.takeError());
            return false;
        }
        symbols.push_back(name->str());
    }
    return true;
}

std::size_t importedDefinitions(const llvm::Module& module) noexcept
{
    std::size_t count = 0;
    for (const llvm::Function& function : module)
        if (!function.isDeclaration() && function.hasAvailableExternallyLinkage()) ++count;
    return count;
}
}

ThinLtoBackend::ThinLtoBackend(std::size_t moduleCount)
    : bitcode_(moduleCount),
      identifiers_(moduleCount),
      finalModuleTexts_(moduleCount)
{
}

bool ThinLtoBackend::available() noexcept
{
    return true;
}

bool ThinLtoBackend::validateNativeObject(std::string_view bytes, std::string& error)
{
    const llvm::StringRef content(bytes.data(), bytes.size());
    auto object = llvm::object::ObjectFile::createObjectFile(llvm::MemoryBufferRef(content, "sidecar-native"));
    if (!object)
    {
        error = "invalid native object in ThinLTO sidecar: " + llvm::toString(object.takeError());
        return false;
    }
    const auto* coff = llvm::dyn_cast<llvm::object::COFFObjectFile>(object->get());
    if (!coff || coff->getMachine() != llvm::COFF::IMAGE_FILE_MACHINE_AMD64 || coff->getNumberOfSections() == 0)
    {
        error = "native object in ThinLTO sidecar is not an AMD64 COFF object with sections";
        return false;
    }
    error.clear();
    return true;
}

bool ThinLtoBackend::collectUndefinedSymbols(
    const std::filesystem::path& file,
    std::vector<std::string>& symbols,
    std::string& error)
{
    llvm::Expected<llvm::object::OwningBinary<llvm::object::Binary>> binary =
        llvm::object::createBinary(thinLtoPathUtf8(file));
    if (!binary)
    {
        error = "cannot read '" + thinLtoPathUtf8(file) + "': " + llvm::toString(binary.takeError());
        return false;
    }
    llvm::object::Binary* contents = binary->getBinary();
    if (const auto* object = llvm::dyn_cast<llvm::object::ObjectFile>(contents))
        return appendUndefinedSymbols(*object, symbols, error);
    const auto* archive = llvm::dyn_cast<llvm::object::Archive>(contents);
    // Any other format (an import-only file, for instance) references nothing of the program.
    // Cualquier otro formato (un archivo solo de imports, por ejemplo) no referencia al programa.
    if (archive == nullptr) return true;
    llvm::Error childError = llvm::Error::success();
    for (const llvm::object::Archive::Child& child : archive->children(childError))
    {
        llvm::Expected<std::unique_ptr<llvm::object::Binary>> member = child.getAsBinary();
        if (!member)
        {
            error = "cannot read a member of '" + thinLtoPathUtf8(file) + "': " + llvm::toString(member.takeError());
            llvm::consumeError(std::move(childError));
            return false;
        }
        const auto* object = llvm::dyn_cast<llvm::object::ObjectFile>(member->get());
        if (object != nullptr && !appendUndefinedSymbols(*object, symbols, error))
        {
            llvm::consumeError(std::move(childError));
            return false;
        }
    }
    if (childError)
    {
        error = "cannot list the members of '" + thinLtoPathUtf8(file) + "': " + llvm::toString(std::move(childError));
        return false;
    }
    return true;
}

bool ThinLtoBackend::requiresLldForStaticInput(
    const std::filesystem::path& file, bool& found, std::string& error)
{
    found = false;
    llvm::LLVMContext context;
    auto binary = llvm::object::createBinary(thinLtoPathUtf8(file), &context);
    if (!binary)
    {
        error = "cannot read '" + thinLtoPathUtf8(file) + "': " + llvm::toString(binary.takeError());
        return false;
    }
    const auto inspect = [&](const llvm::object::Binary& input) {
        if (llvm::isa<llvm::object::IRObjectFile>(&input))
        {
            // NSLink cannot consume LLVM bitcode, independently of TLS.
            found = true;
            return true;
        }
        if (const auto* object = llvm::dyn_cast<llvm::object::COFFObjectFile>(&input))
        {
            std::vector<std::string> symbols;
            if (!appendUndefinedSymbols(*object, symbols, error)) return false;
            if (std::find(symbols.begin(), symbols.end(), "_tls_index") != symbols.end()) found = true;
            for (const llvm::object::SectionRef& section : object->sections())
            {
                auto name = section.getName();
                if (!name)
                {
                    error = llvm::toString(name.takeError());
                    return false;
                }
                if (*name == ".tls" || name->starts_with(".tls$")) found = true;
            }
            return true;
        }
        if (llvm::isa<llvm::object::COFFImportFile>(&input)) return true;
        error = "unsupported input format while inspecting TLS in '" + thinLtoPathUtf8(file) + "'";
        return false;
    };
    if (const auto* archive = llvm::dyn_cast<llvm::object::Archive>(binary->getBinary()))
    {
        llvm::Error childError = llvm::Error::success();
        for (const llvm::object::Archive::Child& child : archive->children(childError))
        {
            auto member = child.getAsBinary(&context);
            if (!member)
            {
                error = "cannot read a member of '" + thinLtoPathUtf8(file) + "': " +
                    llvm::toString(member.takeError());
                llvm::consumeError(std::move(childError));
                return false;
            }
            if (!inspect(**member))
            {
                llvm::consumeError(std::move(childError));
                return false;
            }
        }
        if (childError)
        {
            error = "cannot list members of '" + thinLtoPathUtf8(file) + "': " + llvm::toString(std::move(childError));
            return false;
        }
        return true;
    }
    return inspect(*binary->getBinary());
}

// EN: T16-003. The first input the link takes with a C or C++ dynamic initializer: a pointer in `.CRT$XCU` or
//     `.CRT$XIU`, which only the C runtime startup calls. The objects are always linked; an archive member only
//     when it defines a symbol that the linked inputs still need, as the linker selects it, so a library whose
//     member with initializers is not used (`excptptr.obj` of libcpmt, for instance) does not count. Bitcode and
//     import members have none.
// ES: T16-003. La primera entrada que toma el enlace con un inicializador dinamico de C o C++: un puntero en
//     `.CRT$XCU` o `.CRT$XIU`, que solo llama el arranque del runtime de C. Los objetos siempre se enlazan; un
//     miembro de archivo solo cuando define un simbolo que las entradas enlazadas aun necesitan, como lo elige el
//     enlazador, asi que no cuenta una biblioteca cuyo miembro con inicializadores no se usa (`excptptr.obj` de
//     libcpmt, por ejemplo). Los miembros bitcode y de importacion no tienen.
bool ThinLtoBackend::findLinkedCrtInitializers(std::span<const std::filesystem::path> objects,
                                               std::span<const std::filesystem::path> libraries, std::string& input,
                                               std::string& member, std::string& error)
{
    input.clear();
    member.clear();
    llvm::LLVMContext context;
    std::unordered_set<std::string> defined;
    std::unordered_set<std::string> requested;
    std::vector<std::string> pending;
    // Adds the symbols of a linked object; true in `initializers` when it has a dynamic initializer.
    // Agrega los simbolos de un objeto enlazado; `initializers` es true si tiene un inicializador dinamico.
    const auto link = [&](const llvm::object::Binary& binary, bool& initializers) {
        initializers = false;
        const auto* object = llvm::dyn_cast<llvm::object::COFFObjectFile>(&binary);
        if (object == nullptr) return true;
        for (const llvm::object::SectionRef& section : object->sections())
        {
            auto name = section.getName();
            if (!name)
            {
                error = llvm::toString(name.takeError());
                return false;
            }
            if (*name == ".CRT$XCU" || *name == ".CRT$XIU") initializers = true;
        }
        for (const llvm::object::SymbolRef& symbol : object->symbols())
        {
            auto flags = symbol.getFlags();
            if (!flags)
            {
                error = llvm::toString(flags.takeError());
                return false;
            }
            if ((*flags & llvm::object::SymbolRef::SF_Global) == 0) continue;
            auto name = symbol.getName();
            if (!name)
            {
                error = llvm::toString(name.takeError());
                return false;
            }
            if ((*flags & llvm::object::SymbolRef::SF_Undefined) != 0)
            {
                if (requested.insert(name->str()).second) pending.push_back(name->str());
            }
            else
                defined.insert(name->str());
        }
        return true;
    };
    std::vector<llvm::object::OwningBinary<llvm::object::Binary>> inputs;
    std::vector<std::filesystem::path> archivePaths;
    std::vector<std::unordered_set<std::uint64_t>> pulled;
    const auto open = [&](const std::filesystem::path& file, bool always) {
        auto binary = llvm::object::createBinary(thinLtoPathUtf8(file), &context);
        if (!binary)
        {
            error = "cannot read '" + thinLtoPathUtf8(file) + "': " + llvm::toString(binary.takeError());
            return false;
        }
        if (llvm::isa<llvm::object::Archive>(binary->getBinary()))
        {
            if (always)
            {
                error = "'" + thinLtoPathUtf8(file) + "' is an archive, not an object";
                return false;
            }
            inputs.push_back(std::move(*binary));
            archivePaths.push_back(file);
            pulled.emplace_back();
            return true;
        }
        bool initializers = false;
        if (!link(*binary->getBinary(), initializers)) return false;
        if (initializers && input.empty()) input = thinLtoPathUtf8(file);
        return true;
    };
    for (const std::filesystem::path& file : objects)
        if (!open(file, true)) return false;
    for (const std::filesystem::path& file : libraries)
        if (!open(file, false)) return false;
    if (!input.empty()) return true;
    while (!pending.empty())
    {
        const std::string name = std::move(pending.back());
        pending.pop_back();
        if (defined.contains(name)) continue;
        for (std::size_t index = 0; index < inputs.size(); ++index)
        {
            const auto* archive = llvm::cast<llvm::object::Archive>(inputs[index].getBinary());
            auto child = archive->findSym(name);
            if (!child)
            {
                error = "cannot search '" + thinLtoPathUtf8(archivePaths[index]) + "': " +
                    llvm::toString(child.takeError());
                return false;
            }
            if (!*child) continue;
            if (!pulled[index].insert((*child)->getChildOffset()).second) break;
            auto binary = (*child)->getAsBinary(&context);
            if (!binary)
            {
                error = "cannot read a member of '" + thinLtoPathUtf8(archivePaths[index]) + "': " +
                    llvm::toString(binary.takeError());
                return false;
            }
            bool initializers = false;
            if (!link(**binary, initializers)) return false;
            if (initializers)
            {
                auto childName = (*child)->getName();
                member = childName ? childName->str() : std::string("?");
                if (!childName) llvm::consumeError(childName.takeError());
                input = thinLtoPathUtf8(archivePaths[index]);
                return true;
            }
            break;
        }
    }
    return true;
}

namespace
{
// The qualified C++ name of a Microsoft decorated function symbol (`A::one`), empty for any other symbol.
// El nombre calificado C++ de un simbolo de funcion decorado por Microsoft (`A::one`), vacio en otro caso.
std::string cppQualifiedFunctionName(llvm::StringRef symbol)
{
    if (!symbol.starts_with("?")) return {};
    char* text = llvm::microsoftDemangle(std::string_view(symbol.data(), symbol.size()), nullptr, nullptr,
        llvm::MSDemangleFlags(llvm::MSDF_NoCallingConvention | llvm::MSDF_NoReturnType |
                              llvm::MSDF_NoAccessSpecifier | llvm::MSDF_NoMemberType));
    if (text == nullptr) return {};
    std::string result(text);
    std::free(text);
    const auto parenthesis = result.find('(');
    return parenthesis == std::string::npos ? std::string{} : result.substr(0, parenthesis);
}
}

std::string ThinLtoBackend::demangleSymbol(std::string_view symbol)
{
    return llvm::demangle(symbol);
}

// EN: T17-006. A symbol the program needs is missing when no object defines it and no library provides it,
//     directly or as the `__imp_` import of a DLL. For a decorated C++ function, the library symbols of the same
//     qualified name are the definitions whose declaration differs from the header's: the parameter types, the
//     calling convention or the qualifiers change the decoration.
// ES: T17-006. Un simbolo que necesita el programa falta si ningun objeto lo define y ninguna biblioteca lo da,
//     directamente o como import `__imp_` de una DLL. Para una funcion C++ decorada, los simbolos de biblioteca
//     del mismo nombre calificado son definiciones cuya declaracion difiere de la del encabezado: los tipos de
//     parametros, la convencion o los calificadores cambian la decoracion.
bool ThinLtoBackend::findMissingLinkSymbols(
    std::span<const std::filesystem::path> objects, std::span<const std::filesystem::path> libraries,
    std::span<const std::string> symbols,
    std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::string>>>>& missing,
    std::string& error)
{
    missing.clear();
    llvm::LLVMContext context;
    std::unordered_set<std::string> defined;
    std::unordered_set<std::string> referenced;
    std::vector<llvm::object::OwningBinary<llvm::object::Binary>> archives;
    std::vector<std::filesystem::path> archivePaths;
    // EN: Item 1.9. Any symbolic file counts, COFF objects and the bitcode objects NSC generates for C/C++ inline
    //     code alike; reading only COFF reported symbols that a bitcode object defines as missing.
    // ES: Item 1.9. Cuenta cualquier archivo simbolico, objetos COFF y los objetos bitcode que NSC genera para el
    //     codigo inline C/C++; leer solo COFF informaba como ausentes simbolos que define un objeto bitcode.
    const auto addDefined = [&](const llvm::object::Binary& binary) {
        const auto* object = llvm::dyn_cast<llvm::object::SymbolicFile>(&binary);
        if (object == nullptr) return true;
        for (const llvm::object::BasicSymbolRef& symbol : object->symbols())
        {
            auto flags = symbol.getFlags();
            if (!flags)
            {
                error = llvm::toString(flags.takeError());
                return false;
            }
            if ((*flags & llvm::object::SymbolRef::SF_Global) == 0) continue;
            std::string name;
            llvm::raw_string_ostream stream(name);
            if (auto printed = symbol.printName(stream))
            {
                error = llvm::toString(std::move(printed));
                return false;
            }
            stream.flush();
            // Item 1.9: only a symbol some object refers to can be missing. / Solo puede faltar un simbolo que algun
            //     objeto referencia.
            if ((*flags & llvm::object::SymbolRef::SF_Undefined) != 0) referenced.insert(name);
            else defined.insert(name);
        }
        return true;
    };
    const auto open = [&](const std::filesystem::path& file) {
        auto binary = llvm::object::createBinary(thinLtoPathUtf8(file), &context);
        if (!binary)
        {
            error = "cannot read '" + thinLtoPathUtf8(file) + "': " + llvm::toString(binary.takeError());
            return false;
        }
        if (llvm::isa<llvm::object::Archive>(binary->getBinary()))
        {
            archives.push_back(std::move(*binary));
            archivePaths.push_back(file);
            return true;
        }
        return addDefined(*binary->getBinary());
    };
    for (const std::filesystem::path& file : objects)
        if (!open(file)) return false;
    for (const std::filesystem::path& file : libraries)
        if (!open(file)) return false;
    const auto provided = [&](const std::string& name) {
        for (auto& archive : archives)
        {
            auto child = llvm::cast<llvm::object::Archive>(archive.getBinary())->findSym(name);
            if (!child)
            {
                llvm::consumeError(child.takeError());
                continue;
            }
            if (*child) return true;
        }
        return false;
    };
    for (const std::string& symbol : symbols)
    {
        const std::string imported = "__imp_" + symbol;
        if (!referenced.contains(symbol) && !referenced.contains(imported)) continue;
        if (defined.contains(symbol) || defined.contains(imported) || provided(symbol) || provided(imported))
            continue;
        std::vector<std::pair<std::string, std::string>> sameName;
        const std::string qualified = cppQualifiedFunctionName(symbol);
        const auto separator = qualified.rfind("::");
        const std::string prefix = "?" + (separator == std::string::npos ? qualified : qualified.substr(separator + 2)) + "@";
        for (std::size_t index = 0; !qualified.empty() && index < archives.size() && sameName.size() < 4; ++index)
        {
            const auto* archive = llvm::cast<llvm::object::Archive>(archives[index].getBinary());
            for (const llvm::object::Archive::Symbol& entry : archive->symbols())
            {
                const llvm::StringRef name = entry.getName();
                if (name == symbol || !name.starts_with(prefix) || cppQualifiedFunctionName(name) != qualified)
                    continue;
                sameName.emplace_back(thinLtoPathUtf8(archivePaths[index]), name.str());
                if (sameName.size() == 4) break;
            }
        }
        missing.emplace_back(symbol, std::move(sameName));
    }
    return true;
}

std::string ThinLtoBackend::compatibilityIdentity(const LlvmTargetMachine& targetMachine, LlvmOptimizationLevel level)
{
    // The exact source revision of LLVM, when this LLVM build records one (item 53).
    // La revision exacta de las fuentes de LLVM, cuando este build de LLVM la registra (punto 53).
#if defined(LLVM_REVISION)
    const std::string revision = std::string("llvm-revision ") + LLVM_REVISION + "\n";
#else
    const std::string revision = "llvm-revision none\n";
#endif
    return std::string("llvm ") + LLVM_VERSION_STRING + "\n" + revision +
        "triple " + targetMachine.triple() + "\n" +
        "cpu " + targetMachine.cpuName() + "\n" +
        "features " + targetMachine.featureString() + "\n" +
        "layout " + targetMachine.dataLayout() + "\n" +
        "optimization O" + std::to_string(levelNumber(level)) + "\n";
}

bool ThinLtoBackend::addModule(
    std::size_t ordinal,
    LlvmModule& module,
    const LlvmTargetMachine& targetMachine,
    LlvmOptimizationLevel level,
    std::string& error)
{
    if (ordinal >= bitcode_.size() || module.module_ == nullptr || targetMachine.targetMachine_ == nullptr)
    {
        error = "ThinLTO received an invalid code unit";
        return false;
    }
    LLVMModuleRef moduleRef = static_cast<LLVMModuleRef>(module.module_);
    LLVMPassBuilderOptionsRef options = LLVMCreatePassBuilderOptions();
    const std::string pipeline = "thinlto-pre-link<O" + std::to_string(levelNumber(level)) + ">";
    LLVMErrorRef failure = LLVMRunPasses(
        moduleRef, pipeline.c_str(), static_cast<LLVMTargetMachineRef>(targetMachine.targetMachine_), options);
    LLVMDisposePassBuilderOptions(options);
    if (failure != nullptr)
    {
        char* message = LLVMGetErrorMessage(failure);
        error = "ThinLTO pre-link pipeline failed: " + std::string(message != nullptr ? message : "");
        LLVMDisposeErrorMessage(message);
        return false;
    }
    llvm::Module& unwrapped = *llvm::unwrap(moduleRef);
    llvm::ProfileSummaryInfo profile(unwrapped);
    const llvm::ModuleSummaryIndex index = llvm::buildModuleSummaryIndex(unwrapped, nullptr, &profile);
    std::string bytes;
    llvm::raw_string_ostream stream(bytes);
    // The module hash (GenerateHash) is what lets LLVM cache this unit's backend; without it the
    // in-process backend always runs.
    // El hash de modulo (GenerateHash) es lo que permite a LLVM cachear el backend de esta unidad;
    // sin el, el backend en proceso siempre corre.
    llvm::WriteBitcodeToFile(unwrapped, stream, false, &index, true);
    stream.flush();
    bitcode_[ordinal] = std::move(bytes);
    // ThinLTO keys modules by identifier, so it is the stable ordinal and never a path.
    // ThinLTO identifica los modulos por su identificador: el ordinal estable, nunca una ruta.
    identifiers_[ordinal] = "nativesharp-thinlto-unit-" + std::to_string(ordinal);
    return true;
}

bool ThinLtoBackend::run(
    std::span<const std::filesystem::path> objectFiles,
    const LlvmTargetMachine& targetMachine,
    LlvmOptimizationLevel level,
    std::size_t threads,
    const std::filesystem::path& cacheDirectory,
    std::string& error)
{
    if (objectFiles.size() != bitcode_.size())
    {
        error = "ThinLTO object list does not match its code units";
        return false;
    }
    importedFunctions_ = 0;
    cachedBackends_ = 0;
    backendRuns_ = 0;
    llvm::lto::Config config;
    config.CPU = targetMachine.cpuName();
    config.MAttrs = splitFeatures(targetMachine.featureString());
    config.DefaultTriple = targetMachine.triple();
    config.OptLevel = levelNumber(level);
    config.CGOptLevel = codeGenerationLevel(level);
    // LLVMCreateTargetMachine runs with LLVMRelocDefault and LLVMCodeModelDefault; let LLVM pick the
    // same effective models instead of the LTO defaults.
    // LLVMCreateTargetMachine usa LLVMRelocDefault y LLVMCodeModelDefault; LLVM elige los mismos
    // modelos efectivos en lugar de los valores por omision de LTO.
    config.RelocModel = std::nullopt;
    config.CodeModel = std::nullopt;
    // A final link drops what nothing references only when each function and data object has its
    // own section (item 50). Un enlace final solo tira lo que nadie referencia cuando cada funcion y
    // dato tiene su propia seccion (punto 50).
    config.Options.FunctionSections = internalize_;
    config.Options.DataSections = internalize_;
    config.DiagHandler = [this](const llvm::DiagnosticInfo& info)
    {
        if (info.getSeverity() != llvm::DS_Error) return;
        std::string text;
        llvm::raw_string_ostream stream(text);
        llvm::DiagnosticPrinterRawOStream printer(stream);
        info.print(printer);
        stream.flush();
        const std::lock_guard lock(diagnosticsMutex_);
        diagnostics_ += (diagnostics_.empty() ? "" : "; ") + text;
    };
    // Runs once per backend that really executes; a backend served by the cache skips it.
    // Se ejecuta una vez por backend que de verdad corre; uno servido por el cache no pasa aqui.
    config.PostImportModuleHook = [this](unsigned, const llvm::Module& module)
    {
        ++backendRuns_;
        importedFunctions_ += importedDefinitions(module);
        return true;
    };
    std::unordered_map<std::string, std::size_t> ordinalByIdentifier;
    for (std::size_t ordinal = 0; ordinal < identifiers_.size(); ++ordinal)
        ordinalByIdentifier.emplace(identifiers_[ordinal], ordinal);
    const auto ordinalFor = [&](unsigned task, const llvm::Twine& moduleName) -> std::size_t
    {
        const auto found = ordinalByIdentifier.find(moduleName.str());
        if (found != ordinalByIdentifier.end()) return found->second;
        return task == 0 ? objectFiles.size() : static_cast<std::size_t>(task - 1U);
    };
    if (captureFinalModuleTexts_)
    {
        config.PostOptModuleHook = [this](unsigned task, const llvm::Module& module)
        {
            const std::size_t ordinal = task == 0 ? finalModuleTexts_.size() : task - 1U;
            if (ordinal >= finalModuleTexts_.size()) return true;
            std::string text;
            llvm::raw_string_ostream stream(text);
            module.print(stream, nullptr);
            stream.flush();
            finalModuleTexts_[ordinal] = std::move(text);
            return true;
        };
    }
    // EN: Each backend generates its unit's code here, so the WinEHPrepare workaround that
    //     LlvmTargetMachine::emit applies on the other paths runs here as the last step before code
    //     generation (see WinEhCatchSwitchPhiLegalization.h). LLVM hands the hook its own, non-const
    //     module as a const reference; the rewrite only adds a slot, loads and stores.
    // ES: Cada backend genera aqui el codigo de su unidad, asi que el rodeo de WinEHPrepare que
    //     LlvmTargetMachine::emit aplica en las otras rutas corre aqui como ultimo paso antes de generar
    //     codigo (ver WinEhCatchSwitchPhiLegalization.h). LLVM pasa al gancho su propio modulo, no
    //     constante, como referencia constante; la reescritura solo agrega una ranura, cargas y guardados.
    config.PreCodeGenModuleHook = [](unsigned, const llvm::Module& module)
    {
        legalizeWinEhCatchSwitchPhiUses(llvm::wrap(const_cast<llvm::Module*>(&module)));
        return true;
    };
    llvm::lto::ThinBackend backend = llvm::lto::createInProcessThinBackend(
        llvm::heavyweight_hardware_concurrency(static_cast<unsigned>(threads == 0 ? 1 : threads)));
    llvm::lto::LTO lto(std::move(config), std::move(backend));
    std::unordered_set<std::string> prevailing;
    for (std::size_t ordinal = 0; ordinal < bitcode_.size(); ++ordinal)
    {
        llvm::Expected<std::unique_ptr<llvm::lto::InputFile>> input =
            llvm::lto::InputFile::create(llvm::MemoryBufferRef(bitcode_[ordinal], identifiers_[ordinal]));
        if (!input)
        {
            error = "ThinLTO cannot read code unit " + std::to_string(ordinal) + ": " +
                llvm::toString(input.takeError());
            return false;
        }
        std::vector<llvm::lto::SymbolResolution> resolutions;
        for (const llvm::lto::InputFile::Symbol& symbol : (*input)->symbols())
        {
            llvm::lto::SymbolResolution resolution;
            if (!symbol.isUndefined())
            {
                resolution.Prevailing = prevailing.insert(symbol.getName().str()).second;
                resolution.FinalDefinitionInLinkageUnit = resolution.Prevailing;
                resolution.VisibleToRegularObj =
                    !internalize_ || preservedSymbols_.contains(symbol.getName().str());
            }
            resolutions.push_back(resolution);
        }
        if (llvm::Error added = lto.add(std::move(*input), resolutions))
        {
            error = "ThinLTO cannot add code unit " + std::to_string(ordinal) + ": " +
                llvm::toString(std::move(added));
            return false;
        }
    }
    // The backends write objects without an object path: LLVM records a stream's path as the
    // CodeView object name (S_OBJNAME), which made two identical -g builds differ by the stamp of
    // the temporary directory and a cached build differ from an uncached one.
    // Los backends escriben objetos sin ruta de objeto: LLVM guarda la ruta del stream como nombre
    // de objeto CodeView (S_OBJNAME), lo que hacia diferir dos builds -g identicos por el sello del
    // directorio temporal y un build con cache de uno sin cache.
    const llvm::AddStreamFn addStream = [&](unsigned task, const llvm::Twine& moduleName)
        -> llvm::Expected<std::unique_ptr<llvm::CachedFileStream>>
    {
        const std::size_t ordinal = ordinalFor(task, moduleName);
        if (ordinal >= objectFiles.size())
            return llvm::createStringError("ThinLTO produced an unexpected task " + std::to_string(task));
        std::error_code openError;
        auto output = std::make_unique<llvm::raw_fd_ostream>(
            thinLtoPathUtf8(objectFiles[ordinal]), openError, llvm::sys::fs::OF_None);
        if (openError) return llvm::errorCodeToError(openError);
        return std::make_unique<llvm::CachedFileStream>(std::move(output));
    };
    // The backend cache keeps one raw object per LLVM cache key, named llvmcache-<key> so that
    // llvm::pruneCache bounds it. A hit copies the entry to the unit's object; a miss lets the
    // backend write the object and stores a copy after the run.
    // El cache de backends guarda un objeto crudo por llave de LLVM, llamado llvmcache-<llave> para
    // que llvm::pruneCache lo acote. Un acierto copia la entrada al objeto de la unidad; un fallo deja
    // que el backend escriba el objeto y guarda una copia al terminar.
    std::mutex missesMutex;
    std::vector<std::pair<std::size_t, std::string>> misses;
    llvm::FileCache backendCache;
    if (!cacheDirectory.empty())
    {
        std::error_code createError;
        std::filesystem::create_directories(cacheDirectory, createError);
        if (createError)
        {
            error = "ThinLTO cannot create its backend cache: " + createError.message();
            return false;
        }
        backendCache = llvm::FileCache(
            [&](unsigned task, llvm::StringRef key, const llvm::Twine& moduleName) -> llvm::Expected<llvm::AddStreamFn>
            {
                const std::size_t ordinal = ordinalFor(task, moduleName);
                if (ordinal >= objectFiles.size())
                    return llvm::createStringError("ThinLTO produced an unexpected task " + std::to_string(task));
                const std::filesystem::path entry = cacheDirectory / ("llvmcache-" + key.str());
                std::error_code copyError;
                if (std::filesystem::is_regular_file(entry, copyError) && usableCachedThinLtoObject(entry) &&
                    std::filesystem::copy_file(
                        entry, objectFiles[ordinal], std::filesystem::copy_options::overwrite_existing, copyError))
                    return llvm::AddStreamFn();
                const std::lock_guard lock(missesMutex);
                misses.emplace_back(ordinal, key.str());
                return addStream;
            },
            thinLtoPathUtf8(cacheDirectory));
    }
    if (llvm::Error failed = lto.run(addStream, backendCache))
    {
        error = "ThinLTO failed: " + llvm::toString(std::move(failed));
        if (!diagnostics_.empty()) error += " (" + diagnostics_ + ")";
        return false;
    }
    cachedBackends_ = bitcode_.size() - std::min<std::size_t>(backendRuns_, bitcode_.size());
    if (!cacheDirectory.empty() && diagnostics_.empty())
    {
        // Publishing is best effort: a failed copy only costs a future miss.
        // Publicar es mejor esfuerzo: una copia fallida solo cuesta un fallo futuro.
        for (const auto& [ordinal, key] : misses)
        {
            const std::filesystem::path entry = cacheDirectory / ("llvmcache-" + key);
            std::filesystem::path temporary = entry;
            temporary += ".partial-" + thinLtoCacheProcessTag() + "-" + std::to_string(ordinal);
            std::error_code storeError;
            if (std::filesystem::copy_file(
                    objectFiles[ordinal], temporary, std::filesystem::copy_options::overwrite_existing, storeError))
                std::filesystem::rename(temporary, entry, storeError);
            if (storeError) std::filesystem::remove(temporary, storeError);
        }
    }
    if (!cacheDirectory.empty())
    {
        // The backend cache lives beside the artifact cache but outside its pruning index, so it is
        // bounded here: entries unused for a week go, and the directory stays under 1 GiB.
        // El cache de backends vive junto al cache de artefactos pero fuera de su indice de poda, asi
        // que se acota aqui: salen las entradas sin uso en una semana y el directorio queda bajo 1 GiB.
        llvm::CachePruningPolicy policy;
        policy.Expiration = std::chrono::hours(24 * 7);
        policy.MaxSizeBytes = 1024ULL * 1024ULL * 1024ULL;
        // Pruning is housekeeping: a failure keeps the cache and never fails the build.
        // La poda es mantenimiento: si falla se conserva el cache y el build no falla.
        llvm::Expected<bool> pruned = llvm::pruneCache(thinLtoPathUtf8(cacheDirectory), policy);
        if (!pruned) llvm::consumeError(pruned.takeError());
    }
    if (!diagnostics_.empty())
    {
        error = "ThinLTO reported errors: " + diagnostics_;
        return false;
    }
    return true;
}

#endif

void ThinLtoBackend::appendBitcodeUnit(std::string bitcode, std::string identifier)
{
    bitcode_.push_back(std::move(bitcode));
    identifiers_.push_back(std::move(identifier));
    finalModuleTexts_.emplace_back();
}

std::span<const std::string> ThinLtoBackend::unitBitcode() const noexcept
{
    return bitcode_;
}

void ThinLtoBackend::setPreservedSymbols(std::vector<std::string> symbols)
{
    internalize_ = true;
    preservedSymbols_.clear();
    for (std::string& symbol : symbols) preservedSymbols_.insert(std::move(symbol));
}

void ThinLtoBackend::setCaptureFinalModuleTexts(bool capture) noexcept
{
    captureFinalModuleTexts_ = capture;
}

std::span<const std::string> ThinLtoBackend::finalModuleTexts() const noexcept
{
    return finalModuleTexts_;
}

std::size_t ThinLtoBackend::importedFunctionCount() const noexcept
{
    return importedFunctions_;
}

std::size_t ThinLtoBackend::cachedBackendCount() const noexcept
{
    return cachedBackends_;
}
}
