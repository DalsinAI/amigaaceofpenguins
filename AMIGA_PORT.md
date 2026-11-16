Ace of Penguins 1.4 for GFOS
============================

DJ Delorie's Ace of Penguins: eleven one-player games, each its own
program in the AceOfPenguins drawer.

  Solitaire    Klondike: build the four suits up from the aces
  FreeCell     every card dealt face up, four cells to park cards in
  Spider       two decks, build down in suit to clear the table
  Golf         clear the seven columns onto the pile, one up or down
  Canfield     the classic casino patience
  Thornq       a patience with a reserve and a stock
  Taipei       the tile-matching game (eleven layouts)
  Pegged       peg solitaire
  Mastermind   guess the hidden colours
  Merlin       turn every light on
  Minesweeper  find the mines

Click to pick up and drop, or drag; a double click sends a card home where
it can go. F1 or Help shows the game's help page (Esc closes it), Esc or q
ends the game. Each game's own keys are on its help page.

What it needs
-------------
GFOS (AmigaOS 3.2 with GFOpenUp) on a 68040 or 68060; the FPU is used
when there is one, and not needed. The games draw on ACGame's 8-bit
screen: on RTG (and GFOpenGfx where it is installed) or AGA. They need
no SDL and no GFOpenGPU. Each ships acgame.library in the drawer's Libs/.

The table is 800 x 600: the screen mode that holds it is opened, and the
table is centred on it. For another size, set it, for example

  SetEnv SAVE AceOfPenguins/Display 640x480

(the games lay their cards out for the size they get).

Installing
----------
From the archive: double-click Install-AceOfPenguins; it asks where the
drawer goes (Work:Games by default). With GFOpenUp:

  GFOpenUpTool INSTALL Parts/AceOfPenguins

puts it in SYS:Games/AceOfPenguins.

How it was ported
-----------------
Upstream draws through X11 in lib/xwin.c. That file is not built: the
port's amiga/acexwin.c draws the same calls into ACGame's 8-bit frame
(a 6x6x6 colour cube and 40 greys, every picture mapped to it once), and
amiga/acexwin_amiga.c brings ACGame's screen, mouse and keys (through
keymap.library) and topaz 8 for the text. The help pages' few Xlib calls
are answered by amiga/include/X11/Xlib.h and acexwin.c. The rest of
upstream (the games, lib/table.c, lib/stack.c, lib/help.c, the pictures
made into C by its make-imglib) is built as it is; the one patch,
amiga/patches/0001, lets make-imglib build against libpng 1.6. Taipedit
(the layout editor) is left out.

Tests: tests/host/test_ace.sh builds every game on the build machine over
a frame in memory and runs it with scripted mouse and keys (starts, draws,
deals, the stock, the help page, drags, ends on Esc).

Credits
-------
Ace of Penguins (c) 1998-2001 DJ Delorie and its contributors, GPL-2.0
or later (COPYING.txt; ReadMe-Ace is upstream's README).
Amiga port: Designed, tested and delivered by The AmiCradle Team.
Copyright (c) 2026 AmiCradle Team, GPL-2.0-or-later like Ace of Penguins.
