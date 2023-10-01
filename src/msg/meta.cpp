#include "meta.h"

#include <algorithm>

size_t QnxMessageType::size() const {
    size_t end = 0;
    for (size_t i = 0; i < m_field_count; i++) {
        auto f = m_fields[i];
        end = std::max(end, f.m_offset + f.m_size);
    }
    return end;
}