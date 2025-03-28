#pragma once

#include <cassert>
#include <compare>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace triskel {

template <typename Tag>
struct ID {
    ID() : value(InvalidID.value) {}

    explicit ID(size_t value) : value{value} {}
    explicit operator size_t() const { return value; }

    static const ID InvalidID;

    auto operator==(ID other) const -> bool { return other.value == value; }

    friend auto operator<=>(ID lhs, ID rhs) -> std::strong_ordering {
        return lhs.value <=> rhs.value;
    }

    [[nodiscard]] auto is_valid() { return *this != InvalidID; }
    [[nodiscard]] auto is_invalid() { return *this == InvalidID; }

   private:
    size_t value;
};

template <typename Tag>
const ID<Tag> ID<Tag>::InvalidID{static_cast<size_t>(-1)};

template <typename Tag>
auto format_as(const ID<Tag>& id) -> std::string {
    return std::to_string(static_cast<size_t>(id));
}

}  // namespace triskel

template <typename Tag>
struct std::hash<triskel::ID<Tag>> {
    auto operator()(triskel::ID<Tag> k) const -> std::size_t {
        return std::hash<size_t>{}(static_cast<size_t>(k));
    }
};

namespace triskel {

/// @brief A struct with and id
template <typename Tag>
struct Identifiable {
    virtual ~Identifiable() = default;

    [[nodiscard]] virtual auto id() const -> ID<Tag> = 0;

    auto operator==(const Identifiable& other) const -> bool {
        return id() == other.id();
    }

    // NOLINTNEXTLINE(google-explicit-constructor)
    operator ID<Tag>() const { return id(); }
};

struct NodeTag {};
using NodeId = ID<NodeTag>;
static_assert(std::is_trivially_copyable_v<NodeId>);
static_assert(std::strict_weak_order<std::ranges::less, NodeId, NodeId>);

struct EdgeTag {};
using EdgeId = ID<EdgeTag>;
static_assert(std::is_trivially_copyable_v<EdgeId>);

struct GraphData;
struct EdgeData;

struct NodeData {
    explicit NodeData(NodeId id) : id{id} {}

    NodeId id;

    std::vector<EdgeData*> edges;
    // The first half are parent edges, the second half are child edges
    size_t separator = 0;
};

struct EdgeData {
    explicit EdgeData(EdgeId id) : id{id} {}

    EdgeId id;

    NodeData* from;
    NodeData* to;
};

struct GraphData {
    NodeId root;

    std::unordered_map<NodeId, std::unique_ptr<NodeData>> nodes;
    std::unordered_map<EdgeId, std::unique_ptr<EdgeData>> edges;
};

struct Edge;
struct Node;
struct IGraph;
struct IGraphEditor;

template <typename Extractor, typename Value, typename Data, typename Context>
concept IsExtratable = requires(const Context& ctx, const Data& data) {
    { Extractor::get(ctx, data) } -> std::same_as<Value>;
};

template <typename Container, typename Idx, typename Data>
concept IsIndexable =
    requires(const Context& ctx, const Data& data, size_t idx) {
        { Extractor::get(ctx, data) } -> std::same_as<Value>;
    };

template <typename Extractor, typename Value, typename Data, typename Context>
struct GraphIterator {
   private:
    const Context* ctx;
    const Data** ptr;

   public:
    using iterator_category = std::random_access_iterator_tag;
    using difference_type   = std::ptrdiff_t;
    using value_type        = Value;
    using pointer           = Value*;
    using reference         = Value&;

    GraphIterator() = default;

    GraphIterator(Context* ctx, std::vector<Data*>* data)
        : ctx{ctx}, ptr{data} {}

    ~GraphIterator() = default;

    [[nodiscard]] inline auto operator[](difference_type i) const
        -> value_type {
        return Extractor::get(*ctx, *ptr[i]);
    }

    auto operator*() const -> value_type { return *this[0]; }

    // Pre-increment
    auto operator++() -> GraphIterator& {
        ++ptr;
        return *this;
    }

    // Post-increment
    auto operator++(int) -> GraphIterator {
        auto temp = *this;
        ++(*this);
        return temp;
    }

    auto operator--() -> GraphIterator& {
        --ptr;
        return *this;
    }

    // Post-increment
    auto operator--(int) -> GraphIterator {
        auto temp = *this;
        --(*this);
        return temp;
    }

    auto operator+=(difference_type n) -> GraphIterator& {
        ptr += n;
        return *this;
    }

    auto operator-=(difference_type n) -> GraphIterator& {
        ptr -= n;
        return *this;
    }

    [[nodiscard]] auto operator+(difference_type n) const -> GraphIterator {
        return GraphIterator(ctx, ptr + n);
    }

    [[nodiscard]] auto operator-(difference_type n) const -> GraphIterator {
        return GraphIterator(ctx, ptr - n);
    }

    [[nodiscard]] auto operator-(const GraphIterator& other) const
        -> difference_type {
        return ptr - other.idx;
    }

    friend inline auto operator+(difference_type lhs,
                                 const GraphIterator& rhs) -> GraphIterator {
        return GraphIterator(rhs.ctx, lhs + rhs.ptr);
    }
    friend inline auto operator-(difference_type lhs,
                                 const GraphIterator& rhs) -> GraphIterator {
        return GraphIterator(rhs.ctx, lhs - rhs.ptr);
    }

    [[nodiscard]] auto operator<=>(const GraphIterator& other) const {
        return ptr <=> other.ptr;
    }

    [[nodiscard]] auto operator==(const GraphIterator& other) const -> bool {
        return ptr == other.ptr;
    }
};

template <typename Extractor, typename Value, typename Data, typename Context>
struct GraphArray {
    using iterator = GraphIterator<Extractor, Value, Data, Context>;

    const Context& ctx;
    const std::vector<Data*>& data;

    size_t first_idx;
    size_t last_idx;

    GraphArray(const Context& ctx, const std::vector<Data*>& data)
        : GraphArray(&ctx, &data, 0, 0, data.size()) {}

    GraphArray(const Context& ctx,
               const std::vector<Data*>& data,
               size_t first_idx)
        : GraphArray(&ctx, &data, 0, first_idx, data.size()) {}

    GraphArray(const Context& ctx,
               const std::vector<Data*>& data,
               size_t first_idx,
               size_t last_idx)
        : ctx{ctx}, data{data}, first_idx{first_idx}, last_idx{last_idx} {}

    auto begin() -> iterator { return iterator{ctx, data.begin() + first_idx}; }
    auto end() -> iterator { return iterator{ctx, data.begin() + last_idx}; }
};

struct Unit {};

struct EdgeExtractor {
    [[nodiscard]] inline static auto get(const Unit& _,
                                         const EdgeData& data) -> Edge;
};
using EdgeIterator = GraphArray<EdgeExtractor, Edge, EdgeData, Unit>;

struct ChildExtractor {
    [[nodiscard]] inline static auto get(const Unit&,
                                         const EdgeData& data) -> Node;
};
using ChildNodeIterator = GraphArray<ChildExtractor, Node, EdgeData, Unit>;

struct ParentExtractor {
    [[nodiscard]] inline static auto get(const Unit&,
                                         const EdgeData& data) -> Node;
};
using ParentNodeIterator = GraphArray<ParentExtractor, Node, EdgeData, Unit>;

struct NeighborExtractor {
    [[nodiscard]] inline static auto get(const Node& self,
                                         const EdgeData& data) -> Node;
};
using NeighborNodeIterator =
    GraphArray<NeighborExtractor, Node, EdgeData, Node>;

struct Node : public Identifiable<NodeTag> {
    // NOLINTNEXTLINE(google-explicit-constructor)
    Node(const NodeData& n);
    ~Node() override = default;

    [[nodiscard]] auto id() const -> NodeId final;
    [[nodiscard]] auto edges() const -> EdgeIterator;

    [[nodiscard]] auto child_edges() const -> EdgeIterator;
    [[nodiscard]] auto parent_edges() const -> EdgeIterator;

    [[nodiscard]] auto child_nodes() const -> ChildNodeIterator;
    [[nodiscard]] auto parent_nodes() const -> ParentNodeIterator;
    [[nodiscard]] auto neighbors() const -> NeighborNodeIterator;

   private:
    const NodeData* n_;
};

static_assert(std::random_access_iterator<ChildNodeIterator::iterator>);
static_assert(std::random_access_iterator<ParentNodeIterator::iterator>);
static_assert(std::random_access_iterator<NeighborNodeIterator::iterator>);

struct Edge : public Identifiable<EdgeTag> {
    // NOLINTNEXTLINE(google-explicit-constructor)
    Edge(const EdgeData& e);
    ~Edge() override = default;

    [[nodiscard]] auto id() const -> EdgeId final;
    [[nodiscard]] inline auto from() const -> Node;
    [[nodiscard]] inline auto to() const -> Node;

    /// @brief Returns the other side of the edge
    [[nodiscard]] auto other(NodeId n) const -> Node;

   private:
    const EdgeData* e_;
};
static_assert(std::random_access_iterator<EdgeIterator::iterator>);

/// @brief An interface for a graph
struct IGraph {
    virtual ~IGraph() = default;

    /// @brief The root of this graph
    [[nodiscard]] virtual auto root() const -> Node = 0;

    /// @brief The nodes in this graph
    [[nodiscard]] virtual auto nodes() const -> std::vector<Node> = 0;

    /// @brief The edges in this graph
    [[nodiscard]] virtual auto edges() const -> std::vector<Edge> = 0;

    /// @brief Turns a NodeId into a Node
    [[nodiscard]] virtual auto get_node(NodeId id) const -> Node = 0;

    /// @brief Turns an EdgeId into an Edge
    [[nodiscard]] virtual auto get_edge(EdgeId id) const -> Edge = 0;

    /// @brief The greatest id in this graph
    [[nodiscard]] virtual auto max_node_id() const -> size_t = 0;

    /// @brief The greatest id in this graph
    [[nodiscard]] virtual auto max_edge_id() const -> size_t = 0;

    /// @brief The number of nodes in this graph
    [[nodiscard]] virtual auto node_count() const -> size_t = 0;

    /// @brief The number of edges in this graph
    [[nodiscard]] virtual auto edge_count() const -> size_t = 0;

    /// @brief Gets the editor attached to this graph
    [[nodiscard]] virtual auto editor() -> IGraphEditor& = 0;
};

auto format_as(const Node& n) -> std::string;
auto format_as(const Edge& e) -> std::string;
auto format_as(const IGraph& g) -> std::string;

struct IGraphEditor {
    virtual ~IGraphEditor() = default;

    // ==========
    // Nodes
    // ==========
    /// @brief Adds a node to the graph
    virtual auto make_node() -> Node = 0;

    /// @brief Removes a node from the graph
    /// This will also remove all the edges that contain this node
    virtual void remove_node(NodeId node) = 0;

    // ==========
    // Edges
    // ==========
    /// @brief Create a new edge between two nodes
    virtual auto make_edge(NodeId from, NodeId to) -> Edge = 0;

    /// @brief Modifies the start and end point of an edge
    virtual void edit_edge(EdgeId edge, NodeId new_from, NodeId new_to) = 0;

    /// @brief Removes an edge from the graph
    virtual void remove_edge(EdgeId edge) = 0;

    // ==========
    // Version Control
    // ==========
    /// @brief Creates a new edit frame
    virtual void push() = 0;

    /// @brief Removes all changes from the current frame
    virtual void pop() = 0;

    /// @brief Writes all changes to the graph, erasing the modification
    /// frames
    virtual void commit() = 0;
};

}  // namespace triskel