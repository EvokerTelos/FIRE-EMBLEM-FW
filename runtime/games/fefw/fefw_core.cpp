// SPDX-License-Identifier: GPL-3.0-or-later
// Fire Emblem: Fortune's Weave, the core part of the game module (fefw_game.h): the data part plus the title facts.
#include "fefw_game.h"

#include "nxrt/game.h"
#include "nxrt/services.h"

namespace nxrt::games::fefw {

namespace {

// main.npdm of the v131072 update (SDK 22.2.0); save sizes are placeholders until the boot log shows what the game asks for.
void title_defaults(services::TitleProfile& p) {
    p.program_id = data_info().program_id;
    p.name = data_info().title;
    p.display_version = data_info().version;
    p.supported_languages = data_info().languages;
    p.account_save_size = 0x4000000;
    p.account_save_journal = 0x4000000;
    p.device_save_size = 0x4000000;
    p.device_save_journal = 0x4000000;
    p.main_thread_stack = 0x100000;
    p.main_thread_priority = 44;
    p.main_thread_core = 0;
    p.firmware = "22.2.0";
    p.user_nickname = "Player";
    p.user_uid_hi = 0x0000000000000001ull;
    p.user_uid_lo = 0x5745465752656370ull;     // "pceRWFEW"
}

}  // namespace

const game::Info& core_info() {
    static const game::Info info = [] {
        game::Info g = data_info();
        g.title_defaults = &title_defaults;
        return g;
    }();
    return info;
}

}  // namespace nxrt::games::fefw

void nxrt_register_game_fefw_core() { nxrt::game::set_current(&nxrt::games::fefw::core_info()); }
