PRESI 3D: Desktop Editor for Impress.js Presentations
======================================================

A Qt6/OpenGL desktop application for building zooming, 3D-navigated
presentations and exporting them as standalone Impress.js websites -
no browser DevTools, no hand-written HTML/CSS, no JavaScript required.

PROBLEM
-------
Impress.js produces striking 3D presentations, but authoring one means
hand-editing HTML/CSS and manually computing X/Y/Z/rotation values for
every slide. There is no visual editor for it, and mainstream tools
(PowerPoint, Keynote, Google Slides) have no concept of 3D slide space
at all - they only support flat, linear decks.

SOLUTION
--------
Presi 3D gives you a native desktop editor with a familiar 2D canvas
for laying out slide content, plus a real OpenGL 3D view for arranging
slides in space and picking their path through the presentation. When
you're done, one click exports a self-contained Impress.js website
(index.html + styles.css + assets/) that runs anywhere.

Architecture
------------
```
MainWindow
  |
  +--> SlideListPanel      (slide list, thumbnails, reorder/duplicate)
  |
  +--> EditorArea
  |       |
  |       +--> SlideEditor2D   (QPainter canvas: content authoring)
  |       +--> SlideEditor3D   (QOpenGLWidget: spatial arrangement)
  |
  +--> PropertiesPanel     (inspector: slide + element properties)
  +--> FormatBar           (text formatting toolbar)
  |
  +--> export/HtmlExporter    --> index.html + styles.css + assets/
  +--> import/HtmlImporter    <-- re-import existing Impress.js HTML
  +--> LocalHttpServer        --> serves export over http://127.0.0.1 for preview
                                   (needed for iframe/YouTube embeds, which
                                   refuse to initialize over file://)
```

Key Features
------------

SLIDE MANAGEMENT
- Create, delete, duplicate, rename, drag-and-drop reorder
- Project save/open using a folder-based project format

2D CONTENT EDITOR
- WYSIWYG text, shapes (rectangle, circle, custom via ShapePickerDialog),
  images with drag-and-drop, tables with cell editing/column resize/merge
- Charts (multiple types) via ChartRenderer, LaTeX formulas via
  LatexRenderer, iframe/web embeds, navigation buttons
- Format bar: font, size, color, alignment, bold/italic/underline/strike
  (with selectable underline style - solid/dashed/dotted/wavy - and
  color), lists, format painter, hyperlinks on text elements
- Snap/alignment guides, rotation with Ctrl-to-snap, layer ordering,
  copy/paste, per-element opacity slider
- Undo/redo (debounced, 50 steps)

3D SPATIAL VIEW
- Real OpenGL viewport: orbit / pan / zoom, click-to-pick slides
- Color-coded axis gizmo (X=red, Y=green, Z=blue)
- Position/rotation/scale editing directly from the Properties Panel
- Per-slide camera zoom and view offset - fine-tune how each slide is
  framed when it becomes the active step, independent of its raw
  position/rotation
- Per-slide visibility overrides - set the opacity of every other slide
  while a given slide is active (with a global default-inactive-opacity
  fallback), so off-path slides can fade into the background instead of
  disappearing outright

WORLD OBJECTS (3D)
- Free-floating glTF/GLB models placed directly in 3D world space,
  independent of any slide - a logo or decorative object that drifts
  past the camera as it flies between slides
- Visible/selectable only in the 3D view, with the same move/rotate/
  scale + opacity controls as slides, via the Properties Panel
- Included in HTML export using the `<model-viewer>` web component

INTERACTIVE ELEMENTS
- Checkbox and Slider elements bound to a Boolean/Number variable -
  viewers can toggle or drag them live during the presentation, and the
  bound variable updates immediately (including anywhere else it's
  referenced via `{name}`)
- Buttons support two actions: navigate to another slide, or change a
  variable (increment/decrement/set/toggle) - enabling simple
  interactive/branching presentations without any code

TIMELINE ANIMATIONS
- Per-element entry and exit animations with independent delay and
  duration
- Looping segments, click-triggered segments (in addition to automatic/
  timed ones), and variable-gated visibility (show/hide based on a
  Boolean variable)
- Freely draggable start/end keyframe states - any property (position,
  color, opacity, ...) can be set for the entry/exit state and Presi 3D
  interpolates smoothly to/from it, not just a fixed list of canned
  effects
- Fully reflected in HTML export, not just the editor preview

WEB EMBEDS DONE RIGHT
- iframe element with a URL dialog; YouTube links are auto-normalized
  to embed form (watch?v=... / youtu.be -> /embed/VIDEO_ID)
- Built-in LocalHttpServer so previews and exports run over
  http://127.0.0.1 instead of file://, avoiding embed/CORS failures

VARIABLES & LIVE TEXT
- Define named variables (text/number/boolean), reference them anywhere
  in slide text with `{name}`, use them in simple formulas
  (`{preis + 10}`, `{menge > 0}`), scope them globally or per-slide
- A set of built-in variables (current date, time, year, weekday,
  slide number, ...) work out of the box with no setup - see
  "Built-in Variables" below
- The exported website's `<title>` also accepts `{name}` placeholders

EXPORT / IMPORT
- One-click HTML export to a real Impress.js site (index.html,
  styles.css, assets/)
- Re-import: opens existing Impress.js HTML exports back into the
  editor for further editing

STAYING UP TO DATE
- Checks GitHub releases for a newer version on startup and via
  Help -> Check for Updates..., and can download + launch the new
  installer directly from the app

Unique Advantages vs Competitors
---------------------------------
```
Feature                      Presi 3D   PowerPoint  Keynote   Reveal.js/Slid.es
------------------------------------------------------------------------------
Native 3D slide space (OpenGL) Yes      No          No        No
Visual editor (no code)         Yes     Yes         Yes       No (HTML/CSS)
Exports to standalone website   Yes     No          No        Yes
Live iframe/web embeds          Yes     Limited     Limited   Yes (manual HTML)
Re-import own exported HTML     Yes     N/A         N/A       No
Chart / LaTeX / table elements  Yes     Yes         Yes       No (manual)
Free-floating 3D models (glTF)  Yes     No          No        No
Interactive, variable-bound UI  Yes     No          No        No (manual)
```

Presi 3D is the only tool in this comparison that pairs a true 3D
spatial canvas with a point-and-click editor and a standalone HTML
export - Impress.js itself requires writing markup by hand, and
mainstream office suites have no 3D slide space at all.

See FEATURES_TODO.md for the full roadmap of implemented vs. planned
features.

Technical Specifications
-------------------------
```
Language        C++17
UI Framework    Qt6 (Core, Widgets, OpenGL, OpenGLWidgets, Network)
Rendering       QOpenGLWidget (3D view), QPainter (2D canvas)
Build system    CMake >= 3.16
Package manager vcpkg (Qt6 provided via vcpkg toolchain)
Slide space     1920x1080 base coordinate system
3D coordinates  posX/posY/posZ in pixel units (typical range 0-5000);
                Y axis is inverted on HTML export (matches Impress.js)
Export format   Impress.js-compatible static site (HTML/CSS/JS + assets)
```

Dependencies
------------
- Qt 6.2+ (Core, Widgets, OpenGL, OpenGLWidgets, Network)
- CMake 3.16+
- A C++17 compiler (MSVC / Visual Studio 2022 on Windows)
- vcpkg (recommended way to obtain Qt6 on Windows)

Installation & Setup
---------------------

END USERS (WINDOWS)
```
1. Download the installer:
   https://github.com/Hannes-swd/Presi3D.exe/releases/tag/v1.0

2. Run Presi3DSetup.exe.
   The installer ships only the small Presi3D.exe; it downloads the
   required Qt runtime DLLs (~27 MB) from the release below and
   extracts them automatically, so the installer stays tiny:
   https://github.com/Hannes-swd/Presi3D.exe/releases/tag/deps-v1

3. Launch "Presi 3D" from the Start Menu or desktop icon.
```

BUILDING FROM SOURCE
```
git clone https://github.com/Hannes-swd/Presi3D.exe.git
cd Presi3D.exe

cmake -S . -B build ^
  -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows ^
  -G "Visual Studio 17 2022" -A x64

cmake --build build --config Release
```
Qt6 is resolved via vcpkg (`C:\vcpkg\installed\x64-windows`). Copy the
required Qt DLLs into the build output directory, or run
`windeployqt.exe` against the built executable:
```
C:\vcpkg\installed\x64-windows\tools\Qt6\bin\windeployqt.exe build\Release\Presi3D.exe
```

Quick Start
-----------
```
1. Launch Presi 3D -> choose "New Project" from the start dialog.
2. Add slides in the SlideListPanel.
3. Select a slide, switch to the 2D tab, and add text/shapes/images/
   tables/charts/formulas/iframes/buttons from the toolbar.
4. Switch to the 3D tab to position each slide in space (orbit/pan/
   zoom, drag to place) and set the path order.
5. Use the Properties Panel to fine-tune position, rotation, scale,
   and per-element styling.
6. File -> Export -> choose an output folder to generate a standalone
   Impress.js website (index.html, styles.css, assets/).
7. Open the exported index.html via the built-in local preview server
   to test iframe/YouTube embeds correctly (file:// will block them).
```

Built-in Variables
------------------
Type `{name}` anywhere in slide text and it's replaced with a live
value - either your own variable (created via the Variables dialog) or
one of the built-ins below, which need no setup at all and are always
up to date. Built-ins are resolved before your own variables, so avoid
naming a custom variable the same as one of these.

They're evaluated identically in the editor's live preview and in the
exported HTML/JS, so a slide showing `{now}` shows the real current
time both while editing and after export/on every page load.

| Placeholder     | Example value       | Meaning                     |
|-------------------|----------------------|------------------------------|
| `{today}`        | `07.07.2026`         | Today's date (dd.mm.yyyy)    |
| `{now}`          | `07.07.2026 14:32`   | Current date + time          |
| `{year}`         | `2026`                | Current year                 |
| `{month}`        | `7`                    | Current month (1-12)         |
| `{monthName}`    | `July`                | Current month name           |
| `{day}`          | `7`                    | Current day of month         |
| `{weekday}`      | `Tuesday`             | Current weekday name         |
| `{week}`         | `28`                   | ISO calendar week number     |
| `{time}`         | `14:32`               | Current time (HH:mm)         |
| `{hour}`         | `14`                   | Current hour (0-23)          |
| `{minute}`       | `32`                   | Current minute (0-59)        |
| `{slideNumber}`  | `3`                    | Position of the current slide (1-based) |
| `{totalSlides}`  | `12`                   | Total number of slides       |

These can also be combined with the formula syntax, e.g.
`{"Stand: " + monthName + " " + year}` -> `Stand: July 2026`, or
`{"Folie " + slideNumber + " von " + totalSlides}` for a page-number
footer. In the exported presentation, `{slideNumber}` tracks whichever
slide is currently active as you navigate.

`{slideNumber}`/`{totalSlides}` need a specific "current slide" to make
sense, so they only work in slide content (text, shapes, charts) - not
in the website title below, where they report an error instead.

**Website title**: the page `<title>` (Properties Panel -> Title, shown
in the browser tab of the exported site) also supports `{name}`
placeholders - built-ins and your own global variables both work, e.g.
`My Deck {year}` -> `My Deck 2026`. It's resolved once at export time,
so `{today}`/`{now}` are baked in as of the export, not live-updating.

Advanced Usage
---------------
- **Re-editing an export**: use File -> Import to load a previously
  exported Impress.js HTML file back into the editor.
- **Embedding web content**: paste any URL into the iframe dialog;
  YouTube watch/share links are rewritten to embed form automatically.
  Note that sites that send `X-Frame-Options`/CSP headers (e.g.
  google.com) block embedding at the browser level - this is not
  fixable from the editor.
- **Undo/Redo**: up to 50 debounced steps, covering both 2D edits and
  3D positioning.

Contributing
------------
This is currently a solo project without an established contribution
process. Open an issue on the repository if you'd like to propose a
change: https://github.com/Hannes-swd/Presi3D.exe

License
-------
No license file is currently included in this repository. All rights
reserved by the author until a license is added.
