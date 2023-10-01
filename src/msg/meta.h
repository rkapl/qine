#pragma once

#pragma  once
#include <stdint.h>
#include <stddef.h>

#include "../compiler.h"

struct QnxMessageField;

struct QnxMessageType {
    static constexpr uint16_t NO_SUBTYPE = 0xFFFF;

    uint16_t m_type;
    uint16_t m_subtype;

    const char* m_name;

    size_t m_field_count;
    const QnxMessageField *m_fields;

    size_t size() const;
};

struct QnxMessageField {
    enum class Format {
        U8, U16, U32, PID
    };
    enum class Presentation {
        DEFAULT, DEC, HEX
    };

    const char *m_name;
    size_t m_offset;
    size_t m_size;
    Format m_format;
    Presentation m_presentation;
};

struct QnxMessageList {
    size_t count_messages;
    const QnxMessageType* messages;
};

#include <gen_msg/proc.h>