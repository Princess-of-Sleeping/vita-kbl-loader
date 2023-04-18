cmake_minimum_required(VERSION 3.0)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

project(kbl-loader)
set(SELF_NAME "kbl-loader")
include("${VITASDK}/share/vita.cmake" REQUIRED)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -Wall -O2 -fno-inline -fno-builtin")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions")

enable_language(C ASM)

add_custom_command(OUTPUT payload_scratchpad.c
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/../payload_scratchpad/scratchpad.bin
  COMMAND file2hex4c -raw=false -vname=scratchpad_data -has_size=true -writable=false -file=${CMAKE_CURRENT_BINARY_DIR}/../payload_scratchpad/scratchpad.bin > payload_scratchpad.c
)

add_custom_command(OUTPUT payload_boot.c
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/../payload_boot/boot.bin
  COMMAND file2hex4c -raw=false -vname=boot_data -has_size=true -writable=false -file=${CMAKE_CURRENT_BINARY_DIR}/../payload_boot/boot.bin > payload_boot.c
)

add_executable(${PROJECT_NAME}
  src/main.c
  src/resume.s
  src/corelock.s
  payload_scratchpad.c
  payload_boot.c
)

add_dependencies(${PROJECT_NAME} payload_scratchpad_binary)
add_dependencies(${PROJECT_NAME} payload_boot_binary)

target_link_libraries(${PROJECT_NAME}
  SceCpuForDriver_stub
  # SceCpuForKernel_stub
  SceDebugForDriver_stub
  SceDmacmgrForDriver_stub
  SceIofilemgrForDriver_stub
  SceSblAIMgrForDriver_stub
  SceSysrootForKernel_stub
  SceSysclibForDriver_stub
  SceSysmemForDriver_stub
  ScePowerForDriver_stub
  SceThreadmgrForDriver_stub
  SceKernelUtilsForDriver_stub
  taihenForKernel_stub
)

set_target_properties(${PROJECT_NAME}
  PROPERTIES LINK_FLAGS "-nostdlib"
  COMPILE_FLAGS "-D__PSP2_KERNEL__"
)

vita_create_self(${SELF_NAME}.skprx ${PROJECT_NAME}
  CONFIG kbl-loader.yml
  UNSAFE
  STRIPPED
  REL_OPTIMIZE
)
