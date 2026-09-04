#!/usr/bin/env bash
# Buduje pojedyncze narzedzie z .claude/tools przeciw Release libFiSynth_SharedCode.a.
# Uzycie: build_tool.sh <nazwa-bez-.cpp>   -> wypisuje sciezke do binarki
set -euo pipefail

NAME="${1:?podaj nazwe narzedzia, np. bench_voice}"
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD="$REPO/build-rel"
OUT="${SCRATCH:-${TMPDIR:-/tmp}}/fisynth_tools"
mkdir -p "$OUT"

[[ -f "$BUILD/compile_commands.json" ]] || \
    cmake -S "$REPO" -B "$BUILD" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON > /dev/null

# Biblioteka musi byc nowsza niz naglowki (inaczej -Wodr i wynik klamie).
#
# UWAGA: skanowanie zaleznosci CMake NIE zawsze lapie nowo dodane naglowki
# (SineTable.h przez pewien czas nie byl w compiler_depend.make, wiec zmiany
# w nim nie wyzwalaly rekompilacji i pomiary szly na starej binarce).
# Dotykamy wiec zrodel, zeby przebudowa byla bezwarunkowa.
touch "$REPO"/Source/*.cpp
make -C "$BUILD" -j"$(nproc)" > "$OUT/lib_build.log" 2>&1 || { tail -30 "$OUT/lib_build.log" >&2; exit 1; }

FLAGS=$(python3 - "$BUILD/compile_commands.json" <<'EOF'
import json, shlex, sys
for e in json.load(open(sys.argv[1])):
    if e['file'].endswith('Source/PluginProcessor.cpp'):
        out, skip = [], False
        for t in shlex.split(e['command']):
            if skip: skip = False; continue
            if t == '-o': skip = True; continue
            if t == '-c' or t.endswith('PluginProcessor.cpp'): continue
            out.append(t)
        print(shlex.join(out)); break
EOF
)

eval "$FLAGS -I$REPO/Source -Wno-redundant-decls -c $REPO/.claude/tools/$NAME.cpp -o $OUT/$NAME.o" 2>/dev/null

c++ -O2 "$OUT/$NAME.o" "$BUILD/FiSynth_artefacts/Release/libFiSynth_SharedCode.a" \
    /usr/lib64/libfreetype.so /usr/lib64/libfontconfig.so /usr/lib64/libasound.so \
    -lrt -ldl -lpthread -lX11 -lXext -lXinerama -lXrandr -lXcursor -lcurl \
    -o "$OUT/$NAME" 2>/dev/null

echo "$OUT/$NAME"
