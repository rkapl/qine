#pragma once

#include <stdint.h>
#include <stddef.h>
#include "qnx/term.h"

#include "../compiler.h"

namespace Qnx {
    /* See comment in io_msg.h */
    using PathBuf = char[256];

    using Termios = Qnx::termios;
    using Stdfds = uint8_t[10];
    using Time = int32_t;
}

namespace Meta {
    struct Struct;
    struct Field;
    struct Message;
    struct MessageList;

    struct Struct {
        size_t m_field_count;
        const Field *m_fields;
        size_t m_size;
    };

    struct Field {
        enum class Format {
            SUB, CHAR, U8, U16, U32, I8, I16, I32,  PID, NID, FD, PATH, TERMIOS, TIME, STDFDS
        };
        enum class Presentation {
            DEFAULT, HEX, OCT
        };

        const char *m_name;
        size_t m_offset;
        size_t m_size;
        Format m_format;
        Presentation m_presentation;
        const Struct *m_typeref;
        size_t m_array_len;
    };

    struct Message {
        const char* m_name;
        uint16_t m_type;
        bool m_has_subtype;   
        uint16_t m_subtype;

        const Struct *m_request;    
        const Struct *m_reply;
    };

    struct MessageList {
        size_t count_messages;
        const Message** messages;
    };
};