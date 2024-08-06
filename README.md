# Qine Is Not Emulator for QNX4

It allows you to run QNX4 binaries on your Linux machine (like Wine). At least some of them.

You can use it to run your QNX4 build on modern HW, without a VM. You can run your build in parallel. You can test. You can have fun.

## Demo
[![asciicast](https://asciinema.org/a/msEDHD8S6WESRpJM3LV1DOWPo.svg)](https://asciinema.org/a/620602?autoplay=1)

## Usage

To run Qine binaries, you will need a QNX root file system. At minimum, you will need the QNX system library, `Slib32`. But if you want to run shell, you will want the standad utilities, like `ls`, `cat` etc. Let's assume you've got the QNX root FS extracted in `$QNX_ROOT`. Then this command will run the shell:

`PATH=/bin:/usr/bin qine -m /,$QNX_ROOT -m /host,/ -l $QNX_ROOT/boot/sys/Slib32,sys,entry=0x7D0 -- /bin/sh`

To break it down:

- `-m` specifies that `/` in QNX applications will be mapped to `$QNX_ROOT`. This will allow to e.g. run standard QNX utilities from shell.
- second `-m` specifies that you will be able to access the other files as `/host/`
- `-l` specifies the location of the mandatory system library and its entry points
- We reset the `PATH` environment varible to a sane location applicable for QNX. Some modern distributions do not include `/bin` in path

### Slib

Slib is a system library needed to run most QNX libraries. Qine does not ship with this library, you need to get it from QNX. You need the actual library and you need to know its entry point and supply it to QNX, using the `--lib/-l` argument.

So far, I have tested Qine only with the recent Slib from QNX 4.25.
The command-line for the 16-bit and 32-bit versions of Slib libraries looks like this:
```bash
--lib $QNX_ROOT/boot/sys/Slib32,sys,entry=0x7d0
--lib $QNX_ROOT/boot/sys/Slib16,sys16,entry=0x314
```

They have the following sha256sums if you want to verify
```
d8ba0072383eecf7ce7b52116f40e1c3430980a1cb12a691c91c4fa9316f2a4a Slib16
69e9084c8c41e33df222a0eadbe6122a62536872ebc066a32eaef74be17b83c6 Slib32
```

If you have a different QNX version, you might be able to use the Slib, even if it is not tested. To get its entry parameter, run the `Slib32` or `Slib16` as an executable and you will get an output like this:

```
Attaching name qnx/syslib
Registering shared library Slib32, offset 7d0
You can load the library for your program using:
  --lib $QNX_ROOT/boot/sys/Slib32,sys,entry=0x7d0
  ```

## Supported Features

In general, file-access is supported, QNX IPC is not. Most POSIX-y utilities should run 
(ksh, bash, ls, grep, find etc....). System utilities (sin) and other stuff using the QNX IPC is unlikely to run.

Supported:
- File access (open, read, write etc.)
- Directories, stat
- Pipes
- Signals (PIDs are different in Qine)
- Fork, exec and spawn
- Terminal (tcgetattr etc.)
- 16-bit binaries
- Binaries with relocations
- Basic segment operations, like growing

Not suported (list not complete :)
- QNX IPC, QNX File Servers
- mmap
- advanced segment operations

## Terminal support

I assume you are using terminal emulator like xterm. Most modern emulators (Konsole, Gnome-Terminal etc.)
support the xterm extensions and report themselves as xterm.

In that case, you will get things like bash or elvis running pretty succesfully, as QNX has xterm in its terminfo
database out of the box.

However, the definitions are mediocre and do not match the default xterm behavior in some cases (mouse, alt key reporting).
I suggest to use the Qine's xterm definitions `terminfo.tar.xz`, either by copying them to QNX root,
or by specifying `TERMINFO` environment variable.

In my experience, this will get you up and running with stuff like QED or VEDIT. One important thing that will not work is mouse-drag events,
so you will not be e.g. able to select text or move scrollbars fluently. To overcome this, `qine` comes
with QNX mouse reporting emulation. Specify `--term-emu` to QINE to get this behavior. It will also override the `TERM` variable
to something like `xterm-qine`, which is also supplied in our terminfo definitions.

*Note:* Modern definitions from your box cannot be used, as QNX has some "pecularities", e.g. in where it expects the mouse capabilities to go.

## Compilation
Qine is a standard CMake application. You can build it e.g using:

```
cmake -B build-dir
cmake --build build-dir
```

Python3 and python-lark is required for build.