#pragma once

#include <new>
#include <vector>

/* For FDs, PIDs, selectors etc. 
* T must have default value usable for empty slots and be default-insertable. */
template <class T>
class IdMap {
public:
    inline IdMap(size_t m_max);

    T* alloc();
    T* get(size_t i);
    size_t index_of(T* element);
private:
    size_t m_max;
    std::vector<T> m_data;
    std::vector<bool> m_free;
};

template <class T>
IdMap<T>::IdMap(size_t max): m_max(max) {
    m_data.resize(m_max);
    m_free.resize(m_max, true);
}

template <class T>
T* IdMap<T>::alloc() {
    for (size_t i = 0; i < m_max; i++) {
        if (m_free[i]) {
            m_free[i] = false;
            return &m_data[i];
        }
    }
    throw std::bad_alloc();
}

template <class T>
T* IdMap<T>::get(size_t i) {
    if (m_free[i])
        return nullptr;
    return &m_data[i];
}

template <class T>
size_t IdMap<T>::index_of(T* element)
{
    return element - &m_data[0];
}