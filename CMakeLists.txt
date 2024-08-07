cmake_minimum_required(VERSION 3.19...3.27)

find_package(Python3 3.9 COMPONENTS Interpreter)
message("Using Python ${Python3_EXECUTABLE}")

project(
  quine
  VERSION 0.1
  LANGUAGES CXX)

set(MSG_TYPES io proc dev fsys common)
list(TRANSFORM MSG_TYPES PREPEND ${PROJECT_SOURCE_DIR}/msg/ OUTPUT_VARIABLE MSG_DEFS)
list(TRANSFORM MSG_DEFS APPEND .qmsg OUTPUT_VARIABLE MSG_DEFS)

list(TRANSFORM MSG_TYPES PREPEND ${PROJECT_BINARY_DIR}/gen_msg/ OUTPUT_VARIABLE MSG_GEN_PART)
list(TRANSFORM MSG_GEN_PART APPEND ".cpp" OUTPUT_VARIABLE MSG_SRC)
list(TRANSFORM MSG_GEN_PART APPEND ".h"  OUTPUT_VARIABLE MSG_H)

add_executable(qine
  src/cmd_opts.h src/cmd_opts.cpp
  src/cpp.h
  src/compiler.h
  src/intrusive_list.h
  src/qine.cpp
  src/emu.cpp src/emu.h
  src/fd_filter.h src/fd_filter.cpp
  src/fsutil.h src/fsutil.cpp
  src/guest_context.cpp src/guest_context.h
  src/loader.h src/loader.cpp src/loader_format.h
  src/log.h src/log.cpp
  src/main_handler.h src/main_handler.cpp src/term_handler.cpp
  src/msg_handler.h src/msg_handler.cpp
  src/path_mapper.h src/path_mapper.cpp
  src/process.h src/process.cpp
  src/qnx_fd.h src/qnx_fd.cpp
  src/qnx_pid.h src/qnx_pid.cpp
  src/qnx_sigset.h src/qnx_sigset.cpp
  src/segment.h src/segment.cpp
  src/segment_descriptor.h src/segment_descriptor.cpp
  src/termios_settings.h src/termios_settings.cpp
  src/timespec.h src/timespec.cpp
  src/msg.h src/msg.cpp
  src/util.h src/util.cpp
  src/msg/meta.h src/msg/meta.cpp
  src/msg/dump.h src/msg/dump.cpp
  ${MSG_SRC} ${MSG_H}
)

target_include_directories(qine PRIVATE ${PROJECT_BINARY_DIR} ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(qine PUBLIC -lrt)

# We cannot access fs: and similar registers until full host context is restored
set_property(SOURCE src/emu.cpp APPEND PROPERTY COMPILE_FLAGS -fno-stack-protector)

add_custom_command(
  OUTPUT ${MSG_SRC} ${MSG_H}
  COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/msg/gen.py -d ${PROJECT_BINARY_DIR}/gen_msg ${MSG_DEFS}
  DEPENDS ${MSG_DEFS} msg/gen.py msg/qmsg.lark
)

target_compile_features(qine PUBLIC cxx_std_17)
set_target_properties(qine PROPERTIES CXX_EXTENSIONS OFF)
# We are runnign on x86 and it make sense for accessing msg member
set_target_properties(qine PROPERTIES COMPILE_FLAGS -Wno-address-of-packed-member)
#set_target_properties(qine PROPERTIES LINK_FLAGS -m32)

install(TARGETS qine)