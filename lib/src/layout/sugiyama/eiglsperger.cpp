// https://jgaa.info/index.php/jgaa/article/view/paper111/2849

#include "triskel/layout/sugiyama/eiglsperger.hpp"

#include <fmt/base.h>
#include <fmt/format.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>
#include <list>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

#include "triskel/graph/igraph.hpp"
#include "triskel/utils/attribute.hpp"

// NOLINTNEXTLINE(google-build-using-namespace)
using namespace triskel;

namespace {
[[nodiscard]] auto merge_and_count(const std::vector<size_t>& lo,
                                   const std::vector<size_t>& hi) -> size_t {
    size_t inversions = 0;

    size_t i = 0;
    size_t j = 0;

    while (i < lo.size() && j < hi.size()) {
        if (lo[i] > hi[j]) {
            j++;
            inversions += (lo.size() - i);
        } else {
            i++;
        }
    }

    return inversions;
}

[[nodiscard]] auto merge_and_count(std::vector<size_t>& arr,
                                   int64_t lo,
                                   int64_t mid,
                                   int64_t hi) -> size_t {
    auto lo_arr = std::vector<size_t>{arr.begin() + lo, arr.begin() + mid};

    auto hi_arr = std::vector<size_t>{arr.begin() + mid, arr.begin() + hi};

    auto lo_sz = mid - lo;
    auto hi_sz = hi - mid;

    // assert(lo_sz == lo_arr.size());
    // assert(hi_sz == hi_arr.size());
    // assert(lo_sz + hi_sz == hi - lo);

    size_t i = 0;
    size_t j = 0;
    size_t k = lo;  // Merged index

    size_t inversions = 0;

    while (i < lo_sz && j < hi_sz) {
        if (lo_arr[i] <= hi_arr[j]) {
            arr[k++] = lo_arr[i++];
        } else {
            arr[k++] = hi_arr[j++];
            inversions += (lo_sz - i);
        }
    }

    while (i < lo_sz) {
        arr[k++] = lo_arr[i++];
    }

    while (j < hi_sz) {
        arr[k++] = hi_arr[j++];
    }

    return inversions;
}

// NOLINTNEXTLINE(misc-no-recursion)
[[nodiscard]] auto sort_and_count(std::vector<size_t>& arr,
                                  int64_t lo,
                                  int64_t hi) -> size_t {
    size_t inversions = 0;

    if (hi - lo > 1) {
        auto mid = lo + ((hi - lo) / 2);
        inversions += sort_and_count(arr, lo, mid);
        inversions += sort_and_count(arr, mid, hi);
        inversions += merge_and_count(arr, lo, mid, hi);
    }

    return inversions;
}
}  // namespace

namespace {
struct Segments {
    /// @brief Appends segment s to the end of container S
    void append(const LongEdge* segment) { data.push_back(segment); }

    /// @brief Appends all elements of container S2 to this container
    void join(const Segments& S2) {
        data.reserve(data.size() + S2.size());

        for (const auto& e : S2.data) {
            data.push_back(e);
        }
    }

    [[nodiscard]] auto split(const LongEdge* s) -> Segments {
        auto it = std::ranges::find(data, s);
        // assert(it != data.end());

        Segments res;
        res.data = std::vector<const LongEdge*>(it + 1, data.end());
        data.erase(it, data.end());
        return res;
    }

    [[nodiscard]] auto split(size_t k, bool keep_k) -> Segments {
        // assert(k < size());
        Segments res;
        res.data = std::vector<const LongEdge*>(
            data.begin() + static_cast<int64_t>(k) + (keep_k ? 0 : 1),
            data.end());
        data.resize(k);
        return res;
    }

    [[nodiscard]] auto size() const -> size_t { return data.size(); }

    [[nodiscard]] auto contains(const LongEdge* segment) const -> bool {
        return std::ranges::contains(data, segment);
    }

    std::vector<const LongEdge*> data;
};

struct Set {
    void append(const LongEdge* segment) { data_.append(segment); }

    void join(std::unique_ptr<Set> S2) {
        data_.join(S2->data_);
        S2->data_.data.clear();
    }

    [[nodiscard]] auto split(const LongEdge* segment) -> std::unique_ptr<Set> {
        auto res   = std::make_unique<Set>();
        res->data_ = data_.split(segment);
        return res;
    }

    [[nodiscard]] auto split(size_t k, bool keep_k = true)
        -> std::unique_ptr<Set> {
        auto res   = std::make_unique<Set>();
        res->data_ = data_.split(k, keep_k);
        return res;
    }

    [[nodiscard]] auto size() const -> size_t { return data_.size(); }

    [[nodiscard]] auto empty() const -> bool { return size() == 0; }

    [[nodiscard]] auto contains(const LongEdge* segment) const -> bool {
        return data_.contains(segment);
    }

    [[nodiscard]] auto get(size_t i) const -> const LongEdge* {
        return data_.data[i];
    }

    const Node* prev = nullptr;
    const Node* next = nullptr;

    size_t pos = 0;

   private:
    Segments data_;
};

auto print_set(const Set* s) {
    if (s == nullptr) {
        fmt::print("NULL");
        return;
    }

    for (size_t i = 0; i < s->size(); ++i) {
        fmt::print("{}, ", *s->get(i)->p);
    }
}

using GetParentEdge     = std::function<Container<Edge*>(const Node*)>;
using GetParentEdgeNode = std::function<const Node*(const Edge*)>;

struct Eiglsperger {
    explicit Eiglsperger(const IGraph& g,
                         const NodeAttribute<LongEdge*>& long_edges,
                         GetParentEdge get_node_parent_edges,
                         GetParentEdgeNode get_parent_edge_node,
                         const NodeAttribute<size_t>& layers_)
        : long_edges_{long_edges},
          get_node_parent_edges{std::move(get_node_parent_edges)},
          get_parent_edge_node{std::move(get_parent_edge_node)},
          layers_(layers_),
          prev_(g),
          next_(g),
          pos_(g, std::numeric_limits<size_t>::max()),
          measure_(g, std::numeric_limits<size_t>::max()),
          sorted_neighbor_orders_{g, {}} {}

    using NodeVisitor = std::function<Set*(const Node*)>;
    using SetVisitor  = std::function<const Node*(Set*)>;

    const NodeAttribute<size_t>& layers_;

    const NodeAttribute<LongEdge*>& long_edges_;

    const GetParentEdge get_node_parent_edges;
    const GetParentEdgeNode get_parent_edge_node;

    NodeAttribute<Set*> prev_;
    NodeAttribute<std::vector<size_t>> sorted_neighbor_orders_;
    NodeAttribute<std::unique_ptr<Set>> next_;
    NodeAttribute<size_t> pos_;
    NodeAttribute<size_t> measure_;

    auto is_p(const Node* node) -> bool {
        const auto* long_edge = long_edges_[node];
        return long_edge != nullptr && long_edge->p == node;
    }

    auto is_q(const Node* node) -> bool {
        const auto* long_edge = long_edges_[node];
        return long_edge != nullptr && long_edge->q == node;
    }

    static void visit_layer(std::unique_ptr<Set>& start,
                            const NodeVisitor& node_visitor,
                            const SetVisitor& set_visitor) {
        auto* cursor = start.get();

        while (cursor != nullptr) {
            const auto* node = set_visitor(cursor);

            if (node == nullptr) {
                break;
            }

            cursor = node_visitor(node);
        }
    }

    // The layer is alternating Set / Vertex starting and ending with a set,
    // so visiting only nodes is easy
    static void visit_layer(std::unique_ptr<Set>& start,
                            const NodeVisitor& node_visitor) {
        visit_layer(start, node_visitor, [](const auto* s) { return s->next; });
    }

    // Step 1:
    // > In the first step we append the segment s(v) for each p-vertex v in
    // > layer Li to the container preceding v.  Then we join this container
    // with > the succeeding container. The result is again an alternating
    // layer > (p-vertices are omitted).
    auto step1_visitor(const Node* node) -> Set* {
        auto& next = next_[node];
        auto& prev = prev_[node];

        if (!is_p(node)) {
            return next.get();
        }

        prev->append(long_edges_[node]);

        // Unlink self and next element
        prev->next = next->next;

        if (prev->next != nullptr) {
            prev_[prev->next] = prev;
        }

        prev->join(std::move(next));
        next = nullptr;

        // We are visiting this set twice, but since we're in a node visitor
        // it's OK
        return prev;
    }

    void step1(std::unique_ptr<Set>& layer) {
        visit_layer(layer,
                    [this](const Node* node) { return step1_visitor(node); });
    }

    // Step 2:
    // > In the second step we compute the measure values for the elements
    // in Li+1.
    // > First we assign a position value pos(vij) to all vertices vij in Li.
    // > pos(vi0) = size(Si0) and pos(vij) = pos(vij−1) + size(Sij) + 1.
    // > Note that the pos values are the same as they would be in the median or
    // > barycenter heuristic if each segment was represented as dummy vertex.
    // > Each non-empty container Sij has pos value pos(vij−1) + 1.
    // > If container Si0 is non-empty it has pos value 0.
    // > Now we assign the measure to all non-q-vertices and containers in Li+1.
    // > The initial containers in Li+1 are the resulting containers of the
    // > first step. Recall that the measure of a container in Li+1 > is its
    // > position in Li.
    auto step2_pos_node_visitor(const Node* node) -> Set* {
        const auto& prev = prev_[node];
        pos_[node]       = prev->pos + prev->size();
        return next_[node].get();
    }

    auto step2_pos_set_visitor(Set* set, size_t layer_idx) -> const Node* {
        if (set->prev == nullptr) {
            set->pos = 0;
        } else {
            set->pos = pos_[set->prev] + 1;
        }

        for (size_t i = 0; i < set->size(); ++i) {
            const auto* node = set->get(i)->level[layer_idx];
            pos_[node]       = set->pos + i;
        }

        return set->next;
    }

    void find_pos(std::unique_ptr<Set>& layer, size_t layer_idx) {
        visit_layer(
            layer,
            [this](const Node* node) { return step2_pos_node_visitor(node); },
            [this, layer_idx](Set* set) {
                return step2_pos_set_visitor(set, layer_idx);
            });
    }

    void step2(std::unique_ptr<Set>& layer,
               const std::vector<const Node*>& next_layer,
               size_t layer_idx,
               size_t next_layer_idx) {
        find_pos(layer, layer_idx);

        // Measure
        for (const auto* node : next_layer) {
            // assert(layers_[node] == next_layer_idx);
            if (is_q(node)) {
                continue;
            }

            auto& neighbor_orders = sorted_neighbor_orders_[node];
            neighbor_orders       = {};

            for (const auto* parent : get_node_parent_edges(node)) {
                // assert(layers_[get_parent_edge_node(parent)] == layer_idx);
                const auto* parent_node = get_parent_edge_node(parent);
                const auto pos          = pos_[parent_node];
                neighbor_orders.push_back(pos);
            }
            std::ranges::sort(neighbor_orders);

            if (neighbor_orders.empty()) {
                // TODO: FIX
                measure_[node] = 0;
            } else {
                measure_[node] = neighbor_orders[neighbor_orders.size() / 2];
            }
        }
    }

    // Step 3:
    // > In the third step we calculate an initial ordering of Li+1. We sort
    // > all non-q-vertices in Li+1 according to their measure in a list LV.
    // > We do the same for the containers and store them in a list LS.
    using NodeOrSet  = std::variant<const Node*, std::unique_ptr<Set>>;
    using MixedLayer = std::list<NodeOrSet>;

    [[nodiscard]] static auto is_set(const NodeOrSet& v) -> bool {
        return std::holds_alternative<std::unique_ptr<Set>>(v);
    }

    [[nodiscard]] static auto get_set(NodeOrSet& v) -> std::unique_ptr<Set>& {
        return std::get<std::unique_ptr<Set>>(v);
    }

    [[nodiscard]] static auto get_set(const NodeOrSet& v)
        -> const std::unique_ptr<Set>& {
        return std::get<std::unique_ptr<Set>>(v);
    }

    [[nodiscard]] static auto is_node(const NodeOrSet& v) -> bool {
        return std::holds_alternative<const Node*>(v);
    }

    [[nodiscard]] static auto get_node(const NodeOrSet& v) -> const Node* {
        return std::get<const Node*>(v);
    }

    auto step3(std::unique_ptr<Set>& previous_layer,
               const std::vector<const Node*>& nodes) -> MixedLayer {
        std::vector<std::unique_ptr<Set>> Ls;
        {
            const auto* node = previous_layer->next;

            if (!previous_layer->empty()) {
                Ls.push_back(std::move(previous_layer));
            }
            previous_layer = nullptr;

            while (node != nullptr) {
                auto set = std::move(next_[node]);

                // Clear the node information
                prev_[node] = nullptr;
                next_[node] = nullptr;

                node = set->next;

                if (!set->empty()) {
                    Ls.push_back(std::move(set));
                }
            }
        }

        // Sorts the sets
        std::ranges::sort(Ls, [](const auto& s1, const auto& s2) {
            return s1->pos < s2->pos;
        });

        std::vector<const Node*> Lv;
        Lv.reserve(nodes.size());

        for (const auto* node : nodes) {
            if (!is_q(node)) {
                Lv.push_back(node);
            }
        }

        std::ranges::sort(Lv, [&](const auto* node1, const auto* node2) {
            return measure_[node1] < measure_[node2];
        });

        MixedLayer layer;

        size_t i = 0;
        size_t j = 0;
        while (i < Lv.size() && j < Ls.size()) {
            const auto* head_v = Lv[i];
            const auto& head_s = Ls[j];

            if (measure_[head_v] <= head_s->pos) {
                layer.push_back(Lv[i]);
                i += 1;
            } else if (measure_[head_v] >= (head_s->pos + head_s->size() - 1)) {
                layer.push_back(std::move(Ls[j]));
                Ls[j] = nullptr;
                j += 1;
            } else {
                const auto k = measure_[head_v] - head_s->pos;

                auto S2 = head_s->split(k);
                S2->pos = head_s->pos + k;

                layer.push_back(std::move(Ls[j]));
                layer.push_back(Lv[i]);
                i += 1;

                Ls[j] = std::move(S2);
                // One push and one pop so j stays the same
            }
        }

        while (i < Lv.size()) {
            layer.push_back(Lv[i]);
            i += 1;
        }

        while (j < Ls.size()) {
            layer.push_back(std::move(Ls[j]));
            Ls[j] = nullptr;
            j += 1;
        }

        return layer;
    }

    // Step 4: Add the Q vertices,
    static void step4(MixedLayer& layer, size_t layer_idx) {
        for (auto it = layer.begin(); it != layer.end(); ++it) {
            if (!is_set(*it)) {
                continue;
            }

            auto* s = get_set(*it).get();

            for (size_t i = 0; i < s->size(); ++i) {
                const auto* le = s->get(i);

                if (le->level[layer_idx] == le->q) {
                    auto S2 = s->split(i, false);

                    layer.insert(std::next(it), le->q);
                    layer.insert(std::next(std::next(it)), std::move(S2));
                }
            }
        }
    }

    // Step 5:
    // > Count crossings
    [[nodiscard]] auto step5(MixedLayer& layer) -> size_t {
        auto orders = std::vector<size_t>{};
        orders.reserve(layer.size());  // heuristically

        for (const auto& v : layer) {
            if (is_node(v)) {
                const auto* node      = get_node(v);
                auto& neighbor_orders = sorted_neighbor_orders_[node];
                orders.insert(orders.end(), neighbor_orders.begin(),
                              neighbor_orders.end());
            } else {
                const auto& set = get_set(v);
                // HACK:
                for (size_t i = 0; i < set->size(); ++i) {
                    orders.push_back(set->pos);
                }
            }
        }

        return sort_and_count(orders, 0, static_cast<int64_t>(orders.size()));
    }

    // Step6:
    // > Scan and normalize layer
    [[nodiscard]] auto step6(MixedLayer& layer) -> std::unique_ptr<Set> {
        auto cursor       = std::make_unique<Set>();
        auto next_is_node = true;

        Set* prev_s        = cursor.get();
        const Node* prev_v = nullptr;

        for (auto& e : layer) {
            if (is_set(e)) {
                auto& s = get_set(e);

                if (next_is_node) {
                    // We were looking for a node but found a set, so we
                    // will merge the sets
                    prev_s->join(std::move(s));
                    next_is_node = false;
                } else {
                    s->prev       = prev_v;
                    prev_s        = s.get();
                    next_[prev_v] = std::move(s);
                }

                s = nullptr;
            }

            else {
                const auto* v = get_node(e);
                if (!next_is_node) {
                    // We were looking for a set but found a node, create a
                    // new empty set
                    auto s        = std::make_unique<Set>();
                    s->prev       = prev_v;
                    prev_s        = s.get();
                    next_[prev_v] = std::move(s);
                    s             = nullptr;
                    next_is_node  = true;
                }

                prev_s->next = v;
                prev_[v]     = prev_s;
                prev_v       = v;
            }

            next_is_node = !next_is_node;
        }

        if (!next_is_node) {
            auto s        = std::make_unique<Set>();
            s->prev       = prev_v;
            s->next       = nullptr;
            prev_s        = s.get();
            next_[prev_v] = std::move(s);
            s             = nullptr;
        }

        prev_s->next = nullptr;

        return cursor;
    }

    auto make_initial_layer(std::vector<const Node*>& nodes)
        -> std::unique_ptr<Set> {
        auto res     = std::make_unique<Set>();
        auto* cursor = res.get();

        for (const auto& n : nodes) {
            cursor->next   = n;
            prev_[n]       = cursor;
            next_[n]       = std::make_unique<Set>();
            next_[n]->prev = n;
            cursor         = next_[n].get();
        }

        return res;
    }

    auto is_well_formed(std::unique_ptr<Set>& layer) {
        const Set* set_cursor   = layer.get();
        const Node* node_cursor = set_cursor->next;

        while (node_cursor != nullptr) {
            // assert(prev_[node_cursor] == set_cursor);
            set_cursor = next_[node_cursor].get();
            // assert(set_cursor->prev == node_cursor);
            node_cursor = set_cursor->next;
        }
    }

    auto eiglsperger_up(std::vector<std::vector<const Node*>>& node_layers)
        -> size_t {
        const auto size  = node_layers.size();
        auto layer       = make_initial_layer(node_layers[size - 1]);
        size_t crossings = 0;

        for (size_t i = size - 1; i > 0; --i) {
            is_well_formed(layer);
            step1(layer);
            is_well_formed(layer);
            step2(layer, node_layers[i - 1], i, i - 1);
            is_well_formed(layer);
            auto mixed = step3(layer, node_layers[i - 1]);
            step4(mixed, i - 1);
            crossings += step5(mixed);
            layer = step6(mixed);
        }

        // Gets the pos info for the last layer
        find_pos(layer, 0);

        return crossings;
    }

    auto eiglsperger_down(std::vector<std::vector<const Node*>>& node_layers)
        -> size_t {
        auto layer       = make_initial_layer(node_layers[0]);
        size_t crossings = 0;

        for (size_t i = 0; i < node_layers.size() - 1; ++i) {
            is_well_formed(layer);
            step1(layer);
            is_well_formed(layer);
            step2(layer, node_layers[i + 1], i, i + 1);
            is_well_formed(layer);
            auto mixed = step3(layer, node_layers[i + 1]);
            step4(mixed, i + 1);
            crossings += step5(mixed);
            layer = step6(mixed);
        }

        // Gets the pos info for the last layer
        find_pos(layer, node_layers.size() - 1);

        return crossings;
    }
};

}  // namespace

EVertexOrdering::EVertexOrdering(const IGraph& g,
                                 const NodeAttribute<size_t>& layers,
                                 const NodeAttribute<bool>& is_dummy,
                                 size_t layer_count_)
    : orders_(g, -1), g_{g}, layers_(layers), is_dummy_{is_dummy} {
    node_layers_.resize(layer_count_);

    make_node_layers();

    auto best        = NodeAttribute<size_t>(0, 0);
    size_t crossings = std::numeric_limits<size_t>::max();

    // The 24 comes from
    // https://blog.disy.net/sugiyama-method/
    for (size_t i = 0; i < 1; ++i) {
        auto e = i % 2 == 0
                     ? Eiglsperger(
                           g, long_edges_,
                           [](const Node* n) { return n->parent_edges(); },
                           [](const Edge* e) { return e->from; }, layers_)
                     : Eiglsperger(
                           g, long_edges_,
                           [](const Node* n) { return n->child_edges(); },
                           [](const Edge* e) { return e->to; }, layers_);

        const auto new_crossings = i % 2 == 0
                                       ? e.eiglsperger_up(node_layers_)
                                       : e.eiglsperger_down(node_layers_);

        if (new_crossings < crossings) {
            best      = e.pos_;
            crossings = new_crossings;

            if (new_crossings == 0) {
                break;
            }
        }

        // Flip p and q vertices
        for (auto& le : long_edges) {
            std::swap(le.p, le.q);
        }
    }

    orders_ = best;
}

/// @brief Dummy nodes only have one parent
auto EVertexOrdering::get_dummy_parent(const Node* node) const -> const Node* {
    // assert(is_dummy_[node]);

    // Backedge
    if (node->parent_count() == 0) {
        return nullptr;
    }

    const auto* n = node->parent_edges()[0]->from;

    if (node->parent_count() > 1) {
        // Backedge
        // assert(node->parent_count() == 2);

        if (!is_dummy_[n]) {
            return node->parent_edges()[1]->from;
        }
    }

    return n;
}

/// @brief Dummy nodes only have one child
auto EVertexOrdering::get_dummy_child(const Node* node) const -> const Node* {
    // assert(is_dummy_[node]);

    // Backedge
    if (node->children_count() == 0) {
        return nullptr;
    }

    const auto* n = node->child_edges()[0]->to;

    if (node->children_count() > 1) {
        // Backedge
        // assert(node->children_count() == 2);

        if (!is_dummy_[n]) {
            return node->child_edges()[1]->to;
        }
    }

    return n;
}

/// @brief Returns the segment a dummy node belongs to
/// Segments are identified by their p vertex
// NOLINTNEXTLINE(misc-no-recursion)
[[nodiscard]] auto EVertexOrdering::get_segment(const Node* node) const
    -> const Node* {
    // assert(is_dummy_[node]);

    const auto* parent = get_dummy_parent(node);
    if (parent == nullptr || !is_dummy_[parent]) {
        return node;
    }

    return get_segment(parent);
}

void EVertexOrdering::make_node_layers() {
    for (const auto* node : g_.nodes()) {
        const auto layer = layers_[node];

        if (is_dummy_[node]) {
            const auto* p = get_segment(node);

            if (long_edges_[p] == nullptr) {
                long_edges.emplace_back();
                long_edges.back().level.resize(node_layers_.size());
                long_edges_[p] = &long_edges.back();
            }
            auto* le          = long_edges_[p];
            long_edges_[node] = le;

            le->level[layers_[node]] = node;

            if (p == node) {
                le->p = node;
                node_layers_[layer].push_back(node);
            }

            const auto* child = get_dummy_child(node);
            if (child == nullptr || !is_dummy_[child]) {
                if (le->p == node) {
                    // We have node == p == q
                    // The long edge only has 1 vertex, we will treat it as a
                    // normal node

                    // We should of just created this long edge as
                    // node == p == q
                    // assert(le == &long_edges.back());
                    long_edges.pop_back();

                    long_edges_[node] = nullptr;
                } else {
                    le->q = node;
                    node_layers_[layer].push_back(node);
                }
            }

            continue;
        }
        node_layers_[layer].push_back(node);
    }

    for (const auto& le : long_edges) {
        // assert(le.p != nullptr);
        // assert(le.q != nullptr);
    }
}
