cmake_minimum_required(VERSION 3.15)

project(srb)

add_executable(srb_test
  test.c
  srb.h
)
set_property(TARGET srb_test PROPERTY C_STANDARD 17)
set_property(TARGET srb_test PROPERTY EXPORT_COMPILE_COMMANDS 1)

if (MSVC)
  target_compile_options(srb_test PRIVATE "/experimental:c11atomics")
endif()
