# Cairn

A dark, compact desktop app that records where everything sits in a folder.

A cairn is a stack of stones that marks a place — that is all this does: it
remembers the *arrangement* of a folder so you can look at it later.

## What it is, and what it is not

A Cairn session records, for every item in a folder:

- name, type, size, modified time
- the item's position
- for shortcuts and symlinks, what they point at
- for the desktop, the **real** icon coordinates read from Explorer

It does **not** copy any file. A `.session` file holds a few kilobytes of
metadata — a map, not a backup. No file contents are ever read or stored.

## Running it

`release\` is a self-contained build for Windows x64. Run `release\cairn.exe`
directly — it needs no Qt install and nothing else on the machine:

    cairn.exe                      # the library
    cairn.exe -o desktop-1k4f2a    # a saved snapshot

The folder holds the executable, the Qt and MinGW runtime DLLs, and
`platforms\qwindows.dll`. That last one is not optional: Qt refuses to start
without it, and it must sit in a `platforms` folder next to the exe.

## Using it

On first launch it asks which folder to snapshot. Pick the desktop and it reads
the true icon positions from the shell; pick any other folder and it lays the
items out on a grid, because those have no on-screen positions to record.

Each snapshot gets a handle such as `desktop-1k4f2a`. The main window lists the
snapshots you have taken, and any of them can be opened in its own window:

    cairn -o desktop-1k4f2a        # by handle
    cairn -o C:\path\to\x.session  # by file

Drag a `.session` file onto the executable, or pass it as the first argument.
That is the whole sharing story: send someone the small file.

## Keys

| Key | Action |
| --- | --- |
| `Ctrl+N` | New snapshot (library) |
| `Ctrl+O` | Open a session by its handle (library) |
| `Ctrl+F` | Focus the item search |
| `Ctrl+0` | Fit the map to the window (viewer) |
| `Esc` | Back to the session list (library) |

Closing a window ends the app: the library and a lone viewer are each the whole
program. Minimize, maximize/restore and close live on the title bar of every
window, and the maximize glyph switches to Restore while the window is maximized.

## Building

Requires Qt 6 and a C++17 compiler. On Windows with the Qt MinGW kit:

    build.bat           # builds build\cairn.exe
    build.bat deploy    # also stages the runnable copy in release\

`build.bat` expects Qt at `C:\Qt\6.12.0\mingw_64`; edit the paths at the top of
the script for another install.

Or with CMake directly:

    cmake -S . -B build -DCMAKE_PREFIX_PATH=C:/Qt/6.12.0/mingw_64
    cmake --build build

## Layout of the code

| Path | Purpose |
| --- | --- |
| `src/theme.*` | colours, metrics, the single stylesheet |
| `src/icons.*` | inline SVG icons, rasterised and tinted at paint time |
| `src/chrome.*` | frameless window, title bar, sidebar, status bar |
| `src/sessionmodel.*` | `Entry`, `Session`, type detection, JSON |
| `src/desktoppositions.*` | real Explorer icon positions via the shell view |
| `src/sessionstore.*` | capture, save, load, the session library |
| `src/layoutview.*` | the map and the list |
| `src/inspector.*` | the right-hand detail panel |
| `src/onboarding.*` | the first-run "snapshot a folder" dialog |
| `src/mainwindow.*` | the library and the detail page |
| `src/sessionwindow.*` | a single session in its own window |

## Notes on the layout map

Desktop coordinates come from Explorer, so a snapshot looks like the desktop did.
Empty columns and rows are collapsed before drawing: a single icon dragged to the
far corner would otherwise shrink every other tile to the point of being
unreadable. Order and adjacency survive, absolute distance does not — and the
inspector always shows the exact recorded position of the selected item.

Icons are only generated for a handful of kinds; the rest fall back to a generic
document glyph. Sizes shown for folders are the total of the files directly
inside, not a recursive sum, so a snapshot stays fast on a large tree.
