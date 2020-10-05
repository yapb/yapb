#
# YaPB - Counter-Strike Bot based on PODBot by Markus Klinge.
# Copyright © 2004-2020 YaPB Development Team <team@yapb.ru>.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

project (
    'yapb',
    'cpp',
    
    version: '4.0',
    license: 'GPL',
    
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

flagsLinker = []
flagsCompiler = []

cdata = configuration_data()

if isWindows
    cdata.set ('buildVersionWin',  ','.join (buildVersion.split ('.')))
else
    cdata.set ('buildVersionWin', buildVersion)
endif

cdata.set ('commitHash', run_command ('git', 'rev-parse', '--short', 'HEAD').stdout ().strip ())
cdata.set ('commitCount', run_command ('git', 'rev-list', '--count', 'HEAD').stdout ().strip ())
cdata.set ('commitAuthor', run_command ('git', 'log', '--pretty="%ae"', '-1').stdout ().strip ())

cdata.set ('buildVersion', buildVersion)
cdata.set ('buildMachine', run_command ('hostname', '-f').stdout ().strip ())
cdata.set ('buildCompiler', compilerId + ' ' + compilerVersion)

configure_file (input: 'inc/version.h.in', output: 'version.build.h', configuration: cdata)

flagsCompiler += '-DVERSION_GENERATED'

if isCLang or isGCC or (isIntel and not isWindows)
    flagsCompiler += [
        '-m32',
        '-fno-threadsafe-statics',
        '-fno-exceptions',
        '-fno-rtti'
    ]
    
    if not isDarwin
       flagsCompiler += [
           '-pedantic'
       ]
    endif
    
    if isOptimize
        flagsCompiler += '-msse2'
        
        if isCLang and not isDarwin
            flagsCompiler += '-flto'
            flagsLinker += '-flto'
        endif
    endif
    
    if isLinux
        flagsLinker += [
            '-m32',
            '-lm',
            '-ldl'
        ]
    endif
endif

if isIntel and (isLinux or isDarwin)
    flagsLinker += [
        '-static-intel',
        '-no-intel-extensions'
    ]
endif

if isLinux or isDarwin
    if isDarwin
        flagsCompiler += '-mmacosx-version-min=10.9'
        flagsLinker += [
            '-dynamiclib',
            '-lstdc++',
            '-mmacosx-version-min=10.9'
        ]
    else
        flagsLinker += '-static-libgcc'
    endif
    
    if not isOptimize
        flagsCompiler += [
            '-g3',
            '-ggdb',
            '-DCR_DEBUG'
        ]
    else
        flagsCompiler += [
            '-mtune=generic',
            '-msse2',
            '-mfpmath=sse',
            '-fno-builtin',
            '-funroll-loops',
            '-fomit-frame-pointer',
            '-fno-stack-protector',
            '-fvisibility=hidden',
            '-fvisibility-inlines-hidden'
        ]
        
        if isIntel 
            flagsCompiler += [
                '-funroll-all-loops',
                '-ipo',
                '-wd11076', 
                '-wd11074'
            ]
            
            flagsLinker += [
                '-cxxlib-nostd',
                '-Wl,--no-undefined,-z,notext,--gc-sections',
                '-ipo'
            ]
        elif isCLang and not isDarwin
             llvmLinker = find_program ('lld', required: false)
             
            if llvmLinker.found() == true
                flagsLinker += '-fuse-ld=' + llvmLinker.path ().split ('/')[-1]
            endif
            
            flagsLinker += [
                '-nostdlib++',
                '-Wunused-command-line-argument',
                '-Wl,-z,notext',
                '--no-undefined'
            ]
        elif isGCC and not isDarwin
            flagsCompiler += '-funroll-all-loops'
            flagsLinker += '-Wl,--no-undefined'
        endif
    endif
endif

if isWindows and (isVC or isIntel)
    flagsLinker += [
        '/MACHINE:X86',
        'user32.lib',
        'ws2_32.lib'
    ]
    
    flagsCompiler += [
        '/TP'
    ]
    
    if isOptimize
        flagsCompiler += '/GL'
        flagsLinker += '/LTCG'
    endif
    
elif isWindows and (isCLang or isGCC)
    if isCLang
        flagsLinker += '-Wl,/MACHINE:X86'
    else
        flagsLinker += [
            '-static-libgcc',
            '-Wl,--add-stdcall-alias'
        ]
    endif
    
    flagsLinker += [
        '-luser32',
        '-lws2_32'
    ]
endif

add_global_arguments (flagsCompiler, language: 'cpp')
add_global_link_arguments (flagsLinker, language: 'cpp')

sourceFiles = files (
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
    'src/message.cpp',
    'src/navigate.cpp',
    'src/support.cpp'
)

includes = include_directories ([
    '.', 'inc', 'ext',
], is_system: true)

if isWindows and not isCLang
    sourceFiles += import('windows').compile_resources (
        'vc/yapb.rc', 
        include_directories: includes, 
        args: '-DVERSION_GENERATED'
    )
endif

shared_library (
    'yapb', 
    sourceFiles,
    include_directories: includes,
    
    gnu_symbol_visibility: 'hidden',
    name_prefix: '')
