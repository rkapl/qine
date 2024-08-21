#!/bin/bash
set -eux
ninja -C build
gdb=`which gdb`
exec env "PATH=/bin:/usr/bin" \
    $gdb \
    -ex 'handle SIGUSR1 nostop' \
    -ex 'handle SIGSEGV noprint' \
    -ex 'b Emu::debug_hook_problem' \
    -ex 'set disassembly-flavor intel' \
    --args build/qine \
    -m /,$QNX_ROOT -m /host,/,exec=qnx \
    $QNX_SLIB \
    "$@"


