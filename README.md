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
- Format bar: font, size, color, alignment, bold/italic/underline/strike,
  lists, format painter
- Snap/alignment guides, rotation with Ctrl-to-snap, layer ordering,
  copy/paste, entrance animations
- Undo/redo (debounced, 50 steps)

3D SPATIAL VIEW
- Real OpenGL viewport: orbit / pan / zoom, click-to-pick slides
- Color-coded axis gizmo (X=red, Y=green, Z=blue)
- Position/rotation/scale editing directly from the Properties Panel

WEB EMBEDS DONE RIGHT
- iframe element with a URL dialog; YouTube links are auto-normalized
  to embed form (watch?v=... / youtu.be -> /embed/VIDEO_ID)
- Built-in LocalHttpServer so previews and exports run over
  http://127.0.0.1 instead of file://, avoiding embed/CORS failures

EXPORT / IMPORT
- One-click HTML export to a real Impress.js site (index.html,
  styles.css, assets/)
- Re-import: opens existing Impress.js HTML exports back into the
  editor for further editing

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
