#include "triskel/analysis/patriarchal.hpp"

#include <cassert>
#include <functional>
#include <queue>
#include <vector>

#include "triskel/graph/igraph.hpp"
#include "triskel/utils/attribute.hpp"

// NOLINTNEXTLINE(google-build-using-namespace)
using namespace triskel;

namespace {

using Next = std::function<std::generator<Node>(NodeId)>;

auto bfs(const IGraph& g, const Next& next, NodeId n1, NodeId n2) -> bool {
    auto visited = NodeAttribute<bool>{g.max_node_id(), false};
    auto stack   = std::queue<NodeId>{{n1}};

    while (!stack.empty()) {
        const auto& n = g.get_node(stack.front());
        stack.pop();

        if (visited.get(n)) {
            continue;
        }

        visited.set(n, true);

        for (const auto& child : next(n)) {
            if (visited.get(child)) {
                continue;
            }

            if (child == n2) {
                return true;
            }

            stack.push(child);
        }
    }

    return false;
}
}  // namespace

Patriarchal::Patriarchal(const IGraph& g)
    : g_{g}, parents_{g.max_node_id(), {}}, children_{g.max_node_id(), {}} {}

void Patriarchal::add_parent(NodeId parent, NodeId child) {
    auto& parents  = parents_.get(child);
    auto& children = children_.get(parent);

    parents.push_back(parent);
    children.push_back(child);
}

auto Patriarchal::parent_count(NodeId n) -> size_t {
    return parents_.get(n).size();
}

auto Patriarchal::parents(NodeId n) -> std::generator<Node> {
    for (const auto& node : parents_.get(n)) {
        co_yield (g_.get_node(node));
    }
}

auto Patriarchal::parent_id(NodeId n) const -> NodeId {
    const auto& parents = parents_.get(n);
    assert(parents.size() == 1);
    return parents.front();
}

auto Patriarchal::parent(NodeId n) -> Node {
    return g_.get_node(parent_id(n));
}

auto Patriarchal::children_count(NodeId n) -> size_t {
    return children_.get(n).size();
}

auto Patriarchal::children(NodeId n) -> std::generator<Node> {
    for (const auto& node : children_.get(n)) {
        co_yield g_.get_node(node);
    }
}

auto Patriarchal::child(NodeId n) -> Node {
    auto& children = children_.get(n);
    assert(children.size() == 1);
    return g_.get_node(children.front());
}

auto Patriarchal::precedes(NodeId n1, NodeId n2) -> bool {
    return bfs(g_, [&](NodeId n) { return children(n); }, n1, n2);
}

auto Patriarchal::succeed(NodeId n1, NodeId n2) -> bool {
    return bfs(g_, [&](NodeId n) { return parents(n); }, n1, n2);
}