#pragma once

// Experimental, disconnected from the compiler and from binary metadata.
// A caller must account for every operation and dependency before completing
// a graph. An unclassified operation or an incomplete graph remains unknown.
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace nativesharp::frontend::view_provenance_prototype
{
enum class SinkKind { Return, Output, External };

struct Sink
{
    SinkKind kind;
    std::size_t ordinal;
};

struct Edge
{
    std::size_t sourceParameter;
    Sink sink;
};

struct Summary
{
    bool verified = false;
    std::size_t parameterCount = 0;
    std::vector<Edge> edges;
};

class Graph
{
public:
    using Node = std::size_t;

    Node local()
    {
        sources_.emplace_back(parameterCount_, false);
        return sources_.size() - 1;
    }

    explicit Graph(std::size_t parameterCount) : parameterCount_(parameterCount) {}

    std::optional<Node> parameter(std::size_t ordinal)
    {
        if (ordinal >= parameterCount_) { unknown_ = true; return std::nullopt; }
        Node n = local();
        sources_[n][ordinal] = true;
        return n;
    }

    void transfer(Node source, Node destination)
    {
        if (!valid(source) || !valid(destination)) { unknown_ = true; return; }
        transfers_.emplace_back(source, destination);
    }

    void output(Node source, Sink sink)
    {
        if (!valid(source) || (sink.kind == SinkKind::Output && sink.ordinal >= parameterCount_))
        {
            unknown_ = true;
            return;
        }
        sinks_.emplace_back(source, sink);
    }

    void unsupported() noexcept { unknown_ = true; }

    // Compose a verified callee summary into the caller graph. Each callee
    // parameter maps to a caller node; optional result is required when the
    // callee can return a borrowed view. Unknown inputs never become proof.
    void call(const Summary& callee, const std::vector<Node>& arguments,
              std::optional<Node> result)
    {
        if (!callee.verified || callee.parameterCount != arguments.size())
        {
            unknown_ = true;
            return;
        }
        for (const Edge& edge : callee.edges)
        {
            if (edge.sourceParameter >= arguments.size() || !valid(arguments[edge.sourceParameter]))
            {
                unknown_ = true;
                return;
            }
            const Node input = arguments[edge.sourceParameter];
            switch (edge.sink.kind)
            {
                case SinkKind::Return:
                    if (!result.has_value() || !valid(*result)) { unknown_ = true; return; }
                    transfer(input, *result);
                    break;
                case SinkKind::Output:
                    if (edge.sink.ordinal >= arguments.size() || !valid(arguments[edge.sink.ordinal]))
                    {
                        unknown_ = true;
                        return;
                    }
                    transfer(input, arguments[edge.sink.ordinal]);
                    break;
                case SinkKind::External:
                    output(input, edge.sink);
                    break;
                default:
                    unknown_ = true;
                    return;
            }
        }
    }

    Summary finish(bool allOperationsAnalyzed) const
    {
        auto origins = sources_;
        bool changed = true;
        while (changed)
        {
            changed = false;
            for (const auto& [from, to] : transfers_)
                for (std::size_t p = 0; p < parameterCount_; ++p)
                    if (origins[from][p] && !origins[to][p])
                    {
                        origins[to][p] = true;
                        changed = true;
                    }
        }
        Summary result;
        result.parameterCount = parameterCount_;
        // Known edges are observations only when verified is false; callers
        // must never treat their absence as proof of no dependency.
        result.verified = allOperationsAnalyzed && !unknown_;
        for (const auto& [node, sink] : sinks_)
            for (std::size_t p = 0; p < parameterCount_; ++p)
                if (origins[node][p]) result.edges.push_back({p, sink});
        return result;
    }

private:
    bool valid(Node node) const noexcept { return node < sources_.size(); }
    std::size_t parameterCount_;
    bool unknown_ = false;
    std::vector<std::vector<bool>> sources_;
    std::vector<std::pair<Node, Node>> transfers_;
    std::vector<std::pair<Node, Sink>> sinks_;
};
}
