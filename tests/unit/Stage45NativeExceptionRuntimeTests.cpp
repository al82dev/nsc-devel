#include "nativesharp/LLVM/CoffObjectVerifier.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/Runtime/WindowsRuntimeBuilder.h"

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
    tests.expect(module != nullptr, "Stage 45.8 builds the native Windows exception runtime");
    tests.expect(error.empty(), "Stage 45.8 runtime construction keeps the diagnostic empty");
    if (module == nullptr)
    {
        return tests.result();
    }

    std::string verificationError;
    tests.expect(module->verify(verificationError), "Stage 45.8 runtime passes LLVMVerifyModule");
    tests.expect(verificationError.empty(), "verified Stage 45.8 runtime has no LLVM diagnostic");

    const std::string text = module->text();
    tests.expect(text.find("@__nativesharp_exception_tls_index = global i32 -1") != std::string::npos,
        "runtime stores the process-wide TLS slot index with TLS_OUT_OF_INDEXES sentinel");
    tests.expect(text.find("declare i32 @TlsAlloc()") != std::string::npos,
        "runtime imports TlsAlloc");
    tests.expect(text.find("declare ptr @TlsGetValue(i32") != std::string::npos,
        "runtime imports TlsGetValue");
    tests.expect(text.find("declare i32 @TlsSetValue(i32, ptr") != std::string::npos,
        "runtime imports TlsSetValue");
    tests.expect(text.find("declare void @RaiseException(i32, i32, i32, ptr") != std::string::npos,
        "runtime imports RaiseException");

    tests.expect(text.find("define i32 @__nativesharp_eh_init()") != std::string::npos,
        "runtime defines explicit EH/TLS initialization");
    tests.expect(text.find("define ptr @__nativesharp_exception_current()") != std::string::npos,
        "runtime exposes the current NativeSharp exception for the executing thread");
    tests.expect(text.find("define void @__nativesharp_exception_clear()") != std::string::npos,
        "runtime exposes deterministic clearing of the current exception");
    tests.expect(text.find("define void @__nativesharp_throw(ptr") != std::string::npos,
        "runtime defines the native throw ABI");
    tests.expect(text.find("define i32 @__nativesharp_eh_filter(ptr") != std::string::npos,
        "runtime defines the SEH filter ABI reserved for Stage 45.9");

    tests.expect(text.find("call i32 @TlsSetValue") != std::string::npos,
        "throw/clear publish exception state through Win32 TLS rather than a process-global object pointer");
    tests.expect(text.find("call ptr @TlsGetValue") != std::string::npos,
        "exception_current reads thread-local exception state");
    tests.expect(text.find("call void @RaiseException") != std::string::npos,
        "throw transfers control through native Windows RaiseException");
    tests.expect(text.find("call i32 @__nativesharp_eh_init()") != std::string::npos,
        "native process startup initializes the TLS slot before user Main can create threads");
    tests.expect(text.find("exception_publish_ok") != std::string::npos,
        "throw checks TlsSetValue success before raising the SEH exception");
    tests.expect(text.find("i32 253") != std::string::npos,
        "TLS runtime failure terminates deterministically instead of entering reachable LLVM unreachable UB");
    tests.expect(text.find("-531737855") != std::string::npos ||
            text.find("3763239425") != std::string::npos,
        "runtime embeds NativeSharp private SEH code 0xE04E5301");
    tests.expect(text.find("i32 1, i32 1") != std::string::npos,
        "RaiseException is non-continuable and carries one ULONG_PTR exception parameter");
    tests.expect(text.find("ptrtoint ptr") != std::string::npos,
        "runtime transports the NativeSharp object address as the SEH exception parameter");
    tests.expect(text.find("is_nativesharp_exception") != std::string::npos,
        "SEH filter compares the incoming exception code against the NativeSharp private code");

    tests.expect(text.find("personality ptr @__C_specific_handler") != std::string::npos,
        "native process startup uses the Windows x64 personality for its panic boundary");
    tests.expect(text.find("panic.catchswitch") != std::string::npos &&
            text.find("panic.catchpad") != std::string::npos &&
            text.find("@__nativesharp_panic_eh_filter") != std::string::npos,
        "native process startup contains the dedicated NativeSharp panic funclet");
    tests.expect(text.find("cleanuppad") == std::string::npos,
        "the startup panic boundary does not invent an unrelated cleanup funclet");

    std::string targetError;
    std::unique_ptr<LlvmTargetMachine> target = LlvmTargetMachine::createWindowsX64(targetError);
    tests.expect(target != nullptr, "Stage 45.8 obtains the Windows x64 target machine");
    tests.expect(targetError.empty(), "Windows x64 target creation has no diagnostic");
    if (target != nullptr)
    {
        const std::filesystem::path objectPath =
            std::filesystem::temp_directory_path() / "nativesharp_stage45_8_runtime.obj";
        std::error_code cleanupError;
        std::filesystem::remove(objectPath, cleanupError);
        std::string emissionError;
        tests.expect(target->emit(*module, objectPath, LlvmCodeGenFileType::Object, emissionError),
            "Stage 45.8 native exception runtime emits a real Windows x64 COFF object");
        tests.expect(emissionError.empty(), "Stage 45.8 COFF emission keeps the diagnostic empty");
        CoffObjectVerifier verifier;
        tests.expect(verifier.verify(objectPath),
            "Stage 18 COFF verifier accepts the Stage 45.8 runtime object");
        std::filesystem::remove(objectPath, cleanupError);
    }

    return tests.result();
}
