// SPDX-License-Identifier: GPL-3.0-or-later
// Fire Emblem: Fortune's Weave: the whole game module. No play-mode hooks yet; per-frame pokes run from game::Info::on_present.
//
// Frame rate. The game keeps its frame-rate target in a u32 at [main+0x21E60D8]+0x1DC: 60 on the menus, 30 in gameplay (what it runs at on
// Switch 2). Its logic steps by 60 / target per presented frame, so the same target change that makes it present twice as often makes
// every frame one logic step: a real 60 fps, not a game running twice as fast. [fefw] fps_target (env NXRT_FEFW_FPS_TARGET):
//   60 (default)  the target is raised to 60 whenever the game sets a lower one
//   0             the game's own target (30 in gameplay)
// Every change of the value is logged (fefw: frame-rate target ...) with what was written.
#include <cstdint>
#include <cstring>

#include "fefw_game.h"
#include "nxrt/config.h"
#include "nxrt/log.h"
#include "nxrt/memory.h"

namespace nxrt::games::fefw {

namespace {

constexpr uint64_t kFpsHolder = 0x21E60D8;   // main offset of the pointer to the object holding the target
constexpr uint64_t kFpsField = 0x1DC;        // u32 frame-rate target in that object

void on_present() {
    static const int64_t want = config_get_int("fefw", "fps_target", 60);
    static uint32_t last_seen = 0xffffffffu;
    const uint64_t holder = core_info().main_base + kFpsHolder;
    if (!mem::is_mapped(holder, 8)) return;
    uint64_t obj = 0;
    std::memcpy(&obj, reinterpret_cast<const void*>(holder), 8);
    if (!obj || !mem::is_mapped(obj + kFpsField, 4)) return;
    uint32_t* field = reinterpret_cast<uint32_t*>(obj + kFpsField);
    const uint32_t v = *field;
    if (v != last_seen) {
        NXLOG_I("fefw", "frame-rate target %u (object %llx)%s", v, static_cast<unsigned long long>(obj),
                want > 0 && v != 0 && v < static_cast<uint32_t>(want) ? ": raised" : "");
        last_seen = v;
    }
    if (want > 0 && v != 0 && v < static_cast<uint32_t>(want) && v <= 60) {
        *field = static_cast<uint32_t>(want);
        last_seen = static_cast<uint32_t>(want);
    }
}

}  // namespace

const game::Info& full_info() {
    static const game::Info info = [] {
        game::Info g = core_info();
        g.on_present = &on_present;
        return g;
    }();
    return info;
}

}  // namespace nxrt::games::fefw

void nxrt_register_game_fefw() { nxrt::game::set_current(&nxrt::games::fefw::full_info()); }
