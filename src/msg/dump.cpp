#include "dump.h"
#include "../msg.h"
#include "msg/meta.h"
#include "qnx/msg.h"
#include <algorithm>
#include <bits/types/FILE.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <sys/types.h>

static void print_int(FILE *s, const Meta::Field& f, uint32_t v) {
    if (f.m_presentation == Meta::Field::Presentation::DEFAULT) {
        fprintf(s, "%d", v);
    } else {
        fprintf(s, "%Xh", v);
    }
}

static void print_field(FILE* s, const void *v, const Meta::Field& f) {
    switch (f.m_format) {
        case Meta::Field::Format::U8:
            print_int(s, f, *static_cast<const uint8_t*>(v));
            break;
        case Meta::Field::Format::U16:
            print_int(s, f, *static_cast<const uint16_t*>(v));
            break;
        case Meta::Field::Format::U32:
            print_int(s, f, *static_cast<const uint32_t*>(v));
            break;
        case Meta::Field::Format::PID:
            fprintf(s, "pid %d", *static_cast<const uint32_t*>(v));
            break;
        case Meta::Field::Format::NID:
            fprintf(s, "nid %d", *static_cast<const uint32_t*>(v));
            break;
        case Meta::Field::Format::PATH: {
            std::string str;
            str.append(static_cast<const char*>(v), 256u);
            fprintf(s, "%s", str.c_str());
        }; break;
        default:
            fprintf(s, "unknown field type");
    }
}

void Meta::dump_structure(FILE* s, int indent, const Meta::Struct &t, Msg& msg) {
    std::unique_ptr<uint8_t[]> msg_buf(new uint8_t[t.m_size]);
    msg.read(msg_buf.get(), 0, t.m_size);

    
    for(size_t fi = 0; fi < t.m_field_count; ++fi) {
        auto& f = t.m_fields[fi];
        fprintf(s, "@%02x ", static_cast<uint32_t>(f.m_offset));
        fprintf(s, "   %s: ", f.m_name);

        auto field_data = static_cast<void*>(msg_buf.get() + f.m_offset);
        print_field(s, field_data, f);
        fprintf(s, "\n");
    }
}

const Meta::Message* Meta::find_message(FILE *s, const MessageList &list, Msg &msg) {
    Qnx::MsgHeader hdr; 
    msg.read_type(&hdr);

    
    auto match_msg = [&hdr] (const Meta::Message* t) -> bool {
        bool type_match = t->m_type == hdr.type;
        bool subtype_match = (t->m_subtype == hdr.subtype);
        
        return type_match && (!t->m_has_subtype || subtype_match);
    };

    auto t_end = &list.messages[list.count_messages];
    auto t = std::find_if(list.messages, t_end, match_msg);


    if (t == t_end) {
        fprintf(s, "Unknown message (%x: %x)\n", hdr.type, hdr.subtype);
        return nullptr;
    }

    return *t;
}