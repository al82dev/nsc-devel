#pragma once

#include <memory>
#include <utility>
#include <vector>

namespace nativesharp::ast
{
class AstArena final
{
public:
    AstArena() = default;
    AstArena(const AstArena&) = delete;
    AstArena& operator=(const AstArena&) = delete;
    ~AstArena();

    template <typename T, typename... Args>
    T* make(Args&&... args)
    {
        std::unique_ptr<T> object = std::make_unique<T>(std::forward<Args>(args)...);
        entries_.push_back(Entry{object.get(), &destroy<T>});
        return object.release();
    }

private:
    struct Entry
    {
        void* pointer = nullptr;
        void (*deleter)(void*) = nullptr;
    };

    template <typename T>
    static void destroy(void* pointer)
    {
        delete static_cast<T*>(pointer);
    }

    std::vector<Entry> entries_;
};
}
