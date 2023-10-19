#include <asm-generic/ioctls.h>
#include <dirent.h>
#include <errno.h>
#include <iterator>
#include <stdint.h>
#include <ios>
#include <limits>
#include <stdexcept>
#include <string.h>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ucontext.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <termios.h>

#include "gen_msg/common.h"
#include "gen_msg/dev.h"
#include "gen_msg/fsys.h"
#include "mem_ops.h"
#include "msg.h"
#include "msg/meta.h"
#include "process.h"
#include "msg_handler.h"
#include "main_handler.h"
#include "qnx/errno.h"
#include "qnx/io.h"
#include "qnx/msg.h"
#include "qnx/osinfo.h"
#include "qnx/procenv.h"
#include "qnx/psinfo.h"
#include "qnx/types.h"
#include "segment_descriptor.h"
#include "types.h"
#include "log.h"
#include <gen_msg/proc.h>
#include <gen_msg/io.h>
#include <vector>


void MainHandler::receive(MsgInfo& i) {
    Qnx::MsgHeader hdr;
    i.msg().read_type(&hdr);

    auto unhandled_msg = [&hdr] {
        Log::print(Log::UNHANDLED, "Unhandled message %xh:%xh\n", hdr.type, hdr.subtype);
    };

    switch (hdr.type) {
        case QnxMsg::proc::msg_segment_realloc::TYPE :
            switch (hdr.subtype) {
                case QnxMsg::proc::msg_segment_realloc::SUBTYPE:
                    proc_segment_realloc(i);
                break;
                case QnxMsg::proc::msg_segment_alloc::SUBTYPE:
                    proc_segment_alloc(i);
                break;
                default:
                    unhandled_msg();
                break;
            }
            break;
        case QnxMsg::proc::msg_time::TYPE:
            proc_time(i);
            break;
        case QnxMsg::proc::msg_terminate::TYPE:
            proc_terminate(i);
            break;
        case QnxMsg::proc::msg_open::TYPE:
            proc_open(i);
            break;
        case QnxMsg::proc::msg_fd_attach::TYPE:
            switch (hdr.subtype) {
                case QnxMsg::proc::msg_fd_attach::SUBTYPE:
                    proc_fd_attach(i);
                break;
                case QnxMsg::proc::msg_fd_detach::SUBTYPE:
                    proc_fd_detach(i);
                break;
                case QnxMsg::proc::msg_fd_query::SUBTYPE:
                    proc_fd_query(i);
                break;
                case QnxMsg::proc::msg_fd_action1::SUBTYPE:
                    proc_fd_action1(i);
                    break;
                default:
                    unhandled_msg();
                break;
            };
            break;
        case QnxMsg::proc::msg_vc_attach::TYPE:
            proc_vc_attach(i);
            break;
        case QnxMsg::proc::msg_vc_detach::TYPE:
            proc_vc_detach(i);
            break;
        case QnxMsg::proc::msg_psinfo::TYPE:
            proc_psinfo(i);
            break;
        case QnxMsg::proc::msg_sigtab::TYPE:
            switch(hdr.subtype) {
                case QnxMsg::proc::msg_sigtab::SUBTYPE:
                    proc_sigtab(i);
                    break;
                case QnxMsg::proc::msg_sigact::SUBTYPE:
                    proc_sigact(i);
                    break;
                case QnxMsg::proc::msg_sigmask::SUBTYPE:
                    proc_sigmask(i);
                    break;
                default:
                    unhandled_msg();
                    break;
            }; 
            break;
        case QnxMsg::proc::msg_getid::TYPE: {
            switch (hdr.subtype) {
                case QnxMsg::proc::msg_getid::SUBTYPE:
                    proc_getid(i);
                    break;
                default:
                    unhandled_msg();
                    break;
            }
        };
        break;
        case QnxMsg::proc::msg_osinfo::TYPE:
            proc_osinfo(i);
            break;
        case QnxMsg::proc::msg_spawn::TYPE:
            proc_spawn(i);
            break;
        case QnxMsg::proc::msg_fork::TYPE:
            proc_fork(i);
            break;
        case QnxMsg::proc::msg_exec::TYPE:
            proc_exec(i);
            break;
        case QnxMsg::proc::msg_wait::TYPE:
            proc_wait(i);
            break;

        case QnxMsg::io::msg_handle::TYPE:
        case QnxMsg::io::msg_io_open::TYPE:
            io_open(i);
            break;
        case QnxMsg::io::msg_stat::TYPE:
            io_stat(i);
            break;
        case QnxMsg::io::msg_fstat::TYPE:
            io_fstat(i);
            break;
        case QnxMsg::io::msg_close::TYPE:
            io_close(i);
            break;
        case QnxMsg::io::msg_read::TYPE:
            io_read(i);
            break;
        case QnxMsg::io::msg_write::TYPE:
            io_write(i);
            break;
        case QnxMsg::io::msg_lseek::TYPE:
            io_lseek(i);
            break;
        case QnxMsg::io::msg_readdir::TYPE:
            io_readdir(i);
            break;
        case QnxMsg::io::msg_rewinddir::TYPE:
            io_rewinddir(i);
            break;
        case QnxMsg::io::msg_fcntl_flags::TYPE:
            io_fcntl_flags(i);
            break;
        case QnxMsg::io::msg_dup::TYPE:
            io_dup(i);
            break;

        case QnxMsg::fsys::msg_unlink::TYPE:
            fsys_unlink(i);
            break;

        case QnxMsg::dev::msg_tcgetattr::TYPE:
            dev_tcgetattr(i);
            break;
        case QnxMsg::dev::msg_tcsetattr::TYPE:
            dev_tcsetattr(i);
            break;
        case QnxMsg::dev::msg_term_size::TYPE:
            dev_term_size(i);
            break;

        default:
            unhandled_msg();
            break;
    }
}

void MainHandler::proc_terminate(MsgInfo& i)
{
    QnxMsg::proc::terminate_request msg;
    i.msg().read_type(&msg);
    //i.ctx().dump(stdout);
    exit(msg.m_status);
}

void MainHandler::proc_time(MsgInfo& i)
{
    QnxMsg::proc::time_request msg;
    i.msg().read_type(&msg);
    QnxMsg::proc::time_reply reply;
    memset(&reply, 0, sizeof(reply));
    if (msg.m_seconds == std::numeric_limits<uint32_t>::max()) {
        struct timespec ts;
        if (clock_gettime(CLOCK_REALTIME, &ts) < 0) {
            reply.m_status = Emu::map_errno(errno);
        } else {
            reply.m_status = Qnx::QEOK;
            reply.m_seconds = ts.tv_sec;
            reply.m_nsec = ts.tv_nsec;
        }
    } else {
        reply.m_status = Qnx::QENOTSUP;
        Log::print(Log::UNHANDLED, "set time not supported\n");
    }
    i.msg().write_type(0, &reply);
}

void MainHandler::proc_fd_attach(MsgInfo& i)
{
    /* FD allocation strategy:
     * We keep 1:1 mapping with Linux FD. That keeps things simple for now. It also allows us to inherit FDs.
     * 
     * There is currently a state in QNX that we will called "reserved. E.g. if QNX proc
     * attaches to the FD, but does not yet open it, it is reserved. In the future, we should
     * keep all this info, but for now we do not and just ignore the reservations and hope it will work out.
     * 
     * We also keep handle = fd at all time.
     */
    QnxMsg::proc::fd_request msg;
    i.msg().read_type(&msg);

    int fd = msg.m_fd;
    if (fd == 0) {
        /* Locate a free descriptor by doing open */
        fd = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
        if (fd < 0) {
            if (errno == EMFILE){
                i.msg().write_status(Qnx::QEMFILE);
                return;
            } else if (errno == ENFILE) {
                i.msg().write_status(Qnx::QENFILE);
                return;
            } else {
                throw std::system_error(errno, std::generic_category());
            }
        }
    }

    QnxMsg::proc::fd_reply1 reply;
    memset(&reply, 0, sizeof(reply));
    reply.m_status = 0;
    reply.m_fd = fd;
    i.msg().write_type(0, &reply);
}

void MainHandler::proc_fd_detach(MsgInfo& i) {
    /* We do not do reservations */
    i.msg().write_status(Qnx::QEOK);
}

void MainHandler::proc_fd_query(MsgInfo& i) {
    QnxMsg::proc::fd_request msg;
    i.msg().read_type(&msg);

    QnxMsg::proc::fd_query_reply reply;
    memset(&reply, 0, sizeof(reply));
    reply.m_status = 0;
    reply.m_info.m_handle = 0;
    reply.m_info.m_pid = 1;
    reply.m_info.m_vid = reply.m_info.m_pid;
    reply.m_info.m_nid = 0;
    reply.m_fd = msg.m_fd;
    i.msg().write_type(0, &reply);
}

void MainHandler::proc_fd_action1(MsgInfo &i) {
    // STUB
    i.msg().write_status(Qnx::QEOK);
}

void MainHandler::proc_segment_alloc(MsgInfo& i)
{
    QnxMsg::proc::segment_request msg;
    i.msg().read_type(&msg);
    
    auto seg  = i.ctx().proc()->allocate_segment();
    seg->reserve(MemOps::mega(1));
    seg->grow(Access::READ_WRITE, MemOps::align_page_up(msg.m_nbytes));

    auto sd = i.ctx().proc()->create_segment_descriptor(Access::READ_WRITE, seg);
    
    QnxMsg::proc::segment_reply reply;
    memset(&reply, 0, sizeof(reply));
    reply.m_status = Qnx::QEOK;
    reply.m_sel = sd->selector();
    reply.m_nbytes = seg->size();
    reply.m_addr = reinterpret_cast<uint32_t>(seg->location()); 

    i.msg().write_type(0, &reply);
}

void MainHandler::proc_segment_realloc(MsgInfo& i)
{
    QnxMsg::proc::segment_request msg;
    i.msg().read_type(&msg);
    // TODO: handle invalid segments
    auto sd  = i.ctx().proc()->descriptor_by_selector(msg.m_sel);
    auto seg = sd->segment();
    GuestPtr base = seg->paged_size();

    QnxMsg::proc::segment_reply reply;
    memset(&reply, 0, sizeof(reply));
    if (seg->is_shared()) {
        reply.m_status = Qnx::QEBUSY;
    } else {
        if (seg->size() < msg.m_nbytes) {
            seg->grow(Access::READ_WRITE, MemOps::align_page_up(msg.m_nbytes - seg->size()));
            // TODO: this must be done better, the segment must be aware of its descriptors,
            // at least code and data 
            sd->update_descriptors();
        }
        reply.m_status = Qnx::QEOK;
        reply.m_sel = msg.m_sel;
        reply.m_nbytes = seg->size();
        reply.m_addr = reinterpret_cast<uint32_t>(seg->location()); 
        //printf("New segment size: %x\n", seg->size());
    }
    i.msg().write_type(0, &reply);
}

void MainHandler::proc_open(MsgInfo& i) {
    /* proc_open does not actually open a file, just resolves the file name*/
    QnxMsg::common::open msg;
    i.msg().read_type(&msg);

    msg.m_type = Qnx::QEOK;
    msg.m_pid = 1;
    i.msg().write_type(0, &msg);
}

void MainHandler::proc_vc_attach(MsgInfo& i) 
{
    auto proc = i.ctx().proc();
    QnxMsg::proc::vc vc;
    i.msg().read_type(&vc);

    if (vc.m_nid == 0 || vc.m_nid == proc->nid()) {
        if (vc.m_pid == proc->pid()) {
            QnxMsg::proc::vc_attach_reply reply;
            clear(&reply);
            reply.m_status = Qnx::QEOK;
            reply.m_pid = vc.m_pid;
            i.msg().write_type(0, &reply);
        } else {
            i.msg().write_status(Qnx::QESRCH);
        }
    } else {
        // networking not supported
        i.msg().write_status(Qnx::QENOSYS);
    } 
}


void MainHandler::proc_vc_detach(MsgInfo& i) 
{
    /* This is stubbed, because QNX/slib seems to have a bug where the stat result
     * overwrites the NID in the message and it then mistakenly calls vc_detach.*/

     /* Also calls to proc_vc_attach succeed on local PID, and then the caller can try to detach it */
    i.msg().write_status(Qnx::QEOK);
}

void MainHandler::proc_psinfo(MsgInfo &i)
{
    auto proc = Process::current();
    // known user: wlink tries to find its own exec (via _cmdname   )
    i.msg().write_status(Qnx::QEOK);
    Qnx::psinfo ps;
    memset(&ps, 0, sizeof(ps));
    ps.pid = proc->pid();
    ps.rgid = getgid();
    ps.ruid = getuid();
    ps.egid = getegid();
    ps.euid = geteuid();
    ps.proc.father = proc->parent_pid();

    strlcpy(ps.proc.name, proc->file_name().c_str(), sizeof(ps.proc.name));
    
    i.msg().write_type(2, &ps);
}

void MainHandler::proc_sigtab(MsgInfo &i) {
    auto proc = Process::current();
    QnxMsg::proc::signal_request msg;
    i.msg().read_type(&msg);

    auto sigtabv = proc->translate_segmented(FarPointer(i.ctx().reg_ds(), msg.m_offset), sizeof(Qnx::Sigtab));
    proc->m_sigtab = static_cast<Qnx::Sigtab*>(sigtabv);
    
    i.msg().write_status(Qnx::QEOK);
}

void MainHandler::proc_sigact(MsgInfo &i) {
    QnxMsg::proc::signal_request msg;
    i.msg().read_type(&msg);

    QnxMsg::proc::signal_reply reply;
    memset(&reply, 0, sizeof(reply));
    int r = i.ctx().proc()->m_emu.signal_sigact(msg.m_signum, msg.m_offset, msg.m_mask);
    if (r < 0) {
        reply.m_status = Emu::map_errno(r);
    } else {
        reply.m_status = Qnx::QEOK;
    }
    i.msg().write_type(0, &reply);
}

void MainHandler::proc_sigmask(MsgInfo &i) {
    QnxMsg::proc::signal_request msg;
    i.msg().read_type(&msg);

    QnxMsg::proc::signal_reply reply;
    memset(&reply, 0, sizeof(reply));
    auto emu = &i.ctx().proc()->m_emu;
    reply.m_old_bits = emu->signal_getmask();
    emu->signal_mask(msg.m_mask, msg.m_bits);
    reply.m_new_bits = emu->signal_getmask();
    i.msg().write_type(0, &reply);
}

void MainHandler::proc_getid(MsgInfo &i) {
    auto proc = Process::current();
    QnxMsg::proc::getid_reply msg;
    memset(&msg, 0, sizeof(msg));
    //msg.m_status = Qnx::QEOK;
    msg.m_pid = proc->pid();
    msg.m_ppid = proc->parent_pid();
    msg.m_pid_group = proc->pid();
    msg.m_rgid = getgid();
    msg.m_egid = getegid();
    msg.m_ruid = getuid();
    msg.m_euid = geteuid();
    i.msg().write_type(0, &msg);
}

void MainHandler::proc_osinfo(MsgInfo &i) {
    auto proc = Process::current();
    Qnx::osinfo osinfo;
    memset(&osinfo, 0, sizeof(osinfo));
    osinfo.cpu = 486;
    osinfo.fpu = 487;
    osinfo.cpu_speed = 960;
    osinfo.nodename = proc->nid();

    osinfo.ssinfo_sel = SegmentDescriptor::mk_invalid_sel(0);
    osinfo.disksel = SegmentDescriptor::mk_invalid_sel(1);
    osinfo.machinesel = SegmentDescriptor::mk_invalid_sel(2);
    osinfo.timesel = SegmentDescriptor::mk_sel(proc->m_time_segment_selector);

    strcpy(osinfo.machine, "Qine");
    i.msg().write_status(Qnx::QEOK);
    i.msg().write_type(2, &osinfo);
}

void MainHandler::proc_fork(MsgInfo &i) {
    QnxMsg::proc::loaded_reply reply;
    clear(&reply);

    pid_t r = fork();
    if (r < 0) {
        reply.m_status = Emu::map_errno(errno);
    } else {
        if (r == 0) {
            reply.m_son_pid = 0;
        } else {
            reply.m_son_pid = Process::child_pid();
        }
        reply.m_status = Qnx::QEOK;
    }
    i.msg().write_type(0, &reply);
}

void MainHandler::proc_spawn(MsgInfo &i) {
    QnxMsg::proc::loaded_reply reply;
    clear(&reply);

    pid_t r = fork();
    if (r < 0 ) {
        reply.m_status = Emu::map_errno(errno);
    } else if (r == 0) {
        proc_exec_common(i);
        exit(255);
    } else {
        reply.m_son_pid = Process::child_pid();
        reply.m_status = Qnx::QEOK;
    }
    i.msg().write_type(0, &reply);
}

void MainHandler::proc_exec(MsgInfo &i) {
    QnxMsg::proc::loaded_reply reply;
    clear(&reply);

    proc_exec_common(i);
    reply.m_status = Emu::map_errno(errno);
    i.msg().write_type(0, &reply);
}

void MainHandler::proc_exec_common(MsgInfo &i) {
    QnxMsg::proc::spawn msg;
    i.msg().read_type(&msg);
    MsgStreamReader r(&i.msg(), sizeof(QnxMsg::proc::spawn));

    // copy into an owned buf and remember offsets
    std::vector<char> buf;
    std::vector<size_t > argvo;
    std::vector<size_t > envo;

    auto read_string = [&buf, &r]() {
        size_t len = 0;
        char c = r.get(); 
        while (c) {
            buf.push_back(c);
            len++;
            c = r.get();
        }
        buf.push_back(0);
        return len;
    };

    size_t len;

    size_t exec_path_o = buf.size();
    len = read_string();

    argvo.push_back(buf.size());
    len = read_string();
    while (len != 0) {
        argvo.push_back(buf.size());
        len = read_string();
    }

    envo.push_back(buf.size());
    len = read_string();
    while (len != 0) {
        envo.push_back(buf.size());
        len = read_string();
    }

    // now that the buf is ready, convert offsets into pointers
    std::vector<const char*> argvp;
    std::vector<const char*> envp;
    for (auto o: argvo) {
        argvp.push_back(&buf[o]);
    }
    for (auto o: envo) {
        envp.push_back(&buf[o]);
    }
    argvp.erase(argvp.end() - 1);
    envp.erase(envp.end() - 1);

    // remove prefix, doe not handle malformed ones
    // TODO: pass argv[0] and exec path to qine separately
    const char *f = &buf[exec_path_o];
    if (f[0] == '/' && f[1] == '/') {
        f += 2;
        while (isalnum(f[0]))
            f++;
    }
    argvp[0] = f;

    // combine qine args and our args
    std::vector<const char*> final_argv;
    for (const auto& a: i.ctx().proc()->self_call()) {
        final_argv.push_back(a.c_str());
    }

    //final_argv.push_back("--");
    
    for (auto p: argvp) {
        final_argv.push_back(p);
    }
    final_argv.push_back(nullptr);

    // fixup envp
    const char *qnx_root = getenv("QNX_ROOT");
    std::string root_env;
    if (qnx_root) {
        root_env = "QNX_ROOT=";
        root_env.append(qnx_root);
        envp.push_back(root_env.c_str());
    }
    envp.push_back(nullptr);

     for (auto o: final_argv) {
        //printf("Arg: %s\n", o);
    }
    for (auto o: envp) {
        //printf("Env %s\n", o);
    }

    //printf("Exec: %s\n", f);
    execve(final_argv[0], const_cast<char**>(final_argv.data()), const_cast<char**>(envp.data()));
    exit(255);
}

void MainHandler::proc_wait(MsgInfo &i) {
    QnxMsg::proc::wait_request msg;
    i.msg().read_type(&msg);

    QnxMsg::proc::wait_reply reply;
    clear(&reply);
    // TODO: once we get PID remap, call the real deal
    int status;
    pid_t pid = wait(&status);

    if (pid < 0) {
        reply.m_status = Emu::map_errno(errno);
    } else {
        reply.m_status = Qnx::QEOK;
        reply.m_xstatus = status;
        reply.m_pid = Process::child_pid();
    }
    i.msg().write_type(0, &reply);
}

uint32_t MainHandler::map_file_flags_to_host(uint32_t oflag) {
    uint32_t mapped_oflags = 0;
    if (oflag & Qnx::QO_WRONLY)
        mapped_oflags |= O_WRONLY;
    
    if (oflag & Qnx::QO_RDWR)
        mapped_oflags |= O_RDWR;

    if (oflag & Qnx::QO_APPEND)
        mapped_oflags |= O_APPEND;

    if (oflag & Qnx::QO_CREAT)
        mapped_oflags |= O_CREAT;
    
    if (oflag & Qnx::QO_TRUNC)
        mapped_oflags |= O_TRUNC;

    if (oflag & Qnx::QO_NOCTTY)
        mapped_oflags |= O_NOCTTY;
    
    if (oflag & Qnx::QO_EXCL)
        mapped_oflags |= O_EXCL;

    if (oflag & Qnx::QO_SYNC)
        mapped_oflags |= O_SYNC;

    if (oflag & Qnx::QO_DSYNC)
        mapped_oflags |= O_DSYNC;
    return mapped_oflags;
}

void MainHandler::io_open(MsgInfo& i) {
    QnxMsg::common::open msg;
    i.msg().read_type(&msg);

    int mapped_oflags = O_RDONLY;
    int oflag = msg.m_oflag;
    int eflags = msg.m_eflag;
    if (msg.m_type == QnxMsg::io::msg_io_open::TYPE) {
        mapped_oflags = map_file_flags_to_host(oflag);
    } else if (msg.m_type == QnxMsg::io::msg_handle::TYPE) {
        if (msg.m_oflag == Qnx::IO_HNDL_RDDIR) {
            mapped_oflags |= O_DIRECTORY;
        } else {
            mapped_oflags |= O_PATH;
        }
    }
    
    // TODO: handle cloexec and flags
    int fd = ::open(msg.m_path, mapped_oflags, msg.m_mode);
    if (fd < 0) {
        i.msg().write_status(Emu::map_errno(errno));
        return;
    } else {
        msg.m_type = 0;
    }

    if (fd != msg.m_fd) {
        // move fd to final location
        int r = dup2(fd, msg.m_fd);
        if (r < 0) {
            i.msg().write_status(Emu::map_errno(errno));
            return;
        }
        close(fd);
    }

    i.msg().write_status(Qnx::QEOK);
}

void MainHandler::io_stat(MsgInfo& i) {
    QnxMsg::common::open msg;
    i.msg().read_type(&msg);
    struct stat sb;

    QnxMsg::io::stat_reply reply;
    memset(&reply, 0, sizeof(reply));
    int r = stat(msg.m_path, &sb);
    if (r < 0) {
        reply.m_status = Emu::map_errno(errno);
        i.msg().write_type(0, &reply);
        return;
    }

    transfer_stat(reply.m_stat, sb);
    i.msg().write_type(0, &reply);
}

void MainHandler::io_fstat(MsgInfo& i) {
    QnxMsg::io::fstat_request msg;
    i.msg().read_type(&msg);
    struct stat sb;

    QnxMsg::io::fstat_reply reply;
    memset(&reply, 0, sizeof(reply));
    int r = fstat(msg.m_fd, &sb);
    if (r < 0) {
        reply.m_status = Emu::map_errno(errno);
        i.msg().write_type(0, &reply);
        return;
    }

    transfer_stat(reply.m_stat, sb);
    i.msg().write_type(0, &reply);
}

void MainHandler::io_readdir(MsgInfo &i)
{
    QnxMsg::io::readdir_request msg;
    i.msg().read_type(&msg);

    auto fd = i.ctx().proc()->fd_get(msg.m_fd);
    if (!fd->m_dir) {
        fd->m_dir = fdopendir(msg.m_fd);
        if (!fd->m_dir) {
            i.msg().write_status(Emu::map_errno(errno));
            return;
        }
    }

    constexpr size_t dirent_size = sizeof(QnxMsg::io::stat) + Qnx::QNAME_MAX_T;

    // for each dirent, empty stat with (st_status & _FILE_USED) == 0
    QnxMsg::io::stat stat;
    memset(&stat, 0, sizeof(stat));

    char path_buf[Qnx::QNAME_MAX_T];

    QnxMsg::io::readdir_reply reply;
    memset(&reply, 0x00, sizeof(reply));

    //i.msg().dump_structure(stdout);
    errno = 0;
    size_t dst_off = sizeof(reply);
    for (int di = 0; di < msg.m_ndirs; di++) {
        struct dirent *d = readdir(fd->m_dir);
        if (!d) {
            if (errno == 0) {
                
                break;
            } else {
                reply.m_status = Emu::map_errno(errno);
                return;
            }
        }

        // write the dirent (stat + path)
        strlcpy(path_buf, d->d_name, sizeof(path_buf));
        i.msg().write_type(dst_off, &stat);
        i.msg().write(dst_off + sizeof(stat), path_buf, sizeof(path_buf));
        dst_off += dirent_size;
        reply.m_ndirs++;
    }

    reply.m_status = Qnx::QEOK;
    i.msg().write_type(0, &reply);
}
void MainHandler::io_rewinddir(MsgInfo &i) {
    QnxMsg::io::readdir_request msg;
    i.msg().read_type(&msg);
    auto fd = i.ctx().proc()->fd_get(msg.m_fd);
    if (fd->m_dir) {
        rewinddir(fd->m_dir);
    }
    i.msg().write_status(Qnx::QEOK);
}

void MainHandler::transfer_stat(QnxMsg::io::stat& dst, struct stat& src) {
    dst.m_ino = src.st_ino;
    dst.m_dev = src.st_dev;
    dst.m_size = src.st_size;
    dst.m_rdev = src.st_rdev;
    dst.m_ftime = 0;
    dst.m_mtime = src.st_mtim.tv_sec;
    dst.m_atime = src.st_atim.tv_sec;
    dst.m_ctime = src.st_ctim.tv_sec;
    dst.m_mode = src.st_mode;
    dst.m_uid = src.st_uid;
    dst.m_gid = src.st_gid;
    dst.m_nlink = src.st_nlink;
    dst.m_status = 0;
}

void MainHandler::io_close(MsgInfo& i) {
    QnxMsg::io::close_request msg;
    i.msg().read_type(&msg);

    auto fdi = i.ctx().proc()->fd_get(msg.m_fd);

    int r;
    if (fdi->m_dir) {
        r = closedir(fdi->m_dir);
    } else {
        r = close(msg.m_fd);
    }

    i.ctx().proc()->fd_release(msg.m_fd);
    if (r < 0) {
        i.msg().write_status(Emu::map_errno(errno));
    } else {
        i.msg().write_status(0);
    }
}

void MainHandler::io_lseek(MsgInfo &i) {
    QnxMsg::io::lseek_request msg;
    i.msg().read_type(&msg);

    QnxMsg::io::lseek_reply reply;
    memset(&reply, 0, sizeof(reply));

    // TODO: handle overflow
    off_t off = lseek(msg.m_fd, msg.m_offset, msg.m_whence);
    if (off == -1) {
        reply.m_status = Emu::map_errno(off);
        reply.m_zero = 0;
        reply.m_offset = -1;
    } else {
        reply.m_status = Qnx::QEOK;
        reply.m_zero = 0;
        reply.m_offset = off;
    }
    i.msg().write_type(0, &reply);
}

void MainHandler::io_read(MsgInfo &i) {
    QnxMsg::io::read_request msg;
    i.msg().read_type(&msg);

    std::vector<struct iovec> iov;
    i.msg().write_iovec(sizeof(msg), msg.m_nbytes, iov);

    QnxMsg::io::read_reply reply;
    int r = readv(msg.m_fd, iov.data(), iov.size());
    reply.m_zero = 0;
    if (r < 0) {
        reply.m_status = errno;
        reply.m_nbytes = 0;
    } else {
        reply.m_status = Qnx::QEOK;
        reply.m_nbytes = r;
    }
    i.msg().write_type(0, &reply);
}

void MainHandler::io_write(MsgInfo &i) {
    QnxMsg::io::write_request msg;
    i.msg().read_type(&msg);

    std::vector<struct iovec> iov;
    i.msg().read_iovec(sizeof(msg), msg.m_nbytes, iov);

    QnxMsg::io::write_reply reply;
    int r = writev(msg.m_fd, iov.data(), iov.size());
    reply.m_zero = 0;
    if (r < 0) {
        reply.m_status = errno;
        reply.m_nbytes = 0;
    } else {
        reply.m_status = Qnx::QEOK;
        reply.m_nbytes = r;
    }
    i.msg().write_type(0, &reply);
}

void MainHandler::io_fcntl_flags(MsgInfo &i) {
    QnxMsg::io::fcntl_flags_request msg;
    i.msg().read_type(&msg);

    QnxMsg::io::fcntl_flags_reply reply;
    memset(&msg, 0, sizeof(reply));

    int r = fcntl(msg.m_fd, F_GETFL);
    if (r < 0) {
        reply.m_status = Emu::map_errno(errno);
        i.msg().write_type(0, &reply);
        return;
    }

    if (msg.m_mask != 0) {
        // update flags
        uint32_t mask_flags = map_file_flags_to_host(msg.m_mask);
        uint32_t set_flags = map_file_flags_to_host(msg.m_bits);

        r = (r & ~mask_flags) | set_flags;

        r = fcntl(msg.m_fd, F_SETFL);
        if (r < 0) {
            reply.m_status = Emu::map_errno(errno);
            i.msg().write_type(0, &reply);
            return;
        }

        int r = fcntl(msg.m_fd, F_GETFL);
        if (r < 0) {
            reply.m_status = Emu::map_errno(errno);
            i.msg().write_type(0, &reply);
            return;
        }
    }

    reply.m_status = Qnx::QEOK;
    reply.m_flags = 0;

    if (r & O_WRONLY)
        reply.m_flags |= Qnx::QO_WRONLY;
    
    if (r & O_RDWR)
        reply.m_flags |= Qnx::QO_RDWR;

    if (r & O_APPEND)
        reply.m_flags |= Qnx::QO_APPEND;

    if (r & O_CREAT)
        reply.m_flags |= Qnx::QO_CREAT;
    
    if (r & O_TRUNC)
        reply.m_flags |= Qnx::QO_TRUNC;

    if (r & O_NOCTTY)
        reply.m_flags |= Qnx::QO_NOCTTY;
    
    if (r & O_EXCL)
        reply.m_flags |= Qnx::QO_EXCL;

    if (r & O_SYNC)
        reply.m_flags |= Qnx::QO_SYNC;

    if (r & O_DSYNC)
        reply.m_flags |= Qnx::QO_DSYNC;

    i.msg().write_type(0, &reply);
}

void MainHandler::io_dup(MsgInfo &i) {
    QnxMsg::io::dup_request msg;
    i.msg().read_type(&msg);

    int r = dup2(msg.m_src_fd, msg.m_dst_fd);
    if (r < 0) {
        i.msg().write_status(Emu::map_errno(errno));
    } else {
        i.msg().write_status(Qnx::QEOK);
    }
}

void MainHandler::fsys_unlink(MsgInfo &i) {
    QnxMsg::common::open msg;
    i.msg().read_type(&msg);

    int r = unlink(msg.m_path);
    i.msg().write_status(Emu::map_errno(r));
}

void MainHandler::dev_tcgetattr(MsgInfo &i) {
    QnxMsg::dev::tcgetattr_request msg;
    i.msg().read_type(&msg);

    QnxMsg::dev::tcgetattr_reply reply;
    memset(&reply, 0, sizeof(reply));

    struct termios attr;
    int r = tcgetattr(i.m_fd, &attr);
    if (r < 0) {
        reply.m_status = Emu::map_errno(errno);
        i.msg().write_type(0, &reply);
        return;
    }

    // TODO: do a real translate
    reply.m_status = Qnx::QEOK;
    for(size_t i = 0; i < std::min(NCCS, Qnx::QNCCS); i++) {
        reply.m_state.m_c_cc[i] = attr.c_cc[i];
    }
    reply.m_state.m_c_ispeed = attr.c_ispeed;
    reply.m_state.m_c_ospeed = attr.c_ospeed;
    reply.m_state.m_c_line = attr.c_line;
    reply.m_state.m_cflag = attr.c_cflag;
    reply.m_state.m_iflag = attr.c_iflag;
    reply.m_state.m_lflag = attr.c_lflag;
    reply.m_state.m_oflag = attr.c_oflag;

    i.msg().write_type(0, &reply);
}

void MainHandler::dev_tcsetattr(MsgInfo &i) {
    QnxMsg::dev::tcsetattr_request msg;
    i.msg().read_type(&msg);

    struct termios attr;

    // TODO: do a real translate

    for(size_t i = 0; i < std::min(NCCS, Qnx::QNCCS); i++) {
        attr.c_cc[i] = msg.m_state.m_c_cc[i];
    }
    attr.c_ispeed = msg.m_state.m_c_ispeed;
    attr.c_ospeed = msg.m_state.m_c_ospeed;
    attr.c_line = msg.m_state.m_c_line;
    attr.c_cflag = msg.m_state.m_cflag;
    attr.c_iflag = msg.m_state.m_iflag;
    attr.c_lflag = msg.m_state.m_lflag;
    attr.c_oflag = msg.m_state.m_oflag;

    //int r = tcsetattr(i.m_fd, msg.m_optional_actions, &attr);
    int r = 0;
    if (r < 0) {
        i.msg().write_status(Emu::map_errno(Qnx::QEOK));
    } else {
        i.msg().write_status(Emu::map_errno(Qnx::QEOK));
    }   
}

void MainHandler::dev_term_size(MsgInfo &i) {
    QnxMsg::dev::term_size_request msg;
    i.msg().read_type(&msg);
    constexpr uint16_t no_change = std::numeric_limits<uint16_t>::max();

    winsize term_winsize;
    int r = ioctl(msg.m_fd, TIOCGWINSZ, &term_winsize);
    if (r < 0) {
        i.msg().write_status(Emu::map_errno(r));
    }

    QnxMsg::dev::term_size_reply reply;
    clear(&reply);
    reply.m_oldcols = msg.m_cols;
    reply.m_oldrows = msg.m_rows;

    if (msg.m_cols != no_change || msg.m_rows != no_change) {
        if (msg.m_cols != no_change)
            term_winsize.ws_col = msg.m_cols;
        if (msg.m_rows != no_change)
            term_winsize.ws_row = msg.m_rows;

        r = ioctl(msg.m_fd, TIOCSWINSZ, &term_winsize);
        if (r < 0) {
            i.msg().write_status(Emu::map_errno(r));
        }
    }

    reply.m_status = Qnx::QEOK;
    i.msg().write_type(0, &reply);
}