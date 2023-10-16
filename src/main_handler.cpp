
#include <bits/types/struct_timespec.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <dirent.h>
#include <errno.h>
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

#include "gen_msg/common.h"
#include "gen_msg/dev.h"
#include "gen_msg/fsys.h"
#include "mem_ops.h"
#include "process.h"
#include "msg_handler.h"
#include "main_handler.h"
#include "qnx/errno.h"
#include "qnx/io.h"
#include "qnx/msg.h"
#include "qnx/procenv.h"
#include "qnx/psinfo.h"
#include "qnx/types.h"
#include "types.h"
#include "log.h"
#include <gen_msg/proc.h>
#include <gen_msg/io.h>


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
                default:
                    unhandled_msg();
                break;
            };
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

        case QnxMsg::fsys::msg_unlink::TYPE:
            fsys_unlink(i);
            break;

        case QnxMsg::dev::msg_tcgetattr::TYPE:
            dev_tcgetattr(i);
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
     * We use open(/dev/null) to reserver FD and later replace them with dup3.
     */
    QnxMsg::proc::fd_request msg;
    i.msg().read_type(&msg);

    if (msg.m_fd != 0) {
        throw Unsupported("FD allocation from arbitrary number not supported");
    }

    int fd = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
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

    QnxMsg::proc::fd_reply1 reply;
    memset(&reply, 0, sizeof(reply));
    reply.m_status = 0;
    reply.m_fd = fd;
    i.msg().write_type(0, &reply);
}

void MainHandler::proc_fd_detach(MsgInfo& i) {
    /* QNX first closes the FD with iomgr, then detaches it. We ignore the close, instead close on detach */
    QnxMsg::proc::fd_request fd;
    i.msg().read_type(&fd);

    auto fdi = i.ctx().proc()->fd_get(fd.m_fd);

    int r;
    if (fdi->m_dir) {
        r = closedir(fdi->m_dir);
    } else {
        r = close(fd.m_fd);
    }

    i.ctx().proc()->fd_release(fd.m_fd);
    if (r < 0) {
        i.msg().write_status(Emu::map_errno(errno));
    } else {
        i.msg().write_status(0);
    }
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

void MainHandler::proc_vc_detach(MsgInfo& i) 
{
    /* This is stubbed, because QNX/slib seems to have a bug where the stat result
     * overwrites the NID in the message and it then mistakenly calls vc_detach.*/
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
    int r = i.ctx().proc()->m_emu.signal_sigact(msg.m_signum, msg.m_offset, msg.m_mask);
    i.msg().write_status(r);
}

void MainHandler::proc_sigmask(MsgInfo &i) {
    QnxMsg::proc::signal_request msg;
    i.msg().read_type(&msg);

    msg.m_mask = (msg.m_mask & ~msg.m_bits) | msg.m_bits;

    int r = i.ctx().proc()->m_emu.signal_sigact(msg.m_signum, msg.m_offset, msg.m_mask);
    i.msg().write_status(r);
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

void MainHandler::io_open(MsgInfo& i) {
    QnxMsg::common::open msg;
    i.msg().read_type(&msg);

    int mapped_oflags = O_RDONLY;
    int oflag = msg.m_oflag;
    int eflags = msg.m_eflag;
    if (msg.m_type == QnxMsg::io::msg_io_open::TYPE) {
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

    int r = dup2(fd, msg.m_fd);
    
    if (r < 0) {
        i.msg().write_status(Emu::map_errno(errno));
        return;
    }
    close(fd);
    i.msg().write_type(0, &msg);
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
    // wait for detach
    i.msg().write_status(0);
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

void MainHandler::fsys_unlink(MsgInfo &i) {
    QnxMsg::common::open msg;
    i.msg().read_type(&msg);

    int r = unlink(msg.m_path);
    i.msg().write_status(Emu::map_errno(r));
}

void MainHandler::dev_tcgetattr(MsgInfo &i) {
    QnxMsg::dev::tcgetattr_request msg;
    i.msg().read_type(&msg);

    i.msg().write_status(Qnx::QENOTTY);
}