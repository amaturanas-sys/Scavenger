cmake_minimum_required(VERSION 3.20)
project(estepa C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(ESTEPA_BUILD_GAME "Compilar el juego (requiere raylib)" ON)
option(ESTEPA_BUILD_TESTS "Compilar los tests del nucleo" ON)

if(MSVC)
  set(ESTEPA_WARNINGS /W4)
else()
  set(ESTEPA_WARNINGS -Wall -Wextra -Wpedantic)
endif()

# Nucleo de simulacion: C puro, sin raylib. Se testea sin pantalla.
add_library(estepa_sim STATIC
  src/sim/noise.c
  src/sim/troop.c
  src/sim/loadout.c)
target_include_directories(estepa_sim PUBLIC src)
target_compile_options(estepa_sim PRIVATE ${ESTEPA_WARNINGS})
if(NOT MSVC)
  target_link_libraries(estepa_sim PUBLIC m)
endif()

if(ESTEPA_BUILD_TESTS)
  enable_testing()
  add_executable(sim_tests tests/test_main.c)
  target_link_libraries(sim_tests PRIVATE estepa_sim)
  target_compile_options(sim_tests PRIVATE ${ESTEPA_WARNINGS})
  add_test(NAME sim_tests COMMAND sim_tests)
endif()

if(ESTEPA_BUILD_GAME)
  include(FetchContent)
  set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(raylib)

  add_executable(estepa
    src/main.c
    src/game/player.c
    src/world/terrain.c
    src/world/camp.c)
  target_link_libraries(estepa PRIVATE estepa_sim raylib)
  target_compile_options(estepa PRIVATE ${ESTEPA_WARNINGS})

  # Copia los assets junto al ejecutable.
  add_custom_command(TARGET estepa POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:estepa>/assets)
endif()
