#include "triskel/graph/graph.hpp"
#include <unistd.h>

#include <cassert>
#include <cstddef>
#include <memory>
#include <stack>
#include <unordered_map>
#include <vector>

#include "triskel/graph/igraph.hpp"

// NOLINTNEXTLINE(google-build-using-namespace)
using namespace triskel;

namespace {

/// @brief Adds an edge to each of its nodes
void link_edge(EdgeData& edge) {
    // Child edges go in the back
    edge.from->edges.push_back(&edge);

    // Parent edges go in the front
    edge.to->edges.insert(edge.to->edges.begin(), &edge);
    edge.to->separator++;
}

/// @brief Removes an edge from each of its nodes
void unlink_edge(EdgeData& edge) {
    std::erase(edge.from->edges, &edge);
    std::erase(edge.to->edges, &edge);
    edge.to->separator--;
}

}  // namespace

// =============================================================================
// GraphEditor
// =============================================================================
GraphEditor::GraphEditor(Graph& g) : g_{g} {}

GraphEditor::~GraphEditor() {
    assert(frames.empty());
}

auto GraphEditor::make_node() -> Node {
    auto id = NodeId{next_node_id_};
    ++next_node_id_;

    g_.data_.nodes[id] = std::make_unique<NodeData>(id);

    // Sets the root if it is not defined
    if (g_.data_.root == NodeId::InvalidID) {
        g_.data_.root = id;
    }

    frame().created_nodes_count += 1;
    return g_.get_node(id);
}

void GraphEditor::remove_node(NodeId id) {
    const auto& node = g_.get_node(id);

    // Not handled
    assert(node != g_.root());

    for (const auto& edge : node.edges()) {
        remove_edge(edge);
    }

    auto n = std::move(g_.data_.nodes[id]);
    g_.data_.nodes.erase(id);
    frame().deleted_nodes.push(std::move(n));
}

auto GraphEditor::make_edge(EdgeId id, NodeId from, NodeId to) -> EdgeData& {
    g_.data_.edges[id] = std::make_unique<EdgeData>(id);

    auto& edge = *g_.data_.edges[id];

    edge.from = g_.data_.nodes[from].get();
    edge.to   = g_.data_.nodes[to].get();

    link_edge(edge);

    return edge;
}

auto GraphEditor::make_edge(NodeId from, NodeId to) -> Edge {
    auto id = EdgeId{next_edge_id_};
    ++next_edge_id_;

    auto& edge = make_edge(id, from, to);
    frame().created_edges_count++;
    return {edge};
}

void GraphEditor::remove_edge(EdgeId id) {
    auto edge = std::move(g_.data_.edges[id]);
    g_.data_.edges.erase(id);

    unlink_edge(*edge);

    frame().deleted_edges.push(std::move(edge));
}

void GraphEditor::edit_edge(EdgeId id, NodeId new_from, NodeId new_to) {
    // Delete the old edge
    {
        auto edge = std::move(g_.data_.edges[id]);
        unlink_edge(*edge);
        frame().modified_edges.push(std::move(edge));
    }

    // Create a new edge
    make_edge(id, new_from, new_to);
}

auto GraphEditor::frame() -> Frame& {
    assert(!frames.empty() &&
           "Using the graph editor without a frame. You need to call `push` "
           "before using the editor");
    return frames.top();
}

void GraphEditor::push() {
    frames.push(Frame{});
}

void GraphEditor::pop() {
    auto& f = frames.top();

    // The order here is important, otherwise we might modify deleted elements
    // Revert edited edges
    while (!f.modified_edges.empty()) {
        auto old_edge = std::move(f.modified_edges.top());
        f.modified_edges.pop();

        auto& new_edge = g_.data_.edges[old_edge->id];
        unlink_edge(*new_edge);

        link_edge(*old_edge);
        g_.data_.edges[old_edge->id] = std::move(old_edge);
    }

    // Revert removed edges
    while (!f.deleted_edges.empty()) {
        auto edge = std::move(f.deleted_edges.top());
        f.deleted_edges.pop();
        link_edge(*edge);
        g_.data_.edges[edge->id] = std::move(edge);
    }

    // Revert removed nodes
    while (!f.deleted_nodes.empty()) {
        auto node = std::move(f.deleted_nodes.top());
        f.deleted_nodes.pop();
        g_.data_.nodes[node->id] = std::move(node);
    }

    for (size_t i = 0; i < f.created_edges_count; ++i) {
        next_edge_id_--;
        const auto id = EdgeId{next_edge_id_};
        unlink_edge(*g_.data_.edges[id]);
        g_.data_.edges.erase(id);
    }

    // Revert created nodes
    for (size_t i = 0; i < f.created_nodes_count; ++i) {
        next_node_id_--;
        const auto id = NodeId{next_node_id_};
        g_.data_.nodes.erase(id);
    }

    frames.pop();
}

void GraphEditor::commit() {
    // Deletes all changes
    frames = std::stack<Frame>();
}

// =============================================================================
// Graph
// =============================================================================

Graph::Graph()
    : data_{.root = NodeId::InvalidID, .nodes = {}, .edges = {}},
      editor_{*this} {}

auto Graph::root() const -> Node {
    return get_node(data_.root);
}

auto Graph::nodes() const -> std::vector<Node> {
    return data_.nodes;
}

auto Graph::edges() const -> std::vector<Edge> {
    return data_.edges;
}

auto Graph::get_node(NodeId id) const -> Node {
    assert(id != NodeId::InvalidID);
    return {*data_.nodes.at(id)};
}

auto Graph::get_edge(EdgeId id) const -> Edge {
    assert(id != EdgeId::InvalidID);
    return {*data_.edges.at(id)};
}

auto Graph::max_node_id() const -> size_t {
    return editor_.next_node_id_;
}

auto Graph::max_edge_id() const -> size_t {
    return editor_.next_edge_id_;
}

auto Graph::node_count() const -> size_t {
    return data_.nodes.size();
}

auto Graph::edge_count() const -> size_t {
    return data_.edges.size();
}

auto Graph::editor() -> GraphEditor& {
    return editor_;
}