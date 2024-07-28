#!/bin/bash
set -eux
cd ..
export TERMINFO=/host`pwd`/terminfo/prebuilt/

    env -i "PATH=/bin:/usr/bin" "TERMINFO=$TERMINFO" \
    build/qine -m /,$QNX_ROOT -m /host,/,exec=qnx \
    $QNX_SLIB \
    -- /usr/bin/tic ./terminfo/terminfo.src

cd terminfo/prebuilt
tar -cv -f ../terminfo.tar .
cd ..
xz -f terminfo.tar
rm -rf prebuilt