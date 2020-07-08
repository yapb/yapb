local mesonBuild (buildName, cxx, ld, mesonOptions) = {
  kind: "pipeline",
  type: "exec",
  name: buildName,
  steps: [
    {
      environment: {
         CXX: cxx,
         CXX_LD: ld
      },
      name: "setup",
      commands: [
        "meson setup build " + mesonOptions,
      ]
    },
    {
      name: "build",
      commands: [
        "ninja -C build",
      ]
    },
    {
      name: "upload",
      commands: [
        "upload-binary " + buildName
      ]
    }
  ]
};

[
  mesonBuild("linux-release-gcc", "gcc", "", ""),
  mesonBuild("linux-release-clang", "clang", "lld", ""),
  
  mesonBuild("linux-release-intel", "/opt/intel/bin/icc", "", ""),
  mesonBuild("linux-debug-gcc", "gcc", "", "--buildtype=debug"),
  
  mesonBuild("macos-release-clang", "", "", "--cross-file=/opt/meson/cross/darwin.ini"),
  mesonBuild("macos-debug-clang", "", "", "--cross-file=/opt/meson/cross/darwin.ini --buildtype=debug"),
  
  mesonBuild("win32-release-msvc", "", "", "--cross-file=/opt/meson/cross/msvc.ini -Db_vscrt=mt"),
  mesonBuild("win32-debug-msvc", "", "", "--cross-file=/opt/meson/cross/msvc.ini --buildtype=debug -Db_vscrt=mtd"),
  mesonBuild("win32-release-mingw", "", "", "--cross-file=/opt/meson/cross/mingw.ini"),
]
