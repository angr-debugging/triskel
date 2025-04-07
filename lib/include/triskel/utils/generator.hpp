#pragma once

#include <generator>
#include <vector>

template <typename T>
auto span_to_vec(const std::span<T>& gen) -> std::vector<T> {
    return {gen.begin(), gen.end()};
}
