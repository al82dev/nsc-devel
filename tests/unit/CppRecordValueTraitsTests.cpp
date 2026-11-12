#include "nativesharp/Interop/CppClangSessionFactory.h"
#include "nativesharp/Interop/CppInteropSession.h"
#include <iostream>
#include <memory>
#include <string_view>
#include <variant>

using namespace nativesharp::interop;

int main(int argc, char** argv)
{
    if (argc != 3) return 1;
    CppSessionOptions options;
    options.language = CppLanguageMode::Cpp;
    options.languageStandard = "c++20";
    options.targetTriple = "x86_64-pc-windows-msvc";
    options.driverReferencePath = "clang.exe";
    options.resourceDirectory = argv[1];
    options.workingDirectory = argv[2];
    options.input = CppBufferInput{"record-traits.cpp", R"cpp(
        struct Plain { int x, y; };
        struct UserCtor { UserCtor(); int x; };
        struct UserCopy { UserCopy(const UserCopy&); int x; };
        struct DeletedCopy { DeletedCopy(const DeletedCopy&) = delete; int x; };
        struct UserDtor { ~UserDtor(); int x; };
        struct MixedAccess { int x; private: int y; };
        struct VolatileMember { volatile int x; };
        struct NestedVolatile { VolatileMember inner; };
        struct Incomplete;
    )cpp"};
    CppClangSessionFactory factory;
    auto opened = factory.open(std::move(options), {});
    if (const auto* failure = std::get_if<CppSessionOpenFailure>(&opened))
    {
        std::cerr << "session: " << failure->message << '\n';
        return 2;
    }
    auto session = std::move(std::get<std::unique_ptr<CppInteropSession>>(opened));
    if (!session) return 3;
    auto records = session->recordsSnapshot();
    if (!records) return 4;
    const auto find = [&](std::string_view name) -> const CppRecordDeclaration* {
        for (const auto& record : *records) if (record.name == name) return &record;
        return nullptr;
    };
    const auto check = [&](std::string_view name, CppRecordValueTraits expected) {
        const auto* record = find(name);
        if (!record || !record->valueTraits || *record->valueTraits != expected)
        {
            std::cerr << "unexpected traits for " << name << '\n';
            return false;
        }
        return true;
    };
    if (!check("Plain", {true, true, true, true, true}) ||
        !check("UserCtor", {true, true, false, true, false}) ||
        !check("UserCopy", {false, true, false, true, false}) ||
        !check("UserDtor", {false, true, true, false, false}) ||
        !check("MixedAccess", {true, false, true, true, false})) return 5;
    const auto* incomplete = find("Incomplete");
    if (!incomplete || incomplete->complete || incomplete->valueTraits) return 6;
    const auto* volatileMember = find("VolatileMember");
    const auto* nestedVolatile = find("NestedVolatile");
    if (!volatileMember || !volatileMember->valueTraits ||
        !volatileMember->valueTraits->hasNonLiteralOrVolatileFieldsOrBases ||
        !nestedVolatile || !nestedVolatile->valueTraits ||
        !nestedVolatile->valueTraits->hasNonLiteralOrVolatileFieldsOrBases) return 14;
    // Trivially copyable alone does not prove that an accessible copy exists.
    const auto* deletedCopy = find("DeletedCopy");
    if (!deletedCopy || !deletedCopy->valueTraits || !deletedCopy->valueTraits->triviallyCopyable) return 9;
    if (!session->close().succeeded) return 7;
    if (!find("Plain")->valueTraits->triviallyCopyable) return 8;

    CppSessionOptions cOptions;
    cOptions.language = CppLanguageMode::C;
    cOptions.languageStandard = "c11";
    cOptions.targetTriple = "x86_64-pc-windows-msvc";
    cOptions.driverReferencePath = "clang.exe";
    cOptions.resourceDirectory = argv[1];
    cOptions.workingDirectory = argv[2];
    cOptions.input = CppBufferInput{"record-traits.c", "struct CRecord { int x; };"};
    auto cOpened = factory.open(std::move(cOptions), {});
    if (!std::holds_alternative<std::unique_ptr<CppInteropSession>>(cOpened)) return 10;
    auto cSession = std::move(std::get<std::unique_ptr<CppInteropSession>>(cOpened));
    if (!cSession) return 11;
    const auto cRecords = cSession->recordsSnapshot();
    if (!cRecords || cRecords->size() != 1 || cRecords->front().isCxx ||
        cRecords->front().valueTraits) return 12;
    if (!cSession->close().succeeded) return 13;
    return 0;
}
