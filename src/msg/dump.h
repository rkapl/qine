#include "meta.h"

#include <stdio.h>

class Msg;

namespace Meta {
    const Message * find_message(FILE *s, const MessageList &list, Msg &msg);
    void dump_structure(FILE* s, int indent, const Struct &str, Msg& msg);
}

#include "gen_msg/io.h"
#include "gen_msg/proc.h"