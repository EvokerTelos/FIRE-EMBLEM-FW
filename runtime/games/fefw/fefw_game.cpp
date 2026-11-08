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
#include <string>

#include "fefw_game.h"
#include "nxrt/config.h"
#include "nxrt/log.h"
#include "nxrt/memory.h"

namespace nxrt::games::fefw {

namespace {

constexpr uint64_t kFpsHolder = 0x21E60D8;   // main offset of the pointer to the object holding the target
constexpr uint64_t kFpsField = 0x1DC;        // u32 frame-rate target in that object

// Render resolution. The renderer takes its sizes from one of four resolution profiles (f_7101818350 picks one by mode; 4 entries of
// {object*, refcount*} at main+0x223e0b0). A profile starts with {output w, h, render w, h, shadow map size, ...}: the scene renders at the
// render size and the game's DLSS step scales it to the output size, then a last pass scales that to the display (3840x2160 here). The
// stock profiles render at a quarter of the output's pixels: 2560x1440 <- 1280x720 (the one in use), 2048x1152 <- 1024x576,
// 1706x960 <- 853x480, 1920x1080 <- 960x540; the host has no DLSS, so that step is a bilinear scale and the picture is soft.
// [fefw] render_width / render_height (env NXRT_FEFW_RENDER_WIDTH / _HEIGHT), default 3840x2160: both the output and the render size of
// every profile become that size (native, the DLSS step becomes a copy); 0 keeps the game's profiles. [fefw] shadow_size (default 4096,
// 0 = the game's) sets the shadow map size of the profiles. The renderer copies the profile into its pending sizes ([main+0x21E60D8] +
// 0x3408 render w, h, + 0x3410 output w, h); those are set too when they still hold the stock values, so a profile chosen before the
// first present takes the new size as well. The game's render-target pool (1008 MB, sized for the stock profile) cannot hold 3840x2160 with
// 4096 shadows: recomp/games/fefw/main.sites patches its size to 3056 MB (0xbf000000); with the stock lift 2560x1440 and shadow_size = 0 is
// what fits. Measured on an RX 9070 XT: 3840x2160 + 4096 shadows 54..58 fps in the tutorial battle map (GPU about 92% busy).
constexpr uint64_t kProfiles = 0x223e0b0;
struct Res { uint32_t ow, oh, rw, rh; };
constexpr Res kStock[4] = {{2560, 1440, 1280, 720}, {2048, 1152, 1024, 576}, {1706, 960, 853, 480}, {1920, 1080, 960, 540}};

void apply_resolution(uint64_t renderer) {
    static const uint32_t w = static_cast<uint32_t>(config_get_int("fefw", "render_width", 3840));
    static const uint32_t h = static_cast<uint32_t>(config_get_int("fefw", "render_height", 2160));
    static const uint32_t shadow = static_cast<uint32_t>(config_get_int("fefw", "shadow_size", 4096));
    if (!w || !h) return;
    static bool done[4] = {};
    for (int k = 0; k < 4; ++k) {
        if (done[k]) continue;
        uint64_t obj = 0;
        std::memcpy(&obj, reinterpret_cast<const void*>(core_info().main_base + kProfiles + 16ull * k), 8);
        if (!obj || !mem::is_mapped(obj, 20)) continue;
        uint32_t* p = reinterpret_cast<uint32_t*>(obj);
        if (p[0] != kStock[k].ow || p[1] != kStock[k].oh || p[2] != kStock[k].rw || p[3] != kStock[k].rh) {
            NXLOG_W("fefw", "resolution profile %d holds %ux%u <- %ux%u, not the stock values: left alone", k, p[0], p[1], p[2], p[3]);
            done[k] = true;
            continue;
        }
        NXLOG_I("fefw", "resolution profile %d: %ux%u <- %ux%u becomes %ux%u <- %ux%u%s", k, p[0], p[1], p[2], p[3], w, h, w, h,
                shadow ? (", shadow map " + std::to_string(p[4]) + " -> " + std::to_string(shadow)).c_str() : "");
        p[0] = w; p[1] = h; p[2] = w; p[3] = h;
        if (shadow) p[4] = shadow;
        done[k] = true;
    }
    if (renderer && mem::is_mapped(renderer + 0x3408, 16)) {
        uint32_t* q = reinterpret_cast<uint32_t*>(renderer + 0x3408);   // render w, h, output w, h
        for (const Res& r : kStock)
            if (q[0] == r.rw && q[1] == r.rh && q[2] == r.ow && q[3] == r.oh) {
                NXLOG_I("fefw", "renderer pending sizes %ux%u <- %ux%u become %ux%u", q[2], q[3], q[0], q[1], w, h);
                q[0] = w; q[1] = h; q[2] = w; q[3] = h;
                break;
            }
    }
}

void on_present() {
    {
        uint64_t renderer = 0;
        if (mem::is_mapped(core_info().main_base + kFpsHolder, 8)) std::memcpy(&renderer, reinterpret_cast<const void*>(core_info().main_base + kFpsHolder), 8);
        apply_resolution(renderer);
    }
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
