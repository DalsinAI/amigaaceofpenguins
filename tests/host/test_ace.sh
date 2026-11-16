#!/bin/sh
# Host tests of the port's drawing layer (amiga/acexwin.c), on this
# machine: every game is built with upstream's own lib/ (table, stack, help,
# imagelib, the pictures through make-imglib) over a frame in memory
# (tests/host/acexwin_host.c), and run with scripted mouse and keys:
#  - each of the eleven starts, draws its table and ends on Esc;
#  - Solitaire: a click on the stock turns a card (the frame changes), a drag
#    of a card runs to its end, F1 shows the help page, and the frame has the cards'
#    colours (more than the table's green);
#  - FreeCell, Spider: the cards are dealt (many colours on the table).
# Needs a C compiler and libpng (with its headers); skipped without them.
#   ACE=/path/to/ace-1.4 (patched) sh tests/host/test_ace.sh
# Copyright (c) 2026 AmiCradle Team. GPL-2.0-or-later, like Ace of Penguins.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$HERE/../.." && pwd)
ACE=${ACE:?ACE=upstream source}
T=$(mktemp -d "${TMPDIR:-/tmp}/ace-host.XXXXXX")
trap 'rm -rf "$T"' EXIT INT TERM
CC=${HOSTCC:-cc}

if ! printf '#include <png.h>\nint main(void){return png_access_version_number()==0;}\n' > "$T/p.c" ||
   ! $CC -o "$T/p" "$T/p.c" -lpng -lz 2>/dev/null; then
    echo "host tests: skipped (no libpng to build with)"
    exit 0
fi

# upstream is old C: gnu89, and -fpermissive where the compiler has it (GCC 14 on)
PERM=$($CC -fpermissive -x c -E /dev/null >/dev/null 2>&1 && echo -fpermissive || true)
# upstream's generators, then the pictures and the help pages as C
$CC -O1 -w -std=gnu89 $PERM -o "$T/make-imglib" "$ACE/lib/make-imglib.c" -lpng -lz
$CC -O1 -w -std=gnu89 $PERM -o "$T/text2c" "$ACE/lib/text2c.c"
"$T/make-imglib" -n cards -i "$ACE/lib/png" -d "$T/images.d" "$ACE/lib/images.in" "$T/images.c"
GAMES="canfield freecell golf mastermind merlin minesweeper pegged solitaire spider taipei thornq"

UP="-std=gnu89 $PERM -w -O1 -I$ROOT/amiga/include -I$ROOT/amiga -I$ACE/lib -I$ACE/games"
MINE="-std=gnu99 -Wall -Werror -Wno-unused-function -O1 -I$ROOT/amiga/include -I$ROOT/amiga -I$ACE/lib"
for f in table help imagelib table_rn funcs; do $CC $UP -c -o "$T/$f.o" "$ACE/lib/$f.c"; done
$CC $UP -Dgettimeofday=ace_gettimeofday -Dselect=ace_select -c -o "$T/stack.o" "$ACE/lib/stack.c"
$CC $UP -c -o "$T/images.o" "$T/images.c"
$CC $MINE -c -o "$T/acexwin.o" "$ROOT/amiga/acexwin.c"
$CC $MINE -I"$HERE" -c -o "$T/acexwin_host.o" "$HERE/acexwin_host.c"
LIBO="$T/table.o $T/help.o $T/imagelib.o $T/table_rn.o $T/funcs.o $T/stack.o $T/images.o $T/acexwin.o $T/acexwin_host.o"

pngs_for() {	# the game's pictures, as amiga/Makefile.amigaos lists them
    awk -v g="$1_PNG" '
        $1 == g && $2 == "=" { on = 1; sub(/^[^=]*= */, ""); }
        on { line = $0; c = sub(/\\$/, "", line); printf "%s ", line; if (!c) exit }' \
        "$ROOT/amiga/Makefile.amigaos"
}

for g in $GAMES; do
    # shellcheck disable=SC2046
    "$T/make-imglib" -i "$ACE/games" -n appimglib - "$T/$g-img.c" $(pngs_for "$g")
    extra=""
    if [ -f "$ACE/games/$g.html" ] && [ "$g" != canfield ]; then
        "$T/text2c" "${g}_help" < "$ACE/games/$g.html" > "$T/$g-help.c"
        extra="$T/$g-help.c"
    fi
    [ "$g" = taipei ] && extra="$extra $ACE/games/taipeilib.c"
    # shellcheck disable=SC2086
    $CC $UP -o "$T/$g" "$ACE/games/$g.c" "$T/$g-img.c" $extra $LIBO -lpng -lz -lm
done

fails=0
check() {	# name, ppm, at least this many colours
    python3 - "$1" "$2" "$3" <<'PY' || fails=$((fails + 1))
import sys
name, path, need = sys.argv[1], sys.argv[2], int(sys.argv[3])
data = open(path, "rb").read()
magic, size, depth, pix = data.split(b"\n", 3)
w, h = map(int, size.split())
cols = {pix[i:i + 3] for i in range(0, len(pix), 3)}
if len(pix) != w * h * 3 or len(cols) < need:
    print(f"FAIL {name}: {len(cols)} colours on {w}x{h}, want {need}")
    sys.exit(1)
print(f"ok   {name}: {w}x{h}, {len(cols)} colours")
PY
}
run() {	# game, events, ppm
    if ! ACE_EVENTS="$2" ACE_DUMP="$3" timeout 60 "$T/$1" > "$T/out.txt" 2>&1; then
        echo "FAIL $1 ($2): exit $?"; cat "$T/out.txt"; fails=$((fails + 1)); return 1
    fi
}

for g in $GAMES; do
    run "$g" "w" "$T/$g.ppm" && check "$g starts and draws" "$T/$g.ppm" 4
done
check "solitaire deals the cards" "$T/solitaire.ppm" 12
check "freecell deals the cards" "$T/freecell.ppm" 12
check "spider deals the cards" "$T/spider.ppm" 12

same() { cmp -s "$1" "$2"; }
run solitaire "c:30,40 w" "$T/sol-click.ppm" &&
    { if same "$T/solitaire.ppm" "$T/sol-click.ppm"; then echo "FAIL solitaire: the stock click changed nothing"; fails=$((fails + 1)); else echo "ok   solitaire: a click on the stock turns a card"; fi; }
run solitaire "k:257 w" "$T/sol-help.ppm" &&
    { if same "$T/solitaire.ppm" "$T/sol-help.ppm"; then echo "FAIL solitaire: F1 showed no help"; fails=$((fails + 1)); else echo "ok   solitaire: F1 shows the help page"; fi; }
run solitaire "c:30,40 w d:110,40,110,200 w d:300,200,500,300 w" "$T/sol-drag.ppm" &&
    check "solitaire: drags run to their end" "$T/sol-drag.ppm" 12

# ACE_TEST_KEEP=dir keeps the frames (PPM) to look at
if [ -n "${ACE_TEST_KEEP:-}" ]; then mkdir -p "$ACE_TEST_KEEP" && cp "$T"/*.ppm "$ACE_TEST_KEEP"/; fi
if [ "$fails" -ne 0 ]; then echo "host tests: $fails failed"; exit 1; fi
echo "host tests: all passed"
