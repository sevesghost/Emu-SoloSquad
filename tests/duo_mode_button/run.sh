#!/usr/bin/env bash
# Build + run the Duo mode button host tests. Usage: tests/duo_mode_button/run.sh [case-name-filter]
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"; root="$(cd "$here/../.." && pwd)"
mkdir -p "$here/gen" "$here/build"
python3 "$here/extract.py" "$root" "$here/gen/real_code.inc"
CXX="${CXX:-}"
if [ -z "$CXX" ]; then
	if command -v g++ >/dev/null; then CXX=g++
	elif command -v clang++ >/dev/null; then CXX=clang++
	elif python3 -c 'import ziglang' 2>/dev/null; then CXX="python3 -m ziglang c++"
	else echo "need g++ (>=13), clang++ (>=17) or 'pip install ziglang'"; exit 2; fi
fi
cd "$here"
$CXX -std=c++23 -O0 -g -Wall -Wno-unused -I"$here" duo_mode_button_test.cc -o build/duo_mode_button_test
./build/duo_mode_button_test "$@"
