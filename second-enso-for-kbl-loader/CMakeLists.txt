cmake_minimum_required(VERSION 3.0)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

project(enso_second C ASM)
include("${VITASDK}/share/vita.cmake" REQUIRED)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -Wall -Os -mcpu=cortex-a9 -mthumb-interwork -fno-builtin -Wno-main")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -T ${CMAKE_CURRENT_SOURCE_DIR}/second.x -nostartfiles -nodefaultlibs -nostdlib")

add_executable(enso_second
  second.c
  kbl_hen.c
  ../kbl_loader.c
)

target_link_libraries(enso_second
)

add_custom_command(TARGET enso_second
  COMMAND ${CMAKE_OBJCOPY} -S -O binary enso_second enso_second.bin
  DEPENDS enso_second
)

add_custom_target(enso_second_binary DEPENDS enso_second)
