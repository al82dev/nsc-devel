#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Interop/CppInteropSessionFactory.h"

#include <memory>
#include <string>

using namespace nativesharp;

class FakeSession final : public interop::CppInteropSession
{
public:
    explicit FakeSession(int& destroyed) : destroyed_(destroyed) {}
    ~FakeSession() override { ++destroyed_; }
    interop::CppSessionState state() const noexcept override { return state_; }
    interop::CppSessionOptions optionsSnapshot() const override { return {}; }
    std::vector<interop::CppSessionDiagnostic> diagnosticsSnapshot() const override { return {}; }
    std::optional<std::size_t> topLevelDeclarationCount() const override { return {}; }
    std::optional<std::vector<interop::CppFunctionDeclaration>> freeFunctionsSnapshot() const override { return {}; }
    std::optional<std::vector<interop::CppNamespaceDeclaration>> namespacesSnapshot() const override { return {}; }
    std::optional<std::vector<interop::CppRecordDeclaration>> recordsSnapshot() const override { return {}; }
    std::optional<std::vector<interop::CppGlobalDeclaration>> globalsSnapshot() const override { return {}; }
    interop::CppSessionCloseResult close() override
    {
        state_ = interop::CppSessionState::Closed;
        return {};
    }
private:
    int& destroyed_;
    interop::CppSessionState state_ = interop::CppSessionState::Ready;
};

int main()
{
    int destroyed = 0;
    {
        frontend::ProjectCompilation compilation;
        if (compilation.cppInteropSession() != nullptr) return 1;
        std::string error;
        auto session = std::make_unique<FakeSession>(destroyed);
        auto* expected = session.get();
        if (!compilation.setCppInteropSession(std::move(session), error) || !error.empty()) return 2;
        if (compilation.cppInteropSession() != expected || destroyed != 0) return 3;
        // A failed first attempt must still prevent replacing the session.
        compilation.compileProjectSyntax(
            "A:/nsc-interop-build/nonexistent-interop-fixture.nsproj", error);
        auto replacement = std::make_unique<FakeSession>(destroyed);
        if (compilation.setCppInteropSession(std::move(replacement), error)) return 4;
        if (error.empty() || compilation.cppInteropSession() != expected || destroyed != 1) return 5;
    }
    if (destroyed != 2) return 6;
    {
        frontend::ProjectCompilation compilation;
        std::string error;
        auto closed = std::make_unique<FakeSession>(destroyed);
        closed->close();
        if (compilation.setCppInteropSession(std::move(closed), error)) return 7;
        if (error.empty() || compilation.cppInteropSession() != nullptr || destroyed != 3) return 8;
    }
    interop::CppCancellationToken original;
    auto copy = original;
    copy.cancel();
    if (!original.cancelled()) return 9;
    return 0;
}
