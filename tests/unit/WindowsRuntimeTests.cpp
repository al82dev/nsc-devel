#include "nativesharp/LLVM/CoffObjectVerifier.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/Runtime/WindowsRuntimeBuilder.h"
#include "nativesharp/Runtime/RuntimeAbiContract.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <memory>
#include <string>

int main()
{
    using nativesharp::llvm_backend::CoffObjectVerifier;
    using nativesharp::llvm_backend::LlvmCodeGenFileType;
    using nativesharp::llvm_backend::LlvmModule;
    using nativesharp::llvm_backend::LlvmTargetMachine;
    using nativesharp::runtime::WindowsRuntimeBuilder;
    using nativesharp::tests::TestContext;

    TestContext tests;
    WindowsRuntimeBuilder builder;
    std::string error;
    std::unique_ptr<LlvmModule> module = builder.build(error);
    tests.expect(module != nullptr, "Windows runtime builder creates an LLVM module");
    tests.expect(error.empty(), "successful runtime construction keeps the diagnostic empty");

    if (module != nullptr)
    {
        std::string verificationError;
        tests.expect(module->verify(verificationError), "Windows runtime module passes LLVMVerifyModule");
        tests.expect(verificationError.empty(), "runtime LLVM verification keeps its diagnostic empty");

        const std::string text = module->text();
        std::string contractError;
        tests.expect(nativesharp::runtime::verifyRuntimeAbiContract(
                         text,
                         nativesharp::runtime::RuntimeAbiImageKind::Executable,
                         contractError),
            "runtime module satisfies the formal versioned ABI contract");
        tests.expect(contractError.empty(),
            "successful runtime ABI verification keeps its diagnostic empty");
        bool hasCompleteRuntimeAbi = true;
        for (const nativesharp::runtime::RuntimeAbiSymbol& symbol :
             nativesharp::runtime::kRuntimeAbiSymbols)
        {
            hasCompleteRuntimeAbi = hasCompleteRuntimeAbi &&
                text.find("@" + std::string(symbol.name)) != std::string::npos;
        }
        tests.expect(hasCompleteRuntimeAbi,
            "runtime module contains every symbol in runtime symbol set 2");
        tests.expect(text.find("define ptr @__nativesharp_alloc(i64") != std::string::npos,
            "runtime defines the NativeSharp allocation ABI");
        tests.expect(text.find("define void @__nativesharp_free(ptr") != std::string::npos,
            "runtime defines the NativeSharp deallocation ABI");
        tests.expect(text.find("define void @__nativesharp_trap(i32") != std::string::npos,
            "runtime defines the NativeSharp trap ABI");
        tests.expect(text.find("define void @__nativesharp_start()") != std::string::npos,
            "runtime defines the native Windows executable entry point");
        tests.expect(text.find("define i32 @puts(ptr") != std::string::npos,
            "runtime defines the Stage 19 puts compatibility shim");
        tests.expect(text.find("@_fltused = global i32 0") != std::string::npos,
            "runtime defines the MSVC floating-point marker symbol");
        tests.expect(text.find("declare void @ExitProcess(i32") != std::string::npos,
            "runtime imports ExitProcess from Win32");
        tests.expect(text.find("declare ptr @GetProcessHeap()") != std::string::npos,
            "runtime imports GetProcessHeap from Win32");
        tests.expect(text.find("declare ptr @HeapAlloc(ptr, i32, i64") != std::string::npos,
            "runtime imports HeapAlloc with a 64-bit SIZE_T");
        tests.expect(text.find("declare i32 @HeapFree(ptr, i32, ptr") != std::string::npos,
            "runtime imports HeapFree");
        tests.expect(text.find("declare ptr @GetStdHandle(i32") != std::string::npos,
            "runtime imports GetStdHandle for console output");
        tests.expect(text.find("declare i32 @WriteFile(ptr, ptr, i32, ptr, ptr") != std::string::npos,
            "runtime imports WriteFile for console output");
        tests.expect(text.find("ucrtbase") == std::string::npos &&
                text.find("printf") == std::string::npos,
            "runtime does not depend on CRT startup or formatted I/O");
        tests.expect(text.find("call i32 @Main()") != std::string::npos,
            "runtime entry point transfers control to NativeSharp Main");
        tests.expect(text.find("call void @ExitProcess(i32 %result)") != std::string::npos,
            "runtime terminates the process with Main's result");
        tests.expect(text.find("add i32 %0, 128") != std::string::npos ||
                text.find("add i32 %code, 128") != std::string::npos,
            "runtime maps NativeSharp trap reasons into dedicated process exit codes");
        tests.expect(text.find("getelementptr i8") != std::string::npos,
            "puts shim measures the NUL-terminated string explicitly");
        tests.expect(text.find("WriteFile") != std::string::npos &&
                text.find("GetStdHandle") != std::string::npos,
            "puts shim is built on direct Win32 console/file APIs");
        tests.expect(text.find("declare ptr @FindFirstFileW(ptr, ptr)") != std::string::npos &&
                text.find("declare i32 @FindNextFileW(ptr, ptr)") != std::string::npos,
            "directory enumeration uses the UTF-16 Win32 boundary");
        tests.expect(text.find("@FindFirstFileA") == std::string::npos &&
                text.find("@FindNextFileA") == std::string::npos,
            "directory enumeration never falls back to the active ANSI code page");
        tests.expect(text.find("declare i32 @MultiByteToWideChar") != std::string::npos &&
                text.find("declare i32 @WideCharToMultiByte") != std::string::npos,
            "runtime exposes both sides of the NativeSharp UTF-8 to Win32 UTF-16 boundary");
        tests.expect(text.find("declare i32 @GetFullPathNameW") != std::string::npos &&
                text.find("@GetFullPathNameA") == std::string::npos,
            "Path.GetFullPath normalizes through UTF-16 without the ANSI MAX_PATH boundary");

        std::string targetError;
        std::unique_ptr<LlvmTargetMachine> targetMachine = LlvmTargetMachine::createWindowsX64(targetError);
        tests.expect(targetMachine != nullptr, "runtime test obtains the Windows x64 TargetMachine");
        if (targetMachine != nullptr)
        {
            const std::filesystem::path objectPath =
                std::filesystem::temp_directory_path() / "nativesharp_stage19_runtime_test.obj";
            std::error_code cleanupError;
            std::filesystem::remove(objectPath, cleanupError);
            std::string emissionError;
            tests.expect(targetMachine->emit(*module, objectPath, LlvmCodeGenFileType::Object, emissionError),
                "runtime LLVM module emits to a real COFF object");
            CoffObjectVerifier verifier;
            tests.expect(verifier.verify(objectPath),
                "Stage 18 COFF verifier accepts the emitted Stage 19 runtime object");
            std::filesystem::remove(objectPath, cleanupError);
        }
    }

    return tests.result();
}
