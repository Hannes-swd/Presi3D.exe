# Modding-Support – Refactor-Plan

Status: Diskussionsstand, noch nicht umgesetzt. Ziel: Nutzer/Modder können eigene
Element-Typen hinzufügen (Toolbar-Button, Properties-Panel-Anpassung, Rechtsklick-
Aktionen), die sowohl im Editor als auch im Web-Export (impress.js HTML) funktionieren.

## 1. Ist-Zustand (warum es aktuell nicht moddbar ist)

- `src/models/DataModel.h`: `SlideElement` ist ein **einziges monolithisches Struct**
  mit `enum Type { Text, Shape, Image, Table, Chart, Formula, IFrame, Button,
  Checkbox, Slider, Icon }`. Jeder Typ hat seine eigenen Felder direkt im selben
  Struct (kein Polymorphismus, kein Interface).
- Rendering (`SlideEditor2D`), Properties-Panel (`PropertiesPanel`) und Export
  (`src/export/HtmlExporter.h/.cpp`) verzweigen vermutlich alle über
  `switch(type)` / `if(type==...)` auf diese Typen. Ein neuer Typ von außen
  (Plugin) kann sich da nirgends "einklinken", ohne dass diese Kernstellen
  angefasst werden.
- **Wichtige Erkenntnis:** `SlideEditor3D` (der OpenGL-Renderer, 3D-Übersicht)
  rendert Slides NICHT Element-für-Element in OpenGL. Stattdessen wird jede
  Slide mit **QPainter auf ein QImage gezeichnet** und dieses Bild dann als
  Textur auf ein 3D-Quad geladen (`SlideEditor3D::buildSlideTexture`,
  `SlideEditor3D.cpp:515`). D.h. OpenGL macht nur "Textur auf Fläche kleben" –
  der eigentliche Element-Content kommt in beiden Ansichten (2D-Editor und
  3D-Übersicht) aus QPainter. **Mods müssen also nie OpenGL anfassen.**

## 2. Entscheidung: Kein Web-Viewer im Editor

Ausdrücklicher Wunsch: der OpenGL/QPainter-Rendering-Pfad im Editor soll nicht
durch einen eingebetteten Web-Viewer (QtWebEngine) ersetzt werden. Deshalb
Trennung in zwei getrennte Repräsentationen pro Mod:

- **Editor-Darstellung** (2D-Editor + 3D-Textur-Bake): rein nativ über QPainter.
- **Web-Export**: eigenes HTML/CSS/JS-Template, das nur beim Export verwendet
  wird (läuft im Browser, nicht im Editor-Prozess).

Damit bleibt der bestehende Rendering-Pfad unangetastet; nur die Dispatch-Stellen
(s.u.) müssen erweitert werden.

## 3. Vorgeschlagenes Mod-Format

Ein Mod = ein Ordner (z.B. `mods/mein-element/`) mit:

- `manifest.json` – Name, Icon, Toolbar-Button-Label, Kontextmenü-Einträge
  ```json
  {
    "id": "mein-element",
    "label": "Mein Element",
    "icon": "icon.svg",
    "contextMenu": [
      { "label": "Zufällige Farbe", "action": "randomizeColor" }
    ]
  }
  ```
- `properties.json` – Schema für automatisch generierte Properties-Panel-Felder
  (z.B. `{"type": "color", "label": "Farbe", "key": "color"}`)
- Zeichen-Spec (JSON, deklarativ: Rechtecke/Verläufe/Text + einfache Keyframes)
  – wird von einem generischen Interpreter mit echten QPainter-Aufrufen
  ausgeführt. Läuft automatisch in 2D-Editor UND 3D-Textur-Bake, da beide
  denselben QPainter-Pfad nutzen.
- Optionales Skript (ausgeführt über **QJSEngine**, Qts eingebaute JS-Engine
  ohne Browser/DOM – kein Web-Viewer) für Editor-Logik wie Kontextmenü-Aktionen.
  Zugriff nur über eine begrenzte **Action-API** (Property setzen, Element
  duplizieren/löschen etc.), kein freier Zugriff auf die App – wichtig für
  Sicherheit bei Fremdcode.
- `template.html` (+ css/js) – für den Web-Export, wird 1:1 mit ins exportierte
  Presentation-Verzeichnis kopiert bzw. mit Property-Werten befüllt.

## 4. UI-Registry: Funktionen hinzufügen/entfernen/bearbeiten + Platzierung wählen

Zusätzliche Anforderung: Modder sollen nicht nur neue Elementtypen anbieten,
sondern auch bestehende Funktionen (Buttons, Menüeinträge) hinzufügen,
entfernen oder bearbeiten können – und dabei wählen können, WO (welcher
Ribbon-Tab, welche Position) ein Button erscheint.

**Kernidee: eine zentrale Registry für UI-Aktionen**, in die eingebaute UND
Mod-Funktionen nach demselben Prinzip eingetragen werden – kein getrenntes
System für "echte" vs. "Mod"-Buttons.

- **Slots statt Pixel-Koordinaten:** Jede UI-Stelle (Ribbon-Tab `Start`,
  `Bearbeiten`, `Einfügen`, Kontextmenü, Properties-Panel-Abschnitt) ist ein
  benannter Container mit stabiler ID. Ein Mod gibt im Manifest an, in
  welchem Slot sein Eintrag erscheinen soll (z.B. `ribbon.einfuegen`) plus
  eine relative Position (z.B. "nach ID X" oder eine Sortierzahl) – keine
  freie Layout-Programmierung nötig.
- **Hinzufügen:** Mod registriert einen neuen Eintrag mit eigener eindeutiger
  ID im gewünschten Slot.
- **Entfernen:** Mod referenziert die ID eines bestehenden Eintrags (egal ob
  eingebaut oder von einem anderen Mod) und markiert ihn als versteckt.
- **Bearbeiten:** Mod referenziert eine bestehende ID und überschreibt Label,
  Icon oder Verhalten (Handler).
- **Voraussetzung:** Damit Mods auf bestehende Einträge zeigen können, müssen
  auch die eingebauten Funktionen (Undo, Save, Export, Text-Button, die
  Ribbon-Tabs selbst usw.) über dieselbe Registry laufen, statt wie aktuell
  fest in `MainWindow::setupRibbon()` / `EditorArea` verdrahtet zu sein –
  intern quasi als "System-Mods" behandelt werden.
- **Konfliktfall:** Wenn mehrere Mods dieselbe ID bearbeiten/entfernen
  wollen, braucht es eine Regel (z.B. Ladereihenfolge oder Prioritätszahl im
  Manifest entscheidet).

Das betrifft direkt die frisch gebaute Ribbon-Struktur (Commit "ribbon
interface"): `MainWindow::setupRibbon()`, `EditorArea::insertToolbarWidget()`
und die dort mit `addAction`/`mkBtn` fest verdrahteten Buttons müssten auf
Registry-Einträge umgestellt werden, damit Mods dort mitmischen können.

## 5. Nötige Kernrefactorings (keine Neuschreibung, aber gezielt invasiv)

Betroffene Dateien / Stellen, die von hart-codierten Typen auf ein
Interface/Registry-Pattern umgestellt werden müssen:

1. `src/models/DataModel.h` – `SlideElement` braucht `Type::Custom` +
   `QString modId` + `QMap<QString, QVariant> customProps` (statt für jeden
   Mod-Typ eigene Felder).
2. `src/SlideEditor2D.h/.cpp` – Rendering-Switch muss für `Custom` an den
   generischen Zeichen-Spec-Interpreter delegieren.
3. `src/SlideEditor3D.cpp` (`buildSlideTexture`) – nichts Neues nötig, sofern
   Schritt 2 sauber ist (derselbe QPainter-Pfad wird wiederverwendet).
4. `src/PropertiesPanel.h/.cpp` – muss für `Custom`-Elemente UI-Controls aus
   `properties.json` generieren statt fest verdrahteter Felder.
5. `src/export/HtmlExporter.h/.cpp` – muss für `Custom`-Elemente das
   `template.html` des Mods einbetten/kopieren statt eigener Export-Logik.
6. Speichern/Laden (Projekt-JSON-Format) – `customProps` muss mit-serialisiert
   werden; Mod-Referenz (`modId`) muss beim Laden auflösbar sein (fehlt der
   Mod → Fehlermeldung statt Crash).
7. Mod-Loader beim Programmstart: `mods/*/manifest.json` einlesen, Toolbar-
   Buttons + Kontextmenü-Einträge daraus generieren.
8. `src/MainWindow.cpp` (`setupRibbon`) + `src/EditorArea.h/.cpp`
   (`insertToolbarWidget`) – bestehende Ribbon-Buttons/Tabs müssen als
   Registry-Einträge (siehe Abschnitt 4) statt als direkte `addAction`/
   `mkBtn`-Aufrufe existieren, damit Mods sie referenzieren, verstecken oder
   überschreiben können.

## 6. Offene Fragen / noch zu klären

- Wie wird die Animation im Editor **vorgeschaut** (echte CSS-Animation gibt's
  ja nur im Web-Export)? Optionen: statische Vorschau, oder einfache Keyframe-
  Wiedergabe direkt in der QPainter-Zeichen-Spec.
- Wie genau sieht die Zeichen-Spec-Sprache aus (Umfang: nur Formen/Text/
  Verläufe, oder auch z.B. Pfade/Icons)?
- Umfang der Action-API für QJSEngine-Skripte (welche Editor-Operationen dürfen
  Mods auslösen?).
- Versionierung des Mod-Formats (Breaking Changes bei künftigen Editor-
  Updates?).
- Sicherheits-/Vertrauensmodell: dürfen Mods beliebig aus dem Internet geladen
  werden, oder nur lokal installierte?
- Konfliktregel für die UI-Registry (Abschnitt 4), wenn mehrere Mods dieselbe
  ID bearbeiten/entfernen wollen.

## 7. Aufwandseinschätzung

Kein Komplett-Rewrite. Der OpenGL-Renderer, die Timeline/Animation-Engine, das
Variablen-System und die Dialog-Infrastruktur bleiben unverändert. Der Umbau
betrifft gezielt die ~8 oben genannten Dispatch-Stellen (inkl. der neuen
Ribbon-Struktur). Realistisch: eher ein größeres Feature-Projekt als eine
Kleinigkeit, aber machbar in Etappen (z.B. zuerst nur "neue Elementtypen mit
Properties", dann UI-Registry für Platzierung/Entfernen/Bearbeiten,
Rechtsklick-Aktionen und Web-Export-Templates als spätere Ausbaustufen).
