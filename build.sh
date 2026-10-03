#!/bin/bash
set -e
cd "$(dirname "$0")"

PRESET="release"
COMPILER=""
JOBS=""
NATIVE="OFF"
SKIP_CHECKS="OFF"
EXTRA_D=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    --arch=*)
      PRESET="${1#*=}"
      shift
      ;;
    -c=*|--compiler=*)
      COMPILER="${1#*=}"
      shift
      ;;
    -j=*|--jobs=*)
      JOBS="${1#*=}"
      shift
      ;;
    --native)
      NATIVE="ON"
      shift
      ;;
    --skip-checks)
      SKIP_CHECKS="ON"
      shift
      ;;
    -D*)
      EXTRA_D+=("$1")
      shift
      ;;
    *)
      echo "error: unknown option '$1'" >&2
      exit 1
      ;;
  esac
done

case "$COMPILER" in
  "" ) ;;
  gcc|g++) export CC=gcc CXX=g++ ;;
  clang|clang++) export CC=clang CXX=clang++ ;;
  *) echo "error: unknown compiler '$COMPILER' (want gcc|clang)" >&2; exit 1 ;;
esac

case "$PRESET" in
  release|dist|linux-amd64-asan|linux-x86-asan) ;;
  ci-*) ;;
  *) PRESET="ci-$PRESET" ;;
esac

die() {
  echo "error: $1" >&2
  exit 1
}

check_tool() {
  command -v "$1" >/dev/null 2>&1 || die "'$1' not found on PATH"
}

check_cmake_version() {
  local ver major minor
  ver="$(cmake --version 2>/dev/null | head -n 1 | grep -oE '[0-9]+\.[0-9]+' | head -n 1 || true)"
  major="${ver%%.*}"
  minor="${ver#*.}"
  if [[ -z "$major" || -z "$minor" ]]; then
    return 0
  fi
  if ((major > 3 || (major == 3 && minor >= 28))); then
    return 0
  fi
  die "cmake 3.28+ is required (found $ver)"
}

check_compiler() {
  case "$COMPILER" in
    gcc|g++)
      check_tool gcc
      check_tool g++
      TEST_CC="gcc"
      TEST_CXX="g++"
      ;;
    clang|clang++)
      check_tool clang
      check_tool clang++
      TEST_CC="clang"
      TEST_CXX="clang++"
      ;;
    "")
      if [[ -n "${CC:-}" && -n "${CXX:-}" ]]; then
        TEST_CC="$CC"
        TEST_CXX="$CXX"
      elif command -v gcc >/dev/null 2>&1 && command -v g++ >/dev/null 2>&1; then
        TEST_CC="gcc"
        TEST_CXX="g++"
      elif command -v clang >/dev/null 2>&1 && command -v clang++ >/dev/null 2>&1; then
        TEST_CC="clang"
        TEST_CXX="clang++"
      else
        die "no complete C/C++ toolchain found (need gcc+g++ or clang+clang++)"
      fi
      ;;
  esac
}

needs_m32_check() {
  case "$PRESET" in
    release|dist|linux-x86-asan) ;;
    *) return 1 ;;
  esac
  local d
  for d in "${EXTRA_D[@]}"; do
    case "$d" in
      *YAPB_64BIT=ON*|*YAPB_64BIT:BOOL=ON*|*YAPB_64BIT=TRUE*) return 1 ;;
    esac
  done
  return 0
}

check_m32() {
  local tmp log ok
  tmp="$(mktemp -d 2>/dev/null)" || return 0
  log="$tmp/build.log"
  printf 'int main(void){return 0;}\n' >"$tmp/t.c"
  printf '#include <vector>\nint main(){std::vector<int> v;return (int)v.size();}\n' >"$tmp/t.cpp"
  ok=1
  if ! "$TEST_CC" -m32 "$tmp/t.c" -o "$tmp/t-c" >"$log" 2>&1; then
    ok=0
  fi
  if ! "$TEST_CXX" -m32 "$tmp/t.cpp" -o "$tmp/t-cxx" >>"$log" 2>&1; then
    ok=0
  fi
  if [[ "$ok" == "1" ]]; then
    rm -rf "$tmp"
    return 0
  fi
  echo "error: '$TEST_CC -m32' cannot build 32-bit binaries" >&2
  tail -n 12 "$log" | sed 's/^/  | /' >&2
  rm -rf "$tmp"
  exit 1
}

if [[ "$SKIP_CHECKS" != "ON" ]]; then
  check_tool cmake
  check_cmake_version
  check_tool ninja
  check_compiler

  case "$PRESET" in
    release|dist|linux-amd64-asan|linux-x86-asan) ;;
    ci-apple-arm64|ci-windows-x86|ci-windows-x86-clang-cl|ci-windows-x86-msvc-xp) ;;
    *)
      command -v zig >/dev/null 2>&1 || die "preset '$PRESET' cross-compiles through zig, but zig is not on PATH"
      ;;
  esac

  if needs_m32_check; then
    check_m32
  fi
fi

CONFIGURE_ARGS=()
if [[ "$NATIVE" == "ON" ]]; then
  CONFIGURE_ARGS+=("-DYAPB_NATIVE=ON")
fi

cmake --preset "$PRESET" "${CONFIGURE_ARGS[@]}" "${EXTRA_D[@]}"

BUILD_ARGS=()
if [[ -n "$JOBS" ]]; then
  BUILD_ARGS+=(--parallel "$JOBS")
fi

cmake --build --preset "$PRESET" "${BUILD_ARGS[@]}"
