# Qine Is Not Emulator

In particular, it is not an emulator for QNX4. It allows you to run QNX4 binaries on your Linux machine (like Wine). At least some of them.


## Demo
[![asciicast](https://asciinema.org/a/msEDHD8S6WESRpJM3LV1DOWPo.svg)](https://asciinema.org/a/msEDHD8S6WESRpJM3LV1DOWPo)

## Usage

To run Qine binaries, you will need a QNX root file system. At minimum, you will need the QNX system library, `Slib32`. But if you want to run shell, you will want the standad utilities, like 'ls', 'cat' etc. Let's assume you've got the QNX root FS extracted in `$QNX_ROOT`. Then this command will run the shell:

`PATH=/bin:/usr/bin qine -m /,$QNX_ROOT -m /host,/ -l $QNX_ROOT/boot/sys/Slib32,sys,entry=0x7D0 -- /bin/sh`

To break it down:

- `-m` specifies that '/' in QNX applications will be mapped to `$QNX_ROOT`. This will allow to e.g. run standard QNX utilities from shell.
- second `-m` specifies that you will be able to access the other files as `/host/`
- `-l` specifies the location of the mandatory system library and its entry points
- We reset the `PATH` environment varible to a sane location applicable for QNX. Some modern distributions do not include `/bin`/ in path

## Slib

Slib32 is a system library needed to run most QNX libraries. Qine does not ship with this library, you need to get it from QNX. You need the actual library and you need to know its entry point and supply it to QNX, using the `--lib/-l` argument.

So far, I have tested Qine only with the recent Slib from QNX 4.25. This Slib has the `sha256sum` of
`69e9084c8c41e33df222a0eadbe6122a62536872ebc066a32eaef74be17b83c6` and `entry=0x7D`.

If you have a different QNX version, you might be able to use the Slib, even if it is not tested. To get its entry parameter, run the `Slib32` as an exutable and you will get an output like this:

```
Attaching name qnx/syslib
Registering shared library Slib32, offset 7d0
You can load the library for your program using:
  --lib $QNX_ROOT/boot/sys/Slib32,sys,entry=0x7d0
  ```

## Emulation Details

In general, file-access is supported, QNX IPC is not. Most POSIX-y utilities should run 
(ksh, bash, ls, grep, find etc....). System utilities (sin) and other stuff using the QNX IPC is unlikely to run.


## Compilation
Qine is a standard CMake application. You can build it e.g using:
```
cmake -B build-dir
cmake --build build-dir
```
