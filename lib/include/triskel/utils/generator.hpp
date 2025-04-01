#pragma once

#include <generator>
#include <vector>

template <typename T>
auto gen_to_v(std::generator<T>&& gen) -> std::vector<T> {
    std::vector<T> vec;
    for (const auto& e : gen) {
        vec.push_back(e);
    }
    return vec;
}

template <typename T>
auto gen_to_v(std::generator<T>&& gen, size_t size) -> std::vector<T> {
    std::vector<T> vec;
    vec.reserve(size);

    for (const auto& e : gen) {
        vec.push_back(e);
    }
    return vec;
}