cmake_minimum_required(VERSION 3.15)

project(srb)

add_executable(srb_test
  test.c
  srb.h
)
set_property(TARGET srb_test PROPERTY CXX_STANDARD 20)
set_property(TARGET srb_test PROPERTY EXPORT_COMPILE_COMMANDS 1)
