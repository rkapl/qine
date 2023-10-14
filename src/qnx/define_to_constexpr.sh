#!/bin/sh
set -eu
exec sed -E 's/#define (\S+)\s+(\S+)/static constexpr int \1 = \2;/'