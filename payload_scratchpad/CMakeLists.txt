cmake_minimum_required(VERSION 3.0)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

project(payload_scratchpad C ASM)
include("${VITASDK}/share/vita.cmake" REQUIRED)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -Wall -O2 -mcpu=cortex-a9 -mthumb-interwork -fno-builtin -Wno-main")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -T ${CMAKE_CURRENT_SOURCE_DIR}/link.x -nostartfiles -nodefaultlibs -nostdlib")

add_executable(payload_scratchpad
  reset_main.S
  reset.S
  main.c
  ../payload_boot/arm_opcode.c
)

target_link_libraries(payload_scratchpad
)

add_custom_command(TARGET payload_scratchpad
  COMMAND ${CMAKE_OBJCOPY} -S -O binary payload_scratchpad scratchpad.bin
  DEPENDS payload_scratchpad
)

add_custom_target(payload_scratchpad_binary DEPENDS payload_scratchpad)
