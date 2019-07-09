#pragma once

template<class T> size_t
vector_size(const std::vector<T>& v) {
    size_t result = sizeof(v[0]) * v.size();
    return result;
}
