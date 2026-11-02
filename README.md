# Cairn

A dark, compact desktop app that records where everything sits in a folder.

A cairn is a stack of stones that marks a place — that is all this does: it
remembers the *arrangement* of a folder so you can look at it later.

Source, issues and releases live at
[github.com/HiAmMilkWIthToast/Whereabouts](https://github.com/HiAmMilkWIthToast/Whereabouts).
The mark in the top-left corner of every window opens it.

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

Sessions are kept under your application data folder, not beside the executable:

    %APPDATA%\Cairn\Cairn\sessions

The search box above the list filters by name as you type and suggests matching
sessions; `Ctrl+F` puts the cursor in it. In a viewer it searches the items in
that session instead.

## Keys

| Key | Action |
| --- | --- |
| `Ctrl+N` | New snapshot (library) |
| `Ctrl+O` | Open a session by its handle (library) |
| `Ctrl+F` | Focus the item search |
| `Ctrl+0` or `Ctrl+=` | Fit the map to the window (viewer) |
| Arrow keys | Move the selection between items, wrapping at the edges |
| `Enter` | Activate the selected item |
| `Esc` | Clear the selection, or go back to the session list |

Closing a window ends the app: the library and a lone viewer are each the whole
program. Minimize, maximize/restore and close live on the title bar of every
window, and the maximize glyph switches to Restore while the window is maximized.

## Filtering and sorting

Above the map, one chip per kind of item in the session shows its count. Click a
chip to hide that kind; click it again to bring it back.

In list mode a sort control appears: Name, Size, Modified or Kind, ascending or
descending. Folders always sort ahead of files.

See [FEATURES.md](FEATURES.md) for what each feature does and how it is tested.

## Building

Requires Qt 6 and a C++17 compiler. On Windows with the Qt MinGW kit:

    build.bat           # builds build\cairn.exe
    build.bat deploy    # also stages the runnable copy in release\

`build.bat` expects Qt at `C:\Qt\6.12.0\mingw_64`; edit the paths at the top of
the script for another install.

The executable icon is committed rather than built: `res\cairn.ico` is compiled
in through `res\cairn.rc`, so Explorer, the taskbar and Alt-Tab show it without
the app having to start first. It is drawn by the same code that paints the mark
in the title bar, so the two cannot disagree -- regenerate it after changing the
mark:

    cmake --build build --target cairn_icotool
    build\tools\cairn_icotool.exe

That writes `res\cairn.ico` (seven sizes, 16 through 256) and
`res\cairn-256.png`. Below 32px the icon uses fewer stones, because the full
mark's gaps fall inside a single pixel and the stack merges into one blob.

Or with CMake directly:

    cmake -S . -B build -DCMAKE_PREFIX_PATH=C:/Qt/6.12.0/mingw_64
    cmake --build build

## Layout of the code

| Path | Purpose |
| --- | --- |
| `src/theme.*` | colours, metrics, the single stylesheet |
| `src/icons.*` | inline SVG icons, rasterised and tinted at paint time |
| `src/chrome.*` | frameless window, title bar, sidebar, status bar |
| `res/` | the compiled-in icon, and the resource script that embeds it |
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
document glyph. A folder's size is the total of everything beneath it, not just
the files sitting directly inside -- but the walk stops after 4000 files, so a
snapshot of a deep tree is still quick. On a folder that large the number is a
lower bound, not an exact total.

## Licence

MIT. See [LICENSE](LICENSE) — use it, change it, ship it.
