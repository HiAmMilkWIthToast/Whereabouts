# Cairn

Cairn records where things sit in a folder, so you can look at the arrangement
later.

It saves a `.session` file: names, sizes, modified times, and the position of
each item. It never copies a file. A snapshot is a few kilobytes.

## Running it

`release\cairn.exe` is a self-contained Windows x64 build. Nothing needs to be
installed.

    cairn.exe                      the library
    cairn.exe -o desktop-1k4f2a    a saved snapshot
    cairn.exe -o C:\path\x.session a session file

You can also drop a `.session` file onto the executable.

Sessions live in `%APPDATA%\Cairn\Cairn\sessions`, not next to the executable.

The folder around the exe matters. `platforms\qwindows.dll` must stay where it
is, or Qt refuses to start.

## Using it

The first launch asks which folder to snapshot. Choose the desktop and it reads
the real icon positions from Explorer. Choose anything else and the items are
laid out on a grid, because there are no on-screen positions to record.

Each snapshot gets a handle like `desktop-1k4f2a`. The main window lists them;
each opens in its own window.

The search box filters as you type and suggests matching sessions. `Ctrl+F`
puts the cursor in it. Inside a viewer it searches that session's items.

## Keys

| Key | Action |
| --- | --- |
| `Ctrl+N` | New snapshot (library) |
| `Ctrl+O` | Open a session by handle (library) |
| `Ctrl+F` | Focus the search box |
| `Ctrl+0` or `Ctrl+=` | Fit the map to the window (viewer) |
| Arrows | Move the selection, wrapping at the edges |
| `Enter` | Open the selected item |
| `Esc` | Clear the selection, or return to the list |

Closing a window closes the program. The library and a lone viewer are each the
whole application.

## Filtering and sorting

One chip per kind of item, with a count, sits above the map. Click a chip to
hide that kind; click again to show it.

List mode adds a sort control: Name, Size, Modified or Kind, in either
direction. Folders always come before files.

## The map

Desktop coordinates come from Explorer, so the snapshot looks like the desktop
did.

Empty rows and columns are collapsed before drawing. Without that, one icon
dragged into a far corner shrinks every other tile until it is unreadable.
Order and adjacency survive; absolute distance does not. The inspector always
shows the exact recorded position.

Only a few kinds get their own glyph; the rest fall back to a plain document.
A folder's size totals everything beneath it, but the walk stops after 4000
files, so on a large tree the figure is a lower bound.

## Building

Needs Qt 6 and a C++17 compiler. On Windows with the Qt MinGW kit:

    build.bat           builds build\cairn.exe
    build.bat deploy    also stages a runnable copy in release\

`build.bat` expects Qt at `C:\Qt\6.12.0\mingw_64`. Change the four paths at the
top of the script for a different install.

Or plain CMake:

    cmake -S . -B build -DCMAKE_PREFIX_PATH=C:/Qt/6.12.0/mingw_64
    cmake --build build

`build.bat` and `check.bat` exist because the source is the thing you build;
`release/` is only the result of running them. Editing source does not change
the shipped exe until you rebuild and deploy.

### The address on the mark

The gold mark in the title bar opens a project page when the build is given an
address. Put one in `local.cmake` beside `CMakeLists.txt`:

    set(CAIRN_PROJECT_URL "https://github.com/you/your-project")

That file is ignored by git, so the address stays out of the repository. With
no address the mark is a plain logo: no hand cursor, no tooltip, no link.

### The executable icon

`res\cairn.ico` is committed and compiled in through `res\cairn.rc`, so
Explorer and the taskbar show it before the app starts. The same code draws the
mark in the title bar, so the two cannot drift apart. After changing the mark:

    cmake --build build --target cairn_icotool
    build\tools\cairn_icotool.exe

That writes `res\cairn.ico` in seven sizes, 16 to 256, and `res\cairn-256.png`.
Below 32px the icon drops some stones, because the gaps fall inside a single
pixel and the stack turns into a blob.

## Checking it

`check.bat` builds and runs every check tool. No argument runs them all; a name
runs one.

| Command | Covers |
| --- | --- |
| `check.bat filter` | kind-filter chips |
| `check.bat keys` | keyboard navigation |
| `check.bat sort` | list sorting |
| `check.bat geom` | window geometry persistence |
| `check.bat suggest` | the executable icon and the brand colour |
| `check.bat brand` | the mark, and the address it opens |
| `check.bat icons` | every icon id has artwork; buttons retint on hover |

Each check prints `PASS` or `FAIL` and exits non-zero on failure, so the suite
works from a script.

## Code layout

| Path | Purpose |
| --- | --- |
| `src/theme.*` | colours, metrics, the stylesheet |
| `src/icons.*` | SVG icons, rasterised and tinted when painted |
| `src/icontextbutton.*` | a button that paints its own glyph and label |
| `src/filterchip.*` | the kind-filter pill |
| `src/chrome.*` | frameless window, title bar, sidebar, status bar |
| `src/sessionmodel.*` | `Entry`, `Session`, kind detection, JSON |
| `src/desktoppositions.*` | Explorer icon positions via the shell view |
| `src/sessionstore.*` | capture, save, load, the library |
| `src/layoutview.*` | the map and the list |
| `src/inspector.*` | the right-hand detail panel |
| `src/onboarding.*` | the first-run dialog |
| `src/mainwindow.*` | the library and the detail page |
| `src/sessionwindow.*` | one session in its own window |
| `res/` | the icon and the script that embeds it |

## Licence

MIT. See [LICENSE](LICENSE).
