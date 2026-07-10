# Feature-Roadmap – Impress.js Editor

> **Bereits implementiert:** Slides erstellen/löschen/duplizieren/umbenennen/verschieben, 2D-Editor (Text WYSIWYG, alle Grundformen + ShapeText, Bild + Drag&Drop, Tabelle mit Zellbearbeitung/Spaltenresize/Merge, alle Diagrammtypen, iFrame/Web-Einbettung, LaTeX-Formeln), 3D-Ansicht (OpenGL-Gizmo), Properties Panel, HTML-Export, HTML-Import, FormatBar (Schriftart/Größe/Farbe/Ausrichtung/Bold/Italic/Underline/Strike/Listen/FormatPainter/Hyperlinks), Snap/Ausrichtungslinien, Rotation (Ctrl=Einrasten), Ebenenreihenfolge, Kopieren/Einfügen, Timeline-Animationssystem (Eintritt/Austritt pro Folienelement, Verzögerung + Dauer, Loop, Klick-Trigger, variablengebundene Sichtbarkeit, frei einstellbare Start-/Endzustände per Drag, HTML-Export + Properties-Panel-UI – siehe ANIMATION_PLAN.md), Undo/Redo (debounced, 50 Schritte), Datei speichern/öffnen (Projektordner-Format), Variablen-System (Editor-Dialog, Text/Zahl/Boolean, Berechnungen, global/lokal, Live-Vorschau im Editor + in Diagrammen, eingebaute Variablen `{today}`/`{now}`/`{year}`/`{month}`/`{monthName}`/`{day}`/`{weekday}`/`{week}`/`{time}`/`{hour}`/`{minute}`/`{slideNumber}`/`{totalSlides}`, Variablen auch im Webseiten-`<title>` nutzbar – siehe README.md „Built-in Variables")

---

## Priorität 1 – Grundlagen (ohne diese ist die App nicht nutzbar)

| Feature | Beschreibung |
|---|---|
| **Mehrfachauswahl** | Mehrere Elemente gleichzeitig markieren, verschieben, löschen (aktuell nur Einzelauswahl `m_selectedElem`) |
| **Gruppenbildung** | Elemente zu Gruppen zusammenfassen und gemeinsam bearbeiten |
| **Lineale und Führungslinien** | Einblendbare Lineale, ziehbare Hilfslinien (aktuell nur temporäre Snap-Ausrichtungslinien beim Ziehen, keine Lineale, keine dauerhaften Guides) |

---

## Priorität 2 – Inhaltselemente

| Feature | Beschreibung |
|---|---|
| **Code-Block** | Monospace-Box mit Syntax-Highlighting (C++, Python, JS usw.) |
| **Icons** | Eingebaute Icon-Bibliothek (z. B. Material Icons, Font Awesome), SVG-Import |
| **SVG-Dateien** | SVG direkt importieren und als Vektorgrafik auf der Folie platzieren |
| **Videos** | Lokale Videodateien einbetten, Autoplay-/Loop-Optionen |
| **GIFs** | Animierte GIFs auf Folien platzieren |
| **Audio** | Hintergrundmusik oder Sound-Effekte pro Folie |

---

## Priorität 3 – Textformatierung

| Feature | Beschreibung |
|---|---|
| **Zeilenabstand / Zeichenabstand** | Einstellbar im Properties Panel |
| **Gradient-Text** | Text mit Farbverlauf füllen |
| **Text-Schatten** | Konfigurierbare Schatten hinter Text |
| **Emoji-Picker** | Emoji-Auswahldialog im Texteditor |

---

## Priorität 4 – Design / Styling

| Feature | Beschreibung |
|---|---|
| **Box-Shadow** | Konfigurierbare Schatten hinter Elementen |
| **Deckkraft / Transparenz** | Dedizierter Opacity-Slider pro Element |

---

## Priorität 7 – Animationen

| Feature | Beschreibung |
|---|---|
| **Folienübergänge (UI)** | Impress.js-Übergänge konfigurieren (bereits im HTML, UI fehlt) |
| **3D-Kamerafahrten** | Animierter Pfad durch den 3D-Raum zwischen Folien |

---

## Priorität 8 – Präsentationsmodus

| Feature | Beschreibung |
|---|---|
| **Laserpointer** | Maus als Pointer darstellen (roter Kreis) |
| **Zoom-Geste** | In eine Folie reinzoomen während der Präsentation |

---

## Priorität 9 – Export / Import

| Feature | Beschreibung |
|---|---|
| **PDF-Export** | Jede Folie als PDF-Seite rendern |
| **PNG/JPG-Export** | Einzelne Folien als Bilddateien exportieren |
| **PowerPoint-Import (.pptx)** | PPTX einlesen und in das interne Format umwandeln |
| **PowerPoint-Export (.pptx)** | Presentation als PPTX speichern |
| **Keynote-Import (.key)** | Apple Keynote importieren |
| **Zip-Export** | HTML + Assets als fertige ZIP-Datei |
| **Thumbnail-Export** | Alle Folien als kleines Vorschaubild exportieren |

---

## Priorität 10 – Zusammenarbeit & Verwaltung

| Feature | Beschreibung |
|---|---|
| **Versions-Historie** | Frühere Speicherstände anzeigen und wiederherstellen |
| **Präsentation schützen** | Passwortschutz für Export |
| **Wasserzeichen** | Logo oder Text als Wasserzeichen einblenden |
| **Metadaten** | Autor, Titel, Beschreibung, Erstellungsdatum |

---

## Priorität 11 – KI-Features *(moderne Ergänzung)*

| Feature | Beschreibung |
|---|---|
| **KI-Texterstellung** | Prompt eingeben → Text wird direkt als Element eingefügt |
| **KI-Layout-Vorschlag** | KI schlägt Anordnung von Elementen vor |
| **KI-Zusammenfassung** | Langen Text auf Bullet-Points kürzen |
| **KI-Bildgenerierung** | Bild aus Textbeschreibung generieren (Stable Diffusion oder API) |
| **Auto-Design** | KI passt Farben/Schriften automatisch an (basierend auf Theme) |

---

## Priorität 12 – Technische / Performance-Features

| Feature | Beschreibung |
|---|---|
| **Lazy Loading** | Große Präsentationen laden Folien nur bei Bedarf |
| **Asset-Komprimierung** | Bilder werden beim Export automatisch optimiert |
| **Plugin-System** | Externe Erweiterungen laden (eigene Element-Typen, Exportformate) |
| **Befehlspalette** | Ctrl+P → alle Aktionen über Suchfeld erreichbar (wie VS Code) |
| **Dark Mode / Light Mode** | Umschaltbares UI-Theme |
| **Mehrsprachigkeit (i18n)** | UI in Deutsch, Englisch usw. |
| **Barrierefreiheit** | Alt-Texte für Bilder, Tab-Navigation, Screenreader-Unterstützung |

---

## Zusammenfassung: Die wichtigsten noch fehlenden Features

1. **Mehrfachauswahl** – wichtig für effizientes Layout-Bearbeiten
3. **Gruppenbildung** – baut auf Mehrfachauswahl auf
4. **Lineale, Führungslinien & Zoom im 2D-Editor** – grundlegende Editier-Komfortfunktionen
