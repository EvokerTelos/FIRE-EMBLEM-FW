"""programs_bin.py <programs.tsv> <programs.bin>: the program list the shader preparation reads (fefw.exe --prepare-shaders).

programs.tsv is extract_corpus.py's list of BNSH programs ("vs=<h16> ps=<h16> [gs=<h16>]", the hashes the runtime's shader cache uses);
compute-only programs are left out (they have no graphics pipeline parts). Output: "MPRPRG01", u32 count, 4 bytes zero, then count records of
three u64 (vertex, fragment, geometry; 0 when the stage is absent), distinct programs in first-seen order."""
import struct
import sys


def main(src, dst):
    seen, out = set(), []
    with open(src, encoding='utf-8') as f:
        next(f)
        for line in f:
            parts = line.rstrip('\n').split('\t')
            if len(parts) < 4:
                continue
            st = dict(s.split('=', 1) for s in parts[3].split() if '=' in s)
            if 'vs' not in st or 'ps' not in st:
                continue
            key = (int(st['vs'], 16), int(st['ps'], 16), int(st.get('gs', '0'), 16))
            if key not in seen:
                seen.add(key)
                out.append(key)
    with open(dst, 'wb') as f:
        f.write(b'MPRPRG01' + struct.pack('<II', len(out), 0))
        for k in out:
            f.write(struct.pack('<QQQ', *k))
    print(f'{len(out)} programs -> {dst}')


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
