#
# YaPB - Counter-Strike Bot based on PODBot by Markus Klinge.
# Copyright © 2004-2020 YaPB Project <yapb@jeefo.net>.
#
# SPDX-License-Identifier: MIT
#

project (
   'yapb',
   'cpp',
    
   version: '4.1',
   license: 'MIT',
    
   default_options: [
      'buildtype=release',
      'b_ndebug=if-release',
      'cpp_std=c++14',
      'warning_level=3',
      'werror=true',
      'backend=ninja',
      'strip=true',
      'optimization=3',
      'default_library=static',
      'cpp_eh=none'
   ],
   meson_version: '>=0.48.0')

find_program ('ninja', required: true)
find_program ('git', required: true)
find_program ('hostname', required: true)

buildCompiler = meson.get_compiler ('cpp')
buildSystem = host_machine.system ()
buildVersion = meson.project_version ()
buildCount = run_command ('git', 'rev-list', '--count', 'HEAD').stdout ().strip ()

compilerId = buildCompiler.get_id ()
compilerVersion = buildCompiler.version ()

isOptimize = get_option ('buildtype') == 'release' or get_option ('buildtype') == 'debugoptimized'
isVC = compilerId == 'msvc' or compilerId == 'intel-cl' or compilerId == 'clang-cl'
isGCC = compilerId == 'gcc'
isIntel = compilerId == 'intel' or compilerId == 'intel-cl'
isCLang = compilerId == 'clang'
isWindows = buildSystem == 'windows'
isLinux = buildSystem == 'linux'
isDarwin = buildSystem == 'darwin'

ldflags = []
ccflags = []

cdata = configuration_data()

if isWindows
   cdata.set ('buildVersionWin',  ','.join (buildVersion.split ('.')))
else
   cdata.set ('buildVersionWin', buildVersion)
endif

cdata.set ('commitHash', run_command ('git', 'rev-parse', '--short', 'HEAD').stdout ().strip ())
cdata.set ('commitAuthor', run_command ('git', 'log', '--pretty="%ae"', '-1').stdout ().strip ())

cdata.set ('commitCount', buildCount)
cdata.set ('buildVersion', buildVersion)
cdata.set ('buildMachine', run_command ('hostname', '-f').stdout ().strip ())
cdata.set ('buildCompiler', compilerId + ' ' + compilerVersion)

configure_file (input: 'inc/version.h.in', output: 'version.build.h', configuration: cdata)

ccflags += '-DVERSION_GENERATED'

if isCLang or isGCC or (isIntel and not isWindows)
   ccflags += [
      '-m32',
      '-fno-threadsafe-statics',
      '-fno-exceptions',
      '-fno-rtti'
   ]
    
   if not isDarwin
      ccflags += [
         '-pedantic',
      ]
   endif
    
   if isOptimize
      ccflags += '-msse3'
        
      if (isCLang or isGCC) and not isDarwin
         ccflags += [
            '-flto',
            '-fdata-sections',
            '-ffunction-sections'
         ]
            
         if isGCC
            ccflags += '-fgraphite-identity'
            ldflags += '-flto-partition=none'
         endif
            
         ldflags += [
            '-flto',
            '-Wl,--version-script=../version_script.lds',
            '-Wl,--gc-sections'
         ]
      endif
   endif
    
   if isLinux
      ldflags += [
         '-m32',
         '-lm',
         '-ldl'
      ]
   endif
endif

if isIntel and (isLinux or isDarwin)
   ldflags += [
      '-static-intel',
      '-no-intel-extensions'
   ]
endif

if isLinux or isDarwin
   if isDarwin
      ccflags += '-mmacosx-version-min=10.9'
      ldflags += [
         '-dynamiclib',
         '-lstdc++',
         '-mmacosx-version-min=10.9'
      ]
   else
      ldflags += '-static-libgcc'
   endif
    
   if not isOptimize
      ccflags += [
         '-g3',
         '-ggdb',
         '-DCR_DEBUG'
      ]
   else
      ccflags += [
         '-mtune=generic',
         '-msse3',
         '-mfpmath=sse',
         '-fno-builtin',
         '-funroll-loops',
         '-fomit-frame-pointer',
         '-fno-stack-protector',
         '-fvisibility=hidden',
         '-fvisibility-inlines-hidden'
      ]
        
      if isIntel 
         ccflags += [
            '-ipo',
            '-wd11076', 
            '-wd11074'
         ]
            
         ldflags += [
            '-cxxlib-nostd',
            '-Wl,--no-undefined,-z,notext,--gc-sections',
             '-ipo'
         ]
         elif isCLang and not isDarwin
            llvmLinker = find_program ('lld', required: false)
             
            if llvmLinker.found() == true
               ldflags += '-fuse-ld=' + llvmLinker.path ().split ('/')[-1]
            endif
            
            ldflags += [
               '-nostdlib++',
               '-Wunused-command-line-argument',
               '-Wl,-z,notext',
               '--no-undefined'
            ]
         elif isGCC and not isDarwin
            ldflags += '-Wl,--no-undefined'
         endif
   endif
endif

if isWindows and (isVC or isIntel)
   ldflags += [
      '/MACHINE:X86',
      'user32.lib',
      'ws2_32.lib'
   ]
    
   ccflags += [
      '/TP'
   ]
    
   if isOptimize
      ccflags += [
         '/GL',
         '/arch:SSE2',
         '/GS-',
         '/Ob2',
         '/Oy',
         '/Oi'
      ]
      ldflags += '/LTCG'
   endif
    
elif isWindows and (isCLang or isGCC)
   if isCLang
      ldflags += '-Wl,/MACHINE:X86'
   else
      ldflags += [
         '-static-libgcc',
         '-Wl,--kill-at'
      ]
   endif
    
   ldflags += [
      '-luser32',
      '-lws2_32'
   ]
endif

add_global_arguments (ccflags, language: 'cpp')
add_global_link_arguments (ldflags, language: 'cpp')

sources = files (
   'src/android.cpp',
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
   '.', 'inc', 'ext',
], is_system: true)

if isWindows and not isCLang
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
   command : ['python', meson.source_root() + '/package.py', '@0@.@1@'.format (buildVersion, buildCount)])