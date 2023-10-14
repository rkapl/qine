#include "meta.h"

#include <cstdint>
#include <stdio.h>

class Msg;

namespace Meta {
    const Message * find_message(FILE *s, const MessageList &list, Msg &msg);

    void dump_substructure(FILE* s, const Struct &str, int indent, size_t offset, const uint8_t* msg_buf);
    void dump_structure(FILE* s, const Struct &str, Msg& msg);
    void dump_structure_written(FILE* s, const Struct &str, Msg& msg);
}

#include "gen_msg/io.h"
#include "gen_msg/proc.h"