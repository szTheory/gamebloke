cmake_minimum_required(VERSION 3.25)
project(gamebloke_phase23_host_session LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(gamebloke CONFIG REQUIRED)

add_executable(gamebloke-phase23-host-session consumer.c)
target_link_libraries(gamebloke-phase23-host-session PRIVATE gamebloke::api)
target_compile_options(gamebloke-phase23-host-session PRIVATE
  "$<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic;-Wconversion;-Wshadow;-Wstrict-prototypes;-Wmissing-prototypes;-Wundef;-Wcast-qual>")
