# SPDX-License-Identifier: GPL-3.0-or-later
# zbic_plain: ZBIC-compressed NSO (flag bit 7) -> plain NSO the lifter reads. Stand-in for nx_extract --plain-nso, whose readers
# (tools/setup) are not in this repository. Uses the runtime's ZBIC decoder (runtime/src/loader/zbic.cpp).
cmake_minimum_required(VERSION 3.20)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(zbic_plain CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(RT ${CMAKE_CURRENT_SOURCE_DIR}/../../runtime)
add_executable(zbic_plain main.cpp ${RT}/src/loader/zbic.cpp)
target_include_directories(zbic_plain PRIVATE ${RT}/src/loader)
target_compile_definitions(zbic_plain PRIVATE _CRT_SECURE_NO_WARNINGS)
if(WIN32)
    target_link_libraries(zbic_plain PRIVATE bcrypt)
endif()
