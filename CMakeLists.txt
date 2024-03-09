cmake_minimum_required(VERSION 3.15)

project(srb)
include(CTest)

macro(make_test test_name)
  add_executable(${test_name}
    srb.h
    "tests/${test_name}.c"
  )
  set_property(TARGET ${test_name} PROPERTY C_STANDARD 17)
  set_property(TARGET ${test_name} PROPERTY EXPORT_COMPILE_COMMANDS 1)
  target_include_directories(${test_name} BEFORE PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

  if (MSVC)
    target_compile_options(${test_name} PRIVATE "/experimental:c11atomics")
  endif()

  add_test(NAME ${test_name} COMMAND ${test_name})
endmacro()

if (BUILD_TESTING)
  make_test(simple_test)
  make_test(char_test)
  make_test(multithreaded)
  add_test(NAME multithreaded_10r_1w COMMAND multithreaded 10 1)
  add_test(NAME multithreaded_1r_10w COMMAND multithreaded 1 10)
  add_test(NAME multithreaded_10r_10w COMMAND multithreaded 10 10)
endif()
