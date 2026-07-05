# Update-System

Erklärt, wie das In-App-Update von Presi3D funktioniert und wie du in Zukunft
eine neue Version veröffentlichst.

## Wie es funktioniert

Presi3D besteht aus zwei getrennten, unabhängig versionierten Teilen auf GitHub
Releases (Repo `Hannes-swd/Presi3D.exe`):

1. **App-Releases** (`v1.0`, `v1.1`, ... aktuell `v1.4`)
   Enthalten `Presi3DSetup.exe` — den Inno-Setup-Installer mit der kompilierten
   `Presi3D.exe` (klein, ~2–3 MB).

2. **Deps-Releases** (`deps-v1`, `deps-v2`, `deps-v3`)
   Enthalten `Presi3D-deps.zip` — die Qt-Runtime-DLLs (~27 MB: Qt6Core,
   Qt6Widgets, Qt6Gui, Qt6Network, Qt6OpenGL, Qt6Svg, TLS-Backend-Plugin,
   Windows-Plattform-Plugin usw.). Wird vom Installer bei der Installation
   automatisch heruntergeladen und entpackt (siehe `installer/Presi3D.iss`,
   `[Code]`-Abschnitt). Deshalb bleibt der Installer selbst winzig.

   Die Deps ändern sich nur, wenn sich die **Laufzeit-Abhängigkeiten** ändern
   (z.B. ein neues Qt-Modul dazukommt). Aktuell aktuell: **`deps-v3`**
   (enthält Qt6Svg + TLS-Backend-Plugin `qschannelbackend.dll` — ohne das
   scheitert jede HTTPS-Anfrage, u.a. der Update-Check, lautlos).

### In-App-Updater (`src/UpdateChecker.h/.cpp`)

- Fragt `https://api.github.com/repos/Hannes-swd/Presi3D.exe/releases/latest` ab
- Vergleicht `tag_name` (z.B. `v1.4`) mit der einkompilierten `APP_VERSION`
- Sucht im Release-Asset-Array nach `Presi3DSetup.exe` und dessen
  `browser_download_url`
- Bei neuerer Version: `updateAvailable(version, downloadUrl)` Signal
- Auf Wunsch (`downloadAndInstall()`): lädt den Installer in den Temp-Ordner,
  startet ihn per `QProcess::startDetached`, und die App beendet sich selbst
  (`QApplication::quit()`), damit der Installer die exe überschreiben kann

Eingebunden an zwei Stellen:

| Stelle | Datei | UI |
|---|---|---|
| Start-Bildschirm (vor dem Öffnen eines Projekts) | `src/dialogs/StartDialog.cpp` | Button unten links "Check for Updates", wird bei Update rot/fett |
| Haupt-Editor-Fenster | `src/MainWindow.cpp` | ⋮-Symbol oben rechts in der Menüleiste, roter Punkt bei Update |

Beide prüfen automatisch ~1,5–2 Sek. nach dem Öffnen im Hintergrund (still,
nur Statusleiste/Button-Text ändert sich — kein nerviges Popup).

### Versionsnummer

Zentral in `CMakeLists.txt`:
```cmake
project(Presi3D VERSION 1.4 LANGUAGES CXX)
```
wird über `target_compile_definitions(... APP_VERSION="${PROJECT_VERSION}")`
als Makro `APP_VERSION` ins Programm kompiliert (genutzt in `main.cpp` und
`UpdateChecker.cpp`).

## Neue Version veröffentlichen (Schritt für Schritt)

Normalfall: nur Code geändert, **keine neuen Qt-Module/DLLs** nötig.

1. **Version bumpen** an zwei Stellen (müssen übereinstimmen):
   - `CMakeLists.txt`: `project(Presi3D VERSION X.Y LANGUAGES CXX)`
   - `installer/Presi3D.iss`: `#define MyAppVersion "X.Y"`

2. **Neu konfigurieren & bauen** (Release-Build):
   ```powershell
   cmake --build build --config Release --target Presi3D
   ```

3. **Installer neu kompilieren** (Inno Setup 7 — Inno Setup 6 kann diese
   `.iss`-Datei wegen `ArchiveExtraction=auto` NICHT kompilieren, unbedingt
   Version 7 nutzen):
   ```powershell
   & "C:\Program Files\Inno Setup 7\ISCC.exe" "installer\Presi3D.iss"
   ```
   Ergebnis: `installer\Output\Presi3DSetup.exe`

4. **Committen & pushen:**
   ```bash
   git add -A
   git commit -m "..."
   git push origin main
   ```

5. **GitHub-Release anlegen** (Tag = `vX.Y`, muss zu `MyAppVersion` passen,
   sonst erkennt der In-App-Updater die Version falsch):
   ```bash
   gh release create vX.Y "installer/Output/Presi3DSetup.exe" \
     --repo Hannes-swd/Presi3D.exe \
     --title "Presi3D vX.Y" \
     --notes "..."
   ```

Damit sieht der In-App-Updater sofort die neue Version (kein weiterer Schritt
nötig — er fragt live die GitHub-API ab).

### Sonderfall: neue Laufzeit-Abhängigkeit (neues Qt-Modul, neue DLL)

Passiert, wenn `CMakeLists.txt` ein neues `find_package(Qt6 ... COMPONENTS ...)`
Modul bekommt (so wie `Svg` für die Icons in v1.3). Dann reicht der normale
Ablauf **nicht** — die bestehenden Nutzer würden nach dem Update eine exe
bekommen, die eine DLL braucht, die ihr installierter Deps-Ordner nicht hat.

Zusätzlich nötig:

1. Fehlende DLL(s)/Plugins in `build/deps-bundle/` kopieren (Quelle:
   `C:\vcpkg\installed\x64-windows\bin\` bzw.
   `C:\vcpkg\installed\x64-windows\Qt6\plugins\<kategorie>\`).
   Bekannte Plugin-Kategorien, die leicht vergessen werden:
   - `platforms/qwindows.dll` (ohne das startet die App gar nicht)
   - `tls/qschannelbackend.dll` (ohne das schlägt **jede** HTTPS-Anfrage
     lautlos fehl — betraf den Update-Checker in v1.3/deps-v2)
   - `iconengines/qsvgicon.dll` + `imageformats/qsvg.dll` (für SVG-Icons)

2. Neu zippen:
   ```powershell
   Compress-Archive -Path "build\deps-bundle\*" -DestinationPath "build\Presi3D-deps.zip" -CompressionLevel Optimal
   Get-FileHash "build\Presi3D-deps.zip" -Algorithm SHA256
   ```

3. **Neuen** Deps-Tag anlegen (z.B. `deps-v4`) — alte Deps-Tags NIE
   überschreiben, da ältere Installer (v1.0, v1.1, ...) fest auf ihre
   jeweilige Deps-URL zeigen:
   ```bash
   gh release create deps-vN "build/Presi3D-deps.zip" \
     --repo Hannes-swd/Presi3D.exe --title "Qt runtime deps vN" --notes "..."
   ```

4. In `installer/Presi3D.iss` `DepsUrl` (neuer Tag) und `DepsSHA256`
   (neuer Hash, lowercase) aktualisieren.

5. Installer neu kompilieren, committen, Release wie oben.

### Wenn ein Release-Asset im Nachhinein fehlerhaft war

Nicht die Version hochzählen, sondern das Asset im bestehenden Release
ersetzen (`--clobber` überschreibt ohne Nachfrage):
```bash
gh release upload vX.Y "installer/Output/Presi3DSetup.exe" --repo Hannes-swd/Presi3D.exe --clobber
```
So passiert das z.B. bei v1.3, als `deps-v2` (fehlendes TLS-Plugin) durch
`deps-v3` ersetzt wurde, ohne dass `v1.3` selbst zu `v1.5` hochgezählt werden
musste.

## Nützliche Befehle zum Nachschlagen

```bash
git tag -l                                              # alle Versions-/Deps-Tags
gh release list --repo Hannes-swd/Presi3D.exe            # alle Releases
gh release view vX.Y --repo Hannes-swd/Presi3D.exe --json assets --jq '.assets[]'
```

## Bekannte Stolperfallen

- **Inno Setup 6 vs. 7:** Nur Version 7 kann `Presi3D.iss` kompilieren
  (`ArchiveExtraction=auto` gibt's erst dort). Pfad meist:
  `C:\Program Files\Inno Setup 7\ISCC.exe`
- **App-Version und Installer-Version müssen übereinstimmen** — sonst
  erkennt der In-App-Updater die eigene Version falsch (Vergleich läuft
  über `APP_VERSION`, das aus `CMakeLists.txt` kommt, nicht über
  `MyAppVersion` aus der `.iss`-Datei — beide separat pflegen!)
- **`gh` und Inno Setup sind nicht automatisch im PATH** dieser Shell —
  volle Pfade nutzen (`C:\Program Files\GitHub CLI\gh.exe`,
  `C:\Program Files\Inno Setup 7\ISCC.exe`)
