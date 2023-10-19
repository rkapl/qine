#!/bin/bash
set -eux
ninja -C build
gdb=`which gdb`
exec env -i "QNX_ROOT=$QNX_ROOT" "PATH=$QNX_ROOT/bin" "QINE=1"\
    $gdb \
    -ex 'handle SIGUSR1 nostop' \
    -ex 'handle SIGSEGV noprint' \
    -ex 'b Emu::debug_hook_problem' \
    --args build/qine "$@"

