#!/usr/bin/env python3
"""Rename stdcall-decorated export names of the TCC-built version.dll proxy.

TCC exports `_Name@N`; the importing exe expects plain `Name`. New names are
always shorter, so each name string is rewritten in place and NUL-padded
(no pointer/ordinal tables need to change).
"""
import struct
import sys

def main(path):
    d = bytearray(open(path, 'rb').read())
    pe = struct.unpack_from('<I', d, 0x3c)[0]
    eep = struct.unpack_from('<I', d, pe + 24 + 96)[0]
    if not eep:
        print('no export directory'); return 1
    nsec = struct.unpack_from('<H', d, pe + 6)[0]
    optsz = struct.unpack_from('<H', d, pe + 20)[0]
    so = pe + 24 + optsz
    for i in range(nsec):
        vsz, va, rsz, ro = struct.unpack_from('<IIII', d, so + 8)
        if va <= eep < va + vsz:
            fo = ro + (eep - va)
            cnt = struct.unpack_from('<I', d, fo + 20)[0]
            names = struct.unpack_from('<I', d, fo + 32)[0]
            nfo = ro + (names - va)
            for j in range(cnt):
                no = struct.unpack_from('<I', d, nfo + 4 * j)[0]
                off = ro + (no - va)
                end = d.index(b'\0', off)
                old = d[off:end].decode()
                new = old.lstrip('_').split('@')[0]
                assert len(new) <= len(old), (old, new)
                d[off:off + len(new)] = new.encode()
                d[off + len(new):end] = b'\0' * (end - off - len(new))
                print('export:', old, '->', new)
        so += 40
    open(path, 'wb').write(d)
    return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv[1]))
