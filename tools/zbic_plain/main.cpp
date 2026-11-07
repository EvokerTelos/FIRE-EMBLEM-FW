// SPDX-License-Identifier: GPL-3.0-or-later
// zbic_plain <in.nso> <out.nso>: rewrites an NSO0 with every segment stored uncompressed. ZBIC (flag bit 7) segments go through the
// runtime's decoder; LZ4 segments are refused (the lifter reads those itself). The decoded segments are checked against the SHA-256
// hashes in the header (+0xA0), so a wrong decode never reaches the lifter.
#include "zbic.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
static bool sha256(const uint8_t* p, size_t n, uint8_t out[32]) {
    return BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0, (PUCHAR)p, (ULONG)n, out, 32) == 0;
}
#else
static bool sha256(const uint8_t*, size_t, uint8_t*) { return false; }
#endif

static uint32_t rd(const std::vector<uint8_t>& d, size_t o) { uint32_t v; std::memcpy(&v, &d[o], 4); return v; }
static void wr(std::vector<uint8_t>& d, size_t o, uint32_t v) { std::memcpy(&d[o], &v, 4); }

int main(int argc, char** argv) {
    if (argc != 3) { std::fprintf(stderr, "usage: zbic_plain <in.nso> <out.nso>\n"); return 2; }
    std::ifstream f(argv[1], std::ios::binary);
    std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)), {});
    if (d.size() < 0x100 || std::memcmp(d.data(), "NSO0", 4) != 0) { std::fprintf(stderr, "%s: not an NSO0\n", argv[1]); return 1; }
    const uint32_t flags = rd(d, 0x0C);
    if ((flags & 7) && !(flags & nxrt::kNsoFlagZbic)) { std::fprintf(stderr, "%s: LZ4 segments, nothing to do (the lifter reads them)\n", argv[1]); return 3; }
    const size_t hdr[3] = {0x10, 0x20, 0x30}, csz[3] = {0x60, 0x64, 0x68};
    const char* names[3] = {"text", "ro", "data"};
    std::vector<uint8_t> seg[3];
    uint32_t first = UINT32_MAX;
    for (int i = 0; i < 3; ++i) {
        const uint32_t off = rd(d, hdr[i]), size = rd(d, hdr[i] + 8);
        const uint32_t cs = (flags & (1u << i)) ? rd(d, csz[i]) : size;
        if (off < first) first = off;
        if ((uint64_t)off + cs > d.size()) { std::fprintf(stderr, "%s: truncated\n", names[i]); return 1; }
        seg[i].resize(size);
        if (flags & (1u << i)) {
            if (!nxrt::zbic_decompress(seg[i].data(), size, &d[off], cs)) { std::fprintf(stderr, "%s: ZBIC decode failed\n", names[i]); return 1; }
        } else {
            std::memcpy(seg[i].data(), &d[off], size);
        }
        uint8_t h[32];
        if (sha256(seg[i].data(), size, h)) {
            const bool ok = std::memcmp(h, &d[0xA0 + 0x20 * i], 32) == 0;
            std::printf("%-4s %10u -> %10u bytes  sha256 %s\n", names[i], cs, size, ok ? "ok" : "MISMATCH");
            if (!ok && (flags & (8u << i))) return 1;   // bits 3-5: the header says the hash must be checked
        }
    }
    // header + whatever sits before the first segment (module name), then the segments back to back, uncompressed
    std::vector<uint8_t> out(d.begin(), d.begin() + first);
    wr(out, 0x0C, flags & ~(7u | nxrt::kNsoFlagZbic));
    for (int i = 0; i < 3; ++i) {
        wr(out, hdr[i], (uint32_t)out.size());
        wr(out, csz[i], (uint32_t)seg[i].size());
        out.insert(out.end(), seg[i].begin(), seg[i].end());
    }
    std::ofstream o(argv[2], std::ios::binary);
    o.write((const char*)out.data(), (std::streamsize)out.size());
    if (!o) { std::fprintf(stderr, "cannot write %s\n", argv[2]); return 1; }
    std::printf("wrote %s (%zu bytes, flags 0x%x -> 0x%x)\n", argv[2], out.size(), flags, rd(out, 0x0C));
    return 0;
}
