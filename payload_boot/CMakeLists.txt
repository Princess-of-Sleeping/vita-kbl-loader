cmake_minimum_required(VERSION 3.0)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

project(payload_boot C ASM)
include("${VITASDK}/share/vita.cmake" REQUIRED)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -Wall -O2 -mcpu=cortex-a9 -mthumb-interwork -fno-builtin -Wno-main -DFW_360")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -T ${CMAKE_CURRENT_SOURCE_DIR}/link.x -nostartfiles -nodefaultlibs -nostdlib")

add_executable(payload_boot
  reset.s
  main.c
  arm_opcode.c
  hen.c
  enso/lowio.c
  enso/syscon.c
)

target_link_libraries(payload_boot
)

add_custom_command(TARGET payload_boot
  COMMAND ${CMAKE_OBJCOPY} -S -O binary payload_boot boot.bin
  DEPENDS payload_boot
)

add_custom_target(payload_boot_binary DEPENDS payload_boot)
