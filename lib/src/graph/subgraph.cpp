#include "triskel/graph/subgraph.hpp"

#include <cassert>
#include <cstddef>
#include <memory>
#include <vector>

#include <fmt/core.h>
#include <fmt/printf.h>

#include "triskel/graph/igraph.hpp"

// NOLINTNEXTLINE(google-build-using-namespace)
using namespace triskel;

// =============================================================================
// Subgraph Editor
// =============================================================================
SubGraphEditor::SubGraphEditor(SubGraph& sg)
    : sg_{sg}, editor_{sg.g_.editor()} {}

void SubGraphEditor::select_node(NodeId id) {
    if (!sg_.contains(id)) {
        sg_.data_.nodes[id] = std::make_unique<NodeData>(id);
    }

    select_edges(id);

    if (sg_.data_.root == NodeId::InvalidID) {
        make_root(id);
    }
}

void SubGraphEditor::unselect_node(NodeId id) {
    if (sg_.contains(id)) {
        sg_.data_.nodes[id] = std::make_unique<NodeData>(id);
    }
}

void SubGraphEditor::select_edge(const Edge& g_edge) {
    sg_.data_.edges[g_edge] = std::make_unique<EdgeData>(g_edge);

    auto& sg_edge = sg_.data_.edges[g_edge];

    sg_edge->to   = sg_.data_.nodes[g_edge.to()].get();
    sg_edge->from = sg_.data_.nodes[g_edge.from()].get();

    sg_edge->link();
}

void SubGraphEditor::select_edges(NodeId node) {
    // The node in the complete graph
    auto n = sg_.g_.get_node(node);

    for (const auto& edge : n.edges()) {
        if (sg_.contains(edge.other(node))) {
            select_edge(edge);
        }
    }
}

void SubGraphEditor::unselect_edges(NodeId node) {
    // The node in the complete graph
    auto n = sg_.g_.get_node(node);

    for (const auto& edge : n.edges()) {
        if (sg_.contains(edge.other(node))) {
            sg_.data_.edges.erase(edge);
        }
    }
}

void SubGraphEditor::make_root(NodeId node) {
    sg_.data_.root = node;
}

auto SubGraphEditor::make_node() -> Node {
    auto node = editor_.make_node();
    select_node(node);
    // We want the object in the subgraph
    return sg_.get_node(node);
}

void SubGraphEditor::remove_node(NodeId node) {
    assert(sg_.contains(node));
    editor_.remove_node(node);
}

auto SubGraphEditor::make_edge(NodeId from, NodeId to) -> Edge {
    assert(sg_.contains(from) && sg_.contains(to));
    auto edge = editor_.make_edge(from, to);
    select_edge(edge);
    // We want the object in the subgraph
    return sg_.get_edge(edge);
}

void SubGraphEditor::edit_edge(EdgeId edge, NodeId new_from, NodeId new_to) {
    assert(sg_.contains(new_from) && sg_.contains(new_to));
    sg_.data_.edges.erase(edge);

    editor_.edit_edge(edge, new_from, new_to);

    select_edge(sg_.g_.get_edge(edge));
}

void SubGraphEditor::remove_edge(EdgeId edge) {
    editor_.remove_edge(edge);
}

void SubGraphEditor::push() {
    editor_.push();
}

// TODO:
void SubGraphEditor::pop() {
    editor_.pop();

    auto node_ids = std::vector<NodeId>{};
    node_ids.reserve(sg_.data_.nodes.size());

    auto edge_ids = std::vector<EdgeId>{};
    edge_ids.reserve(sg_.data_.edges.size());

    for (const auto& node : sg_.nodes()) {
        node_ids.push_back(node);
    }

    for (const auto& edge : sg_.edges()) {
        edge_ids.push_back(edge);
    }
}

void SubGraphEditor::commit() {
    editor_.commit();
}

// =============================================================================
// Subgraph
// =============================================================================
SubGraph::SubGraph(Graph& g) : g_{g}, editor_{*this} {}

auto SubGraph::max_node_id() const -> size_t {
    return g_.max_node_id();
}

auto SubGraph::max_edge_id() const -> size_t {
    return g_.max_edge_id();
}

auto SubGraph::editor() -> SubGraphEditor& {
    return editor_;
}

auto SubGraph::contains(NodeId node) -> bool {
    return data_.nodes.contains(node);
}

auto SubGraph::contains(EdgeId edge) -> bool {
    return data_.edges.contains(edge);
}
