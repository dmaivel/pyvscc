cmake_minimum_required(VERSION 3.5.0)
project(vscc VERSION 0.1.0)

include_directories(${vscc_SOURCE_DIR}/include)
include_directories(${vscc_SOURCE_DIR}/vscc/include)

file(GLOB VSCC_GLOB_SRC CONFIGURE_DEPENDS "vscc/src/util/*.c" "vscc/src/ir/*.c" "vscc/src/asm/*.c" "vscc/src/asm/codegen/*.c" "vscc/src/opt/*.c")

add_library(vscc SHARED ${VSCC_GLOB_SRC})
set_target_properties(vscc PROPERTIES PUBLIC_HEADER vscc/include/vscc.h)
set_target_properties(vscc PROPERTIES C_STANDARD 99)

add_executable(pyvscc src/main.c src/lexer.c src/util.c src/pybuild.c src/pyimpl.c)
target_link_libraries(pyvscc vscc)
