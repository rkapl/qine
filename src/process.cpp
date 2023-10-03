#include <cstddef>
#include <cstdint>
#include <cstring>
#include <sys/ucontext.h>
#include "mem_ops.h"
#include "process.h"
#include "emu.h"
#include "qnx/magic.h"
#include "qnx/procenv.h"
#include "segment.h"
#include "segment_descriptor.h"
#include "context.h"
#include "types.h"

Process* Process::m_current = nullptr;

Process::Process(): m_segment_descriptors(1024), m_magic_guest_pointer(FarPointer::null())
{
}

Process::~Process() {}

void Process::initialize() {
    assert(!m_current);
    m_current = new Process();
    m_current->m_emu.init();
    memset(&m_current->startup_context, 0xcc, sizeof(m_current->startup_context));
}

Qnx::pid_t Process::pid()
{
    return 2;
}

Qnx::pid_t Process::parent_pid()
{
    return 1;
}

void Process::enter_emu()
{
    m_emu.enter_emu();
}

void* Process::translate_segmented(FarPointer ptr, uint32_t size, RwOp write)
{
    auto sd = descriptor_by_selector(ptr.m_segment);
    if (!sd)
        throw GuestStateException("segment not present");
    
    if (!sd->segment()->check_bounds(ptr.m_offset, size))
        throw GuestStateException("address out of segment bounds");

    // TODO: write check not implemented
    
    return sd->segment()->pointer(ptr.m_offset, size);
}

std::shared_ptr<Segment> Process::allocate_segment() {
    auto seg = std::make_shared<Segment>();
    m_segments.add_front(seg.get());
    return seg;
}

SegmentDescriptor* Process::create_segment_descriptor(Access access, const std::shared_ptr<Segment>& mem)
{
    auto sd = m_segment_descriptors.alloc();
    auto idx = m_segment_descriptors.index_of(sd);
    *sd = std::move(SegmentDescriptor(idx, access, mem));
    return sd;
}

SegmentDescriptor* Process::create_segment_descriptor_at(Access access, const std::shared_ptr<Segment>& mem, SegmentId id)
{
    auto sd = m_segment_descriptors.alloc_at(id);
    *sd  = std::move(SegmentDescriptor(id, access, mem));
    return sd;
}

SegmentDescriptor* Process::descriptor_by_selector(uint16_t id) {
    return m_segment_descriptors.get(id >> 3);
}

void Process::set_errno(int v) {
    m_magic->Errno = v;
}

void Process::setup_magic(SegmentDescriptor *data_sd, SegmentAllocator& alloc)
{
     /* Allocate place for the magic */
    alloc.alloc(sizeof(Qnx::Magic));
    m_magic_guest_pointer = data_sd->pointer(alloc.offset());
    m_magic = reinterpret_cast<Qnx::Magic*>(alloc.ptr());
    memset(m_magic, 0xCC, sizeof(*m_magic));
    
    /* Now create the magical segment that will be pointing to magic */
    m_magic_pointer = allocate_segment();
    m_magic_pointer->reserve(MemOps::PAGE_SIZE);
    m_magic_pointer->grow(Access::READ_WRITE, MemOps::PAGE_SIZE);
    m_magic_pointer->set_limit(8);
    m_magic_pointer->make_shared();
    // It is swapped for som reason
    *reinterpret_cast<FarPointer*>(m_magic_pointer->pointer(0, sizeof(FarPointer))) = m_magic_guest_pointer;
    /* And publish it under well known ID. Unfortunately, we cannot publish GDT selectors (understandably), 
     * so we will fake it in emu 
     */
    create_segment_descriptor_at(Access::READ_ONLY, m_magic_pointer, SegmentDescriptor::sel_to_id(Qnx::MAGIC_PTR_SELECTOR));

}

void Process::setup_startup_context(int argc, char **argv)
{
    Context ctx(&startup_context);

    if (!m_load.entry_main.has_value() || ! m_load.entry_slib.has_value() || !m_load.data_segment.has_value()) {
        throw GuestStateException("Loading incomplete");
    }

    /* First allocate some space in data segment */
    auto data_sd = m_segment_descriptors.get(m_load.data_segment.value());
    if (!data_sd) {
        throw GuestStateException("Guest does not seem to be loaded properly (no data segment)");
    }

    auto data_seg = data_sd->segment();
    auto alloc = SegmentAllocator(data_seg.get());

    setup_magic(data_sd, alloc);

    /* Now create the stack environment, that is argc, argv, arge  */
    ctx.push_stack(0); // Unknown
    ctx.push_stack(1); // nid?
    ctx.push_stack(parent_pid());
    ctx.push_stack(pid());

    /* Environment */
    ctx.push_stack(0);
    alloc.push_string("PATH=/bin");
    ctx.push_stack(alloc.offset());

    /* Argv */
    ctx.push_stack(0);
    for (int i = argc - 1; i >= 0; i--) {
        alloc.push_string(argv[i]);
        ctx.push_stack(alloc.offset());
    }

    /* Argc */
    ctx.push_stack(argc);

    /* Entry points*/
    auto main_entry = m_load.entry_main.value();
    ctx.push_stack(main_entry.m_segment);
    ctx.push_stack(main_entry.m_offset);

    auto slib_entry = m_load.entry_slib.value();
    ctx.reg(REG_CS) = slib_entry.m_segment;
    ctx.reg(REG_EIP) = slib_entry.m_offset;

    printf("Real start: %x:%x\n", slib_entry.m_segment, slib_entry.m_offset);
    printf("Program start: %x:%x\n", main_entry.m_segment, main_entry.m_offset);

    data_sd->update_descriptors();

    /* What we have allocated */
    ctx.reg(REG_EBX) = data_seg->size();
    /* No, there is nothing free, allocate your own :) */
    ctx.reg(REG_ECX) = 0;

    // ctx.dump(stdout);

    // breakpoints
    //ctx.write<uint8_t>(Context::CS, 0x61e , 0xCC);
}