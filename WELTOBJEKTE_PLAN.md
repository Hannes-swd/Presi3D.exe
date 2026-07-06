# Plan: Frei schwebende 3D-Weltobjekte

> **Status:** Nur Idee/Dokumentation, noch nicht implementiert.

## Idee

Impress.js positioniert Folien frei im 3D-Raum (`posX/Y/Z`, `rotX/Y/Z`, `scale` – siehe `Slide` in `src/models/DataModel.h`). Der Editor hat bereits eine echte 3D-Ansicht mit Kamera-Orbit und Move/Rotate-Gizmo (`src/SlideEditor3D.cpp/.h`).

Bisher füllt dieser Raum ausschließlich Folien. Idee: zusätzlich **Objekte, die nicht an eine Folie gebunden sind**, sondern frei im Weltkoordinatensystem stehen – z. B. ein Logo, ein Icon, eine Deko-Grafik, die zwischen den Folien im Raum "hängt". Während die Kamera beim Folienwechsel durch den Raum fliegt, fliegen diese Objekte an ihr vorbei (Parallax-Effekt). Das ist mit reinem impress.js von Hand machbar (Elemente außerhalb der `.step`-Divs, fest im Weltkoordinatensystem positioniert), aber kein visueller Editor bietet dafür bisher eine UI – echtes Alleinstellungsmerkmal, analog zum Variablen-System.

## Vorgaben aus dem Gespräch (2026-07-06)

- **Kein Folien-Charakter:** Ein Weltobjekt ist keine Folie, taucht nicht in der Foliensortierung auf, hat keinen "Step"-Charakter (Präsentation hält dort nicht an).
- **Reine Kulisse, kein Kamera-Ziel:** Die Kamera navigiert ausschließlich zwischen Folien (wie bisher). Weltobjekte werden nie selbst angefahren – sie stehen einfach fest im Raum und tauchen im Sichtfeld auf/verschwinden wieder, je nachdem wo die Kamera gerade beim Folienwechsel vorbeikommt (Parallax-Hintergrund, kein Ziel-Objekt).
- **Nur im 3D-Modus sichtbar/bearbeitbar:** Im 2D-Editor (`SlideEditor2D`) erscheinen Weltobjekte nicht. Nur in `SlideEditor3D` sichtbar, auswählbar, verschiebbar, drehbar.
- **Echte 3D-Modelle, kein flaches Bild:** Import von glTF/`.glb`-Dateien (Blender-Export oder Downloads, z. B. von Sketchfab) – volle Mesh-Geometrie mit Tiefe, nicht nur eine texturierte Ebene. Das ist eine Korrektur gegenüber der ersten Version dieses Plans (dort war noch "Bild-Auswahl" vorgesehen).
- **Bewegen/Drehen wie Folien:** Gleiche Interaktion wie bei Folien im 3D-Editor (Move-Gizmo, Rotate-Gizmo), nur eben nicht an eine Folie gebunden.

## Technische Konsequenz: glTF-Import

Die bestehende 3D-Ansicht (`SlideEditor3D`) zeichnet aktuell nur texturierte Quads (Folien als Ebene). Echte glTF-Meshes zu laden ist ein deutlich größerer Schritt:

- **Lade-Bibliothek nötig** – reines Qt/OpenGL kann kein glTF parsen. Praktikabelste Optionen:
  - **Assimp** (`vcpkg install assimp`) – liest glTF/glb (und nebenbei auch OBJ/FBX/... falls später gewünscht), liefert fertige Mesh-/Material-/Textur-Daten.
  - Alternativ **tinygltf** (Header-only, nur glTF/glb) – schlanker, aber man baut Material/Texture-Handling selbst.
  - Empfehlung: mit Assimp starten, da es Fehlerquellen bei kaputten/exotischen Downloads aus dem Internet robuster abfängt als eine Minimal-Bibliothek.
- **Neue GL-Ressourcen pro Modell:** eigene VAO/VBO/EBO pro Mesh (nicht der geteilte `m_quadVAO`), Vertex-Layout mit Position/Normal/UV, ggf. mehrere Sub-Meshes pro Datei.
- **Texturen/Material:** glTF bringt PBR-Materialdaten mit (BaseColor-Textur reicht für den Anfang, Metallic/Roughness/Normal-Maps sind ein optionaler Ausbauschritt).
- **Beleuchtung:** aktuelle Shader sind vermutlich nur für flach texturierte Quads gedacht (keine Normals nötig). Für Meshes mit echter Tiefe braucht es mindestens ein einfaches Licht-Modell (z. B. ein fixes Richtungslicht + Ambient), sonst wirken die Modelle flach/falsch beleuchtet.
- **Performance/Fehlerfälle:** Downloads aus dem Internet variieren stark in Qualität (zu viele Polygone, kaputte UVs, fehlende Texturen). Import sollte robust fehlschlagen können (Fehlermeldung statt Absturz) und ggf. eine Polygon-Obergrenze/Warnung haben.

## Datenmodell-Entwurf

Neue schlanke Struktur, unabhängig von `SlideElement` (die ist auf 2D-Folieninhalt zugeschnitten):

```cpp
struct WorldObject {
    QString id;
    QString modelPath;      // Pfad zur .gltf/.glb-Datei (bei .gltf ggf. + zugehöriger .bin/Texturen-Ordner)

    float posX = 0.f, posY = 0.f, posZ = 0.f;      // impress.js-Welteinheiten, wie Slide::posX/Y/Z
    float rotX = 0.f, rotY = 0.f, rotZ = 0.f;       // Grad
    float scale = 1.f;

    float opacity = 1.f;

    WorldObject() : id(QUuid::createUuid().toString(QUuid::WithoutBraces)) {}
};
```

Anders als bei einer Bild-Ebene gibt es keine sinnvolle einzelne `width/height` mehr – die Ausdehnung ergibt sich aus der Modell-Bounding-Box × `scale`. Beim Import könnte automatisch ein sinnvoller Start-`scale` berechnet werden (Modell auf eine Zielgröße normalisieren, damit nicht jedes Modell in einer anderen Einheit ankommt).

In `Presentation` (`src/models/DataModel.h`) ergänzen:

```cpp
QVector<WorldObject> worldObjects;
```

Speicherformat: eigener JSON-Block `"worldObjects": [...]` neben `"slides"` im Projektordner-Format (siehe `Presentation::toJson`/`fromJson`, sofern dort zentral serialisiert wird – ggf. in `models/Presentation.cpp` nachsehen, wie Slides aktuell (de)serialisiert werden, und das spiegeln).

## Rendering im 3D-Editor (`SlideEditor3D`)

Weniger wiederverwendbar als bei der Bild-Variante, da echte Meshes andere GL-Ressourcen brauchen als der bestehende `m_quadVAO`/`m_quadVBO`-Pfad:

- **Neuer Lade-/Zeichenpfad:** Assimp lädt Mesh(es) + Material(ien) einer `.gltf`/`.glb`-Datei einmalig beim Import bzw. Öffnen des Projekts. Pro Mesh: eigenes VAO/VBO (Position/Normal/UV) + EBO (Indices), Textur(en) aus dem Material.
- **Model-Matrix:** analog zu `slideModel(const Slide&)` eine `worldObjectModel(const WorldObject&)` – gleiche Kombination aus Translation/Rotation/Scale, zusätzlich ein Normalisierungsfaktor aus der Modell-Bounding-Box (siehe oben).
- **Shader:** bestehender Shader (vermutlich reiner Textur-Sample-Shader für flache Quads) reicht für beleuchtete Meshes nicht – zusätzlicher (einfacher) Shader mit Normal-basierter Beleuchtung nötig, oder bestehenden Shader um einen Licht-Term erweitern.
- **Gizmos:** `renderMoveGizmo`/`renderRotateGizmo` nehmen aktuell eine `Slide&`. Sinnvoll wäre, die Gizmo-Logik auf eine gemeinsame kleine Schnittstelle (Position + Rotation, egal ob Slide oder WorldObject) umzustellen, damit sie für beide funktioniert, statt sie zu duplizieren.
- **Auswahl:** `pickSlide()` braucht ein Gegenstück `pickWorldObject()` (Ray/Bounding-Box-Test gegen die Modell-Bounding-Box, nicht gegen ein Quad), oder beide in einem gemeinsamen Hit-Test zusammenfassen (z. B. `pickAny()` das zurückgibt, ob Folie oder Weltobjekt getroffen wurde).
- **Selektionszustand:** aktuell `int m_selectedSlide`. Müsste erweitert werden um eine Unterscheidung "Folie X ausgewählt" vs. "Weltobjekt Y ausgewählt" (z. B. ein `enum class SelectionKind { None, Slide, WorldObject }` + Index).

## PropertiesPanel

Wenn im 3D-View ein Weltobjekt ausgewählt ist: kleines Property-Set anzeigen (Position X/Y/Z, Rotation X/Y/Z, Scale, Opacity, Modell wechseln, Löschen). Kein Text/Farbe/Font wie bei normalen Elementen – deutlich schlanker als das bestehende Panel für `SlideElement`.

## Hinzufügen-Flow

Neuer Menüpunkt/Toolbar-Button, nur aktiv während der 3D-Ansicht aktiv ist (analog zu bestehenden Insert-Dialogen wie `InsertIFrameDialog`): öffnet einen Datei-Dialog für `.gltf`/`.glb`, versucht den Import über Assimp, zeigt bei Fehlschlag eine verständliche Fehlermeldung (kaputte/nicht unterstützte Datei), legt bei Erfolg ein neues `WorldObject` mit Default-Transform (z. B. Kamera-Zielpunkt `m_target` als Startposition, automatisch berechneter Normalisierungs-Scale) an und selektiert es direkt zum Positionieren.

Beim Speichern des Projekts muss die Modell-Datei mit ins Projektordner-Format übernommen werden (wie es vermutlich schon für Bilder/Assets gehandhabt wird – dort nachsehen, z. B. `assets/`-Ordner-Konvention prüfen und spiegeln), sonst bricht der Link beim Verschieben des Projektordners.

## HTML-Export (`HtmlExporter.cpp`) – offener technischer Punkt

Der bisherige Plan (Weltobjekt als normaler `<div>` mit eigenem `transform: translate3d(...) rotateX/Y/Z(...) scale(...)` **ohne** `class="step"` direkt im `#impress`-Container) funktioniert für flache Bild-Ebenen, weil impress.js ohnehin nur DOM-Elemente per CSS-3D-Transform bewegt. Für **echte Mesh-Geometrie reicht das nicht** – ein `<div>` kann kein WebGL-Mesh mit Beleuchtung/Perspektive darstellen.

Zwei realistische Wege für den Export:
1. **`<model-viewer>`-Web-Component** (Googles Custom Element, lädt glTF direkt, kein eigener WebGL-Code nötig) in einen `<div>` mit dem gleichen `transform` wie oben platzieren. Einfachste Lösung, aber: `<model-viewer>` rendert intern mit eigener Kamera/Perspektive – ob sich das sauber in die impress.js-3D-Transform-Kette einfügt (insbesondere Verdeckung/Skalierung/Perspektive stimmig mit der impress.js-Kamera), ist ungeklärt und müsste ausprobiert werden.
2. **Eigene Three.js-Szene** parallel zu impress.js, die exakt dieselbe Kamera wie impress.js nachführt (impress.js exponiert die aktuelle Transform-Matrix beim Step-Wechsel) und die Weltobjekte darin rendert. Sauberer/konsistenter, aber deutlich mehr Export-JS-Code als bisher (`HtmlExporter` erzeugt aktuell nur Standard-impress.js-HTML/CSS, keine zusätzliche Rendering-Engine).

**Das ist der größte offene Risiko-Punkt des ganzen Plans** – im Editor (eigenes OpenGL) ist Mesh-Rendering machbares Standardvorgehen, aber der HTML-Export ist bei impress.js grundsätzlich DOM/CSS-3D-basiert, nicht WebGL-basiert. Vor Umsetzung sollte Variante 1 (`<model-viewer>`) an einem Mini-Testexport ausprobiert werden, bevor Zeit in Variante 2 investiert wird.

## Offene Fragen (bewusst noch nicht entschieden)

- **HTML-Export-Ansatz** (siehe oben) – wichtigste offene Frage, blockiert ggf. den ganzen Export-Teil.
- Sollen Weltobjekte pro Folie ein-/ausblendbar sein (ähnlich `Slide::visibilityOverrides`), damit z. B. ein Objekt nur zwischen Folie 3 und 4 sichtbar ist und sonst ausgeblendet? Deutlich mehr Aufwand (Objekt bräuchte dann ebenfalls einen "opacity je nach aktiver Folie"-Mechanismus).
- Lizenz/Herkunft bei Internet-Downloads ist Nutzer-Verantwortung, keine Editor-Fragestellung – aber ggf. ein Hinweistext im Insert-Dialog sinnvoll.
- Undo/Redo: muss der bestehende Undo-Stack (`Presentation`-Snapshots) automatisch mitziehen, sobald `worldObjects` Teil von `Presentation` ist – prüfen, ob der Undo-Mechanismus generisch genug ist oder ob er Weltobjekte separat kennen muss.
- Polygon-/Dateigrößen-Obergrenze, um zu verhindern, dass ein zu großes Downloaded-Modell den Editor oder den Export unbrauchbar langsam macht.

## Grober Phasenplan (bei Umsetzung)

1. **Machbarkeits-Spike zuerst:** Assimp einbinden (vcpkg), ein einzelnes Test-`.glb` laden und in `SlideEditor3D` einmal fest verdrahtet zeichnen (kein UI) – prüft, ob Lade-/Rendering-Pfad grundsätzlich funktioniert, bevor Datenmodell/UI gebaut werden.
2. **HTML-Export-Machbarkeit parallel klären:** Mini-Testexport mit `<model-viewer>` gegen die bestehende impress.js-Kamera prüfen (siehe offener Punkt oben) – Ergebnis entscheidet Variante 1 vs. 2.
3. Datenmodell: `WorldObject`-Struct + `Presentation::worldObjects` + JSON (de)serialisierung + Asset-Kopie ins Projektordner-Format.
4. Rendering: Weltobjekte im 3D-View als beleuchtete Meshes zeichnen (ohne Interaktion).
5. Auswahl + Gizmos: Klick-Picking (Bounding-Box-Ray-Test), Move/Rotate-Gizmo für Weltobjekte (Refactor der Slide-Gizmo-Logik auf eine gemeinsame Basis).
6. Hinzufügen-Flow: Datei-Dialog + Toolbar-Button (nur im 3D-Modus aktiv) + Fehlerbehandlung für kaputte Importe.
7. PropertiesPanel-Ergänzung für Weltobjekt-Auswahl.
8. HTML-Export gemäß der in Schritt 2 gewählten Variante.
9. Undo/Redo-Verifikation.
10. (Optional, spätere Phase) Sichtbarkeits-Overrides pro Folie, falls gewünscht.
