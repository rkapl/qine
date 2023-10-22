#!/bin/bash
set -eux
ninja -C build
gdb=`which gdb`
exec env "QNX_ROOT=$QNX_ROOT" "PATH=/bin:/usr/bin" "QINE=1"\
    $gdb \
    -ex 'handle SIGUSR1 nostop' \
    -ex 'handle SIGSEGV noprint' \
    -ex 'b Emu::debug_hook_problem' \
    --args build/qine "$@"

