cmake_minimum_required(VERSION 3.1...3.27)

project(
  quine
  VERSION 0.1
  LANGUAGES CXX)

add_executable(qine
  src/cpp.h
  src/compiler.h
  src/intrusive_list.h
  src/qine.cpp
  src/emu.cpp src/emu.h
  src/context.cpp src/context.h
  src/loader.h src/loader.cpp src/loader_format.h
  src/process.h src/process.cpp
  src/segment.h src/segment.cpp
  src/segment_descriptor.h src/segment_descriptor.cpp
  src/msg.h src/msg.cpp
  src/msg/meta.h src/msg/meta.cpp
  src/msg/dump.h src/msg/dump.cpp
  ${PROJECT_BINARY_DIR}/gen_msg/proc.h ${PROJECT_BINARY_DIR}/gen_msg/proc.cpp
  ${PROJECT_BINARY_DIR}/gen_msg/io.h ${PROJECT_BINARY_DIR}/gen_msg/io.cpp
)

target_include_directories(qine PRIVATE ${PROJECT_BINARY_DIR} ${PROJECT_SOURCE_DIR}/src)

add_custom_command(
  OUTPUT ${PROJECT_BINARY_DIR}/gen_msg/proc.cpp ${PROJECT_BINARY_DIR}/gen_msg/proc.h
  COMMAND ${PROJECT_SOURCE_DIR}/gen_msg_meta.py -d ${PROJECT_BINARY_DIR}/gen_msg -n proc  ${PROJECT_SOURCE_DIR}/src/msg/proc.json
  DEPENDS ${PROJECT_SOURCE_DIR}/src/msg/proc.json
)

add_custom_command(
  OUTPUT ${PROJECT_BINARY_DIR}/gen_msg/io.cpp ${PROJECT_BINARY_DIR}/gen_msg/io.h
  COMMAND ${PROJECT_SOURCE_DIR}/gen_msg_meta.py -d ${PROJECT_BINARY_DIR}/gen_msg -n io  ${PROJECT_SOURCE_DIR}/src/msg/io.json
  DEPENDS ${PROJECT_SOURCE_DIR}/src/msg/io.json
)

target_compile_features(qine PUBLIC cxx_std_17)
set_target_properties(qine PROPERTIES CXX_EXTENSIONS OFF)
#set_target_properties(qine PROPERTIES COMPILE_FLAGS -m32)
#set_target_properties(qine PROPERTIES LINK_FLAGS -m32)