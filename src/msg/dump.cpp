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

static void print_indent(FILE *s, int indent) {
    for (int i = 0; i < indent; i++) {
        fputs("  ", s);
    }
}

static void print_int(FILE *s, const Meta::Field& f, uint32_t v) {
    if (f.m_presentation == Meta::Field::Presentation::DEFAULT) {
        fprintf(s, "%d", v);
    } else if (f.m_presentation == Meta::Field::Presentation::OCT) {
        fprintf(s, "%oo", v);
    } else {
        fprintf(s, "%Xh", v);
    }
}

static void print_field(FILE* s, const Meta::Field& f, int indent, size_t offset, const uint8_t *msg_buf) {
    auto v = static_cast<const void*>(msg_buf + offset);
    switch (f.m_format) {
        case Meta::Field::Format::SUB:
            fprintf(s, "{\n");
            Meta::dump_substructure(s, *f.m_typeref, indent + 1, offset, msg_buf);
            print_indent(s, indent);
            fputs("   ", s);
            fprintf(s, "}");
            break;
        case Meta::Field::Format::CHAR:
            if (f.m_array_len == 0) {
                fprintf(s, "'%c'", *static_cast<const char*>(v));
            } else {
                fprintf(s, "\"%s\"", static_cast<const char*>(v));
            }
            break;
        case Meta::Field::Format::U8:
        case Meta::Field::Format::I8:
            print_int(s, f, *static_cast<const uint8_t*>(v));
            break;
        case Meta::Field::Format::U16:
        case Meta::Field::Format::I16:
            print_int(s, f, *static_cast<const uint16_t*>(v));
            break;
        case Meta::Field::Format::U32:
        case Meta::Field::Format::I32:
            print_int(s, f, *static_cast<const uint32_t*>(v));
            break;
        case Meta::Field::Format::PID:
            fprintf(s, "pid %d", *static_cast<const uint16_t*>(v));
            break;
        case Meta::Field::Format::NID:
            fprintf(s, "nid %d", *static_cast<const uint32_t*>(v));
            break;
        case Meta::Field::Format::FD:
            fprintf(s, "fd %d", *static_cast<const int16_t*>(v));
            break;
        case Meta::Field::Format::PATH: {
            std::string str;
            str.append(static_cast<const char*>(v), 256u);
            fprintf(s, "%s", str.c_str());
        }; break;
        case Meta::Field::Format::STDFDS: {
            const Qnx::Stdfds *fds = static_cast<const Qnx::Stdfds*>(v);
            const char *fd_names[] = {"stdin", "stdout", "stderr"};
            fprintf(s, "{");
            for (int i = 0; i < 10; i++) {
                int fd = (*fds)[i];
                if (fd == 255)
                    continue;
                if (i < 2) {
                    fprintf(s, " %s = %d,", fd_names[i], fd);
                } else {
                    fprintf(s, " [%d] = %d,", i, fd);
                }
            }
            fprintf(s, "}");
        }; break;
        default:
            fprintf(s, "unknown field type");
    }
}

void Meta::dump_substructure(FILE* s, const Struct &t, int indent, size_t offset, const uint8_t* msg_buf)
{
    for(size_t fi = 0; fi < t.m_field_count; ++fi) {
        auto& f = t.m_fields[fi];
        fprintf(s, "@%02x ", static_cast<uint32_t>(offset + f.m_offset));
        print_indent(s, indent);
        fprintf(s, "%s: ", f.m_name);

        print_field(s, f, indent, offset + f.m_offset, msg_buf);
        fprintf(s, "\n");
    }
}

void Meta::dump_structure(FILE* s, const Meta::Struct &t, Msg& msg) {
    std::unique_ptr<uint8_t[]> msg_buf(new uint8_t[t.m_size]);
    msg.read(msg_buf.get(), 0, t.m_size);

    dump_substructure(s, t, 1, 0, msg_buf.get());
}

void Meta::dump_structure_written(FILE* s, const Meta::Struct &t, Msg& msg) {
    std::unique_ptr<uint8_t[]> msg_buf(new uint8_t[t.m_size]);
    msg.read_written(msg_buf.get(), 0, t.m_size);

    dump_substructure(s, t, 1, 0, msg_buf.get());
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