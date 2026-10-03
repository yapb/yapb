#!/usr/bin/env python3
#
# YaPB, started from PODBot by Count Floyd
# Maintained by YaPB Team <yapb@jeefo.net>
#
# SPDX-License-Identifier: Unlicense
#
# Verifies the binary contract of a built yapb Windows DLL (counterpart of
# check_abi.py for the ELF/Linux side):
#
#   - PE machine matches the requested arch
#   - the module exports the engine/metamod entry points
#   - imported DLLs stay inside the allowed system set (UCRT aware: the
#     dynamic api-ms-win-crt-*/ucrtbase and the static-CRT kernel32/bcrypt
#     paths are both accepted, debug CRT is not)
#
# Accepts a file or a directory (then checks every yapb*.dll except amxx).
# Pure stdlib: parses PE directly, no pefile needed.
#
#   python3 tools/check_dll.py windows-x86 --arch i386
#   python3 tools/check_dll.py windows-amd64/yapb_amd64.dll --arch amd64
#

import argparse
import glob
import os
import struct
import sys

IMAGE_FILE_MACHINE_I386, IMAGE_FILE_MACHINE_AMD64 = 0x14c, 0x8664
IMAGE_DIRECTORY_ENTRY_EXPORT, IMAGE_DIRECTORY_ENTRY_IMPORT = 0, 1

MACHINES = {
    'i386': (IMAGE_FILE_MACHINE_I386, 'Intel 80386'),
    'amd64': (IMAGE_FILE_MACHINE_AMD64, 'AMD x86-64'),
}

# HLDS / Metamod entry points the engine resolves by name
REQUIRED_EXPORTS = frozenset({
    'GetBotAPI',
    'GetEntityAPI',
    'GetNewDLLFunctions',
    'GiveFnptrsToDll',
    'Meta_Attach',
    'Meta_Detach',
    'Meta_Init',
    'Meta_Query',
    'Server_GetBlendingInterface',
    'Server_GetPhysicsInterface',
})

# system DLLs a yapb module may depend on. ucrtbase/vcruntime are the
# dynamic UCRT, msvcrt is the downlevel CRT used by the winxp toolset, and
# the api-ms-win-* api-sets forward to them. debug CRT (e.g. ucrtbased,
# vcruntime140d) is deliberately absent, so it is flagged.
ALLOWED_IMPORTS = frozenset({
    'kernel32.dll',
    'bcrypt.dll',
    'ws2_32.dll',
    'user32.dll',
    'advapi32.dll',
    'gdi32.dll',
    'shell32.dll',
    'ole32.dll',
    'oleaut32.dll',
    'crypt32.dll',
    'winmm.dll',
    'version.dll',
    'msvcrt.dll',
    'ucrtbase.dll',
    'vcruntime140.dll',
    'vcruntime140_1.dll',
    'msvcp140.dll',
})
ALLOWED_IMPORT_PREFIXES = ('api-ms-win-crt-', 'api-ms-win-core-')


class Pe(object):
    def __init__(self, data):
        self.data = data
        if data[:2] != b'MZ':
            raise ValueError('not a PE file (missing MZ)')

        self.pe_off = struct.unpack_from('<I', data, 0x3c)[0]
        if data[self.pe_off:self.pe_off + 4] != b'PE\x00\x00':
            raise ValueError('not a PE file (missing PE signature)')

        coff = self.pe_off + 4
        self.machine = struct.unpack_from('<H', data, coff)[0]
        self.num_sections = struct.unpack_from('<H', data, coff + 2)[0]
        opt_size = struct.unpack_from('<H', data, coff + 16)[0]
        opt = coff + 20

        magic = struct.unpack_from('<H', data, opt)[0]
        if magic == 0x10b:
            self.is64 = False
            dd_off = opt + 96
        elif magic == 0x20b:
            self.is64 = True
            dd_off = opt + 112
        else:
            raise ValueError('unsupported optional header magic 0x%x' % magic)

        self.dirs = []
        for i in range(2):
            self.dirs.append(struct.unpack_from('<II', data, dd_off + i * 8))

        self.sections = []
        sec_off = opt + opt_size
        for i in range(self.num_sections):
            off = sec_off + i * 40
            vsize, vaddr, rawsize, rawptr = struct.unpack_from('<IIII', data, off + 8)
            self.sections.append((vaddr, vsize, rawptr, rawsize))

    def rva_to_off(self, rva):
        for vaddr, vsize, rawptr, rawsize in self.sections:
            if vaddr <= rva < vaddr + max(vsize, rawsize):
                return rawptr + (rva - vaddr)
        return None

    def _cstr(self, off):
        end = self.data.find(b'\x00', off)
        return self.data[off:end].decode('latin1', 'replace')

    def exports(self):
        names = set()
        rva, _ = self.dirs[IMAGE_DIRECTORY_ENTRY_EXPORT]
        if not rva:
            return names

        off = self.rva_to_off(rva)
        num_names = struct.unpack_from('<I', self.data, off + 24)[0]
        addr_names = struct.unpack_from('<I', self.data, off + 32)[0]
        names_off = self.rva_to_off(addr_names)

        for i in range(num_names):
            name_rva = struct.unpack_from('<I', self.data, names_off + i * 4)[0]
            names.add(self._cstr(self.rva_to_off(name_rva)))

        return names

    def imports(self):
        names = set()
        rva, _ = self.dirs[IMAGE_DIRECTORY_ENTRY_IMPORT]
        if not rva:
            return names

        off = self.rva_to_off(rva)
        while True:
            fields = struct.unpack_from('<IIIII', self.data, off)
            if fields == (0, 0, 0, 0, 0):
                break
            names.add(self._cstr(self.rva_to_off(fields[3])).lower())
            off += 20

        return names


def check(path, arch):
    machine, want_name = MACHINES[arch]
    pe = Pe(open(path, 'rb').read())

    errors = []
    if pe.machine != machine:
        errors.append('machine: expected %s (0x%x), got 0x%x' % (want_name, machine, pe.machine))

    exports = pe.exports()
    missing = sorted(REQUIRED_EXPORTS - exports)
    for name in missing:
        errors.append('missing export: %s' % name)

    imports = pe.imports()
    for name in sorted(imports):
        if name in ALLOWED_IMPORTS:
            continue
        if name.startswith(ALLOWED_IMPORT_PREFIXES):
            continue
        errors.append('unexpected import: %s' % name)

    extra = sorted(exports - REQUIRED_EXPORTS)
    return pe, exports, imports, extra, errors


def collect(target):
    if os.path.isdir(target):
        return sorted(p for p in glob.glob(os.path.join(target, 'yapb*.dll'))
                      if 'amxx' not in os.path.basename(p).lower())
    return [target]


def main():
    parser = argparse.ArgumentParser(description='verify the yapb Windows DLL contract')
    parser.add_argument('target', help='DLL file or directory to scan')
    parser.add_argument('--arch', default='i386', choices=sorted(MACHINES),
                        help='target architecture (default: i386)')
    args = parser.parse_args()

    libraries = collect(args.target)
    if not libraries:
        print('%s: no yapb*.dll found' % args.target)
        return 1

    failed = False
    for path in libraries:
        if not os.path.exists(path):
            print('%s: no such file' % path)
            failed = True
            continue

        _, exports, imports, extra, errors = check(path, args.arch)

        if errors:
            failed = True
            for error in errors:
                print('%s: %s' % (os.path.basename(path), error))
            continue

        line = '%s: %s ok, imports=%s, %d exports' % (
            path, args.arch, ','.join(sorted(imports)) or 'none', len(exports))
        if extra:
            line += ' (extra: %s)' % ','.join(extra)
        print(line)

    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
