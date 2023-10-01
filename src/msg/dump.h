#include "meta.h"

#include <stdio.h>

class Msg;

namespace QnxMsg {
    void dump_message(FILE* s, const QnxMessageList& list,  Msg& msg);
}
