# Ace of Penguins for GFOS

The AmiCradle Team's port of DJ Delorie's Ace of Penguins, eleven
one-player card and tile games (Solitaire, FreeCell, Spider, Golf,
Canfield, Thornq, Taipei, Pegged, Mastermind, Merlin and Minesweeper), to
GFOS: AmigaOS 3.2 with GFOpenUp, 68040/68060. The games draw on ACGame's
8-bit screen (acgame.library): no SDL and no GFOpenGPU. It ships as a
release archive with an Installer and as the GFOpenUp part
Parts/AceOfPenguins.

Upstream is fetched at build time (package/ace.lock) and patched
(amiga/patches); this repository holds only the Amiga port. See
[AMIGA_PORT.md](AMIGA_PORT.md).

GPL-2.0-or-later, like Ace of Penguins. Copyright (c) 2026 AmiCradle Team.
