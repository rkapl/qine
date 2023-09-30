#!/bin/bash
set -eux
ninja -C build && gdb \
    -ex 'handle SIGUSR1 nostop' \
    -ex 'handle SIGSEGV noprint' \
    -ex 'b Emu::debug_hook' \
    -ex 'r' \
    --args build/qine ~/build/qnx/qnxdump/boot/sys/Slib32
