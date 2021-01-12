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

sudo apt install
## Abhängigkeiten

Folgende Pakete sind nötig (Liste für Debianderivate wie z.B. Ubuntu/Mint, weitere nehme ich gerne auf, schicken Sie mir einfach die nötigen Daten!).

Zum Übersetzen des Codes:
```
sudo apt install build-essential pkg-config imagemagick libmagickwand-dev libassimp-dev libglm-dev libpng++-dev
```

Für das Buildsystem wenn Sie das Repo direkt verwenden:
```
sudo apt install autoconf automake autoconf-archive
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
./rtgi -s script-sibenik
```
(dazu müssen die entsprechenden Modelle unter `render-data/` liegen, siehe Grips-Seite).

In dem Skript-Fall beendet sich das Programm danach, Sie können es aber auch mit `-l` (für "load") ausführen, dann wird das Skript geladen und ausgeführt, danach können Sie aber noch weitere Kommandos eingeben.
Tipp: Wenn Sie sich in der Szene bewegen wollen drenen Sie `sppx` und `resolution` etwas herunter, dann geht das viel schneller.
Alle verfügbaren Kommandos finden sich in `src/interaction.cpp`, bis auf GI-Algorithmus-spezifische Kommandos, die bei den einzelnen Algorithmen definiert sind (siehe z.B. `src/primary-hit.cpp`).

## Neue Sourcefiles

Bisher ist der Code noch nicht ganz auf die Art und Weise organisiert wie ich das gerne hätte, wenn Sie einen neuen Tracer schreiben, dann legen Sie bitte ein Unterverzeichnis in `rt/` an. Sie können dann das `rt/bbvh-base/Makefile.am` kopieren und anpassen, siehe auch `driver/Makefile.am`. Neue Quelldateien sollten im entsprechenden `Makefile.am` in der Liste hinzugefügt werden, dann sind sie automatisch im Build-Prozess integriert.

## Branching in Projekt/Examensarbeiten

Wenn Sie eine längere Arbeit innerhalb von RTGI machen, erstellen Sie bitte einen FORK des Repositories via Gitlab. In Ihrem Fork, legen Sie dann bitte einen Branch an der also lokaler Masterbranch dient (auch Integrationbranch genannt), Vorschlag <arbeit>-<username>, also z.B. ba-sek38402.

Für einzelne Teilaufgaben können Sie dann, ausgehend von diesem Branch, sogenannte Featurebranches anlegen innerhalb derer Sie die Aufgaben umsetzen können. Damit können wir, wenn Sie Feedback zu Ihrer Umsetzung wollen, einen Pullrequest aufmachen und via Gitlab die Unterschiede zu Ihrem Ursprungsbranch sehen und anhand des Codes kommentieren und entsprechend sehr einfach über Details sprechen.

Wenn wir dann zufrieden mit dem Stand sind können Sie den Featurebranch zurück in Ihren Integrationsbranch mergen.

Sie können regelmäßig vom Original-Repository den master-Branch in Ihren Integrationsbranch mergen, und sollten einzelne Teile oder Ihre Arbeit insgesamt in das Ursprungs-Repository gemerged werden, ist das sehr einfach möglich.

Inspiriert von https://nvie.com/posts/a-successful-git-branching-model/

## Code Conventions

Ein unangenehmes Thema, da aber der Plan ist, dass in das Repo Code aus Projekt- und Examensarbeiten integriert wird würde ich mich sehr freuen wenn Sie Code schreiben der dem Muster des vorhandenen Codes folgt.
Im Zweifel, oder wenn meine Stilbeschreibung unterspezifiziert ist, schauen Sie sich um und orientieren Sie sich an dem, was Sie sehen.

Es wird ein Separates Dokument geben das den Stil beschreibt und auch darauf eingeht wie und wann Sie ohne große Konsequenz davon abweichen können.
Hier geht es nicht darum Ihnen meine Sicht der Welt aufzudrücken, sondern nachfolgenden Studis eine konsistente Codebasis zur Verfügung zu stellen. Erfahrungsgemäß macht das die Einarbeitung etwas einfacher.

## BVH Export

Die verwendeten AABBs (bisher nur für Binary BVHs) können mit dem Kommando `bvh export TIEFE DATEINAME.obj` als OBJ-Datei exportiert, und in Blender zur Veranschaulichung importiert werden.
Hierbei muss das export Kommando nach dem `commit` stehen. Wenn beim Import in Blender die Einstellungen `Split by Group` gesetzt wird, ist es möglich die verschiedenen Tiefen/Level der BVH ein- und auszublenden. Mit `Z` kann in Blender zwischen Solid und Wireframe Ansicht gewechselt werden.
