# SPDX-License-Identifier: GPL-3.0-or-later
# The Fire Emblem: Fortune's Weave game module (docs/GAME_BOUNDARY.md, fefw_game.h). Libraries, each a superset of the one before:
#   nxrt_game_fefw_data   identity, branding (pure data; the launcher links it)
#   nxrt_game_fefw_core   + title facts, test probes (links nxrt)
#   nxrt_game_fefw        + play-mode hooks (links nxrt_gfx; only with NXRT_BUILD_GFX). fefw.exe links this one.
set(_fefw "${CMAKE_CURRENT_SOURCE_DIR}")
set(_rt "${PROJECT_SOURCE_DIR}")
# Identity and file names come from the game manifest (recomp/games/fefw/game.toml) through a generated header (fefw_data.cpp).
get_filename_component(_fefw_recomp "${_rt}/../recomp" ABSOLUTE)
include("${_fefw_recomp}/cmake/GameManifest.cmake")
game_manifest_load("${_fefw_recomp}/games/fefw/game.toml")
set(_fefw_gen "${CMAKE_CURRENT_BINARY_DIR}/gen")
game_manifest_header("${_fefw_gen}/game_manifest.h")
set(NXRT_GAME_FEFW_INCLUDES "${_rt}/include" "${_rt}/games" "${_fefw}" "${_rt}/src/gfx" "${_rt}/src/gfx/play" "${_rt}/src/testtools")

add_library(nxrt_game_fefw_data STATIC "${_fefw}/fefw_data.cpp")
target_include_directories(nxrt_game_fefw_data PUBLIC ${NXRT_GAME_FEFW_INCLUDES})
target_include_directories(nxrt_game_fefw_data PRIVATE "${_fefw_gen}")
target_compile_features(nxrt_game_fefw_data PUBLIC cxx_std_20)

add_library(nxrt_game_fefw_core STATIC "${_fefw}/fefw_core.cpp")
target_link_libraries(nxrt_game_fefw_core PUBLIC nxrt_game_fefw_data nxrt)

if(TARGET nxrt_gfx)
    add_library(nxrt_game_fefw STATIC "${_fefw}/fefw_game.cpp")
    target_link_libraries(nxrt_game_fefw PUBLIC nxrt_game_fefw_core nxrt_gfx)
endif()
