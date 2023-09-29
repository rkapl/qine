cmake_minimum_required(VERSION 3.1...3.27)

project(
  quine
  VERSION 0.1
  LANGUAGES CXX)

add_executable(qine
  src/cpp.h
  src/qine.cpp
  src/loader.h src/loader.cpp
  src/process.h src/process.cpp
  src/segment.h src/segment.cpp
)

target_compile_features(qine PUBLIC cxx_std_20)
set_target_properties(qine PROPERTIES CXX_EXTENSIONS OFF)
set_target_properties(qine PROPERTIES COMPILE_FLAGS -m32)
set_target_properties(qine PROPERTIES LINK_FLAGS -m32)