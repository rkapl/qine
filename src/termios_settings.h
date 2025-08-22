#include "cpp.h"
#pragma 

#include <termios.h>
#include <gen_msg/dev.h>

/** RAII guard that applies temporary termios settings */
class TermiosSettings: public NoCopy {
public:
    termios m_settings;

    TermiosSettings(int fd);
    bool ok() const { return m_ok;}
    int fd() const {return m_fd;}
    void from_dev_read(const QnxMsg::dev::read_request& r);
    static void approximate(const QnxMsg::dev::read_request& r, termios* dst);
    bool set();
    ~TermiosSettings();
private:
    int m_fd;
    bool m_ok;
    termios m_old;
};