#include "dump.h"
#include "../msg.h"
#include "msg/meta.h"
#include "qnx/msg.h"
#include <algorithm>
#include <bits/types/FILE.h>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <sys/types.h>

static void print_int(FILE *s, const QnxMessageField&f, uint32_t v) {
    if (f.m_presentation == QnxMessageField::Presentation::DEFAULT) {
        fprintf(s, "%d", v);
    } else {
        fprintf(s, "%Xh", v);
    }
}

static void print_field(FILE* s, const void *v, const QnxMessageField& f) {
    switch (f.m_format) {
        case QnxMessageField::Format::U8:
            print_int(s, f, *static_cast<const uint8_t*>(v));
            break;
        case QnxMessageField::Format::U16:
            print_int(s, f, *static_cast<const uint16_t*>(v));
            break;
        case QnxMessageField::Format::U32:
            print_int(s, f, *static_cast<const uint32_t*>(v));
            break;
        case QnxMessageField::Format::PID:
            fprintf(s, "pid %d", *static_cast<const uint16_t*>(v));
            break;
        default:
            fprintf(s, "unknown field type");
    }
}

void QnxMsg::dump_message(FILE* s, const QnxMessageList& list,  Msg& msg) {
    Qnx::MsgHeader hdr; 
    msg.read_type(&hdr);
    
    auto match_msg = [&hdr] (const QnxMessageType& t) -> bool {
        bool type_match = t.m_type == hdr.type;
        bool subtype_match = (t.m_subtype == hdr.subtype);
        switch(t.m_match) {
            case QnxMessageType::Kind::MSG_TYPE:
                return type_match;
            case QnxMessageType::Kind::MSG_SUBTYPE:
                return type_match && subtype_match;
            default:
                return false;
        }
    };

    auto t_end = &list.messages[list.count_messages];
    auto t = std::find_if(list.messages, t_end, match_msg);

    if (t == t_end) {
        fprintf(s, "Unknown message (%x: %x)\n", hdr.type, hdr.subtype);
        return;
    }

    std::unique_ptr<uint8_t[]> msg_buf(new uint8_t[t->size()]);
    msg.read(msg_buf.get(), 0, t->size());

    fprintf(s, "message %s {\n", t->m_name);
    for(size_t fi = 0; fi < t->m_field_count; ++fi) {
        auto& f = t->m_fields[fi];
        fprintf(s, "@%02x ", static_cast<uint32_t>(f.m_offset));
        fprintf(s, "   %s: ", f.m_name);

        auto field_data = static_cast<void*>(msg_buf.get() + f.m_offset);
        print_field(s, field_data, f);
        fprintf(s, "\n");
    }
    fprintf(s, "}\n");

}