#!/usr/bin/env python3
#
# YaPB, started from PODBot by Count Floyd
# Maintained by YaPB Team <yapb@jeefo.net>
#
# SPDX-License-Identifier: Unlicense
#
# Verifies the binary ABI contract of a built yapb shared object. The
# contract itself is read from the linker version script, so the checker
# and the exported surface never drift:
#
#   - ELF class / machine / type match the requested arch
#   - DT_NEEDED stays inside the allowed runtime libraries
#   - required symbol versions are <= the glibc baseline and carry no C++
#     runtime (GLIBCXX_/CXXABI_/GCC_)
#   - exported symbols are exactly the globals declared in the version
#     script and all carry its version node
#
# Pure stdlib: parses ELF directly, no readelf/objdump needed.
#
#   python3 tools/check_abi.py build/linux-x86/yapb.so
#   python3 tools/check_abi.py --arch amd64 --max-glibc 2.17 build/linux-amd64/yapb_amd64.so
#

import argparse
import fnmatch
import os
import re
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# e_ident
EI_CLASS, EI_DATA = 4, 5

ELFCLASS32, ELFCLASS64 = 1, 2
ELFDATA2LSB, ELFDATA2MSB = 1, 2
ET_DYN = 3

SHN_UNDEF = 0
STB_GLOBAL, STB_WEAK = 1, 2
STV_DEFAULT, STV_PROTECTED = 0, 3

DT_NULL, DT_NEEDED, DT_SONAME = 0, 1, 14

# arch -> (e_machine, elf class, human name)
ARCHES = {
    'i386': (3, ELFCLASS32, 'Intel 80386'),
    'amd64': (62, ELFCLASS64, 'AMD x86-64'),
    'arm64': (183, ELFCLASS64, 'AArch64'),
    'riscv64': (243, ELFCLASS64, 'RISC-V'),
}

# the only shared libraries a yapb module may pull in; libstdc++/libgcc_s
# are deliberately absent, the bot is built without a C++ runtime
ALLOWED_NEEDED = frozenset({
    'libc.so.6',
    'libm.so.6',
    'libdl.so.2',
    'libpthread.so.0',
    'librt.so.1',
})


class Elf(object):
    def __init__(self, data):
        if data[:4] != b'\x7fELF':
            raise ValueError('not an ELF file')
        self.data = data
        self.cls = data[EI_CLASS]
        self.endian = data[EI_DATA]

        if self.endian == ELFDATA2LSB:
            self.endian_fmt = '<'
        elif self.endian == ELFDATA2MSB:
            self.endian_fmt = '>'
        else:
            raise ValueError('unsupported EI_DATA %d' % self.endian)

        if self.cls not in (ELFCLASS32, ELFCLASS64):
            raise ValueError('unsupported EI_CLASS %d' % self.cls)

        self.is64 = self.cls == ELFCLASS64

        self._parse_ehdr()
        self._parse_shdrs()
        self._parse_dynamic()
        self._parse_dynsym()
        self._parse_versions()

    def _u(self, fmt, off):
        return struct.unpack_from(self.endian_fmt + fmt, self.data, off)[0]

    def _cstr(self, blob, off):
        end = blob.find(b'\x00', off)
        if end < 0:
            end = len(blob)
        return blob[off:end].decode('utf-8', 'replace')

    def _parse_ehdr(self):
        self.e_type = self._u('H', 16)
        self.e_machine = self._u('H', 18)

        if self.is64:
            self.e_shoff = self._u('Q', 40)
            self.e_shentsize = self._u('H', 58)
            self.e_shnum = self._u('H', 60)
            self.e_shstrndx = self._u('H', 62)
        else:
            self.e_shoff = self._u('I', 32)
            self.e_shentsize = self._u('H', 46)
            self.e_shnum = self._u('H', 48)
            self.e_shstrndx = self._u('H', 50)

    def _parse_shdrs(self):
        self.sections = []

        for i in range(self.e_shnum):
            off = self.e_shoff + i * self.e_shentsize

            if self.is64:
                name = self._u('I', off)
                sh_type = self._u('I', off + 4)
                sh_offset = self._u('Q', off + 24)
                sh_size = self._u('Q', off + 32)
                sh_entsize = self._u('Q', off + 56)
            else:
                name = self._u('I', off)
                sh_type = self._u('I', off + 4)
                sh_offset = self._u('I', off + 16)
                sh_size = self._u('I', off + 20)
                sh_entsize = self._u('I', off + 36)

            self.sections.append({
                'name': name,
                'type': sh_type,
                'offset': sh_offset,
                'size': sh_size,
                'entsize': sh_entsize,
            })

        shstr = self.sections[self.e_shstrndx]
        self.shstrtab = self.data[shstr['offset']:shstr['offset'] + shstr['size']]

        self.sec = {}
        for section in self.sections:
            section['sname'] = self._cstr(self.shstrtab, section['name'])
            self.sec[section['sname']] = section

    def _dynstr(self):
        section = self.sec.get('.dynstr')
        if not section:
            return b''
        return self.data[section['offset']:section['offset'] + section['size']]

    def _parse_dynamic(self):
        self.needed = []
        self.soname = None

        section = self.sec.get('.dynamic')
        if not section:
            return

        strtab = self._dynstr()
        step = 16 if self.is64 else 8

        for off in range(section['offset'], section['offset'] + section['size'], step):
            tag = self._u('Q' if self.is64 else 'I', off)
            val = self._u('Q' if self.is64 else 'I', off + step // 2)

            if tag == DT_NULL:
                break
            if tag == DT_NEEDED:
                self.needed.append(self._cstr(strtab, val))
            elif tag == DT_SONAME:
                self.soname = self._cstr(strtab, val)

    def _parse_dynsym(self):
        self.symbols = []

        section = self.sec.get('.dynsym')
        if not section:
            return

        strtab = self._dynstr()
        step = 24 if self.is64 else 16

        for i in range(section['size'] // step):
            off = section['offset'] + i * step

            st_name = self._u('I', off)
            st_info = self.data[off + (4 if self.is64 else 12)]
            st_other = self.data[off + (5 if self.is64 else 13)]
            st_shndx = self._u('H', off + (6 if self.is64 else 14))

            self.symbols.append({
                'name': self._cstr(strtab, st_name),
                'bind': st_info >> 4,
                'other': st_other,
                'shndx': st_shndx,
            })

    def _parse_versions(self):
        self.versym = []
        self.ver_def = {}
        self.ver_need = {}

        section = self.sec.get('.gnu.version')
        if section:
            for i in range(section['size'] // 2):
                self.versym.append(self._u('H', section['offset'] + i * 2))

        strtab = self._dynstr()

        # version definitions: index -> node name (e.g. YAPB_ABI_1.0)
        section = self.sec.get('.gnu.version_d')
        if section:
            off, end = section['offset'], section['offset'] + section['size']
            while off < end:
                vd_ndx = self._u('H', off + 4)
                vd_aux = self._u('I', off + 12)
                vd_next = self._u('I', off + 16)

                vda_name = self._u('I', off + vd_aux)
                self.ver_def[vd_ndx] = self._cstr(strtab, vda_name)

                if vd_next == 0:
                    break
                off += vd_next

        # version requirements: index -> (file, version name)
        section = self.sec.get('.gnu.version_r')
        if section:
            off, end = section['offset'], section['offset'] + section['size']
            while off < end:
                vn_file = self._u('I', off + 4)
                vn_aux = self._u('I', off + 8)
                vn_next = self._u('I', off + 12)

                fname = self._cstr(strtab, vn_file)
                aux = off + vn_aux

                while True:
                    vna_other = self._u('H', aux + 6)
                    vna_name = self._u('I', aux + 8)
                    vna_next = self._u('I', aux + 12)

                    self.ver_need[vna_other] = (fname, self._cstr(strtab, vna_name))

                    if vna_next == 0:
                        break
                    aux += vna_next

                if vn_next == 0:
                    break
                off += vn_next


def parse_version_ver(value):
    return tuple(int(part) for part in value.split('.'))


def parse_version_script(path):
    with open(path, 'r', encoding='utf-8') as handle:
        text = handle.read()

    node = None
    match = re.search(r'([\w.]+)\s*\{', text)
    if match:
        node = match.group(1)

    patterns = []
    match = re.search(r'global\s*:(.*?)(local\s*:|};)', text, re.S)
    if match:
        for token in match.group(1).split(';'):
            token = token.strip()
            if token:
                patterns.append(token)

    return node, patterns


def check(path, arch, max_glibc, version_script):
    machine, want_class, want_name = ARCHES[arch]
    elf = Elf(open(path, 'rb').read())

    errors = []

    if elf.e_machine != machine:
        errors.append('machine: expected %s (EM=%d), got EM=%d' % (want_name, machine, elf.e_machine))
    if elf.cls != want_class:
        errors.append('class: expected %s, got ELF%s' % (
            'ELF64' if want_class == ELFCLASS64 else 'ELF32',
            '64' if elf.is64 else '32'))
    if elf.e_type != ET_DYN:
        errors.append('type: expected ET_DYN (shared object), got %d' % elf.e_type)

    for name in elf.needed:
        if name not in ALLOWED_NEEDED:
            errors.append('unexpected DT_NEEDED: %s' % name)

    limit = parse_version_ver(max_glibc)
    glibc_top = None
    cxx = []

    for _, (_, name) in sorted(elf.ver_need.items()):
        if name.startswith('GLIBC_'):
            value = parse_version_ver(name[len('GLIBC_'):])
            if glibc_top is None or value > glibc_top[0]:
                glibc_top = (value, name)
        elif name.startswith(('GLIBCXX_', 'CXXABI_', 'GCC_')):
            cxx.append(name)
        else:
            errors.append('unexpected version requirement: %s' % name)

    if cxx:
        errors.append('C++ runtime requirement(s): %s' % ', '.join(sorted(set(cxx))))
    if glibc_top and glibc_top[0] > limit:
        errors.append('glibc baseline %s exceeds %s' % (glibc_top[1], max_glibc))

    node, patterns = parse_version_script(version_script)
    exported = []
    matched = dict((pattern, False) for pattern in patterns)

    for i, symbol in enumerate(elf.symbols):
        if symbol['shndx'] == SHN_UNDEF:
            continue
        if symbol['bind'] not in (STB_GLOBAL, STB_WEAK):
            continue
        if (symbol['other'] & 0x3) not in (STV_DEFAULT, STV_PROTECTED):
            continue

        index = elf.versym[i] if i < len(elf.versym) else 0
        version = elf.ver_def.get(index)
        exported.append((symbol['name'], version))

        for pattern in patterns:
            if fnmatch.fnmatchcase(symbol['name'], pattern):
                matched[pattern] = True

    for name, version in exported:
        if not any(fnmatch.fnmatchcase(name, pattern) for pattern in patterns):
            errors.append('exported symbol not declared in version script: %s' % name)
        if version != node:
            errors.append('exported symbol %s carries version %r, expected %r'
                          % (name, version, node))

    for pattern, seen in matched.items():
        if not seen:
            errors.append('version script global matched no symbol: %s' % pattern)

    return elf, exported, glibc_top, errors


def main():
    parser = argparse.ArgumentParser(description='verify the yapb binary ABI contract')
    parser.add_argument('library', help='shared object to check')
    parser.add_argument('--arch', default='i386', choices=sorted(ARCHES),
                        help='target architecture (default: i386)')
    parser.add_argument('--max-glibc', default='2.17',
                        help='maximum allowed GLIBC symbol version (default: 2.17)')
    parser.add_argument('--version-script',
                        default=os.path.join(ROOT, 'ext', 'ldscripts', 'version.lds'),
                        help='linker version script describing the ABI contract')
    args = parser.parse_args()

    if not os.path.exists(args.library):
        print('%s: no such file' % args.library)
        return 1

    elf, exported, glibc_top, errors = check(args.library, args.arch, args.max_glibc, args.version_script)

    if errors:
        for error in errors:
            print('%s: %s' % (os.path.basename(args.library), error))
        return 1

    node, _ = parse_version_script(args.version_script)
    top = glibc_top[1] if glibc_top else 'none'
    print('%s: %s ok, needed=%s, max glibc=%s, %d exports @ %s'
          % (args.library, args.arch, ','.join(elf.needed) or 'none', top, len(exported), node))
    return 0


if __name__ == '__main__':
    sys.exit(main())
