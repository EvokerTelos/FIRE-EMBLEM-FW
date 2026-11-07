// SPDX-License-Identifier: GPL-3.0-or-later
// The Fire Emblem: Fortune's Weave game module (docs/GAME_BOUNDARY.md): what the game-independent base needs to run this title, as one
// nxrt::game::Info. Libraries (games/fefw/CMakeLists.txt), each a superset of the one before:
//   nxrt_game_fefw_data   identity, branding (pure data)          the launcher           nxrt_register_game_fefw_data()
//   nxrt_game_fefw_core   + title facts (needs nxrt)              the service tests      nxrt_register_game_fefw_core()
//   nxrt_game_fefw        + play-mode hooks (needs nxrt_gfx)      fefw.exe               nxrt_register_game_fefw()
// The game exe (game-build/src/fefw_main.cpp) calls nxrt_register_game_fefw() before nxrt::init().
#pragma once

#include "nxrt/game.h"

void nxrt_register_game_fefw();
void nxrt_register_game_fefw_core();
void nxrt_register_game_fefw_data();

namespace nxrt::games::fefw {
const game::Info& data_info();     // fefw_data.cpp
const game::Info& core_info();     // fefw_core.cpp
const game::Info& full_info();     // fefw_game.cpp
}  // namespace nxrt::games::fefw
