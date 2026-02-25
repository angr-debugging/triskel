#pragma once


#include <vector>
#include <span>

template <typename T>
auto span_to_vec(const std::span<T>& gen) -> std::vector<T> {
    return {gen.begin(), gen.end()};
}
