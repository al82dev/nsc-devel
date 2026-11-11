#include "nativesharp/Interop/CppClangSessionFactory.h"
#include "nativesharp/Interop/CppInteropSession.h"
#include <iostream>
#include <memory>
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
    options.input = CppBufferInput{"record-operations.cpp", R"cpp(
        struct Plain { int x; };
        struct DeletedCopy { DeletedCopy(const DeletedCopy&) = delete; int x; };
        struct Both { Both(Both&) = default; Both(const Both&) = delete; int x; };
        struct Ambiguous { Ambiguous(const Ambiguous&, int = 0); Ambiguous(const Ambiguous&, long = 0); };
        struct PrivateCopy { private: PrivateCopy(const PrivateCopy&) = default; public: int x; };
        struct PrivateDtor { private: ~PrivateDtor() = default; public: int x; };
        struct DeletedDtor { ~DeletedDtor() = delete; int x; };
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
    using Status = CppSpecialMemberSelectionStatus;
    const auto selected = [](const CppSpecialMemberSelection& s, CppAccess access) {
        return s.status == Status::Selected && s.access == access && s.isDeleted == false && s.isTrivial == true;
    };
    const auto plain = session->recordValueOperations("Plain");
    if (!plain || !selected(plain->copyFromConst, CppAccess::Public) ||
        !selected(plain->copyFromMutable, CppAccess::Public) || !selected(plain->destructor, CppAccess::Public)) return 4;
    const auto deleted = session->recordValueOperations("DeletedCopy");
    if (!deleted || deleted->copyFromConst.status != Status::MissingOrDeleted ||
        deleted->copyFromConst.isDeleted != true) return 5;
    const auto both = session->recordValueOperations("Both");
    if (!both || both->copyFromConst.status != Status::MissingOrDeleted ||
        !selected(both->copyFromMutable, CppAccess::Public)) return 6;
    const auto ambiguous = session->recordValueOperations("Ambiguous");
    if (!ambiguous || ambiguous->copyFromConst.status != Status::Ambiguous ||
        ambiguous->copyFromConst.access || ambiguous->copyFromConst.isDeleted ||
        ambiguous->copyFromConst.isTrivial) return 12;
    const auto privateCopy = session->recordValueOperations("PrivateCopy");
    if (!privateCopy || !selected(privateCopy->copyFromConst, CppAccess::Private)) return 7;
    const auto privateDtor = session->recordValueOperations("PrivateDtor");
    if (!privateDtor || !selected(privateDtor->destructor, CppAccess::Private)) return 8;
    const auto deletedDtor = session->recordValueOperations("DeletedDtor");
    if (!deletedDtor || deletedDtor->destructor.status != Status::MissingOrDeleted ||
        deletedDtor->destructor.isDeleted != true) return 9;
    if (session->recordValueOperations("Incomplete") || session->recordValueOperations("Missing")) return 10;
    if (!session->close().succeeded || session->recordValueOperations("Plain")) return 11;
    CppSessionOptions cOptions;
    cOptions.language = CppLanguageMode::C;
    cOptions.languageStandard = "c11";
    cOptions.targetTriple = "x86_64-pc-windows-msvc";
    cOptions.driverReferencePath = "clang.exe";
    cOptions.resourceDirectory = argv[1];
    cOptions.workingDirectory = argv[2];
    cOptions.input = CppBufferInput{"record-operations.c", "struct Plain { int x; };"};
    auto cOpened = factory.open(std::move(cOptions), {});
    if (!std::holds_alternative<std::unique_ptr<CppInteropSession>>(cOpened)) return 13;
    auto cSession = std::move(std::get<std::unique_ptr<CppInteropSession>>(cOpened));
    if (!cSession || cSession->recordValueOperations("Plain") || !cSession->close().succeeded) return 14;
    return 0;
}
