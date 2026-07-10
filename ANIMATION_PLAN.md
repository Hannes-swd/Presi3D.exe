# Plan: Timeline-Animationssystem pro Folie

> **Status:** Nur Idee/Dokumentation, noch nicht implementiert.

## Grundidee

Kein objektbasiertes Animationssystem ("Objekt auswählen → Animation aus Dropdown wählen", wie es Microsoft/andere Präsentationsprogramme machen). Stattdessen eine **eigene Timeline-Unteransicht pro Folie**: eine horizontale Zeitleiste, auf der jedes Objekt der Folie standardmäßig als Balken über die **gesamte Breite** liegt (= sichtbar für die volle Verweildauer auf der Folie).

## Grundverhalten der Timeline

- Jedes Objekt hat einen Balken, der standardmäßig die ganze Breite der Timeline einnimmt (Objekt ist die ganze Zeit sichtbar, keine Animation).
- In der **Mitte des Balkens ist ein Strich/Trenner**: die linke Hälfte steht für das, was beim **Öffnen der Folie** passiert (Eintritt), die rechte Hälfte für das, was beim **Weiterklicken** passiert (Austritt). Beide Seiten werden unabhängig voneinander bearbeitet.
- Standardmäßig sind **keine Zeiten** eingestellt – der Balken ist einfach voll, Objekt ist sofort und durchgehend sichtbar.
- Der Balken lässt sich **vorne verkürzen** (linke/Eintritts-Seite) → Objekt erscheint verzögert, wenn man auf die Folie kommt. Sobald hier verkürzt wird, muss die entstehende Wartezeit (wie lange es dauert, bis das Objekt erscheint) als **Zeitangabe sichtbar** sein.
- Der Balken lässt sich **hinten verkürzen** (rechte/Austritts-Seite) → Objekt verschwindet, bevor man weiterklickt. Auch hier wird die Zeit bis zum Verschwinden angezeigt.
- Baut man zusätzlich in so ein verkürztes Segment eine Animation ein (z. B. einen Fade-In auf der Eintritts-Seite), wird **dieses Segment selbst nochmal unterteilt**: ein Teil ist reine Wartezeit, ein Teil ist die Animationsdauer (z. B. Fade-In-Länge) – auch diese beiden Teil-Zeiten müssen einzeln sichtbar/einstellbar sein.
- Die Präsentation **wartet** auf der Folie, bis alle Timeline-Ereignisse durchgelaufen sind, bevor zur nächsten Folie weitergeklickt werden kann.

## Loops

Ein Bereich auf der Timeline kann als **Loop** markiert werden: Objekt erscheint, wartet, verschwindet, erscheint wieder – wiederholt sich, solange die Folie aktiv ist.

## Trigger (Klick-gesteuert)

Bestimmte Abschnitte/Objekte auf der Timeline starten erst bei einem **Klick** des Nutzers (nicht automatisch/zeitgesteuert). Dadurch z. B. Listen umsetzbar, bei denen pro Klick der nächste Unterpunkt erscheint ("Click to reveal").

## Bedingte Sichtbarkeit über Variablen

Zusätzlich zur zeit-/klickbasierten Steuerung: Sichtbarkeit eines Objekts kann an eine **Bool-Variable** aus dem bestehenden Variablen-System gekoppelt werden (Objekt nur sichtbar, wenn Variable `true`/`false` ist). Ergänzt (ersetzt nicht) die Timeline-Steuerung.

## Erscheinungs-/Verschwinde-Animationen (Keyframe-System)

Beim Markieren eines Animations-Abschnitts (z. B. "Erscheinen") auf dem Objekt-Balken:

- Es entsteht ein **Start-Zustand** (Keyframe), den man frei einstellen kann – u. a. durch Ziehen des Objekts an eine andere Position (z. B. außerhalb der Folie). Beim Abspielen bewegt sich das Objekt dann **flüssig vom Start-Zustand zum Standard-Zustand**, mit einstellbarer Geschwindigkeit.
- Der Start-Zustand ist nicht auf Position beschränkt: **beliebige Eigenschaften** können abweichen (z. B. Farbe – weißer Text auf weißem Hintergrund am Start, schwarzer Text am Ziel), sodass der Effekt frei gestaltbar ist statt aus einer festen Liste vordefinierter Animationen zu wählen.
- Einfacher Spezialfall: keine Positions-/Eigenschaftsänderung, Objekt bleibt an Ort und Stelle und blendet nur smooth ein/aus (Opacity-Fade).
- **Wichtig:** Der Übergang zwischen Eingangs-Zustand und Standard-Zustand muss sauber/smooth interpolieren, und zwar für **alle Objekttypen** (Text, Formen, Bilder usw.), nicht nur für einen Typ.
- Einschränkung: Es wird nur unterstützt, was sich **im Browser darstellen lässt** (CSS-Transitions/JS über impress.js/HTML-Export), kein Rendering-Feature, das sich nicht in HTML/CSS/JS abbilden lässt.

## Zu entfernen

Das aktuelle, einfache Animationssystem ("Eintrittanimation" – Objekt auswählen, Animation aus vordefinierter Liste wählen; siehe `Slide`/`SlideElement` in `src/models/DataModel.h`, UI in `src/PropertiesPanel.cpp/.h`, Export in `src/export/HtmlExporter.cpp`) soll **komplett entfernt und durch das Timeline-System ersetzt werden**.

## Offene Fragen (noch nicht besprochen)

- Konkrete UI-Umsetzung der Timeline-Unteransicht (eigenes Fenster/Panel? wie wird sie pro Folie geöffnet?).
- Datenmodell für Timeline-Einträge (Start/Ende, Keyframe-Werte, Loop-Grenzen, Trigger-Flag) – noch nicht entworfen.
- Wie Trigger/Klick-Steuerung sich mit der bestehenden Folien-Navigation (impress.js-Steps) verträgt.
- HTML-Export-Umsetzung der Timeline-Logik (JS-Player, der Timeline-Events/Loops/Trigger im exportierten HTML nachbildet).
