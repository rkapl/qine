#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <limits>
#include <string.h>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ucontext.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <vector>
#include <mntent.h>

#include "fd_filter.h"
#include "fsutil.h"
#include "gen_msg/common.h"
#include "gen_msg/dev.h"
#include "gen_msg/fsys.h"
#include "gen_msg/proc.h"
#include "gen_msg/io.h"
#include "mem_ops.h"
#include "msg.h"
#include "process.h"
#include "msg_handler.h"
#include "main_handler.h"
#include "qnx/errno.h"
#include "qnx/io.h"
#include "qnx/lock.h"
#include "qnx/msg.h"
#include "qnx/osinfo.h"
#include "qnx/procenv.h"
#include "qnx/psinfo.h"
#include "qnx/segment.h"
#include "qnx/signal.h"
#include "qnx/stat.h"
#include "qnx/timers.h"
#include "qnx/types.h"
#include "qnx/wait.h"
#include "segment_descriptor.h"
#include "types.h"
#include "log.h"
#include "qnx/pathconf.h"
#include "unique_fd.h"
#include "unique_file.h"
#include "util.h"


void MainHandler::receive(MsgContext& i) {
    try {
        receive_inner(i);
    } catch(const BadFdException&) {
        i.msg().write_status(Qnx::QEBADF);
    }
}

void MainHandler::receive_inner(MsgContext& i) {
    Qnx::MsgHeader hdr;
    i.msg().read_type(&hdr);

    auto unhandled_msg = [&hdr, &i] {
        Log::print(Log::UNHANDLED, "Unhandled message %xh:%xh\n", hdr.type, hdr.subtype);
        i.msg().write_status(Qnx::QENOSYS);
    };

    switch (hdr.type) {
        case QnxMsg::proc::msg_slib_register::TYPE:
            proc_slib_register(i);
        break;
        case QnxMsg::proc::msg_segment_realloc::TYPE:
            switch (hdr.subtype) {
                case QnxMsg::proc::msg_segment_realloc::SUBTYPE:
                    proc_segment_realloc(i);
                break;
                case QnxMsg::proc::msg_segment_alloc::SUBTYPE:
                    proc_segment_alloc(i);
                break;
                case QnxMsg::proc::msg_segment_free::SUBTYPE:
                    proc_segment_free(i);
                break;
                case QnxMsg::proc::msg_segment_flags::SUBTYPE:
                    proc_segment_flags(i);
                break;
                case QnxMsg::proc::msg_segment_put::SUBTYPE:
                    proc_segment_put(i);
                    break;
                case QnxMsg::proc::msg_segment_arm::SUBTYPE:
                    proc_segment_arm(i);
                    break;
                case QnxMsg::proc::msg_segment_priv::SUBTYPE:
                    proc_segment_priv(i);
                    break;
                default:
                    unhandled_msg();
                break;
            }
            break;
        case QnxMsg::proc::msg_name_attach::TYPE:
            switch (hdr.subtype) {
                case QnxMsg::proc::msg_name_attach::SUBTYPE:
                    proc_name_attach(i);
                break;
                default:
                    unhandled_msg();
                break;
            }; break;
        case QnxMsg::proc::msg_time::TYPE:
            proc_time(i);
            break;
        case QnxMsg::proc::msg_setpgid::TYPE:
            proc_setpgid(i);
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
                    proc_fd_setfd(i);
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
                case QnxMsg::proc::msg_sigraise::SUBTYPE:
                    // used by 16-bit code
                    proc_sigraise(i);
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
        case QnxMsg::proc::msg_timer_create::TYPE:
            switch (hdr.subtype) {
            case QnxMsg::proc::msg_timer_create::SUBTYPE:
                proc_timer_create(i);
            break;
            case QnxMsg::proc::msg_timer_settime::SUBTYPE:
                proc_timer_settime(i);
            break;
            case QnxMsg::proc::msg_timer_alarm::SUBTYPE:
                proc_timer_alarm(i);
            break;
            default:
                unhandled_msg();
            break;
            }break;
        case QnxMsg::proc::msg_wait::TYPE:
            proc_wait(i);
            break;
        case QnxMsg::proc::msg_prefix::TYPE:
            proc_prefix(i);
            break;
        case QnxMsg::proc::msg_sid_query::TYPE:
            switch (hdr.subtype) {
            case QnxMsg::proc::msg_sid_query::SUBTYPE:
                proc_sid_query(i);
                break;
            case QnxMsg::proc::msg_sid_set::SUBTYPE:
                proc_sid_set(i);
                break;
            default:
                unhandled_msg();
        }; break;
        case QnxMsg::proc::msg_sem_init::TYPE: {
            switch(hdr.subtype) {
                case QnxMsg::proc::msg_sem_init::SUBTYPE:
                    proc_sem_init(i);
                break;
                case QnxMsg::proc::msg_sem_destroy::SUBTYPE:
                    proc_sem_destroy(i);
                break;
            }
        }; break;

        case QnxMsg::io::msg_handle::TYPE:
        case QnxMsg::io::msg_io_open::TYPE:
            io_open(i);
            break;
        case QnxMsg::io::msg_chdir::TYPE:
            io_chdir(i);
            break;
        case QnxMsg::io::msg_rename::TYPE:
            io_rename(i);
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
        case QnxMsg::io::msg_fpathconf::TYPE:
            io_fpathconf(i);
            break;
        case QnxMsg::io::msg_chmod::TYPE:
            io_chmod(i);
            break;
        case QnxMsg::io::msg_chown::TYPE:
            io_chown(i);
            break;
        case QnxMsg::io::msg_utime::TYPE:
            io_utime(i);
            break;
        case QnxMsg::io::msg_ioctl::TYPE:
            io_ioctl(i);
            break;
        case QnxMsg::io::msg_qioctl::TYPE:
            io_qioctl(i);
            break;
        case QnxMsg::io::msg_lock::TYPE:
            io_lock(i);
            break;

        case QnxMsg::fsys::msg_unlink::TYPE:
            fsys_unlink(i);
            break;
        case QnxMsg::fsys::msg_mkspecial::TYPE:
            fsys_mkspecial(i);
            break;
        case QnxMsg::fsys::msg_link::TYPE:
            fsys_link(i);
            break;
        case QnxMsg::fsys::msg_sync::TYPE:
            fsys_sync(i);
            break;
        case QnxMsg::fsys::msg_readlink::TYPE:
            fsys_readlink(i);
            break;
        case QnxMsg::fsys::msg_trunc::TYPE:
            fsys_trunc(i);
            break;
        case QnxMsg::fsys::msg_fsync::TYPE:
            fsys_fsync(i);
            break;
        case QnxMsg::fsys::msg_pipe::TYPE:
            fsys_pipe(i);
            break;
        case QnxMsg::fsys::msg_disk_entry::TYPE:
            fsys_disk_entry(i);
            break;
        case QnxMsg::fsys::msg_get_mount::TYPE:
            fsys_get_mount(i);
            break;
        case QnxMsg::fsys::msg_disk_space::TYPE:
            fsys_disk_space(i);
            break;

        case QnxMsg::dev::msg_tcgetattr::TYPE:
            dev_tcgetattr(i);
            break;
        case QnxMsg::dev::msg_tcsetattr::TYPE:
            dev_tcsetattr(i);
            break;
        case QnxMsg::dev::msg_dev_info::TYPE:
            dev_info(i);
            break;
        case QnxMsg::dev::msg_dev_fdinfo::TYPE:
            dev_fdinfo(i);
            break;
        case QnxMsg::dev::msg_tcgetpgrp::TYPE:
            dev_tcgetpgrp(i);
            break;
        case QnxMsg::dev::msg_tcsetpgrp::TYPE:
            dev_tcsetpgrp(i);
            break;
        case QnxMsg::dev::msg_term_size::TYPE:
            dev_term_size(i);
            break;
        case QnxMsg::dev::msg_tcdrain::TYPE:
            dev_tcdrain(i);
            break;
        case QnxMsg::dev::msg_tcflush::TYPE:
            dev_tcflush(i);
            break;
        case QnxMsg::dev::msg_read::TYPE:
            dev_read(i);
            break;
        case QnxMsg::dev::msg_insert_chars::TYPE:
            dev_insert_chars(i);
            break;
        case QnxMsg::dev::msg_mode::TYPE:
            dev_mode(i);
            break;
        default:
            unhandled_msg();
            break;
    }
}


void MainHandler::proc_terminate(MsgContext& i)
{
    QnxMsg::proc::terminate_request msg;
    i.msg().read_type(&msg);
    //i.ctx().dump(stdout);
    exit(msg.m_status);
}

void MainHandler::proc_time(MsgContext& i)
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

void MainHandler::proc_setpgid(MsgContext &i) {
    QnxMsg::proc::setpgid_request msg;
    i.msg().read_type(&msg);

    // find the pids and pgids if needed
    pid_t host_pid = 0;
    if (msg.m_pid) {
        auto pid_info = i.proc().pids().qnx_valid_host(msg.m_pid);
        if (pid_info) {
            host_pid = pid_info->host_pid();
        } else {
            i.msg().write_status(Qnx::QESRCH);
            return;
        }
    }

    pid_t host_pgid = 0;
    if (msg.m_pgid) {
        auto pgid_info = i.proc().pids().qnx_valid_host(msg.m_pgid);
        if (pgid_info) {
            host_pgid = pgid_info->host_pid();
        } else {
            i.msg().write_status(Qnx::QESRCH);
            return;
        }
    }

    int r = setpgid(host_pid, host_pgid);
    if (r < 0) {
        i.msg().write_status(Emu::map_errno(errno));
    } else {
        i.msg().write_status(Qnx::QEOK);
    }
}

void MainHandler::proc_fd_attach(MsgContext& i)
{
    QnxMsg::proc::fd_request msg;
    i.msg().read_type(&msg);

    QnxMsg::proc::fd_reply1 reply;
    clear(&reply);

    try {
        auto fd = i.proc().fds().qnx_fd_attach(msg.m_fd, msg.m_nid, msg.m_pid, msg.m_vid, msg.m_flags);

        reply.m_status = 0;
        reply.m_fd = fd->m_fd;
        i.msg().write_type(0, &reply);
    } catch (const NoFreeId&) {
        i.msg().write_status(Qnx::QEMFILE);
    }
}

void MainHandler::proc_fd_detach(MsgContext& i) {
    QnxMsg::proc::fd_request msg;
    i.msg().read_type(&msg);

    if (i.proc().fds().qnx_fd_detach(msg.m_fd)) {
        i.msg().write_status(Qnx::QEOK);
    } else {
        i.msg().write_status(Qnx::QEBADF);
    }
}

void MainHandler::proc_fd_query(MsgContext& i) {
    QnxMsg::proc::fd_request msg;
    i.msg().read_type(&msg);

    auto fd = i.proc().fds().fd_query(msg.m_fd);

    QnxMsg::proc::fd_query_reply reply;
    clear(&reply);
    if (fd) {
        reply.m_status = Qnx::QEOK;
        reply.m_info.m_handle = fd->m_handle;
        reply.m_info.m_pid = fd->m_pid;
        reply.m_info.m_vid = fd->m_vid;
        reply.m_info.m_nid = fd->m_nid;
        /* Not sure what is going on here, there must be some similar work - around in QNX too.
        * There is some weird not about someone "messing up" the bit position by 8 bits...
        * QNX I think does not have process-specific replies, so not sure what they are doing.
        */
        if (i.proc().m_bits == B32) {
            reply.m_info.m_flags = fd->m_flags << 8;
        } else {
            reply.m_info.m_flags = fd->m_flags;
        }
        reply.m_fd = fd->m_fd;
    } else {
        reply.m_status = Qnx::QEINVAL;
    }
    i.msg().write_type(0, &reply);
}

void MainHandler::proc_fd_setfd(MsgContext &i) {
    QnxMsg::proc::fd_request msg;
    i.msg().read_type(&msg);

    auto fd = i.proc().fds().get_attached_fd(msg.m_fd);
    if (fd->m_open) {
        int r = fcntl(fd->m_host_fd, F_SETFD, msg.m_flags ? FD_CLOEXEC : 0);
        if (r < 0) {
            i.msg().write_status(Emu::map_errno(errno));
            return;
        }
    }
    fd->m_flags = msg.m_flags;
    i.msg().write_status(Qnx::QEOK);
}

void MainHandler::proc_slib_register(MsgContext &i) {
    QnxMsg::proc::slib_register_request msg;
    i.msg().read_type(&msg);

    uint32_t offset;
    const char* lib_type;
    if (i.proc().m_bits == B32) {
        offset = msg.m_offset;
        lib_type = "sys";
    } else {
        offset = msg.m_offset_3;
        lib_type = "sys16";
    }

    fprintf(stderr, "Registering shared library %s, offset %x\n", msg.m_data.m_name, offset);
    fprintf(stderr, "You can load the library for your program using:\n  --lib %s,%s,entry=0x%x\n",
        i.proc().m_executed_file.host_path(), lib_type, offset);

    i.msg().write_status(Qnx::QEOK);
}

void MainHandler::proc_segment_alloc(MsgContext& i)
{
    QnxMsg::proc::segment_request msg;
    i.msg().read_type(&msg);
    
    auto seg  = i.ctx().proc()->allocate_segment();
    seg->reserve(MemOps::mega(1));
    auto nbytes = msg.m_nbytes;
    if (i.proc().m_bits == B16) {
        nbytes &= 0x1FFFFu;
    }
    seg->grow_bytes(nbytes);

    // segment inherits Process bitness in case it is a code segment
    auto sd = i.ctx().proc()->create_segment_descriptor(Access::READ_WRITE, seg, i.proc().m_bits);
    
    QnxMsg::proc::segment_reply reply;
    memset(&reply, 0, sizeof(reply));
    reply.m_status = Qnx::QEOK;
    reply.m_sel = sd->selector();
    reply.m_nbytes = seg->size();
    reply.m_addr = reinterpret_cast<uint32_t>(seg->location()); 

    i.msg().write_type(0, &reply);
}

void MainHandler::proc_segment_realloc(MsgContext& i)
{
    QnxMsg::proc::segment_request msg;
    i.msg().read_type(&msg);
    QnxMsg::proc::segment_reply reply;
    clear(&reply);

    auto sd  = i.ctx().proc()->descriptor_by_selector(msg.m_sel);
    if (!sd) {
        reply.m_status = Qnx::QEINVAL;
        i.msg().write_type(0, &reply);
        return;
    }
    auto seg = sd->segment();
    if (seg->is_shared()) {
        reply.m_status = Qnx::QEBUSY;
    } else {
        if (seg->size() < msg.m_nbytes) {
            seg->grow_bytes(msg.m_nbytes - seg->size());
            // TODO: this must be done better, the segment must be aware of its descriptors
            // or have better understanding how it works on QNX
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

void MainHandler::proc_segment_free(MsgContext& i) {
    QnxMsg::proc::segment_request msg;
    i.msg().read_type(&msg);
    QnxMsg::proc::segment_reply reply;
    clear(&reply);

    auto sd  = i.ctx().proc()->descriptor_by_selector(msg.m_sel);
    if (!sd) {
        reply.m_status = Qnx::QEINVAL;
        i.msg().write_type(0, &reply);
        return;
    }

    i.ctx().proc()->free_segment_descriptor(sd);
    i.msg().write_type(0, &reply);
}

void MainHandler::proc_segment_flags(MsgContext &i) {
    QnxMsg::proc::segment_request msg;
    i.msg().read_type(&msg);
    QnxMsg::proc::segment_reply reply;
    clear(&reply);

    auto sd  = i.ctx().proc()->descriptor_by_selector(msg.m_sel);
    if (!sd) {
        reply.m_status = Qnx::QEINVAL;
        i.msg().write_type(0, &reply);
        return;
    }

    // other flags not handled yet
    sd->change_access(static_cast<Access>(msg.m_flags & Qnx::PMF_ACCESS_MASK));
    sd->update_descriptors();
    reply.m_flags = msg.m_flags;
    reply.m_sel = msg.m_flags;
    i.msg().write_type(0, &reply);
}

void MainHandler::proc_segment_arm(MsgContext& i) {
    // mark segment as global, called during Slib startup
    Log::print(Log::UNHANDLED, "segment_arm is a stub\n");
    i.msg().write_status(Qnx::QEOK);
}

void MainHandler::proc_segment_put(MsgContext& i) {
    Log::print(Log::UNHANDLED, "segment_put is a stub\n");
    // mark segment as global, called during Slib startup
    i.msg().write_status(Qnx::QEOK);
}

void MainHandler::proc_segment_priv(MsgContext& i) {
    Log::print(Log::UNHANDLED, "segment_priv is a stub\n");
    // mark segment as global, called during Slib startup
    i.msg().write_status(Qnx::QEOK);
}

void MainHandler::proc_name_attach(MsgContext &i) {
    QnxMsg::proc::name_request msg;
    i.msg().read_type(&msg);

    // Currently used by Slib when registering its shared library
    fprintf(stderr, "Attaching name %s\n", msg.m_name);

    QnxMsg::proc::name_reply reply;
    reply.m_cookie = 1;
    clear(&reply);
    i.msg().write_type(0, &reply);
}

void MainHandler::proc_open(MsgContext& i) {
    /* proc_open does not actually open a file, just resolves the file name*/
    QnxMsg::proc::open_request msg;
    i.msg().read_type(&msg);

    /* and we do not do any resolution, reply back with the same path and our main server */
    msg.m_type = Qnx::QEOK;
    msg.m_open.m_pid = 1;
    msg.m_open.m_nid = i.ctx().proc()->nid();
    i.msg().write_type(0, &msg);
}

void MainHandler::proc_vc_attach(MsgContext& i) 
{
    auto proc = i.ctx().proc();
    QnxMsg::proc::vc vc;
    i.msg().read_type(&vc);

    if (vc.m_nid == 0 || vc.m_nid == proc->nid()) {
        /* Trying to talk to PID 1 or ourselves? That is ok. */
        if (vc.m_pid == proc->pid() || vc.m_pid == 1) {
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


void MainHandler::proc_vc_detach(MsgContext& i) 
{
    /* This is stubbed, because QNX/slib seems to have a bug where the stat result
     * overwrites the NID in the message and it then mistakenly calls vc_detach.*/

     /* Also calls to proc_vc_attach succeed on local PID, and then the caller can try to detach it */
    i.msg().write_status(Qnx::QEOK);
}

void MainHandler::proc_psinfo(MsgContext &i)
{
    QnxMsg::proc::psinfo_request msg;
    i.msg().read_type(&msg);

    if (msg.m_pid == i.proc().pid()) {
        auto proc = &i.proc();
        // known user: wlink tries to find its own exec (via _cmdname   )
        
        // getsid pulls the sid from here
        i.msg().write_status(Qnx::QEOK);
        Qnx::psinfo ps;
        clear(&ps);
        ps.pid = proc->pid();
        auto pgid_info = proc->pids().host(getpgid(0));
        if (pgid_info) {
            ps.pid_group = pgid_info->qnx_pid();
        } else {
            ps.pid_group = QnxPid::PID_UNKNOWN;
        }
        ps.rgid = getgid();
        ps.ruid = getuid();
        ps.egid = getegid();
        ps.euid = geteuid();
        int host_sid = getsid(0);
        auto sid_info = proc->pids().host(host_sid);
        if (sid_info) {
            ps.sid = sid_info->qnx_pid();
        } else {
            ps.sid = QnxPid::PID_UNKNOWN;
        }
        ps.proc.father = proc->parent_pid();

        qine_strlcpy(ps.proc.name, proc->executed_file().qnx_path(), sizeof(ps.proc.name));
        i.msg().write_type(2, &ps);
    } else {
        // STUB
        i.msg().write_status(Qnx::QESRCH);
    }
}

void MainHandler::proc_sigtab(MsgContext &i) {
    auto proc = Process::current();
    QnxMsg::proc::signal_request msg;
    i.msg().read_type(&msg);

    auto sigtabv = proc->translate_segmented(FarPointer(i.ctx().reg_ds(), msg.m_offset), sizeof(Qnx::Sigtab));
    proc->m_sigtab = static_cast<Qnx::Sigtab*>(sigtabv);
    
    i.msg().write_status(Qnx::QEOK);
}

void MainHandler::proc_sigact(MsgContext &i) {
    QnxMsg::proc::signal_request msg;
    i.msg().read_type(&msg);

    QnxMsg::proc::signal_reply reply;
    memset(&reply, 0, sizeof(reply));
    int r = i.ctx().proc()->m_emu.signal_sigact(msg.m_signum, FarPointer(msg.m_segment, msg.m_offset), msg.m_mask);
    if (r < 0) {
        reply.m_status = Emu::map_errno(errno);
    } else {
        reply.m_status = Qnx::QEOK;
    }
    i.msg().write_type(0, &reply);
}

void MainHandler::proc_sigraise(MsgContext &i) {
    QnxMsg::proc::signal_request msg;
    i.msg().read_type(&msg);

    QnxMsg::proc::signal_reply reply;
    clear(&reply);
    i.proc().m_emu.signal_raise(msg.m_signum);
    i.msg().write_type(0, &reply);
}

void MainHandler::proc_sigmask(MsgContext &i) {
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

void MainHandler::proc_getid(MsgContext &i) {
    auto proc = Process::current();
    QnxMsg::proc::getid_reply msg;
    memset(&msg, 0, sizeof(msg));
    //msg.m_status = Qnx::QEOK;
    msg.m_pid = proc->pid();
    msg.m_ppid = proc->parent_pid();
    msg.m_pid_group = QnxPid::PID_UNKNOWN;
    auto pid = i.proc().pids().host(getpgrp());
    if (pid)
        msg.m_pid_group = pid->qnx_pid();

    msg.m_rgid = getgid();
    msg.m_egid = getegid();
    msg.m_ruid = getuid();
    msg.m_euid = geteuid();
    i.msg().write_type(0, &msg);
}

void MainHandler::proc_osinfo(MsgContext &i) {
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

void MainHandler::proc_fork(MsgContext &i) {
    QnxMsg::proc::loaded_reply reply;
    clear(&reply);

    pid_t r = fork();
    if (r < 0) {
        reply.m_status = Emu::map_errno(errno);
    } else {
        if (r == 0) {
            // in child
            i.proc().update_pids_after_fork(getpid());
            reply.m_son_pid = 0;
        } else {
            // in parent
            auto pid =  i.proc().pids().alloc_child_pid(r);
            reply.m_son_pid = pid->qnx_pid();
            //fprintf(stderr, "forked child %d (qnx %d)\n", r, reply.m_son_pid);
        }
        reply.m_status = Qnx::QEOK;
    }
    i.msg().write_type(0, &reply);
}

void MainHandler::proc_spawn(MsgContext &i) {
    QnxMsg::proc::loaded_reply reply;
    clear(&reply);

    pid_t r = fork();
    if (r < 0 ) {
        reply.m_status = Emu::map_errno(errno);
    } else if (r == 0) {
        proc_exec_common(i);
        perror("exec");
        exit(255);
    } else {
        auto pid = i.proc().pids().alloc_child_pid(r);
        reply.m_son_pid = pid->qnx_pid();
        reply.m_status = Qnx::QEOK;
    }
    i.msg().write_type(0, &reply);
}

void MainHandler::proc_exec(MsgContext &i) {
    QnxMsg::proc::loaded_reply reply;
    clear(&reply);

    proc_exec_common(i);
    reply.m_status = Emu::map_errno(errno);
    i.msg().write_type(0, &reply);
}

void MainHandler::proc_exec_common(MsgContext &i) {
    QnxMsg::proc::spawn msg;
    i.msg().read_type(&msg);
    MsgStreamReader r(&i.msg(), sizeof(QnxMsg::proc::spawn));

    // copy the arguments from the spawn message into an owned buf and remember offsets into the buf
    std::vector<char> buf;
    std::vector<size_t > argvo;
    std::vector<size_t > envo;

    auto read_string = [&buf, &r]() {
        char c = r.get(); 
        while (c) {
            buf.push_back(c);
            c = r.get();
        }
        buf.push_back(0);
    };

    size_t exec_path_o = buf.size();
    read_string();
    
    for (int i = 0; i < msg.m_argc; i++) {
        argvo.push_back(buf.size());
        read_string();
    }
    // skip the separating null string
    read_string();

    for (int i = 0; i < msg.m_envc; i++) {
        envo.push_back(buf.size());
        read_string();
    }
    read_string();

    // now that the buf is ready and will not be moved, convert offsets into pointers

    std::vector<const char*> argvp;
    std::vector<const char*> envp;
    for (auto o: argvo) {
        argvp.push_back(&buf[o]);
    }
    for (auto o: envo) {
        envp.push_back(&buf[o]);
    }

    // remove prefix, does not handle malformed ones
    // TODO: pass argv[0] and exec path to qine separately
    const char *exec_path = &buf[exec_path_o];
    if (exec_path[0] == '/' && exec_path[1] == '/') {
        exec_path += 2;
        while (isalnum(exec_path[0]))
            exec_path++;
    }

    auto mapped_exec = i.proc().path_mapper().map_path_to_host(exec_path);
    const char* final_exec;

    // Theoretically, we should also check the interpreter, format etc. and report any problem
    // before really exec'ing into qine. But this at least let's path work.
    if (access(mapped_exec.host_path(), X_OK) < 0) {
        return;
    }

    std::vector<const char*> final_argv;
    if (mapped_exec.exec_type() == PathMapper::Exec::HOST) {
        final_exec = mapped_exec.host_path();
        // just copy argv
        final_argv = argvp;

        // filter out __CWD and chdir there
        for (auto it = envp.begin(); it != envp.end(); ++it) {
            auto envvar = std::string_view(*it);
            auto cwd_prefix = std::string_view("__CWD=");
            if (starts_with(envvar, "__CWD=")) {
                std::string cwd(envvar.substr(cwd_prefix.size()));
                auto mapped = i.proc().path_mapper().map_path_to_host(cwd.c_str());
                chdir(mapped.host_path());
                it = envp.erase(it);
                if (it == envp.end())
                    break;
            }
        }
    } else {
        // combine qine args and our args
        for (const auto& a: i.ctx().proc()->self_call()) {
            final_argv.push_back(a.c_str());
        }
        final_argv.push_back("--exec");
        final_argv.push_back(mapped_exec.host_path());
        final_argv.push_back("--");

        for (auto p: argvp) {
            final_argv.push_back(p);
        }

        // call qine
        final_exec = final_argv[0];
    }

    envp.push_back(nullptr);
    final_argv.push_back(nullptr);

    for (auto o: final_argv) {
        // printf("Arg: %s\n", o);
    }
    for (auto o: envp) {
         // printf("Env %s\n", o);
    }
    // printf("About to exec: %s\n", final_exec);

    // redirect file descriptors
    for (size_t fdi = 0; fdi < 10; fdi++) {
        uint8_t fd = msg.m_stdfds[fdi];
        if (fd == 0xFF)
            continue;
        
        // ASSSUME: fd mapping is identical
        if (fd != fdi)
            dup2(i.map_fd(fd), fdi);

        // disable cloexec
        fcntl(fdi, F_SETFD, 0);
    }

    execve(final_exec, const_cast<char**>(final_argv.data()), const_cast<char**>(envp.data()));
}

void MainHandler::proc_timer_create(MsgContext &i) {
    QnxMsg::proc::timer_request msg;
    i.msg().read_type(&msg);

    /* proper timers are not supproted yet, only as simple sleep implementation (no proxies, no signals)*/
    #if 0
    struct sigevent sev;
    sev.sigev_notify = SIGEV_NONE;
    switch (msg.m_arg.m_notify_type) {
        #if 0
        case Qnx::QTNOTIFY_SIGNAL:
            sev.sigev_notify = SIGEV_SIGNAL;
            sev.sigev_signo = Emu::map_sig_qnx_to_host(msg.m_arg.m_data);
            sev.sigev_value.sival_int = msg.m_arg.m_cookie;
            break;
        #endif
        case Qnx::QTNOTIFY_SLEEP:
            sev.sigev_notify = SIGEV_NONE;
        break;
        default:
            Log::print(Log::UNHANDLED, "timer with notififcation != sleep\n");
            i.msg().write_status(Qnx::QENOTSUP);
            return;
    }

    timer_t out_timer;
    printf("Got timer %p\n", out_timer);
    int r = timer_create(CLOCK_REALTIME, &sev, &out_timer);
    if (r < 0) {
        i.msg().write_status(Emu::map_errno(errno));
    } else {
    #endif
    QnxMsg::proc::timer_reply reply;
    clear(&reply);
    
    reply.m_status = Qnx::QEOK;
    /* We hope that we will be able to pass the timer directly to QNX*/
    /* Until then, we will need to create a special map. We may need to do it anyway to support the special flags.*/
    reply.m_arg.m_cookie = 3;
    i.msg().write_type(0, &reply);
}

void MainHandler::proc_timer_settime(MsgContext &i) {
    QnxMsg::proc::timer_request msg;
    i.msg().read_type(&msg);

    struct itimerspec tv = {
        .it_interval = {
            .tv_sec = msg.m_arg.m_sec1,
            .tv_nsec = msg.m_arg.m_nsec1,
        },
        .it_value = {
            .tv_sec = msg.m_arg.m_sec2,
            .tv_nsec = msg.m_arg.m_nsec2,
        }
    };

    int host_flags = 0;
    if (msg.m_arg.m_flags & Qnx::QTIMER_ABSTIME) {
        host_flags |= TIMER_ABSTIME;
    }

    if (msg.m_arg.m_flags & (Qnx::QTIMER_PRESERVE_EXEC | Qnx::QTIMER_ADDREL)) {
        Log::print(Log::UNHANDLED, "timer with unsupported flags\n");
        i.msg().write_status(Qnx::QENOTSUP);
        return;
    }

    // only notify sleep supported for now
    struct timespec rem;
    QnxMsg::proc::timer_reply reply;
    clear(&reply);

    int r = clock_nanosleep(CLOCK_REALTIME, host_flags, &tv.it_interval, &rem);
    if (r != 0) {
        reply.m_status = Emu::map_errno(errno);
    }
    reply.m_arg.m_nsec1 = rem.tv_sec;
    reply.m_arg.m_sec1 = rem.tv_nsec;
    i.msg().write_type(0, &reply);
}

void MainHandler::proc_timer_alarm(MsgContext &i) {
    QnxMsg::proc::timer_request msg;
    i.msg().read_type(&msg);

    if (msg.m_arg.m_notify_type != Qnx::QTNOTIFY_SIGNAL || msg.m_arg.m_data != Qnx::QSIGALRM) {
        Log::print(Log::UNHANDLED, "alarm with other settings than QSIGALARM\n");
        i.msg().write_status(Qnx::QENOTSUP);
        return;
    }

    int r = alarm(msg.m_arg.m_sec1);

    QnxMsg::proc::timer_reply reply;
    clear(&reply);
    reply.m_arg = msg.m_arg;
    reply.m_status = Qnx::QEOK;
    reply.m_arg.m_sec1 = r;
    i.msg().write_type(0, &reply);
}

void MainHandler::proc_wait(MsgContext &i) {
    QnxMsg::proc::wait_request msg;
    i.msg().read_type(&msg);

    QnxMsg::proc::wait_reply reply;
    clear(&reply);
    
    // transform the wait code to host code if needed
    int wait_code;
    if (msg.m_pid == -1 || msg.m_pid == 0) {
        wait_code = msg.m_pid;
    } else if (msg.m_pid > 0) {
        auto pid = i.proc().pids().qnx_valid_host(msg.m_pid);
        if (!pid) {
            i.msg().write_status(Qnx::QECHILD);
            return;
        } else {
            wait_code = pid->host_pid();
        }

    } else if (msg.m_pid < -1) {
        auto pid = i.proc().pids().qnx_valid_host(-msg.m_pid);
        if (!pid) {
            i.msg().write_status(Qnx::QECHILD);
            return;
        }
        wait_code = -pid->host_pid();
    }

    int wait_options = 0;
    if (msg.m_options & Qnx::QWNOHANG)
        wait_options |= WNOHANG;

    int host_status;
    //fprintf(stderr, "%d: start waitpid(%d)\n", getpid(), wait_code);
    pid_t child_host_pid = waitpid(wait_code, &host_status, wait_options);

    if (child_host_pid < 0) {
        //fprintf(stderr, "%d: waitpid %s\n", getpid(), strerror(errno));
        reply.m_status = Emu::map_errno(errno);
        reply.m_pid = -1;
    } else if (child_host_pid == 0) {
        //fprintf(stderr, "%d: waitpid = 0\n", getpid());
        reply.m_status = Qnx::QEOK;
        reply.m_pid = 0;
    } else {
        auto child_pid = i.proc().pids().host(child_host_pid);
        if (child_pid) {
            reply.m_pid = child_pid->qnx_pid();
            i.proc().pids().free_pid(child_pid);
        } else {
            reply.m_pid = QnxPid::PID_UNKNOWN;
        }
        reply.m_status = Qnx::QEOK;
        // QNX status has the form (0xEESS) (EE is error status, SS is signal code)
        // Stop signalling is not supported
        if (WIFSIGNALED(host_status)) {
            reply.m_xstatus = QnxSigset::map_sig_host_to_qnx(WTERMSIG(host_status));
        } else if (WIFSTOPPED(host_status)) {
            reply.m_xstatus = QnxSigset::map_sig_host_to_qnx(WSTOPSIG(host_status));
        } else {
            reply.m_xstatus = WEXITSTATUS(host_status) << 8;
        }
        //fprintf(stderr, "%d: waitpid = %d (qnx %d), status %x\n", getpid(), child_host_pid, reply.m_pid, reply.m_xstatus);
    }
    i.msg().write_type(0, &reply);
}

void MainHandler::proc_sem_init(MsgContext &i) {
    QnxMsg::proc::sem_init_request msg;
    i.msg().read_type(&msg);

    msg.m_type = Qnx::QEOK;
    // Just some marker value. We care only about the value.
    msg.m_sem.m_semid = 711;
    i.msg().write_type(0, &msg);
}

void MainHandler::proc_sem_destroy(MsgContext &i) {
    // we did not allocate anything
    i.msg().write_status(Qnx::QEOK);
}

uint32_t MainHandler::map_file_flags_to_host(uint32_t oflag) {
    uint32_t mapped_oflags = 0;
    auto acc = oflag & Qnx::QO_ACCMODE;

    if (acc == Qnx::QO_RDONLY)
        mapped_oflags |= O_RDONLY;
    
    if (acc == Qnx::QO_WRONLY)
        mapped_oflags |= O_WRONLY;
    
    if (acc == Qnx::QO_RDWR)
        mapped_oflags |= O_RDWR;

    if (oflag & Qnx::QO_APPEND)
        mapped_oflags |= O_APPEND;

    if (oflag & Qnx::QO_CREAT)
        mapped_oflags |= O_CREAT;
    
    if (oflag & Qnx::QO_TRUNC) {
        // according to POSIX, this is unspecified, but Linux accepts it (despite the manpage claiming otherwise)
        // and QNX rejects it
        if (acc == Qnx::QO_RDWR || acc == Qnx::QO_WRONLY) {
            mapped_oflags |= O_TRUNC;
        }
    }

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

void MainHandler::io_open(MsgContext& i) {
    /* The oter requests have the same format */
    QnxMsg::io::io_open_request msg;
    i.msg().read_type(&msg);

    auto fd = i.proc().fds().get_attached_fd(msg.m_open.m_fd);

    int mapped_oflags = O_RDONLY;
    int oflag = msg.m_open.m_oflag;
    int eflags = msg.m_open.m_eflag;
    if (msg.m_type == QnxMsg::io::msg_io_open::TYPE) {
        mapped_oflags = map_file_flags_to_host(oflag);
    } else if (msg.m_type == QnxMsg::io::msg_handle::TYPE) {
        if (msg.m_open.m_oflag == Qnx::IO_HNDL_RDDIR) {
            mapped_oflags |= O_DIRECTORY;
        } else if (msg.m_open.m_oflag == Qnx::IO_HNDL_CHANGE || msg.m_open.m_oflag == Qnx::IO_HNDL_UTIME) {
            // chown etc.
            mapped_oflags |= O_RDONLY;
        } else {
            mapped_oflags |= O_PATH;
            if (msg.m_open.m_mode & Qnx::QS_IFLNK) 
                mapped_oflags |= O_NOFOLLOW;
        }
    }

    fd->m_path = PathInfo::mk_qnx_path(msg.m_file, true);
    i.proc().path_mapper().map_path_to_host(fd->m_path);
    
    UniqueFd tmp_fd(::open(fd->m_path.host_path(), mapped_oflags, msg.m_open.m_mode));
    if (!tmp_fd.valid()) {
        i.msg().write_status(Emu::map_errno(errno));
        return;
    }

    if (!fd->assign_fd(std::move(tmp_fd))) {
        i.msg().write_status(Emu::map_errno(errno));
        return;
    }
    i.msg().write_status(Qnx::QEOK);
}

void MainHandler::io_chdir(MsgContext& i) {
    QnxMsg::io::io_open_request msg;
    i.msg().read_type(&msg);

    auto p = i.proc().path_mapper().map_path_to_host(msg.m_file, true);

    // TODO: this is not the right check, it allows us to chdir into executables
    int r = access(p.host_path(), X_OK);
    if (r == 0) {
        i.msg().write_status(Qnx::QEOK);
    } else {
        i.msg().write_status(Emu::map_errno(errno));
    }
}

void MainHandler::proc_prefix(MsgContext &i)
{
    QnxMsg::proc::prefix_request msg;
    i.msg().read_type(&msg);

    QnxMsg::proc::prefix_reply reply;
    clear(&reply);
    reply.m_status = Qnx::QEOK;

    if (msg.m_path[0] == 0) {
        qine_strlcpy(reply.m_prefix, "/=1,a", sizeof(reply.m_prefix));
    } else {
        msg.m_path[0] = 0;
    }
    i.msg().write_type(0, &reply);
}

void MainHandler::proc_sid_query(MsgContext &i)
{
    QnxMsg::proc::session_request msg;
    i.msg().read_type(&msg);

    QnxMsg::proc::session_reply reply;
    clear(&reply);

    // In QNX, SIDs are a separate namespace, whereas in POSIX they share the PID namesace and inherit 
    // the founders PID.
    // The function also supports listing. We do not do that and just assume it is POSIX-style sid

    auto sid_map = i.proc().pids().qnx(msg.m_sid);
    if (!sid_map) {
        reply.m_status = Qnx::QEINVAL;
    } else {
        // stub, we do not have any interesting info
        reply.m_status = Qnx::QEOK;
        reply.m_links = 1;
        qine_strlcpy(reply.m_name, "sys", sizeof(reply.m_name));
        reply.m_pid = msg.m_sid;
        reply.m_sid = msg.m_sid;
        qine_strlcpy(reply.m_tty_name, ctermid(nullptr), sizeof(reply.m_name));
    }
    i.msg().write_type(0, &reply);
}

void MainHandler::proc_sid_set (MsgContext &i) {
    QnxMsg::proc::session_request msg;
    i.msg().read_type(&msg);

    int r = setsid();
    if (r < 0) {
        i.msg().write_status(Emu::map_errno(errno));
    } else {
        i.msg().write_status(Qnx::QEOK);
    }
}

void MainHandler::io_rename(MsgContext& i) {
    QnxMsg::io::rename_request msg;
    i.msg().read_type(&msg);

    auto from_path = i.proc().path_mapper().map_path_to_host(msg.m_from);
    auto to_path = i.proc().path_mapper().map_path_to_host(msg.m_to);

    int r = rename(from_path.host_path(), to_path.host_path());
    if (r == 0) {
        i.msg().write_status(Qnx::QEOK);
    } else {
        i.msg().write_status(Emu::map_errno(errno));
    }
}

void MainHandler::io_stat(MsgContext& i) {
    QnxMsg::io::stat_request msg;
    i.msg().read_type(&msg);
    struct stat sb;

    // TODO: handle trailing / (sent in eflags)

    auto p = i.proc().path_mapper().map_path_to_host(msg.m_path, true);

    QnxMsg::io::stat_reply reply;
    clear(&reply);
    int r;
    
    if (msg.m_args.m_mode & Qnx::QS_IFLNK) {
        r = lstat(p.host_path(), &sb);
    } else {
        r = stat(p.host_path(), &sb);
    }
    
    if (r < 0) {
        reply.m_status = Emu::map_errno(errno);
        i.msg().write_type(0, &reply);
        return;
    }

    transfer_stat(reply.m_stat, sb);
    reply.m_status = Qnx::QEOK;
    i.msg().write_type(0, &reply);
}

void MainHandler::io_fstat(MsgContext& i) {
    QnxMsg::io::fstat_request msg;
    i.msg().read_type(&msg);
    struct stat sb;

    // TODO: handle trailing /

    QnxMsg::io::fstat_reply reply;
    memset(&reply, 0, sizeof(reply));
    int r = fstat(i.map_fd(msg.m_fd), &sb);
    if (r < 0) {
        reply.m_status = Emu::map_errno(errno);
        i.msg().write_type(0, &reply);
        return;
    }

    transfer_stat(reply.m_stat, sb);
    i.msg().write_type(0, &reply);
}

void MainHandler::io_readdir(MsgContext &i)
{
    QnxMsg::io::readdir_request msg;
    i.msg().read_type(&msg);

    auto fd = i.proc().fds().get_open_fd(msg.m_fd);
    if (!fd->prepare_dir()) {
        i.msg().write_status(Emu::map_errno(errno));
        return;
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
        struct dirent *d = readdir(fd->m_host_dir);
        if (!d) {
            if (errno == 0) {
                
                break;
            } else {
                reply.m_status = Emu::map_errno(errno);
                return;
            }
        }

        // write the dirent (stat + path)
        qine_strlcpy(path_buf, d->d_name, sizeof(path_buf));
        i.msg().write_type(dst_off, &stat);
        i.msg().write(dst_off + sizeof(stat), path_buf, sizeof(path_buf));
        dst_off += dirent_size;
        reply.m_ndirs++;
    }

    reply.m_status = Qnx::QEOK;
    i.msg().write_type(0, &reply);
}
void MainHandler::io_rewinddir(MsgContext &i) {
    QnxMsg::io::readdir_request msg;
    i.msg().read_type(&msg);
    auto fd = i.proc().fds().get_open_fd(msg.m_fd);
    if (!fd->prepare_dir()) {
        i.msg().write_status(Emu::map_errno(errno));
        return;
    }
    rewinddir(fd->m_host_dir);
    i.msg().write_status(Qnx::QEOK);
}

void MainHandler::io_qioctl(MsgContext &i) {
    QnxMsg::io::qioctl_request msg;
    i.msg().read_type(&msg);

    Ioctl ioctl(&i);
    if (ioctl.request_group() == 't') {
        terminal_qioctl(ioctl);
    } else  {
        ioctl.set_status(Qnx::QENOSYS);
    }
    ioctl.write_response_header();
}

void MainHandler::io_ioctl(MsgContext &i) {
    QnxMsg::io::ioctl_request msg;
    i.msg().read_type(&msg);

    i.msg().write_status(Qnx::QENOSYS);
}

void MainHandler::io_lock(MsgContext &i) {
    QnxMsg::io::lock_request msg;
    i.msg().read_type(&msg);

    
    int host_fd = i.map_fd(msg.m_lock.m_fd);
    flock host_lock;
    host_lock.l_len = msg.m_lock.m_len;
    host_lock.l_start = msg.m_lock.m_start;
    host_lock.l_whence = msg.m_lock.m_whence;
    host_lock.l_pid = 0;
    host_lock.l_type = 0;

    switch (msg.m_lock.m_type) {
        case Qnx::QF_RDLCK:
            host_lock.l_type = F_RDLCK;
            break;
        case Qnx::QF_WRLCK:
            host_lock.l_type = F_WRLCK;
            break;
        case Qnx::QF_UNLCK:
            host_lock.l_type = F_UNLCK;
            break;
    }

    int host_cmd = 0;
    switch (msg.m_lock.m_cmd) {
        case Qnx::QF_SETLK:
            host_cmd = F_SETLK;
            break;
        case Qnx::QF_SETLKW:
            host_cmd = F_SETLKW;
            break;
        case Qnx::QF_GETLK:
            host_cmd = F_GETLK;
            break;
        default:
            i.msg().write_status(Qnx::QEINVAL);
            Log::print(Log::UNHANDLED, "lock command %d unahdnled\n", msg.m_lock.m_cmd);
            return;
    }

    int r = fcntl(host_fd, host_cmd, &host_lock);
    if (r < 0) {
        i.msg().write_status(Emu::map_errno(errno));
        return;
    }

    QnxMsg::io::lock_reply reply;
    clear(&reply);
    reply.m_status = Qnx::QEOK;
    reply.m_lock.m_cmd = msg.m_lock.m_cmd;
    reply.m_lock.m_fd = msg.m_lock.m_fd;
    if (host_lock.l_start >= std::numeric_limits<int32_t>::max() || host_lock.l_len >= std::numeric_limits<int32_t>::max()) {
        i.msg().write_status(Qnx::QERANGE);
        return;
    }
    reply.m_lock.m_start = host_lock.l_start;
    reply.m_lock.m_len = host_lock.l_len;
    auto qnx_pid = i.proc().pids().host(host_lock.l_pid);
    if (qnx_pid) {
        reply.m_lock.m_pid = qnx_pid->qnx_pid();
    } else {
        reply.m_lock.m_pid = QnxPid::PID_UNKNOWN;
    }
    reply.m_lock.m_whence = SEEK_SET;
    switch (host_lock.l_type) {
    case F_RDLCK:
        reply.m_lock.m_type = Qnx::QF_RDLCK;
        break;
    case F_WRLCK:
        reply.m_lock.m_type = Qnx::QF_WRLCK;
        break;
    default:
    case F_UNLCK:
        reply.m_lock.m_type = Qnx::QF_UNLCK;
        break;
    }
    i.msg().write_type(0, &reply);
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

    // the basic permissions are the same
    dst.m_mode = src.st_mode & ACCESSPERMS;

    switch (src.st_mode & S_IFMT) {
        case S_IFIFO:
            dst.m_mode |= Qnx::QS_IFIFO;
            break;
        case S_IFCHR:
            dst.m_mode |= Qnx::QS_IFCHR;
            break;
        case S_IFDIR:
            dst.m_mode |= Qnx::QS_IFDIR;
            break;
        /* IFNAME is Qnx special */
        case S_IFBLK:
            dst.m_mode |= Qnx::QS_IFBLK;
            break;
        case S_IFREG:
            dst.m_mode |= Qnx::QS_IFREG;
            break;
        case S_IFLNK:
            dst.m_mode |= Qnx::QS_IFLNK;
            break;
        case S_IFSOCK:
            dst.m_mode |= Qnx::QS_IFSOCK;
            break;
        default:
            dst.m_mode |= Qnx::QS_IFCHR;
    }

    if (src.st_mode & S_ISUID)
        dst.m_mode |= Qnx::QS_ISUID;
    if (src.st_mode & S_ISGID)
        dst.m_mode |= Qnx::QS_ISGID;
    if (src.st_mode & S_ISVTX)
        dst.m_mode |= Qnx::QS_ISVTX;
    
    dst.m_uid = src.st_uid;
    dst.m_gid = src.st_gid;
    dst.m_nlink = src.st_nlink;
    dst.m_status = 0;
}

void MainHandler::io_close(MsgContext& i) {
    QnxMsg::io::close_request msg;
    i.msg().read_type(&msg);

    auto fd = i.proc().fds().get_open_fd(msg.m_fd);
    if (!fd->close()) {
        i.msg().write_status(Emu::map_errno(errno));
        return;
    }

    i.msg().write_status(Qnx::QEOK);
}

void MainHandler::io_lseek(MsgContext &i) {
    QnxMsg::io::lseek_request msg;
    i.msg().read_type(&msg);

    QnxMsg::io::lseek_reply reply;
    memset(&reply, 0, sizeof(reply));

    // TODO: handle overflow
    off_t off = lseek(i.map_fd(msg.m_fd), msg.m_offset, msg.m_whence);
    if (off == -1) {
        reply.m_status = Emu::map_errno(errno);
        reply.m_zero = 0;
        reply.m_offset = -1;
    } else {
        reply.m_status = Qnx::QEOK;
        reply.m_zero = 0;
        reply.m_offset = off;
    }
    i.msg().write_type(0, &reply);
}

void MainHandler::io_read(MsgContext &i) {
    QnxMsg::io::read_request msg;
    i.msg().read_type(&msg);

    auto fd = i.proc().fds().get_open_fd(msg.m_fd);
    if (fd->m_filter) {
        fd->m_filter->read(i, *fd, msg);
    } else {
        // Log::dbg("Reading from %lx\n", lseek(fd->m_host_fd, 0, SEEK_CUR));
        std::vector<struct iovec> iov;
        i.msg().write_iovec(sizeof(msg), msg.m_nbytes, iov);

        QnxMsg::io::read_reply reply;
        int r = readv(i.map_fd(msg.m_fd), iov.data(), iov.size());
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
}

void MainHandler::io_write(MsgContext &i) {
    QnxMsg::io::write_request msg;
    i.msg().read_type(&msg);

    std::vector<struct iovec> iov;
    i.msg().read_iovec(sizeof(msg), msg.m_nbytes, iov);

    QnxMsg::io::write_reply reply;
    int r = writev(i.map_fd(msg.m_fd), iov.data(), iov.size());
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

void MainHandler::io_fcntl_flags(MsgContext &i) {
    QnxMsg::io::fcntl_flags_request msg;
    i.msg().read_type(&msg);

    QnxMsg::io::fcntl_flags_reply reply;
    memset(&reply, 0, sizeof(reply));
    int r = fcntl(i.map_fd(msg.m_fd), F_GETFL);
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

void MainHandler::io_dup(MsgContext &i) {
    QnxMsg::io::dup_request msg;
    i.msg().read_type(&msg);

    auto src_fd = i.proc().fds().get_open_fd(msg.m_src_fd);
    auto dst_fd = i.proc().fds().get_attached_fd(msg.m_dst_fd);

    if (dst_fd->m_open) 
        dst_fd->close();

    Log::print(Log::FD, "fd dup %d -> %d\n", msg.m_src_fd, msg.m_dst_fd);
    UniqueFd new_fd = dup2(src_fd->m_host_fd, dst_fd->m_fd);
    if (!new_fd.valid()) {
        /* Yes, we are at inconsitent state, but it should not really happen if replacing a FD */
        i.msg().write_status(Emu::map_errno(errno));
    } else {
        dst_fd->assign_fd(std::move(new_fd));
        dst_fd->m_host_fd = msg.m_dst_fd;
        i.msg().write_status(Qnx::QEOK);
    }
}

void MainHandler::io_fpathconf(MsgContext &i) {
    QnxMsg::io::fpathconf_request msg;
    i.msg().read_type(&msg);

    int linux_name = -1;
    int limit = std::numeric_limits<uint16_t>::max();

    QnxMsg::io::fpathconf_reply reply;
    clear(&reply);

    switch(msg.m_idx) {
        case Qnx::QPC_LINK_MAX:
            linux_name = _PC_LINK_MAX;
            break;
        case Qnx::QPC_MAX_CANON:
            linux_name = _PC_MAX_CANON;
            break;
        case Qnx::QPC_MAX_INPUT:
            linux_name = _PC_MAX_INPUT;
            break;
        case Qnx::QPC_NAME_MAX:
            linux_name = _PC_NAME_MAX;
            limit = Qnx::QNAME_MAX;
            break;
        case Qnx::QPC_PATH_MAX:
            linux_name = _PC_PATH_MAX;
            limit = Qnx::QPATH_MAX;
            break;
        case Qnx::QPC_PIPE_BUF:
            linux_name = _PC_PIPE_BUF;
            break;
        case Qnx::QPC_CHOWN_RESTRICTED:
            linux_name = _PC_CHOWN_RESTRICTED;
            break;
        case Qnx::QPC_NO_TRUNC:
            linux_name = _PC_NO_TRUNC;
            break;
        case Qnx::QPC_VDISABLE:
            linux_name = _PC_VDISABLE;
            break;
        case Qnx::QPC_DOS_SHARE:
        case Qnx::QPC_DOS_LOCKS:
            reply.m_status = Qnx::QEOK;
            reply.m_value = 0;
            i.msg().write_type(0, &reply);
            return;
        default:
            reply.m_status = Qnx::QEINVAL;
            reply.m_value = -1;
            i.msg().write_type(0, &reply);
            return;
    }

    errno = 0;
    int r = fpathconf(i.map_fd(msg.m_fd), msg.m_idx);
    if (r < 0 && errno != 0) {
        i.msg().write_status(Emu::map_errno(errno));
    } else {
        reply.m_status = Qnx::QEOK;
        reply.m_value = std::min(limit, r);
        i.msg().write_type(0, &reply);
    }
}

void MainHandler::io_chmod(MsgContext &i) {
    QnxMsg::io::chmod_request msg;
    i.msg().read_type(&msg);

    int r = fchmod(i.map_fd(msg.m_fd), msg.m_mode);
    i.msg().write_status((r == 0) ? Qnx::QEOK : Emu::map_errno(errno));
}

void MainHandler::io_chown(MsgContext &i) {
    QnxMsg::io::chown_request msg;
    i.msg().read_type(&msg);

    int r = fchown(i.map_fd(msg.m_fd), msg.m_uid, msg.m_gid);
    i.msg().write_status((r == 0) ? Qnx::QEOK : Emu::map_errno(errno));
}

void MainHandler::io_utime(MsgContext &i) {
    QnxMsg::io::utime_request msg;
    i.msg().read_type(&msg);
    int r;

    if (msg.m_cur_flag) {
        r = futimens(i.map_fd(msg.m_fd), NULL);
    } else {
        struct timespec ts[2];
        ts[0].tv_nsec = 0;
        ts[0].tv_sec = msg.m_actime;
        ts[1].tv_nsec = 0;
        ts[1].tv_sec = msg.m_mod;
        r = futimens(msg.m_fd, ts);
    }

    i.msg().write_status((r == 0) ? Qnx::QEOK : Emu::map_errno(errno));
}

void MainHandler::fsys_unlink(MsgContext &i) {
    QnxMsg::fsys::unlink_request msg;
    i.msg().read_type(&msg);

    auto p = i.proc().path_mapper().map_path_to_host(msg.m_path, true);

    int r = unlink(p.host_path());
    if (r < 0 && errno == EISDIR) {
        r = rmdir(p.host_path());
    }

    if (r == 0) {
        i.msg().write_status(Qnx::QEOK);
    } else {
        i.msg().write_status(Emu::map_errno(errno));
    }
}

void MainHandler::fsys_mkspecial(MsgContext &i) {
    QnxMsg::fsys::mkspecial_request msg;
    i.msg().read_type(&msg);
    uint16_t mode = msg.m_open.m_mode;
    int r;

    auto p = i.proc().path_mapper().map_path_to_host(msg.m_path, true);


    if (S_ISDIR(mode)) {
        r = mkdir(p.host_path(), mode & ALLPERMS);
        if (r == 0) {
            i.msg().write_status(Qnx::QEOK);
        } else {
            i.msg().write_status(Emu::map_errno(errno));
        }
    } else if (mode & S_IFLNK) {
        if (Fsutil::is_abs(msg.m_target)) {
            auto tp = i.proc().path_mapper().map_path_to_host(msg.m_target);
            r = symlink(tp.host_path(), p.host_path());
        } else {
            r = symlink(msg.m_target, p.host_path());
        }
        if (r == 0) {
            i.msg().write_status(Qnx::QEOK);
        } else {
            i.msg().write_status(Emu::map_errno(errno));
        }
    } else {
        i.msg().write_status(Qnx::QENOSYS);
    }
}

void MainHandler::fsys_readlink(MsgContext &i) {
    QnxMsg::fsys::readlink_request msg;
    i.msg().read_type(&msg);
    QnxMsg::fsys::readlink_reply reply;
    clear(&reply);

    auto link_path = i.proc().path_mapper().map_path_to_host(msg.m_path, true);
    std::string target_path_raw;
    if (!Fsutil::readlink(link_path.host_path(), target_path_raw)) {
        i.msg().write_status(Emu::map_errno(errno));
    } else {
        if (!target_path_raw.empty() && target_path_raw[0] == '/') {
            auto target_path = i.proc().path_mapper().map_path_to_qnx(target_path_raw.c_str());
            qine_strlcpy(reply.m_path, target_path.qnx_path(), sizeof(reply.m_path));
        } else {
            qine_strlcpy(reply.m_path, target_path_raw.c_str(), sizeof(reply.m_path));
        }
        reply.m_status = Qnx::QEOK;
        i.msg().write_type(0, &reply);
    }
}

void MainHandler::fsys_link(MsgContext &i) {
    /* Note: we cannot use linkat without CAP_DAC_READ_SEARCH, which would be ideal*/
    QnxMsg::fsys::link_request msg;
    i.msg().read_type(&msg);

    auto fd = i.proc().fds().get_open_fd(msg.m_arg.m_fd);
    i.proc().path_mapper().map_path_to_host(fd->m_path);

    auto dst_path = PathInfo::mk_qnx_path(msg.m_new_path);
    i.proc().path_mapper().map_path_to_host(dst_path);

    int r = link(fd->m_path.host_path(), dst_path.host_path());
    if (r == 0) {
        i.msg().write_status(Qnx::QEOK);
    } else {
        i.msg().write_status(Emu::map_errno(errno));
    }
}

void MainHandler::fsys_sync(MsgContext &i) {
    sync();
    i.msg().write_status(Qnx::QEOK);
}

void MainHandler::fsys_trunc(MsgContext &i) {
    QnxMsg::fsys::trunc_request msg;
    i.msg().read_type(&msg);
    int host_fd = i.map_fd(msg.m_fd);
    // we need to get the truncate offset, so save current pos, seek to destination, and seek back
    // how to do that atomically, I do not know
    off_t prev = lseek(host_fd, 0, SEEK_CUR);
    if (prev == -1) {
        i.msg().write_status(Emu::map_errno(errno));
        return;
    }
    off_t to = lseek(host_fd, msg.m_offset, msg.m_whence);
    if (to == -1) {
        i.msg().write_status(Emu::map_errno(errno));
        return;
    }
    lseek(host_fd, prev, SEEK_SET);

    int r = ftruncate(host_fd, to);
    if (r < 0) {
        i.msg().write_status(Emu::map_errno(errno));
        return;
    }
    
    QnxMsg::fsys::trunc_reply reply;
    clear(&reply);
    reply.m_status = Qnx::QEOK;
    reply.m_offset = to;
    i.msg().write_type(0, &reply);
}

void MainHandler::fsys_fsync(MsgContext &i) {
    QnxMsg::fsys::fsync_request msg;
    i.msg().read_type(&msg);
    int r;
    if (msg.m_flags & 0xFF) {
        r = fdatasync(i.map_fd(msg.m_fd));
    } else {
        r = fsync(i.map_fd(msg.m_fd));
    }

    if (r == 0) {
        i.msg().write_status(Qnx::QEOK);
    } else {
        i.msg().write_status(Emu::map_errno(errno));
    }
}

void MainHandler::fsys_pipe(MsgContext &i) {
    QnxMsg::fsys::pipe_request msg;
    i.msg().read_type(&msg);
    auto fds = &i.proc().fds();
    QnxFd* qnx_fds[2] = {
        fds->get_attached_fd(msg.m_fd_in),
        fds->get_attached_fd(msg.m_fd_out),
    };

    int host_fds[2];
    int r = pipe(host_fds);
    if (r < 0) {
        i.msg().write_status(Emu::map_errno(errno));
        return;
    }

    UniqueFd uq_host_fds[2] = {
        UniqueFd(host_fds[0]),
        UniqueFd(host_fds[1]),
    };

    // now we must move the FDs to correct locations
    if (!fds->assign_fds(2, qnx_fds, uq_host_fds)) {
        i.msg().write_status(Emu::map_errno(errno));
        return;
    }
    i.msg().write_status(Qnx::QEOK);
}

void MainHandler::fsys_disk_entry(MsgContext &i) {
    QnxMsg::fsys::disk_entry_request msg;
    i.msg().read_type(&msg);

    QnxMsg::fsys::disk_entry_reply reply;
    clear(&reply);

    struct statvfs stat;
    int r = fstatvfs(i.map_fd(msg.m_fd), &stat);
    if (r < 0) {
        i.msg().write_status(Emu::map_errno(errno));
        return;
    }

    qine_strlcpy(reply.m_drive_name, "/deasd", sizeof(reply.m_drive_name));
    /* We could maybe get better info about the disk itself */
    reply.m_disk_sectors = stat.f_blocks;
    reply.m_num_sectors = stat.f_blocks;
    reply.m_blk_offset = 0;
    reply.m_disk_drv = 1;
    reply.m_disk_type = 2; // harddisk
    
    i.msg().write_type(0, &reply);
}

void MainHandler::fsys_get_mount(MsgContext &i) {
    int r;
    QnxMsg::fsys::get_mount_request msg;
    i.msg().read_type(&msg);

    QnxMsg::fsys::get_mount_reply reply;
    clear(&reply);

    auto path = i.proc().path_mapper().map_path_to_host(msg.m_path);
    
    std::string query_host_path;
    Fsutil::realpath(path.host_path(), query_host_path);
    struct stat query_stat;
    r = stat(query_host_path.c_str(), &query_stat);
    if (r < 0) {
        i.msg().write_status(Emu::map_errno(errno));
        return;
    }

    struct mntent mntent;
    constexpr size_t mntent_buf_size = 4096;
    std::unique_ptr<char[]> mntent_buf(new char[mntent_buf_size]);
    UniqueMntent fd(setmntent("/etc/mtab", "r"));

    while (getmntent_r(fd.get(), &mntent, mntent_buf.get(), mntent_buf_size)) {
        if (Fsutil::path_starts_with(path.host_path(), mntent.mnt_dir)) {
            auto mount_path = i.proc().path_mapper().map_path_to_qnx(mntent.mnt_dir);
            struct stat mount_stat;
            r = stat(mount_path.host_path(), &mount_stat);
            if (r < 0) {
                i.msg().write_status(Emu::map_errno(errno));
                return;
            }

            if (mount_stat.st_dev == query_stat.st_dev) {
                reply.m_status = Qnx::QEOK;
                qine_strlcpy(reply.m_path, mount_path.qnx_path(), sizeof(reply.m_path));
                i.msg().write_type(0, &reply);
                return;
            }
        }
    }
    
    i.msg().write_status(Emu::map_errno(errno));
}

void MainHandler::fsys_disk_space(MsgContext &i) {
    QnxMsg::fsys::disk_space_request msg;
    i.msg().read_type(&msg);

    struct statvfs stat;
    int r = fstatvfs(i.map_fd(msg.m_fd), &stat);
    if (r < 0) {
        i.msg().write_status(Emu::map_errno(errno));
        return;
    }

    QnxMsg::fsys::disk_space_reply reply;
    clear(&reply);
    reply.m_free_blocks = stat.f_bavail;
    reply.m_total_blocks = stat.f_blocks;
    i.msg().write_type(0, &reply);
}

void MainHandler::dev_info(MsgContext &i) {
    QnxMsg::dev::dev_info_request msg;
    i.msg().read_type(&msg);

    QnxMsg::dev::dev_info_reply reply;
    clear(&reply);
    auto fd = i.proc().fds().get_open_fd(msg.m_fd);
    if (fill_dev_info(i, fd, &reply.m_info)) {
        reply.m_status = Qnx::QEOK;
    } else {
        reply.m_status = Emu::map_errno(errno);
    }
    i.msg().write_type(0, &reply);
}

void MainHandler::dev_fdinfo(MsgContext &i) {
    QnxMsg::dev::dev_fdinfo_request msg;
    i.msg().read_type(&msg);

    if (msg.m_pid != i.proc().pid()) {
        i.msg().write_status(Qnx::QESRCH);
        return;
    }

    QnxMsg::dev::dev_fdinfo_reply reply;
    clear(&reply);
    auto fd = i.proc().fds().get_open_fd(msg.m_fd);
    if (fill_dev_info(i, fd, &reply.m_info)) {
        reply.m_status = Qnx::QEOK;
    } else {
        reply.m_status = Emu::map_errno(errno);
    }
    i.msg().write_type(0, &reply);
}

bool MainHandler::fill_dev_info(MsgContext &i, QnxFd *fd, QnxMsg::dev::dev_info *dst) 
{
    // mostly make the stuff up, this is mostly system debugging information or does not map to Linux easily
    dst->m_tty = 1;
    dst->m_nid = i.proc().nid();
    dst->m_driver_pid = QnxPid::PID_PROC;
    dst->m_nature = 0;
    dst->m_attributes = 0;
    dst->m_capabilities = 0;
    dst->m_open_count = 1;
    dst->m_unit = 1;
    dst->m_flags = 0xf; //readers, writers, winch, fwd (just a guess
    auto pgrp = i.proc().pids().host(tcgetpgrp(fd->m_host_fd));
    dst->m_pgrp = pgrp ? pgrp->qnx_pid() : QnxPid::PID_UNKNOWN;
    auto sess = i.proc().pids().host(tcgetsid(fd->m_host_fd));
    dst->m_session = sess ? sess->qnx_pid() : QnxPid::PID_UNKNOWN;

    std::string ttyname;
    if (!Fsutil::ttyname(fd->m_host_fd, ttyname)) {
        return false;
    }

    qine_strlcpy(dst->m_driver_type, "pseudo", sizeof(dst->m_driver_type));

    auto path = i.proc().path_mapper().map_path_to_qnx(ttyname.c_str());

    if (strlen(path.qnx_path()) + 1 > sizeof(dst->m_tty_name)) {
        qine_strlcpy(dst->m_tty_name, "/dev/too-long", sizeof(dst->m_tty_name));
    } else {
        qine_strlcpy(dst->m_tty_name, path.qnx_path(), sizeof(dst->m_tty_name));
    }
    return true; 
}
