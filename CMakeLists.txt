cmake_minimum_required(VERSION 3.20)
project(estepa C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Version: VERSION (semver, para humanos) + codigo de build (entero que siempre
# crece: minutos desde 1970). Android exige que el codigo aumente en cada
# actualizacion; asi ocurre solo, en CI y en local, sin tocar nada a mano.
file(STRINGS "${CMAKE_SOURCE_DIR}/VERSION" ESTEPA_VERSION LIMIT_COUNT 1)
if(NOT DEFINED ESTEPA_BUILD_CODE)
  string(TIMESTAMP _estepa_now "%s" UTC)
  math(EXPR ESTEPA_BUILD_CODE "${_estepa_now} / 60")
endif()
message(STATUS "Estepa ${ESTEPA_VERSION} (build ${ESTEPA_BUILD_CODE})")

option(ESTEPA_BUILD_GAME "Compilar el juego (requiere raylib)" ON)
option(ESTEPA_BUILD_TESTS "Compilar los tests del nucleo" ON)

if(MSVC)
  set(ESTEPA_WARNINGS /W4 /utf-8) # textos del juego en UTF-8 (acentos, ñ)
else()
  set(ESTEPA_WARNINGS -Wall -Wextra -Wpedantic)
endif()

# Nucleo de simulacion: C puro, sin raylib. Se testea sin pantalla.
add_library(estepa_sim STATIC
  src/sim/noise.c
  src/sim/troop.c
  src/sim/loadout.c
  src/sim/clock.c
  src/sim/climate.c
  src/sim/hazards.c
  src/sim/health.c
  src/sim/combat.c
  src/sim/body.c
  src/sim/armor.c
  src/sim/ballistics.c
  src/sim/memory_map.c
  src/sim/champion.c
  src/sim/inventory.c
  src/sim/actions.c
  src/sim/economy.c
  src/sim/animals.c
  src/sim/swarms.c
  src/sim/fire.c
  src/sim/melee.c
  src/sim/anim_index.c)
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
  target_compile_definitions(sim_tests PRIVATE ESTEPA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
  add_test(NAME sim_tests COMMAND sim_tests)
endif()

if(ESTEPA_BUILD_GAME)
  include(FetchContent)
  if(ANDROID)
    # Con la toolchain del NDK: raylib para Android (OpenGL ES 2).
    set(PLATFORM "Android" CACHE STRING "" FORCE)
  endif()
  set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(raylib)

  set(ESTEPA_GAME_SOURCES
    src/main.c
    src/platform.c
    src/game/player.c
    src/game/actions_game.c
    src/game/hazards_game.c
    src/game/combat_game.c
    src/game/fauna_game.c
    src/game/disasters_game.c
    src/world/props.c
    src/ui/theme.c
    src/ui/minimap.c
    src/world/terrain.c
    src/world/camp.c
    src/world/gallery.c
    src/world/sky.c
    src/world/weather.c
    src/world/body_draw.c)

  if(ANDROID)
    # Android: biblioteca compartida que carga NativeActivity (libestepa.so).
    # tools/android/build_apk.sh la empaqueta con los assets en el APK.
    add_library(estepa SHARED ${ESTEPA_GAME_SOURCES})
    target_include_directories(estepa PRIVATE ${ANDROID_NDK}/sources/android/native_app_glue)
    # Mantiene el punto de entrada que busca NativeActivity.
    target_link_options(estepa PRIVATE -u ANativeActivity_onCreate)
  else()
    add_executable(estepa ${ESTEPA_GAME_SOURCES})
    # Copia los assets junto al ejecutable.
    add_custom_command(TARGET estepa POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:estepa>/assets)
  endif()
  target_link_libraries(estepa PRIVATE estepa_sim raylib)
  target_compile_options(estepa PRIVATE ${ESTEPA_WARNINGS})
  target_compile_definitions(estepa PRIVATE
    ESTEPA_VERSION="${ESTEPA_VERSION}" ESTEPA_BUILD_CODE=${ESTEPA_BUILD_CODE})
endif()
