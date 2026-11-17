#!/usr/bin/env bash
# Saturn-Quad 4-player host tests.
# Usage: tests/quad_mode/run.sh [filter]            pr4logic backend (real QuadControls.hh decisions) against this repo
#        QUAD_BACKEND=real run.sh                   full VController backend (all PENDING until backend_real_impl.inc exists)
#        QUAD_SRC=/path/to/Saturn-Quad run.sh       test another checkout instead of this repo
#        QUAD_BACKEND=model run.sh                  reference model (validates the cases themselves)
#        QUAD_MODEL_BUG=<bug> QUAD_BACKEND=model    model with an injected bug (cases must FAIL)
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"; root="${QUAD_SRC:-$(cd "$here/../.." && pwd)}"
mkdir -p "$here/gen" "$here/build"
export QUAD_NEO_SAMPLES="${QUAD_NEO_SAMPLES:-$here/tools/neoduo-samples/samples-pr2b}"  # samples-pr2b only (bit7 format)
backend="${QUAD_BACKEND:-pr4logic}"
python3 "$here/extract.py" "$root" "$here/gen"
CXX="${CXX:-}"
if [ -z "$CXX" ]; then
	if command -v g++ >/dev/null; then CXX=g++
	elif command -v clang++ >/dev/null; then CXX=clang++
	elif python3 -c 'import ziglang' 2>/dev/null; then CXX="python3 -m ziglang c++"
	else echo "need g++ (>=13), clang++ (>=17) or 'pip install ziglang'"; exit 2; fi
fi
srcs=("$here/core/main.cc" "$here/core/backend_${backend}.cc")
shopt -s nullglob; cases=("$here"/cases/*.cc); shopt -u nullglob
[ ${#cases[@]} -eq 0 ] && echo "no cases/*.cc yet"
$CXX -std=c++23 -DQUAD_REAL_QUADCONTROLS="<$root/EmuFramework/include/emuframework/QuadControls.hh>" -O0 -g -Wall -Werror -Wno-unused -I"$here" -I"$here/core" "${srcs[@]}" "${cases[@]}" -o "$here/build/quad_test_$backend"
"$here/build/quad_test_$backend" "$@"
