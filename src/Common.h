#ifndef COMMON_H
#define COMMON_H

#include <cstdint>
#include <cassert>
#include <algorithm>
#include <memory>
#include <vector>
#include <map>

template<class T> size_t GetIndex(const std::vector<T> & vec, T value) { return std::find(begin(vec), end(vec), value) - begin(vec); }
template<class T, class F> void EraseIf(T & container, F f) { container.erase(remove_if(begin(container), end(container), f), end(container)); }

#endif