#!/bin/sh
# The release archive and the GFOpenUp part, for Cradle's stove_build
# (target package):
#  1. upstream Ace of Penguins, the tarball package/ace.lock names (checked
#     by its SHA-256, never kept in this repository), with amiga/patches;
#  2. DalsinAI/acgame at ACGAME_REF: acgame.library and the stubs the games
#     link (each game ships the library it was built against, in Libs/);
#  3. the host tests (tests/host/test_ace.sh), when the machine has a C
#     compiler and libpng;
#  4. the games built by amiga/Makefile.amigaos;
#  5. the drawer, its icon and the Installer script beside it packed as
#     NAME-VERSION-68040.lha (tools/pack_dist.py), with its .sha256 and a
#     short .readme;
#  6. OUT/Parts/AceOfPenguins, a GFOpenUp part: its Part file and the
#     drawer's files for SYS:Games/AceOfPenguins, installed with
#       GFOpenUpTool INSTALL Parts/AceOfPenguins
# Nothing is published from here.
# MIT, Copyright (c) 2026 AmiCradle Team.
set -eu

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
. "$HERE/package.env"
. "$HERE/ace.lock"
OUT=${OUT:-"$ROOT/build-amiga-package"}
mkdir -p "$OUT"
OUT=$(CDPATH= cd -- "$OUT" && pwd)
STOVE=${STOVE:-"$HOME/AmiCradle/stoves/os32-gcc16-open6"}
# outside the checkout: Cradle cleans it (git clean -ffdx) before every job
WORK=${WORK:-"$HOME/.cache/amigaaceofpenguins"}
mkdir -p "$WORK"
cd "$ROOT"
t0=$(date +%s)
if [ -x "$STOVE/prefix/bin/m68k-amigaos-gcc" ]; then PREFIX="$STOVE/prefix"; else PREFIX="$STOVE"; fi

# 1. upstream, checked, unpacked and patched afresh every time
TGZ="$WORK/ace-$ACE_VERSION.tar.gz"
if [ ! -f "$TGZ" ] || [ "$(sha256sum "$TGZ" | cut -d' ' -f1)" != "$ACE_SHA256" ]; then
    curl -fsSL -o "$TGZ.part" "$ACE_URL"
    mv "$TGZ.part" "$TGZ"
fi
[ "$(sha256sum "$TGZ" | cut -d' ' -f1)" = "$ACE_SHA256" ] || { echo "ace: SHA-256 mismatch" >&2; exit 1; }
ACE="$WORK/ace-$ACE_VERSION"
rm -rf "$ACE"
tar xzf "$TGZ" -C "$WORK"
for p in "$ROOT"/amiga/patches/*.patch; do
    patch -s -d "$ACE" -p1 < "$p"
done

# 2. ACGame
ACGAME="$WORK/acgame"
if [ "$(git -C "$ACGAME" rev-parse HEAD 2>/dev/null)" != "$ACGAME_REF" ]; then
    rm -rf "$ACGAME"; mkdir -p "$ACGAME"
    git -C "$ACGAME" init -q
    git -C "$ACGAME" fetch -q --depth 1 https://github.com/DalsinAI/acgame "$ACGAME_REF"
    git -C "$ACGAME" checkout -q FETCH_HEAD
fi
ACGAMELIB="$WORK/acgame-out"
CC="$PREFIX/bin/m68k-amigaos-gcc" AR="$PREFIX/bin/m68k-amigaos-ar" NM="$PREFIX/bin/m68k-amigaos-nm" \
    STOVE="$STOVE" OUT="$ACGAMELIB" sh "$ACGAME/build-amiga.sh" > "$OUT/acgame-output.txt" 2>&1 ||
    { tail -n 60 "$OUT/acgame-output.txt"; exit 1; }

# 3. the host tests
if command -v cc >/dev/null 2>&1; then
    ACE="$ACE" sh tests/host/test_ace.sh
fi

# 4. the games
make -f amiga/Makefile.amigaos ACE="$ACE" ACGAME="$ACGAME" ACGAMELIB="$ACGAMELIB" \
    STOVE="$STOVE" PREFIX="$PREFIX" -j4 $TARGET > "$OUT/build-output.txt" 2>&1 ||
    { tail -n 60 "$OUT/build-output.txt"; exit 1; }

# 5. the archive
python3 "$ROOT/tools/pack_dist.py" --dist "$DIST" --name "$NAME" --out "$OUT"

# 6. the GFOpenUp part
sh "$ROOT/package/write-part.sh" "$DIST/$NAME" "$OUT/Parts/$NAME" "$NAME" "$PORT_VERSION" \
    "Ace of Penguins: Solitaire, FreeCell, Spider and eight more one-player card and tile games"
echo "part: $(grep -c '^copy' "$OUT/Parts/$NAME/Part") files, $(du -sk "$OUT/Parts/$NAME" | cut -f1) KB"
ls -l "$DIST/$NAME"
echo "total: $(( $(date +%s) - t0 )) s"
