#!/usr/bin/env bash
# Build + run the Duo mode button host tests. Usage: tests/duo_mode_button/run.sh [case-name-filter]
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"; root="$(cd "$here/../.." && pwd)"
mkdir -p "$here/gen" "$here/build"
# DUO_DUMP_DIR=<dir>: dump saved key-113 configs there (relative paths are from the caller's cwd)
if [ -n "${DUO_DUMP_DIR:-}" ]; then mkdir -p "$DUO_DUMP_DIR"; DUO_DUMP_DIR="$(cd "$DUO_DUMP_DIR" && pwd)"; export DUO_DUMP_DIR; fi
python3 "$here/extract.py" "$root" "$here/gen/real_code.inc"
CXX="${CXX:-}"
if [ -z "$CXX" ]; then
	if command -v g++ >/dev/null; then CXX=g++
	elif command -v clang++ >/dev/null; then CXX=clang++
	elif python3 -c 'import ziglang' 2>/dev/null; then CXX="python3 -m ziglang c++"
	else echo "need g++ (>=13), clang++ (>=17) or 'pip install ziglang'"; exit 2; fi
fi
cd "$here"
# -Wall -Werror like the project build, so warnings in the extracted real code fail here too.
$CXX -std=c++23 -O0 -g -Wall -Werror -Wno-unused -I"$here" duo_mode_button_test.cc -o build/duo_mode_button_test
./build/duo_mode_button_test "$@"
