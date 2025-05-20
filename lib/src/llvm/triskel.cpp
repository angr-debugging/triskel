#include "triskel/triskel.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include <fmt/format.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/Casting.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include "llvm/IR/ModuleSlotTracker.h"

#include "triskel/llvm/llvm.hpp"

auto triskel::make_layout(llvm::Function* function,
                          Renderer* render,
                          llvm::ModuleSlotTracker* MST)
    -> std::unique_ptr<CFGLayout> {
    auto builder = make_layout_builder();

    // Important, otherwise the CFGs are not necessarily well defined
    llvm::EliminateUnreachableBlocks(*function);

    std::map<llvm::BasicBlock*, size_t> reverse_map;

    for (auto& block : *function) {
        auto content = std::string{};

        for (const auto& insn : block) {
            content += triskel::to_string(insn, MST) + "\n";
        }

        auto node = builder->make_node(content);

        // Adds the block to the maps
        reverse_map.insert_or_assign(&block, node);
    }

    for (auto& block : *function) {
        auto node = reverse_map.at(&block);

        llvm::BasicBlock* true_edge  = nullptr;
        llvm::BasicBlock* false_edge = nullptr;

        const auto& insn = block.back();
        if (const auto* branch = llvm::dyn_cast<llvm::BranchInst>(&insn)) {
            if (branch->isConditional()) {
                true_edge  = branch->getSuccessor(0);
                false_edge = branch->getSuccessor(1);
            }
        }

        for (auto* child : llvm::successors(&block)) {
            auto child_node = reverse_map.at(child);

            auto type = LayoutBuilder::EdgeType::Default;

            if (child == true_edge) {
                type = LayoutBuilder::EdgeType::True;
            } else if (child == false_edge) {
                type = LayoutBuilder::EdgeType::False;
            }

            builder->make_edge(node, child_node, type);
        }
    }

    fmt::print("Measuring nodes\n");
    if (render != nullptr) {
        builder->measure_nodes(*render);
    }
    fmt::print("Starting layout\n");
    return builder->build();
}

auto triskel::Color::from_hex(uint32_t rgba) -> triskel::Color {
    const uint8_t r = ((rgba >> 24) & 0xFF);
    const uint8_t g = ((rgba >> 16) & 0xFF);
    const uint8_t b = ((rgba >> 8) & 0xFF);
    const uint8_t a = (rgba & 0xFF);
    return {.r = r, .g = g, .b = b, .a = a};
}

auto triskel::Color::to_hex() const -> uint32_t {
    return (static_cast<uint32_t>(r) << 24) + (static_cast<uint32_t>(g) << 16) +
           (static_cast<uint32_t>(b) << 8) + static_cast<uint32_t>(a);
}

auto triskel::Color::to_hex_string() const -> std::string {
    return fmt::format("#{:08X}", to_hex());
}