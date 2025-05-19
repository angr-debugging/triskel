#include "triskel/graph/frame.hpp"

#include <utility>

#include "triskel/graph/owning_graph.hpp"

using triskel::Frame;

void Frame::MakeNode::revert(OwningGraph& graph) {
    graph.data_.nodes.erase(id);
    graph.invalid_node_cache = true;
}

void Frame::RemoveNode::revert(OwningGraph& graph) {
    graph.data_.nodes[*node] = std::move(node);
    graph.invalid_node_cache = true;
}

void Frame::AddEdge::revert(OwningGraph& graph) {
    auto* edge = graph.get_edge(id);
    edge->unlink();
    graph.data_.edges.erase(id);
    graph.invalid_edge_cache = true;
}

void Frame::RemoveEdge::revert(OwningGraph& graph) {
    edge->link();
    graph.data_.edges[*edge] = std::move(edge);
    graph.invalid_edge_cache = true;
}

void Frame::ModifyEdge::revert(OwningGraph& graph) {
    auto& new_edge = graph.data_.edges[id];
    new_edge->unlink();

    new_edge->from = from;
    new_edge->to   = to;
    new_edge->link();
}