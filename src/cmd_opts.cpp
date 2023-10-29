#include "cmd_opts.h"
#include "types.h"
#include "util.h"

namespace CommandOptions {

struct Tokenizer {
    std::string_view v;
    // After the last ',' or at start, may be out of bounds
    size_t pos;

    bool eof() {
        return pos >= v.size();
    }
    std::string_view next() {
        auto sep = v.find(',', pos);
        if (sep == v.npos) {
            auto r = v.substr(pos);
            pos = v.size();
            return r;
        } else {
            auto r = v.substr(pos, sep - pos);
            pos = sep + 1;
            return r;
        }
    }
};

struct CommandSpecDeleter {
    const CommandSpec& spec;
    ~CommandSpecDeleter() {
        for (auto s: spec.core)
            delete s;
        for (auto s: spec.kwargs)
            delete s;
    }
};

void parse(std::string_view arg, const CommandSpec &spec) {
    CommandSpecDeleter spec_delete{spec};
    Tokenizer t{arg, 0};
    int argi = 1;
    for (auto spec: spec.core) {
        if (t.eof()) {
            if (argi == 1)
                throw ConfigurationError("Argument missing");
            else
                throw ConfigurationError(std_printf("Argument part %d not given", argi));
        }
        spec->handle(std::string(t.next()));
    }

    while (!t.eof()) {
        auto s = t.next();
        if (s.empty()) {
            throw ConfigurationError("Empty optional argument part");
        }
        auto sep = s.find('=');
        Part p;
        if (sep == s.npos) {
            p.m_key = s;
        } else {
            p.m_key = s.substr(0, sep);
            p.m_value = s.substr(sep + 1);
        }
        bool handled = false;
        for (auto spec: spec.kwargs) {
            if (spec->handle(std::move(p))) {
                handled = true;
                break;
            }
        }
        if (!handled) {
            std::string key(p.m_key);
            throw ConfigurationError(std_printf("Argument part '%s' is not recognized", key.c_str()));
        }
    }
}

Value::~Value() {}
KwHandler::~KwHandler() {}

Flag::Flag(bool *dst): m_dst(dst) {}
Flag::~Flag() {}
void Flag::handle(std::string&& v) {
    if (v.empty() || v == "1") {
        *m_dst = true;
    } else if (v == "0") {
        *m_dst = false;
    } else {
        throw ConfigurationError("flag expects 0 or 1");
    }
}

String::String(std::string *dst): m_dst(dst) {}
String::~String() {}
void String::handle(std::string&& v) {
    *m_dst = std::move(v);
}

template<class T>
Integer<T>::Integer(T *dst, T min, T max): m_dst(dst), m_min(min), m_max(max) {

}

template<class T>
void Integer<T>::handle(std::string&& part) {
    T v;
    char *end;
    errno = 0;
    if (std::is_signed<T>::value) {
        v = strtoll(part.c_str(), &end, 0);
    } else {
        v = strtoull(part.c_str(), &end, 0);
    }
    if (errno == ERANGE) {
        throw ConfigurationError("Argument is out of range");
    }
    if (*end != 0)
        throw ConfigurationError("Argument is not an integer");

    if (std::is_signed<T>::value) {
        if (v < m_min)
            throw ConfigurationError(std_printf("Argument is smaller than %lld", static_cast<long long>(m_min)));
        if (v > m_max)
            throw ConfigurationError(std_printf("Argument is bigger than %lld", static_cast<long long>(m_max)));
    } else {
        if (v < m_min)
            throw ConfigurationError(std_printf("Argument is smaller than %llu", static_cast<unsigned long long>(m_min)));
        if (v > m_max)
            throw ConfigurationError(std_printf("Argument is bigger than %llu", static_cast<unsigned long long>(m_max)));
    }

    *m_dst = v;
}

template<class T>
Integer<T>::~Integer() {}

template class Integer<uint8_t>;
template class Integer<uint16_t>;
template class Integer<uint32_t>;
template class Integer<uint64_t>;
template class Integer<int8_t>;
template class Integer<int16_t>;
template class Integer<int32_t>;
template class Integer<int64_t>;

}