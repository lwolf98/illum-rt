# RTGI -- Ray Tracing & Global Illumination Framework

## Stand der Dinge

Für den Moment ist das ganze Framework als prototypisch anzusehen.
Das Ziel der aktuellen Entwicklung ist dass im WS'20 der geplante Stoff des Wahlfachs anhand des Codes nachvollzogen werden kann.

Eine bessere Aufteilung und schnellere Ray Tracer sind sekundär :)

## Verwendung des Repos

Wenn Sie den Code direkt aus dem Repository verwenden sind Sie so zu sagen im Maintainer-Mode, d.h. Sie müssen die nötigen Autotools installiert haben (automake, autoconf, pkg-config).
Zur Vorlesung werde ich wahrscheinlich Tarballs austeilen in denen Sie nur `configure` ausführen müssen.

Wenn Sie das Repo verwenden müssen Sie das Buildsystem erzeugen und konfigurieren, bevor Sie initial den Code übersetzten können:
```
$ autoreconf -if
$ ./configure
```

## Configure & Make

Wenn Sie den Code mit gutem Debug-Support übersetzen wollen (auf Kosten der Laufzeit) können Sie `configure CXXFLAGS="-ggdb3 -O0"` verwenden.
Zum Debuggen empfehle ich `cgdb`.

Übrigens kann mit `make -j N` parallel übersetzt werden, wobei `N` die Anzahl von Jobs ist. Faustregel: etwas mehr als die Anzahl der Prozessorkerne. Auf einem 4-Core System mit Hypterthreading also z.B. `make -j9`, auf meinem 8-Core System `make -j20` (auch wenn es dafür noch nicht genug Sourcefiles gibt).

## Verbesserungen

Wenn Sie eine Verbesserung mit mir teilen möchten können Sie gerne einen Patch senden, noch besser: Forken Sie das Repo, committen Sie den Code und stellen einen Pullrequest :)

## Ausführen

Um das Programm auszuführen verwenden Sie eines der Skriptfiles:
```
./src/rt -s script-sibenik
```
(dazu müssen die entsprechenden Modelle unter `render-data/` liegen, siehe Grips-Seite).

In dem Skript-Fall beendet sich das Programm danach, Sie können es aber auch mit `-l` (für "load") ausführen, dann wird das Skript geladen und ausgeführt, danach können Sie aber noch weitere Kommandos eingeben.
Tipp: Wenn Sie sich in der Szene bewegen wollen drenen Sie `sppx` und `resolution` etwas herunter, dann geht das viel schneller.
Alle verfügbaren Kommandos finden sich in `src/interaction.cpp`, bis auf GI-Algorithmus-spezifische Kommandos, die bei den einzelnen Algorithmen definiert sind (siehe z.B. `src/primary-hit.cpp`).

## Neue Sourcefiles

Bisher ist der Code noch nicht auf die Art und Weise organisiert wie ich das gerne hätte, deshalb können alle Quelldateien unter `src/` abgelegt werden. Neue Quelldateien sollten in `src/Makefile.am` in der Liste hinzugefügt werden, dann sind sie automatisch im Build-Prozess integriert.


