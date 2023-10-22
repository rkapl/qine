#pragma once

#include <vector>
#include <type_traits>
#include <memory>
#include <limits>
#include <assert.h>

class NoFreeId: public std::bad_alloc {};
class IdAlreadyTaken: public std::bad_alloc {};

/* 
* For FDs, PIDs, selectors etc. 
*
* Each slot can be used/free.
*/
template <class T>
class IdMap {
public:
    using Id = size_t;
    using OptId = size_t;
    static constexpr size_t INVAL = std::numeric_limits<size_t>::max();

    inline IdMap(size_t max);

    template <class C> T* alloc_any(C&& creator);
    template <class C> T* alloc_exactly_at(size_t i, C&& creator);
    template <class C> T* alloc_or_get_at(size_t i, C&& creator);
    template <class C> T* alloc_starting_at(size_t i, C&& creator);

    T* operator[](Id i);
    void free(Id i);
    ~IdMap();
private:
    /* Internally, Id can  refer to the unallocate places, do not assume the map has grown */

    template <class C> T* emplace(size_t i, C&& creator);
    // Grow the data and map to a given size. The specified size must be less than max.
    void grow(size_t to_size);
    // Look for a free item, either used or unused (as specified). Returns INVAL if not found
    Id search(Id from, bool used);

    size_t m_max;
    std::vector<std::unique_ptr<T>> m_data;
};

// public
template <class T>
IdMap<T>::IdMap(size_t max): m_max(max) {
}

template <class T> template <class C> T* IdMap<T>::alloc_any(C&& creator) {
    OptId i = search(0, false);
    if (i == INVAL) {
        throw NoFreeId();
    }
    return emplace(i, creator);
}
template <class T> template <class C> T* IdMap<T>::alloc_exactly_at(size_t i, C&& creator) {
    if (i >= m_max)
        throw NoFreeId();
    if ((*this)[i])
        throw IdAlreadyTaken();
        
    return emplace(i, creator);
}

template <class T> template <class C> T* IdMap<T>::alloc_or_get_at(size_t i, C&& creator) {
    if (i >= m_max)
        throw NoFreeId();
    
    if ((*this)[i])
        return m_data[i];

    return emplace(i, creator);
}

template <class T> template <class C> T* IdMap<T>::alloc_starting_at(size_t from, C&& creator) {
    if (from >= m_max)
        return nullptr;

    OptId i = search(from, false);
    if (i == INVAL) {
        throw NoFreeId();
    }
    return emplace(i, creator);
}

template <class T>
T* IdMap<T>::operator[](Id i) {
    if (i >= m_data.size())
        return nullptr;
    return m_data[i].get();
}

template <class T>
void IdMap<T>::free(Id i) {
    m_data[i].release();
}


// private:
template <class T> template <class C> T* IdMap<T>::emplace(size_t i, C&& creator) {
    grow(i + 1);
    T* p = creator(i);
    m_data[i].reset(p);
    return p;
}

template <class T>
auto IdMap<T>::search(Id from, bool used) -> Id {
    for (; from < m_data.size(); from++) { 
        if (bool(m_data[from]) == used) {
            return from;
        }
    }
    // if looking for unused, we can return the first from the unallocated space
    if (!used && from < m_max) {
        return from;
    }
    return INVAL;
}

template <class T>
void IdMap<T>::grow(size_t to_size) {
    assert(to_size <= m_max);
    if (to_size <= m_data.size())
        return;
    
    m_data.resize(to_size);
}

template <class T>
IdMap<T>::~IdMap() {
}