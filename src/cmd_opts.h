#pragma once

#include <limits>
#include <vector>
#include <string_view>

#include "types.h"

namespace CommandOptions {

class Part;
class Value;
class KwHandler;
struct CommandSpec;

/* 
 * Parser for complex option commands separated by ','.
 *
 * The argument has N core parameters that are required, followed by flags or keywords.
 * Example: path,path2,flag1,kw=x
 *
 * The parser uses callback in the form of the \ref Arg structure.
 */

void parse(std::string_view arg, const CommandSpec& spec);


class Value {
public:
    virtual ~Value();
    virtual void handle(std::string&& part) = 0;
};

class String: public Value {
public:
    String(std::string *dst);
    ~String();
    void handle(std::string&& part) override;
private:
    std::string *m_dst;
};

template<class T>
class Integer: public Value {
public:
    Integer(T *dst, T min = 0, T max = std::numeric_limits<T>::max());
    void handle(std::string&& part) override;
    ~Integer();
private:
    T *m_dst;
    T m_min;
    T m_max;
};

class Flag: public Value {
public:
    Flag(bool *dst);
    ~Flag();
    void handle(std::string&& part) override;
private:
    bool *m_dst;
};

class KwHandler {
public:
    virtual bool handle(Part&& part) = 0;
    virtual ~KwHandler();
};

class Part {
public:
    bool has_value() const { return !m_value.empty(); }

    std::string m_key;
    std::string m_value;
};

struct CommandSpec {
     std::initializer_list<Value*> core;
     std::initializer_list<KwHandler*> kwargs;
};

template <class T>
class KwArg: public KwHandler {
public:
    KwArg(std::string_view key, T&& value): m_key(key), m_value(std::move(value)) {}

    template <class... Args>
    KwArg(std::string_view key, Args... args): m_key(key), m_value(std::forward<Args...>(args...)) {}

    bool handle(Part&& part) override {
        if (part.m_key == m_key) {
            m_value.handle(std::move(part.m_value));
            return true;
        }
        return false;
    }
private:
    std::string_view m_key;
    T m_value;
};

}