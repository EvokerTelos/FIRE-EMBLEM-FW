// SPDX-License-Identifier: GPL-3.0-or-later
// Texture preparation ahead of play (fefw.exe --prepare-textures; nxrt/nvn_backend.h prepare_textures). The GPU cannot sample ASTC, so every
// ASTC texture the game loads is transcoded to BC7 on its first use and kept in the disk cache ([nvn] bc7_cache, nvn_texdata.cpp); that first use
// costs a hitch (a new unit's attack: 150..450 ms frames). This walks the romfs, finds every BNTX (plain .bntx files and the ones embedded in
// bfres / ptcl / other containers, zstd-compressed or not), builds each ASTC texture the way the game's nn::gfx layer does (NVN format, target,
// size, levels, the packaged layout's block height from the BRTI) and runs the runtime's own prepare_texture on the file's bytes, so the cache key
// (a hash of the storage bytes plus the layout) and the entry are exactly what the game's first use would make. Entries are written at the high
// preset (high_preset(), bc7e "veryslow" with the ISPC build): the entry a game started with [gfx] astc_quality = high looks for first.
//
// BNTX (Switch / Switch 2, little endian): "BNTX" 0000, BOM 0xFEFF at +0x0c, file size u32 at +0x1c. BRTI (texture info, 8-aligned in the
// file): +0x16 u16 mip count, +0x1c u32 nn::gfx ImageFormat (type << 8 | kind; ASTC types 0x2d..0x3a, kind 6 = sRGB), +0x24 width, +0x28 height,
// +0x2c depth, +0x30 array length, +0x34 block height log2 (in GOBs), +0x50 image size, +0x5c u8 ImageDimension, +0x70 u64 offset (from the
// BNTX start) of the mip offset table (u64 per mip, from the BNTX start). The texture's storage in the game's memory is the BRTD data in place,
// so the bytes hashed are the file's bytes from mip 0 on.
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <zstd.h>

#include "nvn_impl.h"
#include "nvn_texdata.h"
#include "nvn_texture_layout.h"
#include "nxrt/config.h"
#include "nxrt/log.h"
#include "nxrt/nvn_backend.h"

namespace nxrt::nvn {
namespace {

constexpr const char* kTag = "nvn.texprep_ahead";

uint16_t rd16(const uint8_t* p) { uint16_t v; std::memcpy(&v, p, 2); return v; }
uint32_t rd32(const uint8_t* p) { uint32_t v; std::memcpy(&v, p, 4); return v; }
uint64_t rd64(const uint8_t* p) { uint64_t v; std::memcpy(&v, p, 8); return v; }

bool read_file(const std::filesystem::path& p, std::vector<uint8_t>& out) {
    std::ifstream f(p, std::ios::binary | std::ios::ate);
    if (!f) return false;
    const std::streamsize n = f.tellg();
    if (n <= 0) return false;
    out.resize(static_cast<size_t>(n));
    f.seekg(0);
    return static_cast<bool>(f.read(reinterpret_cast<char*>(out.data()), n));
}

// One or more zstd frames into out. False when the data is not zstd.
bool unzstd(const std::vector<uint8_t>& in, std::vector<uint8_t>& out) {
    if (in.size() < 4 || rd32(in.data()) != 0xFD2FB528u) return false;
    out.clear();
    ZSTD_DStream* ds = ZSTD_createDStream();
    ZSTD_initDStream(ds);
    ZSTD_inBuffer ib = {in.data(), in.size(), 0};
    const unsigned long long known = ZSTD_getFrameContentSize(in.data(), in.size());
    if (known != ZSTD_CONTENTSIZE_UNKNOWN && known != ZSTD_CONTENTSIZE_ERROR && known < (1ull << 33)) out.reserve(static_cast<size_t>(known));
    std::vector<uint8_t> chunk(ZSTD_DStreamOutSize());
    bool ok = true;
    while (ib.pos < ib.size) {
        ZSTD_outBuffer ob = {chunk.data(), chunk.size(), 0};
        const size_t r = ZSTD_decompressStream(ds, &ob, &ib);
        if (ZSTD_isError(r)) { ok = false; break; }
        out.insert(out.end(), chunk.data(), chunk.data() + ob.pos);
        if (r == 0 && ib.pos >= ib.size) break;
    }
    ZSTD_freeDStream(ds);
    return ok;
}

struct TexJob {
    int format = 0, target = 0, width = 0, height = 0, depth = 1, levels = 1;
    uint8_t bh_log2 = 0;
    const uint8_t* data = nullptr;
    size_t avail = 0;   // bytes of the file from mip 0 on
};

// NVN target and depth (layers for arrays) from the BRTI's ImageDimension.
bool target_of(uint8_t dim, uint32_t depth, uint32_t arr, int* target, int* d) {
    switch (dim) {
        case 1: *target = arr > 1 ? NVN_TEXTURE_TARGET_2D_ARRAY : NVN_TEXTURE_TARGET_2D; *d = arr > 1 ? static_cast<int>(arr) : 1; return true;
        case 2: *target = NVN_TEXTURE_TARGET_3D; *d = static_cast<int>(std::max(1u, depth)); return true;
        case 3: *target = NVN_TEXTURE_TARGET_CUBEMAP; *d = 6; return true;
        case 5: *target = NVN_TEXTURE_TARGET_2D_ARRAY; *d = static_cast<int>(std::max(1u, arr)); return true;
        case 8: *target = NVN_TEXTURE_TARGET_CUBEMAP_ARRAY; *d = static_cast<int>(std::max(6u, arr)); return true;
        default: return false;
    }
}

// Every ASTC texture of every BNTX in d.
void find_textures(const std::vector<uint8_t>& d, std::vector<TexJob>& out, uint64_t* bntx_count, uint64_t* tex_count) {
    const size_t n = d.size();
    for (size_t i = 0; i + 0x20 <= n; i += 8) {
        if (std::memcmp(&d[i], "BNTX", 4) != 0 || rd32(&d[i + 4]) != 0 || rd16(&d[i + 0x0c]) != 0xFEFF) continue;
        const size_t size = rd32(&d[i + 0x1c]);
        if (size < 0x40 || i + size > n) continue;
        ++*bntx_count;
        const uint8_t* b = &d[i];
        for (size_t o = 0x20; o + 0xa0 <= size; o += 8) {
            if (std::memcmp(b + o, "BRTI", 4) != 0 || rd32(b + o + 4) != rd32(b + o + 8)) continue;
            const uint8_t* t = b + o;
            ++*tex_count;
            const uint32_t gfx = rd32(t + 0x1c);
            const uint32_t type = gfx >> 8, kind = gfx & 0xff;
            if (type < 0x2d || type > 0x3a) continue;                       // not ASTC: passed through, nothing to prepare
            TexJob j;
            j.format = static_cast<int>((kind == 6 ? NVN_FORMAT_RGBA_ASTC_4x4_SRGB : NVN_FORMAT_RGBA_ASTC_4x4) + (type - 0x2d));
            j.levels = std::max<int>(1, rd16(t + 0x16));
            j.width = static_cast<int>(rd32(t + 0x24));
            j.height = static_cast<int>(rd32(t + 0x28));
            if (!target_of(t[0x5c], rd32(t + 0x2c), rd32(t + 0x30), &j.target, &j.depth)) continue;
            j.bh_log2 = static_cast<uint8_t>(rd32(t + 0x34) & 0xf);
            const uint64_t mips = rd64(t + 0x70);
            if (mips + 8 > size) continue;
            const uint64_t mip0 = rd64(b + mips);
            if (mip0 >= size) continue;
            j.data = b + mip0;
            j.avail = n - (i + static_cast<size_t>(mip0));
            if (j.width <= 0 || j.height <= 0) continue;
            out.push_back(j);
        }
        i += (size & ~size_t{7}) ? (size & ~size_t{7}) - 8 : 0;   // past this BNTX (its BRTIs are done)
    }
}

}  // namespace

bool prepare_textures(const TexturePrepareOptions& o, TexturePrepareResult* res) {
    TexturePrepareResult r;
    const auto t0 = std::chrono::steady_clock::now();
    namespace fs = std::filesystem;
    std::error_code ec;
    if (o.romfs.empty() || !fs::is_directory(o.romfs, ec)) { r.error = "no romfs folder: " + o.romfs; if (res) *res = r; return false; }
    if (config_get_str("nvn", "bc7_cache", "bc7_cache").empty()) { r.error = "[nvn] bc7_cache is empty (the cache is off)"; if (res) *res = r; return false; }
    // Entries at the high preset: with astc_quality = fast the transcode writes at bc7_quality, so bc7_quality becomes the high preset.
    const int hp = be::bc7_high_preset();
    config_set("gfx", "astc_quality", "fast");
    config_set("nvn", "bc7_quality", std::to_string(hp).c_str());
    NXLOG_I(kTag, "preparing ASTC textures from %s at bc7e preset %d into %s", o.romfs.c_str(), hp, config_get_str("nvn", "bc7_cache", "bc7_cache").c_str());

    std::vector<fs::path> files;
    for (fs::recursive_directory_iterator it(o.romfs, ec), end; it != end; it.increment(ec)) {
        if (ec) break;
        if (!it->is_regular_file(ec)) continue;
        std::string name = it->path().filename().string();
        for (char& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        const auto ends = [&](const char* s) { const size_t l = std::strlen(s); return name.size() >= l && name.compare(name.size() - l, l, s) == 0; };
        if (ends(".zst") || ends(".bntx") || ends(".bfres") || ends(".ptcl")) files.push_back(it->path());
    }
    r.files = files.size();
    const int threads = o.threads > 0 ? o.threads : std::max(1, std::min(12, static_cast<int>(std::thread::hardware_concurrency()) - 2));
    std::atomic<size_t> next{0};
    std::atomic<uint64_t> done{0}, bntx{0}, textures{0}, astc{0}, cached{0}, made{0}, failed{0};
    std::atomic<bool> cancel{false};
    std::mutex report_mu;
    auto last = std::chrono::steady_clock::now();
    auto work = [&] {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
        std::vector<uint8_t> raw, plain;
        for (;;) {
            if (cancel.load()) break;
            const size_t k = next.fetch_add(1);
            if (k >= files.size()) break;
            if (read_file(files[k], raw)) {
                const std::vector<uint8_t>* d = &raw;
                if (unzstd(raw, plain)) d = &plain;
                std::vector<TexJob> jobs;
                uint64_t nb = 0, nt = 0;
                find_textures(*d, jobs, &nb, &nt);
                bntx.fetch_add(nb);
                textures.fetch_add(nt);
                for (const TexJob& j : jobs) {
                    astc.fetch_add(1);
                    be::Texture t;
                    t.b.format = j.format;
                    t.b.target = j.target;
                    t.b.width = j.width;
                    t.b.height = j.height;
                    t.b.depth = j.depth;
                    t.b.levels = j.levels;
                    t.b.samples = 0;
                    t.b.packaged_layout[0] = j.bh_log2;
                    t.b.has_packaged_layout = true;
                    const be::TexLayout L = be::compute_texture_layout(t.b);
                    if (!L.valid || !L.fmt) { failed.fetch_add(1); continue; }
                    // the storage as the game's memory has it: the file's bytes from mip 0 on, zero past the end of the file
                    std::vector<uint8_t> padded;
                    const uint8_t* mem = j.data;
                    if (j.avail < L.storage_size) {
                        padded.assign(static_cast<size_t>(L.storage_size), 0);
                        std::memcpy(padded.data(), j.data, j.avail);
                        mem = padded.data();
                    }
                    t.fmt = L.fmt;
                    t.mem = const_cast<uint8_t*>(mem);
                    t.storage_size = L.storage_size;
                    t.block_linear = L.block_linear;
                    t.levels = L.levels;
                    be::TexCaps caps;
                    caps.astc = false;
                    caps.bc7_transcode = true;
                    caps.encode_threads = 1;
                    be::PreparedTexture out;
                    if (!be::prepare_texture(t, caps, out)) failed.fetch_add(1);
                    else if (out.from_cache) cached.fetch_add(1);
                    else made.fetch_add(1);
                    t.mem = nullptr;
                }
            }
            done.fetch_add(1);
            if (o.progress) {
                std::lock_guard<std::mutex> lk(report_mu);
                const auto now = std::chrono::steady_clock::now();
                if (now - last >= std::chrono::seconds(2) || done.load() == files.size()) {
                    last = now;
                    if (!o.progress(done.load(), files.size(), astc.load(), made.load(), cached.load())) cancel.store(true);
                }
            }
        }
    };
    std::vector<std::thread> pool;
    for (int i = 1; i < threads; ++i) pool.emplace_back(work);
    work();
    for (auto& th : pool) th.join();
    r.bntx = bntx.load(); r.textures = textures.load(); r.astc = astc.load(); r.cached = cached.load(); r.made = made.load(); r.failed = failed.load();
    r.cancelled = cancel.load();
    r.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    NXLOG_I(kTag, "files %llu, BNTX %llu, textures %llu, ASTC %llu: %llu already cached, %llu made, %llu failed, %.1f s", (unsigned long long)r.files,
            (unsigned long long)r.bntx, (unsigned long long)r.textures, (unsigned long long)r.astc, (unsigned long long)r.cached, (unsigned long long)r.made,
            (unsigned long long)r.failed, r.seconds);
    if (res) *res = r;
    return true;
}

}  // namespace nxrt::nvn
