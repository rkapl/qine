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
)

target_compile_features(qine PUBLIC cxx_std_17)
set_target_properties(qine PROPERTIES CXX_EXTENSIONS OFF)
set_target_properties(qine PROPERTIES COMPILE_FLAGS -m32)
set_target_properties(qine PROPERTIES LINK_FLAGS -m32)