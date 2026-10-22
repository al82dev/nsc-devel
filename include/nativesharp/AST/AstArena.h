#pragma once

#include <cstddef>
#include <exception>
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
    ~AstArena() noexcept;

    // Transfers ownership of every object allocated by `other` without moving the objects
    // themselves. Pointers previously returned by `other.make<T>()` therefore remain valid.
    // This is used by parallel compiler workers: allocation stays private to each worker and the
    // coordinator absorbs the completed arena before publishing any of its pointers.
    void absorb(AstArena&& other);

    template <typename T, typename... Args>
    T* make(Args&&... args)
    {
        T* const object = std::construct_at(
            static_cast<T*>(allocate(sizeof(T), alignof(T))),
            std::forward<Args>(args)...);
        try
        {
            entries_.push_back(Entry{object, &destroy<T>});
        }
        catch (...)
        {
            try
            {
                std::destroy_at(object);
            }
            catch (...)
            {
                std::terminate();
            }
            throw;
        }
        return object;
    }

private:
    struct Block final
    {
        Block(std::size_t capacity, std::size_t alignment);
        Block(const Block&) = delete;
        Block& operator=(const Block&) = delete;
        Block(Block&& other) noexcept;
        Block& operator=(Block&& other) noexcept;
        ~Block() noexcept;

        void* allocate(std::size_t size, std::size_t alignment) noexcept;

        std::byte* data = nullptr;
        std::size_t capacity = 0;
        std::size_t used = 0;
        std::size_t alignment = 0;
    };

    struct Entry
    {
        void* pointer = nullptr;
        void (*deleter)(void*) = nullptr;
    };

    template <typename T>
    static void destroy(void* pointer)
    {
        std::destroy_at(static_cast<T*>(pointer));
    }

    void* allocate(std::size_t size, std::size_t alignment);

    std::vector<Block> blocks_;
    std::vector<Entry> entries_;
};
}
