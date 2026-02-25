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

using Next = std::function<Container<const Node*>(const Node*)>;

auto bfs(const IGraph& g, const Next& next, const Node* n1, const Node* n2)
    -> bool {
    auto visited = NodeAttribute<bool>{g.max_node_id(), false};
    auto stack   = std::queue<const Node*>{{n1}};

    while (!stack.empty()) {
        const auto* n = stack.front();
        stack.pop();

        if (visited[*n]) {
            continue;
        }

        visited[*n] = true;

        for (const auto* child : next(n)) {
            if (visited[*child]) {
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

void Patriarchal::add_parent(const Node* parent, const Node* child) {
    auto& parents  = parents_[child];
    auto& children = children_[parent];

    parents.push_back(parent);
    children.push_back(child);
}

auto Patriarchal::parent_count(const Node* node) -> size_t {
    return parents_[node].size();
}

auto Patriarchal::parents(const Node* node) -> Container<const Node*> {
    return {parents_[node]};
}

auto Patriarchal::parent(const Node* node) const -> const Node* {
    const auto& parents = parents_[node];
    assert(parents.size() == 1);
    return parents.front();
}

auto Patriarchal::children_count(const Node* node) -> size_t {
    return children_[node].size();
}

auto Patriarchal::children(const Node* node) -> Container<const Node*> {
    return {children_[node]};
}

auto Patriarchal::child(const Node* node) const -> const Node* {
    const auto& children = children_[node];
    assert(children.size() == 1);
    return children.front();
}

auto Patriarchal::precedes(const Node* n1, const Node* n2) -> bool {
    return bfs(g_, [&](const Node* n) { return children(n); }, n1, n2);
}

auto Patriarchal::succeed(const Node* n1, const Node* n2) -> bool {
    return bfs(g_, [&](const Node* n) { return parents(n); }, n1, n2);
}