# Features and changes

Every feature and quality-of-life change, newest last, with what it does, what
was broken on the first attempt, and how it is tested.

Each entry was built, debugged, and verified before the next one started.

---

## 1. Kind filter chips

**What it does.** The detail page shows a row of chips above the map — one per
kind of item the session actually contains, each with its count. Clicking a chip
hides every item of that kind. Turning everything back on restores the full view.

    Document 2   Archive 1   Executable 1

**Why.** `LayoutView::setKindFilter()` already existed and was tested by the e2e
tool, but nothing in the app ever called it: the filter was dead code reachable
only from a test. This connects it.

**Bug found while building it.** `rebuildKindFilter()` writes the "N of M items"
status line, but it was called *before* the code that set the plain "M items"
line, so the filtered count was overwritten a few lines later on every session
open. Fixed by calling it last, where it now owns the status text.

**Details.**
- Chips follow the inspector's kind order, so both panels agree.
- A session with fewer than two kinds hides the row rather than showing one
  useless chip.
- Only switched-off kinds are recorded in the map, so a missing key means
  "visible" rather than silently hiding a kind.

**Tested by** `check.bat filter` — chip count, chip labels and counts, filtering
down, filtering to nothing, restoring, and the single-kind case.

---

## 2. Keyboard navigation

**What it does.**

| Key | Action |
| --- | --- |
| Arrow keys | Move the selection; wraps at the edges |
| Enter | Activate the selected item |
| Esc | Clear the selection |

**Why.** The map could only be driven with a mouse. The first arrow press
selects the first tile, so the keyboard is usable without clicking first.

**Details.**
- **Map mode** picks the nearest tile *in the pressed direction*, weighting the
  off-axis distance, so a tile directly ahead beats a nearer one diagonally off
  to the side. A fixed index step would be wrong for an irregular layout.
- **List mode** is a single column: up/down walk it and wrap, left/right do
  nothing.
- When nothing lies further in a direction it wraps to the far side, which keeps
  the keys useful in a sparse layout.
- The view now takes focus on click and via Tab; without that it never received
  the keys at all.
- The selected tile is scrolled into view after each move.

**Tested by** `check.bat keys` — focus policy, first press, all four directions
changing the selection, Escape, Enter activating the right item, list wrapping,
and an empty session not inventing a selection.

---

## 3. List sorting

**What it does.** A sort control appears in list mode: choose Name, Size,
Modified or Kind, and toggle ascending/descending. The control hides in map mode,
where order is positional and a sort would mean nothing.

**Why.** The list was always alphabetical. Size is the common reason to look at
a folder, and it was the one order the app could not show.

**Details.**
- Folders always sort ahead of files, then within each group by the chosen key.
  This matches how Explorer presents them and reads better than interleaving.
- Ties break on name, then suffix, so the order is stable between rebuilds.
- Folders are not comparable to files by size (a folder's size is a capped
  directory total), so the size comparison never mixes the two groups.
- The sort survives a map/list round trip instead of resetting on toggle.

**Bugs found while building it.**
- The sort bar's `connect` used the wrong anchor text and silently did not
  apply, leaving the control always visible.
- `QComboBox` was forward-declared in the header but never included in the
  `.cpp`, so the build failed with "incomplete type".

**Tested by** `check.bat sort` — every order in both directions, folders staying
first, and the control appearing only in list mode.

---

## 4. Window geometry is remembered (QoL)

**What it does.** The library window reopens at the size and position you left
it, instead of snapping back to 1040x660 every launch.

**Details.**
- Saved on close, restored on construction.
- **Only restored if it still lands on an attached screen**, requiring a real
  overlap rather than a single pixel. Unplugging a monitor otherwise brought the
  window back entirely off-screen with no way to reach it.
- A corrupt stored value falls back to the default size and position instead of
  failing to show a window.

**Tested by** `check.bat geom` — write on close, exact restore, corrupt-value
recovery, and the minimum usable size being kept.

---


## 5. An executable icon

**What it does.** `cairn.exe` carries its own icon, so Explorer, the taskbar
menu, Alt-Tab and the Start menu show the mark instead of a blank application
box.

**Details.**
- The repository had no icon file at all. The window icon was drawn at runtime,
  which covers the window but not anything Windows shows before the app starts.
- `res\cairn.ico` is committed and compiled in through `res\cairn.rc`, with
  version information beside it.
- It is generated by `cairn_icotool` from the same drawing code as the title-bar
  mark, so the file cannot drift from what the app draws.

**Broken on the first attempt.** Qt's ICO writer emits only the one image it is
handed, so passing it a 256px pixmap produced a single-entry file that Windows
downscaled to 16 — small sizes were soft and the compact mark below was never
reached. `cairn_icotool` now assembles the ICO itself with a directory entry per
size; the file went from 270 KB holding one image to 15 KB across seven.

**Broken on the second attempt.** At 16px the full mark's stones merge into one
blob: their gaps fall inside a single pixel. `Icons::Id::CairnCompact` drops to
three stones with wider spacing for sizes under 32.

**Tested by** `check.bat suggest` — the brand tone is warm, the committed files
exist, the PNG is 256x256, the corners are transparent, and gold pixels are
actually present. A "file exists" check alone passes for a blank icon.

---

## 6. The mark opens the project page

**What it does.** The mark and the app name in the title bar are one target;
clicking either opens
[the repository](https://github.com/HiAmMilkWIthToast/Whereabouts).

**Details.**
- Painted rather than styled, because the hover state is animated and QSS cannot
  interpolate. Hovering fades in a faint gold pill and lifts the mark a pixel;
  pressing brightens further.
- Timings follow `IconButton` (150ms in, 90ms for the press), so the corner feels
  like the rest of the chrome.
- The address lives in one constant in `chrome.cpp`. Two dead QSS rules styled
  the labels this replaces and were removed; `QLabel#appMark` still carried the
  old blue accent.

**Tested by** `check.bat brand` — the mark and name are a single target, hovering
changes it gradually rather than snapping (a mid-flight frame differs from both
ends), leaving restores it, pressing is distinguishable, and the source carries
the exact address.

Both failure modes were confirmed by breaking the code on purpose: making hover
snap fails the gradual check, and changing one letter of the URL fails the
address check. The click itself was followed once end to end with the handler
recording its URL instead of launching a browser; it received exactly the
address above. That probe was then removed, because a test that opens a browser
during `check.bat` would hijack the machine running the checks.

---

## Checks added

Each feature added its own check tool, all wired into `check.bat`:

| Command | Covers |
| --- | --- |
| `check.bat filter` | kind-filter chips |
| `check.bat keys` | keyboard navigation |
| `check.bat sort` | list sorting |
| `check.bat geom` | window geometry persistence |
| `check.bat suggest` | the executable icon and the brand tone |
| `check.bat brand` | the clickable, animated mark |
| `check.bat icons` | every icon id has artwork, and buttons retint on hover |

Running `check.bat` with no argument runs everything.
