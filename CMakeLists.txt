cmake_minimum_required(VERSION 3.14)
project(trs-tools LANGUAGES C)

add_executable(trs-tools)
set_target_properties(trs-tools PROPERTIES C_STANDARD 99)

target_sources(trs-tools
  PRIVATE
  src/main.c
  src/trs.c)

target_link_libraries(trs-tools PRIVATE m)

target_compile_options(trs-tools PRIVATE
  $<$<C_COMPILER_ID:AppleClang,Clang,GNU>:
  -Wall
  -Wlogical-not-parentheses
  -Wno-implicit-fallthrough
  -Wnull-dereference
  -Wshadow
  -Wunused-const-variable
  -Wunused-parameter
  -Wwrite-strings
  -s
  -flto
  -pedantic>)
