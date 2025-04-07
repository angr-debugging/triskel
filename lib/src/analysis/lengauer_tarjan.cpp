/// An Explanation of Lengauer-Tarjan Dominators Algorithm, Jayadev Misra
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <set>
#include <triskel/analysis/lengauer_tarjan.hpp>

#include <ranges>

#include "triskel/analysis/dfs.hpp"
#include "triskel/graph/igraph.hpp"
#include "triskel/utils/attribute.hpp"
#include "triskel/utils/generator.hpp"

// NOLINTNEXTLINE(google-build-using-namespace)
using namespace triskel;

namespace {
using Bucket = std::set<const Node*>;

struct Forest {
    explicit Forest(const IGraph& g, const NodeAttribute<size_t>& semis)
        : ancestors{g, nullptr}, label(g, nullptr), semis{semis} {
        // Initialize the labels
        for (const auto* v : g.nodes()) {
            label[*v] = v;
        }
    }

    void link(const Node* v, const Node* w) { ancestors[w] = v; }

    auto eval(const Node* v) -> const Node* {
        if (is_root(v)) {
            return v;
        }

        compress(v);

        return label[v];
    }

    // NOLINTNEXTLINE(misc-no-recursion)
    void compress(const Node* v) {
        // This function assumes that v is not a root in the forest
        assert(!is_root(v));

        if (!is_root(ancestors[v])) {
            compress(ancestors[v]);

            auto*& ancestor = ancestors[v];

            if (semis[*label[*ancestor]] < semis[*label[v]]) {
                label[v] = label[*ancestor];
            }

            ancestor = ancestors[*ancestor];
        }
    }

    // Is a node a root in the forest
    [[nodiscard]] auto is_root(const Node* v) const -> bool {
        return ancestors[v] == nullptr;
    }

    // The ancestor of a node in the forest
    NodeAttribute<const Node*> ancestors;

    // IDK lol
    NodeAttribute<const Node*> label;

    // The semi dominator of a node
    const NodeAttribute<size_t>& semis;
};

}  // namespace

auto triskel::make_idoms(const IGraph& g) -> NodeAttribute<const Node*> {
    auto dfs = DFSAnalysis(g);

    auto nodes = span_to_vec(dfs.nodes());
    // TODO: remove root

    // Immediate Dominator
    auto doms = NodeAttribute<const Node*>{g, nullptr};

    // SemiDominator
    auto semis = NodeAttribute<size_t>{g, 0};

    /// Vertices w in bucket[v] have sd[w] = v
    auto buckets = NodeAttribute<Bucket>{g, Bucket{}};

    auto forest = Forest{g, semis};

    for (const auto* node : nodes) {
        semis[*node] = dfs.dfs_num(node);
    }

    for (const auto* w :
         nodes | std::ranges::views::drop(1) | std::ranges::views::reverse) {
        for (const auto* parent_edge : w->parent_edges()) {
            const auto& v = parent_edge->from;
            const auto* u = forest.eval(v);
            semis[*w]     = std::min(semis[*w], semis[*u]);
        }

        buckets[*nodes[semis[*w]]].insert(w);
        const auto& parent_w = dfs.parent(w);
        forest.link(parent_w, w);

        for (const auto& v : buckets[parent_w]) {
            const auto& u = forest.eval(v);

            if (semis[*u] < semis[*v]) {
                doms[*v] = u;
            } else {
                doms[*v] = parent_w;
            }
        }

        buckets[parent_w].clear();
    }

    for (const auto* w : nodes | std::ranges::views::drop(1)) {
        if (doms[*w] != nodes[semis[*w]]) {
            doms[*w] = doms[*doms[*w]];
        }
    }

    return doms;
}
