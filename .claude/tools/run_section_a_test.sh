#!/usr/bin/env bash
# Buduje i uruchamia testy sekcji A audytu (section_a_test.cpp):
#   - stabilnosc filtra przy 22050/32000/44100/48000 Hz,
#   - bezpiecznik NaN/Inf (nanResetCount),
#   - declick przy kradziezy glosu.
#
# Wymaga zbudowanego build-rel. Uzycie: .claude/tools/run_section_a_test.sh
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD="$REPO/build-rel"
LIB="$BUILD/FiSynth_artefacts/Release/libFiSynth_SharedCode.a"
OUT="${TMPDIR:-/tmp}/fisynth_section_a"
mkdir -p "$OUT"

# compile_commands.json powstaje raz; nie wymusza przebudowy.
if [[ ! -f "$BUILD/compile_commands.json" ]]; then
    echo ">> generuje compile_commands.json"
    cmake -S "$REPO" -B "$BUILD" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON > /dev/null
fi

# Biblioteka MUSI byc nowsza niz naglowki, inaczej link sypie -Wodr i test klamie.
echo ">> buduje biblioteke (build-rel)"
make -C "$BUILD" -j"$(nproc)" > "$OUT/build.log" 2>&1 || { tail -30 "$OUT/build.log"; exit 1; }

echo ">> kompiluje test"
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
eval "$FLAGS -I$REPO/Source -Wno-redundant-decls -c $REPO/.claude/tools/section_a_test.cpp -o $OUT/test.o" 2>/dev/null

echo ">> linkuje"
c++ -O2 "$OUT/test.o" "$LIB" \
    /usr/lib64/libfreetype.so /usr/lib64/libfontconfig.so /usr/lib64/libasound.so \
    -lrt -ldl -lpthread -lX11 -lXext -lXinerama -lXrandr -lXcursor -lcurl \
    -o "$OUT/section_a_test" 2>/dev/null

echo
"$OUT/section_a_test"
