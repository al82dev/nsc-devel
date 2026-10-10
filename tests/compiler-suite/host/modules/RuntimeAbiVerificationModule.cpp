#include "RuntimeTestSupport.h"

namespace nsc::suite
{
namespace
{
using namespace runtime_support;

void actualSymbolBoundaryCases(TestContext& context, std::size_t variant,
    std::string ir, std::string& error);
void expectImage(TestContext& context, runtime::RuntimeAbiImageKind imageKind,
    bool reverse, std::string& error);
void imagePartitionMatrix(TestContext& context, std::size_t variant, std::string& error);

bool verify(std::string_view ir, runtime::RuntimeAbiImageKind imageKind, std::string& error)
{
    return runtime::verifyRuntimeAbiContract(ir, imageKind, error);
}

void signatureParsingContract(TestContext& context, std::size_t variant)
{
    using runtime::RuntimeAbiImageKind;
    std::string error;
    if (variant == 1)
        context.expect(verify("define void @__nativesharp_start() {}\n",
            RuntimeAbiImageKind::EntryPoint, error) && error.empty(),
            "minimal canonical entry signature must verify");
    else if (variant == 2)
        context.expect(verify(imageIr(RuntimeAbiImageKind::Executable),
            RuntimeAbiImageKind::Executable, error),
            "representative signatures with named parameters must canonicalize");
    else if (variant == 3)
        context.expect(!verify("define void @__nativesharp_start(\n",
            RuntimeAbiImageKind::EntryPoint, error) && error.find("cannot determine") != std::string::npos,
            "truncated signature must be rejected at the parser boundary");
    else if (variant == 4)
        context.expect(!verify("define i32 @__nativesharp_start() {}\n",
            RuntimeAbiImageKind::EntryPoint, error) && error.find("type mismatch") != std::string::npos,
            "wrong return type must be rejected with a type diagnostic");
    else if (variant == 5)
        context.expect(verify("define dso_local void @__nativesharp_start() {}\r\n",
            RuntimeAbiImageKind::EntryPoint, error),
            "attributes and CRLF must preserve the canonical function type");
    else if (variant == 6)
        context.expect(verify(imageIr(RuntimeAbiImageKind::SupportLibrary),
            RuntimeAbiImageKind::SupportLibrary, error),
            "all support-library function arities must canonicalize");
    else if (variant == 7)
        context.expect(verify(imageIr(RuntimeAbiImageKind::Executable, true),
            RuntimeAbiImageKind::Executable, error),
            "signature parsing must not depend on declaration order");
    else
        for (std::size_t repetition = 0; repetition < 64; ++repetition)
            context.expect(verify(imageIr(RuntimeAbiImageKind::Executable),
                RuntimeAbiImageKind::Executable, error),
                "large repeated signature catalogues must complete deterministically");
}

void actualSymbolContract(TestContext& context, std::size_t variant)
{
    using runtime::RuntimeAbiImageKind;
    std::string ir = imageIr(RuntimeAbiImageKind::Executable);
    std::string error;
    if (variant == 1)
        context.expect(verify(imageIr(RuntimeAbiImageKind::EntryPoint),
            RuntimeAbiImageKind::EntryPoint, error), "single entry definition must be collected");
    else if (variant == 2)
        context.expect(verify(ir, RuntimeAbiImageKind::Executable, error),
            "complete function and global catalogue must be collected");
    else if (variant == 3)
    {
        ir += "define void @__nativesharp_start() {}\n";
        context.expect(!verify(ir, RuntimeAbiImageKind::Executable, error) &&
            error.find("duplicate runtime symbol") != std::string::npos,
            "duplicate NativeSharp symbol must be rejected");
    }
    else if (variant == 4)
    {
        const std::string data = "@__nativesharp_exception_tls_index = global i32 0\n";
        ir.replace(ir.find(data), data.size(),
            "define i32 @__nativesharp_exception_tls_index() {}\n");
        context.expect(!verify(ir, RuntimeAbiImageKind::Executable, error) &&
            error.find("kind mismatch") != std::string::npos,
            "function substituted for TLS data must be rejected");
    }
    else
        actualSymbolBoundaryCases(context, variant, std::move(ir), error);
}

void actualSymbolBoundaryCases(TestContext& context, std::size_t variant,
    std::string ir, std::string& error)
{
    using runtime::RuntimeAbiImageKind;
    if (variant == 5)
    {
        ir += "define void @__nativesharp_uncontracted() {}\n";
        context.expect(!verify(ir, RuntimeAbiImageKind::Executable, error) &&
            error.find("uncontracted runtime ABI symbol") != std::string::npos,
            "uncontracted NativeSharp symbol must be rejected");
    }
    else if (variant == 6)
    {
        const std::string original = "@__nativesharp_exception_tls_index = global i32 0\n";
        ir.replace(ir.find(original), original.size(),
            "@__nativesharp_exception_tls_index = global i64 0\n");
        context.expect(!verify(ir, RuntimeAbiImageKind::Executable, error) &&
            error.find("type mismatch") != std::string::npos,
            "global data type drift must be rejected");
    }
    else if (variant == 7)
        context.expect(verify(imageIr(RuntimeAbiImageKind::Executable, true),
            RuntimeAbiImageKind::Executable, error),
            "symbol collection must be independent of definition order");
    else
    {
        ir += imageIr(RuntimeAbiImageKind::Executable);
        context.expect(!verify(ir, RuntimeAbiImageKind::Executable, error) &&
            error.find("duplicate runtime symbol") != std::string::npos,
            "duplicate detection must remain bounded for a complete repeated catalogue");
    }
}

void imagePartitionContract(TestContext& context, std::size_t variant)
{
    using runtime::RuntimeAbiImageKind;
    std::string error;
    if (variant == 1)
        expectImage(context, RuntimeAbiImageKind::EntryPoint, false, error);
    else if (variant == 2)
        expectImage(context, RuntimeAbiImageKind::Executable, false, error);
    else if (variant == 3)
        context.expect(!verify({}, RuntimeAbiImageKind::EntryPoint, error) &&
            error.find("__nativesharp_start") != std::string::npos,
            "entry image without start must report the missing producer symbol");
    else if (variant == 4)
    {
        std::string ir = imageIr(RuntimeAbiImageKind::EntryPoint);
        ir += "define ptr @__nativesharp_alloc(i64 %p0) {}\n";
        context.expect(!verify(ir, RuntimeAbiImageKind::EntryPoint, error) &&
            error.find("unexpectedly defines") != std::string::npos,
            "entry image must reject support-runtime symbols");
    }
    else if (variant == 5)
    {
        std::string ir = imageIr(RuntimeAbiImageKind::SupportLibrary);
        ir += "define void @__nativesharp_start() {}\n";
        context.expect(!verify(ir, RuntimeAbiImageKind::SupportLibrary, error) &&
            error.find("unexpectedly defines") != std::string::npos,
            "support image must reject the separately supplied start symbol");
    }
    else
        imagePartitionMatrix(context, variant, error);
}

void expectImage(TestContext& context, runtime::RuntimeAbiImageKind imageKind,
    bool reverse, std::string& error)
{
    context.expect(verify(imageIr(imageKind, reverse), imageKind, error) && error.empty(),
        "runtime image must contain exactly its contracted partition");
}

void imagePartitionMatrix(TestContext& context, std::size_t variant, std::string& error)
{
    using runtime::RuntimeAbiImageKind;
    const bool reverse = variant == 7;
    const std::size_t repetitions = variant == 8 ? 32 : 1;
    for (std::size_t repetition = 0; repetition < repetitions; ++repetition)
    {
        expectImage(context, RuntimeAbiImageKind::Executable, reverse, error);
        expectImage(context, RuntimeAbiImageKind::SupportLibrary, reverse, error);
        expectImage(context, RuntimeAbiImageKind::EntryPoint, reverse, error);
    }
}
}

void registerRuntimeAbiVerificationModule(TestRegistry& registry)
{
    addRuntimeContract(registry, 5, "unit", signatureParsingContract, {}, {"abi-ir-signature"});
    addRuntimeContract(registry, 6, "unit", actualSymbolContract, {}, {"abi-ir-symbols"});
    addRuntimeContract(registry, 7, "unit", imagePartitionContract, {}, {"abi-image-partition"});
}
}
