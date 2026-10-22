#include "nativesharp/AST/AstArena.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <new>
#include <utility>

namespace nativesharp::ast
{
namespace
{
constexpr std::size_t DefaultBlockCapacity = 64U * 1024U;
}

AstArena::Block::Block(std::size_t requestedCapacity, std::size_t requestedAlignment)
    : data(static_cast<std::byte*>(
          ::operator new(requestedCapacity, std::align_val_t(requestedAlignment)))),
      capacity(requestedCapacity),
      alignment(requestedAlignment)
{
}

AstArena::Block::Block(Block&& other) noexcept
    : data(std::exchange(other.data, nullptr)),
      capacity(std::exchange(other.capacity, 0)),
      used(std::exchange(other.used, 0)),
      alignment(std::exchange(other.alignment, 0))
{
}

AstArena::Block& AstArena::Block::operator=(Block&& other) noexcept
{
    if (this == &other) return *this;
    if (data != nullptr) ::operator delete(data, std::align_val_t(alignment));
    data = std::exchange(other.data, nullptr);
    capacity = std::exchange(other.capacity, 0);
    used = std::exchange(other.used, 0);
    alignment = std::exchange(other.alignment, 0);
    return *this;
}

AstArena::Block::~Block() noexcept
{
    if (data != nullptr) ::operator delete(data, std::align_val_t(alignment));
}

void* AstArena::Block::allocate(std::size_t size, std::size_t requestedAlignment) noexcept
{
    if (alignment < requestedAlignment || used > capacity) return nullptr;
    void* pointer = data + used;
    std::size_t space = capacity - used;
    void* const result = std::align(requestedAlignment, size, pointer, space);
    if (result == nullptr) return nullptr;
    used = static_cast<std::size_t>(static_cast<std::byte*>(result) - data) + size;
    return result;
}

void* AstArena::allocate(std::size_t size, std::size_t requestedAlignment)
{
    if (!blocks_.empty())
    {
        if (void* const result = blocks_.back().allocate(size, requestedAlignment)) return result;
    }

    const std::size_t blockAlignment =
        std::max(requestedAlignment, alignof(std::max_align_t));
    const std::size_t blockCapacity = std::max(size, DefaultBlockCapacity);
    blocks_.emplace_back(blockCapacity, blockAlignment);
    return blocks_.back().allocate(size, requestedAlignment);
}

void AstArena::absorb(AstArena&& other)
{
    if (this == &other || other.entries_.empty()) return;
    blocks_.reserve(blocks_.size() + other.blocks_.size());
    entries_.reserve(entries_.size() + other.entries_.size());
    blocks_.insert(blocks_.end(),
        std::make_move_iterator(other.blocks_.begin()),
        std::make_move_iterator(other.blocks_.end()));
    entries_.insert(entries_.end(), other.entries_.begin(), other.entries_.end());
    other.blocks_.clear();
    other.entries_.clear();
}

AstArena::~AstArena() noexcept
{
    for (auto iterator = entries_.rbegin(); iterator != entries_.rend(); ++iterator)
    {
        try
        {
            iterator->deleter(iterator->pointer);
        }
        catch (...)
        {
            // Arena teardown is best-effort: one invalid throwing destructor
            // must not prevent the remaining owned objects from being released.
        }
    }
}
}
