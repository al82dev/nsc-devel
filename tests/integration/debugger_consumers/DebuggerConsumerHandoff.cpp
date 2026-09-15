#define NOMINMAX
#include <Windows.h>
#include <DbgHelp.h>
#include <dia2.h>
#include <diacreate.h>

#include <array>
#include <cwchar>
#include <iostream>
#include <string>
#include <string_view>

namespace
{
template <typename T>
class ComPtr
{
public:
    ~ComPtr() { if (value_ != nullptr) value_->Release(); }
    T** put() { return &value_; }
    T* operator->() const { return value_; }
    explicit operator bool() const { return value_ != nullptr; }

private:
    T* value_ = nullptr;
};

bool contains(std::wstring_view value, std::wstring_view needle)
{
    return value.find(needle) != std::wstring_view::npos;
}

bool verifyDia(const wchar_t* diaDll, const wchar_t* pdbPath)
{
    ComPtr<IDiaDataSource> source;
    HRESULT result = NoRegCoCreate(
        diaDll, CLSID_DiaSource, IID_IDiaDataSource,
        reinterpret_cast<void**>(source.put()));
    if (FAILED(result) || !source)
    {
        std::wcerr << L"DIA creation failed: 0x" << std::hex << result << L'\n';
        return false;
    }
    result = source->loadDataFromPdb(pdbPath);
    if (FAILED(result))
    {
        std::wcerr << L"DIA PDB load failed: 0x" << std::hex << result << L'\n';
        return false;
    }
    ComPtr<IDiaSession> session;
    if (FAILED(source->openSession(session.put())) || !session) return false;
    ComPtr<IDiaSymbol> global;
    if (FAILED(session->get_globalScope(global.put())) || !global) return false;
    ComPtr<IDiaEnumSymbols> symbols;
    if (FAILED(global->findChildren(SymTagFunction, nullptr, nsNone, symbols.put())) || !symbols)
        return false;

    std::array<bool, 4> found{};
    constexpr std::array<std::wstring_view, 4> required = {
        L"InspectSehStack", L"CaptureComFrames", L"StackCounter.CaptureDepth",
        L"InspectClosureStack.lambda#0"};
    while (true)
    {
        IDiaSymbol* raw = nullptr;
        ULONG fetched = 0;
        if (symbols->Next(1, &raw, &fetched) != S_OK || fetched == 0) break;
        ComPtr<IDiaSymbol> symbol;
        *symbol.put() = raw;
        BSTR name = nullptr;
        if (SUCCEEDED(symbol->get_name(&name)) && name != nullptr)
        {
            const std::wstring_view view(name, SysStringLen(name));
            for (std::size_t index = 0; index < required.size(); ++index)
                if (contains(view, required[index])) found[index] = true;
            SysFreeString(name);
        }
    }
    for (std::size_t index = 0; index < required.size(); ++index)
    {
        if (!found[index])
        {
            std::wcerr << L"DIA did not expose " << required[index] << L'\n';
            return false;
        }
    }
    return true;
}

struct DbgHelpState
{
    std::array<bool, 4> found{};
};

BOOL CALLBACK enumerateSymbol(PSYMBOL_INFOW symbol, ULONG, void* context)
{
    auto& state = *static_cast<DbgHelpState*>(context);
    const std::wstring_view name(symbol->Name, symbol->NameLen);
    constexpr std::array<std::wstring_view, 4> required = {
        L"InspectSehStack", L"CaptureComFrames", L"CaptureDepth", L"InspectClosureStack"};
    for (std::size_t index = 0; index < required.size(); ++index)
        if (contains(name, required[index])) state.found[index] = true;
    return TRUE;
}

bool verifyDbgHelp(const wchar_t* executablePath)
{
    HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
    if (!SymInitializeW(process, nullptr, FALSE)) return false;
    const DWORD64 base = SymLoadModuleExW(
        process, nullptr, executablePath, nullptr, 0, 0, nullptr, 0);
    if (base == 0)
    {
        SymCleanup(process);
        return false;
    }
    DbgHelpState state;
    const BOOL enumerated = SymEnumSymbolsW(process, base, nullptr, enumerateSymbol, &state);
    SymUnloadModule64(process, base);
    SymCleanup(process);
    if (!enumerated) return false;
    for (bool found : state.found)
        if (!found) return false;
    return true;
}
}

int wmain(int argc, wchar_t** argv)
{
    if (argc != 4)
    {
        std::wcerr << L"usage: debugger-consumer <msdia.dll> <exe> <pdb>\n";
        return 2;
    }
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(initialized)) return 3;
    const bool dia = verifyDia(argv[1], argv[3]);
    const bool dbghelp = verifyDbgHelp(argv[2]);
    CoUninitialize();
    if (!dia || !dbghelp)
    {
        std::wcerr << L"debugger consumer handoff failed (DIA=" << dia
                   << L", DbgHelp=" << dbghelp << L")\n";
        return 4;
    }
    std::wcout << L"Visual Studio DIA and Windows DbgHelp consumed the NativeSharp PDB\n";
    return 0;
}
