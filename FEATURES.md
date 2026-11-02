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

## Checks added

Each feature added its own check tool, all wired into `check.bat`:

| Command | Covers |
| --- | --- |
| `check.bat filter` | kind-filter chips |
| `check.bat keys` | keyboard navigation |
| `check.bat sort` | list sorting |
| `check.bat geom` | window geometry persistence |

Running `check.bat` with no argument runs everything.
