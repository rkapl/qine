#!/bin/bash
set -eux
W=$QNX_ROOT/usr/watcom/10.6
Q=../build/qine

$Q -- $W/bin/wcc386 main.c -I$W/include -I$QNX_ROOT/usr/include

$Q -- $W/bin/wlink FORM qnx flat NAME main FILE main.o LIBP $W/include:$QNX_ROOT/usr/lib OP q

$Q -- ./main