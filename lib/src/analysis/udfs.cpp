#include "triskel/analysis/udfs.hpp"

#include <cstddef>
#include <vector>

#include "triskel/analysis/patriarchal.hpp"
#include "triskel/graph/igraph.hpp"

// NOLINTNEXTLINE(google-build-using-namespace)
using namespace triskel;

using UDFS = UnorderedDFSAnalysis;

UDFS::UnorderedDFSAnalysis(const IGraph& g)
    : Patriarchal(g),
      g_{g},
      dfs_nums_(g.max_node_id(), 0),
      types_(g.max_edge_id(), EdgeType::None) {
    nodes_.reserve(g.node_count());

    udfs(g.root());
}

// NOLINTNEXTLINE(misc-no-recursion)
void UDFS::udfs(const Node* node) {
    nodes_.push_back(node);
    dfs_nums_[node] = nodes_.size() - 1;

    for (const auto* edge : node->edges()) {
        const auto& child = edge->other(*node);

        if (!was_visited(child)) {
            udfs(child);

            add_parent(node, child);
            types_[edge] = UDFS::EdgeType::Tree;
            continue;
        }

        if (types_[edge] == UDFS::EdgeType::None) {
            types_[edge] = UDFS::EdgeType::Back;
        }
    }
}

auto UDFS::was_visited(const Node* node) -> bool {
    return (dfs_nums_[node] != 0) || (node == g_.root());
}

auto UDFS::nodes() -> Container<const Node*> {
    return {nodes_};
}

auto UDFS::is_tree(const Edge* e) -> bool {
    return types_[e] == EdgeType::Tree;
}

auto UDFS::is_backedge(const Edge* e) -> bool {
    return types_[e] == EdgeType::Back;
}

void UDFS::set_backedge(const Edge* e) {
    types_[e] = EdgeType::Back;
}

auto UDFS::dfs_num(const Node* n) -> size_t {
    return dfs_nums_[n];
}