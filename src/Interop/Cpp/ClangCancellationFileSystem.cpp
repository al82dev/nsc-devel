#include "ClangSessionSupport.h"

#include <system_error>
#include <utility>

namespace nativesharp::interop::detail
{
namespace
{
class CancellationFileSystem final : public llvm::vfs::ProxyFileSystem
{
public:
    CancellationFileSystem(llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> underlying,
                           CppCancellationToken cancellation)
        : llvm::vfs::ProxyFileSystem(std::move(underlying)), cancellation_(std::move(cancellation)) {}

    llvm::ErrorOr<llvm::vfs::Status> status(const llvm::Twine& path) override
    {
        if (cancellation_.cancelled()) return std::make_error_code(std::errc::operation_canceled);
        return ProxyFileSystem::status(path);
    }

    bool exists(const llvm::Twine& path) override
    {
        return !cancellation_.cancelled() && ProxyFileSystem::exists(path);
    }

    llvm::ErrorOr<std::unique_ptr<llvm::vfs::File>> openFileForRead(const llvm::Twine& path) override
    {
        if (cancellation_.cancelled()) return std::make_error_code(std::errc::operation_canceled);
        return ProxyFileSystem::openFileForRead(path);
    }

    llvm::vfs::directory_iterator dir_begin(const llvm::Twine& path, std::error_code& error) override
    {
        if (cancellation_.cancelled())
        {
            error = std::make_error_code(std::errc::operation_canceled);
            return {};
        }
        return ProxyFileSystem::dir_begin(path, error);
    }

    std::error_code getRealPath(const llvm::Twine& path, llvm::SmallVectorImpl<char>& output) override
    {
        if (cancellation_.cancelled()) return std::make_error_code(std::errc::operation_canceled);
        return ProxyFileSystem::getRealPath(path, output);
    }

    std::error_code isLocal(const llvm::Twine& path, bool& result) override
    {
        if (cancellation_.cancelled()) return std::make_error_code(std::errc::operation_canceled);
        return ProxyFileSystem::isLocal(path, result);
    }

private:
    CppCancellationToken cancellation_;
};
}

llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> makeCancellationFileSystem(
    llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> underlying, CppCancellationToken cancellation)
{
    return llvm::makeIntrusiveRefCnt<CancellationFileSystem>(std::move(underlying), std::move(cancellation));
}
}
