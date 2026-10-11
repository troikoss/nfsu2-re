cmake_minimum_required(VERSION 3.16)
project(nfsu2re CXX)

# Native 32-bit analysis/demo build. Recovered sources keep the original PC x86 layouts,
# so this target must be built as i386 code without position independence.
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE OFF)
set(UG2_ARCH_FLAGS -m32 -fno-pie -mfpmath=387 -fno-strict-aliasing -fno-stack-protector)

file(GLOB UG2_RECOVERED_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/Speed/Indep/Src/Physics/*.cpp
    ${CMAKE_SOURCE_DIR}/src/Speed/Indep/Src/World/*.cpp
    ${CMAKE_SOURCE_DIR}/src/Speed/Indep/Libs/bWare/Strings.cpp)
include_directories(SYSTEM ${CMAKE_SOURCE_DIR}/src/Platform/Native/include)
add_library(ug2recovered STATIC ${UG2_RECOVERED_SOURCES})
target_compile_options(ug2recovered PRIVATE ${UG2_ARCH_FLAGS} -O1 -g -w
    -include ${CMAKE_SOURCE_DIR}/src/Platform/Native/NativeCompat.h)

add_library(ug2native STATIC
    src/Platform/Native/OriginalImage.cpp
    src/Platform/Native/NativeVehicle.cpp
    src/Platform/Native/NativeWorld.cpp)
target_compile_options(ug2native PRIVATE ${UG2_ARCH_FLAGS} -O2 -Wall
    -include ${CMAKE_SOURCE_DIR}/src/Platform/Native/NativeCompat.h)

include_directories(SYSTEM ${CMAKE_SOURCE_DIR}/src/Platform/Native/include)
set(UG2_LINK_FLAGS -m32 -no-pie ${CMAKE_SOURCE_DIR}/src/Platform/Native/OriginalSymbols.ld)

add_executable(ug2sim src/Platform/Native/SimMain.cpp)
target_compile_options(ug2sim PRIVATE ${UG2_ARCH_FLAGS} -O2 -Wall
    -include ${CMAKE_SOURCE_DIR}/src/Platform/Native/NativeCompat.h)
target_link_libraries(ug2sim -Wl,--start-group ug2native ug2recovered -Wl,--end-group m)
target_link_options(ug2sim PRIVATE ${UG2_LINK_FLAGS})

add_executable(ug2demo src/Platform/Native/DemoMain.cpp)
target_compile_options(ug2demo PRIVATE ${UG2_ARCH_FLAGS} -O2 -Wall -D_REENTRANT
    -include ${CMAKE_SOURCE_DIR}/src/Platform/Native/NativeCompat.h)
target_include_directories(ug2demo BEFORE PRIVATE ${CMAKE_SOURCE_DIR}/src/Platform/Native/include /usr/include/SDL2)
target_link_directories(ug2demo PRIVATE /usr/lib/i386-linux-gnu)
target_link_libraries(ug2demo -Wl,--start-group ug2native ug2recovered -Wl,--end-group m -l:libSDL2-2.0.so.0 -l:libGL.so.1)
target_link_options(ug2demo PRIVATE ${UG2_LINK_FLAGS})

add_executable(ug2probe src/Platform/Native/CollisionProbe.cpp)
target_compile_options(ug2probe PRIVATE ${UG2_ARCH_FLAGS} -O1 -g -w
    -include ${CMAKE_SOURCE_DIR}/src/Platform/Native/NativeCompat.h)
target_link_libraries(ug2probe -Wl,--start-group ug2native ug2recovered -Wl,--end-group m)
target_link_options(ug2probe PRIVATE ${UG2_LINK_FLAGS})
