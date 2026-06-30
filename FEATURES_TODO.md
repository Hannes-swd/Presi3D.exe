# Feature-Roadmap – Impress.js Editor

> **Bereits implementiert:** Slides erstellen/löschen/duplizieren/umbenennen/verschieben, 2D-Editor (Text WYSIWYG, alle Grundformen + ShapeText, Bild + Drag&Drop, Tabelle mit Zellbearbeitung/Spaltenresize/Merge, alle Diagrammtypen), 3D-Ansicht (OpenGL-Gizmo), Properties Panel, HTML-Export, FormatBar (Schriftart/Größe/Farbe/Ausrichtung/Bold/Italic/Underline/Strike/Listen/FormatPainter), Snap/Ausrichtungslinien, Rotation (Ctrl=Einrasten), Ebenenreihenfolge, Kopieren/Einfügen, Eintrittanimation (Daten + HTML-Export)

---

## Priorität 1 – Grundlagen (ohne diese ist die App nicht nutzbar)

| Feature | Beschreibung |
|---|---|
| **Datei speichern / öffnen** | JSON-Format für das gesamte Projekt; Ctrl+S / Ctrl+O |
| **Undo / Redo** | Ctrl+Z / Ctrl+Y, Command-Pattern, mind. 50 Schritte |
| **Autosave** | Alle X Minuten automatisch speichern, Wiederherstellung nach Absturz |
| **Tastaturkürzel** | Vollständiges Shortcut-System (Kopieren, Einfügen, Auswählen, Löschen …) |
| **Mehrfachauswahl** | Mehrere Elemente gleichzeitig markieren, verschieben, löschen |
| **Gruppenbildung** | Elemente zu Gruppen zusammenfassen und gemeinsam bearbeiten |
| **Lineale und Führungslinien** | Einblendbare Lineale, ziehbare Hilfslinien |
| **Zoom im 2D-Editor** | Ctrl+Mausrad zum Rein-/Rauszoomen in die Arbeitsfläche |

---

## Priorität 2 – Inhaltselemente

| Feature | Beschreibung |
|---|---|
| **Linien als eigenes Element** | Gerade Linien mit konfigurierbaren Pfeilspitzen an beiden Enden |
| **Code-Block** | Monospace-Box mit Syntax-Highlighting (C++, Python, JS usw.) |
| **Gleichungen / Mathe** | LaTeX-Eingabe, gerenderte Formeldarstellung |
| **Icons** | Eingebaute Icon-Bibliothek (z. B. Material Icons, Font Awesome), SVG-Import |
| **SVG-Dateien** | SVG direkt importieren und als Vektorgrafik auf der Folie platzieren |
| **Videos** | Lokale Videodateien einbetten, Autoplay-/Loop-Optionen |
| **GIFs** | Animierte GIFs auf Folien platzieren |
| **Audio** | Hintergrundmusik oder Sound-Effekte pro Folie |
| **YouTube / iFrame** | Externen Webinhalt einbetten (wird im exportierten HTML funktionieren) |
| **Hyperlinks** | Text oder Elemente als Klick-Link definieren |
| **QR-Code-Generator** | URL eingeben → QR-Code wird direkt als Element eingefügt |

---

## Priorität 3 – Textformatierung

| Feature | Beschreibung |
|---|---|
| **Zeilenabstand / Zeichenabstand** | Einstellbar im Properties Panel |
| **Google Fonts** | Google Fonts Integration (lokale Systemschriften bereits vorhanden) |
| **Gradient-Text** | Text mit Farbverlauf füllen |
| **Text-Schatten** | Konfigurierbare Schatten hinter Text |
| **Emoji-Picker** | Emoji-Auswahldialog im Texteditor |

---

## Priorität 4 – Design / Styling

| Feature | Beschreibung |
|---|---|
| **Themes / Templates** | Vorgefertigte Farbschemas und Layouts, eigene Themes speichern |
| **Master-Slide / Vorlage** | Eine Basis-Folie definieren, die auf alle anderen angewendet wird |
| **Globale Farbpalette** | Projektweite Farben definieren, überall nutzbar |
| **Hintergrundverläufe** | Linearer und radialer Gradient als Folienhintergrund |
| **Hintergrundbilder** | Bild als Folienhintergrund, mit Deckkraft und Positionsoptionen |
| **Animierte Hintergründe** | Partikelsysteme, CSS-Animationen (Wellen, Sterne usw.) |
| **Box-Shadow** | Konfigurierbare Schatten hinter Elementen |
| **Deckkraft / Transparenz** | Dedizierter Opacity-Slider pro Element |
| **Farben mit Dropper** | Farbe direkt von der Folie aufnehmen (Color-Picker mit Pipette) |

---

## Priorität 5 – Diagramm-Erweiterungen

| Feature | Beschreibung |
|---|---|
| **CSV-Import für Diagramme** | Diagramm-Daten direkt aus CSV-Dateien laden |
| **Dynamische Diagramme** | Diagramm-Daten können Variablen als Quelle nutzen |

---

## Priorität 6 – Variablen *(einzigartiges Alleinstellungsmerkmal)*

Kein anderes Präsentationsprogramm hat das – eine echte Stärke dieser App.

| Feature | Beschreibung |
|---|---|
| **Variablen-Editor** | Panel oder Dialog: Liste aller Variablen mit Name + Wert; keine Coding-Kenntnisse nötig |
| **Variable einfügen** | Im Texteditor `{{variablenname}}` einfügen (oder per Dropdown auswählen) |
| **Live-Vorschau** | Im Editor wird sofort der Wert angezeigt, nicht der Platzhalter |
| **Variable Typen** | Text, Zahl (Integer/Float), Datum, Boolean (ja/nein), Farbe |
| **Berechnungen** | Einfache Formeln: `{{preis * 1.19}}`, `{{a + b}}`, `{{max - min}}` |
| **Datum-Variablen** | `{{heute}}`, `{{jetzt}}` – aktuelles Datum/Uhrzeit beim Präsentieren |
| **Import aus CSV/JSON** | Variablenwerte aus externer Datei laden (z. B. aktuelle Verkaufszahlen) |
| **Globale vs. lokale Variablen** | Global = gilt für die ganze Präsentation; lokal = nur diese Folie |
| **Bedingte Sichtbarkeit** | Element nur anzeigen wenn `{{zeige_abschnitt}} == true` |

---

## Priorität 7 – Animationen

| Feature | Beschreibung |
|---|---|
| **Animations-UI** | UI zum Setzen von Eintrittanimationstyp, Delay und Dauer pro Element (Daten + HTML bereits vorhanden) |
| **Austrittsanimationen** | Fade Out, Slide Out, Shrink usw. |
| **Folienübergänge (UI)** | Impress.js-Übergänge konfigurieren (bereits im HTML, UI fehlt) |
| **Timeline-Editor** | Visuelle Timeline pro Folie: Wann startet welche Animation? |
| **Trigger** | Animation startet bei Klick, nach X Sekunden, oder wenn vorherige endet |
| **Loop-Animationen** | Elemente dauerhaft animieren (pulsieren, rotieren) |
| **Scroll-Animationen** | Elemente erscheinen beim Scrollen in der Webanwendung |
| **3D-Kamerafahrten** | Animierter Pfad durch den 3D-Raum zwischen Folien |

---

## Priorität 8 – Präsentationsmodus

| Feature | Beschreibung |
|---|---|
| **Vollbildmodus** | Präsentation im Vollbild starten (F5) |
| **Presenter-Ansicht** | Zweiter Bildschirm: Notizen + Timer + Vorschau nächste Folie |
| **Notizen pro Folie** | Sprechernotizen eingeben (sichtbar nur im Presenter-Modus) |
| **Timer / Stoppuhr** | Countdown oder Aufwärtszähler beim Präsentieren |
| **Laserpointer** | Maus als Pointer darstellen (roter Kreis) |
| **Zoom-Geste** | In eine Folie reinzoomen während der Präsentation |
| **Nicht-lineare Navigation** | Verlinkungen zwischen beliebigen Folien (Verzweigungen) |
| **Foliensortieransicht** | Tabellarischer Überblick aller Folien, per Drag & Drop anordnen |

---

## Priorität 9 – Export / Import

| Feature | Beschreibung |
|---|---|
| **PDF-Export** | Jede Folie als PDF-Seite rendern |
| **PNG/JPG-Export** | Einzelne Folien als Bilddateien exportieren |
| **PowerPoint-Import (.pptx)** | PPTX einlesen und in das interne Format umwandeln |
| **PowerPoint-Export (.pptx)** | Presentation als PPTX speichern |
| **Keynote-Import (.key)** | Apple Keynote importieren |
| **HTML-Import** | Vorhandene Impress.js-HTML-Dateien öffnen (HtmlImporter bereits vorhanden) |
| **Zip-Export** | HTML + Assets als fertige ZIP-Datei |
| **Thumbnail-Export** | Alle Folien als kleines Vorschaubild exportieren |

---

## Priorität 10 – Zusammenarbeit & Verwaltung

| Feature | Beschreibung |
|---|---|
| **Kommentare / Annotationen** | Notizzettel auf Folien hinterlassen, für Review-Prozesse |
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

1. **Variablen ohne Code** – absolutes Alleinstellungsmerkmal, kein Konkurrent hat das
2. **Datei speichern / öffnen** – grundlegende Voraussetzung für produktiven Einsatz
3. **Undo / Redo** – essentiell für fehlerfreies Arbeiten
4. **Mehrfachauswahl** – wichtig für effizientes Layout-Bearbeiten
5. **Animations-UI** – Infrastruktur (Daten + HTML-Export) bereits vorhanden
