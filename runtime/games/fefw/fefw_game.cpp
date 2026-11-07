// SPDX-License-Identifier: GPL-3.0-or-later
// Fire Emblem: Fortune's Weave: the whole game module. Bring-up: no play-mode hooks yet, so the base runs the game as it is.
#include "fefw_game.h"

namespace nxrt::games::fefw {

const game::Info& full_info() { return core_info(); }

}  // namespace nxrt::games::fefw

void nxrt_register_game_fefw() { nxrt::game::set_current(&nxrt::games::fefw::full_info()); }
