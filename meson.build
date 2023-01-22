#
# YaPB - Counter-Strike Bot based on PODBot by Markus Klinge.
# Copyright © 2004-2023 YaPB Project <yapb@jeefo.net>.
#
# SPDX-License-Identifier: MIT
#

project (
   'yapb',
   'cpp',
    
   version: '4.2',
   license: 'MIT',
    
   default_options: [
      'buildtype=release',
      'b_ndebug=if-release',
      'cpp_std=c++14',
      'warning_level=3',
      'werror=true',
      'backend=ninja',
      'optimization=3',
      'default_library=static',
      'cpp_eh=none',
      'b_vscrt=static_from_buildtype',
      'b_lto=true',
      'b_lundef=true',
   ],
   meson_version: '>=0.56.0')

find_program ('ninja', required: true)
find_program ('git', required: true)
find_program ('hostname', required: true)

cpp = meson.get_compiler ('cpp')
sys = host_machine.system ()
target = build_machine.cpu_family ()
version = meson.project_version ()
count = run_command ('git', 'rev-list', '--count', 'HEAD', check: false).stdout ().strip ()

cpp_id = cpp.get_id ()
cpp_version = cpp.version ()

optmize = get_option ('buildtype') == 'release' or get_option ('buildtype') == 'debugoptimized'

msvc = cpp_id == 'msvc' or cpp_id == 'clang-cl'
gcc = cpp_id == 'gcc'
clang = cpp_id == 'clang'

win32 = sys == 'windows'
linux = sys == 'linux'
mac = sys == 'darwin'
aarch64 = target == 'aarch64'

ldflags = []
ccflags = []

cdata = configuration_data()

if win32
   cdata.set ('version_win',  ','.join (version.split ('.')))
   cdata.set ('machine', run_command ('hostname', check: false).stdout ().strip ())
else
   cdata.set ('version_win', version)
   cdata.set ('machine', run_command ('hostname', '-f', check: false).stdout ().strip ())
endif

cdata.set ('hash', run_command ('git', 'rev-parse', '--short', 'HEAD', check: false).stdout ().strip ())
cdata.set ('author', run_command ('git', 'log', '--pretty="%ae"', '-1', check: false).stdout ().strip ())

cdata.set ('count', count)
cdata.set ('version', version)

cdata.set ('compiler', cpp_id + ' ' + cpp_version)

configure_file (input: 'inc/version.h.in', output: 'version.build.h', configuration: cdata)

ccflags += '-DVERSION_GENERATED'

if clang or gcc
   ccflags += [
      '-fno-threadsafe-statics',
      '-fno-exceptions',
      '-fno-rtti'
   ]

   if not aarch64
      ccflags += '-m32'
   endif
    
   if not mac
      ccflags += [
         '-pedantic',
      ]
   endif
   
   if optmize
      if (clang or gcc) and not mac
         if not aarch64 and not (clang and win32)
            ccflags += [
               '-fdata-sections',
               '-ffunction-sections'
            ]
         endif

         if gcc
            ccflags += '-fgraphite-identity'
            ldflags += '-flto-partition=none'
         endif

         if not aarch64 and not (clang and win32)
            ldflags += [
               '-Wl,--version-script=../ldscript.lds',
               '-Wl,--gc-sections'
            ]
         endif
      endif
   endif
    
   if linux
      ldflags += [
         '-lm',
         '-ldl'
      ]
      if not aarch64
         ldflags += '-m32'
      endif
   endif
endif

if linux or mac or (win32 and (gcc or clang))
   if mac
      ccflags += '-mmacosx-version-min=10.9'
      ldflags += [
         '-lstdc++',
         '-mmacosx-version-min=10.9'
      ]
   else
      ldflags += '-static-libgcc'
   endif
    
   if not optmize
      ccflags += [
         '-g3',
         '-ggdb',
         '-DCR_DEBUG'
      ]
   else
      ccflags += [
         '-mtune=generic',
         '-fno-builtin',
         '-funroll-loops',
         '-fomit-frame-pointer',
         '-fno-stack-protector',
         '-fvisibility=hidden',
         '-fvisibility-inlines-hidden'
      ]

      if not aarch64
         ccflags += [
            '-msse2',
            '-mfpmath=sse',
         ]
      else
         ccflags += '-march=armv8-a+fp+simd'
      endif

      if clang and not mac
         ldflags += [
            '-nostdlib++',
            '-Wunused-command-line-argument'
         ]
      elif gcc and not mac
         ldflags += '-Wl,--no-undefined'
      endif
   endif
endif

if win32 and msvc
   ldflags += [
      '/MACHINE:X86',
      'user32.lib',
      'ws2_32.lib'
   ]
    
   ccflags += [
      '/TP',
	  '/D _WIN32_WINNT=0x0501',
	  '/D _USING_V110_SDK71_',
	  '/Zc:threadSafeInit-'
   ]
    
   if optmize
      ccflags += [
         '/GL',
         '/GS-',
         '/Ob2',
         '/Oy',
         '/Oi',
         '/Ot',
         '/fp:precise',
         '/GF'
      ]
      ldflags += [
		 '/LTCG',
		 'delayimp.lib',
		 '/DELAYLOAD:user32.dll',
		 '/SUBSYSTEM:WINDOWS,5.01',
	  ]
   endif
    
elif win32 and (clang or gcc)
   ldflags += [
      '-Wl,--kill-at',
      '-luser32',
      '-lws2_32'
   ]

   if clang
      ldflags += '-Wl,/MACHINE:X86'
      ccflags += '-Wl,/MACHINE:X86'
   endif
endif

add_global_arguments (ccflags, language: 'cpp')
add_global_link_arguments (ldflags, language: 'cpp')

sources = files (
   'src/botlib.cpp',
   'src/chatlib.cpp',
   'src/combat.cpp',
   'src/config.cpp',
   'src/control.cpp',
   'src/engine.cpp',
   'src/graph.cpp',
   'src/linkage.cpp',
   'src/manager.cpp',
   'src/module.cpp',
   'src/message.cpp',
   'src/navigate.cpp',
   'src/support.cpp'
)

includes = include_directories ([
   '.', 'inc', 'ext', 'ext/crlib'
], is_system: true)

if win32
   sources += import('windows').compile_resources (
      'vc/yapb.rc', 
      include_directories: includes, 
      args: ['-DVERSION_GENERATED']
   )
endif

shared_library (
   meson.project_name (), 
   sources,
   include_directories: includes,
   gnu_symbol_visibility: 'hidden',
   name_prefix: '')

run_target ('package',
   command: ['python3', meson.project_source_root () + '/package.py', '@0@.@1@'.format (version, count)])