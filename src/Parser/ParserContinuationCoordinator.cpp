#include "nativesharp/Parser/Parser.h"

#include <cassert>
#include <cstddef>
#include <deque>
#include <limits>
#include <vector>

namespace nativesharp::parser
{
class Parser::ParseContinuationCoordinator final
{
public:
    explicit ParseContinuationCoordinator(Parser& parser) noexcept;
    ~ParseContinuationCoordinator() noexcept;

    ast::AstNode* parse(ContinuationRequestKind kind);

private:
    static constexpr std::size_t noFrame = std::numeric_limits<std::size_t>::max();

    struct ResultSlot final
    {
        ast::AstNode* value = nullptr;
        bool ready = false;
    };

    struct BlockFrame final
    {
        std::size_t beginIndex = 0;
        std::size_t beforeIndex = 0;
        ast::AstNode* block = nullptr;
        ast::AstNode* destination = nullptr;
    };

    struct Work final
    {
        ContinuationRequestKind kind = ContinuationRequestKind::Expression;
        std::size_t resultSlot = 0;
        std::size_t frameIndex = noFrame;
    };

    std::size_t request(ContinuationRequestKind kind);
    void drain(std::size_t resultSlot);
    void execute(Work work);
    ast::AstNode* parseDirect(ContinuationRequestKind kind);
    void finishBlockFrame(std::size_t frameIndex, ast::AstNode* block);

    Parser& parser_;
    ParseContinuationCoordinator* previous_ = nullptr;
    std::deque<ResultSlot> resultSlots_;
    std::deque<BlockFrame> blockFrames_;
    std::vector<Work> work_;
};

Parser::ParseContinuationCoordinator::ParseContinuationCoordinator(Parser& parser) noexcept
    : parser_(parser), previous_(parser.activeContinuationCoordinator_)
{
    parser_.activeContinuationCoordinator_ = this;
}

Parser::ParseContinuationCoordinator::~ParseContinuationCoordinator() noexcept
{
    parser_.activeContinuationCoordinator_ = previous_;
}

ast::AstNode* Parser::ParseContinuationCoordinator::parse(ContinuationRequestKind kind)
{
    const std::size_t resultSlot = request(kind);
    drain(resultSlot);
    return resultSlots_[resultSlot].value;
}

std::size_t Parser::ParseContinuationCoordinator::request(ContinuationRequestKind kind)
{
    const std::size_t resultSlot = resultSlots_.size();
    resultSlots_.push_back({});
    std::size_t frameIndex = noFrame;
    if (kind == ContinuationRequestKind::Block)
    {
        frameIndex = blockFrames_.size();
        blockFrames_.push_back({parser_.index_, parser_.index_, nullptr, nullptr});
    }
    work_.push_back({kind, resultSlot, frameIndex});
    return resultSlot;
}

void Parser::ParseContinuationCoordinator::drain(std::size_t resultSlot)
{
    while (!resultSlots_[resultSlot].ready)
    {
        assert(!work_.empty());
        const Work work = work_.back();
        work_.pop_back();
        execute(work);
    }
}

void Parser::ParseContinuationCoordinator::execute(Work work)
{
    if (work.frameIndex != noFrame) blockFrames_[work.frameIndex].beforeIndex = parser_.index_;
    ast::AstNode* value = parseDirect(work.kind);
    if (work.frameIndex != noFrame) finishBlockFrame(work.frameIndex, value);
    resultSlots_[work.resultSlot].value = value;
    resultSlots_[work.resultSlot].ready = true;
}

ast::AstNode* Parser::ParseContinuationCoordinator::parseDirect(ContinuationRequestKind kind)
{
    switch (kind)
    {
    case ContinuationRequestKind::Expression:
        return parser_.parseExpressionImpl();
    case ContinuationRequestKind::Block:
        return parser_.parseBlockImpl();
    case ContinuationRequestKind::Statement:
        return parser_.parseStatementImpl();
    }
    return nullptr;
}

void Parser::ParseContinuationCoordinator::finishBlockFrame(
    std::size_t frameIndex,
    ast::AstNode* block)
{
    BlockFrame& frame = blockFrames_[frameIndex];
    frame.block = block;
    frame.destination = block;
    assert(frame.beforeIndex >= frame.beginIndex);
}

ast::AstNode* Parser::parseWithContinuation(ContinuationRequestKind kind)
{
    if (activeContinuationCoordinator_ != nullptr)
        return activeContinuationCoordinator_->parse(kind);
    ParseContinuationCoordinator coordinator(*this);
    return coordinator.parse(kind);
}
}
